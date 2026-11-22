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
    extern void nv2a_pb_swmethod_seen(void);
    static int on = -1, budget = 400;
    /* This pump reads the PGRAPH words: the latched mark, if any, is
     * now visible to the guest's service chain (patch 0026 hold rule). */
    nv2a_pb_swmethod_seen();
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
    static int on = -1, budget = 400;
    if (on < 0) {
        const char *e = getenv("RECOMP_GSVC_TRACE");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (on && budget > 0) {
        budget--;
        fprintf(stderr, "[SWM] obj=%08X param=%08X class=%u\n",
                g_ecx, g_eax, (unsigned)((g_eax & 0x1F) - 1));
        fflush(stderr);
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
    extern void nv2a_pb_swmethod_seen(void);
    static int on = -1, seen = 0;
    uint32_t pmc = MEM32(0xFD000100);
    /* The DPC body reads the gate/PGRAPH words: service visibility. */
    nv2a_pb_swmethod_seen();
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
