# Note 0016 — executor-honor-set-semaphore-offset

Date: 2026-11-21. State: note 0011 post-script 5 (first flips; frames
black; main later parks again).

## Observed

With `RECOMP_PB_EXEC=1 RECOMP_VBLANK=1` (+`RECOMP_PB_WALK=dma`) the
title plays through the intro (223-234 flips) and then **flips stop**:
`[GPU] 0.00 fps (223 flips)` forever
(`logs/run-walk1-201153.out`, `logs/run-long1-200120.out`).

Core at t=240 s (`/tmp/opencode/stop2.core`, `kill -ABRT` + coredumpctl
recipe): thread 1 (main) sits in the D3D8 flip wait —

```
sub_002D3CC0  <- the spin on [ctx+0x8700] == 0
sub_001DE5C0 / sub_001DE310 / sub_0011B020      (D3D present chain)
sub_00011DE0 / sub_00012090 / sub_00011560      (SOL startup, main)
```

while the method executor counts **`0x1D70 x710`**
(BACK_END_WRITE_SEMAPHORE_RELEASE) in the unhandled table — nobody in
the runtime writes the GPU semaphore releases into guest memory, so the
word the title spins on keeps the value guest code stored
(`MEM32(edi + 0x8700) = 1`, recomp_0053.c:28816).

The PB scan (`RECOMP_PB_SCAN=1`) shows the title programs
**`0x1D6C` (SET_SEMAPHORE_OFFSET) x1** but releases **`0x1D70` x710**:
one programmed semaphore word per kick pattern, releases per kick. The
executor writes releases only to a fixed target registered via
`nv2a_pb_set_semaphore_target()`, which no runtime calls (tests only) —
so for this title every release is dropped
(`nv2a_pb_exec.c:4195` gate `&& s_sem_va`).

## Hypothesis

H21: honoring the title's own programmed offset — last value written to
method `0x1D6C` (already stored in the executor's `s_reg` mirror) as the
target for `0x1D70` writes — delivers the GPU completion semantics the
D3D8LTCG wait structure expects, and main leaves the `sub_002D3CC0`
spin after the movie. Falsified if the wait word is a different one
(spins keep going with releases delivered) or if the guard rejects the
programmed VA (trace will show `tgt=0`).

Guard needed: X-Men Legends held junk (`0xFF000000`) in `0x1D6C`
(upstream comment at the 0x1D70 block), so only accept a page-aligned
offset below the 0x80000000 RAM aperture.

## Planned change

Toolkit patch 0016 (host side, executor only): in `nv2a_pb_exec_method`
the `0x1D70` early-return falls back to `s_reg[0x1D6C/4]` when no fixed
target is registered, with the guard above; `RECOMP_SEM_TRACE=1` prints
the first 8 delivered releases (tgt + value). No guest patching, no
gen/ edits.

## Outcome (first experiment, patch 0016)

The honor-the-offset path **never fires for the flip semaphore**: the
trace (`RECOMP_SEM_TRACE=1`, `logs/run-sem3-202423.out`) shows exactly
**one** `0x1D6C` in the whole stream, programming offset **0x00000000**
— D3D8LTCG resolves the semaphore word through the **DMA instance**
(`SET_CONTEXT_DMA_SEMAPHORE`, method 0x01A4) instead: word = DMA-base
+ offset. The 673 releases (values 5,7,9…0x545) are the per-kick ring
counters, not the flip flag; writing them to one fixed word would be
wrong anyway. H21 falsified in that form.

Corrected anatomy (cores `stop2.core` + `sem.core`, `x/wx` via
`g_xbox_mem_offset`):

* `[0x002E3638] = 0x002E1190` (dev); `[dev+0x934] = 0xFD000000`
  (stable across three runs: watchdog eax, stop2, sem);
* the Present wait spins on `MEM32(0xFD008700) == 0`;
* that word is **0x00000001** in the parked state — set by the submit
  code (`MEM32(edi + 0x8700) = 1`, recomp_0053.c:28816) immediately
  before it queues the flip kick;
* the flip kick's completion on hardware is the GPU flip-counter
  machinery (`SET_FLIP_READ/WRITE/MODULO 0x0120/0x0124/0x0128`,
  `FLIP_INCREMENT_WRITE 0x012C`): the executor *models* these counters
  and counts `s_gpu.flips` (224) but writes nothing to guest memory —
  the flag stays 1 and main spins in `sub_002D3CC0` forever.

H22 (next): mirror the flip-completion write host-side, same shape as
the note-0011 fence mirror — register the measured chain
(`dev global 0x2E3638 → +0x934 → +0x8700`) and have the executor clear
the word when it performs a flip (`flips++`). Falsified if clearing at
`FLIP_INCREMENT_WRITE` is the wrong moment (guest then desyncs flip
accounting) or if more of the counter semantics are needed.

## Outcome 2 (patch 0017, flip-done chain)
(fill in AFTER the experiment)
