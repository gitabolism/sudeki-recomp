# Note 0001 — first lift of Sudeki, and where the brief's numbers move

Date: 2026-11-21. Base: upstream xboxrecomp `v0.18.0`
(commit `37b61d589168adee3680fabd51805ee81427bf07`), Linux host.

Inputs, all verified on this machine before anything was trusted:

- Disc: `Sudeki (World) (En,Fr,De,Es,It).iso`, 3,812,491,264 bytes — matches
  the brief.
- `game/default.xbe` extracted with `python3 -m tools.xiso get`; SHA-1
  `24e532b1aed6aed9412a089f5736fc3bdaa0fa38`, SHA-256
  `1e28c3db033a3de471ba78ccdd13a824ebddd17fdd0479bf69a795f63bd92419` — both
  match the brief exactly.
- `tools.xbe_parser` agrees with every pinned fact: Title ID `0x4D53000A`,
  entry `0x0026C703`, image size 0x416F80, 34 sections, 119 kernel imports,
  8 libraries incl. D3D8LTCG + XGRAPHCL, TLS zero-fill 144.

Measured pipeline numbers (no title-specific seeds):

| Measurement | Brief said | v0.18.0 here |
|---|---|---|
| Functions (disasm) | 15,441 (13,169 in .text) | **14,741** (12,499 in .text) |
| Instructions | 1,125,376 | **1,125,397** |
| Rejected call targets | 0 | **0** |

The function-count gap (700, ~4.6%) is a difference between whatever build
of the toolkit the brief was measured with and v0.18.0's detector, not a
difference in the input — hashes match. The brief is explicitly scratch
notes, so the v0.18.0 numbers are taken as the current truth and recorded
here; nothing downstream of the count changes.

Lift (step 5), `tools.recomp --all --split 250 --gen-dir src/recomp/gen`:

- 14,741 / 14,741 translated, 0 failed, 1,975,265 lines, ~43 s wall.
- Generated tree: 63 files in `src/recomp/gen/` (59 chunks +
  `recomp_stubs_unresolved.c`, `recomp_dispatch.c`, `recomp_funcs.h`,
  `recomp_types.h`), 76 MB. Gitignored, never committed.
- `RECOMP_UNIMPL` sites (`/* TODO: mnemonic */` in the generated C):
  **432**, per-mnemonic: outsb 135, outsd 78, insd 60, insb 59, arpl 57,
  daa 16, bound 14, cli 3, sti 3, das 2, sgdt 1, salc 1, outsw 1, ljmp 1,
  hlt 1. The generated count and the lifter's own summary agree exactly.
  This shape matches the JSRF story in the lifter's `_unimplemented`
  comment — port-I/O strings and 0F-prefixed oddities are what a linear
  sweep reads over XDK data (the .rdata kernel-thunk region 0x3156C0+
  supplies many `ins*/out*` sites), so most are expected to be dead. They
  are runtime-logged, not silent.
- Unresolved call stubs: 2.

Process notes:

- `abi_analysis` must run *after* `func_id` finishes (it reads
  `identified_functions.json`); running them in parallel fails.
- `tools.recomp` reads all three analysis outputs; order is xbe_parser →
  disasm → func_id → abi_analysis → recomp.
- The repo's `recomp_*.c` ignore pattern (aimed at generated chunks) also
  catches `src/recomp_manual.c`, the hand-written override layer. There is
  now a `!src/recomp_manual.c` negation; without it the project's one
  committed override file silently cannot be added.

Postscript — first smoke run of `build/sudeki` (Linux, 20 s timeout, log
kept local at `/tmp/opencode/sudeki_first_run.log`):

- Not a crash: killed by timeout while still alive.
- Guest reached its own startup: kernel-call `ret=` addresses sit in
  `.text` right past the entry point (first calls return to 0x0026D1F5,
  0x0026D218).
- 7,834 kernel calls: RtlEnter/LeaveCriticalSection dominate (x3553 /
  x3551, consistent with the brief's lock counts), plus RtlInitAnsiString,
  RtlEqualString, NtAllocateVirtualMemory, and NtWaitForSingleObjectEx
  x39 — the loop it was sitting in when the timeout fired.
- Next suspect, per the brief's ranked risk 2: thread start routines
  pushed as arguments to `PsCreateSystemThreadEx` get no dispatch entry.
  Note the process did *not* exit silently — it waits — so the thread
  creation path is probably reached; what is missing may be the routine
  it jumps to. Check that before debugging the wait itself.
