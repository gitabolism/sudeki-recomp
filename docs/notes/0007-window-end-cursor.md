# Note 0007 — the game's stream cursor walks into the end of the contiguous window

Date: 2026-11-21 (session 4). Follows note 0006 (window placement).

## Outcome of note 0006 / patch 0006 (placement insurance)

With `0006-span-window-fits.patch`, a run that gets a high OS-chosen span
base is re-placed low, and all five fixed windows map. In the observed
healthy run (`logs/run-20261121-051112.log`) the OS base was already low
enough, every window mapped cleanly (`Contiguous window: 64 MB at Xbox VA
0x80000000`, NV2A/MCPX/flash/tiled all present) — **and the guest still
faulted at exactly `0x84000000`**, one byte past the end of the 64 MB
window. So H8 ("the crash is purely the unmapped window") is dead as
stated: the mapping succeeded, the fault is at the window *boundary*.
Patch 0006 stays as insurance against the nondeterministic top-of-address-
space placement, but the wall is elsewhere.

## Measured (core dump of pid 342640, `logs/run-20261121-051112.log`)

Post-mortem core analysis works even though live gdb/ptrace does not
(systemd-coredump → `coredumpctl dump` → gdb on the 2.1 GB core). The
crash instruction chain in `sub_000223F0`:

```
call sub_00022270                     ; the write wrapper (note 0005)
ecx = MEM32(ebp + 0x18)               ; the stream cursor field
edx = g_edx                           ; = cursor + 0x3E
MEM8(ecx) = LO8(edx)                  ; fault: write to 0x84000000
```

The frame/stream area (guest VA 0x02682E98, on a heap-allocated thread
stack — the base shifts by 0x1000 between runs):

```
+0x18 cursor   0x84000000   (set to 0x80044000 by the caller sub_00021C0:
                              MEM32(obj+0x18) = MEM32(obj+0x2C))
+0x1C end      0x80054000
+0x2C          0x80044000
+0x38 base     0x80034000   <- [CONTIG] alloc #5: 192 KB at 0x80034000
+0x44 end?     0x80054000   (base + 0x20000)
```

So the copy loop inside `sub_000223F0` (the function that calls the
disk-full'd write wrapper) advanced its destination cursor **0x3BC000
(3.9 MB) past** where the caller set it, through the end of the game's
own 192 KB contiguous buffer (0x80034000..0x80064000), across ~60 MB of
other window storage (trampling it silently — those writes never fault
because the toolkit backs the whole 64 MB window), until it hit the
first unmapped host page at window end.

The whole execution is reproducible at the instruction level: identical
register signature (`eax=ecx=0x84000000, edx=0x8400003E, ebx=0x744,
esi=0x3E, edi=0xFFFFFEFF`) in three runs; only the thread-stack base
address shifts, which is why `RECOMP_WATCH` pre-arming cannot catch the
writer of the cursor (the target slot address is racy).

The STATUS_DISK_FULL code (`0xC000007F`) is present on the stack next to
the cursor fields — the loop is post-disk-full code.

Facts, not verdicts (rule 6): it is not established whether on hardware
(a) the cursor wraps because the game sizes the transfer from a volume
that is not full at that point, (b) the cursor is bounded by a size our
volume-info answers fed wrong, or (c) the game really does address
0x84000000+ and hardware decodes it (AGP aperture aliasing).

## Hypotheses

1. **H10:** after disk-full, `sub_000223F0` keeps streaming into its
   contiguous ring; the bound comes from data our emulation supplies
   (volume info / the E_FAIL path), and the cursor runs away. The fix is
   then in the sizing data, not the mapping.
2. **H11:** the game legitimately reaches into `0x84000000..0x87FFFFFF`
   and hardware decodes that aperture (it is physical-address space above
   64 MB, aliasing DRAM). Our window is only 64 MB, so the access faults
   here but would not on hardware.

## Planned change (probe, one variable)

`patches/xboxrecomp/0007-contig-alias.patch`, env `RECOMP_CONTIG_ALIAS=1`:
map a second view of the same contiguous file mapping at
`0x84000000` (another 64 MB, zeroed, aliases the window's bytes — the
hardware-aperture model). Default unchanged. Falsifier for H11: with the
alias mapped, if the run still stops at the same wall (or the game reads
only zeros and behaves identically-bad), the fault was not the mapping
and H10 (sizing) is the live one.

## Outcome (probe 0007 run, `logs/run-20261121-051703.log`)

`RECOMP_CONTIG_ALIAS=1` mapped the alias cleanly ("Contiguous alias: 64 MB
at Xbox VA 0x84000000 (mapped)") and **the fault at 0x84000000 is gone**.
The run became HANG again — back to the original wall: main thread polls
`NtWaitForSingleObjectEx token=0x48000004` forever; still exactly one
event signal all run (`0x48000002`). So H11 ("the aperture mapping is the
wall") is dead: the alias removes the crash but changes nothing else.
Patch 0007 stays env-gated off by default as a bring-up tool.

What the wall looks like now (facts, same log):

1. The writing function is not a copy: `sub_000223F0` is a **bitstream
   decoder** (bit iterator `ebx >>= 1` with `0xFF00` sentinel, byte
   refill, source/dst cursor pairs `+0x10/+0x14` and `+0x18/+0x1C`,
   refill helper `sub_00022340`, flush wrapper `sub_00022270`).
2. `boot.baf` on disc: header `00 c8 1b 00` = **1,820,672 bytes
   uncompressed**, then a compressed stream (679,936 B compressed,
   heavily repetitive — long runs of `0x29`). The game reads the whole
   file exactly once (22 reads, byte-exact vs disc, verified against
   `game/boot.baf`), then **writes 61,440-byte chunks to `Z:\boot.baf`
   unboundedly** (94k+ writes; the ring-wrap/flush logic works while
   writes succeed — flush at cursor==end, cursor resets).
3. Expected decoded output is ~1.8 MB ≈ 30 chunks; observed 94,000+ and
   still growing → the decoder never sees end-of-input on our host.
   Why is open: input bytes into the ring are verified correct at the
   file layer; at crash time the ring holds decode output (it rotates),
   so the core cannot show whether the decoder ever saw the real bytes.
4. Event chain after the cache phase: main thread creates
   `0x5A000024` (guest `0x48000004`) on its own stack and polls it;
   worker #1 (`PsCreateSystemThreadEx` ctx `0x00021AD0`) blocks
   **INFINITE** on `NtWaitForMultipleObjectsEx` for `0x48000001`
   (`0x5A00001C`, created on main's stack right after the benign
   `T:\$u\contentmeta.xbx` NOT_FOUND — a retail console's empty TDATA
   gives the same miss) + its own `0x48000006`; worker #2 (ctx
   `0x00021B90`) is the decoder thread. Nothing signals `0x48000001` or
   `0x48000004`. So the launch completes only if the decoder thread
   finishes, and it never finishes because the decoder never terminates.

## Next (next session's queue)

- Determine why the decoder does not terminate: instrument the refill
  path `sub_00022340` → XAPI read helper `sub_00270FBD` (read site
  `0x271012`): does refill keep being called after EOF and silently
  return stale/zero data, and does the source-cursor end (`+0x14`)
  match 679,936? An env-gated per-read log of buffer VA (patch 0001
  extension) plus a `RECOMP_WATCH` on the *object* (found via a first
  run's core, then re-armed — object address, unlike thread stacks, may
  be stable across runs: verify).
- Identify the container format offline (small python decoder from the
  lifted structure) and compare expected vs written output sizes —
  if 1,820,672 is achievable host-side, the decoder's non-termination
  is an input-delivery bug, not the format.
