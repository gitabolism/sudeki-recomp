# 0021 — attract-demo freeze: `sub_002D57D0` waits on `0x002E2F5C`; ring-settle never signals it

Naming discipline (per user directive 2026-11-22): every function/object in
this note is named by measured address. Role words ("event", "ring") appear
only where a measured field or host-resolved symbol backs them.

## Observed

Recipe: freeze recipe = working recipe + `RECOMP_PAD_LIVE` presses at
t≈105 s+ (4× start, 12 s apart). After input worked (note 0020), the title
enters its attract demo (`badger.plap`, `speech_buki` open) and then flips
stop: `0.00 fps (1821 flips)` across several 10 s samples. Vblank keeps
ticking (~49.5/s). No crash.

Core: SIGABRT at 200 s (`logs/run-frz2-*.out`, pid 154533,
`/tmp/opencode/sudeki.frz2.core`).

Main thread backtrace (gdb, host symbols):

    sub_001E0680 → sub_002D35A0 → sub_002D3420 → sub_002D57D0
      → kernel_thunk_dispatch → bridge_KeWaitForSingleObject

So the blocking call resolves (host-resolved, not inferred) to
`KeWaitForSingleObject`. Guest code at `loc_002D5911` (gen
`recomp_0053.c`) pushes `(obj, 6, 1, 0, 0)` with
`obj = dev + 0x1DCC`, `dev = MEM32(0x002E3638)`, through KTA slot
`0x003157E4`.

Measured values in the core (`g_memory_offset = 0x7f3662e00000`):

| guest VA | value | note |
|---|---|---|
| `0x002E3638` | `0x002E1190` | the `dev` pointer (same as note 0016 chain) |
| `0x002E2F5C` (=dev+0x1DCC) | `0x00040000` | dispatcher header word: Type=4, SignalState=0 — a *not-signalled* dispatcher object of the "4" type |
| `dev+0x2C` | `0x00002B95` | counter the title bumps itself (gate below) |
| `dev+0x30` | `0x80065000` | pointer; word there = `0x00002B95` — equal to `dev+0x2C` at freeze |
| `dev+0x24` | `0x80066000` | block base (contiguous-window VA) |
| `0x80066000..` | `0x00040000 0x0000000D …` | header-shaped words (count/subch/method form) |

Gate decoded from the lifted body of `sub_002D57D0`:
`submitted(=[dev+0x2C]) - consumed(=*[dev+0x30])` vs a parameter; at freeze
the two counters are **equal**, so the wait is not counter-starved. The
branch taken is `loc_002D58B1`: call `sub_002D55B0` (returns a size); if
`eax < 0x2000` the engine goes on, else it calls `KeWaitForSingleObject`
on `dev+0x1DCC` — measured identity of the only blocking call in the frame.

Watchdog NV2A samples differ per run: `run-frz1` `PUT=0x000CC570
GET=0x000CC448` (GET behind); `run-frz3` (`RECOMP_NV2A_TRACE=1`) at freeze
`PUT=0x0024109C GET=0x0024109C` (ring fully consumed) with the engine still
waiting. So "GET behind PUT" was one sample's state, not the invariant.

## H25 — FALSIFIED (recorded so it isn't relitigated)

H25 claimed `nv2a_pb_scan` read the ring through a stale RAM view
(`mem+phys`) while the title wrote commands through the contiguous window
(`g_contig_memory`), based on reading `g_memory_offset+0xCC448` (x86 code
+ `0xCCCCCCCC` poison) vs `g_contig_memory+0xCC448` (method-shaped words).
That was an address-arithmetic error on my part: `0x80000000 | 0xCC448 =
0x800CC448` (window, aliased), **not** `0x8CC448` (RAM offset 9.2 MB, which
is what I actually sampled as "zeros"). The core disproves the two-backing
model directly:

    g_contig_memory = 0x7f36e2e00000
    g_memory_base   = 0x7f3662e00000 ; 0x7f3662e00000 + 0x80000000 = 0x7f36e2e00000

The contiguous window is mapped *inside the main span* at exactly
`span + 0x80000000`, so `mem + (0x80000000|phys)` and
`g_contig_memory + phys` are the same host bytes. `RECOMP_CONTIG_ALIAS`
is not implicated; H8/H11's "mapping is not the wall" still stands.
(The method-shaped words at `g_contig_memory+0xCC448` are real ring
content reached correctly by the walk; the "code + poison" at
`mem+0xCC448` is guest VA 0xCC448 — a different VA entirely.)

## H26 (open) — who signals `0x002E2F5C`: the `sub_002DB4A0` software-method chain

Event-trace evidence (`logs/run-frz4-005631.out`, `RECOMP_EVENT_LOG=1`,
freeze at 1478 flips):

- `KeSetEvent token=0x002E2F4C from=0x002DB0BE` × **1393** — the object at
  `dev+0x1DBC` is set continuously (that site `loc_002DB0A0` services
  PCRTC/vblank: writes `edi+0x600100`, spins on `edi+0x100` bit
  0x01000000 — `edi=0xFD000000` register-window base).
- **zero** create/wait/set lines for `0x002E2F5C`. Wait side is
  `bridge_KeWaitForSingleObject` (not logged by patch 0015 — Nt-level
  waits only), consistent with the frz2 core.

Static chain in the lifted tree (all addresses; LTCG puts the D3D8LTCG
library in `.text`, so "sub_002Dxxxx" is library ring code, not engine code):

- `sub_002D57D0` gate: `sub_002D55B0` returns ring distance
  (`[esi]` cursor, `[esi+0x44]` size, clamped); at `>= 0x2000` the caller
  waits on `esi+0x1DCC` via KTA `0x003157E4` (host-resolved:
  `bridge_KeWaitForSingleObject`).
- `sub_002DB4A0` = software-method dispatcher. Switch table at
  `0x002DB6B4` (read from core): index `(param>>5)&0x1F`, 14 entries;
  **index 4 (param 0x80–0x9F) → `loc_002DB60E` →
  `KeSetEvent(object+0x1A4, 1, 0)`** — the waited object. `index 10 →
  loc_002DB5C6` decrements `[0x2E11E0]` (=dev+0x50); etc.
- `sub_002DB250` = GPU-interrupt service body: `esi = [ebx]` =
  `0xFD000000` (from `[dev+0x1C28]`, read from core); reads
  `esi+0x400100` (PGRAPH_INTR *via the aperture mirror*), `esi+0x400704`
  (trap data, `&0x1FFC` = method; gate `cmp edi,0x100` →
  **NO_OPERATION**), `esi+0x400108` (param), tests bit 20 of PGRAPH_INTR
  (`HI8 & 0x10`). Called from 5 sites (`loc_002DA7A8/0x2DABBB/0x2DAE67/
  0x2DB427/0x2DE53B`) = D3D's own service pumps.
- Executor census: `0x0100 NO_OPERATION` appears 3375× in the unhandled
  table — the executor consumes the software methods but delivers no
  PGRAPH notification.

Mechanism (hardware): GPU executes NO_OPERATION software method → PGRAPH
latches bit 20 + trap method/param → D3D ISR reads the aperture mirror,
dispatches → `KeSetEvent(0x002E2F5C)`. Under the toolkit the aperture
mirror at `0xFD400100+` is never written (the `NV2A_ACK` hold applies to
the MMIO file at `0xF0000000`, not the aperture), so the service finds
INTR=0 and the dispatcher arm never runs.

Census to decide the fix shape (running next): manual wrappers at
`sub_002DB250` and `sub_002DB4A0` — do the pumps even run (nonzero → the
missing piece is only the latched INTR/trap registers; zero → D3D's
service is never invoked and the wake path needs a host-side delivery).

## Advance — the pump already runs: vector 3 belongs to D3D

`logs/run-frz4-005631.out`:

    [KERNEL] KeConnectInterrupt: vector 3 -> routine 0x002DAD20 context 0x002E2DB8

`0x002E2DB8 = dev + 0x1C28` — the same object the GPU service body uses —
so the title's **vector-3 ISR IS D3D's GPU service**, and the toolkit's
`RECOMP_VBLANK` machinery already calls vector 3 at 60 Hz
(`kernel_bridge.c`: `NV2A_VECTOR = 3`, `XBOX_NV2A_REG_BASE = 0xFD000000`
— the vblank bits are latched into the *aperture* register block the
D3D ISR reads, and cleared by the vblank-ack path).

Only the PGRAPH half of that latch is missing: nothing ever sets
`+0x400100` (PGRAPH_INTR) bit 0, `+0x400704` (trap data, method field
`0x100` = NO_OPERATION), `+0x400108` / `+0x400708` (param) when the
executor consumes a NOP software method. Service-body branch map
decoded from the lift: bit 20 (`HI8 & 0x10`) = trap-data path; bit 0
(the `test eax, 0x100001` gate) = the NOP-param path →
`sub_002DB4A0(ecx = device cell, eax = param)`; the body's
`MEM32(esi+0x400100) = eax` "clear" is a write-1-to-clear on hardware
and a no-op on plain RAM, so the toolkit must clear the latched word
itself (the vblank ack already does exactly this for PCRTC/PMC).

Fix (patch 0025, probe-gated): executor remembers the last
`NV097_NO_OPERATION` param; `kernel_vblank_deliver` latches
`PGRAPH_INTR bit0 + trap 0x100 + param` before calling vector 3 and
clears after — the guest ISR then dispatches `sub_002DB4A0` naturally,
arm 4 (`param 0x80–0x9F`) runs `KeSetEvent(0x002E2F5C)` and the ring
wait releases. One variable: census run first (wrappers at
`sub_002DB250`/`sub_002DB4A0`), then the patch.

## Census outcome (gsvc1, 230 s, froze at 2211 flips): ZERO service calls

`[GSVC]` (wrapper at `sub_002DB250`) and `[SWM]` (`sub_002DB4A0`): both
zero for the whole run, while `[NV2A] vblank -> ISR claimed it` fires.
So the vector-3 ISR (`sub_002DAD20`) runs 60 Hz but its *service leg*
is skipped by its own gates. Decoded body (measured values at freeze
from `sudeki.frz2.core`, aperture confirmed single backing:
`g_nv2a_memory` = the 0xFD000000 window, "zeroed, no register
semantics" per `xbox_memory_layout.c`; there is no second register
file for the library):

| gate | site | field (frz2) | verdict |
|---|---|---|---|
| `[esi+0xA0] != 0` | `loc_002DAD20` | `0x2E2E58` = 1 | pass |
| `[edi+0x140] != 0` (PCRTC_START) | `loc_002DAD36` | `0xFD000140` = 1 | pass (watchdog's `PCRTC_START=0` line reads the *MMIO* offset, different word) |
| `[esi+0xA4] == 0` → continue else DPC-only | `loc_002DAD6A` | `0x2E2E5C` = 0 | pass |
| enable mirror bit 0x01000000 | `loc_002DAD78` | `[0x2E2F6C]` = `0x00600104` | no bit → fall through |
| `eax` from `([esi+8]&0xC0000000)==0x80000000 && ([0x2E2F6C]&0x200000)` | `loc_002DAD44/55` | `[0x2E2DC0]` = `0x0801010D` → bits30-31 = 0 | **eax=0 → exit to DPC-only leg** |
| (even with eax=1) `PMC_INTR_0 & 0x01000000` | `loc_002DAD8C` | `[0xFD000100]` = 0 at sample time | would fail too |

## Full service chain, decoded (the ISR is D3D's vblank service)

The vector-3 ISR `sub_002DAD20` is D3D8LTCG's **vblank service**: after
its gates it reads the encoder field pin (`in al,0x80C0` bit 5 →
`esi+0x1D0`), walks swap-chain slots (`esi+0x1BC/0x1C0`, poster word
`0x2E47EC`), then at the common tail `loc_002DAE0E` writes
`[regs+0x140]=0` (its own ack) and **always queues its DPC** via
ICALL KTA `0x315860` (`KeInsertQueueDpc`, DPC object `esi+0x84` =
`0x2E2E3C`). Measured in `sudeki.frz2.core`: DPC
`DeferredRoutine = 0x002DAE40`, `DeferredContext = 0x2E2DB8` (the
register-window cell), `Inserted = 0` at freeze — the queue path ran.

The DPC body `sub_002DAE40` is the real dispatcher — it reads the
**PMC aperture word `[0xFD000100]`** and branches per source bit:

| word bit | leg |
|---|---|
| `0x1000` | `sub_002DB250(ecx = cell)` — the **PGRAPH service pump** |
| `0x100000` | `sub_002DB100` |
| `0x1000000` | flip leg: waits `[regs+0x600100]` cleared |
| `0x100` | `sub_002DB310` → also calls `sub_002DB250` |
| `0x10000` | poster/swap-chain leg |

`sub_002DB250` (thiscall, `esi = [[cell]] = 0xFD000000`) decodes the
software method from the PGRAPH words (correcting the earlier note
line: bit **20** = the *generic trap* path; the NOP path uses
**bit 0**):

    eax   = [0xFD400100]              PGRAPH_INTR
    meth  = [0xFD400704] & 0x1FFC    trap data method field
    param = [0xFD400108]
    → requires eax & 0x100001 (bit 0), param != 0, !(param & 0x40),
      meth == 0x0100 (NO_OPERATION) → sub_002DB4A0 → arm 4
      (param 0x80..0x9F) → KeSetEvent(0x002E2F5C)

So the latch a NOP must leave in RAM for the guest chain to service
itself (hardware-faithful; the toolkit already brackets PMC/PCRTC
words the same way for vblank):

1. `[0xFD000100] |= 0x00001000` (PMC PGRAPH source — the DPC gate)
2. `[0xFD400100] |= 0x00000001` (PGRAPH bit 0 = software method)
3. `[0xFD400704] = 0x00000100` (trap method field = NO_OPERATION)
4. `[0xFD400108] = param` (0x80..0x9F = ring space)

and since the service's "clear" of PGRAPH_INTR is a write-1-to-clear
on hardware and a no-op on RAM, the toolkit clears words 1-4 again
after the ISR+DPC call returns (the same ack pattern the vblank
latch already uses).

Writer census for the ISR's identity/flags cache: `RECOMP_WATCH=0x2E2DC0`
caught `[002E2DC0] 00000000 -> 0801010D` twice at startup, both from
the D3D register-write helper `sub_002DB911+0xc43` (stack carries the
device object, the register cell, and offset `0x400104`) — a cached
register value, not host-missing state; the ISR leg its bits gate
(`esi+8 == 0x80000000` class, `esi+0x1B4` bits 0x200000/0x01000000)
is the *swap-slot* leg and is not required for the pump: the pump
runs off the DPC's PMC word instead.

Decisive census (running): wrapper at `sub_002DAE40` logging
`[0xFD000100]` + the PGRAPH words — does the DPC run per tick and
find `0x1000` missing, or does it never run at all?

## Census outcome (dpc1, 230 s): the DPC runs, the PGRAPH line is the gap

`[DPC]` fired 300/300 budget (60 Hz cadence, whole run) — the DPC
**runs on every vblank tick**. Its dispatch word reads `PMC=01000000`
(flip leg active during movies) and later `PMC=00000000` (freeze:
nothing latched), but **never `0x1000`**, and `PGRAPH/TRAP/PARAM`
are `00000000` for the whole run. `[GSVC]`/`[SWM]` zero: the pump
gate `edi & 0x1000` never opens because nothing ever raises the PMC
PGRAPH source for the consumed NOPs. Hypothesis H26 confirmed end to
end; patch 0025 implements exactly the latch above.

## Patch-0025 first cut falsified its own hook site (gfault1)

First extraction hooked `NO_OPERATION` before the subchannel filter
and returned early (gfault1, `logs/run-gfault2-*` compares): the
pump ran **once** in 230 s with `PARAM=5C980141` — a vertex-data
word. Two consumption branches sit *before* any software-method
reading in `nv2a_pb_exec.c`:

* `imm_vertex_method(method, param)` — immediate-mode vertex data is
  encoded as non-zero-param NOPs; the census param above is one.
* `nv2a_pb_exec_flip_pacing()` — when `RECOMP_FLIP_PACING` is set,
  NOP params become swap-interval pacing (default **off** in our
  recipe).

The toolkit's own comment at the `case NV097_NO_OPERATION` says it:
"Non-zero, it is one of D3D's software methods, which the GPU traps
to D3D's interrupt handler… the rest stays a no-op" — the
software-method latch belongs **inside** `if (!imm_vertex_method(…))`,
which is exactly what patch 0025 now does, plus a `[NOPSW]` census
before the subchannel filter counting every non-zero-param NOP with
its subchannel so "no marks ever reached the walk" can be told from
"marks reached but were mis-consumed".

## Fix design (patch 0025, final form after the drain-site audit)

The earlier bracket idea (latch/clear around `kernel_call_isr`) is
**wrong**: the DPC queued by the ISR does not run inside that call —
`xbox_IrqlLeaveInterrupt` only restores IRQL bookkeeping; queued DPCs
drain at `dpc_run_here()` (kernel_hal.c), reached from the guest's
own `KfRaiseIrql`/`KfLowerIrql` crossings and the timer thread's
drain. `frz2` shows DPC `Inserted = 0`, so the drain does run
regularly. Final shape:

* `nv2a_pb_exec.c` — at `NV097_NO_OPERATION` with `param != 0`, when
  `[0x400100] == 0` (trap register idle), latch through
  `xbox_Nv2aRegPtr()`: `[0x000100] |= 0x00001000` (PMC PGRAPH source,
  the DPC gate), `[0x400100] = 0x00000001` (software-method bit),
  `[0x400108] = param` (the `!= 0` / `& 0x40` gates),
  `[0x400704] = 0x00000100` (trap method = NOP; `& 0x1FFC` then
  `cmp 0x100`), `[0x400708] = param` — `loc_002DB2E2` dispatches
  `sub_002DB4A0(ecx = cell, eax = [0x400708])`, so the ring param
  0x80..0x9F must sit in the trap word, not just `+0x400108`.
* `kernel_hal.c dpc_run_here()` — after `xbox_DpcDrainHere()`, clear
  the latched words (`nv2a_pb_swmethod_done()`): the guest's clear
  of PGRAPH_INTR is write-1-to-clear on hardware and a write-back
  no-op on RAM, so the host owes the ack, exactly as the vblank ack
  path already does for PMC/PCRTC.
* Guest chain then runs on its own: DPC gate `0x1000` →
  `sub_002DB250` → gates pass → `sub_002DB4A0` arm 4 →
  `KeSetEvent(dev+0x1DCC = 0x002E2F5C)` → the `sub_002D57D0` ring
  wait releases. No guest-code edits.

## Mechanism proven; dispatcher table corrected (gfault2)

With the latch in place the whole guest chain services itself:
`[GSVC]` 95 and `[SWM]` 94 hits — the DPC gate opened, the pump ran,
the dispatcher ran, and the earlier "arm 4 / params 0x80..0x9F" note
was **wrong**: `sub_002DB4A0` switches on **`(param & 0x1F) - 1`**
(table at guest `0x2DB6B4`, 14 slots read from `sudeki.frz2.core`),
with `param >> 5` as a sub-argument:

| class (`param&0x1F`) | target | role |
|---|---|---|
| 1 | `0x2DB4C4` | swap-chain slot bookkeeping (`esi+0x178/0x17C/0x1C4/0x1CC`, completion ICALL `[esi+0x18C]`) — params `0x101/0x121/0x141` observed |
| **5** | **`0x2DB60E`** | **`KeSetEvent(esi+0x1A4 = 0x002E2F5C, 1, 0)` — the waited event** |
| 6,7,9 | `0x2DB625/0x2DB663` | aperture register writes (`param 0x9` observed) |
| 8 | `0x2DB63D` | PGRAPH `+0x750/+0x754` writes (`param 0x28` observed) |
| 12,13,14 | `0x2DB5CC/0x2DB5F3/0x2DB5F9` | launch-ring family (`sub_002D68C0/sub_002D5C80`) |

So the missing signal corresponds to NOP **class 5**, and the freeze
run's first 40 NOPs showed classes 1/8/9 only — a class histogram
(patch 0025 census, `[NOPSW] seen= classes=`) now answers whether
class-5 NOPs are ever emitted into the walked DMA stream.
Static checks so far: the ring buffer at `0x80066000` (dev+0x24,
128 KiB) contains **no NOP entries at all**; the walk decoder in
`nv2a_pb_scan.c` handles count/subchannel/non-increment headers
correctly, and D3D8LTCG emits one software method per `count=1` NOP
entry (the observed class-1 params prove that path works), so
"mis-decoded as 0x0104" is excluded for the *observed* marks.

## The class-5 mark exists but was dropped by the latch's coalescing rule (gfault3)

Full-run histogram: `1:2300 5:1 8:1 9:12` (seen=2314). Class 1 = one
per Present (swap NOP, matches the 2300 flips); **class 5 appears
exactly once**, at NOP `#2312` — right when the demo scene submits
its launch buffer, one word behind a swap NOP. It never reached the
dispatcher (no `[SWM]` with `param=00000005`): the first-cut
`if (*intr) return;` "trap register busy" rule **dropped it**,
because the adjacent swap NOP was latched and not yet serviced.

Hardware does not drop it: the pushbuffer pipeline serialises NOPs,
each trapping after the previous one's clear. Patch 0025 now keeps a
FIFO of pending marks (`SWM_PENDING_N=64`); `nv2a_pb_swmethod_done()`
(the post-drain ack) re-arms the trap with the next queued param, so
each drain services exactly one mark, as one trap-data register
does. (The queue full-case logs `[GFAULT] pending queue full`.)

Timeline reading: with one mark emitted and never serviced, the
launch-ring writer sits in `sub_002D57D0` (wait `0x2E2F5C`) while
flips of the last drawn frame continue — the attract-demo
`0.00 fps` freeze, end to end explained.

## gfault5/6/7 — the freeze is reproducible; the ack/drain loop is the gap

* gfault5 (sparse presses): **no freeze** — the press schedule never
  started the demo (no class-5 mark at all: `1:3356 8:1 9:12`,
  `direct=3358 enq=11 drop=0 pop=0`). Feeder lesson again: press
  densely, or the wall doesn't reproduce.
* Reproduction is stochastic even with dense presses: gfault9 (16
  presses, 8 s cadence) froze @2518 flips; frz3 (12 presses, same
  cadence) was still running at 190 s. Capture runs must therefore be
  **adaptive**: poll the log, `kill -ABRT` on three consecutive
  `0.00 fps` lines, fallback abort at 260 s (recipe in
  `logs/run-frz4-*` command; `coredumpctl dump <pid>` with the real
  direct child PID — not timeout's).
* gfault6 (dense presses, 8 s cadence from t=100 s): freeze
  reproduced at 2281 flips, exactly one class-5 mark (#2293),
  `enq=12 pop=0 drop=0`: the last 12 marks (the class-5 one among
  them) sat in the pending queue; **no `done()` ack ever ran after
  the freeze** — so no DPC drain ran after the freeze, on either
  drain path.
* The runs' vblank mode is **TICK** (`[NV2A] vblank 600 (tick): last
  600 in ~13400 ms` ≈ 45 Hz) — the `kernel_timer_paced()` false
  branch, whose 10 ms loop calls `kernel_drain_dpcs()` every pass
  (`kernel_bridge.c:3847`). So DPCs should drain ~100/s even with
  every guest thread parked — yet `pop=0` and the pump census froze
  with the queue non-empty. The probe `[VBLDBG]` (delivered-tick +
  DPC-depth census) landed on the PACED deliver path and printed
  nothing — mode is TICK; gfault7 moves the evidence to the DPC
  body itself: every pass whose gate word has `0x1000` up, plus a
  `done()` ack counter (`ack=` in the latch census line).

Open question gfault7 answers: does the DPC body itself stop
running after the freeze (drain starvation: gate held / pending
flag semantics), or does it run and never see the latched line
(latch lifetime)?

## gfault8 — 0026-v1 (hold-while-pending) wedges the guest service loop

Patch 0026-v1 (ack pass leaves PGRAPH bit0 + PMC bit12 while a mark is
"pending"; timer-drain ack) **regressed hard**: freeze at only 179
flips, `direct=1 enq=63 drop=128 pop=0 ack=0`, zero vblank tick
summaries, `[DPC]` body census ≈ 2 hits, `[GSVC]`/`[SWM]` census both
cap-pinned at 400 — every one on a stale `PARAM=00000028` (class 8).

Root cause, decisive: the guest's PGRAPH service chain (pump call
sites at `gen/recomp_0054.c:15658/15720` in the `sub_002DE53B` service
family, plus `sub_002DA72B`/`sub_002DAB87`) **loops until
`[0xFD400100]` reads clear**. The guest's own write-1-to-clear is a
plain store on RAM (writes the bit value back → still set), so the
toolkit's aperture ack pass ("hold interrupt words at zero") is the
ONLY thing that ever ends that loop. Holding the words "until acked"
wedged that loop inside the timer thread itself — the same thread the
DPC drain and vblank ticks live on; everything downstream died with
it. (The nv2a-ack *watch* thread only tracks PCRTC bits — not the
stall; the wedge is in the guest loop.)

Design correction (0026-v2): **hold the swmethod bits only from latch
until the guest's service chain has read them once** —
`nv2a_pb_swmethod_hold()` = latched && !seen; `seen` set by the manual
census wrappers at `sub_002DAE40` (DPC entry) and `sub_002DB250` (pump
entry) — the exact points where the guest reads the words. After
that, the ack pass clears as always, ending the guest loop. Ack:
`done()` (both drain paths) — if seen, clear words and pop the FIFO
(re-latch the next); if NOT seen, the mark was ack-eaten before any
service read it → re-arm the SAME param without popping (retry), so
the FIFO drains only at real service rate. Closes both failure modes:
gfault6/7's race (only 75 of 2437 marks ever seen) and gfault8's
wedge. Also fixed: the census `[SWM] idx` used `(eax>>5)&0x1F`; the
verified table key is `(param&0x1F)-1` (this session's correction).

## gfault9 — 0026-v2: the sw-method chain WORKS end to end; wall moved

hold-until-seen + re-arm-unseen + FIFO pop at service rate:
`direct=3242 enq=1149 drop=0 pop=1149 ack=2475`, no wedge (vblank
ticks 10 800 @ ~45 Hz, DPC body census 1844 real hits), class-5 NOP
observed (#2530) — and for the FIRST time:

    [EVENT] KeSetEvent token=0x002E2F5C from=0x002DB61F

the exact class-5 arm site (verified jump-table reading;
`0x2DB60E`→`0x2DB61F` is the `KeSetEvent(cell+0x1A4)` call). The ring
event `0x2E2F4C` set 2421×. Patch 0025+0026-v2 close the
latch→gate→pump→dispatch→event wall.

But the plateau persists: 0.00 fps at 2518 flips; the last frame dumps
(`reference/fb/gf9000xx.bmp`, 35 222 distinct colours) show a FULLY
RENDERED 3D scene — geometry, textures, lighting all drawn. So the
wall is no longer "GPU never notified the CPU"; it is whatever the
game waits on AFTER that — ring-space accounting in the writer
(`sub_002D57D0` compares `[ctx+0x2C]` put vs `*[[ctx+0x30]]` and
`[ctx+0x44]` segment size), a query/visibility report the executor
fakes, or a scene-script wait. Next evidence: watchdog guest-thread
samples during the plateau (`logs/run-wdsamp1-*`).

Census-budget lesson: `[SWM]`/`[GSVC]` wrappers cap at 400 and
mislead after ~30 s; `[DPC]` (per-gate-bit) and `[EVENT]` (patch
0015) are the trustworthy timelines.

At freeze the executor reports real 3D batches: 7373 VSH batches,
92 175 indices, 39 846 triangles rasterised, 863 M pixels, surface
`0x030A4000`/`0x031D0000` pitch 2560. Position attr type 5 (S32K) shows
raw ±32767 values; xemu `pgraph/vertex.c` also scales S32K by 1 (as-is),
so toolkit decode matches the reference implementation. Watch-item for
geometry sanity after the freeze is fixed.

## Outcome

(to fill in)
