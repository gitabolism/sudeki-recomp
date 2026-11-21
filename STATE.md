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
  `RECOMP_CONTIG_ALIAS=1` aperture probe (default off), `0008` `[READ]
  buf=` VA. Table in THIRD_PARTY.md.
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

1. **H10 (primary, refined by note 0008):** input delivery is *correct*
   (reads land in the same window the decoder reads, byte-exact, valid
   decoded output) but the decoder still never terminates. Refill
   `sub_00022340` returns 0 on every path including exhausted-source;
   termination must come from the decoder's own `+0x10/+0x14` checks or
   a stream terminator. Question: after the last 22,528-byte chunk, what
   do `+0x10/+0x14` become and why does the bit iterator keep running?
2. **H12 dead** (note 0008): the 21 `boot.baf` reads alternate
   `buf=0x80034000`/`0x8003C000` — the same contiguous arena the decoder
   references. No alias mismatch.
3. Pre-arming `RECOMP_WATCH` across two runs does **not** work: worker
   stack tops shift (+0x1000) even with identical env. Instead:
   one-shot dump the object from a SIGABRT core (search the thread stack
   by pointer *families*, not fixed quartets — fields rotate
   pointer/count semantics), or write the offline `.baf` decoder.

## Queue (next concrete actions)

1. Write the offline `.baf` decoder (python, from the lifted structure +
   disc bytes): expected output ≈ 1,820,672 B from 679,936 B (triplet →
   8-byte entry expansion, verified on the first 96 bytes of
   `Z:\boot.baf` vs disc @2048). If the python decoder terminates
   cleanly, the guest's non-termination is a state-divergence to hunt
   with a one-shot object dump from a SIGABRT core.
2. One-shot object dump: SIGABRT core mid-copy (18 s recipe), scan
   worker #2's stack for the stream-object pointer family (fields
   rotate semantics — search `0x8003xxxx` clusters), then read
   `+0x10/+0x14/+0x24/+0x28/+0x3C/+0x40` at that instant.
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
| 2026-11-21 s4b | notes 0006-0007: placement patch 0006 (canonical-limit), contig-alias probe 0007; core-dump analysis pins the fault to the boot-archive decoder; **wall = decoder never terminates → launch events never signalled (HANG)**; build.sh patch series made deterministic | `0f5068d` |
| 2026-11-21 s4c | note 0008 + patch 0008 (`[READ] buf=`): reads land in the decoder's own arena (H12 dead), output is valid decoded data (triplet→8-byte expansion), refill returns 0 on every path; watch pre-arming ruled out (stack jitter); SIGABRT-core recipe added | (this commit) |
