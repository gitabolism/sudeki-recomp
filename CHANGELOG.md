# Changelog

Working log for the Sudeki static recompile. Stages and their proof are
defined in `BRIEF.md` ("Definition of progress"). Details live in
`docs/notes/`.

## 2026-11-21 — Stage reached: LIFT (and ENTRY evidence)

Setup:

- Upstream toolkit: `xboxrecomp` pinned at tag **v0.18.0**, commit
  `37b61d589168adee3680fabd51805ee81427bf07` (recorded in `THIRD_PARTY.md`;
  the clone is gitignored, fetched not vendored).
- Disc verified first: `Sudeki (World) (En,Fr,De,Es,It).iso`,
  3,812,491,264 bytes; `game/default.xbe` SHA-1 `24e532b1...` and SHA-256
  `1e28c3db...` both match the brief exactly before anything ran.
- Linux host target built: GCC 16.2.1, SDL2 2.32.74, libepoxy 1.5.10,
  OpenGL 3.3 back end. Toolkit libraries build standalone; the game
  project (template adapted for POSIX hosts in `CMakeLists.txt` +
  `src/main.c`) links `build/sudeki`, a 51 MB executable.

**LIFT — proof:**

- Generated tree exists: `src/recomp/gen/`, 63 files, 76 MB — 59 chunks at
  `--split 250`, plus `recomp_dispatch.c`, `recomp_stubs_unresolved.c`,
  `recomp_funcs.h`, `recomp_types.h`. (Gitignored; regenerate from the XBE
  alone, per the rules.)
- `tools.recomp`: 14,741 / 14,741 functions translated, 0 failed,
  1,975,265 lines.
- `/* mnemonic */` stub count recorded: **432** `RECOMP_UNIMPL` sites —
  outsb 135, outsd 78, insd 60, insb 59, arpl 57, daa 16, bound 14, cli 3,
  sti 3, das 2, sgdt 1, salc 1, outsw 1, ljmp 1, hlt 1. Generated markers
  and the lifter's tally agree exactly. Most sit in XDK data the linear
  sweep read over; they are runtime-logged, not silent.
- Disassembly (v0.18.0): 14,741 functions / 1,125,397 instructions /
  0 rejected — ~4.6% fewer functions than the brief's scratch number;
  input hashes match, so the difference is the detector version. See
  `docs/notes/0001-first-lift.md`.
- Stronger than the stage requires: the generated tree compiles and links
  on this Linux host with zero errors.

**ENTRY — evidence, not yet fully verified:**

The first smoke run (20 s, then killed by timeout — *not* a crash) shows
the guest running its own startup: kernel calls' `ret=` values are inside
`.text` immediately past the entry point `0x0026C703` (e.g.
`ret=0x0026D1F5`), and the trace carries real ordinals — 7,830+ calls by
the log's last summary: `RtlEnterCriticalSection` x3553,
`RtlLeaveCriticalSection` x3551, `RtlInitAnsiString` x236,
`RtlEqualString` x215, `NtAllocateVirtualMemory` x39,
`NtWaitForSingleObjectEx` x39. At timeout it was alive and looping in a
wait, not dead. Per the brief's ranked risks, the Threads stage comes
next (thread start routines invisible to static analysis), and no
reference capture exists yet, so nothing here is claimed beyond ENTRY's
own proof.

Not done / open:

- Threads stage: find the `PsCreateSystemThreadEx` start routines and get
  them dispatch entries.
- Reference baseline captures from xemu/cxbx for the first scene, before
  any behaviour is judged wrong (rule 2).
- The two unresolved call stubs.
