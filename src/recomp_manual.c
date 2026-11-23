/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>   /* ptrdiff_t */

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state ────────────────────────────────────────── */
/* Declared (thread-local, matching the runtime's definitions) by the
 * generated runtime header — do not re-declare here without RECOMP_TLS. */
#include "gen/recomp_types.h"

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
/* ── Boot-archive stream refill wrapper (note 0010, RECOMP_REFILL_TRACE=1) ──
 *
 * sub_00022340 is the ring refill of the boot-archive reader. The wall is
 * one phantom 0x8000 budget charge (20 charges vs 19 chunk reads); the
 * flag-2 branch at +0x22345 (consume path, charge at +0x2236F) is the
 * suspect. The toolkit's --exclude-manual scan sees the `extern ... _gen`
 * line below, emits the lifted body as sub_00022340_gen, and links every
 * caller against this hand-written wrapper — the original stays reachable,
 * nothing in gen/ is edited by hand. Prints the object's accounting slots
 * around every call, so a phantom charge is a (pre,post) delta with no
 * [READ] line beside it. Slots (crash-core semantics):
 *   +0x3C last transfer count, +0x40 budget, +0x44 flags byte,
 *   +0x24/+0x28 ring read/write bufs, +0x10/+0x14 active segment.
 */
#include <stdlib.h>   /* getenv (recomp_types.h above declares the state) */

extern void sub_00022340_gen(void);

static int refill_trace_enabled(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *e = getenv("RECOMP_REFILL_TRACE");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached;
}

extern void sub_0026C79D_gen(void);

void sub_0026C79D(void)
{
    /* note 0017 probe (RECOMP_TLS_TRACE=1): the XDK error-slot setter.
     * It computes the slot as [[fs:4] + tls_index*4] + 8 with the image's
     * tls_index (0x376620) negative; a corrupt fs:[4] on some thread made
     * the deref fault at 0xFFFFFF6C. Log the whole chain per call. */
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RECOMP_TLS_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    {
        extern __thread uint32_t g_fs_base;
        uint32_t fsb = g_fs_base;
        uint32_t arr = MEM32(fsb + 4);
        uint32_t idx = MEM32(0x376620);
        uint32_t slot = arr + idx * 4u;
        /* note 0017 probe: after a thread's TLS teardown the array word
         * holds poison (0 or 0xC0000000); the SetLastError-class store has
         * nothing valid to land in, and the dereference faults the whole
         * process. Skip the store, keep the run alive to the next gap.
         * RECOMP_TLS_GUARD=0 disables; RECOMP_TLS_TRACE=1 logs every call. */
        static int guard = -1, trace = -1;
        if (guard < 0) {
            const char *e = getenv("RECOMP_TLS_GUARD");
            guard = (e && *e && *e == '0') ? 0 : 1;
        }
        if (trace < 0) {
            const char *e = getenv("RECOMP_TLS_TRACE");
            trace = (e && *e && *e != '0') ? 1 : 0;
        }
        if (trace) {
            fprintf(stderr, "[TLSSET] fs=%08X arr=%08X idx=%08X slot=%08X arg=%08X\n",
                    fsb, arr, idx, slot, MEM32(g_esp + 4));
            fflush(stderr);
        }
        if (guard && (!arr || arr >= 0xC0000000u)) {
            static int warned;
            if (!warned++)
                fprintf(stderr, "[TLSSET] torn-down TLS (fs=%08X arr=%08X) — "
                                "store skipped (note 0017; further ones silent)\n",
                        fsb, arr);
            return;
        }
    }
    sub_0026C79D_gen();
}

void sub_00022340(void)
{
    if (!refill_trace_enabled()) {
        sub_00022340_gen();
        return;
    }
    uint32_t obj = MEM32(g_esp + 4);   /* arg; caller's PUSH32 moved g_esp */
    fprintf(stderr, "[REFILL] obj=%08X pre  bud=%08X cnt=%08X flg=%02X "
                    "rbuf=%08X wbuf=%08X seg=%08X..%08X io=%08X..%08X\n",
            obj, MEM32(obj + 0x40), MEM32(obj + 0x3C), MEM8(obj + 0x44),
            MEM32(obj + 0x24), MEM32(obj + 0x28), MEM32(obj + 0x10), MEM32(obj + 0x14),
            MEM32(obj + 0x60), MEM32(obj + 0x7C));
    fflush(stderr);
    sub_00022340_gen();
    fprintf(stderr, "[REFILL] obj=%08X post bud=%08X cnt=%08X flg=%02X "
                    "rbuf=%08X wbuf=%08X seg=%08X..%08X eax=%08X io=%08X..%08X\n",
            obj, MEM32(obj + 0x40), MEM32(obj + 0x3C), MEM8(obj + 0x44),
            MEM32(obj + 0x24), MEM32(obj + 0x28), MEM32(obj + 0x10), MEM32(obj + 0x14),
            g_eax, MEM32(obj + 0x60), MEM32(obj + 0x7C));
    fflush(stderr);
}

/* ── Boot task-poster wrappers (note 0011, RECOMP_POST_TRACE=1) ──────────
 *
 * Post-decode deadlock (note 0011 post-scripts): worker pump waits
 * INFINITE {0x48000007, 0x48000001}, main/worker B poll 0x48000004 in
 * finite loops. The only code that sets tokens 1 and 4 is sub_00021CD0
 * (XSetEvent 0x397920 @0x21D7F, and it tail-calls sub_000220E0 @0x21D92
 * which sets 0x397924 = token 4). If these never run, the game's task
 * flow starved upstream of the events; if they run but the events never
 * arrive, the bridge token mapping is at fault. Wrappers log entry with
 * tid/RA/eax/esi/edi and tail-call the untouched generated body.
 */
#include <pthread.h>

extern void sub_00021CD0_gen(void);
extern void sub_000220E0_gen(void);

static int post_trace_enabled(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *e = getenv("RECOMP_POST_TRACE");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached;
}

void sub_00021CD0(void)
{
    if (post_trace_enabled()) {
        uint32_t task = g_eax;
        fprintf(stderr, "[POST] q1 tid=%lu ra=%08X task=%08X task+8=%04X "
                        "lock=%08X unlock=%08X cs=%08X list=%08X ev=%08X\n",
                (unsigned long)pthread_self(), MEM32(g_esp),
                task, MEM16(task + 8),
                MEM32(0x3156C0), MEM32(0x3156C4), MEM32(0x36CC54),
                MEM32(0x396E8C), MEM32(0x397920));
        fflush(stderr);
    }
    sub_00021CD0_gen();
    if (post_trace_enabled())
        fprintf(stderr, "[POST] q1 done list=%08X ev=%08X eax=%08X\n",
                MEM32(0x396E8C), MEM32(0x397920), g_eax);
}

void sub_000220E0(void)
{
    if (post_trace_enabled()) {
        fprintf(stderr, "[POST] q2 tid=%lu ra=%08X task=%08X task+8=%04X "
                        "task+C=%08X list=%08X ev=%08X setthunk=%08X\n",
                (unsigned long)pthread_self(), MEM32(g_esp), g_esi,
                g_esi ? MEM16(g_esi + 8) : 0, g_esi ? MEM32(g_esi + 0xC) : 0,
                MEM32(0x396E98), MEM32(0x397924), MEM32(0x315764));
        fflush(stderr);
    }
    sub_000220E0_gen();
    if (post_trace_enabled())
        fprintf(stderr, "[POST] q2 done list=%08X ev=%08X eax=%08X\n",
                MEM32(0x396E98), MEM32(0x397924), g_eax);
}

recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu)\n",
            va, (unsigned long long)g_icall_count);

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    fflush(stderr);
}

/* ── Untranslated instructions ───────────────────────────────────────────
 *
 * The lifter emits RECOMP_UNIMPL(text, va) at every instruction it has no
 * translation for, in place of the bare comment it used to leave. The
 * instruction is still a no-op; this only stops the omission being silent.
 * RECOMP_UNIMPL_TRAP=1 aborts at the first hit, at the guest address of the
 * cause rather than wherever the damage surfaces. */
#include <stdlib.h>

void recomp_unimpl(const char *text, uint32_t va)
{
    static int printed;
    const char *trap = getenv("RECOMP_UNIMPL_TRAP");
    int stop = trap && *trap && *trap != '0';

    if (printed < 50 || stop) {
        printed++;
        fprintf(stderr,
                "[UNIMPL] untranslated instruction REACHED: `%s` at 0x%08X"
                " (a no-op; set RECOMP_UNIMPL_TRAP=1 to stop here)\n",
                text, va);
        fflush(stderr);
    }
    if (stop) abort();
}

/* ── DSOUND packet-copy helper (note 0017) ───────────────────
 * sub_0028EA91(this, off, src): memcpy to *[[this+8]+0x10] + off from src.
 * The wavebank stream pump writes through that mailbox base; in this run the
 * base landed on our TIB page (0x4000) and the resulting stores are what the
 * error-slot setter later faulted on. RECOMP_DS_COPY=1 logs every call's
 * chain; calls whose computed dst is below the image load address are always
 * logged (that can never be a legitimate commit ring). */
extern void sub_0028EA91_gen(void);

void sub_0028EA91(void)
{
    extern __thread uint32_t g_ecx, g_esp;
    static int on = -1, budget = 300;
    if (on < 0) {
        const char *e = getenv("RECOMP_DS_COPY");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    uint32_t self = g_ecx;
    uint32_t parent = MEM32(self + 8);
    uint32_t m = parent ? MEM32(parent + 0x10) : 0;
    uint32_t base = m ? MEM32(m) : 0;
    uint32_t arg1 = MEM32(g_esp + 4);
    uint32_t arg2 = MEM32(g_esp + 8);
    uint32_t dst = base + arg1;
    int bad = dst < 0x00010000u;
    if ((on && budget > 0 && !bad) || (bad && budget > 0)) {
        budget--;
        fprintf(stderr, "[DS-COPY%s] this=%08X parent=%08X M=%08X *M=%08X "
                        "off=%08X dst=%08X src=%08X ret=%08X\n",
                bad ? "!" : "", self, parent, m, base, arg1, dst, arg2,
                MEM32(g_esp));
        fflush(stderr);
    }
    sub_0028EA91_gen();
}

/* sub_002854B3(this): the voice-state setter; ecx=this, and it calls the
 * packet-copy chain with the object stored at [this+0x14]. note 0017: that
 * slot was 0 and the copy ran with this=NULL. RECOMP_DS_VOICE=1 logs every
 * call's this and slot. */
extern void sub_002854B3_gen(void);

void sub_002854B3(void)
{
    extern __thread uint32_t g_ecx, g_esp;
    static int on = -1, budget = 200;
    if (on < 0) {
        const char *e = getenv("RECOMP_DS_VOICE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    uint32_t self = g_ecx;
    if (on && budget > 0) {
        budget--;
        fprintf(stderr, "[DS-VOICE%s] this=%08X slot14=%08X ret=%08X\n",
                (self && MEM32(self + 0x14)) ? "" : "!",
                self, self ? MEM32(self + 0x14) : 0, MEM32(g_esp));
        fflush(stderr);
    }
    sub_002854B3_gen();
}

/* ── title audio-driver pool allocator census (note 0017 H21) ───────
 * sub_002885AE(size, tag, zero) via [0x2a3530] arena; eax = block or 0.
 * sub_0028AF42(n, thiscall): creates the per-voice 'SdaD' packet object;
 * its alloc result is stored at [this+0x14] through a neg/sbb mask, so a
 * failed alloc leaves 0x8007000E (E_OUTOFMEMORY) there — a value that
 * *looks* valid (it sits inside the 0x80000000 contiguous window) and
 * poisons the whole mailbox chain downstream. RECOMP_DS_ALLOC=1. */
extern void sub_002885AE_gen(void);
extern void sub_0028AF42_gen(void);

void sub_002885AE(void)
{
    extern __thread uint32_t g_esp;
    static int on = -1, budget = 400;
    if (on < 0) {
        const char *e = getenv("RECOMP_DS_ALLOC");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && budget > 0) {
        budget--;
        uint32_t sz = MEM32(g_esp + 4), tag = MEM32(g_esp + 8);
        sub_002885AE_gen();
        fprintf(stderr, "[DS-ALLOC%c] size=%u tag='%c%c%c%c' arena=%08X -> %08X\n",
                g_eax ? ' ' : '!', sz,
                (char)(tag & 0xFF), (char)((tag >> 8) & 0xFF),
                (char)((tag >> 16) & 0xFF), (char)((tag >> 24) & 0xFF),
                MEM32(0x2a3530), g_eax);
        fflush(stderr);
    } else {
        sub_002885AE_gen();
    }
}

void sub_0028AF42(void)
{
    extern __thread uint32_t g_ecx, g_esp;
    static int on = -1, budget = 200;
    if (on < 0) {
        const char *e = getenv("RECOMP_DS_ALLOC");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && budget > 0) {
        budget--;
        uint32_t self = g_ecx;
        uint16_t n = (uint16_t)MEM32(g_esp + 4);
        sub_0028AF42_gen();
        uint32_t slot = self ? MEM32(self + 0x14) : 0;
        fprintf(stderr, "[DS-SDAD%c] this=%08X n=%u slot14=%08X\n",
                (slot && slot < 0x80000000u) ? ' ' : '!', self, n, slot);
        fflush(stderr);
    } else {
        sub_0028AF42_gen();
    }
}

/* sub_0028F8C3: the title audio driver's master open — five sub-inits
 * (0x28f5ce, 0x28f501, 0x28ed06, 0x28f0bc, 0x28f40d), each must return
 * >= 0. RECOMP_DS_INIT=1 prints which one fails: their hardware polls
 * live in the MCPX apertures (0xFE800000 APU, 0xFEC00000 AC97/CPC91),
 * and RECOMP_AC97_READY decides whether those answer. */
extern void sub_0028F8C3_gen(void);
typedef void (*recomp_func_t)(void);
extern recomp_func_t recomp_lookup(uint32_t xbox_va);

typedef uint32_t (*subr_t)(void);
static const uint32_t kDsInits[5] = { 0x28F5CE, 0x28F501, 0x28ED06,
                                      0x28F0BC, 0x28F40D };

void sub_0028F8C3(void)
{
    extern __thread uint32_t g_ecx;
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RECOMP_DS_INIT");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (!on) {
        sub_0028F8C3_gen();
        return;
    }
    uint32_t self = g_ecx;
    fprintf(stderr, "[DS-INIT] this=%08X start\n", self);
    fflush(stderr);
    for (int i = 0; i < 5; i++) {
        recomp_func_t f = recomp_lookup(kDsInits[i]);
        if (!f) { fprintf(stderr, "[DS-INIT] sub %u UNRESOLVED\n", i); continue; }
        uint32_t prev_esp = g_esp;
        g_ecx = self;
        f();
        uint32_t r = g_eax;
        g_eax = r;
        fprintf(stderr, "[DS-INIT] sub %u (0x%06X) -> %08X (%d) esp %08X->%08X\n",
                i, kDsInits[i], r, (int32_t)r, prev_esp, g_esp);
        fflush(stderr);
        if ((int32_t)r < 0) { g_eax = r; return; }
    }
    /* remaining tail (28f90d..28f925) skipped: we only came for the census */
    fprintf(stderr, "[DS-INIT] all 5 sub-inits OK (tail not run)\n");
    g_eax = 0;
}

/* ── game allocator census (note 0017 / H23) ────────────────
 * sub_0011F070(this): free/splice path reading heap-control links at
 * this+0x10/0x18/0x1C; the H23 crash splices a node whose link reads
 * 0xFE087E8D (NV2A aperture space). RECOMP_ALLOC_TRACE=1 prints this,
 * the control words, and flags links that point above 0xFE000000. */
extern void sub_0011F070_gen(void);

void sub_0011F070(void)
{
    extern __thread uint32_t g_ecx, g_esp;
    static int on = -1, budget = 400;
    if (on < 0) {
        const char *e = getenv("RECOMP_ALLOC_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    uint32_t self = g_ecx;
    if (on && budget > 0) {
        budget--;
        uint32_t l0 = self ? MEM32(self + 0x10) : 0,
                 l1 = self ? MEM32(self + 0x18) : 0,
                 l2 = self ? MEM32(self + 0x1C) : 0;
        int bad = (l0 | l1 | l2) & 0xFE000000u;
        fprintf(stderr, "[ALLOC%c] this=%08X +10=%08X +18=%08X +1C=%08X "
                        "ret=%08X\n", bad ? '!' : ' ', self, l0, l1, l2,
                MEM32(g_esp));
        fflush(stderr);
    }
    sub_0011F070_gen();
}

/* sub_001A9A50: the game's block allocator core (guest 0x1A9A50..0x1A9CC0,
 * one big lift). Reads esi on entry (caller-provided block/list arg) and
 * walks free/allocated lists via +4/+0xC/+0x14/+0x18/+0x1C links. The H23
 * crash splices at 0x1A9C1D with a chain tail reading 0xFE087E8D (NV2A
 * aperture). RECOMP_SPLICE_TRACE=1 logs entry state; ! = any pointer at
 * or above 0xFC000000 (device aperture: can never be a heap link). */
extern void sub_001A9A50_gen(void);

void sub_001A9A50(void)
{
    extern __thread uint32_t g_esi, g_esp;
    static int on = -1, budget = 4000;
    if (on < 0) {
        const char *e = getenv("RECOMP_SPLICE_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && budget > 0) {
        budget--;
        uint32_t s = g_esi;
        uint32_t l10 = s ? MEM32(s + 0x10) : 0;
        uint32_t l18 = s ? MEM32(s + 0x18) : 0;
        uint32_t l1c = s ? MEM32(s + 0x1c) : 0;
        int bad = (s | l10 | l18 | l1c) & 0xFC000000u;
        fprintf(stderr, "[SPLICE%c] esi=%08X +10=%08X +18=%08X +1C=%08X "
                        "ret=%08X\n", bad ? '!' : ' ', s, l10, l18, l1c,
                MEM32(g_esp));
        fflush(stderr);
        /* If we were called from the stream manager (ret 0x11F15E/11F16A),
         * edi = its control block: probe whether this call changes the
         * control links (the H23 poison path). */
        uint32_t ret = MEM32(g_esp);
        uint32_t esp_in = g_esp;
        uint32_t before[10];
        for (int k = 0; k < 10; k++)
            before[k] = esp_in ? MEM32(esp_in - 4 - 4 * k) : 0;
        sub_001A9A50_gen();
        if ((ret == 0x0011F15Eu || ret == 0x0011F16Au) && g_edi
                && g_edi < 0x04000000u) {
            fprintf(stderr, "[SPLICE>] edi=%08X +18=%08X +1C=%08X esp=%08X\n",
                    g_edi, MEM32(g_edi + 0x18), MEM32(g_edi + 0x1C), g_esp);
            fprintf(stderr, "[SPLICEesp] %08X -> %08X (want +%X)\n", esp_in,
                    g_esp, ret == 0x11F16Au || ret == 0x11F15Eu ? 8u : 0u);
            for (int k = 0; k < 10; k++) {
                uint32_t now = esp_in ? MEM32(esp_in - 4 - 4 * k) : 0;
                if (now != before[k])
                    fprintf(stderr, "[SPLICE!] [esp-%02X] %08X -> %08X\n",
                            4 + 4 * k, before[k], now);
            }
            fflush(stderr);
        }
        return;
    }
    sub_001A9A50_gen();
}

/* Probe: does the splice walk corrupt the stream manager's saved slot?
 * In the manager lift, before call 0x1a9a50 (ret 0x11F15E) the guest stack
 * top is esp-4 = the pushed arg; scan raw stack for the control pointer
 * around the first splice and report the slot holding it before/after.
 * Implemented as a wrapper on sub_001A99A0 (the float helper the walk
 * calls): prints its esp, the caller's arg slot [esp+0x14] before/after —
 * 0x1a99a0 is documented to WRITE [esp+0x14] (caller's pushed arg). */
extern void sub_001A99A0_gen(void);

void sub_001A99A0(void)
{
    extern __thread uint32_t g_esp;
    static int on = -1, budget = 200;
    if (on < 0) {
        const char *e = getenv("RECOMP_WALK99A0");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && budget > 0) {
        budget--;
        uint32_t esp = g_esp;
        uint32_t arg = esp ? MEM32(esp + 0x14) : 0;
        fprintf(stderr, "[99A0] esp=%08X [esp+14]=%08X ret=%08X\n", esp, arg,
                esp ? MEM32(esp) : 0);
        fflush(stderr);
        /* Watch the 64 bytes below our entry: the saved-register corridor
         * of our caller chain. Snapshot, call, diff. */
        uint32_t before[16];
        for (int k = 0; k < 16; k++)
            before[k] = esp ? MEM32(esp - 4 - 4 * k) : 0;
        sub_001A99A0_gen();
        uint32_t esp2 = g_esp;
        fprintf(stderr, "[99A0>] esp=%08X [esp+14]=%08X\n", esp2,
                esp2 ? MEM32(esp2 + 0x14) : 0);
        for (int k = 0; k < 16; k++) {
            uint32_t now = esp ? MEM32(esp - 4 - 4 * k) : 0;
            if (now != before[k])
                fprintf(stderr, "[99A0!] [esp-%02X] %08X -> %08X\n",
                        4 + 4 * k, before[k], now);
        }
        fflush(stderr);
        return;
    }
    sub_001A99A0_gen();
}

/* sub_0011F0C0: streaming arena manager (SOL gex streaming). edi = stream
 * control: +8 byte cursor (float math for progress), +0x10 arena cursor,
 * +0x14 limit, +0x18/+0x1C arena low/high block pointers; tail calls the
 * allocator sub_001A9A50 with esi=[edi+0x18] then esi=[edi+0x1C]. H23:
 * [edi+0x1C] held 0x00041C83 (a FILE-OFFSET-sized value, not a pointer)
 * — reading it as a block gave the NV2A-aperture garbage link.
 * RECOMP_STREAM_TRACE=1 logs the control block per call. */
extern void sub_0011F0C0_gen(void);

void sub_0011F0C0(void)
{
    extern __thread uint32_t g_edi, g_eax, g_esp;
    static int on = -1, budget = 200;
    if (on < 0) {
        const char *e = getenv("RECOMP_STREAM_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && budget > 0) {
        budget--;
        uint32_t d = g_edi;
        uint32_t l18 = d ? MEM32(d + 0x18) : 0, l1c = d ? MEM32(d + 0x1C) : 0;
        int bad = (l18 && l18 < 0x01000000u) || (l1c && l1c < 0x01000000u);
        fprintf(stderr, "[STREAM%c] edi=%08X eax=%08X +8=%08X +10=%08X "
                        "+14=%08X +18=%08X +1C=%08X ret=%08X\n",
                bad ? '!' : ' ', d, g_eax, d ? MEM32(d + 8) : 0,
                d ? MEM32(d + 0x10) : 0, d ? MEM32(d + 0x14) : 0,
                l18, l1c, MEM32(g_esp));
        fflush(stderr);
    }
    sub_0011F0C0_gen();
}

/* sub_001A9B60: allocator block-walk helper called by the splice core.
 * ESP-delta probe for H23 (one of its callees leaves the stack +4 short,
 * desynchronising every later guest stack read). RECOMP_B60_TRACE=1. */
extern void sub_001A9B60_gen(void);

void sub_001A9B60(void)
{
    extern __thread uint32_t g_esp;
    static int on = -1, budget = 500;
    if (on < 0) {
        const char *e = getenv("RECOMP_B60_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && budget > 0) {
        budget--;
        uint32_t esp_in = g_esp;
        sub_001A9B60_gen();
        fprintf(stderr, "[B60esp] %08X -> %08X (want +8) ret=%08X\n", esp_in,
                g_esp, esp_in ? MEM32(esp_in) : 0);
        fflush(stderr);
        return;
    }
    sub_001A9B60_gen();
}

/* ── GPU software-method service census (note 0021 H26) ─────────────
 * sub_002DB250(ecx=device regs cell): the D3D GPU-interrupt service
 * body; reads [[ecx]+0x400100] (PGRAPH_INTR via the 0xFD000000 aperture
 * mirror). sub_002DB4A0(ecx=object, eax=method): the software-method
 * dispatcher; switch (eax>>5)&0x1F, arm 4 = KeSetEvent(obj+0x1A4).
 * RECOMP_GSVC_TRACE=1 logs both; the question is whether the pumps run
 * at all, and what INTR they find. */
extern void sub_002DB250_gen(void);

void sub_002DB250(void)
{
    extern __thread uint32_t g_ecx;
    static int on = -1, budget = 400;
    /* frz51 core correction: the pump ENTRY is NOT the servicing point.
     * The pump body gates on INTR/PARAM/TRAP words and can bail without
     * reaching the dispatcher; consuming at this edge (0035 v1-v4 via
     * seen()) cleared the very words the gates must read, so swap NOPs
     * were eaten unread and the 4C completion never fired. seen() now
     * fires at the DISPATCHER entry (sub_002DB4A0), the one place the
     * mark's param is provably in hand. Census only here. */
    if (on < 0) {
        const char *e = getenv("RECOMP_GSVC_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && budget > 0) {
        budget--;
        uint32_t obj = g_ecx;
        uint32_t reg = obj ? MEM32(obj) : 0;
        fprintf(stderr, "[GSVC] ecx=%08X regs=%08X INTR=%08X TRAP=%08X "
                        "PARAM=%08X\n",
                obj, reg,
                reg ? MEM32(reg + 0x400100) : 0,
                reg ? MEM32(reg + 0x400704) : 0,
                reg ? MEM32(reg + 0x400108) : 0);
        fflush(stderr);
    }
    sub_002DB250_gen();
}

extern void sub_002DB4A0_gen(void);

void sub_002DB4A0(void)
{
    extern __thread uint32_t g_ecx;
    extern void nv2a_pb_swmethod_seen(void);
    static int on = -1, budget = 2500;
    /* frz51 core: THE servicing point. Reaching the guest dispatcher
     * means TRAP_DATA was read into eax with the mark's param in hand
     * — hardware's service read. Consume-at-seen (0035) must trigger
     * here, not at pump/DPC entry, where the bodies can bail through
     * their gates and the eaten words never got read (swap NOPs
     * vanished unread -> 4C never set -> engine INFINITE). */
    nv2a_pb_swmethod_seen();
    if (on < 0) {
        const char *e = getenv("RECOMP_GSVC_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (budget > 0 || (g_eax & 0x1Fu) <= 2u)) {
        if (budget > 0) budget--;
        {   /* caller attribution (frz36, note 0023): at entry the guest
             * stack top holds the return address; the 228k-plateau
             * re-dispatches (frz35) name their driver this way. Dedup
             * consecutive identical (param, caller) pairs into a run. */
            static uint32_t lp = 0xFFFFFFFFu, lc = 0xFFFFFFFFu;
            static unsigned run;
            static unsigned n;
            uint32_t prm = g_eax, cal = MEM32(g_esp);
            if (prm == lp && cal == lc) {
                run++;
                if (((n++) & 0xFFFFu) == 0)
                    fprintf(stderr, "[SWM] ..still param=%08X from=%08X run=%u\n",
                            prm, cal, run);
            } else {
                if (run)
                    fprintf(stderr, "[SWM] (x%u)\n", run);
                run = 0; lp = prm; lc = cal;
                fprintf(stderr, "[SWM] obj=%08X param=%08X class=%u from=%08X\n",
                        g_ecx, prm, (unsigned)((prm & 0x1F) - 1), cal);
                fflush(stderr);
            }
        }
    }
    sub_002DB4A0_gen();
}

/* DPC-body census (note 0021 H26): sub_002DAE40 is the deferred
 * routine of the DPC the vector-3 ISR queues every tick (DPC object
 * 0x2E2E3C, context 0x2E2DB8 = the register-window cell; measured in
 * sudeki.frz2.core). Its dispatch word is [regs+0x100] (PMC aperture).
 * RECOMP_GSVC_TRACE=1 logs it: decides whether the DPC runs at all.
 * Logs every pass whose gate bit 0x1000 (PGRAPH line) is up. */
extern void sub_002DAE40_gen(void);

void sub_002DAE40(void)
{
    static int on = -1, seen = 0;
    uint32_t pmc = MEM32(0xFD000100);
    /* frz51 correction: no seen() here either — the DPC body gates on
     * PMC bits and may bail before any real service; consume happens
     * at the dispatcher entry (sub_002DB4A0). */
    if (on < 0) {
        const char *e = getenv("RECOMP_GSVC_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (seen < 12 || (pmc & 0x1000u) || (seen % 2000) == 0)) {
        seen++;
        fprintf(stderr, "[DPC] PMC=%08X PGRAPH=%08X TRAP=%08X "
                        "PARAM=%08X\n",
                pmc, MEM32(0xFD400100),
                MEM32(0xFD400704), MEM32(0xFD400108));
        fflush(stderr);
    } else if (on) {
        seen++;
    }
    sub_002DAE40_gen();
}

static volatile unsigned long long g_flip_issue, g_flip_wait;
static volatile unsigned long long g_swap_pump, g_eng_submit;
static volatile unsigned long long g_swap_pump2, g_eng_submit2;

/* frz60 (note 0023 H30): per-caller-site histograms for the swap pump
 * and engine submit, logged when a site's count doubles (power-of-two
 * cadence = O(log) lines). Which caller sites are still live at the
 * plateau is the H30 question; the last site lines before the freeze
 * name the feeder that went quiet. */
static void swap_site(const char *tag, uint32_t cal,
                      const volatile unsigned long long *n,
                      const volatile unsigned long long *m)
{
    struct ent { uint32_t va; unsigned long long n, pow; } *e = 0;
    static struct ent tab[24];
    int i;
    if (!cal || cal >= 0x8000000u)
        return;
    cal |= (tag[0] == 'E') ? 0x8000000u : 0;   /* separate PUMP/ENG keys */
    for (i = 0; i < 24; i++) {
        if (tab[i].va == cal) { e = &tab[i]; break; }
        if (!tab[i].va) { e = &tab[i]; e->va = cal; break; }
    }
    if (!e)
        return;
    e->n++;
    if (e->n >= e->pow) {
        e->pow = e->pow ? e->pow * 2 : 1;
        fprintf(stderr, "[%s] n=%llu from=%08X m=%llu\n",
                tag, e->n, cal, m ? *m : 0ull);
        fflush(stderr);
    }
}

/* ── device-message handler census (note 0023 H30, RECOMP_ED20_TRACE=1) ──
 *
 * frz45/48/50/53 plateau anatomy: Presents keep issuing and the ring
 * keeps advancing (submit/iGET climb, patch 0031 semaphore word i0
 * follows), but FLIP_INCREMENT_WRITE stops: ctx+0x8700 (the XGRAPHCL
 * flip-PENDING word) reads the poison constant 0x51747D8B, so D3D's
 * Present path never emits the next flip, the swap pump sub_002D35A0
 * (pump=flips) and engine submit sub_001E0390 freeze, and 5C/4C stop
 * firing. sub_0011ED20 is the handler that invokes BOTH the pump and
 * (transitively) the submit; its callers are the engine message
 * consumers sub_0001E9E0/EDC0/F0F0, the stream helper sub_00022270
 * and a tail-jmp from sub_0011D200. Census: invocation count, caller
 * site (deduped runs), and the flip-pending word at entry — decides
 * "handler stops being invoked" vs "invoked but the pump branch is
 * skipped". */
extern void sub_0011ED20_gen(void);
static volatile unsigned long long g_ed20_calls;

void sub_0011ED20(void)
{
    extern __thread uint32_t g_esp;
    static int on = -1;
    static uint32_t lp = 0xFFFFFFFFu;
    static unsigned run;
    if (on < 0) {
        const char *e = getenv("RECOMP_ED20_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on) {
        g_ed20_calls++;
        uint32_t cal = MEM32(g_esp);
        uint32_t c = MEM32(0x2E3638);
        uint32_t sh = (c >= 0x1000u && c < 0x4000000u)
                        ? MEM32(c + 0x8700) : 0u;
        if (cal != lp) {
            if (run)
                fprintf(stderr, "[ED20] (x%u)\n", run);
            run = 0; lp = cal;
            fprintf(stderr, "[ED20] n=%llu from=%08X sh8700=%08X "
                            "pump=%llu eng=%llu\n",
                    g_ed20_calls, cal, sh, g_swap_pump, g_eng_submit);
            fflush(stderr);
        }
        if (run < 0xFFFFu)
            run++;
        if ((g_ed20_calls % 2000u) == 0)
            fprintf(stderr, "[ED20] n=%llu from=%08X sh8700=%08X\n",
                    g_ed20_calls, lp, sh);
    }
    sub_0011ED20_gen();
}

/* ── post-attract render-gate census (note 0023 H28, RECOMP_GATE_TRACE=1) ──
 *
 * The XAPI frame loop (sub_0011B020) runs sub_001DE310 per frame; its
 * gate sub_002C3200(ctx) decides render vs skip — a non-zero return
 * jumps to 0x1DE40C and the whole render/Present segment is skipped
 * (no ring traffic, no flips: the observed 0.00-fps stall). The three
 * mid-stall cores land in three different phases (render-transform
 * sub_002CB420/sub_002CBAF0, and the gate's own skip-path ICALL
 * sub_002CA490), so frames keep cycling — the question is the gate's
 * streaming/budget state.
 *
 * Aggregate per second (cumulative counters): gate calls/pass/skip,
 * render-segment entries (sub_002C27A0), present-path entries
 * (sub_002D5BF0), and the ctx slots the gate reads -- NOTE 0025:
 * ctx is a BINK struct (online movie): +0x14/+0x18 fps num/den,
 * +0x270 frames decoded, +0x280 pacing tick stamp, +0x284 end
 * marker, +0x27C last tick, +0x2B4 present target, +0x338/+0x33C
 * budget accumulators. Off (no wrapper cost beyond one
 * getenv-cached branch when unset).
 */
#include <time.h>

extern void sub_002C3200_gen(void);
extern void sub_002C27A0_gen(void);
extern void sub_002D5BF0_gen(void);

/* frz14 swap-machine census (note 0023): flip-issue / flip-wait /
 * swap-completion pump / engine-submit counters; per-second line
 * printed from the Present-API wrapper (only function proven to
 * tick 30/s even during the stall). */
extern void sub_002D3B50_gen(void);
extern void sub_002D3CC0_gen(void);
extern void sub_002D35A0_gen(void);
extern void sub_001E0390_gen(void);

static int swap_census_on(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *e = getenv("RECOMP_PRES_TRACE");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached;
}

static volatile unsigned long long g_gate_calls, g_gate_pass, g_gate_skip;
static volatile unsigned long long g_rend_seg, g_pres_seg;

static int gate_trace_enabled(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *e = getenv("RECOMP_GATE_TRACE");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached;
}

void sub_002C27A0(void)
{
    if (gate_trace_enabled())
        g_rend_seg++;
    sub_002C27A0_gen();
}

void sub_002D5BF0(void)
{
    if (gate_trace_enabled())
        g_pres_seg++;
    {
        /* frz12 probe (note 0023 H28c): dump the Present short-circuit
         * inputs — the D3D device's state/effect fields decide whether
         * Present reaches the submit chain at all. */
        static int on = -1;
        if (on < 0) {
            const char *e = getenv("RECOMP_PRES_TRACE");
            on = (e && *e && *e != '0') ? 1 : 0;
        }
        if (on) {
            uint32_t dev = g_eax;
            if (dev >= 0x1000u && dev < 0x80000000u) {
                static time_t last;
                static uint32_t prevf;
                uint32_t f = MEM32(dev), d8 = MEM32(dev + 8);
                uint32_t d14 = MEM32(dev + 0x14);
                time_t now = time(NULL);
                if (now != last || f != prevf) {
                    last = now;
                    prevf = f;
                    fprintf(stderr, "[PRESN] dev=%08X flags=%08X dev8=%08X "
                                    "dev14=%08X d14f=%08X\n",
                            dev, f, d8, d14,
                            d14 >= 0x1000u && d14 < 0x80000000u ? MEM32(d14) : 0);
                    {
                        uint32_t c = MEM32(0x2E3638), idx = 0, slot = 0;
                        if (c >= 0x1000u && c < 0x4000000u) {
                            idx = MEM32(c + 0x2478);
                            if (idx < 16u)
                                slot = MEM32(c + idx * 4 + 0x1974);
                            fprintf(stderr, "[SWAP] issue=%llu wait=%llu "
                                            "pump=%llu eng=%llu idx=%u "
                                            "slot=%08X f8D4=%08X f8D8=%08X "
                                            "pend=%08X want=%08X\n",
                                    g_flip_issue, g_flip_wait, g_swap_pump,
                                    g_eng_submit, idx, slot,
                                    MEM32(c + 0x8D4), MEM32(c + 0x8D8),
                                    MEM32(0xFD008700), MEM32(0xFD008704));
                        fprintf(stderr, "[SWAP2] pump2=%llu eng2=%llu\n",
                                g_swap_pump2, g_eng_submit2);
                        }
                    }
                    fflush(stderr);
                }
            }
        }
    }
    sub_002D5BF0_gen();
}

void sub_002C3200(void)
{
    uint32_t ctx;
    if (!gate_trace_enabled()) {
        sub_002C3200_gen();
        return;
    }
    ctx = g_eax;
    g_gate_calls++;
    sub_002C3200_gen();
    if (g_eax) g_gate_skip++; else g_gate_pass++;
    {
        static time_t last;
        time_t now = time(NULL);
        if (now != last) {
            last = now;
            if (ctx >= 0x1000u)
                /* NOTE (note 0025): ctx is a BINK struct (online-mode
                 * player), NOT a load-job queue. mdec=+0x270 frames
                 * decoded (climbs through movie loops), mpr=+0x2B4
                 * present-target, mtsk=+0x280 pacing tick stamp,
                 * mtick=+0x27C last tick, mdone=+0x284 end marker,
                 * bud/cons=pace accumulators, sc14/sc18=fps num/den. */
                fprintf(stderr, "[GATE] calls=%llu pass=%llu skip=%llu rend=%llu "
                                "pres=%llu ctx=%08X FC=%08X io=%08X q=%08X "
                                "mdec=%08X mtsk=%08X mdone=%08X mtick=%08X mpr=%08X "
                                "bud=%08X cons=%08X fps=%08X/%08X\n",
                        g_gate_calls, g_gate_pass, g_gate_skip, g_rend_seg,
                        g_pres_seg, ctx,
                        MEM32(ctx + 0xFC), MEM32(ctx + 0x2A0), MEM32(ctx + 0x2F8),
                        MEM32(ctx + 0x270), MEM32(ctx + 0x280), MEM32(ctx + 0x284),
                        MEM32(ctx + 0x27C), MEM32(ctx + 0x2B4),
                        MEM32(ctx + 0x338), MEM32(ctx + 0x33C),
                        MEM32(ctx + 0x14), MEM32(ctx + 0x18));
            else
                fprintf(stderr, "[GATE] calls=%llu pass=%llu skip=%llu rend=%llu "
                                "pres=%llu ctx=%08X\n",
                        g_gate_calls, g_gate_pass, g_gate_skip, g_rend_seg,
                        g_pres_seg, g_eax);
            fflush(stderr);
        }
    }
}

/* ── Present/submit chain census (note 0023 H28b, RECOMP_PRES_TRACE=1) ──
 *
 * frz10: the game-side Present wrapper ticks 30/s during the 0-fps
 * stall while the GPU is fully submission-quiet (draws/clears/
 * unhandled constant). The D3D runtime Present sub_002D57D0 has a
 * ring free-space gate (free < 0x2400 or signed-negative ->
 * loc_002D58D3) that skips the method writes and the submit leaf
 * sub_002D5C80; block alloc sub_002D55E0 failing (eax=0) exits even
 * earlier. If submits go quiet while attempts tick, the ring
 * bookkeeping words printed here name the stuck quantity.
 * ctx = MEM32(0x2E3638): +0x2C dev, +0x30 block list, +0x44 ring
 * size, +0x19FC submit callback, +0x1A00 last cursor.
 */
extern void sub_002D57D0_gen(void);
extern void sub_002D5760_gen(void);
extern void sub_002D5C80_gen(void);
extern void sub_002D55E0_gen(void);

static volatile unsigned long long g_pres_try, g_pres_sub, g_alloc_ok, g_alloc_fail;
static volatile uint32_t g_prog_seen[8]; /* recent sub_002D5760 returns */
static volatile unsigned g_prog_idx;

static int pres_trace_enabled(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *e = getenv("RECOMP_PRES_TRACE");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached;
}

void sub_002D5C80(void)
{
    if (pres_trace_enabled())
        g_pres_sub++;
    sub_002D5C80_gen();
}

/* H29 census (note 0023, frz29): the ring free-space calculator
 * sub_002D55B0 (lifted): eax = GET VA on entry; edx = MEM32(esi)
 * CPU cursor; wraps via MEM32(esi+0x44) (ring size); arg block+4 =
 * the allocation's start cursor; returns wrapped free (clamped to 0
 * when negative). The writer's wait branch is (free < 0x2000). Print
 * the whole tuple during the plateau so the stuck term is named. */
extern void sub_002D55B0_gen(void);
void sub_002D55B0(void)
{
    extern __thread uint32_t g_eax;
    uint32_t in_eax, w, size, blk4;
    if (!pres_trace_enabled()) {
        sub_002D55B0_gen();
        return;
    }
    in_eax = g_eax;
    w = MEM32(0x2E3638u);
    {
        uint32_t ctx = w;
        w = (ctx >= 0x1000u) ? MEM32(ctx) : 0u;
        size = (ctx >= 0x1000u) ? MEM32(ctx + 0x44) : 0u;
        blk4 = MEM32(MEM32(g_esp + 4) + 4);
    }
    sub_002D55B0_gen();
    {
        static unsigned n;
        if (n++ < 40 || (n & 0xFFFu) == 0)
            fprintf(stderr, "  [FREE] GETv=%08X w=%08X size=%08X blk+4=%08X"
                            " -> free=%08X\n",
                    in_eax, w, size, blk4, g_eax);
    }
}

void sub_002D55E0(void)
{
    if (!pres_trace_enabled()) {
        sub_002D55E0_gen();
        return;
    }
    sub_002D55E0_gen();
    if (g_eax) g_alloc_ok++; else g_alloc_fail++;
}

void sub_002D5760(void)
{
    if (!pres_trace_enabled()) {
        sub_002D5760_gen();
        return;
    }
    sub_002D5760_gen();
    g_prog_seen[g_prog_idx & 7u] = g_eax;
    g_prog_idx++;
}

struct pres_ret { uint32_t va; unsigned long long n; unsigned long long prev; };
static struct pres_ret g_pres_rets[48]; /* census-only; counts race benign */

void sub_002D57D0(void)
{
    extern __thread uint32_t g_esp;
    if (!pres_trace_enabled()) {
        sub_002D57D0_gen();
        return;
    }
    {
        uint32_t ret = MEM32(g_esp); /* guest return VA = caller site */
        struct pres_ret *slot = 0;
        int i;
        for (i = 0; i < 48; i++) {
            if (g_pres_rets[i].va == ret) { slot = &g_pres_rets[i]; break; }
            if (!g_pres_rets[i].va && !slot) slot = &g_pres_rets[i];
        }
        if (slot) { slot->va = ret; slot->n++; }
    }
    g_pres_try++;
    sub_002D57D0_gen();
    {
        static time_t last;
        time_t now = time(NULL);
        if (now != last) {
            uint32_t ctx = MEM32(0x2E3638);
            int k, a = -1, b = -1, c = -1;
            unsigned long long da = 0, db = 0, dc = 0;
            last = now;
            for (k = 0; k < 48 && g_pres_rets[k].va; k++) {
                unsigned long long d;
                struct pres_ret *p = &g_pres_rets[k];
                d = p->n - p->prev;
                p->prev = p->n;
                if (d >= da) { c = b; dc = db; b = a; db = da; a = k; da = d; }
                else if (d >= db) { c = b; dc = db; b = k; db = d; }
                else if (d > dc) { c = k; dc = d; }
            }
            if (ctx >= 0x1000u) {
                /* H29 census (note 0023): the ring writer's own GET
                 * source is MEM32(MEM32(ctx+0x1C20)+0x44)|0x80000000
                 * (lifted sub_002D5760 loc_002D5783) — the channel
                 * INSTANCE block, a different pointer from the USER
                 * window at ctx+0x934 that the executor advances.
                 * Dump both GET feeds, the kick latch at
                 * (ctx+0x934)+0x400B10, and the ring bounds. */
                uint32_t inst = MEM32(ctx + 0x1C20);
                uint32_t user = MEM32(ctx + 0x934);
                fprintf(stderr, "[PRES] try=%llu submit=%llu alloc=%llu/%llu "
                                "callers=%08X:%llu %08X:%llu %08X:%llu "
                                "ctx=%08X w=%08X blk=%08X blk0=%08X "
                                "size=%08X cb=%08X "
                                "inst=%08X iGET=%08X user=%08X uGET=%08X "
                                "latch=%08X lo=%08X hi=%08X w2C=%08X "
                                "i0=%08X i4=%08X sh8700=%08X\n",
                        g_pres_try, g_pres_sub, g_alloc_ok, g_alloc_fail,
                        a >= 0 ? g_pres_rets[a].va : 0u, da,
                        b >= 0 ? g_pres_rets[b].va : 0u, db,
                        c >= 0 ? g_pres_rets[c].va : 0u, dc,
                        ctx, MEM32(ctx), MEM32(ctx + 0x30),
                        MEM32(MEM32(ctx + 0x30)),
                        MEM32(ctx + 0x44), MEM32(ctx + 0x19FC),
                        inst,
                        inst >= 0x1000u ? MEM32(inst + 0x44) : 0u,
                        user,
                        user >= 0x1000u ? MEM32(user + 0x44) : 0u,
                        (user >= 0x1000u) ? MEM32(user + 0x400B10u) : 0u,
                        MEM32(ctx + 0x24), MEM32(ctx + 0x28),
                        MEM32(ctx + 0x2C),
                        inst >= 0x1000u ? MEM32(inst) : 0u,
                        inst >= 0x1000u ? MEM32(inst + 4) : 0u,
                        MEM32(ctx + 0x8700));
            } else
                fprintf(stderr, "[PRES] try=%llu submit=%llu alloc=%llu/%llu\n",
                        g_pres_try, g_pres_sub, g_alloc_ok, g_alloc_fail);
            fflush(stderr);
        }
    }
}

/* frz14 swap-machine census wrappers (note 0023). One line of cost
 * when RECOMP_PRES_TRACE is unset; the per-second report lives in
 * the Present-API wrapper. */
void sub_002D3B50(void)
{
    if (swap_census_on())
        g_flip_issue++;
    sub_002D3B50_gen();
}

void sub_002D3CC0(void)
{
    if (swap_census_on())
        g_flip_wait++;
    sub_002D3CC0_gen();
}

void sub_002D35A0(void)
{
    extern __thread uint32_t g_esp;
    /* frz60 discrimination (note 0023): count UNCONDITIONALLY too.
     * frz59 paradox: [PRES] shows the ring writer called 64/s from the
     * pump's own site 0x2D3640 while g_swap_pump reads frozen at 1511 —
     * either the census gate (swap_census_on) is lying or the site
     * attribution is stale-stack fiction. pump2 settles it. */
    g_swap_pump2++;
    if (swap_census_on()) {
        g_swap_pump++;
        swap_site("PUMP", g_esp, &g_swap_pump, NULL);
    }
    sub_002D35A0_gen();
}

void sub_001E0390(void)
{
    extern __thread uint32_t g_esp;
    g_eng_submit2++;
    if (swap_census_on()) {
        g_eng_submit++;
        swap_site("ENG", g_esp, &g_eng_submit, NULL);
    }
    sub_001E0390_gen();
}

/* ── read-helper attribution trace (note 0025 H31, RECOMP_RD_TRACE=1) ──
 * sub_00270FBD is the title's stock archive/movie read helper: both
 * Bink buffers and the BAF config streams land in NtReadFile through
 * it. The bridge [READ] line names path/size but not the consumer;
 * this wrapper logs thisptr + stack args + the caller chain (EBP
 * frames, LTCG frames are usually intact) so the moment BAF
 * consumption stops can be attributed to the function that stopped
 * asking. One line per read: arm it only on forensics runs. */
extern void sub_00270FBD_gen(void);

void sub_00270FBD(void)
{
    extern __thread uint32_t g_esp, g_ebp, g_ecx;
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on) {
        /* The helper is frameless LTCG (EBP inherited, no chain), so
         * attribution = scan the caller's live stack for return
         * addresses: a coarse but FPO-proof caller fingerprint.
         * NOTE (sub_0001E170 anatomy): the real stream object is the
         * CALLER's ECX, saved at push site esp+0x24; obj+0x08 = file
         * handle, obj+0x14 = byte cursor, obj+0x30+0x81C = lock.
         * Log obj + cursor before, and cursor after, per read. */
        /* (esp+0x24 "caller ecx" probe on frz100 printed code words --
         * the save slot is not uniform; stream-object identity stays
         * with the [PUMP] census instead.) */
        fprintf(stderr, "  [RD] a1=%08X a2=%08X n=%u stack:",
                MEM32(g_esp + 4), MEM32(g_esp + 8), MEM32(g_esp + 0xC));
        for (uint32_t k = 0; k < 0x140u; k += 4) {
            uint32_t v = MEM32(g_esp + k);
            if (v >= 0x11000u && v < 0x286000u)
                fprintf(stderr, " %08X", v);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    sub_00270FBD_gen();
}

/* ── BAF job-pump state census (note 0025 H31, RECOMP_RD_TRACE=1) ──
 * sub_0023B830(0, mgr) is the title's job-pump, called from the BAF
 * manager tick sub_001EDA20 (test obj+0x28 & 0x200 gates the whole
 * path; obj+0x04 non-null with first byte != 0 must also hold). The
 * pump walks the manager node list, per-node vtbl[2] open / vtbl[1]
 * read (-> our [RD]) / vtbl[0] release, under global cs 0x3156C0,
 * with reentrancy slot mgr+0xC0E4 and type gate sub_001F96C0.
 * Per-entry line: manager, stage id +0x18, state +0x28, node head
 * +0x04, queue slot, caller stack fingerprint. Separates:
 * pump-stops-running vs runs-with-empty-list vs nodes-never-read. */
extern void sub_0023B830_gen(void);

void sub_0023B830(void)
{
    extern __thread uint32_t g_esp;
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    uint32_t mgr = 0;
    if (on) {
        mgr = (g_esp >= 0x10000u && g_esp < 0x7FFF0000u)
                  ? MEM32(g_esp + 8) : 0u;   /* call is (0, mgr) */
        if (mgr && ((n++ & 0x3F) == 0 || n < 400)) {
            fprintf(stderr, "  [PUMP] x%u mgr=%08X id=%04X st=%08X "
                    "node=%08X q=%u:", n, mgr, MEM16(mgr + 0x18),
                    MEM32(mgr + 0x28), MEM32(mgr + 4),
                    MEM32(mgr + 0xC0E4));
            for (uint32_t k = 0; k < 0x60u; k += 4) {
                uint32_t v = MEM32(g_esp + k);
                if (v >= 0x11000u && v < 0x286000u)
                    fprintf(stderr, " %08X", v);
            }
            fprintf(stderr, "\n");
            fflush(stderr);
        }
    }
    sub_0023B830_gen();
    if (on && mgr && n < 400) {
        fprintf(stderr, "  [PUMP] x%u -> ret=%08X\n", n, g_eax);
        fflush(stderr);
    }
}

/* sub_001EDA20 is the manager tick that DECIDES to pump (both pump
 * call sites live inside it). ENTRY-ONLY probe: no guest/stack
 * dereferences (g_esp was stale on the faulting thread, frz102).
 * If [MTICK] keeps printing while [PUMP] goes silent, the tick runs
 * and its internal gates close; if [MTICK] also goes silent, the
 * era loop above stopped calling the tick. */
extern void sub_001EDA20_gen(void);

void sub_001EDA20(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (n++ & 0x3F) == 0) {
        void *ra = __builtin_extract_return_addr(__builtin_return_address(0));
        fprintf(stderr, "  [MTICK] x%u ra=%p\n", n, ra);
        fflush(stderr);
    }
    sub_001EDA20_gen();
}

/* sub_001EF1D0 sits in the load-pipeline era loop
 * (sub_001EC130 -> sub_001EF0C0 -> sub_001EF1D0 -> sub_001EE110 ->
 * tick) and reads the HMF manager global 0x397668. Entry-only probe:
 * if this keeps ticking while [PUMP] is silent, the pipeline runs
 * but the tick's service gate stays shut. */
extern void sub_001EF1D0_gen(void);

void sub_001EF1D0(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    uint32_t a1 = (g_esp >= 0x10000u && g_esp < 0x7FFF0000u)
                      ? MEM32(g_esp + 4) : 0u;
    uint32_t a2 = (g_esp >= 0x10000u && g_esp < 0x7FFF0000u)
                      ? MEM32(g_esp + 8) : 0u;
    if (on && n < 400) {
        void *ra = __builtin_extract_return_addr(__builtin_return_address(0));
        fprintf(stderr, "  [EF1D0] x%u ra=%p a1=%08X a2=%08X bkt=%u\n",
                n, ra, a1, a2, (a1 >> 6) & 31u);
        fflush(stderr);
    }
    n++;
    sub_001EF1D0_gen();
    if (on && n < 400) {
        fprintf(stderr, "  [EF1D0] x%u -> esi=%08X\n", n, g_esi);
        fflush(stderr);
    }
}

/* sub_001EF0C0 is the relay dispatcher above EF1D0: it picks WHICH
 * manager list gets serviced from the load-context mode (arg2 & 3):
 * {0,1} -> list A, 2 -> list B, 3 -> NO servicing at all (skips the
 * EF1D0 call). [EF0C0] prints (arg1, arg1+8, mode) on mode/arg
 * transitions + first 20 + every 200. */
extern void sub_001EF0C0_gen(void);

void sub_001EF0C0(void)
{
    static int on = -1;
    static unsigned n = 0;
    static uint32_t pa = 0xDEAD, pm = 0xDEAD;
    extern __thread uint32_t g_esp;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    uint32_t a1 = (g_esp >= 0x10000u && g_esp < 0x7FFF0000u)
                      ? MEM32(g_esp + 4) : 0u;
    uint32_t a2 = (g_esp >= 0x10000u && g_esp < 0x7FFF0000u)
                      ? MEM32(g_esp + 8) : 0u;
    uint32_t f8 = (a1 >= 0x01000000u && a1 < 0x04000000u)
                      ? MEM32(a1 + 8) : 0xDEADu;
    if (on && (n < 20 || a1 != pa || (a2 & 3) != pm)) {
        fprintf(stderr, "  [EF0C0] x%u a1=%08X a1+8=%08X a2=%08X mode=%u\n",
                n, a1, f8, a2, a2 & 3u);
        fflush(stderr);
    }
    if (on) { pa = a1; pm = a2 & 3u; n++; }
    sub_001EF0C0_gen();
}

/* Relay membership: sub_001EC2C0 REGISTERs a manager obj (esi) into
 * bucket (arg-epoch eax >> 6) & 31 of the 0x37A120 array; sub_001EC4E0
 * DEREGISTERs (edi). frz103 core: all buckets + all three EF0C0 lists
 * EMPTY at kill, while 018CF100's pump (x167) never reached its
 * sentinel -> it was DEREGed mid-work and the next leg was never
 * REGed. These two print the exact unlink/register sequence. */
extern void sub_001EC2C0_gen(void);

void sub_001EC2C0(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (n < 40 || (n % 200) == 0)) {
        fprintf(stderr, "  [REG] x%u obj=%08X epoch=%08X\n", n, g_esi, g_eax);
        fflush(stderr);
    }
    n++;
    sub_001EC2C0_gen();
}

extern void sub_001EC4E0_gen(void);

void sub_001EC4E0(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (n < 40 || (n % 200) == 0)) {
        fprintf(stderr, "  [DEREG] x%u obj=%08X\n", n, g_edi);
        fflush(stderr);
    }
    n++;
    sub_001EC4E0_gen();
}

/* Producer side: sub_001EE190(list=eax, queue=ecx, scratch, flag) is
 * the enqueue/wait cycle over the mode lists; sub_001EABD0 is the
 * game-level load-request API (single caller 0x00021B1B). If these
 * go silent at the stall, the PRODUCER stopped -- the engine is
 * waiting for the last leg to signal completion, and the leg never
 * does. */
extern void sub_001EE190_gen(void);

void sub_001EE190(void)
{
    static int on = -1;
    static unsigned n = 0;
    static uint32_t pl = 0xDEAD;
    extern __thread uint32_t g_esp;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    uint32_t a1 = (g_esp >= 0x10000u && g_esp < 0x7FFF0000u)
                      ? MEM32(g_esp + 4) : 0u;
    if (on && (n < 40 || g_eax != pl || (n % 200) == 0)) {
        fprintf(stderr, "  [ENQ] x%u list=%08X queue=%08X a1=%08X\n",
                n, g_eax, g_ecx, a1);
        fflush(stderr);
    }
    if (on) { pl = g_eax; n++; }
    sub_001EE190_gen();
    if (on && n < 40) {
        fprintf(stderr, "  [ENQ] x%u -> %08X\n", n, g_eax);
        fflush(stderr);
    }
}

extern void sub_001EABD0_gen(void);

void sub_001EABD0(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (n < 40 || (n % 200) == 0)) {
        void *ra = __builtin_extract_return_addr(__builtin_return_address(0));
        fprintf(stderr, "  [REQ] x%u ra=%p\n", n, ra);
        fflush(stderr);
    }
    n++;
    sub_001EABD0_gen();
}

/* Funnel between EF0C0 and EF1D0 (frz107: EF0C0 >=419 entries but
 * EF1D0 <256 while buckets are FULL -> the skip is between:
 * EF0C0 -> EC590 (registrar) -> EC6B0 -> [mode gate] -> EF1D0).
 * EC590/EC6B0 receive (ctx=ebx, list=edi, esi-arg): count + args so
 * the drop-off is localized exactly. */
extern void sub_001EC590_gen(void);

void sub_001EC590(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (n < 40 || (n % 128) == 0)) {
        fprintf(stderr, "  [REG590] x%u list=%08X esi=%08X\n", n, g_edi, g_esi);
        fflush(stderr);
    }
    n++;
    sub_001EC590_gen();
}

extern void sub_001EC6B0_gen(void);

void sub_001EC6B0(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (n < 40 || (n % 128) == 0)) {
        fprintf(stderr, "  [S6B0] x%u\n", n);
        fflush(stderr);
    }
    n++;
    sub_001EC6B0_gen();
}

/* Load-session wait cycle sub_001EAAB0: Wait(0x37A118) / FE8A0 /
 * FE690 / reset 0x37A114 / Wait / ... If this entry count freezes,
 * a thread is parked INFINITE on the load-complete event.
 * Reservation-accounting helpers sub_001EF4C0 / sub_001EF510 (byte
 * pending 0x3975A8/AC): if NEVER called, reservations are never
 * released -> byte budget can pin servicing gates. */
extern void sub_001EAAB0_gen(void);

void sub_001EAAB0(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (n < 20 || (n % 64) == 0)) {
        void *ra = __builtin_extract_return_addr(__builtin_return_address(0));
        fprintf(stderr, "  [EACYC] x%u ra=%p\n", n, ra);
        fflush(stderr);
    }
    n++;
    sub_001EAAB0_gen();
    if (on && n < 20) {
        fprintf(stderr, "  [EACYC] x%u -> %08X\n", n, g_eax);
        fflush(stderr);
    }
}

/* Pump's two service processors (frz106 pump chain). F99D0's body
 * refuses at loc_001F9B5C unless popcount(ebx)==1 && popcount(ecx)==1
 * && ebx==ecx BEFORE its RESADD site at 1F9DAF; if [S99D0] fires but
 * [RESADD] stays 0 the refusal is the stall gate. ebx/ecx carry the
 * job flag bitmasks. */
extern void sub_001F99D0_gen(void);

void sub_001F99D0(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && n < 300) {
        void *ra = __builtin_extract_return_addr(__builtin_return_address(0));
        fprintf(stderr, "  [S99D0] x%u ra=%p ebx=%08X ecx=%08X\n",
                n, ra, g_ebx, g_ecx);
        fflush(stderr);
    }
    n++;
    sub_001F99D0_gen();
    if (on && n < 300) {
        fprintf(stderr, "  [S99D0] x%u -> eax=%08X\n", n, g_eax);
        fflush(stderr);
    }
}

extern void sub_001F9720_gen(void);

void sub_001F9720(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (n < 30 || (n % 64) == 0)) {
        void *ra = __builtin_extract_return_addr(__builtin_return_address(0));
        fprintf(stderr, "  [S9720] x%u ra=%p ebx=%08X edx=%08X\n",
                n, ra, g_ebx, g_edx);
        fflush(stderr);
    }
    n++;
    sub_001F9720_gen();
}

extern void sub_001EF4C0_gen(void);

void sub_001EF4C0(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (n < 20 || (n % 256) == 0)) {
        fprintf(stderr, "  [RESREL] x%u eax=%08X\n", n, g_eax);
        fflush(stderr);
    }
    n++;
    sub_001EF4C0_gen();
}

extern void sub_001EF510_gen(void);

void sub_001EF510(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && (n < 20 || (n % 256) == 0)) {
        fprintf(stderr, "  [RESADD] x%u eax=%08X ecx=%08X\n", n, g_eax, g_ecx);
        fflush(stderr);
    }
    n++;
    sub_001EF510_gen();
}

/* H31 master switch: sub_001EE7D0 (load-pipeline driver, 60+ call
 * sites) returns immediately when guest global byte 0x397110 == 0.
 * sub_001E0210 (engine frame driver) zeroes that flag on entry and
 * restores it on its normal exit -- but its EARLY exit at 0x001E0387
 * (taken when sub_002D2340 returns != 0) leaves the flag 0: the whole
 * pipeline + pump + managers go silent, exactly what frz101/103/104
 * census shows. [E0210] logs flag transitions and ret; [POOL] logs
 * sub_002D2340 args/return + pool globals 0x2E47E4/0x2E47E8; [PALLOC]
 * logs sub_002D85D0's signed status. */
extern void sub_001E0210_gen(void);

void sub_001E0210(void)
{
    static int on = -1;
    static unsigned n = 0, dead = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    uint32_t f0 = MEM8(0x397110);
    sub_001E0210_gen();
    uint32_t f1 = MEM8(0x397110);
    if (on) {
        n++;
        if (n < 20 || f1 != 1) {
            if (f1 != 1 && ++dead % 300 == 1)
                fprintf(stderr, "  [E0210] x%u flag %u->%u ret=%u (dead x%u)\n",
                        n, f0, f1, g_eax, dead);
            else if (f1 == 1 && n < 20)
                fprintf(stderr, "  [E0210] x%u ok flag=%u ret=%u\n", n, f1, g_eax);
            if (f1 != 1) fflush(stderr);
        }
    }
}

extern void sub_002D2340_gen(void);

void sub_002D2340(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on)
        fprintf(stderr, "  [POOL] x%u eax=%08X ecx=%08X cap=%08X blk=%08X\n",
                n, g_eax, g_ecx, MEM32(0x2E47E8), MEM32(0x2E47E4));
    sub_002D2340_gen();
    if (on) {
        int32_t r = (int32_t)g_eax;
        if (n < 20 || r < 0)
            fprintf(stderr, "  [POOL] x%u -> ret=%d (%08X)\n", n, r, g_eax);
        fflush(stderr);
        n++;
    }
}

extern void sub_002D85D0_gen(void);

void sub_002D85D0(void)
{
    static int on = -1;
    static unsigned n = 0;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    sub_002D85D0_gen();
    if (on) {
        int32_t r = (int32_t)g_eax;
        if (n < 20 || r < 0) {
            fprintf(stderr, "  [PALLOC] x%u -> %d (%08X)\n", n, r, g_eax);
            fflush(stderr);
        }
        n++;
    }
}

/* Decision points inside the tick/pump pair (frz103: last pump ever
 * = mgr 018CF100 id=5678 st=00970002; tick's bypass branch pumps only
 * when sub_001ECC40(job lookup) reports state&3 == 0). [JOB] logs
 * (id -> job, job[0] state); [HANDLE] logs the async-handle lookup
 * sub_001E1750(handle) the pump uses for obj+0x04. Exact counts,
 * printed on (arg,result) transitions + first 20 + every 200. */
extern void sub_001ECC40_gen(void);

void sub_001ECC40(void)
{
    static int on = -1;
    static unsigned n = 0;
    static uint32_t pid = 0xDEAD, pres = 0;
    extern __thread uint32_t g_esp;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    uint32_t id = (g_esp >= 0x10000u && g_esp < 0x7FFF0000u)
                      ? MEM32(g_esp + 4) : 0xDEADu;
    sub_001ECC40_gen();
    if (on) {
        uint32_t job = g_eax;
        uint32_t st = (job >= 0x01000000u && job < 0x04000000u)
                          ? MEM32(job) : 0u;
        if (n < 20 || (id != pid || job != pres) || (n % 200) == 0)
            fprintf(stderr, "  [JOB] x%u id=%08X -> job=%08X st=%08X\n",
                    n, id, job, st);
        if (n < 20 || id != pid || job != pres) fflush(stderr);
        pid = id; pres = job; n++;
    }
}

extern void sub_001E1750_gen(void);

void sub_001E1750(void)
{
    static int on = -1;
    static unsigned n = 0;
    static uint32_t ph = 0xDEAD, pres = 0;
    extern __thread uint32_t g_esp;
    if (on < 0) {
        const char *e = getenv("RECOMP_RD_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    uint32_t h = (g_esp >= 0x10000u && g_esp < 0x7FFF0000u)
                     ? MEM32(g_esp + 4) : 0xDEADu;
    sub_001E1750_gen();
    if (on) {
        if (n < 20 || (h != ph || g_eax != pres) || (n % 200) == 0)
            fprintf(stderr, "  [HANDLE] x%u h=%08X -> %08X\n", n, h, g_eax);
        if (n < 20 || h != ph || g_eax != pres) fflush(stderr);
        ph = h; pres = g_eax; n++;
    }
}
