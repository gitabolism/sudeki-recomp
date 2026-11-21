# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-21 (session 4, late)
**Stage:** LIFT complete, ENTRY evidence strong. Current wall: **the
title's boot-archive *decoder* thread never terminates** — it decodes
`boot.baf` into unbounded `Z:\boot.baf` writes, so the launch-complete
events (`0x48000001`, `0x48000004`) are never signalled and the title
polls forever (HANG). The old walls (write-loop, window mapping) are
solved/instrumented; classification with default env: **HANG**.

## Where things are verified

- Toolkit: `xboxrecomp/` @ v0.18.0 / `37b61d589168adee3680fabd51805ee81427bf07`
  (`scripts/doctor.sh` green). Checkout kept at the pin; local changes are
  patch files under `patches/xboxrecomp/`, and `scripts/build.sh` now
  resets `src/` to the pin and re-applies the series **in order** every
  build (the old per-patch idempotency check broke once patch 0002
  touched patch 0001's lines).
- Patches: `0001` `[WRITE]` trace, `0002` `[EVENT]` trace, `0003` write
  CSV + backtrace, `0004` FSCTL probe, **`0005` cache-volume capacity =
  the write-loop fix**, `0006` span placement insurance, `0007`
  `RECOMP_CONTIG_ALIAS=1` aperture probe (default off). Table in THIRD_PARTY.md.
- XBE: pinned SHA-256 ✔. Gen tree unchanged since session 1 (432 stubs).
- Skills: five pinned, digest gate green.

## Last run

`scripts/run.sh 90 RECOMP_WATCHDOG_SECS=80 RECOMP_EVENT_LOG=1
RECOMP_MISSING_FILES=all RECOMP_KERNEL_LOG_BUDGET=200 RECOMP_CONTIG_ALIAS=1`
→ `logs/run-20261121-051703.log`: **HANG** (crash-free).

- Cache volume fills at the derived 786 MB cap → writer takes its
  disk-full branch → game opens `Z:\`, writes `Z:\badger.plap` (512 B),
  then all threads idle. Main polls `0x48000004` (finite timeouts)
  forever; worker #1 (ctx `0x00021AD0`) blocks INFINITE on
  `0x48000001` + `0x48000006`; only `0x48000002` is ever signalled.
- Without `RECOMP_CONTIG_ALIAS=1` the same flow instead faults exactly
  at guest `0x84000000` (end of the 64 MB contiguous window) in the
  decoder (`sub_000223F0`) after ~94k writes — the E_FAIL path leaves
  the destination cursor unreset and it walks off the window (note 0007).

## Solved / established this session (notes 0005-0007)

1. The write loop was the game's cache writer; cache capacity (patch
   0005) ends it the way hardware does. Commit `35cb8a4`.
2. H7 (FSCTL mount) falsified; `0x90020` stub is a real gap but not the wall.
3. Window placement: nondeterministic top-of-address-space spans break
   all five high windows at once (x86-64 canonical limit) — patch 0006
   re-places the span low. But **mapping is not the wall** (H8/H11
   dead): with everything mapped, the guest still lands at `0x84000000`.
4. `sub_000223F0` is a **bitstream decoder**, not a copy: `boot.baf` =
   679,936 B compressed, header `0x001BC800` (1,820,672 B uncompressed).
   Reads are byte-exact vs disc (verified `game/boot.baf`), one pass;
   writes of 61,440-byte chunks continue unboundedly → **the decoder
   never sees end-of-input** → worker #2 never finishes → the two
   launch events never get signalled → HANG. That is the wall.
5. **Post-mortem core analysis works** with this runtime (unlike live
   gdb/ptrace): `coredumpctl dump <pid> -o /tmp/opencode/sudeki.core` +
   `gdb build/sudeki core` gives host backtraces, the exact faulting
   instruction, and guest memory via `g_xbox_mem_offset`. Use it.
6. Thread-stack bases shift between runs (racy) — `RECOMP_WATCH`
   pre-arming on stack slots can't be done reliably; watch heap/object
   addresses instead (verify stability first).

## Live hypotheses

1. **H10 (primary):** the decoder's input delivery never reports EOF —
   refill (`sub_00022340` ← XAPI read helper `sub_00270FBD`, read site
   `0x271012`) keeps feeding stale ring data after the 679,936-byte
   source is consumed, so the bit iterator loops forever. Falsifier:
   instrument refill (buffer VA + returned length) and/or `RECOMP_WATCH`
   on the decoder object's source cursor/end fields (`+0x10/+0x14`) and
   show whether `+0x10` reaches `+0x14` and what happens then.
2. **H12 (alternative):** the decoder mis-reads its source because of a
   window/mirror aliasing difference: the toolkit deliberately backs the
   contiguous window (0x80000000) with **separate storage** from RAM and
   its 0xC0000000+ mirrors, but on hardware all three alias the same
   DRAM. If the game reads via one alias and decodes via another, the
   decoder sees garbage. Falsifier: in a core, search for the last-read
   chunk bytes (e.g. `75 00 ec fc d5 16 82 d9` = boot.baf @624640)
   across RAM, mirrors and the window — inconclusive so far because the
   ring rotates decode output over the file chunks; needs a stopped-time
   capture earlier in the flow (watchdog core mid-copy, not at crash).

## Queue (next concrete actions)

1. Note 0008 first: extend the `[READ]` trace (patch 0001 family) to
   print buffer VA + caller for reads from `0x00271012`, and watch the
   decoder object fields; run with alias on, capture a core ~10 s after
   the first write (watchdog abort gives one), and test H12 by signature
   search at a *fresh* moment.
2. Identify the `.baf` container format offline (python re-encode of the
   lifted decoder structure): if 1,820,672 output bytes are reachable
   from the disc bytes, input delivery is confirmed broken, not format.
3. Keep H9 (FATX mount/geometry) queued; patch 0005 approximates it.
4. xemu reference baseline still awaiting user approval (approval gate:
   installing software). Ask.

## Blockers / open questions

- Where exactly the decoder's EOF/termination signal comes from (its
  code is lifted, so observation = watch on its object + read-side
  instrumentation, not breakpoints).
- Whether the 786 MB of `Z:\boot.baf` written before disk-full is even
  correct behaviour — unanswerable without the reference capture (rule 6).

## Housekeeping facts

- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env,skills_check,gdb_probe}.sh`;
  `build.sh` resets toolkit `src/` to the pin, then applies `patches/xboxrecomp/*.patch` in order.
- Logs → `logs/`, captures → `reference/` (gitignored). Cores →
  systemd (`coredumpctl`), dump to `/tmp/opencode/` (~2.1 GB each).
- The run writes up to the cache cap (~786 MB) into
  `~/.local/share/xboxrecomp/Cache/` per run — `rm -f` that dir between
  long runs.
- Useful env: `RECOMP_MISSING_FILES=all`, `RECOMP_IO_WRITE_LOG=1`,
  `RECOMP_IO_WRITE_CSV=<path>`, `RECOMP_EVENT_LOG=1`, `RECOMP_FSCTL_OK=1`,
  `RECOMP_CACHE_CAP_BYTES=<n>`, `RECOMP_CONTIG_ALIAS=1`,
  `RECOMP_WATCH=<guest-va>` + `RECOMP_WATCH_RAW=1`,
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
| 2026-11-21 s4a | note 0005: write loop is the game's cache writer; H7 falsified; **patch 0005 cache capacity → loop ends (HANG→CRASH)**; instrumentation patches 0002-0004 | `35cb8a4` |
| 2026-11-21 s4b | notes 0006-0007: placement patch 0006 (canonical-limit), contig-alias probe 0007; core-dump analysis pins the fault to the boot-archive decoder; **wall = decoder never terminates → launch events never signalled (HANG)**; build.sh patch series made deterministic | (this commit) |
