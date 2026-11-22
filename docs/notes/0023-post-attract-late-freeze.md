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

frz17 follow-up (sparse-nav probe, single starts at t=112/132/152):
navigation FAILED by timing, and the failure mode is informative.
Dump cadence 15 flipped: dumps 1-4 = movies (43k colors), dump 5+ =
BLACK, never a title frame — because by t=112 the title had already
auto-entered the ATTRACT demo (badger.plap + speech_Elco/buki/woluf
opens at log lines 2928-2958; `fma01_poem.bik` at 4564) and presses
landed inside attract/transition states, wandering. Surface census
across the run: VSH rasterisation CONTINUES through every 0-fps
window (tris 15.4k→27.6k→33.1k while the screen sits black) — the
level/attract scene is rendered off-screen, only PRESENTATION pauses
(H28 mechanism, now proven in both directions: renders yes, swaps
no). Frz18 recipe: press DURING the FMVs (note 0020's proven skip
path: five starts t=20-44), title→menu→New Game→Buki at t=65/80/95,
then total input silence; PRES/GATE census re-armed to read pump-vs-
render state in black windows; 430 s to outlast the load.

frz18 RESULT — navigation SUCCEEDED: five FMV-skip starts (t=14.8-
38.9 s), then title→menu→New Game→Buki starts (t=59.9/74.9/89.8) →
**the Buki loading card was PRESENTED and animating** (frz18 dumps
16-17: full character art + "Loading..." + tip text, 35.7k colors,
luma 84, d=93.8% between — `/tmp/opencode/frz1800017.bmp.png`).
`[SWAP]` census behaves exactly per H28: flip-issue keeps ticking
30/s while pump/eng freeze at the presented count, slot table idle.
But the prologue load then sat 0-fps for the last ~190 s of the
window (kill@430) — too slow OR stalled; undecided. Side audit: the
`[FILE] D:\data\Startup\ -> 0xC0000034` refusal is FAITHFUL — the
disc's `Data/` holds only SOLWORLDM.gex + SOLXM.fubi, no Startup
dir, so real hardware answers NOT_FOUND too. frz19 = identical nav
+ `RECOMP_IO_READ_LOG=1` + 560 s window: reads streaming into the
tail = slow load (extend window); quiescent = real hang → core
forensics at plateau (existing recipe).

frz19 RESULT — the tail is a REAL HANG (new wall **H29**), not a long
load: IO goes fully quiescent ~150 s BEFORE the terminal plateau
(last `[READ]` at line 18141; plateau starts 19114, runs 38+ periods
to kill@560 s), and unlike the H28 load state the renderer is FROZEN
too (`rasterised` count constant 515898 through the whole plateau) —
the engine is blocked, not streaming. Only the D3D ring heartbeat
survives (`KeSetEvent 0x2E2F4C from 0x2DB0BE` keeps firing = the
NOP-mark pump is alive). Prime suspect surfaced by frz16's own census:
`BACK_END_WRITE_SEMAPHORE_RELEASE` (0x1D70) ×5903 counted UNHANDLED —
the executor's 0x1D70 handlers (registered `s_sem_va` + patch-0016
offset fallback) REJECT when `s_reg[0x1D6C]` is 0/junk/out-of-RAM and
the release falls through; if any guest thread waits on one of those
semaphore words it parks forever. frz20 = same nav with plateau
detector (≥150 s of 0.00 fps) → SIGABRT core for per-thread guest PCs;
`RECOMP_SEM_TRACE=1` will name accepted/rejected release targets next.

frz20 RESULT — **H29 root-caused** (core at plateau, pid 581534):
thread 1 (main guest thread) backtrace = `xbe_entry_point →
sub_0026D47C → bridge_PsCreateSystemThreadEx → sub_0026C68F →
sub_0011560 → sub_00122C0 → sub_001E0390(engine submit) →
sub_002D57D0(ring writer) → kernel_thunk_dispatch →
bridge_KeWaitForSingleObject → ke_shadow_lookup` — parked waiting on
event token `0x2E2F5C` (live reg r12 = 0x2E2F5C at the frame). The
`[NOPSW]` census convicts: seen classes `1:2877 5:1 8:1 9:12` — the
class-5 (ring-free) software-method mark, whose guest arm at
`sub_002DB61F` does `KeSetEvent(0x2E2F5C)`, was serviced EXACTLY ONCE
in the whole run; after flip 2877 no class-5 wake ever fires again
(`KeSetEvent token=0x2E2F5C` last line 22080, plateau starts 22224).
D3D8LTCG programs the ring-free NOP once; on hardware every later
pushbuffer drain re-raises the ring-free source and the standing arm
re-signals the event. Our executor only marks NOPs the guest pushed,
so after the ring writer's first wait the chain can never restart:
writer waits → engine submits freeze (`eng=pump=flip-count` exactly) →
renderer+IO quiescent (the frz19 tail signature). Fix (patch 0030):
store the last class-5 NOP param; on each busy→idle `DMA_GET==DMA_PUT`
edge in the NV2A ack thread, re-latch it through the same sw-method
chain. Verifying now (frz23: frz18 nav + full census, expect the
plateau to clear and the FIRST PRESENTED level frame after the Buki
loading card).

frz23 v1-check RESULT — patch 0030 v1 (ack-thread poll edge) did NOT
fire: census still `5:1`, plateau unchanged at flip 2340. Reason: the
ack thread samples `DMA_GET==DMA_PUT` once per tick, but drains finish
inside one tick window — the poll never OBSERVES the busy→idle edge.
v2 moves the hook to the DMA walk itself: `nv2a_pb_scan` records
`get_started`, and when its walk loop ends with `s_get==put` after
having had work, it calls `nv2a_pb_rering_free_drained()` → same latch
chain. `memory_layout.c` reverted to its pre-patch state (one variable).
(label slip: the verify script inherited the frz18 log/dump prefix and
re-wrote `reference/fb/frz18*` dumps; the card evidence this note
cites survives in `/tmp/opencode/frz1800017.bmp.png` and the recorded
color/diff stats. Renamed from frz24 on.)

frz24 v2-check RESULT — census blind spot found: `[NOPSW] classes`
counts NOPs IN THE GUEST STREAM (`nv2a_pb_exec_method`, line ~4548),
not latch calls — so re-latches are invisible there and `5:1` did NOT
prove v2 unfired. Queue counters hint it fired only a handful of times
(frz24 `enq=1179 drop=12` vs frz20 `enq=1175 drop=0`), and
`KeSetEvent 0x2E2F5C` stayed at 1. Structural fact: `nv2a_pb_scan` is
called ONLY when DMA_PUT changes (memory_layout poll tick, `put !=
last_put`); during the plateau PUT is frozen, so no walk — the drain
edge must have (or not) happened DURING the load. frz25 adds decisive
census: `[RERING] drain-edge #n` lines + `[NOPSW] rering calls/latched`
+ `[PB] walk calls: work=/drained=` counters.

Corroboration from the surviving frz18-050934 log (the card-presented
run): `KeSetEvent 0x2E2F5C` appears ZERO times there — its writer never
waited on ring-free, and navigation worked. In frz20/23/24 the class-5
arm appears exactly once = the ring actually filled at least once
during load/attract, writer parked, and the one-shot event left it
parked when the drain woke it once and it filled again. Plateau timing
is therefore stochastic (flip 2103/2340/2877/3261) and can strike at
title idle without any navigation (frz24 froze at the title frame —
fbpeek: dumps 16-23 live title w/ rain, 24+ frozen, PNG verified).
The frz20 core evidence (writer parked on 5C) + frz18 (never parked,
no 5C) + census (`5:1`) together: the missing hardware behavior is a
per-drain ring-free raise, exactly what patch 0030 models. Secondary
suspect kept under audit via RECOMP_SEM_TRACE: D3D's per-kick `0x1D70`
BEW releases (frz16: 5903 rejected by the page-align guard at
nv2a_pb_exec.c ~4447, `!(off & 0xFFF)`); if D3D programs a
non-page-aligned semaphore word (object page 0x2E2xxx prime suspect),
the ring-progress copy never lands either. frz25 carries SEM_TRACE for
that verdict.

frz25 RESULT (census binary; two runs, note: they overlapped on the
shared Cache dir for ~4 min — counters self-consistent, but keep the
caveat). PATCH 0030 MECHANISM CONFIRMED FIRING: `walk calls:
work=6098 drained=6098` (every walk drains fully) and `rering
calls=6098 latched=1`: drain edges flowed all run, and exactly one
re-latch (the first after the guest's single class-5 NOP entered the
stream) reached the latch chain — `drop=0`, and it still produced NO
second `KeSetEvent 0x2E2F5C`. After the last drain the ring is idle
(GET==PUT), the writer parks anyway, PUT freezes, no further edges —
edge-on-drain cannot wake a writer that parks on an already-drained
ring. [SEM] verdict (run 2): `SET_SEMAPHORE_OFFSET -> 0x00000000` and
`release REJECTED off=0 value=5,7,9,B,D,F,11,13` — the per-kick ring
counters (frz16: values reached 0x2E21, 5903 releases) target
instance+0, rejected. Core forensics (frz20 core, symbol base
0x55fe0da13000, g_xbox_mem_offset=0x7f9226800000):
ctx=*0x2E3638=0x002E1190; **the ring writer's GET feed is
MEM32(MEM32(ctx+0x1C20)+0x44) = MEM32(0xFD800044) = 0x001BA4D0** —
the channel INSTANCE block, a different window from the USER page
(ctx+0x934=0xFD000000) the executor advances (USER+0x40/0x44 read 0).
At the freeze instance-GET(phys)=0x1BA4D0 == walk s_get — so D3D's
GET was current at plateaU; its park decision therefore keys on the
BEW counters/`ebp+4` alloc math or the 0x400B10 kick latch, not on
GET staleness. Next census (frz26): [PRES] line extended in
src/recomp_manual.c to print inst/iGET/user/uGET/latch/lo/hi/w2C per
second during the plateau — names the stuck quantity directly.

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

frz26 RESULT (ring-writer census `[PRES] inst/i0/i4/latch/uGET/w2C`):
the plateau is BIMODAL. frz20 variant = writer parked on `5C` (core-
proven). frz26 variant = writer ALIVE: `submit=4434` climbing,
`iGET == w cursor` exactly (the walk consumes everything the writer
emits), latch advancing — yet flips frozen (1555) with `[SWAP]
issue=3967` alive, `pump=eng=1450` frozen, `wait=5` pending, and `5C`
wake count ZERO: the H28 device-message-pump signature, not the
parked writer. Constants across ALL plateau runs: `sh8700=51747D8B`
(the swap-completion slot holds the same poison word in every core
and every run; the `0x1D70`-shaped release never landed), `i0=i4=0`
(channel-block semaphore words never written — all releases rejected
`off=0`), and `iGET` never lags the cursor. Patch 0030 fired
faithfully (walk work==drained, one re-latch, drop=0) but the class-5
NOP emulation is not how D3D8LTCG asks for ring space — the BEW
release IS the mechanism. Next fix (0031): `off==0` → write through
the NV2A aperture channel block (`xbox_Nv2aRegPtr(0x800000+off)`),
same block whose `+0x44` GET the walk already publishes. frz27
expectation: `i0` tracks the odd counter; flips resume past the load
state; `sh8700` stops holding poison.
