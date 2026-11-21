# Note 0002 — watchdog dump: missing `boot.baf`, and a poll at APU MMIO

Date: 2026-11-21. State: lift complete (note 0001); ENTRY evidence in
CHANGELOG. Run: `scripts/run.sh 30 RECOMP_WATCHDOG_SECS=18` →
`logs/run-20261121-015311.log` (gitignored), guest aborted by watchdog
(no exit after 18 s).

## Observed

Watchdog context:

- `[WATCHDOG] no exit after 18s; guest esp=0x00F7FD54`,
  `icalls so far: 8967`, kernel calls 7,845.
- recent ICALL targets: **sixteen consecutive `FE0000B4`** — a tight loop
  jumping into the APU MMIO window (0xFE000000 + 0xB4).
- `NV2A USER PUT=00065B28 GET=00065B28` — the guest *has* submitted a
  pushbuffer (display init ran); PFIFO REF/SUBR all zero.
- guest stack holds `0x0026D789` among the return addrs — .text, right
  past entry, consistent with the same startup region as before
  (`0x0026D1F5`, `0x0026D218`).

File path (the loud one):

```
[FILE] z:\boot.baf (disp 1 opts 0x4C from 0x0027158E) -> 0x00000000
[FILE] D:\boot.Baf (disp 1) -> 0xC0000034 FAILED (win32 err=2 ERROR_FILE_NOT_FOUND)
[FILE] missing D:\boot.Baf -> game/boot.Baf
[FILE] missing at watchdog: 1 high, 0 low
```

`boot.baf` exists on the disc (679,936 bytes — seen in the `tools.xiso ls`
listing during session 1). `game/` only holds `default.xbe`, because
session 1 extracted just the XBE. Ordinal census unchanged from first run:
277/294 (RtlEnter/LeaveCriticalSection) dominate, 234 (NtWaitForSingleObjectEx) x39.

## Hypothesis

**H2 (now primary):** the boot thread's file open failed with
FILE_NOT_FOUND, and the startup waits/retries on it (the NtWait loop)
while other threads poll audio hardware. Providing the disc's file tree
under `game/` is the correct host-side completion — a real machine finds
`boot.baf`; nothing about the guest is wrong. Falsifier: after extracting
the full tree, the wait loop and ordinal census are unchanged.

**H1 (threads, still live but demoted):** unchanged evidence; the process
does not silently exit, so no dispatch-entry wall is proven yet. Revisit
if the census shows a thread never starting after the file fix.

Side observation worth keeping: the FE0000B4 poll predates any audio
env switches; `RECOMP_AC97_READY` + the APU model is the toolkit's
documented answer *if* it stays stuck after files are present. One
variable at a time — not set this round.

## Planned change

Host-side data only: `python3 -m tools.xiso unpack <iso> -o game/` (full
disc tree under gitignored `game/`). No code changes, no seeds, no
overrides. Re-run with identical watchdog settings and diff the log.

## Outcome

Filled by note 0003: `boot.baf` opens fine after extraction (H2's strong
form falsified) — but the process still idles, now in a *different* state:
write loop + `NtWriteFile`-thunk polling instead of the `NtWait`-thunk
poll. The tree extraction was real progress (state moved, reads of
boot.Baf now run) and is documented in note 0003.
