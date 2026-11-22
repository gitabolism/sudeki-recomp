# 0019 — xemu playability: the STOP was our boot-ROM profile (closed)

**Observed (session 4l).** The user asked for a *playable* xemu baseline and
confirmed a fresh AUR `xemu-bin` behaves exactly like our pinned AppImage —
emulator binary/packaging eliminated. xemu 0.8.139 is the latest official
release. Disc verified Redump (symlink `sudeki-world.iso`), HDD qcow2
verified pristine by digest. Yet every boot reached `STOP 0x7E` inside the
boot-archive decoder (note 0009), while upstream compat reports (Jan 2026)
say the title's FMVs play fine and the known issues are in-game graphics
(#2039 vertex explosions) — nobody upstream reports a boot bugcheck.

**Research (decisive).** SpiritBoot's own tracker documents the failure
class: Def Jam FFNY, issue #75 — with the 128 MB SpiritBoot profiles the
guest kernel *reports 128 MiB* via `GlobalMemoryStatus`; 2004-era titles
derive huge `MmAllocateContiguousMemoryEx` requests from "available −
headroom", the allocator clamps to the 64 MiB title allocation domain,
returns NULL, the title writes through it → STOP 0x7E. A retail-derived
("Complex") BIOS exposing 64 MiB boots the same title at 64 MiB. SpiritBoot
couldn't do 64 MiB itself (issue #76: mandatory structures above 64 MiB),
which is why the *official v1.0/v1.1 releases* ship a **512 KiB 64 MB
profile** and an experimental `128MB-64MB-game-memory` profile ("128 MiB
physical backing, only 64 MiB exposed to titles").

Our firmware files (`optimized-xemu/SpiritBoot-release.bin`,
`SpiritBoot-checked.bin`, 256 KiB) were **unofficial dev builds** predating
that line — they require 128 MB and report 128 MB. Sudeki's boot allocator
strategy then matches the Def Jam mechanism exactly: our `STOP 0x7E` was a
boot-ROM profile artifact, not an xemu or game defect, and not the async-IO
convention (note 0011's "xemu answers wrongly" reading is superseded —
the xemu runs that reproduced our quartet were also running the 128 MB
profile).

**Fix (applied).** Downloaded official SpiritBoot v1.1.0
`SpiritBoot-v1.1.0-64MB.bin` (sha256 `75f0680d…`) and
`SpiritBoot-v1.1.0-128MB-64MB-game-memory.bin` (sha256 `c403570c…`),
digests verified against the release manifest; pointed
`flashrom_path` at the 64 MB profile (xemu `mem_limit` default 64).

**Result.** **The title boots on xemu**: user directly observed the Ailish
loading card and the intro movie playing (screenshots 23:3x); a parallel
headless `scripts/xemu_boot_probe.sh upstream 90 sb64` run produced
`/tmp/opencode/ram-sb64.elf` with `SOL.exe` resident in PC RAM. The user's
session then froze mid-movie with the window "Not Responding" — most
plausibly resource contention: my probe instance ran concurrently (plus
the inference engine holds ~23.9/24.5 GB VRAM, leaving ~660 MiB for
desktop + *two* xemu windows). Single-instance retest is the next data
point; if it freezes alone, capture QMP `info registers` +
`dump-guest-memory` at the freeze and decode against the v1.1.0 kernel
source (released in the same assets).

**Tooling corrections found this note:**
- `scripts/xemu_ref.sh` used QMP JSON against an `-monitor` (HMP) port —
  fixed to `-qmp`; `XEMU_MEM` env passthrough added.
- xemu's QEMU fork strips `screendump` from **both** HMP and QMP
  (query-commands: no screen/shot commands); `sendkey` reaches the guest,
  not xemu's UI hotkey. Only file-based capture path:
  `dump-guest-memory` + VRAM reconstruction (note 0009 recipe — offset
  0x3C00000/pitch 720 matched the OLD ROM layout; re-measure on the v1.1.0
  dump: scan candidates in the `pa=0xF0000000` PT_LOAD).
- xemu rewrites `xemu.toml` on exit (config edits can vanish; set keys,
  then kill with SIGINT and re-verify).

## Outcome

True as written: the 64 MB official profile removed the STOP completely;
game reaches FMV/movie stage on xemu. Freeze reproduction pending single-
instance retest. Reference side-by-side (Menu artefact, rule 6) is now
unblocked in principle; capture pipeline (FB offset re-measure) queued.
