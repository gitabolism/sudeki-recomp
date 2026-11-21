# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-21 (session 3)
**Stage:** LIFT complete, ENTRY evidence strong. Current wall: **XAPILIB
utility-drive bootstrap — Partition5 FATX bring-up retry loop** (old
"loader stalls mid-read" framing is dead; the read completes — note 0004).

## Where things are verified

- Toolkit: `xboxrecomp/` @ v0.18.0 / `37b61d589168adee3680fabd51805ee81427bf07` (`scripts/doctor.sh` all ok). Checkout kept clean; local changes are patch files, see below.
- Local toolkit patches: `patches/xboxrecomp/0001-io-write-trace.patch`
  (`[WRITE]` trace, env `RECOMP_IO_WRITE_LOG=1`) — auto-applied by
  `scripts/build.sh`, recorded in THIRD_PARTY.md.
- XBE: `game/default.xbe` pinned SHA-256 ✔. `game/` = full disc tree
  (442 files, 3.6 GB, gitignored).
- Generated tree unchanged since session 1 (432 stubs agree; note 0001).
- Executable: `build/sudeki` (rebuilt this session with the [WRITE] patch).
- Skills: five pinned skills installed, digest gate in `doctor.sh` green
  (commits `3b10d34`, `73a8d93`).

## Last run

`scripts/run.sh 60 RECOMP_WATCHDOG_SECS=50 RECOMP_MISSING_FILES=all RECOMP_IO_WRITE_LOG=1 RECOMP_KERNEL_LOG_BUDGET=10`
→ `logs/run-20261121-022812.log`: **HANG (watchdog abort)**. Facts:

- `boot.Baf` fully read from `D:\` (24 sync reads, last `@624640 want=22528
  got=22528` = 679,936 total — session-2's "stops at 657,408" was a miscount).
- Copy to `Z:\boot.baf` (`~/.local/share/xboxrecomp/Cache/boot.baf`),
  4096-byte positioned writes, all `st=0`, **offset grows unbounded** —
  ~614.4 MB per 10k writes, file hit 4,295,024,640 B (≈ 2^20+4 blocks,
  near a 4 GiB/32-bit wrap). Deleted at handoff (sparse, harmless).
- Sequence: open `Partition5` → `NtFsControlFile(0x90020)` → **stub →
  STATUS_NOT_SUPPORTED** → mount/copy proceeds → retry-ish forever, with a
  poller thread in 500 ms `NtWaitForSingleObjectEx` timeouts on event
  `0x5A000024` (guest token `0x48000004`, manual-reset created unsignalled
  at log line 227).
- gdb-on-bridge probing is a dead end: ptrace breaks the runtime's
  SIGSEGV/SEH emulation (spurious `sub_0026D079` crash). Use env-gated
  instrumentation instead (`scripts/gdb_probe.sh` kept for host-side-only
  debugging).

## Live hypotheses

1. **H6 (primary):** the copy/mount loop's exit predicate depends on FATX
   volume facts the stub never gives — FSCTL `0x90020` (FILE_ANY_ACCESS
   function 8) returns `STATUS_NOT_SUPPORTED`, and POSIX writes never fail
   at partition capacity, so the guest never sees "volume ready / disk
   full" and loops. Falsifier: implement/answer `0x90020` correctly (or
   cap the Partition5 image) and the loop terminates or errors forward.
2. **H5 (secondary):** a guest-side 32-bit block-count wrap (host file
   stopped exactly near 2^20 blocks ≈ 4 GiB) — if the guest would stop at
   capacity on real hardware, the only host defect is *allowing* the
   overshoot; fixing capacity semantics alone may suffice.

## Queue (next concrete actions)

1. Identify FSCTL `0x90020` from public XDK/XAPILIB headers (GitHub code
   search is auth-gated; fetch raw header files, e.g. cxbx/xbe headers —
   webfetch proxy is flaky, prefer `curl`). Note 0005 before code changes.
2. Implement `bridge_NtFsControlFile` host-side for that code against the
   Partition5 image (`~/.local/share/xboxrecomp/Partition5.img`); one
   variable at a time. Consider FATX capacity semantics for cache images.
3. After the loop resolves, re-run and check whether event `0x5A000024`
   finally gets SetEvent (launch-complete) → next wall (APU poll, note 0002).
4. Reference baseline (xemu) still queued; ask user before installing
   software (approval gate). Session paused by user frequently — machine is
   also used for Kodi; keep GPU-heavy steps off (game never touches GPU,
   but keep runs bounded and clean the toolkit cache between long loops).

## Blockers / open questions

- FSCTL 0x90020 identity unknown (headers fetch pending). Candidates in
  the FATX FSCTL family (mount/dismount/lock/geometry) — do not guess,
  read the guest's input buffer at the call instead if headers stay elusive.
- Toolkit checkout is dirty *after* a build (patches auto-applied) — by
  design; `doctor.sh` verifies the pin commit, not tree cleanliness.

## Housekeeping facts

- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env,skills_check,gdb_probe}.sh`.
  Logs → `logs/`, captures → `reference/` (gitignored).
- Useful env: `RECOMP_MISSING_FILES=all` (every open logged),
  `RECOMP_IO_WRITE_LOG=1` (needs patch 0001, auto), `RECOMP_KERNEL_LOG_BUDGET`,
  `RECOMP_WATCHDOG_SECS`.
- OpenCode layer: `opencode.jsonc` + commands `/resume /status /run /cycle
  /note /lift /handoff /setup`; skills under `.opencode/skills/` (pinned).
- Remote origin push URL is `NO_PUSH`. Do not "fix" this.

## Session log

| date | what happened | commit |
|---|---|---|
| 2026-11-21 s1 | pinned v0.18.0, hashes verified, full pipeline, lift 14,741 fn / 432 stubs, Linux exe builds, first run alive at timeout | `eca6f9f` |
| 2026-11-21 s2 | autonomy layer (AGENTS/STATE/scripts/notes); full disc tree extracted (notes 0002-0003); wall framed as XAPILIB read/write/wait cluster | `55b8693` |
| 2026-11-21 s3 | skills pinned + drift gate (`3b10d34`,`73a8d93`); note 0004: read completes, wall = Partition5 FATX bootstrap retry loop (FSCTL 0x90020 stub, unbounded cache writes); [WRITE] patch + gdb-probe lessons; Kodi GPU incident investigated (not repo-caused) | (this commit) |
