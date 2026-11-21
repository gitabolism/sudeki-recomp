# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-21 (session 4f)
**Stage:** LIFT complete, ENTRY evidence strong. Wall refined hard (note
0010): the boot-archive **data side is solved** — the real LZ77 stream is
file [0x800, 0x9E000) (decoded ≈ 1,489,655 B); the disc file's last
30,720 B (0x9E000..0xA6000) are **padding**, and note 0009's
"END@0xA5E0D / output == header" result was a padding false-positive.
The guest (and xemu, identically) stops reads at 0x9E000 with a closed
budget, the decoder has **no EOF termination** (only the dist==0 END
token exits), so it consumes ring residue/stale heap past the valid end
→ garbage output inflation → writer floods the cache → output cursor
overruns the contiguous window → CRASH (xemu: same overrun, STOP 0x7E).
Classification with default env: **CRASH** (deterministic, rip in
`sub_000223F0`); the 1 GiB cache-cap experiment rules out disk-full as
the trigger.

## Where things are verified

- Toolkit: `xboxrecomp/` @ v0.18.0 / `37b61d589168adee3680fabd51805ee81427bf07`
  (`scripts/doctor.sh` green). Checkout kept at the pin; local changes are
  patch files under `patches/xboxrecomp/`, and `scripts/build.sh` now
  resets `src/` to the pin and re-applies the series **in order** every
  build (the old per-patch idempotency check broke once patch 0002
  touched patch 0001's lines).
- Patches: `0001` `[WRITE]` trace, `0002` `[EVENT]` trace, `0003` write
  CSV + backtrace, `0004` FSCTL probe, **`0005` cache-volume capacity =
  the write-loop fix**, `0006` span placement insurance, `0007`
  `RECOMP_CONTIG_ALIAS=1` aperture probe (default off), `0008` `[READ]
  buf=` VA, `0009` `RECOMP_IO_WRITE_DUMP` content dump, `0010` `[READ]
  sz=/path=`, `0011` `[QINFO]` size-query trace, `0012`/`0013`
  `RECOMP_WATCH` arm fixes (errno, multi-page window). Table in
  THIRD_PARTY.md.
- XBE: pinned SHA-256 ✔. Gen tree unchanged since session 1 (432 stubs).
- Skills: five pinned, digest gate green.

## Last run

`scripts/run.sh 60 RECOMP_WATCHDOG_SECS=55 RECOMP_KERNEL_LOG_BUDGET=200
RECOMP_CACHE_CAP_BYTES=1073741824 RECOMP_EVENT_LOG=1`
→ `logs/run-20261121-174222.log`: **CRASH** — same decoder overrun
(rip = `sub_000223F0`+0), launch events still never signalled. Large
cache cap does NOT change the failure → disk-full is not the trigger.

## Solved / established this session (note 0010)

1. **Stream boundary**: real LZ77 stream = file [0x800, 0x9E000); tail
   0x9E000..0xA6000 is padding inside the recorded file (XDVFS entry
   size 0xA6000, LBA 0x4BFD). Guest output matches the offline decoder
   byte-exact to output 0x16BAF6 (input ≈ 0x9E000), i.e. through the
   whole valid stream.
2. **Read/budget accounting closes**: budget init 0xA5800
   (filesize−0x800), 3×0x8000 prefetch, chunk ops to zero; 21 file reads
   (header + 19×0x8000 + 0x5800) end at file 0x9E000. One 0x8000 budget
   charge has no `[READ]` (suspect the flag-2 refill branch at 0x2236F,
   `budget -= [+0x3C]`). **QINFO (patch 0011) proves the kernel size
   answer was correct** (`eof=0xA6000`) — the read-stop is game logic,
   byte-identical to xemu's STOP RAM dump (same quartet: budget
   0xA5800-region, total-read 0x9E000, chunk 0x5800, ret `0x1F335`).
3. **Decoder has no EOF exit** (disasm: only the dist==0 token returns,
   `je 0x226ED`; refill at budget==0 returns without data and the byte
   iterator reads past `+0x14` into ring residue). Post-EOF garbage
   re-consumes the guest's own output (the first divergent literals sit
   inside the contiguous arena, not in boot.baf). Both worlds overrun
   the output window identically.
4. **1 GiB cache cap → same CRASH**: the disk-full flag amplifies but
   does not trigger the overrun.
5. Tools: `baf_decode_offline.py` gained `--eof N` clamp and
   `--tokens FILE` token trace; docstring records the padding fact.
6. Patches 0011 `[QINFO]`, 0012/0013 `RECOMP_WATCH` fixes committed;
   very wide watch windows destabilize the run — keep windows small and
   pick the stack parity (0x02681/0x02691/0x02692 pages) from a probe
   run first.

## Established earlier (notes 0005-0009)

1. The write loop was the game's cache writer; cache capacity (patch
   0005) ends it the way hardware does. Commit `35cb8a4`.
2. H7 (FSCTL mount) falsified; `0x90020` stub is a real gap but not the wall.
3. Window placement: nondeterministic top-of-address-space spans break
   all five high windows at once (x86-64 canonical limit) — patch 0006
   re-places the span low. But **mapping is not the wall** (H8/H11
   dead): with everything mapped, the guest still lands at `0x84000000`.
4. `sub_000223F0` is a **bitstream decoder**, not a copy: `boot.baf` =
   679,936 B compressed, header `0x001BC800` (1,820,672 B uncompressed).
   Reads are byte-exact vs disc (verified `game/boot.baf`), one pass;
   writes of 61,440-byte chunks continue unboundedly → **the decoder
   never sees end-of-input** → worker #2 never finishes → the two
   launch events never get signalled → HANG. That is the wall.
5. **Post-mortem core analysis works** with this runtime (unlike live
   gdb/ptrace): `coredumpctl dump <pid> -o /tmp/opencode/sudeki.core` +
   `gdb build/sudeki core` gives host backtraces, the exact faulting
   instruction, and guest memory via `g_xbox_mem_offset`. Use it.
6. Thread-stack bases shift between runs (racy) — `RECOMP_WATCH`
   pre-arming on stack slots can't be done reliably; watch heap/object
   addresses instead (verify stability first).
7. **xemu reference baseline is installed and boots the title to our wall**
   (note 0009, THIRD_PARTY.md "Reference emulator"): xemu v0.8.139 AppImage
   + clean-room SpiritBoot-release + xemu-dashboard HDD; only `release`
   image with `-m 128` gets past BIOS. Screen shows `STOP 0x7E
   (0xC0000005, EIP 0x000226D8, 0x84FB8B1C, 0x84FB8820)` — fault EIP is
   inside `sub_000223F0`'s byte-output loop (verified by capstone on the
   disc: `0x226d8 mov [eax],bl` with flush `call 0x22270` at 0x226d0),
   dest cursor past mapped RAM. Both environments mis-signal the stream
   end; a clean title-screen reference needs correct drive/memory
   geometry on the xemu side too. VRAM contention with the inference
   engine ruled out (xemu uses ~80 MiB; screen renders fine alongside).
   Scripts: `xemu_ref.sh`, `xemu_kill.sh`, `xemu_boot_probe.sh`,
   `xemu_standalone.sh`.

## Live hypotheses

1. **H14 (primary, note 0010): token-level divergence at the stream
   EOF.** Guest and offline parser agree through the entire valid stream
   (common output prefix ends exactly at input 0x9E000 = first padding
   byte); then the guest consumes non-file bytes (ring residue / stale
   heap) and diverges. The exact token mechanism (tag-bit carry across
   the valid end? the phantom 0x8000 budget charge?) needs a guest
   token trace.
2. **H13c (platform geometry): hardware may see a different `boot.baf`
   size** (e.g. a re-packed archive whose payload ends where the stream
   does, with a real END token). Disc says 0xA6000 and both worlds agree
   on every observed byte, so this is now the *reference-side* question:
   check the SpiritBoot HDD image's own `boot.baf` entry/size on the
   xemu side (cheap image parse, no run needed).
3. **H13a dead in its cheap form**: 1 GiB cache cap changes nothing;
   disk-full flag is an amplifier only.
4. H9 (FATX mount/geometry) still queued behind these.

## Queue (next concrete actions)

1. **gdb token trace of the decoder around the divergence** (recipe in
   note 0010): conditional breakpoints at guest 0x226C0 / 0x226A0 /
   0x22635 (host = `sub_000223F0` + offsets), condition on the output
   cursor field `[rbp-0xA4]+0x18` in the window around
   `0x80004000+0x16A000`; capture ~200 tokens, diff against
   `tools/baf_decode_offline.py --tokens` (clamped stream).
2. Same probe run also settles the phantom charge: 16-byte
   `RECOMP_WATCH` at the object's `+0x3C/+0x40` slots using the parity
   seen in that run's own crash esp (object = esp+0x58 at the fault).
3. xemu-side cheap check: parse the SpiritBoot HDD image for its
   `boot.baf` entry (size/LBA) — if its payload is exactly 0x9D800,
   H13b/H13c collapse into "the game reads its own repacked archive;
   hardware's D: copy must end at the stream".
4. If token trace shows a lift divergence (not shared with xemu):
   fix host-side per rules; if shared: the answer is in file geometry,
   and the next lever is providing `D:\boot.Baf` as payload-only
   (0x9E800 B) as an experiment — host-side file choice, no code
   change, and legitimate (both are disc-derived slices; try in logs
   only, verdict needs reference).
5. Cleanup between runs: `rm -f ~/.local/share/xboxrecomp/Cache/*`.

## Blockers / open questions

- Token-level mechanism of the H14 divergence (gdb trace queued).
- The tension to resolve: hardware boots, yet both observed environments
  stop reading at 0x9E000 with no END token in reach. Either (a) the
  phantom 0x8000 budget charge is an artefact of a kernel answer both
  xemu and us give (if hardware's reads reach 0xA6000, the stream ends
  cleanly at END@0xA5E0D with output == header — note 0009's exact-size
  result gains meaning again), or (b) the game genuinely handles the
  residue/EOF region differently based on data we have not compared.
  The object watch on `+0x3C/+0x40` plus the token trace decide.
- Whether the 786 MB of `Z:\boot.baf` written before disk-full is even
  correct behaviour — unanswerable without the reference capture (rule 6).

## Housekeeping facts

- Harness: `scripts/{doctor,pipeline,build,run,stubs,new_note,env,skills_check,gdb_probe}.sh`;
  `build.sh` resets toolkit `src/` to the pin, then applies `patches/xboxrecomp/*.patch` in order.
- Logs → `logs/`, captures → `reference/` (gitignored). Cores →
  systemd (`coredumpctl`), dump to `/tmp/opencode/` (~2.1 GB each).
- The run writes up to the cache cap (~786 MB) into
  `~/.local/share/xboxrecomp/Cache/` per run — `rm -f` that dir between
  long runs.
- Useful env: `RECOMP_MISSING_FILES=all`, `RECOMP_IO_WRITE_LOG=1`,
  `RECOMP_IO_WRITE_CSV=<path>`, `RECOMP_EVENT_LOG=1`, `RECOMP_FSCTL_OK=1`,
  `RECOMP_CACHE_CAP_BYTES=<n>`, `RECOMP_CONTIG_ALIAS=1`,
  `RECOMP_WATCH=<guest-va>` + `RECOMP_WATCH_RAW=1`,
  `RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_WATCHDOG_SECS`.
- OpenCode layer: `opencode.jsonc` + commands `/resume /status /run /cycle
  /note /lift /handoff /setup`; skills under `.opencode/skills/` (pinned).
- Remote origin push URL is `NO_PUSH`. Do not "fix" this.

## Session log

| date | what happened | commit |
|---|---|---|
| 2026-11-21 s1 | pinned v0.18.0, hashes verified, full pipeline, lift 14,741 fn / 432 stubs, Linux exe builds, first run alive at timeout | `eca6f9f` |
| 2026-11-21 s2 | autonomy layer (AGENTS/STATE/scripts/notes); full disc tree extracted (notes 0002-0003); wall framed as XAPILIB read/write/wait cluster | `55b8693` |
| 2026-11-21 s3 | skills pinned + drift gate; note 0004: read completes, wall = Partition5 FATX bootstrap retry loop; [WRITE] patch | `17aa020` |
| 2026-11-21 s4a | note 0005: write loop is the game's cache writer; H7 falsified; **patch 0005 cache capacity → loop ends (HANG→CRASH)**; instrumentation patches 0002-0004 | `35cb8a4` |
| 2026-11-21 s4b | notes 0006-0007: placement patch 0006 (canonical-limit), contig-alias probe 0007; core-dump analysis pins the fault to the boot-archive decoder; **wall = decoder never terminates → launch events never signalled (HANG)**; build.sh patch series made deterministic | `0f5068d` |
| 2026-11-21 s4c | note 0008 + patch 0008 (`[READ] buf=`): reads land in the decoder's own arena (H12 dead), output is valid decoded data (triplet→8-byte expansion), refill returns 0 on every path; watch pre-arming ruled out (stack jitter); SIGABRT-core recipe added | (this commit) |
| 2026-11-21 s4e | **xemu reference installed (user-approved) & boots the title to OUR wall**: SpiritBoot-release + `-m 128` → `STOP 0x7E` inside `sub_000223F0` byte loop (EIP 0x226D8, flush `call 0x22270`) — same decoder, same runaway cursor as note 0007; VRAM-forensics recipe (FB @0x3C00000, pitch 720) reconstructs the bugcheck screen as `reference/xemu/` artefact; VRAM contention with inference engine ruled out; note 0009 + `scripts/xemu_{ref,kill,boot_probe,standalone}.sh` + THIRD_PARTY reference table | (this commit) |
| 2026-11-21 s4f | **data side of the boot-archive wall solved (note 0010)**: real stream = [0x800, 0x9E000), tail 30,720 B = padding; END@0xA5E0D was a padding false-positive; guest == offline decoder byte-exact to output 0x16BAF6; QINFO (patch 0011) shows kernel size answers correct — 0x9E000 read-stop is game budget logic (identical in xemu STOP RAM); decoder has no EOF exit (only dist==0 END); ring reconstruction from core; 1 GiB cache cap → same CRASH (disk-full not the trigger); tool `--eof/--tokens` modes; patches 0011-0013 | (this commit) |
