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

## Outcome
(fill in AFTER the experiment: what was true)
