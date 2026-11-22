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
