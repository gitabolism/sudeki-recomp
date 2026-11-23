# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-23 02:1x (session 5a, started 22:0x)
**Stage:** LIFT complete, ENTRY strong, **MENU artefact reached**
(xemu same-moment capture still owed, see Queue);
**navigation works**; load-stall wall (H31, re-posed x9, home = note
0025) at heartbeat-producer level: the servicing chain
`loader wait -> EE190 pulse -> EE110 -> EDA20 tick -> 23B830 pump`
runs 167 iterations and dies at pump x167 (job `0A260002`, manager
`018CF14C`, size `0x1400` = one 5120-B record); the job is NOT stuck
by any data gate — at kill the pump gate is structurally OPEN (job
byte0=0, flags eligible, handle resolvable). The loader then waits
INFINITE `{48000007, XAPI-msg event}` and every wake/pulse producer
falls silent; frz125 (`[SMEVT]` on `26D5AE` SetEvent + `[F0F80]`
queue census + `POST` task trace) names the wake that should fire
pulse #6 but doesn't. ONE regime (fresh vs reused cache identical,
frz121); loader events DO get Set (5 pulses = 5 Sets) — "events
never Set" is dead.

## Where things are verified

- Toolkit: `xboxrecomp/` @ v0.18.0 / `37b61d589168adee3680fabd51805ee81427bf07`;
  `scripts/doctor.sh` green (XBE SHA-256 pin, skill digests, stubs
  432 agree). Patch series **`0001..0051`**; the patch-by-patch table
  lives in THIRD_PARTY.md (its one home); `scripts/build.sh` resets
  to the pin and re-applies in order.
- Manual overrides (`src/recomp_manual.c`, `--exclude-manual`): census
  wrappers incl. PUMP/S9720/S99D0/EE110/EE190(ENQ)/MTICK/EED70/CQ/
  SMEVT/F0F80/POST/REFILL + cache-writer/audio/swap wrappers.
- Raw-bytes decode law (verified against lift): `.text` file offset =
  VA − 0x11000 + 0x1000 (capstone direct on the XBE; the earlier
  0x1F0000-offset decode was garbage — deleted from note 0025).
  Kernel thunk placeholders `[0x3156xx]=0xFE000000+i*4` are the real
  bridge VA encoding (slot = index into the XBE import table).
- Ghidra bench installed; flat-import recipe in THIRD_PARTY.md; the
  `/tmp/opencode/ghidra` project was lost to a host reboot — no
  re-import needed so far (capstone raw decode replaced it).
- Git: pushed `origin/main = f4495af` per `git reflog` (4 pushes;
  last two postdate the recorded user-approved push `7ce51cb`).
  **User question (rule 7):** AGENTS says the remote is `NO_PUSH`,
  but `remote.origin.pushurl` is a live SSH URL and pushes happened
  after `7ce51cb`. Confirm posture (re-approved pushes, or restore
  NO_PUSH) — agent keeps pushing nothing either way.
- JP second sample: `~/Downloads/xbox/sudeki-jap/default.xbe`
  (parked, outside git; naming `<game>-<region>/`).

## Last run

Session 5a (22:0x–02:1x+): **frz107→125 census arc; H31 anatomy
complete to the heartbeat producer; patches 0048–0051.**
- Terminal shape (deterministic, fresh AND reused cache): PUMP x167
  = last servicing call ever (bypass site, mgr `018CF14C`, id
  `0A260002`, node key `EA3D4B48`, backtrace `1EDB68<-1EE160<-
  1EE221`); EE110=167, MTICK=167, CQ(23AD50), F9720 — ALL silent
  after (frz124 per-event EE110 probe: last x166 2 lines before
  x167; 100k log lines of attract-movie IO only).
- Pump raw decode: async/sync branch = `job[+8]` bit2 (`0x06610B21`
  -> sync `F9720`); tick pump gate = `MEM8(job)==0` — OPEN at kill
  (frz117/123 core forensics; core reader law: find ram base via the
  `FE000000..FE000008` triple at `[0x3156C0]`).
- Loader wait anatomy (raw): `EABD0` (single caller `21B1B`) holds
  CS `37A0F8` across INFINITE wait on `{48000007, [0x397920]}`;
  `EAAB0` 5-ms cycle ("poll" `[0x315818]` = RtlTryEnterCS);
  `F0F80` processes worker queues `[0x3D771C/720/740]` (all null at
  kill) and its handshake Sets the loader events (5 Sets = 5
  pulses); `EF1D0` bucket walk (bucket `37A120+((tick>>6)&31)*4`,
  all-empty at kill) waits on the loader container via `EAAB0`;
  `26D5AE/CE`=Set/ResetEvent, `26D77A/78C`=WFSO/WFMO, `26D54D`=
  CreateEvent.
- `[ENQ]` wrapper IS `EE190`: 4-5 calls EVER per run = the pulses.
  Pulse servicing = per-manager `EE110 -> EDA20 -> pump` walks.
- Install integrity proven: write map covers `0..27,762,688` once,
  no gaps; the polled 17-position tail slots ARE zeros the guest
  itself installed; the cycling reads are the attract movie.
  No level `.baf` ever opens. Boot-era `startup.baf` (disc,
  8,585,216 B) -> installed `Cache//startup.baf` 27,762,688 B.
- Regime split REFUTED (frz121). Patch 0050 (pread/pwrite = NT FPO
  semantics) correct, not the wall. Patch 0051: writes dispatch
  `case 236 -> xbox_NtWriteFile` directly — the bridge-side `[WRITE]`
  log + patch-0005 capacity check never see the real writes
  (decision owed, see Queue).
- /review audit ran (fresh context, this session): artefacts verified
  (patches no gaps, cores, hashes, doctor green, stubs 432); drift
  applied by this compaction (regime/trailer-bit4/events-never-Set
  deleted from the header; queue items 1-2 were moot). Audit's
  direction verdict: H31 is the critical path AND the closest
  artefact — keep, but close the Menu side-by-side gap in parallel.

Earlier sessions live in the notes (0001–0011, 0016–0025) + session
log.

## Live hypotheses

1. **H31 (ACTIVE, note 0025, RE-POSED x9): the post-x167 wake never
   fires, so the heartbeat that would finish job `0A260002` never
   runs.** Candidates named and being measured by frz125: (a) task
   producer `21CD0`/`220E0` (POST trace) stops posting the next-leg
   task upstream of the loader; (b) `26D5AE`-routed SetEvent sites —
   SMEVT census names which handshake died; (c) worker-queue refill
   (`[0x3D7720]` pending list) — F0F80 census proves emptiness.
   Data-side gates all REFUTED (job byte0=0, queue lists null,
   buckets empty, install complete).
2. H14–H30 CLOSED — one-line lessons index (homes = notes
   0001-0011/0016-0024): cache capacity ends the boot write-loop
   (0005); async-IO return convention = the phantom charge (0011);
   ICALL-skip stack drift (0018); AC97 trap arch gap (0019 era,
   patch 0022); input = arm OHCI + scripted-pad START bit (0020);
   sw-method latch/ack chain (0021); `ke_shadow_init` double-init
   (0022); load-state pump pause is the game's own design (0023);
   service-chain livelock (0023); D3D RAM events need shadow mirrors
   — `RECOMP_TITLE_KEVENTS=1` part of the recipe (0024).

## Queue (next concrete actions)

* **1. frz125 verdict (IN FLIGHT, log `logs/run-frz125-020517.out`)**:
  `[SMEVT]` ra+handle for every loader-event Set; `[F0F80]`
  `[0x3D7740/71C/720]` census; `[POST]` task-producer entries.
  Decision tree: POST silent post-x167 -> who gates the poster
  (task queue consumer?); POST fires but SMEVT no Set -> consumer
  side; SMEVT fires on other handles -> map the missing Set's owner
  (guest site) and why it never runs.
* **2. Menu artefact completion (rule 6, one run-window): xemu
  title-screen capture for side-by-side with `reference/fb/frz35*`
  dumps** — needs the QMP/HMP headless fix (note 0019 recipe).
  Interleave between frz cycles; `reference/xemu/` today holds only
  the STOP screen.
* **3. Write-path decision**: capacity enforcement (patch 0005)
  sits on `bridge_NtWriteFile` which the guest never uses (patch
  0051 log proves raw thunk route). Move it into `kernel_file.c` or
  retire it — affects disk-full experiments only.
* **4. Then the Gameplay gate:** first presented level frame =
  fbpeek + visual + texture-coverage carry-over; xemu side-by-side
  of the SAME load as corroborator only (item 2's script fix serves
  both).
* Carry-over: prior-art hour unspent (upstream Discord "Sudeki"
  search; Black-2006 `black`-branch runtime fixes for streaming/APU
  — BRIEF Prior art); JP disc regression sample later; roadmap
  staging (vanilla -> Vulkan -> features) per the user directive;
  post-playable feature order named by the user (2026-11-22):
  HDR -> Raytracing -> Multiplayer (LAN/VPN co-op + split-screen);
  prior art `github.com/sudeki-together/sudeki-mp` (SudekiMP —
  hooks GOG `SUDEKI.exe`, same SOL engine; AGPL-3.0; deep engine RE
  docs; reference value only — any code reuse is a license call for
  the user; never vendored without it).

## Blockers / open questions

- **H31 wall write-up (AGENTS work-loop rule):** 30+ runs
  (frz93→125), 9 re-poses, 3 header-level facts overturned mid-wall
  (regime split, trailer-bit4, events-never-Set). No host-side
  exoneration yet: with the job gate provably open and queues empty,
  the missing piece is a game-side producer; if frz125 exonerates
  all three candidates, next re-pose compares against a real
  reference trace (xemu kernel trace of the same load — item 2
  serves).
- User question (rule 7): push posture — see "Where things are
  verified" Git bullet.

## Housekeeping facts (recipe home)

- Working recipe (frz92+ regime, nav_run2x): `RECOMP_VBLANK=1
  RECOMP_PB_EXEC=1 RECOMP_PB_WALK=dma RECOMP_KERNEL_LOG_BUDGET=0
  RECOMP_AC97_READY=1 RECOMP_USB=1 RECOMP_ARM_UNTIL_SEEN=0
  RECOMP_TITLE_KEVENTS=1` + FB dumps + census (`RECOMP_IO_READ_LOG=1
  RECOMP_IO_WRITE_LOG=1 RECOMP_IO_WRITE_MAX=0 RECOMP_GATE_TRACE=1
  RECOMP_PRES_TRACE=1 RECOMP_EVENT_LOG=1 RECOMP_GSVC_TRACE=1
  RECOMP_RD_TRACE=1 RECOMP_MISSING_FILES=all RECOMP_POST_TRACE=1
  RECOMP_LOG_ORDS=123,138,145,205,225`).
- Harness trio current: `/tmp/opencode/nav_run32.sh`,
  `frz_watch*.sh`, `core_forensics.bt` (+`core_ram.py` Core class —
  ram-base finder above). `/tmp/opencode` ephemeral.
- Other useful env: `RECOMP_CACHE_CAP_BYTES`, `RECOMP_WATCH=<va>…`,
  `RECOMP_SEM_TRACE`, `RECOMP_LOG_ORDS=…` (patch homes: THIRD_PARTY
  table).
- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env,
  skills_check}.sh`; logs -> `logs/`, captures -> `reference/`
  (gitignored); cores via `coredumpctl dump <pid>` (~2.4 GB each —
  23 now / 9.8 GB in /tmp/opencode; prune to frz117/121/123-era at
  next pause); cache dir `~/.local/share/xboxrecomp/Cache/*`
  between long runs.
- Shell law: never edit a script a live bash re-reads; never rebuild
  `build/sudeki` while a run/core is in flight (copy the binary to
  `/tmp/opencode/sudeki-frzNN.bin`); watcher stdout block-buffered;
  kill by exact pid or `pgrep -x sudeki` (NEVER `pgrep -f`).
- Seeds file (local, 18 measured VAs): `logs/seeds-icall-0011C640.json`.
- Deterministic markers: PUMP x167 terminal, EE110=167, baf-read
  counts vary by kill timing (don't pin on them); regime pinned in
  run header from now on.
- Git: never push (rule 7); local commits through `524d2f6`; every
  agent commit this session is factory/docs only.

## Session log (chronological; HH:MM = commit times)

| date/window | what happened | commit |
|---|---|---|
| 11-21 ~01:00–02:13 s1 | pinned v0.18.0, pipeline, lift 14,741 fn / 432 stubs, Linux exe builds, first run alive | `eca6f9f` |
| 11-21 s2 | autonomy layer; full disc tree extracted; wall framed (XAPILIB cluster) | `55b8693` |
| 11-21 02:13–02:34 s3 | skills pinned + drift gate; note 0004 Partition5 bootstrap | `73a8d93` `17aa020` |
| 11-21 05:09 s4a | note 0005 cache writer; **patch 0005 cache capacity: HANG->CRASH** | `35cb8a4` |
| 11-21 05:22 s4b | notes 0006-0007; placement patch 0006; core-analysis recipe; wall = decoder never terminates | `0f5068d` |
| 11-21 05:25 s4c | note 0008 `[READ] buf=`; watch pre-arming ruled out | `1f038e6` |
| 11-21 16:48 s4e | xemu reference installed, boots to our wall (STOP 0x7E reconstructed) | `1f1db4a` |
| 11-21 17:03–17:56 s4f | boot archive decoded end-to-end (note 0010); patches 0011-0013 | `95f12a0`..`91d5f2f` |
| 11-21 19:19 s4g | **phantom 0x8000 = async-IO convention (patch 0014)**; decode byte-exact default | `7faa86f` |
| 11-21 19:58 s4h | boot reaches intro + first GPU flips (measured seeds; PB_EXEC/VBLANK recipe) | `5ce7e66` |
| 11-21 21:31 s4i | flip-done settle chain; TLS guard; level streaming reached; audio root-cause | `ecbd691` |
| 11-21 21:50 s4j | **audio wall CLOSED (patch 0022, AC97 trap arch gap)** | `c640f54` |
| 11-21 23:18 s4k | **H23 CLOSED; first content frames incl. MENU title dump** | `b169ce3` |
| 11-22 00:36 (+23:50) s4m | **input wall CLOSED (OHCI + pad START bit)**; xemu playability fixed (64 MB ROM) | `203b268` `795b4ef` |
| 11-22 02:10–02:37 s4n | **H26+H27 CLOSED** (patches 0024-0027); roadmap staging directive | `40c8b4c` `457810a` |
| 11-22 05:01 s4q | **H28+H19 CLOSED**; frz18 nav recipe (Buki card); README lands (user) | `8e7c8ed` |
| 11-22 06:17–06:26 s4r | H29 anatomy complete; pre-push audit; **user pushed** | `af91f07` `57ea06b` |
| 11-22 07:59–12:38 s4s/4t | **H29 livelock CLOSED (0034/0035)**; regime matrix frz62-83 (0039-0046); frz82 proved depth=luck | `f5df7d8`..`c168fa0` |
| 11-22 12:38–17:55 s4u | frz87 deepest = title-era luck; pump-chain anatomy; Ghidra bench; close-out | `105e357`..`f00a8bf` |
| 11-22 18:50–19:23 s4v | **H30 ROOT-CAUSED (note 0024)**; frz91 probe; user-approved history scrub + rule 12 | `19b5f4f` `ff20cad` |
| 11-22 19:56–22:3x s4w | **H31 framed (note 0025)**; frz92 GPU-wall-dead verdict chain; patch 0047; /review harness + first audit + README sync | `c4b9be4`..`7ce51cb` |
| 11-22 22:0x–11-23 02:1x s5a | frz107-125 census arc; H31 re-posed x9 to heartbeat-producer level; patches 0048-0051; raw-decode law; regime split refuted; frz125 in flight; /review audit #2 | `230ede7`..`524d2f6` |
