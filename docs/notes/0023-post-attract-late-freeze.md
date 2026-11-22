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
