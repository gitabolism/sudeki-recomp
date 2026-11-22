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
