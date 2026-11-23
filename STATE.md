# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-23 02:5x (session 5a, 22:0x–02:5x)
**Stage:** LIFT complete, ENTRY strong, **MENU artefact reached**
(xemu same-moment capture still owed, see Queue);
**navigation works**; load-stall wall (H31, re-posed x12, home = note
0025) in ONE sentence: the engine never re-issues the next load leg
(`1F1470` fires ONCE whole-run, `1F1870` never), so the loader
heartbeat (`1EACC0`) stays idle, the sole wake producer (`1EAB90`
-> evt6, 5 Sets ever) goes silent, and loader servicing (EE110 ->
tick -> pump, terminal x167) starves — while job `0A260002` is
still registered on manager `018CF14C` with its pump gate OPEN.
ONE regime (frz121); loader thread alive (attract-era REQ); 17/17
thread starts dispatched; module globals all NULL at kill (frz126
core); frz128 probes (ENQ flag+state, SRST) committed, NOT yet run.

## Where things are verified

- Toolkit: `xboxrecomp/` @ v0.18.0 / `37b61d589168adee3680fabd51805ee81427bf07`;
  `scripts/doctor.sh` green (XBE SHA-256 pin, skill digests, stubs
  432 agree). Patch series **`0001..0051`**; the patch-by-patch table
  lives in THIRD_PARTY.md (its one home); `scripts/build.sh` resets
  to the pin and re-applies in order.
- Manual overrides (`src/recomp_manual.c`, `--exclude-manual`): census
  wrappers incl. PUMP/S9720/S99D0/EE110/EE190(ENQ)/MTICK/EED70/CQ/
  SMEVT/SRST/F0F80/POST/REFILL/EACK/EACYC/EBUSY/EDRV/ISSU1/ISSU2 +
  cache-writer/audio/swap wrappers. All source changes are complete
  units, committed; `build/sudeki` currently carries the frz127-era
  probes — the frz128 probes (ENQ flag/state, SRST) are committed
  but need the gen+build in Queue 1 before they take effect.
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

Session 5a (22:0x–02:5x): **frz107→127 census arc; H31 anatomy
complete to the issuer; patches 0048–0051.**
- frz127 verdict (decisive): `[ISSU1]` (`1F1470` issue-and-service
  loop) = **1 call whole-run** (line 12331, boot era, ctx
  `014B6300`, `ctx+0x34`=2, caller inside `1F3310` region);
  `[ISSU2]` (`1F1870`) = 0; `[EDRV]` (`1EAD70` wait-sessions) = 4,
  all instant non-busy; `[EBUSY]` (`1EACC0`) alive all run
  (x4096+, engine-frame ra `1E0C1E`), always idle after ~x1555.
- frz126: `[EACK]` (`1EAB90`) = 5 total, ALL from `1EACC0` ack
  branch, last 16 lines before x167; core: module globals all NULL.
- frz125: `[SMEVT]`/`[POST]`/`[F0F80]` census -> every `NtSetEvent`
  is wrapper-routed (`from=26D5BA`); **48000007 never Set**; q1
  posts alive to line 52291 (poster NOT the gap); q2 boot-era only.
- Terminal shape (deterministic, fresh AND reused cache): PUMP x167
  = last servicing call ever (bypass site, mgr `018CF14C`, id
  `0A260002`, node key `EA3D4B48`); EE110=167, MTICK=167, CQ,
  F9720 — ALL silent after.
- Wake pair law decoded (frz125/126): evt7/evt6 + flags `37A0F4/F5`;
  loader wakes ONLY on evt6 Sets from `1EAB90` (5/run, from `1EACC0`
  ack branch); EAAB0 request path dead by construction (needs evt6
  Set first); EE190 wake branch never reached — OPEN contradiction
  (its FLAG=1 calls should Set evt7; frz128 settles it).
- frz126 core: async-load module all-NULL at kill (idle) while job
  `0A260002` still on manager; frz127 proved the issuer loops
  (`1F1470`/`1F1870`) fire once/never.
- Pump raw decode: async/sync branch = `job[+8]` bit2; tick pump
  gate = `MEM8(job)==0` — OPEN at kill (frz117/123 cores).
- Loader wait anatomy (raw): `EABD0` WFMO{48000007,48000006};
  `21AD0` = consumer loop body (drains q1 keyed by `task[+0xC]`);
  `21CD0` routing law (flag bits {15,9,10,11,12} -> q2/`220E0`);
  q2 = boot-era only; `1EAD70` = main-loop load-wait sessions
  (caller `11AF57`); `1EACC0` = per-frame heartbeat+busy-check.
- Install integrity proven (frz119 write map 0..27,762,688 no gaps);
  attract-movie IO explains tail-slot cycling; no level `.baf` ever
  opens. Regime split REFUTED (frz121). Patch 0050 correct, not the
  wall; patch 0051 raw-write log (capacity-check decision owed).
- /review audit #2 ran + applied (drift deleted, README Status
  honesty pass, ETA 2–4 sessions).

Earlier sessions live in the notes (0001–0011, 0016–0025) + session
log.

## Live hypotheses

1. **H31 (ACTIVE, note 0025, RE-POSED x12): the engine never
   re-issues the next load leg.** `1F1470` fires once (boot, caller
   in `1F3310` region), `1F1870` never; heartbeat `1EACC0` alive
   but always idle; wake supply `1EAB90`->evt6 dries with the
   idleness. Two open edges: (a) WHO is supposed to issue leg N+1 —
   candidates are the loader relay itself (manager `EBF20` static-
   descriptor dispatch, `00F95xxx` tables) and the worker chain
   `F0DF0/F0F80` (only 11 calls whole-run); supporting datum:
   reservation-add probe `[RESADD]` = **0 calls whole-run** while
   `[RESREL]` = 21 (frz125) — the reserve side never runs at the
   wall. (b) the EE190 FLAG=1/evt7 contradiction (frz128 probes
   committed). Host-side exoneration not yet proven either way.
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

* **1. frz128 run (probes already committed, just gen+build+run)**:
  `[ENQ]` logs flag + evt7/evt6 state per call and `[SRST]`
  censuses `26D5CE` ResetEvents — settles the EE190 FLAG=1/evt7
  contradiction and maps all 8 request decisions. Command:
  `cd xboxrecomp && python3 -m tools.recomp ../game/default.xbe --all --split 250 --gen-dir ../src/recomp/gen --game-name sudeki --exclude-manual ../src/recomp_manual.c && bash scripts/build.sh && cp build/sudeki /tmp/opencode/sudeki-frz128.bin && sed 's|nav_run11|nav_run35|g; s|reference/fb/127|reference/fb/128|g' /tmp/opencode/nav_run34.sh > /tmp/opencode/nav_run35.sh && rm -rf ~/.local/share/xboxrecomp/Cache/* && bash /tmp/opencode/nav_run35.sh 128 1200`
  (kill early once `[PUMP] x167` appears in the log).
* **2. Leg-issuer hunt (next after frz128):** symbolize the lone
  `[ISSU1]` caller precisely (bin `/tmp/opencode/sudeki-frz127.bin`
  + core `sudeki.frz127.core`, ra `0x55b8e99426e2`); decode the
  loader relay leg-registration — `EBF20` manager-descriptor
  dispatch (`00F95xxx` tables) and why `[RESADD]`=0 while
  `[RESREL]`=21 (which API should call the reserve side at the
  wall). Probe that API entry in frz129.
* **3. Menu artefact completion (rule 6, one run-window): xemu
  title-screen capture for side-by-side with `reference/fb/frz35*`
  dumps** — needs the QMP/HMP headless fix (note 0019 recipe).
  Interleave between frz cycles; `reference/xemu/` today holds only
  the STOP screen.
* **4. Write-path decision**: capacity enforcement (patch 0005)
  sits on `bridge_NtWriteFile` which the guest never uses (patch
  0051 log proves raw thunk route). Move it into `kernel_file.c` or
  retire it — affects disk-full experiments only.
* **5. Then the Gameplay gate:** first presented level frame =
  fbpeek + visual + texture-coverage carry-over; xemu side-by-side
  of the SAME load as corroborator only (item 3's script fix serves
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

- **H31 wall write-up (AGENTS work-loop rule):** 34+ runs
  (frz93→127), 11 re-poses, 3 header-level facts overturned mid-wall
  (regime split, trailer-bit4, events-never-Set). No host-side
  exoneration yet: with the job gate provably open, module globals
  null and 17/17 threads dispatched, the missing piece is a
  game-side issuer; if frz127 exonerates EBUSY/EDRV/ISSU, next
  re-pose compares against a real reference trace (xemu kernel
  trace of the same load — Queue item 2 serves).
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
- Harness trio current: `/tmp/opencode/nav_run34.sh`,
  `frz_watch*.sh`, `core_forensics.bt` (+`core_ram.py` Core class —
  ram-base finder above). `/tmp/opencode` ephemeral.
- Other useful env: `RECOMP_CACHE_CAP_BYTES`, `RECOMP_WATCH=<va>…`,
  `RECOMP_SEM_TRACE`, `RECOMP_LOG_ORDS=…` (patch homes: THIRD_PARTY
  table).
- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env,
  skills_check}.sh`; logs -> `logs/`, captures -> `reference/`
  (gitignored); cores via `coredumpctl dump <pid>` (~2.4 GB each —
  26 now / ~11 GB in /tmp/opencode; prune to frz117/121/123/126/127
  era at next pause); cache dir `~/.local/share/xboxrecomp/Cache/*`
  between long runs.
- Shell law: never edit a script a live bash re-reads; never rebuild
  `build/sudeki` while a run/core is in flight (copy the binary to
  `/tmp/opencode/sudeki-frzNN.bin`); watcher stdout block-buffered;
  kill by exact pid or `pgrep -x sudeki` (NEVER `pgrep -f`).
- Seeds file (local, 18 measured VAs): `logs/seeds-icall-0011C640.json`.
- Deterministic markers: PUMP x167 terminal, EE110=167, baf-read
  counts vary by kill timing (don't pin on them); regime pinned in
  run header from now on.
- Git: never push (rule 7); local commits through `b0204cd` (+ this
  close-out); every agent commit this session is factory/docs only.

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
| 11-22 22:0x–11-23 02:5x s5a | frz107-127 census arc; H31 re-posed x12 to issuer level (1F1470 once/1F1870 never; wake supply evt6 x5 only); patches 0048-0051; raw-decode law; regime split refuted; /review audit #2; frz128 probes committed | `230ede7`..`b0204cd` |
