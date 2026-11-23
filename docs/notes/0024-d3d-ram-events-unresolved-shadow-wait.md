# Note 0024 — D3D hand-built RAM events resolve to garbage handles; the ring-free wait is the terminal spin (H30 root cause)

Date: 2026-11-22. State: frz90 (note 0023 4v section) was the first
matched-symbol PLATEAU core with current symbols; it killed the
"+0x400700 spin" theory and pointed at a host resolution gap.

## Observed (frz90: plateau 3966 flips, watcher ABRT t=431 s; core
`/tmp/opencode/sudeki.frz90.core`, forensics `/tmp/opencode/forensics-frz90.txt`)

* Terminal thread map: thread 1 (engine/main guest thread)
  `sub_001E0390` (submit) -> `sub_002D57D0_gen` -> `kernel_thunk_dispatch`
  -> `bridge_KeWaitForSingleObject` -> **`ke_shadow_lookup` (frame #0)**;
  no `pthread_mutex_lock` beneath (NOT the frz4/H27 signature);
  `g_ke_shadow_cs` healthy in the core (allocated `LockSemaphore`,
  RecursionCount 0).
* The wait target is the ring-free event: writer loop `loc_002D5920`
  calls ICALL `0x3157E4` (KeWaitForSingleObject) on `ctx+0x1DCC`
  = **0x2E2F5C** and retries (`cmp eax,0; jne`) until the wait
  returns 0. No escape hatch in the loop.
* Guest RAM at the plateau: 0x2E2F5C SignalState=1, wait-lists empty,
  quartet/spin words all idle (same idle baseline as frz89).
* Census (log `logs/run-frz90-181030.out`): `KeSetEvent token=0x2E2F4C`
  x4534 (from 0x2DB0BE, 1:1 with flips), `KeSetEvent 0x2E2F5C` x1
  (from 0x2DB61F, the H26 class-5 chain); `[SWAP] issue=6245
  wait=7 pump=eng=3732`; frames end on the STATIC title (d=0.00 %,
  35,765 colors, dump 9000035+) — nav STARTs (guest t=124.8-154.8 s,
  PAD census fired correctly, digital 0x0200) never navigated.
* Shadow table scan (gdb on core, all 4096 `g_ke_shadow` slots):
  **no entry for 0x2E2F4C, no entry for 0x2E2F5C**. `bridge_KeInitialize*`
  is the only inserter and was never called on them (no census lines)
  — D3D8LTCG builds these events with its statically-linked XBOXKRNL,
  entirely inside guest RAM.

## Why the climb survived anyway (static, lifted C + toolkit source)

* Every wait/set/reset on the two tokens takes the fallback chain
  `ke_shadow_lookup` miss -> `bridge_resolve_handle` miss ->
  `XBOX_TO_NATIVE(guest_va)` = *native pointer to the guest word*,
  which kernel_sync.c:286 documents verbatim: "An unresolved object
  arrives as a value the shim does not know, and the wait **fails with
  STATUS_UNSUCCESSFUL**... It must not be dereferenced to find that out."
  So all 4534 `KeSetEvent(4C)` were no-op `SetEvent(garbage)`.
* The credit gate (sub_002D3420, `loc_002D3474`) exits its loop on the
  return of the **following `KeResetEvent`** — `bridge_KeResetEvent`
  returns 0 unconditionally — so a failed wait there terminates anyway:
  fire-and-forget pacing (the engine ran at the vblank 1.8 fps rhythm,
  never event-paced).
* The writer ring-free wait (`loc_002D5920`) polls `eax==0` with no
  such exit: **the first time ring free space fell below 0x2000**
  (flip ~3966; host GET lags under scene load) the wait was reached,
  failed instantly forever, and the engine thread spun — 0 fps, IO
  threads still streaming, watchdog silent. That IS the H30 terminal
  state across the whole luck band (189..17435 flips = when GET lag
  first crosses the threshold).

## Hypothesis (one variable)

Making bridge Wait/Set/Reset treat header-shaped guest-RAM dispatcher
objects as RAM-truth events with a host mirror (upstream
`ke_guest_event`, kernel_bridge.c:8170) resolves both tokens, exits
the ring-free loop, and re-paces the credit gate honestly. The
classifier accepts our headers: word 0x00040000 -> Type byte 0
(notification), byte 2 = 4.

## Planned change

Probe only (no rebuild): run frz91 = the fixed frz90 harness
(v4 regime + nav gate + watcher + `RECOMP_GSVC_TRACE=1`) +
`RECOMP_TITLE_KEVENTS=1`. Falsification: plateau unchanged at similar
depth -> theory dead for this wall; also watch the frz34-43 side
effect (XAPILIB `HcControl` unprogrammed, zero `[OHCI0]` lines -> no
PAD lines): that regression predates patches 0023/0037 and is
re-testable. If OHCI dies, fallback = patch 0047 scoping the
classifier (host-side, toolkit).

## Outcome (frz91, `logs/run-frz91-185035.out`, core `/tmp/opencode/sudeki.frz91.core`, forensics `/tmp/opencode/forensics-frz91.txt`)

* Half-true: the mirror gap was real and `RECOMP_TITLE_KEVENTS=1`
  fixed the resolution — "built by the title" for `0x002E2F4C`
  (notification) + `0x00F7FC7C` (sync); frz91 core `g_ke_shadow` scan
  finds entries for both (never for `0x2E2F5C` — no wait/set on it
  occurred in that run). The XAPILIB/OHCI regression did NOT recur
  (`[OHCI0] raised 00000002 -> ISR claimed it` works, but
  enumeration landed at t=209.5 s — the `.bik+120 s` nav gate pressed
  at 170-200 s, so that run's nav attempt was void; harness fixed to
  gate on the ISR-claimed line).
* Falsified as THE fix: plateau unchanged — 1,593 flips, watcher ABRT
  t=420 s, late dumps 9100035-42 static (d=0.00 %).
* Morphology flipped from livelock to quiet deadlock: thread 1 now
  genuinely blocks (`wait_single`/`pthread_cond_wait`), chain
  `sub_00252AF0..sub_0011B020->sub_001DE310->sub_001DE5C0->
  bridge_KeWaitForSingleObject`. Lifted C (recomp_0034.c:9027+):
  `sub_001DE5C0` waits via ICALL slot `0x3157E4` on `ctx+0x1DBC` =
  **0x2E2F4C** — the mirrored swap-slot event. No thread anywhere in
  `sub_002D57D0` (writer) or the `sub_0011ED20` device-message chain;
  `[SWAP] issue=3960` climbing, `pump=eng=1593` frozen, `wait=5`;
  `[SWM]` latches still fire from `002DB2E9`; PGRAPH_INTR=1;
  BEW channel-block sem `+0=0x260b` (patch 0031 chain healthy);
  guest RAM `0x2E2F4C` SignalState=0, `0x2E2F5C` SignalState=1.
* The paradox (frz91, unconfirmed which side): completion
  `KeSetEvent(0x2E2F4C)` from `0x2DB0BE` keeps firing to kill, the
  shadow resolves both sides (host event mirrored), yet the engine's
  INFINITE wait never returns. Leading hypotheses: (a) notification
  event with a competing waiter — the credit gate `sub_002D3420` was
  seen INFINITE on the same token in frz79 — each pulse wakes the
  gate, engine starved; (b) a wake-ordering race where the pulse
  lands while the engine is outside the wait and the notification
  auto-reset eats it before re-entry.
* Kept as insurance: `TITLE_KEVENTS` stays in the regime (honest
  pacing beats fire-and-forget). Next probe (frz93, wrappers only,
  no toolkit patch): entry/wait counters on `sub_002D3420` (gate) vs
  `sub_001DE5C0` (engine) vs `sub_002DAFF0` (completion set) — catches
  the stealer or the lost pulse directly.

## 4v addendum — frz92 (pad-driven nav + all three mirrors): the GPU wall is DEAD

frz92 (`logs/run-frz92-190958.out`, core `sudeki.frz92.core`): the
ring-free wait was REACHED and the 5C mirror built on demand
(`[KERNEL] event 0x002E2F5C built by the title`, line 26504); the
class-5 chain ran fully — `KeSetEvent(5C)` ~1100x `from=0x2DB61F`
(H26 chain alive, vs x1 in frz90). Everything ALIVE at the
"plateau": ring writer 25/s from both sites, `submit=20112` climbing,
iGET draining, alloc failures 0; engine loop ~10 fps
(`[GATE] pass=5506`, 70.4 M skip decisions = "load in progress");
IO streaming LEVEL assets — speech banks `speech_hask_b/haskilia/
shadow/TSA.xwb`: navigation (8 STARTs after pad enumeration) REACHED
A LEVEL LOAD; presented frame = 99.6 % loading card.
`[SWAP] pump2/eng2` (unconditional counters) pin at exactly 4096
while submits continue — swap-pool accounting quirk, NOT a stall.
GATE job-queue at kill: `tot=0x5F9 acc=0x5AD` (95 % accepted, gap
~76) — the era-guarded watcher's flip criterion false-fired on a
HEALTHY load era and ABRTed seconds from load completion. v2 watcher
counts GATE `acc` deltas as progress; frz93 = same regime, 2500 s
window, no early kill: the Gameplay-gate attempt (navigation ->
presented level frame). Guest RAM `4C SignalState=1` pending at core
time with the main thread mid-body (not in a wait): the LTCG-inlined
dispatcher fast-path mutates event words directly (`MEM32(ctx+0x1DC0)
= 0` preambles at both 4C wait sites), so mirror/RAM coherence is
real — but no longer wall-relevant in this regime.

