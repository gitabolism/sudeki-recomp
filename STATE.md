# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-23 06:3x (session 5b, 03:0x–06:3x)
**Stage:** LIFT complete, ENTRY strong, **MENU artefact reached**
(xemu same-moment capture still owed, see Queue);
**navigation works**; load-stall wall (H31, re-posed x14, home = note
0025) in ONE sentence: the streaming machinery COMPLETES its whole
job set cleanly and goes quiet (frz133: 522 CQ pushes all mode-0
completions incl. the terminal manager itself; zero pump-chain
pushes ever), while the engine-side issue chain
(task-table->6AD00->74F10->7D1E0->157B10->EF770 new-session) fires
only x2 at boot — and this is NOT input self-sabotage (frz134 A/B:
frz18-exact presses, zero tail STARTs, terminal shape identical).
frz128→134 census arc complete; nav_run36a.sh = clean-press harness.

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

Session 5c (06:3x–…): **frz135→142 census arc; wall narrowed to the
task-ISSUER tier; submit-API law found; xemu capture law work.**
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

1. **H31 (ACTIVE, note 0025, RE-POSED x21: endgame = ENGINE RESUBMIT
   LOOP; the vtable message tier is the invisible edge).** frz142
   overturned the "issuer silent" pose: SUBM hit its print cap (x199,
   cap law: count==LIM = saturation), job-record tier ticks to the
   terminal burst itself; terminal era CYCLICALLY re-creates the
   same jobs (id/ptr pairs `0AC20002->02967C30`, `0A7F0002->02967A8C`
   x3), EACK between bursts FROM the loader pump region (1EACC0),
   EBUSY x2560 whole-run. Dispatch tier never died (WALK "x30" was
   the cap) — frz140's tier-death laws retired. Scene global
   `[0x3DC83C]` written ONCE at boot to `0281BEC0` — scene object
   EXISTS; SWEEP x0 = cmd 55 never arrives OR leaf tier is
   dispatch-invisible (MKLU/FRM/TBLU/SWEEP all x0 vs ticking
   direct-call tiers). frz143 run (in flight) = tail-sampled censuses
   + MSG1(F1CA0)/MSG2(F1D50) leaf probes + `[0x397110]` gate watch.
   Heap-recycle law: log-quoted heap VAs are recycled by kill time —
   core forensics must be live-watched or per-era re-derived.
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

* **1. frz142 RUNNING (scene-global watch + MKLU/JPSS), then
  frz143:** rebuild carries TBLU(6AD00)/TCTO(7D1E0) — decides
  whether the task-lookup tier is reached at all post-wall. Then
  xemu QMP probe window (`scripts/xemu_qmp_probe.sh`) for the
  screenshot question. Gen law: recomp does NOT take functions
  paths (`--functions` is disasm's `--seed-functions` via
  `pipeline.sh --seed`) — never pass data-file args to recomp.
* **2. Menu artefact completion (rule 6):** xemu shotseq now uses
  unique dump names (collision fixed); FB law still OPEN (linear +
  Z-order both decode TEXTURE at 0x1A3B000; `0x3C00000` pitch-720
  law = old-ROM debug-overlay era only). QMP probe decides whether
  an in-emulator screenshot path exists.
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

- **H31 wall write-up (AGENTS work-loop rule):** 41+ runs
  (frz93→134), 14 re-poses, 4 header-level facts overturned mid-wall
  (regime split, trailer-bit4, events-never-Set, START-spam
  confounder). Host side now EXONERATED end-to-end (frz128–134:
  loader healthy-parked, completions complete, input A/B clean);
  the wall is engine-side re-issue. Next comparison if the static
  decode stalls: xemu kernel trace of the same load.
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
- Harness current: `/tmp/opencode/nav_run35.sh` (8-start spam,
  early-kill on `[PUMP] x167 `) and `/tmp/opencode/nav_run36a.sh`
  (frz18-exact presses, no early kill — clean-input A/B);
  `core_forensics.bt` + `core_ram.py` Core class. `/tmp/opencode`
  is tmpfs: everything there vanishes on host reboot.
- Other useful env: `RECOMP_CACHE_CAP_BYTES`, `RECOMP_WATCH=<va>…`,
  `RECOMP_SEM_TRACE`, `RECOMP_LOG_ORDS=…` (patch homes: THIRD_PARTY
  table).
- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env,
  skills_check}.sh`; logs -> `logs/`, captures -> `reference/`
  (gitignored); cores via `coredumpctl dump <pid>` (~2.4 GB each —
  26 now / ~11 GB in /tmp/opencode; prune to frz117/121/123/126/127
  era at next pause); cache dir `~/.local/share/xboxrecomp/Cache/*`
  between long runs.
- Shell law: never edit a script a live bash re-reads; never edit
  `src/recomp_manual.c` while a gen+build chain is in flight (gen
  reads OLD manual, make compiles NEW -> `undefined reference to
  sub_X_gen` — explains all three "transient" wrap misses); never
  rebuild `build/sudeki` while a run/core is in flight (copy the
  binary to `/tmp/opencode/sudeki-frzNN.bin`); watcher stdout
  block-buffered; kill by exact pid or `pgrep -x sudeki` (NEVER
  `pgrep -f`).
- Seeds file (local, 18 measured VAs): `logs/seeds-icall-0011C640.json`.
- Deterministic markers: PUMP x167 terminal, EE110=167, baf-read
  counts vary by kill timing (don't pin on them); regime pinned in
  run header from now on.
- Git: never push (rule 7); local commits through `729777e` (+ this
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
| 11-23 03:0x–06:3x s5b | frz128→134 census arc: evt7 polarity closed, loader healthy-parked, submit chain runs (ISSMTH x17 drain/SWEEP x0), state-3 refuted, 522 CQ pushes all-completions, START-spam confounder convicted then exonerated (frz134 clean-input reproduces wall) -> **H31 re-posed x14: engine-side stop-of-issue**; xemu FB capture pipeline live (`reference/xemu/title90.png`) | `99d6370`..`729777e` |
