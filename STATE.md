# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-21 (session 4)
**Stage:** LIFT complete, ENTRY evidence strong. Current wall: **the guest
faults on Xbox VA `0x84000000` — the contiguous window (and other apertures)
the loader could not map on this host.** The old wall (XAPILIB/utility-drive
write loop) is **solved** by toolkit patch 0005; run classification moved
from HANG to CRASH, i.e. real progress.

## Where things are verified

- Toolkit: `xboxrecomp/` @ v0.18.0 / `37b61d589168adee3680fabd51805ee81427bf07`
  (`scripts/doctor.sh` green). Checkout clean at the pin; local changes are
  patch files auto-applied by `scripts/build.sh` — see THIRD_PARTY.md.
- Local toolkit patches now: `0001` `[WRITE]` trace, `0002` `[EVENT]` trace,
  `0003` write-CSV + guest backtrace, `0004` FSCTL probe (env-gated),
  **`0005-cache-volume-capacity.patch` = the fix** (note 0005).
- XBE: `game/default.xbe` pinned SHA-256 ✔. `game/` = full disc tree (gitignored).
- Generated tree unchanged since session 1 (432 stubs agree; note 0001).
- Skills: five pinned, digest gate green.

## Last run

`scripts/run.sh 90 RECOMP_WATCHDOG_SECS=80 RECOMP_EVENT_LOG=1 RECOMP_MISSING_FILES=all RECOMP_KERNEL_LOG_BUDGET=200`
→ `logs/run-20261121-050812.log`: **CRASH** (was HANG before patch 0005).

- `boot.Baf` copied to the utility drive until the volume is full:
  `[FILE] cache volume full (used 786371584 +61440, cap 786432000) … from 0x002710FF`
  → the title's writer (`sub_00022270`) then returns E_FAIL and the launch
  path proceeds. New ordinals appear that never did during the loop
  (187 `NtClose`, 190, 184, 67).
- Then: `[CRASH] … Xbox VA of fault: 0x84000000 in sub_000223F0+0x2F3B`,
  frames `0x95800 0x22234 0x1BC800 0x1F335 0xA5800 0x9E000 0x1F0E8 0x1E60A
  0x21C1F 0x26D456`. This is the window the loader warned about at startup:
  `contiguous window at 0x80000000 (native 0x800002800000) failed (error 8)`,
  plus NV2A (0xFD000000), MCPX (0xFE800000), flash (0xFF000000) and tiled
  (0xF0000000) apertures all unmapped.

## Solved this session (facts, artefacts in note 0005)

1. The write loop is the **game's own cache writer** (backtrace: call site
   `0x000222E0` into the XAPI write helper `sub_002710AA` ← frames
   `0x226D5/0x22234/0x1F335/0x1F0E8/0x1E60A/0x21C1F`), not XAPILIB.
2. Its only exit is a short/failed write; a host directory has no capacity,
   so it wrote 94,643 × 61,440 B (>5.8 GB, one 32-bit wrap) forever.
3. H7 (FSCTL mount failure drives it) **falsified** with patch 0004.
4. The `0x5A000024` / `0x48000004` wait is a symptom: nothing ever signals
   that event (`RECOMP_EVENT_LOG` shows exactly one signal all run,
   `0x48000002`).
5. gdb/ptrace on the bridge is unusable with this runtime (SIGSEGV-based SEH
   emulation), so env-gated instrumentation is the debugging route.

## Live hypotheses

1. **H8 (primary, new):** the crash at `0x84000000` is purely the unmapped
   contiguous/AGP window — the toolkit's attempt collides with its own guest
   RAM mirror reservation (`occupant: base 0x800002800000 … state 0x10000`).
   Falsifier: map the window (and apertures) successfully at startup and the
   fault disappears / moves.
2. **H9 (deferred gap):** the guest's own FATX superblock for Partition5 says
   `bytes_per_sector=1, sectors_per_cluster=0, cluster_count=0xFFFFFFFF`
   because `IoMountVolume`/geometry is not implemented. Patch 0005 emulates
   capacity at the write layer; a title that trusts that geometry could still
   mis-size. Not the current wall, but it is the honest follow-up.

## Queue (next concrete actions)

1. Note 0006 first: read the loader's aperture mapping in
   `xboxrecomp/src/…/xbox_memory_layout.c` (why "error 8", what native
   address it asks for), then map the 64 MB contiguous window at
   `0x80000000` + the tiled/NV2A/MCPX/flash apertures host-side (patch 0006).
   One variable: contiguous window alone, then re-run.
2. Re-run and re-classify; expect the wall to move to APU/GPU register
   access (the other "will fault" warnings) or to the `0x5A000024`
   launch-complete event once the window exists.
3. Consider implementing FATX mount/geometry (FSCTL family) instead of the
   capacity approximation (H9) once the window gap is closed.
4. Reference baseline (xemu) still queued — ask user before installing
   software (approval gate). Keep runs bounded; the toolkit cache dir
   (`~/.local/share/xboxrecomp/Cache`) can grow to the volume capacity —
   clear it between long runs.

## Blockers / open questions

- Why the aperture mmap fails on this host (error 8) and whether the fix is
  a different native base for the mirror reservation or explicit MAP_FIXED
  holes — measure before patching.
- Patch 0005 caps cache volumes only; Partition1-backed saves (TDATA/UDATA)
  remain unbounded by choice, recorded in THIRD_PARTY.md.

## Housekeeping facts

- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env,skills_check,gdb_probe}.sh`.
  Logs → `logs/`, captures → `reference/` (gitignored).
- Useful env: `RECOMP_MISSING_FILES=all`, `RECOMP_IO_WRITE_LOG=1`,
  `RECOMP_IO_WRITE_CSV=<path>`, `RECOMP_EVENT_LOG=1`, `RECOMP_FSCTL_OK=1`,
  `RECOMP_CACHE_CAP_BYTES=<n>` (default = backing image size, 0 disables),
  `RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_WATCHDOG_SECS`.
- OpenCode layer: `opencode.jsonc` + commands `/resume /status /run /cycle
  /note /lift /handoff /setup`; skills under `.opencode/skills/` (pinned).
- Remote origin push URL is `NO_PUSH`. Do not "fix" this.

## Session log

| date | what happened | commit |
|---|---|---|
| 2026-11-21 s1 | pinned v0.18.0, hashes verified, full pipeline, lift 14,741 fn / 432 stubs, Linux exe builds, first run alive at timeout | `eca6f9f` |
| 2026-11-21 s2 | autonomy layer (AGENTS/STATE/scripts/notes); full disc tree extracted (notes 0002-0003); wall framed as XAPILIB read/write/wait cluster | `55b8693` |
| 2026-11-21 s3 | skills pinned + drift gate; note 0004: read completes, wall = Partition5 FATX bootstrap retry loop; [WRITE] patch | `17aa020` |
| 2026-11-21 s4 | note 0005: write loop is the game's cache writer; H7 falsified; **patch 0005 cache capacity → loop ends, wall moves to unmapped 0x80000000 window (HANG→CRASH)**; instrumentation patches 0002-0004 | (this commit) |
