# Note 0023 — post-attract "late freeze" (gfault12)

Date: 2026-11-22. Prior: notes 0021/0022 (both freeze classes closed),
STATE.md "Last run" session 4n.

## Observed

* gfault12 (`logs/run-gfault12-*.out`; dense presses ×20 from t=100 s,
  FB dumps every 60 flips, 320 s): long run (4280 flips), opens
  `D:\movies\fma01_poem.bik` + `D:\movies\Sudeki Atttract Mode.bik`,
  then Presents stop: `0.00 fps (4280 flips)`; last two frame dumps
  BYTE-IDENTICAL (`compare -metric AE = 0`), one-before-last differs
  33 % (scene was moving until the stall). Plumbing alive at stall:
  vblank ticks 13 200 @45 Hz, sw-method census healthy
  (`direct=5313 enq=3067 pop=3067 ack=4643 drop=34`), rasterizer did
  real work (29 290 triangles/pass; VSH-IO census = screen-space
  640×480 passthrough programs — the Bink video plane). Game parked;
  GPU plumbing not.
* Config-sensitivity: gfault11 (NO FB dumps, 16 presses, 250 s) never
  froze (4.43 fps climbing); gfault12 (WITH dumps, 20 presses, 320 s)
  froze ~t≈300 s. Either the dump I/O perturbs timing, or the wall is
  simply past 250 s. Unresolved; capture both configs.
* frz5 capture probe (`logs/run-frz5-*.out`) capture-protocol lessons
  (its core is an artifact, see below):
  1. `0.00 fps ×3` is not a plateau test — cold cache (`Cache/*`
     cleared) gives ≥30 s load silences early. Gate detector on
     `flips>=2500` AND presses-started.
  2. The toolkit CATCHES SIGABRT (crash handler prints, re-raises);
     writing the 2.4 GB core takes seconds, so `kill -0` right after
     sees the process alive → my second ABRT corrupted state; the
     thread-1-in-allocator sample (`sub_0011B020`, cursor+list words
     all zero, single sample) proved nothing. One ABRT, then wait.
  3. Always keep a hard `timeout` around the run even with a
     detector, else a no-plateau capture hangs the harness.

## Hypothesis (falsifiers named; one variable = the core)

At the late plateau the main guest thread will show either
1. a wait on an object only hardware/D3D signals (H26 family — e.g.
   an NOP class or ring-complete edge the executor still doesn't
   emit; falsified if thread 1 is not in a wait), or
2. a game input-mode wait (attract-end expects a specific button —
   not a bug; falsified by thread 1 inside a kernel wait instead of
   the input pump), or
3. a Bink movie pump waiting on the APU/audio clock (falsified if the
   movie path is idle in the thread map while thread 1 waits
   elsewhere).

## Evidence in flight / status

* frz6 (`logs/run-frz6-*`, same config, correct detector but gate
  `flips>=2500` too high): **froze at 2280 flips** — detector missed
  it (tail shows 0.00 ×3 at 2280). So the late freeze IS reproducible
  in the dumps config, at variable flip counts (2280…4280), t≈250-300s.
* frz6 class census at freeze: `1:2280 5:1 8:1 9:12` — the class-5
  mark (ring-space event `0x2E2F5C`) was emitted in the freezing run,
  i.e. the note-0021 chain fired and the game still parked →
  hypothesis 1 (missing notify edge) weakened; hypothesis 2/3 alive.
* frz7 in flight: same config + `RECOMP_EVENT_LOG=1`, time-gated
  detector (t>=150 s, plateau x3), single ABRT, hard TERM fallback at
  250 s, `coredumpctl` → `sudeki.frz7.core`.

## Outcome (frz7 core — capture protocol worked)

frz7 (`logs/run-frz7-*.out`, `RECOMP_EVENT_LOG=1`, time-gated detector,
ABRT at t=150 s plateau, core `/tmp/opencode/sudeki.frz7.core`, thread
map `/tmp/opencode/frz7b.bt`): froze at **1444 flips**, during/right
after opening `D:\movies\fma01_poem.bik` (second attract movie
`Sudeki Atttract Mode.bik` never opened this run).

Thread map (27 threads):

* **Thread 1 (main guest) BUSY, not waiting** — frames
  `sub_001A99A0_gen → sub_00043290 → sub_00252AF0 → sub_002538D0 →
  sub_00254150 → sub_0026C240 → sub_0026C330 → sub_0011B020 (thread
  wrapper) → sub_001DE310 → sub_002C27A0 → sub_002C94F0 → sub_002C7840
  → sub_002CB420` (lifted body `recomp_0053.c:13865`, original
  0x2CB420-0x2CB74B, 240 insns). Guest regs: RDX=0x0031B0F4 (image
  data VA), RBX=0x00F7F9EA / RCX=0x00F7F750 (stack), RAX=RSI=RDI=0.
* Thread 16: guest worker in `NtWaitForSingleObjectEx`
  (`sub_0026D664/sub_0026D77A → sub_00021B90 → sub_0026D3E4`).
* Thread 6: guest worker in `NtWaitForMultipleObjectsEx`
  (`sub_0026D6A8/sub_0026D78C → sub_001EABD0 → sub_00021AD0`).
* Timer/OHCI/APU/nv2a-ack/raster-pool threads all healthy-idle.

`[EVENT]` tail at stall: `0x48000001` (from 0x0026D5BA) and ring event
`0x2E2F4C` (from 0x002DB0BE) STILL being set → not an event deadlock;
hypothesis H1 ("missing notify edge") effectively dead for this wall.

Open question: spin vs long-haul inside `sub_002CB420`. A single sample
cannot discriminate. Next steps (queued in STATE): (a) second run, ABRT
at plateau+~30 s, compare PC offset within sub_002CB420 + guest regs
(same offset & unchanged words = spin); (b) read the lifted body for
data-dependent loops (first grep: `loc_002CB4A9` branch structure seen,
body not yet fully read).

frz6 lesson: flip-gated detector (≥2500) missed the freeze (froze at
2280 flips). Time-gate (t≥150 s + 3×0.00) is the working detector.

## Structural anatomy (session 5, while A/B cores in flight)

frz7 core quantified (gdb, `frz_regs.gdb`): host PC =
**sub_002CB420 + 3945**; guest `RDX=0x31B0F4` = scale table @**0x31ADE0**
+0x314 (int32 words /2048 fixed-point, all sane positive); the object at
`esp+0x10` = caller stack scratch (int16 deltas −2..+1) + advanced
counters `0x713E/0x7252/0x7387`. `sub_002CB420` is **straight-line**
(no loop): 12 int16 fields @this+0x10..0x70 × table, sar 11 — a
6-corner fixed-point transform. It has **no direct `call` anywhere in
.text** — invoked through a function-pointer slot (lift resolves it at
the site `PUSH32(esp,0x002C8118)` inside the `sub_002C7840` body).

Frame-loop anatomy (lifted):

* `sub_0011B020` = XAPI-thread **frame loop** (flag check, quit-tag
  scan for entry tag 0x74, `sub_001DE310`, `sub_00258420`,
  `sub_0003F1E0` continue-test, loop).
* `sub_001DE310` = per-frame sequence: **gate `sub_002C3200(ctx)` at
  0x1DE32D — non-zero return skips the whole render/Present segment**
  (`goto 0x1DE40C`); else `sub_002C27A0` (render prep) → `sub_002D5BF0`
  → `sub_002D9D90` → `sub_002C1ED0` → `sub_002D3B50` (640×480 present
  swap, double-buffer toggle @0x3970C0) → … → `sub_002D3CC0`
  (flip-done wait). Globals: ctx ptr @0x3970C8, buffer idx @0x3970C0,
  buffer table @0x3970B8.
* `sub_002C3200` (149 insn) = streaming/frame throttle on the scene
  ctx: pumps `sub_002C0C50`/`sub_002C0980` (queue @ctx+0x2F8/0x300),
  tick source `sub_002CA6E0`, progress `sub_002C31A0`; ms-budget math
  ((`ctx+0x270`−`ctx+0x284`)×1000×`ctx+0x18`/`ctx+0x14` vs
  `ctx+0x338/0x33C`); `return 1` (skip) via 0x2C33A4 (also
  `sub_002C6000`/ICALL `sub_002CA490`@ctx+0x120/`sub_0026D644`),
  `return 0` via 0x2C3372/0x2C339A/0x2C3382.

frz8b (second capture, plateau detector at t=150 s but ABRT at t=180 s):
**the game had self-recovered before the ABRT** — 11 consecutive
`0.00 fps` at 1828 flips (≈65-95 s stall), then `2.95 fps (2044 flips)`
= normal ~3 fps. Core at recovery shows main thread in the D3D submit
path (`sub_002D5C80` family) — a different call site than frz7 (mid
render prep). So the wall is a 65-170 s stretch, sometimes terminal
(gfault12), sometimes transient; GPU chain is **idle during the stall**
(NOPSW `direct=` frozen 2547→2547; jumps +158 with `drop=32` only at
recovery) → no Presents reach the ring → consistent with main thread
never completing a frame-prep pass, not with a missing notify edge.

NOPSW lesson for detectors: during the stall the *last* `[GPU]` line is
usually a `draw #` flood — grep ` fps (` explicitly; the 0.00×N
trigger is valid (fps lines keep printing from the scan pass).

Planned census (RECOMP_GATE_TRACE=1 manual wrapper on `sub_002C3200`):
per-second line = return value + ctx slots +0xFC/+0x2A0/+0x2F8/+0x270/
+0x280/+0x284/+0x27C/+0x2B4/+0x338/+0x33C/+0x14/+0x18 — settles
gate-skip vs never-completing-prep, and names the stalled drain path.

frz10 census RESULT (RECOMP_GATE_TRACE, 260 s, killed mid stall #2):
**the gate-skip reading of H28 is falsified.** During 0-fps stalls the
frame loop keeps ticking: `pass` grows exactly **+30/s**, render
(`sub_002C27A0`) and present (`sub_002D5BF0`) wrappers enter every
pass (pres=rend+17 constant = the movie/UI call sites), gate pumped
~365k calls/s. Yet GPU counters are **bit-frozen for 100+ s**:
`draws 6800 | clears 1805 | 73066 unhandled` identical across ALL
stall-window status lines; recovery coincides with a render-target
switch (surface 0x00828000→0x030A4000) and draws→20431 with
world-scale coords (level scene starts). So during the stall the game
**presents but nothing reaches the pushbuffer**. Reframed wall =
between `sub_002D5BF0` entry and ring writes: the lift of its D3D
submit chain shows silent skip paths — `sub_002D57D0` returns if
alloc `sub_002D55E0`=0 or free-space<0x2400 (loc_002D58D3), and
`sub_002D5BF0` itself early-exits unless `[0x2E3638]`≠0 &&
`(*eax&0x70000)==0x50000` && `[eax+0x14]`≠0 && …(test 0x780000).
Also: gate `ctx` printed all-zero fields — wrapper masked eax with
0x0FFFFFFF; frz9B stack shows aperture-aliased pointers 0x82648040/
0x82648150. Wrapper fix: print raw.

Next (frz11, RECOMP_PRES_TRACE=1): count `sub_002D5BF0` entries vs
`sub_002D57D0` (D3D submit-chain) vs `sub_002D5C80` (leaf submit)
vs `sub_002D55E0` alloc==0; print `[0x2E3638]` + arg-object flag
words per second. Decides "Present never reaches submit" (alloc/
free-space gate shut → host ring-accounting suspect) vs "submits
with empty draw list" (game-side streaming state).

Chain coverage pre-audit (static, before frz11):
* `sub_002D5760(eax)`: eax≠0 spins on `(PGRAPH_0x400B10 ^ (cursor<<2))
  & 0x7C` — the kick-fence latch, already modelled in
  `nv2a_pb_exec.c` (method `0x0310:5` latches it; mirror-fence case
  patched into the same site). eax==0 (D57D0's first 2 calls) skips
  the spin and returns ring cursors via `[esi]`/`[block+4]`.
  D57D0's 3rd call passes eax=1 → CAN spin — see [PRES] prog= values.
* `sub_002D5C80` leaf: writes `[base+0x100410]`, spins while bit
  0x10000 set — toolkit ack exists (`xbox_memory_layout.c`
  `NV2A_ACK_PFB_FLUSH`, `{0x100410,0x10000}` row; same class as
  Halo's 0x1EF930). Leaf cannot be the hang.
* Free-space math in D57D0 uses only D3D-side words
  (`[ctx]`,`[ctx+0x30]`,`[ctx+0x44]`,`[ctx+0x1C20]` DMA-ring base
  via D5760's `([edx+0x44]|0x80000000)`) + D55E0 alloc; DMA_GET
  publication is upstream's walk (nv2a_pb_scan.c). So frz11's
  `[PRES]` numbers pin the stuck word exactly.
* `loc_002D5911` ICALL target word `[0x3157E4]` = kernel thunk slot
  (slot 73 of 119 sorted-ordinal table = `NtQueryVirtualMemory`,
  5 args (region,6,1,0,0) — page-info probe after submit; frz10
  log has 0 "Failed to resolve" lines, so it resolves.
* frz10 also confirms gate ctx raw value should print unmasked
  (frz9B stack had aperture aliases 0x82648040/150) — wrapper fixed.

frz11 census RESULT (try/submit/alloc/prog per second, 2779-flip kill
mid stall #2 @1837 flips, 11×0.00 fps ≈110 s):
**the free-space gate inside `sub_002D57D0` is NOT the wall — the D3D
Present never reaches it during the stall.** [PRES] (D57D0 entries)
ticks every ~88/s healthy but goes near-silent in the whole stall
window: try only +167 over ~110 s (≈1.5/s) while frz10 showed the
Present API `sub_002D5BF0` entered 30/s → Present short-circuits in
its own flag checks. Ring words: blk=80065000 constant, blk0 (cursor)
keeps creeping +30/s even in the stall (allocations happen for the
pump path); `[ctx+0x44]` (size)=0 through the movies, 0x2FF850 after
recovery; alloc fail never observed (0 failures all run).

Full short-circuit table (lift of `sub_002D5BF0`, dev=`eax` arg):
esi=[0x2E3638]≠0; then submit `D57D0(dev,2)` iff
  ((`*dev & 0x70000`)==0x50000 && [`dev+0x14`]≠0 &&
   (`*dev & 0x780000` || `*[dev+0x14] & 0x780000`))
  OR (`*dev & 0x780000`) on the alt entry — else falls to loc_002D5C6E:
  `dev[8]`≠0 → `D57D0(dev[8],0x10)`; else RETURN with no submit.
So during the stall: state field `*dev & 0x70000` ≠ 0x50000, or
`[dev+0x14]`=0, or the 0x780000 effect bits missing, and dev[8]=0.
`0x70000`=a 3-bit device-state field (ready value 5);
`0x780000`=5-bit swap-effect-ish field. H28c: that state word stays
stuck (or its feeder never advances) exactly during the streaming-
pump phase.

Next (frz12, RECOMP_PRES_TRACE): `[PRESN]` line per second (and on
change) from the `sub_002D5BF0` wrapper: dev, `*dev`, dev[8],
dev[0x14], `*[dev+0x14]` — healthy vs stall values name the stuck
state field; then trace its writer.

frz12 RESULT — **H28c falsified: the Present short-circuit is
structural, not stall-specific.** `[PRESN]` during the stall is
identical to the healthy attract phase (dev=029998F0 flags=01050001
dev8=0 dev14=014B3BC4 d14f=00040002 in BOTH windows): `*dev&0x780000`
=0 (bit24 set, not the effect field), `*dev14&0x70000`=4≠5,
dev8=0 → this D5BF0 entry NEVER submits, healthy or stalled. So the
D5BF0 site we count (+30/s) is a permanently-no-op Present path (a
secondary device/chain object), NOT the game's real per-frame
present. Different dev objects appear over boot (010236D0, 00FD8800,
028ECE40, 02999920) — several Present entry points.
Therefore the 88/s healthy `D57D0` traffic (≈20 per flip) comes from
OTHER call sites: `sub_002D57D0` has 20+ callers across the game
(frame sequence sites ~0x1DE2F8/0x1E0xxx, UI 0x1Fxxxx-0x21Exxx, plus
D5BF0's own 0x2D5C6C/0x2D5C7A). Reframed question: WHICH caller
sites go quiet during the stall.

Next (frz13): attribute `D57D0` entries by guest return VA
(`MEM32(g_esp)` at wrapper entry) into a small table; per-second line
prints the 3 biggest per-second deltas → names the exact call site
that dies at stall start and the one(s) left alive.

frz13 RESULT — the frame-loop sites die wholly, not partially.
Healthy attract: exactly two dominant `D57D0` callers, paired at
30-70/s: `002D3640` (inside `sub_002D35A0` = swap-COMPLETION pump:
iterates `[[ctx+0x1974] + [ctx+0x2478]*4]` slot table, calls
`D57D0(slot,·)` while slot≠0 and bumps the idx — callers of
`D35A0` are XAPI-thread loop region 0x11EDCC/11EF3E/11EF4D + engine
0x1E0687) and `001E03D8` (inside engine submit `sub_001E0390`, 538
B). Stall window (1439 flips, ~110 s): [PRES] silent, try +15 total
(all from `001F8D41` ~UI) → BOTH sites emit ZERO. Meanwhile frz10
proves the frame pass still runs: gate pass+30/s, render
`sub_002C27A0` +30/s, Present-API `sub_002D5BF0` +30/s. And the
Present-API path itself NEVER submits (frz12: structurally
short-circuited) — the real per-frame submit chain is the engine
sequence `sub_001E0390` + swap pump `sub_002D35A0`, both silent.
Game flip-issue `sub_002D3B50` (buffer toggle 0x3970C0, table
0x3970B8, sets `[0xFD008700]=1` pending) is called straight-line
from the frame sequence at ret `0x1DE3E9` — its stall behavior is
the unknown. So the wall = swap state machine: either the frame
sequence skips the flip-issue segment via a branch, or the pump
runs but the slot table is empty.

Next (frz14, RECOMP_PRES_TRACE): swap-machine census — per-second
`[SWAP]` line printed from the `sub_002D5BF0` wrapper (ticks 30/s
even in stall) with counters for flip-issue `sub_002D3B50`,
flip-wait `sub_002D3CC0`, swap-pump `sub_002D35A0`, engine-submit
`sub_001E0390`, plus pump inputs `idx=[ctx+0x2478]`,
`slot=[[ctx+0x1974]+idx*4]`, `[ctx+0x8D4]`, `[ctx+0x8D8]`. Decides
flip-issue-silent (frame-seq branch) vs pump-sees-empty (queue never
written) vs pump-not-called (XAPI loop starved).

frz14 RESULT — **H28 CLOSED: not a hang, not a host gap — the
game's own level-streaming load state.** Anatomy of the "stall"
(1500 flips, ~75-110 s in frz13/frz14):
* Swap-machine: flip-issue `sub_002D3B50` keeps ticking 30/s (the
  frame sequence runs and queues flips — it's gated by
  `sub_002C1F00(...,0x8000000D)` returning 0, which it does), but
  the swap-COMPLETION pump `sub_002D35A0` and engine submit
  `sub_001E0390` are frozen at EXACTLY the flip count (1500=1500=
  1500): one pump pass per completed frame, zero new passes.
* Pump callers are all inside `sub_0011ED20` (ret sites 0x11EDCC/
  11EF3E/11EF4D), itself called from game-logic handlers
  (0x1EC000-0x1F2xx family). During the load state the game logic
  simply stops invoking the device-message handler — deliberate
  guest behavior, NOT a lost notify (flip-wait `sub_002D3CC0`
  count barely moves: it is never even reached).
* Streaming is alive throughout: 5517 `[READ]` ops (~90 MB, 16 KiB
  chunks via 0x271057) inside the stall window; recovery then
  flips resume and hold steady 1.83 fps gameplay (frz14) / 1.8-
  4.4 fps (frz13). Long-haul, not spin; the terminal variants
  (gfault12) = load longer than the observation window.
* Screen frozen on the last flipped frame because swap completions
  only happen via the pump the game itself paused — expected
  behavior for this engine's load state; real hardware just loads
  ~16x faster at that point (matches our ~1.8/30 fps scale).

Host-side follow-up (one legit inefficiency found): the `[READ]`
trace (patches 0008/0010) was UNCONDITIONAL incl. per-read
`stat()` + path resolution + `fflush` — pure overhead during the
exact load phases we time. Patch 0028 gates it behind
`RECOMP_IO_READ_LOG=1` (default OFF now). A/B (frz15, same recipe,
no read-log): if the 0-fps load window shrinks materially vs
frz14's ~75-110 s, IO-tracing cost was a significant throttle;
either way H28 stays closed — record the measured load duration.

H19 inventory tooling (queued PRIORITY, prep while frz15 runs):
patch 0029 = ranked `[UNH] m=NNNN n=count last=param` table (top 40
per report) in `nv2a_pb_exec_report`, env `RECOMP_PB_UNH_TABLE=1`.
frz16 will carry it to classify the 150 distinct unhandled methods
against `nv2a_regs.h` (method numbers only; names resolved offline)
— the implement-in-order list for the VSH/3D coverage work.

frz16 RESULT — **H19 inventory phase CLOSED: the draw-coverage gap is
largely phantom.** Ranked table (tool `tools/unh_names.py`, names vs
nxdk `nv_regs.h`): top unhandled methods are all benign per-frame
render states — `SET_CULL_FACE_ENABLE` ×15019, `SET_CULL_FACE` ×8444,
`BREAK_VERTEX_BUFFER_CACHE` ×6424 (= once per draw, cache invalidation
no-op), `BACK_END_WRITE_SEMAPHORE_RELEASE` ×5903, `SET_CLIP_MIN/MAX`
×5201, lighting/specular enables ×4306, window-clip trio ×3720,
`WAIT_FOR_IDLE` ×1890, point/light/polygon-offset/TEXGEN one-offs.
NO draw-blocking method in the list; and the same report proves the
executor DRAWS the level scene: 6139 VSH batches / 67252 verts,
33626 triangles rasterised per gameplay pass, 795 M pixels written.
Frame-dump anatomy (31 dumps @60 flips, BMP→PNG converter + file read
per rule 5): dump 27 (~flip 1620, during the 1616 stall) is the
**fully textured title screen** — SUDEKI logo, character art, "Press
START", © line, rain — 37025 distinct colors, near-zero black. So the
menu paint path (textures+combiner) already works far beyond the old
219-color `frame00016`; the level scene rasterises but is never
PRESENTED during the load state (swap pump paused by the game itself,
H28) — the screen holds the last flipped frame.
Remaining visible-gap candidates: (i) texture coverage in the LEVEL
scene (report line "0 textured, 0 with no texcoords, 0 with texcoords
but no usable stage" — may be a combiner/shader-texture gap or an
artifact of which batches count; confirm once a level frame is
actually presented); (ii) MENU NAVIGATION is the real Gameplay gate:
the dense start-feed (every 8 s) distorted the state machine —
repeated START during a load plausibly cancels to title, explaining
frz16's second 110 s load and the title frozen after recovery (all
dumps 17-30 are the SAME title frame = swap paused, nothing new
presented). Next probe (frz17): sparse scripted navigation — single
start at the title, then `a` presses, dumps every 15 flips, no start
spam.

(Name source: nxdk `lib/pbkit/nv_regs.h` fetched to /tmp/opencode for
offline resolution; `tools/unh_names.py` parses our own headers.)

frz15 A/B RESULT (patch 0028, read-log OFF): log volume collapsed
from 80% of lines to zero `[READ]` lines, but the load stall window
is UNCHANGED (~11 report periods ≈ 110 s, vs frz14's ~11 ≈ 110 s).
So the IO-trace cost was NOT the load throttle — the load is bound
by emulated-CPU decode/streaming (~16× scale, same as the 1.8/30
fps gameplay ratio). Load duration = performance surface, feeding
Stage B (Vulkan) / general perf work, not a correctness wall.
H28 stays CLOSED. Patch 0028 kept for log hygiene + opt-in forensics.

frz10 census (RECOMP_GATE_TRACE=1, 260 s, KILL mid stall #2 @2342 flips)
— **the gate reading is inverted**:

* During the 0-fps stall, gate `pass`/`rend`/`pres` advance **+30/s each**
  (full 30 Hz frame ticks through the render `sub_002C27A0` and Present
  `sub_002D5BF0` segments) while the streaming pump hammers the gate
  ~368k calls/s (99.98 % skip). The frame loop cycles; it is NOT gated.
* The GPU status block is **fully submission-quiet** during the stall:
  `draws 6800 | clears 1805 | unhandled 73066` byte-identical across all
  stall-window status lines; recovery bursts the backlog
  (`draws 20431 → 47711` at 3 fps). Matches frz8b NOPSW `direct=` frozen.
* Post-recovery: **zero** gate calls (pump drains) — the stall window is
  exactly the pump-spinning phase.
* The ctx slot offsets printed (static reading, note's earlier anatomy)
  all read 0 — wrong object/offsets; the counters alone carried the signal.

Present chain anatomy (lifting, D3D section): `sub_002D5BF0` = game
Present call (ctx=`[0x2E3638]`, device flag `0x780000` swap-effect gate,
`sub_002CA490` vsync-check ICALL) → `sub_002D57D0(dev=[ctx+0x2C],2)` =
D3D-runtime exchange-end: `sub_002D55E0` block alloc (0 → silent ret),
progress via `sub_002D5760(0)` ring-wrapped against `[ctx]`/`[ctx+0x44]`
(ring size), **free-space < 0x2400 or signed-negative → skips the method
writes and the submit leaf `sub_002D5C80` via loc_002D58D3** — the skip
path still fires the `[ctx+0x19FC]` callback with `flags|0x80000000` and
stores the cursor to `[ctx+0x1A00]`; the good path writes method headers
(`0x40110/0x40100`, `[edi+0x10..0x1C]`), submits via `sub_002D5C80` ×2,
then `loc_002D5911` ICALLs `[0x3157E4]` (kernel import thunk slot) with
(dev+0x1DCC,6,1,0,0).

Hypothesis H28b: during the pump phase the runtime free-space gate sticks
shut (a ring progress/consumed quantity the host must advance), so every
Present submits nothing → no clear/flip → 0 fps, while the frame loop and
pump look alive. Next probe (RECOMP_PRES_TRACE=1 wrappers): per-second
counts of `sub_002D57D0` attempts vs `sub_002D5C80` submits,
`sub_002D55E0` alloc ok/fail with eax, `sub_002D5760` return values, plus
`[ctx]`,`[ctx+0x2C]`,`[ctx+0x30]`,`[ctx+0x30][0]`,`[ctx+0x44]`,`[ctx+0x19FC]`
— names the stuck quantity if the submit leaf goes quiet.
