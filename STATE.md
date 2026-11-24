# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-23 22:1x (session 5g, ~21:1x–…)
**Stage:** LIFT complete, ENTRY strong, **MENU artefact reached**
(xemu same-moment capture still owed, see Queue);
**navigation works; the game reaches a fully rendered "Press START"
title and CYCLES it as an attract loop (frz172/174 FB dumps)**;
load-stall wall (H31, re-posed x39, home = note 0025) in ONE sentence:
**pose x39 = BRIEF risk #2 at last: the healthy heartbeat tier
(11F0C0->1A9A50 drain -> scene-mgr 27430 -> batchA AE630 -> activation
thunk -> AFBF0 -> OpenResource) runs on the game BOOT THREAD whose
chain 26C68F/26D3E4 -> 272470 -> 2722EC -> engine block (12090..122BA)
has ZERO static xrefs in the XBE; our port SPAWNS the thread identical
(PsCreateSystemThreadEx routine=26D3E4 ctx=26C68F, run frz178 log) but
those routine owners were NEVER LIFTED (nm: absent) so the thread body
runs nothing. FIX IN FLIGHT: frz179 regen +4 measured seeds
(logs/seeds-frz179-merged.json, 33 seeds; regen detached ~22:1x,
check 22:57).**

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
- Git: history SCRUBBED session 5f (user directive): 14 commits
  `f55e0cd..d075914` rewritten to drop the machine-user absolute path
  (harness `ROOT=` lines); old backup refs/reflogs/objects purged;
  verified zero identifier hits across all refs + fsck. Local
  `origin/main` mirror deleted (remote itself STILL HAS the old
  commits — user force-push needed, see Blockers). `core.hooksPath`
  = `.githooks` installed (pre-commit leak guard; doctor checks it).
  Rule 13 (AGENTS): personal identifiers never enter the repo.
- JP second sample: `~/Downloads/xbox/sudeki-jap/default.xbe`
  (parked, outside git).

## Last run

Session 5g (21:1x–22:1x): **H31 collapsed to the invisible boot-thread
lift gap (pose x39); frz179 regen in flight.**
- frz178 core: batchA obj PRESENT (02853AE0 vptr 325AB8), gate +0x2C=0
  clean-closed; ui_font instance 0280BA10; resys 40/66 (era fingerprint
  018Axxxx lineage).
- xemu chain resolved rung-by-rung: 27430 (container dispatcher, ra
  1A9A03 = heartbeat 1A99A0) <- drain 1A9A50 <- 11F0C0 <- engine block
  (12090..122BA, entered once at boot, loops eternal) <- XAPILIB tier
  (2722EC/272470) <- boot thread (worker 26D3E4, ctx 26C68F).
- ours: thread SPAWNED identical (frz178 kernel log #1/#2) but owners
  26C68F/26D3E4/2722EC/272470 absent from binary -> thread body inert.
- Laws banked: hbreak unsupported at xemu stub; -S passthrough works
  (boot tier runs <1 s post-reset, entry bps miss legit); dword-scan
  ModRM false positives (0x12090 x7); runtime .text == static (no drift);
  ours_trace symbol format sub_00XXXXXX (6-digit VAs, 0x1A9A50 not
  0x01A9A50) — s5f "x0" verdicts re-verified TRUE under corrected fmt.
- frz179: seeds merged 33 (4 new measured), pipeline+build detached,
  automation checkpoint 22:57.

## Live hypotheses

1. **H31 (ACTIVE, note 0025, RE-POSED x39: BRIEF risk #2 confirmed —
   the engine heartbeat chain runs on the game BOOT THREAD (worker
   26D3E4, ctx 26C68F, then 272470 -> 2722EC -> engine block
   12090..122BA -> 11F0C0/1A9A50 drain -> 27430 scene-mgr -> AE630
   batchA -> activation thunk -> AFBF0 -> OpenResource). Our port
   spawns the SAME thread (identical routine/ctx in kernel log) but
   the chain owners were never lifted (zero static xrefs in XBE =
   invisible thread-start args), so the thread body is inert and every
   downstream rung is x0. frz179 regen (+4 measured seeds) tests the
   fix; if heartbeat wakes, expect asset activation + load progress.)**
   Prior frz18-era Buki-card runs prove input end-to-end works when the
   tier lives.
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

* **1. frz179 boot-thread fix (regen +4 seeds, detached, checkpoint
  22:57):** verify new symbols (26C68F/26D3E4/2722EC/272470), 15-min
  nav window, host-gdb liveness census (12090/11F0C0/1A9A50/27430/
  AE630). Alive -> re-check AFBF0/OpenResource tier + Press-START/
  load state + FB dumps; dead -> trace ours-side worker 26D3E4 flow
  vs healthy (block-entry ABI may need manual wiring).**
* **2. paging:true xemu RAM dump (ram-paged.elf, in flight):** redo the
  healthy-vs-ours globals diff with VA-correct mapping (the earlier
  "inverted regime" reading is REFUTED — string pool, see note 0025).
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
- **USER ACTION REQUIRED (privacy):** the pre-scrub commits (old
  `f55e0cd..027d79a`) still exist on GitHub (`origin/main` was
  `f55e0cd`). From this machine, when you choose: `git fetch --prune`
  is not needed — just `git push --force-with-lease origin main`
  (or `--force` since the tracking ref was removed) to replace remote
  history with the scrubbed one. Then GitHub GC eventually drops the
  old objects; for immediate removal ask GitHub support ("remove
  cached objects for commit SHAs"). Rule 7: the agent never pushes.
  Open question: personal Gmail in some commit AUTHOR metadata — tell
  me if that should be rewritten too (touches all 427 commits).

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
- **DETACHED-RUN LAW (session 5f)**: background shells die at the 600 s
  harness cap AND cascade SIGTERM to the game. Runs with windows
  >10 min MUST be `setsid nohup ... & disown` from a short shell;
  completion discovered via automation or wrap-log check, never polling.
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
- **XREF SCAN LAW (session 5e)**: `.text` VA = file offset + 0x10000
  (NOT +0x11000). Every pre-5e "ZERO static xrefs" conclusion was an
  off-by-0x1000 scan bug and is re-verified false; corrected scanner =
  the blob-relative loops in note 0025 ("xref-scan-bug overturn" section).
- **XEMU REFERENCE TOOLING (session 5e)**: xemu 0.8.139 = QEMU 10.2
  wrapper; extra QEMU args pass through (`XEMU_EXTRA` in
  `scripts/xemu_boot_probe.sh`). **Guest trace**: `-gdb tcp::12345` +
  host `gdb -batch -x /tmp/opencode/gdbtrace.cmds` with
  `set architecture i386`, soft `break *0xVA`, commands-block auto-log
  (regs + 14 esp dwords + continue). **RAM dump law: use
  `dump-guest-memory paging:true`** — paging:false = PHYSICAL segments
  (per-page scatter; identity-pair delta valid only within one page-run;
  sanity-check every read set). Cores kept: frz171 (CF logo era),
  frz175 (CF black era), frz176 (BF pill era); tmpfs 63G, no prune need.
- **GDB TWO-MODE LAW (session 5f)**: xemu round = `target remote
  127.0.0.1:<port>` (xemu launched with `-gdb tcp::<port>`, port must
  match cmds file) + `set architecture i386`, breaks at GUEST VAs.
  Ours round = `gdb -batch -x cmds -ex run --args build/sudeki`, breaks
  at HOST symbols `sub_XXXXXXXX` (lifted guest fns are host symbols —
  reachability questions need NO census regen). Ours round MUST set
  `handle SIGSEGV nostop noprint pass` (+ SIGINT/SIGTERM) — the runtime
  absorbs an XPP-section fault at sub_00310375 and gdb otherwise fakes a
  crash. Hot-site bps: sample + auto-`delete N` after N hits.
- **GDB FRAGILITY LAW (session 5f, post-frz178-call)**: batch watch
  chains (hw watchpoints armed in commands blocks, shared bp/watch
  numbering, mid-run script edits) burned 5 rounds; for
  state-at-window-end questions prefer **plain run -> ABRT core ->
  corescan** (tools/harness/run_window.sh). Host symbol entries can be
  mid-block (sub_00311B48 never hits despite callers' ret-addrs inside
  it) — bp only functions proven to print census tags.
- **Harness self-snapshot law**: tools/harness scripts re-exec from a
  frozen /tmp copy (mid-run edits corrupted live bash twice).
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
| 11-23 15:5x–17:5x s5e | **xref-scan-bug overturn** (VA=off+0x10000; false zero-xref laws); frz175 core verdict (loader anatomy CLOSED, no livelock); 478-table ctors run + instances exist; **xemu RAM-diff REFUTED** (paging:false = physical/string-pool); input tier ALIVE (pad CTOR); **FRM x0 era-invariant**; frz176 core; **xemu gdb-stub trace tool online** (pose x32) | (sess 5e) |
| 11-23 19:1x–21:0x s5f | lean xemu ticktraces: **healthy pump anatomy** (F8240 vtable-slot caller 1A9A03, pump class vptr 3237F0, ctor 58810, core composite 58E30, resource-open roots 5EC70/5EDB0, heartbeat 1A9A50->1A99A0); ours host-gdb probe: **whole chain x0**; **REVERSAL: resource system + 40/66 handlers + asset-batch instances ALL PRESENT in ours** (frz176 core; healthy identical 40/66) — missing = invocation of asset methods (AFBF0/Background_CharactersSG, B08F0/ui_font); XPP SIGSEGV gdb law; durable tools/harness + chainup.py; **history scrub (username path, 14 commits) + rule 13 leak guards** (pose x36) | (sess 5f) |
| 11-23 21:1x–22:1x s5g | frz178 core: batch obj present + gate clean-closed; activation domino chain AE630<-27430(container)<-heartbeat 1A9A03; **BOOT-THREAD DECODE: engine block <- 2722EC/272470 <- worker 26D3E4 (spawned identical in ours, but owners NEVER LIFTED — invisible thread-start, BRIEF risk #2 confirmed)**; ours_trace x0 verdicts re-verified under corrected symbol fmt; frz179 regen +4 measured seeds (33) detached | (this session) |
