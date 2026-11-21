# Note 0004 — write-loop-lifted-c

Date: 2026-11-21 (session 3). Supersedes part of STATE.md's session-2 summary.

## Correction first (rule: facts before conclusions)

STATE.md said the loader "stalls after reading 657,408/679,936 bytes of
`boot.Baf`". That was a miscount. `logs/run-20261121-015639.log` line 320:

```
[READ] from=0x00271012 ev=00000000 apc=00000000 @624640 want=22528 got=22528 st=0x00000000
```

624,640 + 22,528 = **679,936 — boot.Baf is fully read**, every read `st=0`.
The reads are not the wall.

## Observed (same log)

- After the last read: a thread loops `NtWaitForSingleObjectEx
  token=0x48000004 handle=0x5a000024 timeout=finite` → STATUS_TIMEOUT (258)
  forever. That event was created at line 227:
  `NtCreateEvent: handle_ptr=0x00F7FD64 type=0 init=0 → handle=0x5A000024`
  — **manual-reset, initially unsignalled**, allocated on the XAPI launcher
  stack (cf. the other boot events' `handle_ptr=0x00F7FF40`). `0x48000004`
  is the guest-side object token; `0x5A000024` is the runtime's Win32-compat
  handle (tag `HTAB_TAG=0x5A000000`, win32_compat.c).
- Per-500 ms poll window, ~4,690 calls to ordinal 236 (`NtWriteFile`) run
  **between** the waits, all returning success (no `[READ]`-style write log
  exists — see gap below). Census at watchdog time: `236 x44,210+` and
  still climbing; `277 RtlEnterCriticalSection x3553` and `294
  RtlLeaveCriticalSection x3552` **plateaued** (loop holds no locks);
  `289 RtlInitAnsiString x237`, `279 RtlEqualString x215` — strings
  initialised/compared once per opened object, not per write.
- Write-site (guest): `sub_002717D6+0x272` = `loc_00271A48` calls
  `MEM32(0x315704)` = import idx 17 = ordinal 236 `NtWriteFile` with
  `ev=0 apc=0` (pushed 0,0,0 at `loc_00271A3x`) and an `offset_va` —
  synchronous, positioned write. Post-call at `loc_00271A4E`:
  `cmp eax, edi; jl → error path` — a **negative NTSTATUS would branch**;
  the loop therefore does not depend on failure of the write call itself.
- Reader is `sub_00270FBD` (read loop @0x271012, `ev=0`, synchronous),
  opener `sub_00271466` (path via `MEM32(0x315710)` call at 0x2714CB,
  strips trailing `\`), all in the XAPILIB boot cluster near entry 0x26C703.
- Toolkit already signals I/O completion events correctly:
  `bridge_complete_file_io` SetEvent's the event arg, and
  `xbox_NtWriteFile` (POSIX backend, kernel_file.c:1429) returns
  `STATUS_INVALID_HANDLE` for bad fds — no fabricating success. H3's
  "runtime never signals the completion event" is weakened: these calls
  pass `ev=0` and complete synchronously anyway.

## Hypothesis

**H5 (replaces H3 as primary):** the boot worker (`tid=1003`, stack top
`0x02682FF0` from `PsCreateSystemThreadEx #3`) is stuck **inside its own
task** in an unbounded write loop at `sub_002717D6`, so it never reaches
the `SetEvent` of the launch-complete event `0x5A000024` that the poller
thread waits on. The wall is the write loop, not the wait, and not I/O
completion. The decisive unknown: **which host file/device is the write
handle** — the toolkit logs reads but has no `[WRITE]`-style trace (gap).

Falsifier: if the writes target a legitimate destination and advance
offset/length monotonically (a slow but finite log/copy), then the worker
is not looping and the wall is elsewhere; if the target is a dev-only
device (debug monitor / badger plugin path) that real retail hardware
would lack, this is an HLE/device gap to fix host-side.

## Planned change (host-side only)

Probe (no source changes): run under gdb, breakpoint
`xbox_NtWriteFile` (T symbol, psABI args: rdi=FileHandle, rsi=Event,
rdx=ApcRoutine, stack @rsp+8..: IoStatusBlock, Buffer, Length,
ByteOffset), print `w32_handle_path($rdi)` + Length + ByteOffset for the
first 12 calls. `w32_handle_path` is callable from gdb (`T` symbol).
Run under `timeout 40s` (breakpoint overhead bounds output).

## Outcome (probe runs, 2026-11-21)

Tool: `[WRITE]` trace added host-side as `RECOMP_IO_WRITE_LOG=1`
(`patches/xboxrecomp/0001-io-write-trace.patch`, auto-applied by
`scripts/build.sh`); plus `RECOMP_MISSING_FILES=all` for every-open logging.
A gdb-breakpoint probe was tried first and **abandoned**: ptrace stops the
runtime's SIGSEGV-based SEH emulation (`sub_0026D079+0x137` crash under
gdb that never happens headless). Instrumentation beats ptrace here.

What became true (`logs/run-20261121-022602.log`, `-022812.log`):

1. It is a **file copy**: `boot.Baf` from `D:\` (DVD → game dir) into
   `Z:\boot.baf` (`~/.local/share/xboxrecomp/Cache/boot.baf`). XAPILIB
   bootstraps the utility drive (Partition5) at startup:
   `[FILE] \Device\Harddisk0\Partition5 (disp 1)`, `NtFsControlFile(0x90020) - stub`,
   then opens `z:\boot.baf (disp 3 = FILE_OPEN_IF)` and `D:\boot.Baf`.
2. Writes are **4096-byte positioned writes, all `st=0`**, and the
   destination **never stops growing**: samples every 10k writes show the
   offset advancing ~614,400,000 bytes per 10k writes; host let the file
   reach **4,295,024,640 bytes** (sparse) at watchdog time. Block count =
   1,048,580 blocks ≈ just over 4 GiB — suspiciously close to a 32-bit
   wrap (1,048,576), i.e. the guest likely *should* have stopped at
   FATX/partition capacity or hit a wrap condition that host success hid.
3. `NtFsControlFile` is a bare stub returning `STATUS_NOT_SUPPORTED`
   (kernel_bridge.c ~5318). The guest ran `0x90020` (FILE_ANY_ACCESS
   function 8) once before the copy. **H6 (new primary):** the copy loop
   is driven by a predicate that expects a real FATX volume answer the
   stub never gives — either capacity/geometry from the mount-time FSCTL,
   or a disk-full error that POSIX `write()` never returns. The 500 ms
   waits on event `0x5A000024` are a *poller* (session-2 fact: waits
   interleave with growing writes), not the wall.
4. Side effect worth knowing: the runaway sparse file was deleted at
   handoff; the toolkit cache dir is per-user (`~/.local/share/xboxrecomp/`),
   safe to clear any time. (User-side note: GPU pressure killed Kodi at
   02:29 the same evening — `EGL_BAD_ALLOC`, coincident with this
   diagnosis window; game runs never touch the GPU, but the CUDA engine
   for the session does.)

Next falsifier (queue): identify FSCTL 0x90020 (public XDK/XAPILIB
headers — GitHub code search needs auth, fetch raw header files instead),
then implement host-side against Partition5 image semantics, and/or make
the Partition5 image FATX-capacity-limited so writes fail like real
hardware. One variable at a time.
