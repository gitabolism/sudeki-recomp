# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-22 21:0x (session 4x)
**Stage:** LIFT complete, ENTRY strong, **MENU artefact reached**;
**navigation works**; load-stall wall (H31) **anatomy complete**:
install writes complete byte-exact, read-back of `Cache//startup.baf`
is the dying stream; its consumer = the BAF job-pump chain
`sub_001EDA20 -> sub_0023B830 -> vtbl[1] read -> sub_0001E170 ->
sub_00270FBD`. `[PUMP]` census (frz101) decides WHERE it stops.

## Where things are verified

- Toolkit: `xboxrecomp/` @ v0.18.0 / `37b61d589168adee3680fabd51805ee81427bf07`;
  `scripts/doctor.sh` green (also XBE SHA-256 pin, skill digests).
  Local changes = patch series `patches/xboxrecomp/0001..0047`;
  **the patch-by-patch table lives in THIRD_PARTY.md** (its one home);
  `scripts/build.sh` resets `src/` to the pin and re-applies in order.
- Manual overrides (`src/recomp_manual.c`, `--exclude-manual` in
  `scripts/pipeline.sh`): refill/error-slot/posters/audio census/
  swap-gate census wrappers; gen tree emits `*_gen` bodies; stubs 432.
- Ghidra bench installed (12.2 @ pinned JDK, doctor green); the flat
  import recipe is in THIRD_PARTY.md — **project at `/tmp/opencode/
  ghidra/sudeki` was lost to the host reboot; re-import when needed**.
  `/tmp/opencode` is ephemeral: cores/harness scripts cited below may
  need recreation (harness: `nav_run3.sh`, `frz_watch2.sh`,
  `core_forensics.bt` — all re-derivable from the notes).
- Git: scrubbed+reseated history **pushed**; `origin/main = 7ce51cb`
  (user pushed, verified 20:1x). Pre-scrub backup bundle + map were
  deleted after push verification (audit drift fixed: they were cited
  as live).
- JP second sample: `~/Downloads/xbox/sudeki-jap/default.xbe`
  (parked, outside git; naming `<game>-<region>/`).

## Last run

Session 4w (19:0x–20:2x+): **H30 root-cause landed, GPU wall died,
H31 framed.**
- frz90 (matched-symbol plateau core): D3D hand-built RAM events
  (swap-slot 0x2E2F4C / ring-free 0x2E2F5C / 0x00F7FC7C) were never
  shadow-registered -> waits resolved to garbage -> instant-fail
  spins (note 0024, CLOSED as the H30 cause).
- frz91 (`RECOMP_TITLE_KEVENTS=1`): mirrors land for 4C+7C; plateau
  morphed (quiet engine block on 4C); completion Sets kept firing.
- frz92 (pad-gated nav + 5C mirror built on demand): **GPU wall
  DEAD** — class-5 chain ~1100 wakes, ring writer draining
  (submit=20112), engine ~10 fps, and nav REACHED A LEVEL LOAD
  (`speech_hask*/shadow/TSA.xwb` stream). Watcher false-fired on the
  healthy load era (0-fps presents are normal while loading — H28
  rule); v2 watcher counts GATE `acc` deltas too.
- frz93/frz94 (2500 s, acc-aware watcher): TRUE plateaus at 642/1128 s.
  Load queue: `tot +30/s, acc +28/s` but `cur` (+0x280) FROZEN at
  0x000E5D5C, `done/tick` 0 -> streaming throttle (`sub_002C3200`,
  note 0023 anatomy) skips rendering forever; presented frame =
  99.6 % loading card (frz94: 9,964 presented window frames). GPU
  aperture fully idle-clean at kill; watch on 4C object quiescent
  (20 transitions all run) — coherence NOT the issue. Terminal shape:
  engine parked INFINITE on mirrored 4C, SignalState=0 (nothing
  asks for the next swap). Repeated identical `want=116 got=116`
  ZERO-byte seq-reads from `0x00271057` in `sub_00270FBD` (the
  standard read helper) = the stuck-unit signature -> **H31**.
- Patch 0047: `[READ] @seq` form gains `sz=`/`path=` (EOF contract
  verified correct upstream: 0-byte read -> `STATUS_END_OF_FILE`, so
  those zeros are real stored content, not a past-EOF artifact).
- **CORRECTION (frz94 core forensics + `core_ram.py` core-reader):
  the frz93/94 "load queue" numbers are Bink MOVIE stats** (see note
  0025 anatomy; ring content matched `Movies/Sudeki Atttract
  Mode.bik`; tree-vs-disc size audit 443/443 exact). H31 re-posed:
  awaited load-completion event unknown, movie loops under the card.
- Harness (user directives): AGENTS rule 12 (mission-only content),
  wall-clock discipline + every-3rd-session audit, `/review` command
  (+ README/progress-bar sync as worktree drafts). First audit ran
  (fresh context): artefacts verified, drift list applied by this
  compaction; its tactical steer adopted = core+static forensics
  FIRST, frz96 (window 1200 s, plateau ~t+500 post-nav) parallel.

Earlier sessions live in the notes (0005–0025) + the session log; the
4n–4v blocks are summarized there, not here.

## Live hypotheses

1. **H31 (ACTIVE, note 0025, RE-POSED x3): the load stalls when
   `startup.baf` config reads stop at 41 % — deterministically at
   read-head `0xAD5D09`.** frz101 census: pumped managers drain
   while the HMF-class queue (`0xFD9550`, global `0x397668`) keeps
   an unserviced ~177-node backlog; last pump tick == last read.
   Consumer chain named: `sub_001EDA20` tick -> `sub_0023B830` pump
   -> vtbl[1] -> `sub_0001E170` -> `sub_00270FBD`. Next datum:
   `[MTICK]` (which gate closes / who stops calling the tick).
   Serving layer exonerated read+write; GPU/events healthy. Old
   "cur/done queue" census was movie pacing (labels fixed in
   recomp_manual.c).
2. H14–H30 CLOSED — one-line lessons index (homes = notes 0005–0024):
   cache capacity ends the boot write-loop (0005); async-IO return
   convention = the phantom charge (0011); ICALL-skip stack drift
   (0018); AC97 trap arch gap (0019 era, patch 0022); input = arm
   OHCI + scripted-pad START bit (0020); sw-method latch/ack chain
   (0021); `ke_shadow_init` double-init (0022); load-state pump
   pause is the game's own design (0023); service-chain livelock
   (0023); D3D RAM events need shadow mirrors — `RECOMP_TITLE_
   KEVENTS=1` is now part of the working recipe (0024).

## Queue (next concrete actions)

* **1. frz102 (MTICK probe)**: build after gen+idle; `nav_run10.sh
  102 1200` with `RECOMP_RD_TRACE=1` -> `[MTICK]` names which gate
  of `sub_001EDA20` closes when the stall begins (or shows the tick
  itself stops being entered -> then wrap ITS caller). Pair with
  frz101 core facts (backlog queue at `0xFD9550` unserviced).
* **2. Node semantics decode** (frz102 core, matched bin): the
  stride-0x24 node class at `0xFD9550+04` queue — find its vtable
  from code cross-refs, not heap guesses; then read the in-flight
  node's file-object/handle/offset fields directly.
* **3. Then the Gameplay gate:** first presented level frame =
  fbpeek + visual + texture-coverage carry-over; xemu side-by-side
  of the SAME load as corroborator (needs the QMP/HMP script fix;
  note 0019 recipe) — corroborator only, not the shortest path.
* Carry-over (opt-in probes unchanged: SWMARK_INTR12/PF/IRQPOST/
  PB_PARK; regime v4 + TITLE_KEVENTS + queue-2048 + 0046 + write
  knobs now proven safe one-run-at-a-time); JP disc regression
  sample later; roadmap staging (vanilla -> Vulkan -> features) per
  the user directive, untouched. Post-playable feature order named
  by the user (2026-11-22): HDR -> Raytracing -> Multiplayer
  (LAN/VPN co-op + split-screen). Prior art for that stage:
  `github.com/sudeki-together/sudeki-mp` (SudekiMP — hooks the GOG
  Windows `SUDEKI.exe`, same SOL engine; AGPL-3.0; deep engine RE
  docs: structures/engine-functions/combat-graph/lan-arena protocol;
  reference value only — any code reuse is a license call for the
  user; never vendored without it).

## Blockers / open questions

- None blocking. xemu reference boots the title (note 0019); its
  headless-capture script needs the QMP/HMP fix before side-by-side
  work (queue item 4).

## Housekeeping facts (recipe home)

- Working recipe (frz92+ regime): `RECOMP_VBLANK=1 RECOMP_PB_EXEC=1
  RECOMP_PB_WALK=dma RECOMP_KERNEL_LOG_BUDGET=0 RECOMP_AC97_READY=1
  RECOMP_USB=1 RECOMP_ARM_UNTIL_SEEN=0 RECOMP_TITLE_KEVENTS=1` +
  FB dumps (`RECOMP_FB_WINDOW=1 RECOMP_FB_DUMP=reference/fb/NN
  RECOMP_FB_WINDOW_DUMP_EVERY=60`) + census (`RECOMP_IO_READ_LOG=1
  RECOMP_GATE_TRACE=1 RECOMP_PRES_TRACE=1 RECOMP_EVENT_LOG=1
  RECOMP_GSVC_TRACE=1`); harness trio `nav_run3.sh` (pad-gated nav),
  `frz_watch2.sh` (flips AND GATE-acc progress), `core_forensics.bt`.
- Other useful env: `RECOMP_MISSING_FILES=all`, `RECOMP_IO_WRITE_*`,
  `RECOMP_CACHE_CAP_BYTES`, `RECOMP_WATCH=<va>[_LEN/_RAW/_PANIC]`,
  `RECOMP_SEM_TRACE`, `RECOMP_LOG_ORDS=...`, `RECOMP_POST/REFILL/
  DS_*/TLS_*/PB_UNH_TABLE` traces (patch homes: THIRD_PARTY table).
- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env,
  skills_check}.sh`; logs -> `logs/`, captures -> `reference/`
  (gitignored); cores via `coredumpctl dump <pid>` (~2.4 GB each);
  cache dir `~/.local/share/xboxrecomp/Cache/*` between long runs.
- Shell law: never edit a script a live bash re-reads; never rebuild
  `build/sudeki` while a run/core is in flight (copy the binary to
  `/tmp/opencode/sudeki-frzNN.bin` for matched forensics); watcher
  stdout is block-buffered (file lag != dead watcher).
- Seeds file (local, 18 measured VAs): `logs/seeds-icall-0011C640.json`.
- Git posture: push only with the user's explicit approval (AGENTS
  rule 7); history was rewritten once, user-approved 2026-11-22, and
  re-verified clean at HEAD and in `origin/main`.

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
| 11-22 19:56– s4w | **H31 framed (note 0025)**; frz92 GPU-wall-dead verdict chain; patch 0047; /review harness + first audit + README sync (drafts) | `c4b9be4`.. |
