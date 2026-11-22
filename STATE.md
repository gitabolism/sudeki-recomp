# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-22 (session 4u)
**Stage:** LIFT complete, ENTRY strong, **MENU artefact reached** —
`reference/fb/frz1600027.bmp` is the fully textured title screen
(SUDEKI logo + character art + "Press START" + rain, 37,025 distinct
colours); input reaches New Game (note 0020); audio + level/wavebank
streaming work. H28, H19, H29-GPU-chapter CLOSED. frz87 verdict:
17,435-flip marathon = deepest run yet but the content era is the
RAIN-ANIMATED TITLE (fbpeek + visual confirm) — plateau depth is pure
luck (189..17435) around ONE wall; the Gameplay gate is now
navigation→presented-level-frame. frz89 nav run IN FLIGHT
(log `logs/run-frz89-174613.out`, window to ~18:11; plateau-watcher
armed → auto-ABRT core `sudeki.frz89` planned; dumps `reference/fb/frz89*`).
Ghidra secondary lens ONLINE (project `/tmp/opencode/ghidra/sudeki`).

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
  `RECOMP_WATCH` arm fixes (errno, multi-page window), `0014` async-IO
  return convention = the boot-archive fix, `0015` event create/wait/set
  caller trace, `0016` `0x1D70` honor-`0x1D6C`-offset fallback +
  `RECOMP_SEM_TRACE`, **`0017` flip-done settle chain**
  (`nv2a_pb_set_flip_done_chain`, ack-tick clear at DMA_GET==DMA_PUT),
  `0018` watch arming below `XBOX_BASE_ADDRESS`, `0019` watch
  guest-stack + host-rip attribution, `0020` watch TF single-step re-arm
  + `RECOMP_WATCH_PANIC`, `0021` `RECOMP_LOG_ORDS` never-budgeted ordinal
  trace, **`0022` AC'97 bus-master write trap on Linux x86-64 = the audio
  fix** (POSIX arm guard widened; decoder+routing already existed),
   `0023` scripted-pad START bit, `0024` VSH-IO trace, **`0025`+`0026`
   sw-method latch/FIFO + hold-until-seen ack ownership = the H26
   fix**, `0027` `ke_shadow_init` once-guard = the H27 fix, `0028`
   `[READ]` log gate, `0029` ranked unhandled-method table, `0030`
   class-5 re-latch on walk drain edges (fired faithfully; the NOP
   path is NOT D3D's real ring-free protocol - see H29), `0031` BEW
   release → channel block, `0032` host ring-free event signal,
   `0033` GET-lag instrumentation (falsified as fix, kept as probe),
   **`0034` trap-quartet idle on service + `0035` consume-at-first-
   read = the service-chain livelock fix** (H29 GPU chapter closed).
  Table in THIRD_PARTY.md.
  Manual overrides: `--exclude-manual src/recomp_manual.c` now wired into
  `scripts/pipeline.sh`; wrappers = `sub_00022340` (stream refill),
  `sub_0026C79D` (error-slot setter + TLS guard), `sub_00021CD0/
  sub_000220E0` (posters), `sub_0028EA91`/`sub_002854B3` (audio census);
  gen tree emits bodies as `*_gen`, stubs still 432.
- XBE: pinned SHA-256 ✔. Gen tree regenerated session 4p with
  `--exclude-manual` + the 18-seed file (incl. `A8A10 B5A50 BC450`);
  432 stubs stable.
- Skills: five pinned, digest gate green.
- JP second sample lives at `~/Downloads/xbox/sudeki-jap/default.xbe`
  (3,842,048 B, SHA-256 `1f8a9611…`, XPP section identical size to World);
  naming convention for parked disc artefacts: `<game>-<region>/` (user
  directive 2026-11-22).

## Last run

Session 4u (latest): **frz87 deep-marathon verdict + pump-chain
anatomy + Ghidra lens online.**
- frz87: 17,435 flips, content frames to t+23:45 — but fbpeek + visual
  check of late dumps = TITLE screen (rain = the 98 % inter-delta).
  Sparse feed never navigated. All mark-layer knobs stay as of 4t.
- Static anatomy (lifted C + Ghidra pseudocode, note 0023): pump
  `sub_002DB250` busy-spins on `[aperture+0x400700]` (gate PGRAPH_INTR
  bit 0x1000); completion `sub_002DAFF0` = PCRTC-ack store, spin while
  `PMC_INTR_0 & 0x01000000`, then `KeSetEvent(this+0x194,1,0)` = the
  swap-slot event 0x2E2F4C (ICALL slot 0x3157DC); `sub_0011ED20` calls
  engine `sub_001DE5C0` directly (640x480 swap descriptors on stack).
  **No write sites to +0x400700 exist in the whole gen tree** — the
  latch theory needs a matched-symbol plateau core (frz83 core =
  stale symbols, unreliable frames).
- frz88 lesson: flip-count gate can't see title (attract/title eras
  climb flips forever); run killed mid-movie-era by mistake → core
  `sudeki.frz88.core` kept as HEALTHY baseline: PMC=0 PGRAPH=0,
  +0x704=0x100 (NOP mark armed), +0x708=0x0DF80101, +0x720=1,
  head/tail=372/376, ctx=0x2E1190 ctx+0x8700=51747D8B.
- frz89 = nav_run.sh with `.bik`-log gate (first movie open + 45 s =
  title settled, then 4 starts @10 s, then silence — frz18 proven
  cadence). Plateau watcher `/tmp/opencode/frz89_plateau_watch.sh`
  auto-ABRTs on CONFIRMED plateau (re-checks last fps line — the
  frz88 mistake is encoded away).
- Ghidra: flat-image import recipe landed (THIRD_PARTY usage line);
  pseudocode lens `/tmp/opencode/ghidra_pd.txt` agrees with lifted C.
  API verification via javap against the pinned jar (lesson recorded).

Session 4t (earlier): **mark-machinery regime search (frz62-70, note
0023)** — the terminal plateau's anatomy is now measured end to end:
the walk's drain-edge class-5 generator (0030/0032) fires per drain
(frz69: 3552 edges) far faster than the guest's single-trap service
chain consumes; when the pending queue (64) overflowed it LOST
class-5 ring-free + swap marks (frz62 drop=609, frz65 1546) = the
H30 "feeder frozen" signature. Regime census:
  v4-churn + queue-depth-2048 (0039) + SHORT pad feed = frz66:
    3659->7113 flips STILL CLIMBING at kill, no plateau = best.
  strict one-owner (0040): frz68 froze at 1850 with a 1500-mark
    backlog (enq=1659 >> consume=335) — weaker regime.
  dense post-movie pad feed = harmful (frz69 froze at the exact
    frz62 point 1531; matches the older "dense START distorts the
    state machine" finding) — feeds reverted to short cadence.
  Patch 0041 = generator back-pressure (drain edge invisible while
    a mark is armed; idle-gated pop). The whole 004x probe family
    after it: 0042 walk-suspension and 0043 irq-post both FROZE boot
    and are now OPT-IN probes; 0044 PMC-PF bit and 0045 regime-scoped
    pop-gate (the 0040 gate had leaked into the v4 baseline and froze
    frz77 at 187) are current. Series 0001..0045 verifies from pin;
    stubs 432 stable.
  frz79 CORE at plateau (v4+PF): events healthy (4C sets 1:1 with
    flips, 5C signalled-idle), mark machinery idle — the wall is the
    D3D ring WRITER not emitting the next swap NOP while the engine
    (sub_001DE5C0, INFINITE) and the swap-slot credit gate
    (sub_002D3420 @0x2D3497, INFINITE) wait on that completion.
    Freeze depth is heavily run-luck (189..7113 across arms).
  Arms in flight: frz80 = exact-frz66-regime core probe (PF off);
    frz81 = same + RECOMP_PB_GET_LAG=0 (D3D fence-path selector).
  Shell hygiene: never edit census_run.sh mid-run (live bash
    re-reads); pin exact log paths in census greps.

Session 4r (earlier): **H29 anatomy COMPLETE (commit `af91f07`)** —
three patch-0030 iterations + censuses: v1 ack-thread poll edge never
observed (drains finish inside a tick); v2/v3 walk-tail drain edge
FIRES faithfully (`walk work==drained==6098`, `rering latched=1`,
drop=0) but a second `0x2E2F5C` wake never lands (frz25r2: the single
class-5 NOP serviced ZERO times). frz26 ring-writer census
(`[PRES] inst/i0/i4/latch/uGET/w2C`) shows the plateau is BIMODAL:
frz20 = writer parked on 5C; frz26 = writer ALIVE (submit climbing,
`iGET==cursor` exactly, latch advancing) with pump/eng frozen +
`wait=5` — the H28 pump signature again. Constants everywhere:
`sh8700=51747D8B` poison swap-slot, `i0=i4=0` (channel-block
semaphore words never written; every `0x1D70` release rejected
`off=0`), `iGET` never lags. Verdict: class-5 NOP emulation is not
D3D8LTCG's ring-free protocol — the BEW release chain is; fix
candidate 0031 (`off==0` → `xbox_Nv2aRegPtr(0x800000+off)`), verify
frz27. Pre-push audit GREEN (tree + full history + secrets + pins);
**pushed by the user 2026-11-22: `origin/main` = `57ea06b`** (the
harness policy denies agent `git push`; push URL now set to the real
one).

Session 4q (earlier): **H19 inventory CLOSED (note 0023 frz16)** —
ranked `[UNH]` table (patch 0029, `tools/unh_names.py`, names vs nxdk
`nv_regs.h` cached at /tmp/opencode/nv_regs.h): all 40 top unhandled
methods are benign render states, none draw-blocking; executor
already rasterises the level scene (6139 VSH batches, 33.6k tris/
pass); frz16 dump 27 = fully textured title screen (37k colors). The
Gameplay gate moved from GPU coverage to **input flow**: dense start
feed (every 8 s) distorted the state machine (second 110 s load;
dumps 17-30 all the SAME frozen title frame). frz17 queued: sparse
navigation + 15-flip dump cadence.

Session 4p: **H28 CLOSED** via five census runs (frz10-15,
`RECOMP_GATE_TRACE/RECOMP_PRES_TRACE` manual wrappers on
`sub_002C3200/_C27A0/_D5BF0/_D57D0/_D5760/_D5C80/_D55E0/_D3B50/
_D3CC0/_D35A0/_001E0390`). The post-attract "stall" = the engine's
level-streaming load state: frame loop + flip-issue alive (30/s),
swap-completion pump `sub_002D35A0` + engine submit `sub_001E0390`
frozen exactly at the flip count (their only caller `sub_0011ED20`
is a device-message handler the load state stops invoking), IO
streaming continues (~90 MB in-window), recovery → steady ~1.8 fps
gameplay. No host defect; spin-vs-long-haul answered: LONG-HAUL.
frz15 A/B (patch 0028 read-log gate): load duration unchanged
(~110 s) → load bound by emulated CPU, not tracing. Patches 0028 +
0029 (`RECOMP_IO_READ_LOG`, ranked `[UNH]` unhandled-method table)
in the series; frz16 = first H19 inventory run with
`RECOMP_PB_UNH_TABLE=1`.

Earlier 4n (still valid baseline): freeze walls
**H26+H27 both CLOSED** (notes 0021/0022; patches 0024-0027 in the
series). gfault9 proved the GPU→CPU sw-method chain (`KeSetEvent
0x2E2F5C from 0x2DB61F`, `pop=1149 ack=2475`); frz4 core root-caused
the surviving freeze to the unguarded `ke_shadow_init()` double-init
(main guest thread blocked forever in `ke_shadow_lookup` on an
orphaned mutex; CS read all-zero with init flag 1); patch 0027
(`InitOnceExecuteOnce`) cleared it — gfault10 (250 s, 3534 flips,
1.84 fps steady, frames animating 3 %/frame, 35k colours) and
gfault11 (plateau-lines=0, 4.43 fps climbing) both clean; no freeze
recipe reproduces it anymore. gfault12 (RECOMP_VSH_IO census +
denser FB dumps, 320 s) in flight for the next wall: vertex-program
coverage in the level scene. Working recipe unchanged; add
`RECOMP_VSH_IO=1` for GPU census runs.

**xemu reference status at pause (user testing manually):** xemu 0.8.139
is the *latest official release* (checked vs GH API) — same as pinned.
HDD qcow2 verified pristine (digest matches pin). Disc is the Redump ISO
(via `sudeki-world.iso` symlink). Upstream evidence says the title
RUNS on xemu: compat page "Starts", Jan-2026 report says intro FMVs
play perfectly, top-issues list has Sudeki *ingame* vertex explosions
(#2039) — nobody upstream reports a boot-time STOP. So our `STOP 0x7E`
in the `sub_000223F0` decoder is environment-specific, NOT an xemu-wide
title bug. Suspects eliminated: HDD cache corruption (pristine), bad
disc (Redump), stale binary (latest). Open suspects: the `-m 64 -m 128`
flag doubling in older probe runs (128 won; a clean 64 MB run was never
completed — probe script QMP/HMP mismatch), xemu-generated EEPROM
language/AV fields, and the xemu-dashboard HDD interaction. NOTE:
`~/.local/share/xemu/xemu/xemu.toml` now has `mem_limit = 64` added
session 4k (was default/extra-flag); if 64 MB stalls at the SpiritBoot
logo, set it back to 128. xemu ref-script needs the QMP-vs-HMP fix
before more headless captures.

Session 4i (final cycles): **audio wall closed by patch 0022** —
`env RECOMP_VBLANK=1 RECOMP_PB_EXEC=1 RECOMP_PB_WALK=dma
RECOMP_KERNEL_LOG_BUDGET=0 RECOMP_AC97_READY=1 ./build/sudeki` →
`logs/run-ac97d-1-*.out`/`-2`: APU up (SDL2), "AC97: bus-master writes
trapped at 0xFEC00000 (POSIX)", wavebanks incl. `speech_buki` stream,
1225/1211 flips, deterministic SIGSEGV `0xFE087E99` at `0x1A9C1D`
(real `sub_001A9B60` body; reported `sub_001A9A50+0x1CD`) — H23. With
`RECOMP_DS_INIT=1 RECOMP_DS_VOICE=1` (`logs/run-ac97c-214452.out`): all
five DDS9 sub-inits -> 0, zero `DS-VOICE!`/`DS-COPY!` (probe wrapper skips
the init tail so that run dies earlier at `sub_0028A5A4+0x184` — wrapper
artifact, not a product bug). Pre-fix runs: sub 3 = `0x88780078`
DSERR_NODRIVER (`logs/run-dsinit1-213937.out`).

Session 4i (mid cycles): audio-wall anatomy via manual wrappers —
`env RECOMP_VBLANK=1 RECOMP_PB_EXEC=1 RECOMP_PB_WALK=dma
RECOMP_KERNEL_LOG_BUDGET=0 RECOMP_DS_COPY=1 RECOMP_DS_VOICE=1
./build/sudeki` → `logs/run-dsvoice-212823.out`: one `[DS-VOICE!]
this=029A00F4 slot14=00000000 ret=00285689` then one `[DS-COPY!]
this=0 … off=00000818 dst=00000818 src=027F3698 ret=002893C0` —
the wavebank chunk (0x6168 B, from `buf=0x027F2E80 want=24936` read)
copies onto guest 0x818..0x6978 → TIB page zeroed → guarded TLSSET
(`torn-down TLS` fires once) → surviving crash moves to
`sub_000B08F0+0xDFB2` writing through `esi=0x89D8458B` during
`spot_sounds.xwb`/`UISoundScript.xsb` phase. Note 0017 outcome filled
in; the TLS-lifetime reading is falsified.

Earlier 4i: `env RECOMP_VBLANK=1 RECOMP_PB_EXEC=1 RECOMP_PB_WALK=dma
RECOMP_KERNEL_LOG_BUDGET=0 ./build/sudeki` (settle chain registered in
`src/main.c`, TLS guard in `src/recomp_manual.c`) →
`logs/run-tlsguard-204217.out`: **PROGRESS** — movies play (331 flips),
then the title streams first-level assets: `SOLWORLDM.gex`,
`z:\wavebanks\BS_brightwater/Elco_weapons/S_missile.xwb`,
`D:\sound\missiles.xsb`, second `badger.plap` pass. No crash (TLS guard
fired once: `[TLSSET] torn-down TLS (fs=00004000 arr=00000000) — store
skipped`). State at 120 s: main active in `sub_0011D460` (allocator
scan; PC advances between cores = progress, not spin), NV2A USER
PUT==GET (`0x000B9018`), vblank DPC re-inserts ~48/s (ICALL
`FE0001A0` = `KeInsertQueueDpc` slot), **no flips after the movies**
(menu draws are all vertex-program batches → skipped), zero
`NtDeviceIoControlFile` (pad device never opened yet).
`DECODE-EXACT` still holds.

Earlier 4i runs: `logs/run-fd1-202904.out` (first crash site, TLS
fault), `logs/run-walk1-*.out`/`run-sem1-3` (fence/KickOff/semaphore
anatomy), `logs/run-watch4k2-203539.out` (4-byte watch on 0x4004 via
patch 0018), `logs/run-tls1-203840.out` (461 healthy TLSSET then
arr=0).

## Solved this session (4i; notes 0016, 0017)

1. **Post-movie Present/ring-rearm wait** (`sub_002D3CC0` spinning on
   `[ctx+0x8700]==1`, ctx=`*([0x2E3638]+0x934)`=0xFD000000, stable
   across cores): the GPU ring-conpletion word. Patch 0017 =
   `nv2a_pb_set_flip_done_chain` + ack-tick clear when DMA_GET==DMA_PUT
   (measured chain, registered in `main.c`) → level streaming reached.
   The `0x1D70` releases are ring counters whose target resolves via
   the DMA instance (`0x1D6C` programs offset 0), so the
   honor-the-offset fallback (patch 0016) never fires; kept as trace.
2. **TLS teardown crash** at `sub_0026C79D` (XDK error-slot setter,
   `slot=[[fs:4]+(-37)*4`, tls_index@0x376620=0xFFFFFFDB): after a
   thread's TLS teardown the TIB array word holds poison (0 /
   0xC0000000) yet guest code still runs with that `g_fs_base`.
   Toolkit thread-lifecycle gap (note 0017); probe guard in the manual
   wrapper skips the store (`RECOMP_TLS_GUARD` default on,
   `RECOMP_TLS_TRACE=1` census).
   **CORRECTED later same session (note 0017 outcome):** no thread
   teardown involved — a DSOUND/XACT packet copy with `this=NULL`
   (voice child slot +0x14 never initialized) writes a 24 KB wavebank
   chunk to guest 0x818, zeroing the TIB page as a side effect. Guard
   kept as insurance; real wall = audio init gap (H21).
3. Tools: `RECOMP_WATCH` arms below `XBOX_BASE_ADDRESS` (patch 0018);
   4-byte watch caught `fs:[4]` init (`00770094→00F80094` =
   `bridge_layout_tls`) and the teardown zero; TLSSET wrapper census;
   `RECOMP_PB_WALK=dma` is part of the working GPU recipe.

## Solved this session (session 4h, note 0011 post-scripts 3-5)

1. **The post-decode "3-way deadlock" was a lift gap, not events:** the
   q2 task-completion notify `ICALL 0x0011C640` was unresolved (skipped).
   `0x11C5F0/0x11C610/0x11C640` are real func_id misses (int3-padded,
   tail-jmp `0x11D200`). Seeded the measured one → completion notify ran
   → token `0x48000004` set **for the first time** → main proceeded.
2. Second/third misses surfaced (`0x001E330`, `0x00273EEF`) — same
   seed-file mechanism; gen tree now has **zero unresolved ICALLs** at
   boot (`logs/seeds-icall-0011C640.json`, local per gitignore law;
   addresses recorded in note 0011).
3. **Boot reaches the movie:** `startup.baf`, `SOLXM.fubi`,
   `badger.plap` (FNT/FMH/PSH/SHX chunks), 26 MB VRAM arena,
   `D:\movies\MS Game Studios Logo-final.bik`.
4. **Flip wall solved:** XGRAPHCL flip-pending flag `ctx+0x8700` is
   GPU-written; runtime needs `RECOMP_PB_EXEC=1` (method executor, off
   by default upstream) + `RECOMP_VBLANK=1` (guest ISR at vector 3
   claimed) + our `xbox_Nv2aMirrorFence(0x2E3638,0x2C,0x30)` in
   `src/main.c`. First frame dumps exist (black).
5. Tools: poster wrappers `sub_00021CD0/sub_000220E0` +
   `RECOMP_POST_TRACE=1` (settled the task/event anatomy); PC-sampling
   via two timed cores distinguishes spin vs progress; watchdog
   regs/NV2A PUT-GET lines are a cheap first diagnostic.

## Solved earlier this session (4g, note 0011 core)

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

1. **H14-H18 CLOSED:** async-IO return convention (patch 0014) + three
   measured seed lifts (`0x11C640/1E330/273EEF`) + host fence mirror
   (`src/main.c`) + `RECOMP_PB_EXEC=1` + `RECOMP_VBLANK=1` take the boot
   from the phantom-charge stall all the way to real flips and the title
   intro movie. Decode still byte-exact.
2. **H19 inventory CLOSED (note 0023 frz16, patch 0029 +
   `tools/unh_names.py`):** the 150 distinct unhandled NV2A methods
   are ALL benign per-frame states (cull-face, BREAK_VERTEX_BUFFER_
   CACHE ×1-per-draw, clip min/max, light enables, window-clip,
   WAIT_FOR_IDLE, BEW semaphore release) — no draw-blocking method.
   The executor already draws the level scene (6139 VSH batches,
   33.6k tris/pass, 795M px) and the title screen now dumps FULLY
   TEXTURED (37k colors, logo+art+rain, dump frz1600027). Level
   frames just aren't PRESENTED during the game's load state (H28
   swap pause holds the last frame). Follow-up (not a wall): confirm
   texture coverage once a level frame is presented; the "0 textured
   batches with usable stage" line may hide a combiner gap.
3. **H20 (cheap, verify first):** confirm `0x48000004`-set boot tasks
   don't re-starve later subsystems; the ordinal profile with PB_EXEC is
   NtWaitForSingle+NtReleaseMutant ×19M/160s on a D3D worker — check it
   is the movie player, not a lock regression.
4. **H9 (FATX mount/geometry)** still queued, likely moot now (file
   opens past the boot archive all succeed).
5. **H21 CLOSED (patch 0022):** the audio init gap was the AC'97
   bus-master write trap never arming on Linux x86-64 (arm compiled for
   aarch64 only; the VEH fallback is a NULL stub). With the trap armed +
   `RECOMP_AC97_READY=1` the toolkit MCPX APU (SDL2, VP passthrough) comes
   up, all five DDS9 sub-inits return 0, and null-voice submits go to 0.
   The 24 KB copy onto the TIB page and the `sub_0028F8C3`-family poison
   chain are gone. `RECOMP_AC97_READY=1` now belongs to the working recipe.
6. **H22 CLOSED:** the `sub_000B08F0` wild crash was the same audio
   poison chain; it no longer occurs.
7. **H23 CLOSED (note 0018):** the "wild pointer" was stack drift, not
    poison — unresolved ICALL `0x27430` (real `ret 4` thiscall, func_id
    miss) hit the `ICALL_SAFE_AT_CC` skip (assumes caller-cleanup) →
    4-byte esp deficit → allocator epilogue pops misread → wild
    `esi`/`edi`. Fix: seed measured targets (feedback-dump pipeline).
    Lesson: audit every `[ICALL] Failed to resolve VA` line — the skip
    can corrupt the stack far from where it fires.
8. **H26 CLOSED (note 0021, gfault9):** D3D ring/present waits wake on
    GPU software-method NOPs — chain now runs end to end host-side.
    Final design (patches 0025 + 0026-v2): executor latches the aperture
    words at `NV097_NO_OPERATION` (param≠0) + 64-deep FIFO; ack pass
    holds PGRAPH bit0/PMC bit12 **only until the guest service chain
    reads them once** (`seen` set at the `sub_002DAE40` DPC /
    `sub_002DB250` pump wrappers); `done()` (guest + timer drains)
    clears+pops when seen, re-arms same param when unseen. Verified:
    `KeSetEvent(0x2E2F5C) from 0x002DB61F` (predicted class-5 arm) fires;
    ring event `0x2E2F4C` ×2421; `pop=1149 ack=2475 drop=0`. gfault8
    lesson: the guest's PGRAPH service loop spins until the word reads
    clear and RAM-stores its w1c back — the ack pass is the ONLY clearer;
    never hold past first service (wedges the timer thread).
9. **H27 CLOSED (note 0022):** the post-notify freeze was a HOST lock
   race, not game logic: frz4 core caught the main guest thread
   (`sub_002D57D0`→`bridge_KeWaitForSingleObject`→`ke_shadow_lookup`)
   blocked forever on an ORPHANED heap mutex while `g_ke_shadow_cs`
   read all-zero with init flag 1 — unguarded `ke_shadow_init()`
   double-init (memset + mutex swap; `EnterCriticalSection` lazy
   self-init worsens it; first-use race window = the stochasticity).
   Patch 0027 = `InitOnceExecuteOnce` guard. Confirmed twice:
   gfault10 (no plateau, scene animating) + gfault11
   (`plateau-lines=0`, 4.43 fps). Lesson for the toolkit's static
   CSes: any lazily-init'd `CRITICAL_SECTION` shares this hazard;
   audit if the symptom recurs elsewhere.
10. **H28 CLOSED (note 0023, frz10-18 censuses):** the post-attract
    "stall" is the game's own level-streaming load state, NOT a hang:
    frame loop + flip-issue alive, swap pump + engine submit frozen
    exactly at the flip count (their only caller `sub_0011ED20` is a
    device-message handler the load state stops invoking), IO
    streaming continues, recovery to ~1.8 fps gameplay. Host win:
    patch 0028 read-log gate (A/B: tracing not the load throttle).
11. **H29 ACTIVE (note 0023, frz20 core + frz25/26 censuses):**
    prologue-level plateau hang. Root anatomy: ring writer
    `sub_002D57D0` parks in `KeWaitForSingleObject(0x2E2F5C)`; that
    event's only arm is D3D's ONE class-5 NOP (`[NOPSW] 5:1` of 2891).
    Patch 0030 (class-5 re-latch on walk drain edges) CONFIRMED
    FIRING (`walk calls work=6098 drained=6098`, `rering latched=1`)
    but a second wake never reaches `KeSetEvent` — the NOP-emulation
    hop is unreliable (frz25 run2: `5:1` serviced ZERO times). The
    real D3D8LTCG ring-free protocol is the BEW semaphore:
    `sub_002D54E0` (writer's wait path) emits `0x4A310 marker +
    0x41D70 release(value=ctx+0x2C counter)` into the ring;
    `0x1D6C` programs offset 0 → target = channel block at
    aperture+0x800000 (`0xFD800000`, same block as `USER_DMA_GET
    +0x44` which the walk DOES advance correctly — earlier "stale
    GET" reading corrected); that block's +0/+4 words read ZERO at
    plateau = releases rejected by patch-0016's `off != 0` guard.
    Candidate fix (next): allow `off==0` → NV2A aperture target
    (0031); frz26 census (`[PRES] inst/i0/i4/latch/...`) verifying
    what the guest expects to read there.
    **4s UPDATE — GPU chapter CLOSED:** 0031 landed; frz33/34 proved
    the waiter spin (hand-built event) and the true terminal waiter
    (engine INFINITE on 0x2E2F4C); frz35-39 census proved a
    service-chain RE-SERVICE livelock (guest pump re-dispatching one
    mark 84,884x inside a single call while done() ran 100x rarer);
    0034 (trap-quartet idle on service) + 0035 (consume at the
    first-read `seen` edge, synchronous — the ack-tick variant v1
    was proven inert) killed it: seen_l 743k→6.7k==consume, drop=0,
    runs=1. frz40 PRES census: ring HEALTHY at the plateau
    (submit 24,493, iGET following the write cursor). New wall H30:
    the engine device-message layer — `sub_0011ED20` stops invoking
    the swap pump; frz41 plateau-core in flight.

## Queue (next concrete actions)

* **PRIORITY (4u->5a): frz89 verdict recorded (inconclusive, harness bug):**
  the plateau-watcher false-fired at t+2:19 during the fast-boot/FMV
  era (0.00-fps lines are NORMAL between flip bursts in decode/movie
  eras - 7 equal lines is not a plateau signature mid-run). Core
  exists (coredumpctl pid 1124423, SIGABRT 17:48:32, matched symbols)
  but it is a MOVIE-ERA snapshot, not a plateau. Dumps live at
  `reference/fb/89*.bmp` (nav_run prefix is `$RUN`, not `frz$RUN`) -
  all black/FMV era. HARNESS FIXES before the next nav run
  (frz90): (a) watcher arms only when elapsed >= 420 s AND flips >=
  1000 before evaluating the 0.00-fps criterion; (b) nav press gate =
  first `.bik` + 120 s (movies span several .bik opens - 45 s fired
  mid-FMV); (c) census greps + fbpeek must use prefix `89`/`90`, not
  `frz89`. Then the GAMEPLAY-artefact check per note 0023: late-dump
  fbpeek + visual confirm of an animated level scene.
* If nav failed again: the pad-feed model itself is suspect — add a
  `[PAD]` state census (XAPILIB state fetch 0x31052E/0x31053A wrappers)
  to see whether presses reach the game at all post-movie.
* A/B inventory for later: RECOMP_SWMARK_INTR12/PF/IRQPOST/PB_PARK all
  opt-in probes; RECOMP_ARM_UNTIL_SEEN=0 (v4) stays the working regime;
  queue depth 2048 + 0046 idle-clear default-on.
* Then: confirm level-scene texture coverage on the presented frame
  (the "0 textured batches" census line).
* xemu title/menu side-by-side capture (queued since 4m; note 0019
  boot fixed; xemu_ref.sh QMP greeting fixed 4t; FB-offset remeasure
  pending).

1. **xemu reference (note 0019 CLOSED for boot):** STOP was our unofficial
   256 KiB SpiritBoot dev ROM forcing 128 MB (Def Jam #75 failure class).
   Official `SpiritBoot-v1.1.0-64MB.bin` + xemu default 64 MB **boots the
   title** (user saw loading card + intro FMV). Next: single-instance
   retest of the mid-movie freeze (likely VRAM contention: strata holds
   ~23.9/24.5 GB); re-measure FB offset on the new dump (note 0009 recipe
   pinned old layout); then capture title/menu for the side-by-side.
2. **Input wall CLOSED (note 0020, session 4m):** the toolkit's OHCI +
   Xbox-pad model existed but `xbox_OhciInit()` was never called by our
   project; arming it in `src/main.c` (+ `RECOMP_USB=1`, now part of the
   recipe) made the title's XAPILIB USB stack enumerate the virtual pad
   fully; patch 0023 fixed the scripted-pad START bit to real hardware
   (0x0200). Evidence: movies skip on press, title→New Game (Buki card
   `reference/fb/skipframe00003.bmp`), attract demo loads
   (`badger.plap` + speech_buki) — and the zero-input baseline run never
   self-advances. Feeder lesson: press DENSELY across the whole
   post-boot window (a pad3 timing miss looked like a failure).
3. **Unhandled-method inventory (GPU wall):** enable the executor's
   unhandled table (already counting: 150 distinct) and dump
   method+count+last_param; cross-reference `nv2a_regs.h`. Identify the
   minimal set the intro movie/Bink blit needs (image blit vs triangle
   path).
4. Decide where method coverage lands: it is host NV2A work in the
   toolkit (`nv2a_pb_exec.c`) — a **new subsystem the brief didn't
   contemplate** → architectural gate (see Blockers). Do NOT hand-edit
   gen/; any fix is executor-side.
5. Keep the frame-dump recipe as the Menu-stage artefact once draws
   land: `RECOMP_PB_EXEC=1 RECOMP_VBLANK=1 RECOMP_FB_WINDOW=1
   RECOMP_FB_DUMP=reference/fb/frame`; side-by-side needs an xemu
   reference of the same intro moment (blocked on xemu geometry —
   Blockers).
6. Seeds file `logs/seeds-icall-0011C640.json` is gitignored (analysis
   artefact), now **18 measured VAs**; newest batch recorded in note 0018
   (`027430 04FCD0 20B710 22BA00 53740 91860 AFBF0 11AF30 A8A10 B5A50
   BC450`). Batch protocol: `cmake -B build-fb -DSUDEKI_ICALL_FEEDBACK=ON`
   probe run (SIGTERM flushes `build-fb/icall_targets.dump`) →
   `tools.recomp.icall_feedback merge` → `seeds --xbe` → union in.
7. Cleanup between runs: `rm -rf ~/.local/share/xboxrecomp/Cache/*`.
8. Enhancement roadmap (user doc `docs/chats/recompile-features.leo.md`,
   scope-banned; commit `ddaaa08`): **user directive 2026-11-22 —
   "vanilla first, then features"; Vulkan accepted as the END-STATE
   backend, staged:**
   - **Stage A (now):** vanilla works under the current GL back end +
     current process (nothing changes; gfault9 etc. continue).
   - **Stage B (after vanilla Gameplay):** swap the renderer to Vulkan
     by backporting upstream's `vulkan-backend` *branch* onto our
     v0.18.0 pin — NOT the stale v0.13.1 fork. Acceptance gate =
     parity: identical `RECOMP_FB_DUMP` artefacts + same gameplay under
     both back ends. Pin-change mechanics already gated (AGENTS); this
     directive pre-approves the direction, each build still gets its
     own pin/audit entry.
   - **Stage C (post-Stage-B, opt-in flags only):** texture replacement
     (dump hook exists: `RECOMP_TEX_DUMP`) → resolution scale (real
     work; `RECOMP_GL_SCALE` does NOT exist at our pin) → HDR →
     upscaling → RT. Menu/Gameplay artefacts stay disc-faithful
     (side-by-side rule); no enhancement code in any default path.
   Leo's chat has title-specific errors (calls it RenderWare; it is the
   custom SOL engine) — re-verify any claim from it before acting.

## Blockers / open questions

- **GPU draw coverage (new primary wall):** frames flip but black;
  150 distinct unhandled NV2A methods. Inventory first (queued). If
  coverage means writing a programmable-vertex/VSH executor into the
  toolkit, that is a large upstream feature — note it as a possible
  architectural-gate moment, but the inventory itself is pure analysis
  and proceeds autonomously.
- xemu reference: **RESOLVED (note 0019)** — boots on official
  SpiritBoot v1.1.0-64MB profile; the note-0009/0011 "xemu reproduces
  our wall" reading was an artifact of the 128 MB dev ROM. Outstanding:
  mid-movie freeze (retest single-instance) and FB capture offsets.

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
  `RECOMP_WATCH_LEN`/`RECOMP_WATCH_PANIC` (patches 0013/0018/0020),
  `RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_WATCHDOG_SECS`;
  **session-4h set that reaches flips:** `RECOMP_PB_EXEC=1
  RECOMP_VBLANK=1 RECOMP_FB_WINDOW=1 RECOMP_FB_DUMP=reference/fb/frame
  RECOMP_FB_WINDOW_DUMP_EVERY=60`; `RECOMP_POST_TRACE=1` (poster
  wrappers), `RECOMP_REFILL_TRACE=1` (refill wrapper), `RECOMP_TLS_TRACE=1`
  / `RECOMP_TLS_GUARD` (error-slot setter), `RECOMP_DS_COPY=1`
  (`sub_0028EA91` packet-copy census, `!` lines always logged),
  `RECOMP_DS_VOICE=1` (`sub_002854B3` voice-state census),
  `RECOMP_DS_INIT=1` (`sub_0028F8C3` DDS9 master-open census; the probe
  skips the init tail — diagnostics only), `RECOMP_LOG_ORDS=44,98,...`
  (patch 0021), **`RECOMP_AC97_READY=1` now part of the working recipe**
  (MCPX APU + AC97 bus-master trap; audio initialises), **`RECOMP_IO_READ_LOG=1`**
  (patch 0028; `[READ]` trace now default-OFF — re-arm for IO forensics).
  Seeds file for
  pipeline `--seed` lives at `logs/seeds-icall-0011C640.json` (local).
- Never end a turn on "standing by": while a build/run is in flight, do
  concrete analysis (prior logs, lifted bodies, next probe script); chain
  dependent steps into one background script. Idle waits banned by user
  directive 2026-11-22 ("make sure stopping doesn't happen again").
- OpenCode layer: `opencode.jsonc` + commands `/resume /status /run /cycle
  /note /lift /handoff /setup`; skills under `.opencode/skills/` (pinned).
- Remote origin push URL is `NO_PUSH`. Do not "fix" this *unprompted*.
  User directive 2026-11-22: a push to origin may happen only with the
  user's explicit formal approval in a live conversation ("wait for my
  formal approval before to push"); pre-push audits (disc-artefact
  scan of tree AND history, secrets scan, doctor pins) are mandatory
  each time. The push URL is switched only for the approved push.

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
| 2026-11-21 s4g | **THE phantom 0x8000 solved (note 0011): patch 0014 async-IO PENDING default = upstream-correct return convention on async handles → decode completes byte-exact with default env on disc-exact boot.baf (22 reads, 1.8 MB cache, HANG not CRASH)**; manual-override contract wired (`--exclude-manual` + `sub_00022340` wrapper + REFILL_TRACE); refill/init double-charge proven; patch 0015 event create/wait/set caller trace; SIGABRT-core thread-map recipe; post-decode wall reframed = 3-way event deadlock (worker pump {7,1}, worker B polls 4, main INFINITE on its own-consumed 2) | `7faa86f` |
| 2026-11-21 s4h | **Boot reaches the title intro + first GPU flips (note 0011 post-scripts 3-5):** post-decode "deadlock" was an unresolved task-completion ICALL `0x0011C640` (measured seed; siblings 1E330/273EEF same cause) → token 4 set → main proceeds; XGRAPHCL flip-pending `ctx+0x8700` solved by `RECOMP_PB_EXEC=1`+`RECOMP_VBLANK=1`+ host `xbox_Nv2aMirrorFence(0x2E3638,0x2C,0x30)` in `src/main.c`; opens `MS Game Studios Logo-final.bik`; 7 frame dumps (black) = first framebuffer artefacts; poster wrappers + RECOMP_POST_TRACE; new wall = NV2A draw-method coverage (150 distinct unhandled) | (this commit) |
| 2026-11-21 s4i | **level streaming reached + audio wall root-caused (notes 0016/0017)**: flip-done settle chain (patch 0017 + `main.c` chain `0x2E3638→+0x934→ctx+0x8700`) clears post-movie wait → `SOLWORLDM.gex`/wavebanks/`BS_brightwater.xwb` stream; 4 measured seeds more (`0550A0 0FD7D0 135AA0 092010`) lift remaining boot ICALL misses; the "TLS teardown poison" crash **falsified and root-caused**: DSOUND voice `sub_002854B3(this=029A00F4)` slot +0x14=0 → `sub_0028EA91` 24 KB wavebank copy to guest 0x818 tramples exception page + TIB page; TLS guard kept, audio census wrappers (`RECOMP_DS_COPY/DS_VOICE`, `!`=impossible-address auto-log); watch upgraded (guest-stack chain 0019, TF single-step + panic core 0020); surviving crash = `sub_000B08F0+0xDFB2` wild write `esi=0x89D8458B` (same audio subsystem, H22) | (this commit) |
| 2026-11-21 s4k | **H23 CLOSED (note 0018) + MENU artefact reached**: "wild allocator pointer" = 4-byte esp drift from an unresolved-ICALL skip (`0x27430`, real `ret 4` thiscall; `ICALL_SAFE_AT_CC` skip assumes caller-cleanup) corrupting saved-register pops — proven by esp-delta probes, fixed by seeding measured targets (18-seed file); runs now: no crash @240 s, 152k draws, first content frames incl. **title screen** `reference/fb/frame00016.bmp`; ICALL feedback instrument wired (CMake `SUDEKI_ICALL_FEEDBACK`, `build-fb` probe, SIGTERM dump) for batch seeding; xemu ref investigation: 0.8.139 = latest release, HDD pristine, upstream shows title running (no bugcheck reports) -> our STOP is environment-specific; user taking over xemu playability test manually | (this commit) |
| 2026-11-22 s4m | **input wall CLOSED (note 0020) + xemu boot fixed (note 0019)**: xemu playability = official SpiritBoot v1.1.0-64MB ROM (unofficial 128MB dev ROM was the STOP; user verified FMVs/audio/pad/rumble on xemu); recomp input = arm toolkit OHCI model (`xbox_OhciInit` in `main.c`, `RECOMP_USB=1`) + patch 0023 (script-pad START bit 0x0200 = real hardware); press-driven proofs (movie skip, Buki loading card, attract demo) vs zero-input baseline; XPP input-chain map (ISR 0x31428B, DPC 0x31495F, glue 0x31052E/0x31053A state fetch); JP disc (`Sennen no Akatsuki`, XBE `1f8a9611…`, same LTCG libs, parked outside git) as second regression sample; next wall = NV2A 3D draw coverage (demo scene freezes at 0 fps when VSH batches skipped) | (this commit) |
| 2026-11-22 s4n-4p | **freeze walls H26+H27 CLOSED** (notes 0021/0022; patches 0024-0027: sw-method latch+FIFO, hold-until-seen ack ownership, scripted-pad fix, `ke_shadow_init` `InitOnceExecuteOnce` — frz4 core proved the orphaned-mutex double-init); gfault10/11 clean (scene animating, 4.43 fps); gfault12 VSH-IO census = Bink passthrough programs, level scene rasterises 29k tris/pass; **H28 CLOSED (note 0023, frz10-15 censuses)** = post-attract stall is the game's level-streaming load state (pump+engine-submit frozen at flip count, IO alive, ~110 s, recovery to 1.8 fps), not a host hang; patches 0028 (`RECOMP_IO_READ_LOG` gate; A/B: tracing not the load throttle) + 0029 (`RECOMP_PB_UNH_TABLE` ranked unhandled-method table for H19 inventory); README co-authored rewrite + AGENTS rule 9 | (this commit) |
| 2026-11-22 s4q | H19 CLOSED (patch 0029 ranked `[UNH]` table + `tools/unh_names.py`: all 40 top unhandled methods benign; executor already rasterises level scene; frz16 dump 27 = fully textured title, 37k colors); frz18 nav recipe PROVED the Buki loading card presents + animates; frz19 proved the tail is a real hang (IO quiescent 150 s pre-plateau); `tools/fbpeek.py` frame forensics; README agent-contract + OpenCode sections drafted (user landed `a89c05a`); AGENTS rule 11 (user) | `8e7c8ed` |
| 2026-11-22 s4r | **H29 anatomy COMPLETE**: frz20 core = ring writer parked in `KeWaitForSingleObject(0x2E2F5C)`; class-5 NOP arm fires once per run; patch 0030 (drain-edge re-latch) iterated v1 (ack-poll, blind) -> v2/v3 (walk-tail, FIRES: work==drained, latched=1) yet no second wake; frz26 `[PRES]` ring census: plateau BIMODAL (parked writer vs pump-freeze with ring alive, `iGET==cursor`); BEW chain convicted (`0x1D6C` offset 0, releases rejected `off!=0`, channel-block semaphors never written, `sh8700=51747D8B` constant); fix candidate 0031; pre-push audit green (tree+history+secrets+pins) | `af91f07` (push pending user approval) |
| 2026-11-21 s4j | **audio wall CLOSED (patch 0022)**: DDS9 master open `sub_0028F8C3` needs AC97 bus-master trap; upstream armed it only on aarch64 (x86-64 Linux fell to NULL-VEH stub) → DSERR_NODRIVER → 'SdaD' create skipped → null-voice 24 KB copy = the whole note-0017 chain. Patch widens POSIX arm to x86-64; with `RECOMP_AC97_READY=1` MCPX APU (SDL2) + AC97 trap up, all 5 sub-inits 0, DS-VOICE!/DS-COPY! 0, wavebanks stream, ~1250 flips. New wall H23: deterministic allocator crash — free-list link `0xFE087E8D` (NV2A aperture value) spliced at real `0x1A9C1D` (`sub_001A9B60` family, misattributed `sub_001A9A50+0x1CD`); census wrappers `sub_0028F8C3/sub_002885AE/sub_0028AF42`, patch 0021 `RECOMP_LOG_ORDS` | (this commit) |
| 2026-11-22 s4u | frz87 deepest run (17,435 flips) = luck, content era is the rain-animated TITLE (fbpeek+visual); nav gate reframed state-driven (`.bik`-log gate, frz89 in flight); pump/completion chain anatomy from lifted C + **new Ghidra 12.2 second-lens project** (flat-image import recipe landed; javap-before-script lesson; frz83-core frames retracted as stale-symbol artifacts); frz88 mistake -> HEALTHY-era core baseline captured (+0x704=0x100 armed, PMC/PGRAPH 0); plateau-watcher for auto matched-symbol core | (this commit) |
| 2026-11-22 s4t | **mark-machinery regime matrix (frz62-83)**: queue-depth 0039 kills drop-overflow but not the plateau; strict one-owner (0040) weaker than v4 churn; walk-suspension (0042) and irq-post (0043) froze boot → opt-in probes; PMC-PF bit (0044) + regime-scoped pop-gate (0045, the 0040 gate had leaked: frz77 187-flip freeze); **frz82 bisect proved the frz66 7113-climb was run luck** (same binary froze at 231) — one stable wall: the guest's swap-mark DISPATCH stops (SWM 135 vs 231 flip-writes vs 150 slot-event sets) while the ring writer stays alive; frz79 core: events healthy, mark machinery idle, engine+credit-gate INFINITE on the next completion; next read = pump/dispatcher chain (sub_002DB250/4A0 + the 0x400700 busy flag + the 0x2DB0BE set condition) | (this commit) |
