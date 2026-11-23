# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-23 17:0x (session 5d/5e cont., ~13:0x–…)
**Stage:** LIFT complete, ENTRY strong, **MENU artefact reached**
(xemu same-moment capture still owed, see Queue);
**navigation works; the game reaches a fully rendered "Press START"
title and CYCLES it as an attract loop (frz172/174 FB dumps)**;
load-stall wall (H31, re-posed x31, home = note 0025) in ONE sentence:
the old "stuck Loading" picture was partly a HARNESS ARTIFACT (frz171
law); un-killed, the load machinery CYCLES and renders the Press START
title — but the SOL **scene/object-graph tier stays dead for the whole
pill era** (MSG/SWEEP/MKLU/SCH/CTOR census ALL x0, frz174); the
**xref-scan-bug overturn (session 5e)** showed the tier family DOES
have static refs (SWEEP = F1CA0 dispatcher cmd 0x37 leg; MSG1 callers
= .rdata-table methods; CTOR called from ~12 dispatcher-family
methods), and the whole dead chain bottomed at command-object methods
that are never vtable-invoked; frz175 (CF14C, black era) core: main
parked in D3D task-wait (1DE5C0), wakes era-invariant, runtime-table
hunt REFUTED (no heap table copies anywhere). frz176 (regen+census
REGI/REGX/CMD1 running) decides where instantiation dies.

## Where things are verified

- Toolkit: `xboxrecomp/` @ v0.18.0 / `37b61d589168adee3680fabd51805ee81427bf07`;
  `scripts/doctor.sh` green (XBE SHA-256 pin, skill digests, stubs
  432 agree). Patch series **`0001..0053`**; the patch-by-patch table
  lives in THIRD_PARTY.md (its one home); `scripts/build.sh` resets
  to the pin and re-applies in order.
- Manual overrides (`src/recomp_manual.c`, `--exclude-manual`): census
  wrappers incl. PUMP/S9720/S99D0/EE110/EE190(ENQ)/MTICK/EED70/CQ/
  SMEVT/SRST/F0F80/POST/REFILL/EACK/EACYC/EBUSY/EDRV/ISSU1/ISSU2 +
  INIT/CTOR/UP/FAM tier + WINC/WDEC/CLOSE ledger + cache-writer/audio/
  swap + MCOR/ITER/EXEC/ISERV/EENT + SAPI/EENT + NAM(1EED20)/
  TISS(16F520) tier. NEW WRAPPER COST LAW: a new wrapped VA needs a
  full `pipeline.sh --seed` regen before it links (`_gen` rename;
  frz169 link-fail taught this).
- Raw-bytes decode law: `.text` file offset = VA − 0x11000 + 0x1000.
  Kernel thunk placeholders `[0x3156xx]=0xFE000000+i*4` real.
- Registry RTTI law: 3367xx "vtbls" are class-NAME strings
  ("Angola1/2","Skip1","Selecte...","elements") — objects carry
  class tags, not vtables, at +0 (frz154 generalized).
- Ghidra bench installed; capstone raw decode is the working bench.
- Git: `origin/main = 77f770f` per `git reflog`; posture question
  lives in Blockers — agent pushes nothing either way.
- JP second sample: `~/Downloads/xbox/sudeki-jap/default.xbe`
  (parked, outside git).

## Last run

Session 5d (13:0x–15:3x): **waiter ledger closed; frz163-171 overturn
arc; wall moved from "deadlock" to "Press START, input unanswered".**
- frz163-166: FNT1-lookups refuted; queue-band watch: 620-queue drained
  twice at boot then quiet; mode-2 push anatomy decoded (1EC540 dedupe,
  1EC2C0 double pending++, 1EC6B0 = head writer, 0x4000 push-bit).
- frz168: terminal-manager field timeline (WATCH): +0x14 record-array
  NEVER installed; +2C=1 at birth; state 0x10120→0x25120.
- frz169: NAM law (stack-descriptor ELSE-leg); loader drain DID run
  post-burst (ENQ x3, print-cap artifact overturned "boot-era only");
  bucket-entry lifecycle complete; 7 siblings EF1D0-serviced same window;
  container census: type-2 FNT1 41/2842 records resident.
- frz171 (TISS census + BUCK id-gate): 16F520 = task-issue tier, lists
  EMPTY at entry, one node installed by submit; PUMP printed >=334 ->
  **x167-terminal was the early-kill artifact**; post-x167 window:
  WDEC decrements the orphan, engine re-issues with desc installed.
- frz172 (no-early-kill, 1750 s, era BF14C): PUMP 167 true-terminal,
  FB dumps: black loading era -> **#172 = Press START fully rendered**
  (~vbl 20.7k); endgame ~850 s: vblank alive to 71.4k, GPU draws,
  evt6 ms=0 POLL-SPIN x749M at 1EACC0+2767 (the loader pump itself,
  livelock shape), PB census blk=8006 steady, 25 pad-STARTs ISR-claimed,
  NO menu response.
- Symbolization: base-diff law (SIGKILL runs have no core; map run-B
  host RAs into run-A space via a same-site census anchor).
- frz173 (1150 s, dense presses): ended PRE-pill; FB cadence law: dumps
  = 120 PRESENTED frames ~ 10 s wall (title era presents ~12 fps); the
  pill appears at dump ~172 = ~28 min wall — earlier "pill at 350 s" was
  a fps-assumption error.
- frz174 (1900 s): pill + © at #176/180/182, black fades #172/186/187 =
  ATTRACT LOOP; scene-tier census ALL x0 the whole window; SWEEP 4E1F0
  ZERO static refs (runtime-table dispatch law for the SOL tier family).
- Loader pump static decode complete (1EACC0 pump / 1EAD70 service loop
  sleeps 16 ms / 1EAD50 evt6 status-probe = the x749M census site; the
  loader thread itself healthy-parks W2MO x41923 with ret=1 wakes).
- Prior art: upstream README port table checked 2026-11-23 — NO Sudeki
  row (16 ports); title still unclaimed.
Full verdicts in note 0025 (sections through "frz172"); one-line each:
- frz162: EENT 167 submits; terminal x159 unique template pair.
- frz163: type-2 FMH-miss tolerated by healthy submits; two submitter tiers.
- frz164: SAPI state census (0x00x116/0x84x42/0x20x5/0x01x3); EF0C0 tail law.
- frz165/166: queue-head witnesses; queue-band ledger 16 changes all decoded.
- frz167 BUCK dump tier; frz168 EF0C0 a4 payload census (payload-0 law
  later REFUTED for terminal — a4=template, a5=tmpl2).

## Live hypotheses

1. **H31 (ACTIVE, note 0025, RE-POSED x30: post-frz172/174 the wall is a
   DEAD SCENE TIER, not a load deadlock — the game renders and attract-
   cycles the full Press START title for 30+ min while MSG/SWEEP/MKLU/
   SCH/CTOR census stay x0; pad START arrives (OHCI claimed) but the
   handler tier is behind the never-started object-graph, whose entry
   points (SWEEP 4E1F0 et al.) have ZERO static xrefs (runtime-table
   dispatch). Candidates: (a) the table that should list scene executors
   is never fed (who feeds it?); (b) a scene thread exists but its
   dispatch entry table slot is empty; (c) gate/global [0x3DC83C]-stub
   blocks instantiation. frz175 core (pill-era, SIGABRT) = thread stacks
   + globals + handler lists at the stuck moment).** Prior frz18-era
   Buki-card runs prove input end-to-end works when the tier lives.
2. H14–H30 CLOSED — one-line lessons index (homes = notes
   0001-0011/0016-0024): cache capacity ends the boot write-loop (0005);
   async-IO return convention = the phantom charge (0011); ICALL-skip
   stack drift (0018); AC97 trap arch gap (0019 era, patch 0022);
   input = arm OHCI + scripted-pad START bit (0020); sw-method latch/ack
   chain (0021); `ke_shadow_init` double-init (0022); load-state pump
   pause is the game's own design (0023); service-chain livelock (0023);
   D3D RAM events need shadow mirrors — `RECOMP_TITLE_KEVENTS=1` part of
   the recipe (0024).

## Queue (next concrete actions)

* **1. frz175 core forensics (core arrives at pill era):** (i) per-thread
  guest stack scan (26 host LWPs; find the scene-thread candidates parked
  or spinning — `info threads` + stack dwords -> base-diff symbolize);
  (ii) globals at stuck moment: gate [397110], scene [3DC83C], handler
  lists 3975E4/E8 (identity-chunk law), [37A0F5], heads 3D7620/68C/71C/740;
  (iii) search heap for runtime dispatch tables containing the 4E1F0
  family VAs (dword-scan 0x37A000-region + heap 01xxxxxx for values
  0x0004E1F0/0x00157B10/... = the table that should list scene executors).
* **2. Loader-pump livelock anatomy**: 1EACC0+2767 ms=0 evt6 wait — why
  never signaled post-burst; compare vs healthy boot era (evt6 Set sites).
* **3. Menu artefact completion (rule 6):** xemu shotseq unique-name pair
  still owed; FB law OPEN; `xemu_qmp_probe.sh` unrun. Now the GUEST side
  has a crisp same-moment (Press START, frz172 #172) — the xemu pair is
  this exact screen.
* **4. Write-path decision**: capacity enforcement (patch 0005) sits on
  `bridge_NtWriteFile` the guest never uses — move or retire.
* **5. Then the Gameplay gate:** first presented level frame.
* Carry-over: JP disc regression sample later; roadmap staging (vanilla
  -> Vulkan -> features) per user directive; post-playable feature order
  (2026-11-22): HDR -> Raytracing -> Multiplayer; prior art
  `github.com/sudeki-together/sudeki-mp` (reference value only, AGPL).

## Blockers / open questions

- **H31 wall write-up (AGENTS work-loop rule):** 70+ runs
  (frz93->173), 28 re-poses, 7 header-level facts overturned mid-wall
  (regime split, trailer-bit4, events-never-Set, START-spam confounder,
  empty-handler-lists core misread, payload-0 skip, **PUMP-x167-terminal
  = harness artifact**). Era lottery law: per-boot regime variance is
  REAL (frz171 CF-era cycles vs frz172 BF-era true-167) — judge runs by
  era fingerprint, never by single-run markers.
- User question (rule 7): push posture — see "Where things are
  verified" Git bullet.

## Housekeeping facts (recipe home)

- Working recipe (frz92+ regime, nav): `RECOMP_VBLANK=1 RECOMP_PB_EXEC=1
  RECOMP_PB_WALK=dma RECOMP_KERNEL_LOG_BUDGET=0 RECOMP_AC97_READY=1
  RECOMP_USB=1 RECOMP_ARM_UNTIL_SEEN=0 RECOMP_TITLE_KEVENTS=1` + FB
  dumps + census (`RECOMP_IO_READ_LOG=1 RECOMP_IO_WRITE_LOG=1
  RECOMP_IO_WRITE_MAX=0 RECOMP_GATE_TRACE=1 RECOMP_PRES_TRACE=1
  RECOMP_EVENT_LOG=1 RECOMP_GSVC_TRACE=1 RECOMP_RD_TRACE=1
  RECOMP_MISSING_FILES=all RECOMP_POST_TRACE=1 RECOMP_LOG_ORDS=123,138,
  145,205,225`).
- Harness current: `/tmp/opencode/nav_run51.sh` (**no early-kill — use
  this for endgame runs**; `WATCHVA/WATCHLEN/WATCHPOLL` env params;
  BIN pattern `/tmp/opencode/sudeki-frz$N.bin` — cp per N) +
  `nav_run52.sh` (dense 5 s STARTs) + `nav_run53.sh` (timeout->SIGABRT
  = CORE at window end; use when the core IS the artefact) +
  `nav_run49.sh` (legacy kill) + `tools/hangwatch.sh` v2; core reads via
  `tools/corepeek.py` + anchor pair `0037306C,00373074` @`0x3975E4` +
  gate `[0x397110]==1`.
  `/tmp/opencode` is tmpfs: everything there vanishes on host reboot.
- Cores kept (tmpfs ~5.8 GB): frz171 (frz171/172 host-base pair);
  prune at next pause.
- Other useful env: `RECOMP_CACHE_CAP_BYTES`, `RECOMP_WATCH=<va>…`,
  `RECOMP_SEM_TRACE`, `RECOMP_LOG_ORDS=…` (patch homes: THIRD_PARTY
  table).
- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env,
  skills_check}.sh`; logs -> `logs/`, captures -> `reference/`
  (gitignored); cache dir `~/.local/share/xboxrecomp/Cache/*` between
  long runs. FB dumps are uncompressed BMPs (size = no signal) —
  `magick X.bmp X.png` then view; census-timeline anchors: vblank-600
  tick lines.
- Shell law: never edit a script a live bash re-reads; never edit
  `src/recomp_manual.c` while a gen+build chain is in flight; new
  wrapped VA => pipeline regen before link; never rebuild `build/sudeki`
  while a run is in flight; watcher stdout block-buffered; kill by exact
  pid or `pgrep -x sudeki` (NEVER `pgrep -f`); pipeline `--seed` needs
  ABSOLUTE path (py helper cds into toolkit dir).
- Seeds file (local, 19 measured VAs): `logs/seeds-icall-0011C640.json`.
- Era fingerprints: frz168-171 = CF1xx heap era; frz172 = BF1xx era
  (heap drifts with gen/manual edits — re-anchor always).
- Git: never push (rule 7); local commits through `22c4964`+; every
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
| 11-22 22:0x–11-23 02:5x s5a | frz107-127 census arc; H31 re-posed x12 to issuer level; patches 0048-0051; raw-decode law; regime split refuted; /review audit #2; frz128 probes committed | `230ede7`..`b0204cd` |
| 11-23 03:0x–06:3x s5b | frz128→134 census arc: evt7 polarity closed, loader healthy-parked, submit chain runs, state-3 refuted, 522 CQ pushes all-completions, START-spam exonerated -> H31 re-posed x14: engine-side stop-of-issue; xemu FB capture pipeline live | `99d6370`..`729777e` |
| 11-23 06:3x–13:0x s5c | frz135→152 census arc + patch 0052; hangwatch v2; object-tier census ALL x0 then CORE-READ CORRECTION; waiter ledger decoded; H31 re-posed x24: one never-closed waiter; audit #3 applied | `546c64e`..`f497810` |
| 11-23 13:0x–15:3x s5d | frz161-173: queue-push full decode; NAM/TISS/BUCK probe tier; **frz171/172 OVERTURN: PUMP-x167-terminal = harness artifact; game reaches Press START** (FB #172); loader pump evt6 livelock identified; base-diff symbolization law; new-wrapper-regen law; upstream prior-art re-checked (unclaimed); pose x29 | `d39a8dc`..`22c4964`+ |
