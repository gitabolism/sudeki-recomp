# Note 0003 — full disc tree changes the wall: `NtWriteFile` ×42k

Date: 2026-11-21. Follows note 0002. Experiment: host-side data only —
`tools.xiso unpack` of the full disc (442 files, 3.6 GB) into gitignored
`game/`, XBE hash re-verified unchanged. Re-run:
`scripts/run.sh 30 RECOMP_WATCHDOG_SECS=18` → `logs/run-20261121-015407.log`.

## Observed (delta vs note 0002's run)

- H2 strong form **falsified**: `D:\boot.Baf` now opens
  (`[FILE] ... -> 0x00000000`, no "missing" line) — and the process still
  idles into the watchdog. But the state moved: NV2A USER `PUT/GET` now
  `0/0` (was `00065B28/00065B28`), and the recent-ICALL target changed
  `FE0000B4 → FE000044`. Bridge decode: thunk VA = 0xFE000000 + 4·slot,
  so `FE0000B4`=slot 45=ordinal 234 (`NtWaitForSingleObjectEx`),
  `FE000044`=slot 17=**ordinal 236 (`NtWriteFile`)** — the loop is now
  lifted code calling the *write* thunk through a pointer.
- Kernel census: **ordinal 236 x42,043** of 55,119 total. That is the wall:
  a write loop on an already-open handle (no new `[FILE]` lines between
  `badger.plap`/`boot.baf` opens and the watchdog).
- `Z:\badger.plap` (XDK debug-adapter plugin, shipped on the disc) now
  **opens successfully** — before the tree existed it failed. Strong
  candidate for what the writes go to, or what enabled the code path that
  writes. (Fact: opens succeed; that badger is the *target* is inference.)
- Threads: `PsCreateSystemThreadEx` fired 3× — all
  `routine=0x0026D3E4`, the XAPILIB launcher thunk, which **does** have a
  dispatch entry (`sub_0026D3E4`, verified in `recomp_funcs.h` + dispatch
  table) and workers do spawn (`spawned worker 0x0026D3E4 … stack top
  0x02611FF0`, worker-stack `esp=0x02692DAC` at the summary). The real
  entry points ride in the ctx blocks (`0x00021AD0`, `0x00021B90`) —
  H1's simple form is not the current wall.
- Unresolved ICALLs: 2 × `VA 0x00273EEF`, which sits *mid-function* in
  `sub_00273EC5` (+0x2A). Do NOT seed it (seed_from_log's guard exists
  precisely because it would clamp the containing function); this is the
  coalescence/alias case from `docs/technical/function-coalescence.md`.
  Minor: 2 of 55k calls, eax=0 pop path — not the wall.

## Hypothesis

**H3 (new, primary):** the writes are debug-plugin logging (badger.plap
path) or a console/mcstream output loop on the worker thread; the game is
spinning output while waiting for something else (likely the display/audio
readiness it polls). Falsifier: after identifying the write handle, the
target is NOT badger/console, or removing the write loop does not move
the state.

Identify the handle first — no code changes: re-run with
`RECOMP_KERNEL_LOG_BUDGET=2000 RECOMP_WAIT_LOG=1` to expose `ret=` for the
236 calls (the caller function) and correlate with `[FILE]` lines. The
caller names a lifted function; from it we read what buffer/handle the
lifted tree passes.

## Planned change

None yet — measurement only (one variable: log verbosity).

## Outcome

(fills in after the trace run — see note 0004 for the fix if any)
