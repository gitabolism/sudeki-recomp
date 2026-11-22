# 0011 — the phantom 0x8000 was the sync-read return convention; async PENDING fixes it (patch 0014)

Follows note 0010 post-script 3 (the padded-size diagnostic that proved the
game's read accounting starves exactly one 0x8000 segment short). Per the
queue: find which kernel-visible answer produces the phantom charge, host-side.

## Observation (guest refill trace, first run of the new manual-wrapper machinery)

`src/recomp_manual.c` now hand-defines `sub_00022340` (the boot-archive ring
refill) via the toolkit's `--exclude-manual` contract: the generated body is
emitted as `sub_00022340_gen` and every caller links against our wrapper,
which prints the object's accounting slots (`RECOMP_REFILL_TRACE=1`). The
pipeline (`scripts/pipeline.sh`) now passes `--exclude-manual
../src/recomp_manual.c`; stub count unchanged (432).

Trace on the disc-exact file (`logs/run-20261121-190004.log`):

- refill #1: `bud 0xA5800 -> 0x9D800` (charged, matched by a read ✓)
- refill #2: **pre `bud=0x95800`** — an extra −0x8000 between the calls, with
  no refill in between. The only other writer of the budget slot is the
  init routine `sub_000221C0` at 0x22215 (`budget -= [+0x3C]`).

So the phantom = **double charge of the prefetch segment**: the refill
charged it (its post-read path at 0x223D1) AND init charged it again.

## Mechanism (from disasm + trace, cross-checked against the wrapper code)

The io struct the refill passes to XAPI `0x270FBD` is really its own
`+0x3C/+0x40` pair as the *out* slot; the `IO_STATUS_BLOCK` the kernel
writes is the **parent stream object** (`+0 Status, +4 Information,
+8 byte position` — hence `add [parent+8], count` advances the file
position, matching the `[READ] @offsets`).

XAPI `0x270FBD` (lifted, verified by capstone) branches on the kernel's
*return value*: `0` = final status, `0x103` = PENDING. Under the bridge's
synchronous `STATUS_SUCCESS`:

- refill's read completes inline → refill takes 0x223D1 and charges budget;
- init then runs its completion block unconditionally at 0x22209-0x22223
  (`budget -= [+0x3C]`) — written for the async world where the refill
  itself never charged.

On a real kernel the handle is **asynchronous** — `bridge_NtCreateFile`
already classifies it (open options `0x4C`, `options & 0x30 == 0` ⇒ no
FILE_SYNCHRONOUS_IO bit ⇒ `bridge_note_async_handle`) — and a real kernel
returns `STATUS_PENDING (0x103)` for such reads even when the data is in
hand. Then refill #1 exits via the flag-2 path **without charging**, init
waits/completes and charges once, and steady state is one charge per read.

`RECOMP_ASYNC_IO=1` (upstream switch: `bridge_handle_is_async && → g_eax =
STATUS_PENDING`) reproduces exactly that: trace shows refill #1 `flg=04→06,
no charge`, init charges once (0xA5800→0x9D800), then clean 1:1 −0x8000
steps, **44 refill calls total** (vs 9,948 in the starved run).

## Result (default env after patch 0014, disc-exact `game/boot.baf` sha `b6e5751c…`)

`logs/run-20261121-190438.log`:

- 22 reads covering the whole real file, final `@657408 want=22528` —
  END token region reached on the *real* file size (no padding games);
- `Cache/boot.baf` = **1,820,672 B, `cmp` byte-exact vs golden**;
- cache stays 1.8 MB (no flood), **no crash**, no decoder overrun;
- classification: **HANG (post-decode idle)** — the archive pipeline
  completes; the title still waits on unsignalled events
  `0x48000001/04/06` (only `0x48000002` ever set).

## Fix

Patch 0014 `patches/xboxrecomp/0014-async-io-default.patch`: flip
`bridge_async_io_enabled()` to ON by default (`RECOMP_ASYNC_IO=0` opts out
to the old behaviour). This is a kernel-answer fix (rule: fix host-side),
faithful to the hardware contract; nothing game-side, nothing disc-derived.

## What this falsifies / confirms

- Confirms H15: the stream is the whole file and hardware's answers make
  reads reach it; no file-size slack is needed at all — the padded-file
  result of note 0010 was a coincidence of compensating errors.
- Kills the "phantom refill charge in game code" reading: the double-charge
  structure is real game code, but it is only reached under a *wrong*
  kernel answer (sync SUCCESS on an async handle).
- xemu's identical 0x9E000 stop (note 0009) now reads as xemu answering
  the same convention wrongly — consistent, not corroborating.

## Next (the post-decode wall)

Events `0x48000001/04/06`: who creates them, who should signal them.
Event-creation trace (numbered 0015 to avoid colliding with 0014): log
`NtCreateEvent`/`NtOpenEvent` with token + caller VA.

## Tools/mechanisms added

- `--exclude-manual src/recomp_manual.c` in `scripts/pipeline.sh`;
  `sub_00022340` wrapper + `RECOMP_REFILL_TRACE=1` in `src/recomp_manual.c`
  (the first wrapped manual override in this repo — generated tree renamed
  the body to `sub_00022340_gen`, stub count unchanged at 432).
- Patch 0014 (async IO default-on), patch 0015 (`from=`/`esp=` on
  `NtCreateEvent`/waits/`[EVENT]`, wait log caps 20→300). Series: 15
  patches, deterministic reset+reapply in `scripts/build.sh`.

## Post-script — anatomy of the post-decode stall (SIGABRT-core recipe)

The decode-complete HANG's thread map (`kill -ABRT` at t=32 s →
`coredumpctl` → `thread apply all bt`):

- **Thread 3** boot worker #1: `sub_00021AD0` (subsystem-list walker,
  CS `0x320D08`) → `sub_001EABD0` (pump, created events `0x37A118`
  init=0 and `0x37A114` init=1 at its own stack) → blocked in
  `XWaitForMultipleObjects {0x37A118, esi}` INFINITE, `esi` = the
  subsystem-block event (`0x397920`) = token `0x48000001`.
- **Thread 4** boot worker #2: `sub_00021B90` (CS `0x320CF8`) → INFINITE
  wait on `0x397924` = token `0x48000004`, never set; the "polling"
  look (3557 calls, finite label) is this loop re-arming.
- **Thread 1** main: `sub_0026D47C → sub_0026C68F → sub_00011560 →
  sub_00012090 → sub_00011DE0 → XWait` — INFINITE wait on token
  `0x48000002` (second wait; the event was already consumed).

Event protocol (5 creates, all via XAPI `0x26D584`; patch 0015 now prints
creator/waiter/setter sites + esp):

| token | created into | identity | who should set |
|---|---|---|---|
| `48000001` | main stack `0x00F7FF40` | subsystem-1 event (`0x397920`) | task producer `sub_00021CD0` @0x21D7F (`XSetEvent(0x397920)` after list insert) — never fired |
| `48000002` | main stack `0x00F7FF40` (same slot, later create) | main's own rendezvous | main itself set it once (`esp=0x00F7FB0C`) — auto-reset, one set consumed, second wait blocks |
| `48000004` | main stack `0x00F7FD64` | subsystem-2 event (`0x397924`) | task producer `sub_000220E0` @0x22147 — never fired |
| `48000007/08` | worker stack | pump's A (`0x37A118`) / pre-set B (`0x37A114`) | gate cluster `sub_001EAAB0/sub_001EAB90` set-paths — never fired |

So the pump loop (`sub_001EABD0` @0x1EAC60: `do { sub_001F0F80() } while !=0`)
ran the boot-archive task to completion, returned 0 (queue empty), and
blocks waiting for the NEXT task; the task-posters (`sub_00021CD0`/
`sub_000220E0`, which set events 1/4) are never called because main is
itself stuck in the `0x48000002` rendezvous inside `sub_00011DE0`. The
next wall is therefore main-thread's `sub_00011560/0x12090/0x11DE0`
chain: what completes that wait — the set came from main's own earlier
code (esp `0x00F7FB0C`), so look for the second producer on that path
and whether an auto-reset/manual-reset nuance or a lost second set is
ours or the game's.

Recipe worth keeping: SIGABRT-core + `thread apply all bt` + frame
`sub_XXXX` names gives the full guest thread map in one shot.

## Post-script 2 — create/wait census (patch 0015, run-20261121-191748)

All five creates come from XAPI `0x0026D584` and are logged with
handle_ptr/type/init; waits carry `from=` + `esp=`. Steady state after
decode: 70 finite waits on `0x48000004` (worker B poll loop — ordinal
277 ×3557), 2 on `0x48000002`, 1 on `0x48000001`, one multi-wait
`{0x48000007, 0x48000001}` INFINITE (worker A pump idle). Only set ever:
`0x48000002` from `0x0026D5BA esp=0x00F7FB0C` (main's own rendezvous).
Decode artefact re-verified byte-exact in this run.

Note: **every create logged `type=0` (Notification)** — XAPI
`XCreateEvent` maps to Notification; if the game's rendezvous expects
semaphore/automatic semantics for the *second* wait on `0x48000002`
(set-once → consumed → second wait blocks), the lost-set hypothesis
needs checking against hardware XAPI behaviour (does the game wait the
same event twice by design, expecting two sets from two producers?).
Both `0x48000001` and `0x48000002` were created into the *same* main
stack slot `0x00F7FF40` (sequential creates, different runs? no — same
run: slot reused after the first event's handle went out of use).

## Post-script 3 — THE post-decode wall is an unresolved ICALL (measured)

Poster wrappers (`sub_00021CD0`/`sub_000220E0`, `RECOMP_POST_TRACE=1`)
+ full event/wait census (`logs/run-20261121-192410.log`) settle it:

- The "3-way deadlock" was misread: token slots hold `0x397920`=`48000001`,
  `0x397924`=`48000002` (posters' events); `0x48000004` lives in the boot
  object at `[0x3DC81C+0xA4]` (main's 500 ms poll) and is polled by
  nobody else. Worker B (`sub_00021B90`) is the **q2 list consumer**:
  INFINITE wait on `0x397924`, drain list `0x396E98`, per task
  `ICALL [task+4]` (execute) then `ICALL [[task]](1)` (completion).
- Task `0x00FD8600` (flags `0x8001` → bit15 routes poster1 to poster2's
  q2 list; poster1's queue-1/event-1 path is a *different* subsystem)
  was posted, poster2 set token 2, worker B woke, **executed the task**
  (boot.baf opened + fully decoded + `Z:\badger.plap` opened — all on
  worker B), then `ICALL` to **`0x0011C640` FAILED to resolve** and was
  silently skipped. That call is the task-completion notify:
  `lock xadd` refcount decrement + store `[arg1+0x28]=1|3` + tail
  `jmp 0x11D200` — i.e. exactly the code that would mark the task done
  and let the boot manager signal `0x48000004`, which main polls.
- `0x11C5E6..0x11C67F` is a gap of three **real** entries missed by
  func_id (int3-padded, identical bodies, tail-jump `0x11D200` confuses
  it): `0x11C5F0`, `0x11C610`, `0x11C640`. Measured address from run log
  → seeded via `--seed-functions` (policy: measured only; seeded just
  `0x0011C640`; siblings enter only if they too show up measured).

Hypothesis H18: lifting `0x0011C640` makes the completion notify run →
task marked complete → `0x48000004` set → main's poll returns and boot
proceeds (next stage: `badger.plap` — the game data archive).

## Post-script 4 — H18 CONFIRMED; new wall = D3D fence mirror (host-side)

Regenerated with the measured seed `0x0011C640` (+ `0x001E330`,
`0x00273EEF` — all three were func_id misses, all lifted, zero unresolved
ICALLs now): `logs/run-20261121-193224.log`.

- Task completion notify ran → **`0x48000004` SET** (first time ever);
  main moved past its poll, opened `Z:\badger.plap`, read the baf TOC,
  `FNT/FMH/PSH/SHX` chunks (fonts/shaders), allocated the 26 MB VRAM
  arena at `0x81000000`, `KeConnectInterrupt vector 3` — the deepest
  boot yet. Workers idle normally (pump {B,1}, worker B token-2 wait).
- Steady state after ~1 s: main spins with **zero kernel calls** inside
  `sub_002D57D0` (statically-linked D3D8LTCG, `game_render` category).
  Loop `0x002D5900`: `dev=[0x002E3638]; while (PUT(dev+0x2C) -
  *(dev+0x30) … outstanding > need` — D3D waits for the GPU to advance
  its "consumed" counter word (address in `dev+0x30`, resolved to
  `0x80065000` in the contiguous window — watchdog regs `edx=80065000`,
  word reads 0). NV2A USER ring PUT==GET (executor consumed all); the
  counter word is what never gets written.
- Fix = the toolkit's per-title fence mirror hook (documented feature,
  host-side, no guest patch): `xbox_Nv2aMirrorFence(0x002E3638, 0x2C,
  0x30)` — poll thread copies "submitted" into the word the GPU would
  acknowledge. Offsets measured from our own lift (this D3D build),
  registration in `src/main.c` after `xbox_MemoryLayoutInit`.
  H19: main proceeds into first device/swap-chain init after this.

## Post-script 5 — first flips reached (RECOMP_PB_EXEC + vblank + mirror)

With the mirror compiled in (`src/main.c`, registered at startup):
`sub_002D57D0`'s drain wait passes; main proceeds into the movie path,
opens `D:\movies\MS Game Studios Logo-final.bik`, and spins in
`sub_002D3CC0` on `[ctx+0x8700]==0` — the XGRAPHCL flip-pending flag
(`ctx = *(*(0x002E3638)+0x934)`); nothing in guest code ever writes it
0: the GPU does, via the pushbuffer's semaphore-release methods.

Two runtime switches turn out to be load-bearing (both off by default
in v0.18.0):

1. `RECOMP_VBLANK=1` — raises vblanks to the guest's connected ISR
   (`KeConnectInterrupt: vector 3 -> routine 0x002DAD20` — XGRAPHCL's;
   NV2A_VECTOR=3 matches). Log shows `vblank -> ISR claimed it`.
2. `RECOMP_PB_EXEC=1` — **enables the pushbuffer method executor**
   (`nv2a_pb_exec`); without it the ring is only walked (GET advances,
   methods ignored), so BACK_END_WRITE_SEMAPHORE_RELEASE (`0x1D70`)
   and every draw method are inert → no flip ever happens.

With both: **first frames ever captured** — 7 dumps over 75 s
(`reference/fb/frame0000X.bmp`, gitignored), 640x480, surface
`0x00828000` pitch 2560, 234 clears — and the game is flipping at
several Hz. Frames are **solid black**: the executor counted 8716
unhandled methods (150 distinct) — the NV2A programmable path this
XDK D3D build uses (VSH programs/constants, indexed vertex buffers,
image-blit classes) is outside the toolkit's FFP model, so geometry
never lands. Next wall = GPU method coverage for the draws (Menu
stage); inventory the unhandled set first (executor keeps a table —
`nv2a_pb_exec_report`).

Note also: with PB_EXEC the ordinal profile changed — NtWaitForSingle
+ NtReleaseMutant ×19M in 160 s (D3D thread churn) — normal busy game,
not a lock stall. `nv2a_pb_set_semaphore_target` remains uncalled in
the runtime (tests only) — with PB_EXEC on, `0x1D70` still won't write
unless a target is registered; flips worked anyway, so this title's
flip completion flows through the DMA_GET/mirror path. If a later wait
turns out to need the explicit semaphore, register `ctx+0x8700` (chain
`*(0x002E3638)+0x934+0x8700`) — host-side one-liner.
