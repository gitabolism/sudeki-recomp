# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-23 00:5x (session 5a)
**Stage:** LIFT complete, ENTRY strong, **MENU artefact reached**;
**navigation works**; load-stall wall (H31) anatomy now at pump /
service-processor level: final leg = manager `018CF14C` (job id
`0A260002` at `+0x18`, size `0x1400` at `+0x1C`); its pump x167
(bypass site, `eax=scratch ptr`) reads the handle object via the
`047CE4/1145D/1152A` vtable chain, takes the trailer dword
`3EF04C91` (`buf+6140`, bit4 CLEAR) -> SYNC `F9720` branch, ret=1;
after that pump/tick servicing is silent and the loader wait-cycle
(`EAAB0`) loops `WAIT_TIMEOUT (0x102)` forever — load events
`0x37A118/0x37A114` are NEVER Set in any run; job-slot teardown
`ECB10` fires ZERO times. TWO REGIMES: **R1 fresh-install** (pumps
freeze ~x119 in sentinel polls, final job never created) vs **R2
reused-install** (pump x167 + cycling 17-position record chain over
the installed-cache tail). Patch 0050 (positioned IO = pread/pwrite,
NT FPO semantics) is correct but NOT the wall — the guest uses
explicit Set-Position + seq reads.

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

Session 5a (22:0x-00:5x): **frz107->116 census arc; H31 anatomy at
processor level; patches 0048/0049/0050 landed.**
- frz108-109 funnel: walk `EF1D0` only ~66 session-wide (earlier
  "~255" was the registrar count); walks match legs by
  `obj+0x10 == ctx-filter`.
- frz110 (`[EACYC]/[RESREL]/[RESADD]` + `LOG_ORDS=123,138,145,205,
  225`): wait cycle loops `0x102` (loader thread NOT parked); load
  events NEVER Set; `RESADD` (sole caller `F99D0`@1F9DAF) = 0 events.
- frz111/112: 5th/6th deterministic R2 replays; `F99D0` fired ONCE
  ever (boot attract cmd via `sub_00240390`) -> relay legs run via
  SYNC `F9720`; `JOBFREE` (`ECB10`, sole `0x37A3D0` slot clearer)
  fires ZERO times per run.
- PUMP probe arg-slot fix (bypass site = 1 arg): true final-leg mgr
  = `018CF14C` id `0A260002` size `+0x1C=0x1400`; the old "018CF100
  id=5678" reading was the wrong stack slot.
- frz113 core: trailer dword `3EF04C91` (bit4 clear -> sync branch);
  5120-B record buffers all-zero in-core while installed file has
  mixed data at the seq positions.
- patch 0048 `[READSEQ]`: seq positions JUMP (24.6M->27.6M->back) =
  guest does explicit Set-Position + seq reads on one handle.
- patch 0050 pread/pwrite (NT FPO semantics): correct, but frz116
  shows the wall unchanged -> guest never relied on the broken
  semantics; keep the fix regardless.
- REGIME SPLIT: R1 fresh-cache (frz116: pumps freeze ~x119 sentinel
  polls, final job never created) vs R2 reused-cache (frz103-115:
  pump x167 + 17-position chain cycles). All "deterministic" replays
  so far were R2; pin the regime in run headers from now on.
- Boot era reads DISC `game/startup.baf` (8,585,216 B positioned,
  installer); load era streams INSTALLED `Cache//startup.baf`
  (27,762,688 B seq); installed vs disc: 0/2096 identical 4K blocks
  (install expands+relocates; disc byte-compare invalid).

Earlier sessions live in the notes (0005-0025) + the session log; the
4n-4w blocks are summarized there, not here.

## Live hypotheses

1. **H31 (ACTIVE, note 0025, RE-POSED x7): the final relay leg's job
   never completes -> its manager never sentinelizes; servicing gates
   shut; loader wait-cycle spins.** R2 terminal shape: pump x167
   (bypass site, handle `E1750 -> 010E3178`, trailer `3EF04C91` ->
   `F9720` sync branch, ret=1); after it: zero pumps, but the tick's
   `23A4F0` leg still fires 256+ times during stall -> tick RUNS,
   pump gates (`D550`, job-state&3, `HI8(obj+0x28)&2`) shut;
   `EAAB0` loops WAIT_TIMEOUT; events never Set; `ECB10` never fires.
   frz117 census: `[EED70]` (completion handler `F9720` calls at
   1F92C9/1F99A9), `[D550]` (tick gate helper), per-event `[MTICK]`
   -> names the failing gate or proves completion ran without any
   Set path.
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

* **1. frz117 (gate census)**: wrap `[EED70]` (sub_001EED70, the
  completion handler F9720 calls at 1F92C9/1F99A9), `[D550]`
  (sub_0023D550 tick pump-gate helper: args + result), per-event
  `[MTICK]` (tick count is small). Verdict tree: EED70 fires during
  stall but no Set path -> hunt the missing Set host-side; EED70
  silent -> the F9720 guard chain refuses (read its masks with the
  real trailer `3EF04C91`); D550==0 -> the tick gate itself.
* **2. R1-vs-R2 regime header**: pin regime per run (cache clear or
  not) — and re-check whether R1's earlier freeze (no final job at
  all) is the SAME wall one stage earlier (it likely is: fresh
  install = first-ever run of the load; real users start in R1).
* **3. Relay-leg decode** (unchanged): map load-table descriptors
  (`0x00F95678`-class `{flags, buf, size, tag}`) to archive FILE
  names so "the leg that never finishes" gets a file, not just a VA.
* **4. Then the Gameplay gate:** first presented level frame =
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
