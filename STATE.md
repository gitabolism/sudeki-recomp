# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-21 (session 4g)
**Stage:** LIFT complete, ENTRY evidence strong. **SOLVED the boot-archive
wall (note 0011, patch 0014):** the phantom 0x8000 was our bridge
returning `STATUS_SUCCESS` synchronously for reads on **async** handles.
Real kernel answers `STATUS_PENDING (0x103)`; the game's init
(`sub_000221C0`) then does the completion charge once instead of
double-charging (refill + init). Patch 0014 flips upstream
`RECOMP_ASYNC_IO` to default-ON. With default env + disc-exact
`game/boot.baf`: 22 reads cover the whole file, `Z:\boot.baf` =
1,820,672 B **byte-exact golden**, cache 1.8 MB, **no crash, no flood,
no padding games**. Wall now = post-decode events `0x48000001/04/06`
never signalled (HANG, idle wait).

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
  buf=` VA, `0009` `RECOMP_IO_WRITE_DUMP` content dump, `0010` `[READ]
  sz=/path=`, `0011` `[QINFO]` size-query trace, `0012`/`0013`
  `RECOMP_WATCH` arm fixes (errno, multi-page window), **`0014` async-IO
  return convention = the boot-archive fix**. Table in THIRD_PARTY.md.
  Manual overrides: `--exclude-manual src/recomp_manual.c` now wired into
  `scripts/pipeline.sh`; first wrapper = `sub_00022340` (stream refill) +
  `RECOMP_REFILL_TRACE=1`; gen tree emits the body as `sub_00022340_gen`,
  stubs still 432.
- XBE: pinned SHA-256 ✔. Gen tree regenerated session 4g with
  `--exclude-manual` (one body renamed `sub_00022340_gen`; 432 stubs).
- Skills: five pinned, digest gate green.

## Last run

Default env, disc-exact `game/boot.baf` (sha `b6e5751c…`), patch 0014
in the series: `scripts/run.sh 40 RECOMP_WATCHDOG_SECS=35
RECOMP_KERNEL_LOG_BUDGET=0 RECOMP_EVENT_LOG=1`
→ `logs/run-20261121-191748.log`: **HANG (post-decode idle)** — 22
reads cover the whole real file (final `@657408 want=22528` = END-token
region); `Cache/boot.baf` = 1,820,672 B `cmp` **byte-exact vs golden**;
cache 1.8 MB, no flood, no crash. Steady state: 70 finite waits on
`0x48000004` (worker B poll), pump INFINITE `{0x48000007,1}`, main
INFINITE on `0x48000002` (its own rendezvous event, set once, consumed).

## Solved this session (session 4g, note 0011)

1. **The phantom 0x8000 = the sync-read return convention (patch 0014).**
   First manual-override wrapper (toolkit `--exclude-manual` contract,
   now in `pipeline.sh`; `RECOMP_REFILL_TRACE=1`) showed refill #1
   charging 0x8000 AND init (`sub_000221C0` @0x22215) charging it again —
   double-charge of the prefetch segment, only reachable when NtReadFile
   answers `STATUS_SUCCESS` synchronously on an **async** handle (open
   options `0x4C`: no FILE_SYNCHRONOUS bit; bridge already records such
   handles). Hardware answers `STATUS_PENDING`; the game's flag-2 consume
   path then charges once. Patch 0014 = upstream `RECOMP_ASYNC_IO`
   default-ON, `=0` opts out.
2. Consequence: boot archive decodes **byte-exact on the disc-exact file**
   with default env (44 refill calls vs 9,948; no flood, no crash). The
   note-0010 padded-file result was compensating errors, not a lever.
   H14/H15 closed; no file-slack decision needed (queue item 3 dropped).
3. xemu's identical 0x9E000 stop is xemu answering the same convention
   wrongly — consistent, not corroborating. Reference-comparison caveat
   updated in note 0011.

## Established in session 4f (note 0010)

1. **Stream boundary** *(superseded by item 7 below — tail is real
   stream)*: reads/prefetch/budget close at 0x9E000 under our kernel
   answers; disc XDVFS entry size 0xA6000, LBA 0x4BFD; guest output
   matches the offline decoder byte-exact to output 0x16BAF6 (where the
   starved runs diverge).
2. **Read/budget accounting closes**: budget init 0xA5800
   (filesize−0x800), 3×0x8000 prefetch, chunk ops to zero; 21 file reads
   (header + 19×0x8000 + 0x5800) end at file 0x9E000. One 0x8000 budget
   charge has no `[READ]` (suspect the flag-2 refill branch at 0x2236F,
   `budget -= [+0x3C]`). **QINFO (patch 0011) proves the kernel size
   answer was correct** (`eof=0xA6000`) — the read-stop is game logic,
   byte-identical to xemu's STOP RAM dump (same quartet: budget
   0xA5800-region, total-read 0x9E000, chunk 0x5800, ret `0x1F335`).
3. **Decoder has no EOF exit** (disasm: only the dist==0 token returns,
   `je 0x226ED`; refill at budget==0 returns without data and the byte
   iterator reads past `+0x14` into ring residue). Post-EOF garbage
   re-consumes the guest's own output (the first divergent literals sit
   inside the contiguous arena, not in boot.baf). Both worlds overrun
   the output window identically.
4. **1 GiB cache cap → same CRASH**: the disk-full flag amplifies but
   does not trigger the overrun.
5. Tools: `baf_decode_offline.py` gained `--eof N` clamp and
   `--tokens FILE` token trace; docstring records the padding fact.
6. Patches 0011 `[QINFO]`, 0012/0013 `RECOMP_WATCH` fixes committed;
   very wide watch windows destabilize the run — keep windows small and
   pick the stack parity (0x02681/0x02691/0x02692 pages) from a probe
   run first.
7. **CORRECTION (post-script 3 of note 0010, decisive):** padding
   `game/boot.baf` +0x8000 (diagnostic, since reverted — sha back to
   `b6e5751c…`) made the decode **complete**: `Z:\boot.baf` = 1,820,672
   B, `cmp` byte-exact vs golden; reads covered the whole file
   (final `@657408 want=22528`). The 0x9E000 read-stop is the game's
   accounting being exactly 0x8000 short (phantom charge), not a file
   boundary; the tail is real stream. Post-decode the title still
   waits on unsignalled events `0x48000001/04/06` — the next wall.

## Established earlier (notes 0005-0009)

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
7. **xemu reference baseline is installed and boots the title to our wall**
   (note 0009, THIRD_PARTY.md "Reference emulator"): xemu v0.8.139 AppImage
   + clean-room SpiritBoot-release + xemu-dashboard HDD; only `release`
   image with `-m 128` gets past BIOS. Screen shows `STOP 0x7E
   (0xC0000005, EIP 0x000226D8, 0x84FB8B1C, 0x84FB8820)` — fault EIP is
   inside `sub_000223F0`'s byte-output loop (verified by capstone on the
   disc: `0x226d8 mov [eax],bl` with flush `call 0x22270` at 0x226d0),
   dest cursor past mapped RAM. Both environments mis-signal the stream
   end; a clean title-screen reference needs correct drive/memory
   geometry on the xemu side too. VRAM contention with the inference
   engine ruled out (xemu uses ~80 MiB; screen renders fine alongside).
   Scripts: `xemu_ref.sh`, `xemu_kill.sh`, `xemu_boot_probe.sh`,
   `xemu_standalone.sh`.

## Live hypotheses

1. **H14/H15 CLOSED (session 4g, note 0011):** the phantom 0x8000 was
   the sync `STATUS_SUCCESS` return on async handles; patch 0014
   (async PENDING default) makes the boot-archive decode complete
   byte-exact on the disc-exact file. No file-slack/geometry theory
   needed at all.
2. **H16 (primary, post-decode wall, refined by core+budget runs):**
   three-way deadlock after the pump drains the boot-archive task:
   worker A pump `sub_001EABD0` INFINITE `{0x48000007, 0x48000001}`;
   worker B `sub_00021B90` polls `0x48000004` forever; main INFINITE on
   `0x48000002` (set once by main itself @esp `0x00F7FB0C`, auto-reset,
   consumed by an earlier wait). Task posters that set 1/4
   (`sub_00021CD0`@0x21D7F, `sub_000220E0`@0x22147) never run because
   main is parked. Open question: main's `sub_00011DE0` waits token 2
   *after* setting it once itself — likely it waits **twice** by design
   and expects a second set from a producer that our semantics starved,
   or the XAPI create (all `type=0` Notification) conflates a
   manual-reset/semaphore variant. Investigate main's chain
   `sub_00011560 → sub_00012090 → sub_00011DE0` + `sub_0026D47C`.
3. **H17 (check first, cheap): async reads may have unblocked other
   subsystems too** — compare the new run's ordinal histogram and file
   activity vs `logs/run-20261121-172449.log`: more ordinals / new
   opens (D:, X:, Y:) would say the title gets further overall.
4. **H13b/H13c dead**: the xemu HDD image holds only an erased
   `boot.baf` cache-dir entry; xemu runs the title from the same ISO
   XDVFS (size 0xA6000) we do — hence byte-identical read totals.
   Unlimited-cap run (cache cap off): the decoder writes forever
   (4.1 GB @ 45 s) — that was the starved-geometry behaviour only.
5. H9 (FATX mount/geometry) still queued behind these.

## Queue (next concrete actions)

1. **Read main's stall chain statically**: `sub_0026D47C` (XAPI
   launch?), `sub_00011560`, `sub_00012090`, `sub_00011DE0` — what
   should produce the second `0x48000002` set; check XAPI
   `XCreateEvent`/`XCreateEventEx` (manual reset) usage in the game.
2. **H17 cheap diff:** ordinal histogram + `[FILE]` opens of
   `logs/run-20261121-191748.log` vs the 4f reference run.
3. If main's wait is legitimately first-set-by-another-thread, find the
   producer path (likely `sub_00012090`'s callee list) and why it never
   posts (event 1/4 posters `sub_00021CD0/220E0` never called).
4. The decoder-object construction-site question is no longer load-
   bearing (T=0x8000 site + events 0x48000009/0A at +0x8/+0xC) — keep
   only if H16 needs it.
5. Keep the offline tool as the oracle: `--eof 0xA6000` clamp; golden =
   1,820,672 B confirmed by guest (two independent worlds now: padded
   run AND async-default run, same bytes).
6. Cleanup between runs: `rm -f ~/.local/share/xboxrecomp/Cache/*`
   (no longer GBs with the fix, but keep the habit).

## Blockers / open questions

- Post-decode 3-way event deadlock (note 0011 post-scripts): who owns
  the second `0x48000002` set, and why task-posters `sub_00021CD0/
  sub_000220E0` (events 1/4) never run — main parked first.
- xemu's bugcheck (note 0009) must be re-checked against the async
  semantics before using its STOP screen as a "same wall" baseline —
  xemu likely mis-answers the same convention; a corrected xemu
  reference may be possible (config/quirks) or may need its own patch.
- Whether the 786 MB of `Z:\boot.baf` written before disk-full was ever
  correct behaviour — explained (starved decoder under the old sync
  answers); verdict reserved until the reference pipeline matches.

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
| 2026-11-21 s4e | **xemu reference installed (user-approved) & boots the title to OUR wall**: SpiritBoot-release + `-m 128` → `STOP 0x7E` inside `sub_000223F0` byte loop (EIP 0x226D8, flush `call 0x22270`) — same decoder, same runaway cursor as note 0007; VRAM-forensics recipe (FB @0x3C00000, pitch 720) reconstructs the bugcheck screen as `reference/xemu/` artefact; VRAM contention with inference engine ruled out; note 0009 + `scripts/xemu_{ref,kill,boot_probe,standalone}.sh` + THIRD_PARTY reference table | (this commit) |
| 2026-11-21 s4f | **boot archive decoded end-to-end (note 0010)**: padded-size diagnostic (+0x8000, reverted) → `Z:\boot.baf` = 1,820,672 B byte-exact vs golden, no flood/crash; wall = game's byte accounting starves reads exactly one 0x8000 segment (phantom refill charge) under our+xemu kernel answers; QINFO patch 0011 (size answers correct), watch fixes 0012/0013, core final-state recipe (`obj = esp+0x78`), tool `--eof/--tokens`, xemu-HDD H13b dead, cap-off run proves no natural termination (4.1 GB/45 s); next: events 0x48000001/04/06 unsignalled post-decode | (this commit) |
| 2026-11-21 s4g | **THE phantom 0x8000 solved (note 0011): patch 0014 async-IO PENDING default = upstream-correct return convention on async handles → decode completes byte-exact with default env on disc-exact boot.baf (22 reads, 1.8 MB cache, HANG not CRASH)**; manual-override contract wired (`--exclude-manual` + `sub_00022340` wrapper + REFILL_TRACE); refill/init double-charge proven; patch 0015 event create/wait/set caller trace; SIGABRT-core thread-map recipe; post-decode wall reframed = 3-way event deadlock (worker pump {7,1}, worker B polls 4, main INFINITE on its own-consumed 2) | (this commit) |
