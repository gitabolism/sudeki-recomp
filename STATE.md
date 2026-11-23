# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-23 13:3x (session 5d, ~13:0x–…)
**Stage:** LIFT complete, ENTRY strong, **MENU artefact reached**
(xemu same-moment capture still owed, see Queue);
**navigation works**; load-stall wall (H31, re-posed x26, home = note
0025) in ONE sentence: the terminal burst's engine-task submit
(`16F520`→`1EE7D0`→`EE2A0`, id `0A260002`, state-arg `0x20` + template
arg) creates a fresh manager whose waiter opens while `[mgr+8]` desc is
still 0, and because state `0x20` routes the node through the EF0C0
**mode-2 no-owner leg**, the loader drains it EMPTY (`EE110 arr=0
cnt=0`), nothing completes it, the pending word stays 1, the loader
parks INFINITE, and the engine PUMP rescans to the deterministic x167
terminal while the game sits on a static "Loading..." screen (healthy
fresh managers take the **mode-0** leg and service fine).

## Where things are verified

- Toolkit: `xboxrecomp/` @ v0.18.0 / `37b61d589168adee3680fabd51805ee81427bf07`;
  `scripts/doctor.sh` green (XBE SHA-256 pin, skill digests, stubs
  432 agree). Patch series **`0001..0053`**; the patch-by-patch table
  lives in THIRD_PARTY.md (its one home); `scripts/build.sh` resets
  to the pin and re-applies in order.
- Manual overrides (`src/recomp_manual.c`, `--exclude-manual`): census
  wrappers incl. PUMP/S9720/S99D0/EE110/EE190(ENQ)/MTICK/EED70/CQ/
  SMEVT/SRST/F0F80/POST/REFILL/EACK/EACYC/EBUSY/EDRV/ISSU1/ISSU2 +
  INIT/CTOR/UP/FAM census tier + WINC/WDEC/CLOSE waiter ledger +
  cache-writer/audio/swap wrappers + MCOR/ITER/EXEC/ISERV/EENT tier.
  All source changes are complete units, committed;
  build era = frz159 (EENT census; frz160 control staged).
- Raw-bytes decode law (verified against lift): `.text` file offset =
  VA − 0x11000 + 0x1000 (capstone direct on the XBE; the earlier
  0x1F0000-offset decode was garbage — deleted from note 0025).
  Kernel thunk placeholders `[0x3156xx]=0xFE000000+i*4` are the real
  bridge VA encoding (slot = index into the XBE import table).
- Ghidra bench installed; flat-import recipe in THIRD_PARTY.md
  (capstone raw decode has replaced it for now).
- Git: `origin/main = 77f770f` per `git reflog` (four pushes
  postdating the recorded user-approved push `7ce51cb`); posture
  question lives in Blockers — agent pushes nothing either way.
- JP second sample: `~/Downloads/xbox/sudeki-jap/default.xbe`
  (parked, outside git; naming `<game>-<region>/`).

## Last run

Session 5d (13:0x–…): **waiter ledger closed wall anatomy; poll-diff overturn (ctor-at-burst, stack-garbage name, B29=class tag); EACYC=EF1D0 handshake x73 exits 102; frz157 = THE ORDERING (pose x25); frz161 census: completion tier dies PRE-BURST.**
- frz160 healthy-manager control VOID (boot-era lottery death; loader
  line "XBE loaded: 3903488" law = file size, no input drift).
- frz161: KICK x~300 ALL = lookup-release leg in submit-hit path
  (ra=1EE6D0+1642), ZERO for terminal, dies 59876; SVC x12800 ALL from
  EB0C0 tick (node geometry getters), dies 59198 = BEFORE WINC x207
  (59868); SVC2 x0; EENT <200 calls whole-run.
- Static: manager [0x14] = COPIED record-array ptr (template copy at
  1EE451); builder 1EDE80 UNLIFTED + no measured ICALL ever; EF1D0
  unlink law explains healthy arr=0 at kill; CLOSE = EC350 or 1EEEE0
  at-zero (1F92E0+1ED9B0+vtbl[0](1)); type-slot [0x37A3D0][2] =
  "FNT1"-looking tag deterministic in 4 cores (type-2 resolution
  never happens -> SVC2 x0 consistent).
- frz162 running (EENT post-state per submit + node-stamped SVC;
  syntax-gate catch + brace fix committed).
Full verdicts in note 0025 (sections frz152b / frz153 / frz154-155 /
frz157 / frz159 / static addenda / frz161); one-line each:
- frz152b: ledger reproduces net +1; unmatched open = EE2A0 relay
  `1EE682`; loader drains once then parks INFINITE, never woken.
- frz153/153b: mprotect watch-trap UNRELIABLE -> patch **0053**
  `RECOMP_WATCH_POLL` poll-diff (LEN 0x40 over whole manager).
- frz154/155: terminal manager block CONSTRUCTED at the burst; name
  field = stack garbage; `06610B29` = FMH-class desc TAG not key;
  EACYC = per-node handshake x73 all `0x102`; wake-chain tier decoded.
- **frz157 = THE ORDERING:** WINC x207 on uninit slot -> no-owner mode2
  -> sibling batch ctors -> loader drain -> **EE110 processed record
  EMPTY (`arr=[mgr+0x14]=0 cnt=0`), never completed**; inline init too
  late; ISERV never for terminal epoch.
- frz159: `EE2A0`'s SOLE caller = `1EE7D0` submit API, one call per job
  id; WINC-leg + batch-ctor leg inside the same call window.

Session 5c (06:3x–13:0x): **frz135→144 census arc; wall narrowed to the
message/object-graph tier; submit-API law found; xemu capture law work.**
- frz142/143 OVERTURN arc (cap law): dispatch/submit/loader tiers ALL
  alive whole-run (SUBM >=257, EACK from loader 1EACC0, resubmit
  bursts of identical jobs, WALK endgame cluster); message-leaf tier
  (MSG1/MSG2/SWEEP/MKLU/SCH1-3/scene-executor) NEVER ticks ->
  object-graph-never-instantiated pose (CTOR probes frz144).
- Gate byte `[0x397110]` = one boot write 0->1, never cleared (law
  closed); scene global written once at boot (`0281BEC0`, exists).
- frz135-137: SUB2A(1FD660) dead-code x0; WALK x31 = complete task
  list; corrected EF0C0 mode census (mode2 x279 — probe-arg law);
  terminal manager = ONE never-closed request (pending word stuck 1).
- frz138: pending-count ledger — whole streaming era balanced,
  terminal burst nets +1; unclosed open = `EE2A0` relay (`1EC148`).
- frz140: dispatch tier dies EXACTLY with session 31; loader drain
  era outlives it; batch-dword race refuted (plain single-open leak);
  queue heads ALL ZERO at kill (no stuck task); `0x115000`
  never-lifted law found; SWEEP x0 = command 55 never posts; scene
  global `[0x3DC83C]` = stub at kill.
- frz141: `0x115000` seeded -> 0 resolve-fails (law closed, wall
  unchanged); FRM x0 (vtable fanout not probe-reachable) -> census
  by method wrappers; **heap-VA drift law** (manager `018BF14C ->
  018CF14C` across seed-era change): watch data globals across eras.
- frz141 FB dumps: at kill the game SHOWS an ANIMATED SUDEKI TITLE —
  frame tier alive; wall = START never advances (no next task batch).
- Static: `1EE7D0` = THE job-submit API (single master gate
  `MEM8(0x397110)`, all direct-called from the 2C-2D thunk region);
  `1EE400` is NOT a function (mid-body label of EE2A0 — JPSS retired);
  ISSMTH ancestry runs solely through `6AD00->74F10->7D1E0`.
- Harness: gen law (recomp takes no data-file args; seeds =
  `pipeline.sh --seed` at disasm); xemu_shotseq unique dump names
  (collision law); `xemu_fbpeek --zorder` (0x1A3B000 = block-swizzled
  TEXTURE, FB law still open); `xemu_qmp_probe.sh` written (screenshot
  question); shell-chain timeout law (keep timeouts > run ETAs).
- frz145-146 regime arc: vblank-TICK death in 145-era = boot-era
  LOTTERY (3rd matrix-law confirmation; patch 0052 VBLDIAG);
  hangwatch v1 (ord-161 spin threshold) CONVICTED as confounder —
  healthy eras spin 160/161 at >100k/s; hangwatch v2 = vblank-tick
  lag law. frz146c/frz146b/frz148: healthy full eras.
- frz146c/148 census (healthy, PUMP x167 terminal): CTOR/CTOR2/CTRX/
  REGT/RQ/HND/MSG/MKLU/TBLU/SWEEP ALL x0; SUBM/ATT2/ATT3/ATT4 caps
  saturated -> object-graph pose CONFIRMED. frz148 core RA census:
  ATT2/ATT3 called inside retry drivers 1F5A70/1F5AE0, ATT4 inside
  20ABF0; gdb `info symbol` on bin+core = direct symbolization law.
- Visual wall (frz146c FB dumps): after START, a static black
  "Loading..." screen to kill — the endless-load state IS the wall.
- Census tag law: tags unique, blocks count==1, gcc-syntax-check
  before commit (a double-committed probe block hid in HEAD for 5
  builds; a REG/[REG] collision nearly reversed the registrar verdict).
- frz149-152 arc (this close-out): dead-chain census ALL x0
  (UP1-4/FAM2-12/REGT/class ctors) but **CORE-READ CORRECTION**:
  handler lists 3975E4/E8 populated from boot (identity-chunk core
  addressing law; earlier "empty" read was a 0x10000 math bug);
  CRT static-init tier (initterm 26CFF8, 271-thunk table 33D410,
  insert thunks 2815Exx/2819Bxx) + waiter ledger decoded
  (EC130 inc / EC350 dec / zero-count CLOSE 1F92E0+1ED9B0);
  terminal desc F95738 key 06610B29 size 0x373F (frz150 core);
  INIT tier all ticked (frz151); PUMP x167 = full manager-array
  scan, terminal = only st=0x25120 slot. fresh-reader audit #3
  applied (note 0025 dedupe, CHANGELOG backfill ENTRY/THREADS/MENU,
  THIRD_PARTY hygiene, README Status sync).
Session 5b (03:0x–06:3x): **frz128→134 census arc; H31 stands with
clean input; harness START-spam confounder convicted then EXONERATED
as cause; xemu FB capture pipeline live.**
- frz129/130: evt7 = notification event created SIGNALED -> wake/park
  short-circuit is guest-coherent (frz126 "contradiction" was polarity
  misread); W2MO fixed: loader parks INFINITE on {evt6,evt1}, wakes
  17x on evt1 (REQ) — loader healthy. ISSDRV(1EF770) x2 whole-run;
  session-gate law: EF770 issues iff F8670(0)==0; F8670 x32 all
  key-miss (11 healthy sessions exist, table allocated).
- frz131 (corrected base recipe `runtime .text − 0x10ee00`): ISSMTH
  (157B10) x17 ALL drain (arg5 bit0=1), EFSUB (EF7C0) x16-17,
  SWEEP (4E1F0) **x0**; REQ ra=21AD0+0x4cf (loader consumer);
  issuer chain static-complete: task-table (LE ref file 0x3229E8) ->
  6AD00 -> 74F10 -> 7D1E0 -> 157B10 -> {EF770 new | EF7C0 drain}.
  Cache cursor 25,137,248 = FMH sparse padding AFTER an index record
  (key dword 0x06610B21 @0x17F905C, payload resumes 0x17F915C) —
  install data fine, FMH container law noted.
- frz132: `[SESSN]` sessions peak 31 (26x1,3x2,1x3) — state-3 exists,
  NOT the discriminator. Loader pump law decoded raw (1EAC50..):
  servicing gated SOLELY on `[0x3D7620]!=0`; else loader parks.
- frz133: CQ uncapped = 522 pushes total (3D76B0x226/68Cx95/5FCx95/
  6D4x46/764x31/6F8x29); ZERO pushes ever target 3D7620/668;
  EF0C0 x398 ALL mode=0; terminal burst (56803+) completes the
  TERMINAL manager `018BF14C` itself -> machinery finishes cleanly,
  engine never issues the NEXT job.
- frz134 (A/B, frz18-exact presses, zero tail presses, full 1150 s):
  PUMP x167 terminal at line 55017 UNCHANGED; buki era reached
  (line 9806). Wall NOT input-induced.
- Patches unchanged (0001..0051). Tools: `tools/xemu_fbpeek.py`
  (auto-scan + `--top` uniformity scan), `tools/xemu_rampeek.py`,
  `tools/hddpeek.py` (qcow2+FATX, unproven), `scripts/xemu_titlecap.sh`
  (single-shot; title90/title-t60 both TORN-FMV garbage — the
  "v1.1.0 law 0x1A3B000 640x576 score .996" is NOT a valid FB read,
  alpha-score fooled by video surfaces), `scripts/xemu_shotseq.sh`
  (multi-shot; t=60 dump OK, t>=90 QMP TIMEOUT — fixed with
  id-matched line-delimited QMP reads; re-run owed). xemu VRAM at
  t=60 = textured FMV surfaces (uniformity max 0.49); title-screen
  capture needs a LATER timestamp. `reference/xemu/` still has no
  valid title artefact (only the STOP screen).
- xemu RAM dump dead-end law: game code in RAM is paged + XBE-region
  encrypted (pattern scans fail); globals NOT VA-locatable in RAM
  dumps; HDD qcow2 header parse pending (tools/hddpeek.py).

Session 5a summary: frz107-127 census arc; H31 re-posed x12 to issuer
level; patches 0048-0051; raw-decode law; /review audit #2.

Earlier sessions live in the notes (0001–0011, 0016–0025) + session
log.

## Live hypotheses

1. **H31 (ACTIVE, note 0025, RE-POSED x26: the terminal submit (engine
   task tier `16F520`, id `0A260002`, state-arg `0x20`, template arg
   `018C5D88`) is the only burst call routed through the EF0C0 mode-2
   no-owner leg (state bit `0x20`); its fresh manager's waiter drains
   EMPTY and is never completed (pending word stays 1; CLOSE never
   runs); the seven sibling submits (arg3=0, mode-0) create+service
   fresh managers fine in the same window).** Type-slot corruption
   REFUTED (frz163 watch: slot[2] valid+stable; my earlier "FNT1"
   reading was a core-index error); resolution healthy (`job st=0`).
   frz164: SAPI full id-schedule + template-arg census.
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

* **1. frz164 (running): SAPI full submit schedule + template census.**
  Then: name what the engine expects to fill `[mgr+8]` for a mode-2
  fresh manager — the mode-0 sibling leg proves fresh+WINC is benign;
  the mode-2 no-owner completion is the missing writer. Candidate
  probes: EF0C0 branch-side census (which leg per call) + the loader
  1EAC50 pump condition on 3D7620.
* **2. Menu artefact completion (rule 6):** xemu shotseq unique-name
  pair still owed; FB law OPEN (linear + Z-order both decode TEXTURE
  at 0x1A3B000); `xemu_qmp_probe.sh` unrun decides screenshot path.
* **3. Write-path decision**: capacity enforcement (patch 0005)
  sits on `bridge_NtWriteFile` which the guest never uses (patch
  0051 log proves raw thunk route). Move it into `kernel_file.c` or
  retire it — affects disk-full experiments only.
* **4. Then the Gameplay gate:** first presented level frame =
  fbpeek + visual + texture-coverage carry-over; xemu side-by-side
  of the SAME load as corroborator only.
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

- **H31 wall write-up (AGENTS work-loop rule):** 60+ runs
  (frz93→159, frz160 staged), 25 re-poses, 5 header-level facts
  overturned mid-wall (regime split, trailer-bit4, events-never-Set,
  START-spam confounder, empty-handler-lists core misread) plus the
  frz154/155 poll-diff overturn (ctor-at-burst, stack-garbage name,
  B29=tag-not-key). Host side EXONERATED end-to-end (frz128–134);
  wall now = submit-before-init ordering at the engine tier
  (frz157/frz159). Next comparison if the frz160 control stalls:
  xemu kernel trace of the same load.
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
- Harness current: `/tmp/opencode/nav_run48.sh` (`WATCHVA` env param,
  `RECOMP_WATCH_LEN=0x40 RECOMP_WATCH_POLL=150`; pad gate: OHCI raised
  00000002|4 OR vblank-600 fallback; early SIGABRT on `[PUMP] x167 `;
  BIN pattern `/tmp/opencode/sudeki-frz$N.bin` — cp per N) +
  `tools/hangwatch.sh` v2; core reads via `tools/corepeek.py` + the
  identity-chunk law (note 0025).
  `/tmp/opencode` is tmpfs: everything there vanishes on host reboot.
- Other useful env: `RECOMP_CACHE_CAP_BYTES`, `RECOMP_WATCH=<va>…`,
  `RECOMP_SEM_TRACE`, `RECOMP_LOG_ORDS=…` (patch homes: THIRD_PARTY
  table).
- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env,
  skills_check}.sh`; logs -> `logs/`, captures -> `reference/`
  (gitignored); cores via `coredumpctl dump <pid>` (~2.4 GB each —
  dumped set: frz138/144/145b/146b/148/150/152b/153/155/159 in
  /tmp/opencode ~7 GB tmpfs; prune at next pause); cache dir
  `~/.local/share/xboxrecomp/Cache/*` between long runs.
- Shell law: never edit a script a live bash re-reads; never edit
  `src/recomp_manual.c` while a gen+build chain is in flight (gen
  reads OLD manual, make compiles NEW -> `undefined reference to
  sub_X_gen` — explains all three "transient" wrap misses); never
  rebuild `build/sudeki` while a run/core is in flight (copy the
  binary to `/tmp/opencode/sudeki-frzNN.bin`); watcher stdout
  block-buffered; kill by exact pid or `pgrep -x sudeki` (NEVER
  `pgrep -f`).
- Seeds file (local, 19 measured VAs): `logs/seeds-icall-0011C640.json`.
- Deterministic markers: PUMP x167 terminal, EE110=167, baf-read
  counts vary by kill timing (don't pin on them); regime pinned in
  run header from now on.
- Git: never push (rule 7); local commits through `f497810` (+ this
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
| 11-23 03:0x–06:3x s5b | frz128→134 census arc: evt7 polarity closed, loader healthy-parked, submit chain runs (ISSMTH x17 drain/SWEEP x0), state-3 refuted, 522 CQ pushes all-completions, START-spam confounder convicted then exonerated (frz134 clean-input reproduces wall) -> **H31 re-posed x14: engine-side stop-of-issue**; xemu FB capture pipeline live (`reference/xemu/title90.png` — later adjudicated torn-FMV, see s5c Queue) | `99d6370`..`729777e` |
| 11-23 06:3x–13:0x s5c | frz135→152 census arc + patch 0052 (vblank-diag; frz145 death = regime lottery); hangwatch v1 convicted→v2; object-tier census ALL x0 (frz146c/148/150/151) then **CORE-READ CORRECTION**: handler lists populated, ATT tier = live dispatch, ATT2 allocs job objects; CRT static-init + waiter-ledger (EC130/EC350/CLOSE) decoded; terminal desc F95738/key 06610B29 named; INIT tier alive; **H31 re-posed x24: one never-closed waiter**; audit #3 applied (CHANGELOG backfill, note dedupe, README/THIRD_PARTY sync) | `546c64e`..`f497810` |
