# 0022 — ke_shadow critical section double-init = the post-notify freeze (H27)

## Observed (frz4 core, `logs/run-frz4-*`, SIGABRT at plateau 0 fps @1831 flips)

Core: `/tmp/opencode/sudeki.frz4.core` (binary `build/sudeki` from
`build-0026b`, NOT rebuilt after capture — symbols match).

Thread 1 — the guest main/ring-writer thread — is the freeze, host-side:

    #0 pthread_mutex_lock
    #1 EnterCriticalSection
    #2 ke_shadow_lookup                  (kernel_bridge.c)
    #3 bridge_KeWaitForSingleObject
    #4 kernel_thunk_dispatch
    #5 sub_002D57D0                      (D3D8LTCG ring-space waiter)

i.e. the waiter for event `0x2E2F5C` cannot even resolve its shadow
handle: it is stuck inside the KE-shadow table lock. The GPU-side
chain from note 0021 is proven healthy in the same core: event words
at `0x2E2F5C`/`0x2E2F4C` show normal Header/SignalState (the class-5
`KeSetEvent` census fired this run too). No submits from thread 1 →
no flips → 0.00 fps plateau. All other threads normal (raster
workers idle, ohci/timer sleeping, APU fine).

Corrupted-state facts (gdb on statics, no DWARF types → raw casts):

    g_ke_shadow_init == 1
    g_ke_shadow_cs   == all zero (LockSemaphore NULL, RecursionCount 0)
    stuck mutex (heap 0x5648f7b319a0): __lock=1, __count=1 — one
      unpaired Enter on a mutex no longer referenced by the CS.

So the CS was re-`InitializeCriticalSection`'d (memset first!) at
runtime while threads were using it; the old heap mutex was swapped
out and stranded with one unpaired lock; thread 1 read that old
pointer and blocked forever.

## Why the CS can be re-inited (code facts, upstream v0.18.0)

1. `ke_shadow_init()` gate is UNGUARDED:
   `if (g_ke_shadow_init) return;` — two first-callers can both pass
   it; each runs `InitializeCriticalSection(&g_ke_shadow_cs)` =
   `memset(cs,0)` + fresh malloc mutex.
2. `EnterCriticalSection` (win32_compat) lazily self-inits:
   `if (!cs->LockSemaphore) InitializeCriticalSection(cs);` — a CS
   caught mid-memset invites a THIRD init and leaves waiters on the
   abandoned mutex.
3. `cs->RecursionCount++/--` are plain increments racing mutex swaps.

Consequence classes: waiter blocks forever on an orphaned mutex
(frzh4), or Enter/Leave pair across a swap leaves an unpaired lock.
This matches the H27 run-to-run randomness exactly (gfault6/9 froze,
gfault5/7/frz3 ran fine at the same wall clock): it is a first-use
race window, not deterministic game logic.

Note: this does NOT invalidate note 0021 — the sw-method chain fix
was proven by its own census (`KeSetEvent 0x2E2F5C from 0x2DB61F`);
the two walls are stacked: 0021 fixed the GPU→CPU notify, 0022 is a
host lock race on the waiter side.

## Fix (patch 0027, host-side toolkit)

Replace the unguarded gate with the compat layer's
`InitOnceExecuteOnce` (callback runs at most once; late callers block
until it completes — verified implementation, win32_compat.c:181+):

    static INIT_ONCE g_ke_shadow_once = INIT_ONCE_STATIC_INIT;
    ke_shadow_init() -> InitOnceExecuteOnce(&g_ke_shadow_once, cb);
    cb: InitializeCriticalSection + memset table + init flag.

After the once completes, `LockSemaphore` is never NULL again, so the
lazy self-init path in EnterCriticalSection can never fire for this
CS. Known sharp edge left alone for now (separate variable): the lazy
path still exists for OTHER static CSes; audit if a similar symptom
appears elsewhere.

## Outcome (gfault10, patch 0027)

Same freeze recipe (dense presses from t=100 s): **no plateau at all**
— 1.83–1.84 fps steady to the 250 s end (3534 flips; old wall plateaued
@1797–2518). Frame dumps `reference/fb/g10000xx.bmp` advance in
content (296 → 11 367 → 34 839 → 36 531 colours, rising luminance)
and consecutive frames differ ~3 % (ImageMagick `compare -metric AE`)
— the attract/level scene is genuinely animating. Latch census healthy:
`direct=4624 enq=2875 pop=2875 ack=3887 drop=37` (drop = mark bursts
past the 64-deep FIFO; revisit `SWM_PENDING_N` only if a class-5 mark
is ever among them). Stochastic wall → confirmation runs required:
gfault10 clean + gfault11 (in flight) before H27 is declared closed.
The frz4 core stands as the falsification-proof: same binary minus
patch 0027 deadlocks in `ke_shadow_lookup`; with it, the waiter
returns and the game moves.
