# Note 0017 — TLS teardown poison after worker exit

Date: 2026-11-21. State: notes 0011 PS5 + 0016 (flip settle chain took the
title past the intro into world/level loading: `SOLWORLDM.gex`,
`z:\wavebanks\`, `D:\sound\BS_brightwater.xwb` — first level's audio).

## Observed

With the patch-0017 settle chain the game leaves the movie (flips
continue, `1.10 fps (256 flips)`), opens the first-level wavebank, and
**SIGSEGVs** inside the lifted XDK error-slot setter `sub_0026C79D`
(`logs/run-fd1-202904.out`, `[CRASH] … VA of fault: 0xFFFFFF6C, in
sub_0026C79D+0x111`). The same fault survives `RECOMP_FSCTL_OK=1`
(`logs/run-fsctl1-203441.out`) — the FSCTL answer is not the trigger.

`sub_0026C79D` (measured from the lift, host disassembly confirms):
`slot = [[fs:4] + tls_index*4]`, `slot[+8] = arg`, with the image's
tls_index at 0x376620 = **0xFFFFFFDB = −37** (148-byte TLS ⇒ matches the
toolkit's own loader arithmetic).

Probe wrappers + watches (tools added this session):

* `RECOMP_WATCH` could not arm at 0x4004 — the TIB page sits below
  `XBOX_BASE_ADDRESS` and `peek_readable` didn't list the low mapping
  (patch 0018 adds it). Armed: `[WATCH] [00004004] 00770094 -> 00F80094`
  at spawn #1 (`bridge_layout_tls`, correct), and the `-> 00000000`
  event lands *after* the crash print (teardown noise, not the trigger).
* `RECOMP_TLS_TRACE=1` wrapper on `sub_0026C79D` (`logs/run-tls1-203840.out`):
  461 calls, all `fs=00004000 arr=00F80094 slot=00F80000` (healthy),
  then a call with **`arr=00000000` → slot=FFFFFF6C** — fault.
* In the watch run the faulting thread instead had
  `[g_fs_base+4] = 0xC0000000` (registers: eax=0xFFFFFFDB the index,
  fault = arr + idx*4) — a **poison marker** in that thread's own TIB.

So two faces of one gap: after a thread's CRT/TLS teardown, the TLS
array word in its TIB holds garbage (`0xC0000000` / `0`), yet guest code
still executes with that `g_fs_base` — the error-slot setter is the
first thing to dereference it. On the tls1 core the affected thread is
the host main thread itself (inline game-main chain
`sub_0026D47C → PsCreateSystemThreadEx → sub_0026D3E4 → …`); on the
watch run it was a worker whose routine had returned. Worker spawns all
got their own TIB (no "shares the main thread's" message).

This is a toolkit thread-lifecycle/TLS emulation gap (which thread may
still touch TLS after teardown, and the `fs:[4]` dual use as StackBase
in `bridge_layout_tls:511` vs TLS-array in the loader). Not resolved
here. Falsified alternative: FSCTL 0x90020 answer (A/B'd).

## Planned change

Probe-level guard only (manual override, not a behavior fix): the
`sub_0026C79D` wrapper skips the store when the computed slot is not in
plausible RAM (arr==0 or ≥0xC0000000), logging once. On hardware the
TLS would still have been alive; the store is a SetLastError-class
write, so skipping on a torn-down TLS loses nothing the title could
have read back, and keeps the run alive to reach the next gap. The
proper fix is toolkit-side TLS lifetime; recorded as a follow-up.

## Outcome

The "TLS teardown poison" reading was WRONG. The watch+TF-step probe
(patches 0019/0020, guest-stack attribution + single instruction re-arm)
caught the writers directly:

* The TIB page stores are **DSOUND/XACT packet copies**: lifted
  `sub_0028EA91(this, off, src)` memcpy's to `*[[this+8]+0x10] + off`;
  `RECOMP_DS_COPY=1` (manual wrapper) shows exactly one poisoned call per
  run: `this=0 parent=0 M=0 *M=0 off=0x818 dst=0x818 src=<read buffer>
  ret=002893C0` — a **0x6168-byte wavebank chunk written to guest 0x818**,
  covering the exception page, the runtime TIB page (0x4000: fs:[0],
  fs:[4], StackBase/Limit, Self all zeroed by the chunk) and holes to
  0x6978. That is what made the next `sub_0026C79D` (SetLastError-class
  store) fault at `arr + (-37)*4 = 0xFFFFFF6C` in unguarded runs.
* `RECOMP_DS_VOICE=1` (wrapper on `sub_002854B3`) pins the single bad
  call: `this=0x029A00F4 slot14=0` — a voice/stream object in the game's
  `pool` arena (0x29A0000) whose child slot (+0x14) was never
  initialized; `sub_00289351` then runs with the null child and the copy
  helper resolves its mailbox chain to 0. So the wall is an
  **audio-engine initialization gap** (something earlier in this title's
  stream-voice setup failed under our kernel answers), not thread
  lifetime. The TLS guard stays as cheap insurance, but the real fix is
  finding why slot +0x14 was never set.
* Same subsystem also produces the surviving crash
  `sub_000B08F0+0xDFB2` writing through `esi=0x89D8458B` (garbage pointer
  in contiguous-window space, 0x80000000+ > 64 MB) — the game's own
  structures poisoned upstream of the null voice object.

Tools added: patch 0019 (watch prints guest-stack chain + host rip),
patch 0020 (TF single-step re-arm so every store in a block faults with
its own attribution; `RECOMP_WATCH_PANIC=N` cores at the Nth hit), manual
wrappers `sub_0028EA91` (RECOMP_DS_COPY) and `sub_002854B3`
(RECOMP_DS_VOICE) — `!`-marked lines are impossible addresses (below the
image), auto-logged even without the trace env.
