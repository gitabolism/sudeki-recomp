# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-21 (session 2)
**Stage:** LIFT complete, ENTRY evidence strong. Current wall: **XAPILIB
loader cluster stalls after reading 657,408/679,936 bytes of `boot.Baf`.**

## Where things are verified

- Toolkit: `xboxrecomp/` @ v0.18.0 / `37b61d589168adee3680fabd51805ee81427bf07` (`scripts/doctor.sh` all ok).
- XBE: `game/default.xbe` pinned SHA-256 ✔. **`game/` now holds the full
  disc tree** (442 files, 3.6 GB, gitignored; `tools.xiso unpack`, XBE
  re-hashed after).
- Generated tree unchanged since session 1 (432 stubs agree; note 0001).
- Executable unchanged: `build/sudeki`.

## Last run

`scripts/run.sh 90 RECOMP_WATCHDOG_SECS=80 RECOMP_KERNEL_LOG_BUDGET=60`
→ `logs/run-20261121-015639.log`: **HANG (watchdog abort)**, 196,640
kernel calls total. Facts:

- `[READ]` log advances @0 → … → **@624640 (24 reads, all `st=0`
  STATUS_SUCCESS), then stops** at 657,408 of 679,936 bytes of `D:\boot.Baf`.
- `tid=1003` loops `[WAIT] NtWaitForSingleObjectEx handle=0x5a000024
  timeout=500ms result=258 (STATUS_TIMEOUT)` forever, interleaved with
  `NtWriteFile` calls returning success.
- Write-loop callers map to lifted fns (XAPILIB loader cluster, all
  statically-detected, dispatch entries fine):
  `sub_002717D6` (write loop @0x2719B9/@0x271A4E), `sub_00271466`
  (opens @0x27158E), `sub_00270FBD` (read loop @0x271012), `sub_00270877`
  (@0x270AAF). All near entry region 0x26C703 — this is XAPILIB title/
  asset bring-up code running lifted.
- `Z:\badger.plap` (XDK debug plugin shipped on disc) now **opens
  successfully** since the full tree exists (was FILE_NOT_FOUND).
- Threads: 3× `PsCreateSystemThreadEx routine=0x0026D3E4` (XAPI launcher
  thunk, HAS dispatch entry; workers spawn and run — worker esp in the
  summary). H1's simple form is dead.
- Unresolved ICALLs: only 2× `0x00273EEF`, which sits **mid-function** in
  `sub_00273EC5` → coalescence/alias case, do NOT seed it
  (`docs/technical/function-coalescence.md`).

## Live hypotheses

1. **H3 (primary):** the 500 ms-timeout waits on event handle
   `0x5a000024` (token `0x48000004`) are the boot worker waiting for an
   I/O completion that the runtime never signals for the *last* (22,528-byte
   remainder) read, or for the write whose event the loop polls. Falsifier:
   trace shows the completion path firing correctly while the guest still
   stalls → then it's a guest-side predicate (badger/console path).
2. **H4:** the write loop is debug-plugin (badger.plap) logging enabled by
   the plugin now being found; removing/renaming `game/badger.plap` (host
   data only!) would falsify/confirm. One variable at a time — do this as
   a probe run, revert after.

## Queue (next concrete actions)

1. Read the lifted C of `sub_002717D6` + `sub_00271466` (gen tree, read
   only) to identify the write handle/event and which XAPI API they
   implement (likely `XapiLaunchComplete`/`XapiInitFileSystem` family).
   Note 0004 first.
2. Probe A (host data, reversible): rename `game/badger.plap` → run → diff.
3. Probe B: find where the runtime signals I/O completion events
   (toolkit `kernel_file.c` / `IofCompleteRequest` path) and check against
   what the guest waits on; if the toolkit's HLE misses the completion,
   fix host-side (never guest).
4. Reference baseline (xemu) still queued; ask user before installing
   software (approval gate).

## Blockers / open questions

- Need to identify handle `0x5a000024`/token `0x48000004` creation — the
  creation line fell outside the kernel log budget; raise
  `RECOMP_KERNEL_LOG_BUDGET` or find the object at startup (NtCreateEvent
  lines near boot show handles `0x5A00002C/0x5A000030` — 24 predates them).
- Windowed runs may open a window on the user's Wayland/Xwayland desktop;
  capture stays file-based (`RECOMP_FB_DUMP` etc.).

## Housekeeping facts

- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env}.sh`
  (usage in headers). Logs → `logs/`, captures → `reference/` (gitignored).
- run.sh: exit classification fixed for watchdog-abort (game exits 3 from
  watchdog abort; verdict now "HANG (watchdog abort)").
- kernel_calls census counts only detailed lines (RECOMP_KERNEL_LOG_BUDGET
  caps them; totals are in `[KERNEL] summary:` lines).
- Remote origin push URL is `NO_PUSH`. Do not "fix" this.

## Session log

| date | what happened | commit |
|---|---|---|
| 2026-11-21 s1 | pinned v0.18.0, hashes verified, full pipeline, lift 14,741 fn / 432 stubs, Linux exe builds, first run alive at timeout | `eca6f9f` |
| 2026-11-21 s2 | autonomy layer (AGENTS/STATE/scripts/notes); full disc tree extracted (notes 0002-0003); wall is now the XAPILIB loader read/write/wait cluster | (this commit) |
