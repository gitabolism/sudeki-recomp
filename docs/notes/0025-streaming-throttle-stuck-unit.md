# Note 0025 — level-load never completes: streaming throttle `cur/done` wedged, one unit re-read (H31)

Date: 2026-11-22. State: supersedes H30 (note 0024 CLOSED as cause of
the current wall); regime = frz92+ (v4 marks + `RECOMP_TITLE_KEVENTS`,
all three event mirrors live, ring writer healthy, engine ~10 fps).

## Observed (runs frz93/frz94; logs `logs/run-frz93-192142.out`,
`logs/run-frz94-193422.out`; cores `sudeki.frz93.core`,
`sudeki.frz94.core`; watcher v2 acc-aware, so these are TRUE plateaus)

* Navigation works: 8 STARTs (pad-enumeration-gated) -> New Game ->
  level load (speech banks `speech_hask*/shadow/TSA.xwb` stream,
  58-82 MB each, wavebank header tag `57 42 4E 44`).
* GATE census (`sub_002C3200` = streaming/frame throttle, note 0023
  anatomy): at plateau `tot` (+0x270) climbs +30/s and `acc` (+0x2B4)
  +28/s (gap ~130), but `cur` (+0x280) is FROZEN at 0x000E5D5C
  (941,404 — a byte offset), `done` (+0x284)=0 and `tick` (+0x27C)=0
  for the entire plateau; `bud` (+0x338)=`cons` (+0x33C)=0. Budget
  math `(tot-done)*1000*sc18/sc14` vs `bud/cons` keeps returning the
  SKIP decision -> render segment never runs -> presented frame stays
  the 99.6 %-black loading card for 600-900 s (frz94: 9,964
  presented window frames, all card).
* Everything else is ALIVE at kill (frz94 census timeline): ring
  writer (`[PRES]` to line 147,288), mark latches (`[SWM]` 147,532),
  completions `KeSetEvent 4C from 0x2DB0BE` (147,454), DPCs (150,832),
  engine gate calls (155.8 M), IO (`[READ]` 150,674). Watch on the
  4C object: quiescent (20 value transitions all run) - mirror
  coherence is NOT the issue.
* The IO tail is suspicious: repeated identical `want=116 got=116`
  reads returning ZERO bytes, same buffer 0x827AFC60, caller
  0x00271057 (seq-read site), thousands of times
  (`@seq` form lacked path/offset - the datum the wall needs).

## Hypothesis (one variable)

The loader's *current streaming unit* (byte window 941,404 in some
archive) never completes because its reads land on zero-filled slack
(or a mis-sized view) at our FATX/XDVFS layer, so the pump retries it
forever; `done` never advances and the throttle skips rendering
forever. Falsifier: if the seq-read path/offset (patch 0047) shows
the stuck reads hitting a *real data* window, the zero content is a
guest-side decode rejection, not our geometry -> pivot to the
consumer (which pump consumes the unit: `sub_002C0C50/C0980/C31A0`).

## Planned change (host-side only)

* **Patch 0047** (instrumentation, `patches/xboxrecomp/`): `[READ]`
  seq form gains `sz=` + `path=` (rpath/rsz already computed
  upstream). frz96 = same regime, 2500 s: names the stuck file and
  (with the `@` form) its offset; compare against the Redump ISO
  entry sizes (XDVFS table, gitignored) to see whether we are
  serving slack-as-data.
* Control frz95 (no presses, mirror regime): confirms the mirrors
  still reach title/attract normally (guards against a nav-free
  regression).

## Outcome

### frz96 full + frz97 write-forensics: serving layer exonerated, wall is guest-side

* frz96 (watcher-fired ABRT t=868 s): kill-state — engine alive
  (T1 inside submit chain `sub_002D5C80<-...<-sub_00207400`), nv2a
  PB exec active, XAPI workers idle-waiting (T14/T17 chains under
  `sub_00021B90/sub_00021AD0`), movie mid first lap (mdec 2276).
* frz97 (RECOMP_IO_WRITE_LOG + WRITE_DUMP): `[WRDUMP]` stream replay
  (`wreplay.py`) vs live Cache files: **startup.baf 27.7 MB 0
  mismatched bytes** (within dump-cap), boot.baf 0, Partition5/P0/
  badger/SOLXM 0. The 4 MB of zero runs inside Cache/startup.baf
  were written as zeros BY THE GAME (its own pre-allocated slack).
  Our write path is faithful; read path already proven exact
  (443/443 disc sizes + got=want).
* => H31 is purely guest consumer logic: the BAF config reader
  (through `sub_00270FBD`) stops asking at ~11.36 MB. Next
  measurement built: `RECOMP_RD_TRACE=1` manual wrapper on
  `sub_00270FBD` logging thisptr+args+EBP caller chain per read
  (frz98) -> name the consumer and the branch it stops taking.

### frz96 partial (patch 0047 live, ~9 min in, `logs/run-frz96-201101.out`) — the REAL load census

Phase IO census (path= per read, `io_phase_census.py`):
* PRE-LOAD: MS/ClimaxLogo biks, Cache/boot.baf, startup.baf,
  SOLWORLDM.gex — normal boot.
* LOAD-era: `Movies/FMA01_poem.bik` read **exactly 1.00x of its
  111,181,980 B** (whole opening FMV, once); every sfx/speech wave
  bank read 1.00x (complete); then IO collapses to attract-movie
  looping + intermittent `Cache//startup.baf`.
* `Cache//startup.baf` (27,762,688 B — the game's own X: install
  target, page-multiple 27112 KiB, created by the game at boot
  20:11:42, disc `startup.baf` is the 8,585,216 B source) is read
  **0.41x total (11.3 MB) with burst+33-KiB-strided-seek access,
  and goes fully silent mid-run** — that IS the load-progress
  stream, and it stalls at 41 %.
* Cache BAFs (boot/startup/shared) are all page-multiples and much
  larger than disc versions = game-written sparse containers (zero
  runs = its own slack, not our extraction: disc tree 443/443
  size-exact).
* Wavebanks unpacked to X: at 20:13, then quiet.
* No short reads on startup.baf (all got=want) — reads that happen
  all succeed; it is the CONSUMER side that stops requesting.

### CORRECTION (BINK anatomy, frz94 core): the GATE census never measured level loading

frz94 core + section map: ctx 0x82648040 is a **BINK struct** (Bink
player sections live at 0x002C0880-0x002CCFFB — so `sub_002C3200/
sub_002C0C50/sub_002C0980/sub_002CA6E0/sub_002C31A0` are all Bink
online-mode internals). Decoded fields (from core + bodies):
+0x270=frames decoded (grew +30/s = real-time decode; 2296 at kill =
exactly the movie's NumFrames; frz93's 3499 = second loop),
+0x2B4=frames presented (+28/s, lags ~130 = the online buffer),
+0x280/274=tick timestamps, +0x278/27C/28C=last tick (global
0x3969C8 matches), +0x028=file read pos (EOF-8 at kill),
+0x160/164/188=file size (EOF=0x1C4CD94), +0x14/18=fps 2997/100,
+0x020=496 KiB ring (slots matched 16 KiB at stride 0x4000).
IO-ring content-match (core): all 16 slots =
`Movies/Sudeki Atttract Mode.bik` @ 0x1BCE158-0x1C46158 (file size
29,674,900 = matches XDVFS table exactly; extraction tree verified
443/443 entries byte-size-exact vs disc).
=> "load stuck at 90 % with stream position frozen at 0xE5D5C" was
MOVIE pacing mislabeled: the attract movie (640x480, 76.6 s, 2296
frames) streams to EOF, finishes, and LOOPS under the loading card.
No load-progress census exists; H31 must be re-posed as: what event
does the game await after entering the load state? (frz96 path= per
phase is the decisive dataset: does non-movie IO persist?)

### frz95 (no-press control, 900 s, `logs/run-frz95-195504.out`) — control PASSED, wall sharpened

* Mirror regime reaches title/attract normally (no regression): rich
  attract frames to dump #85 (~30-37k colors, luma ~85), engine
  ~10 fps (`pass≈rend≈9.2k`), 16,077 reads, no crash lines.
* **The stall signature appears WITHOUT navigation**: at kill, the
  same GATE ctx shows `cur` frozen at 0x000D0194 (852,372),
  `done=0`, `tot`/`acc` still climbing; last 3 window dumps pure
  black (24 colors, luma 0) — same terminal shape as the nav runs.
  So H31 is era-general (title-era tail at ~t+600 s), not
  level-load-specific.
* `cur` semantics corrected: in the healthy era it WALKS a stream —
  0x000152FD -> 0x0006D7E2 -> 0x0009C5BD -> 0x000D0194 (monotone,
  with momentary 0 resets between samples) = a stream byte position.
  Stall = position stops advancing. `done=0` occurs in healthy eras
  too — census `done` label is NOT load progress (re-derive later).
* Watch on 4C: 7,694 writes, ZERO value transitions in the whole
  healthy title era, rich frames flowed anyway -> the 4C swap-slot
  event is not what feeds healthy rendering; the 20-pulse census in
  frz94 was incidental. (frz93's "parked on 4C" = terminal detail.)
