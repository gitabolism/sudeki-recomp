# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-21 (session 4f)
**Stage:** LIFT complete, ENTRY evidence strong. **Decoded the boot
archive end-to-end for the first time (note 0010, post-script 3):**
with a diagnostic +0x8000 file-size padding, `Z:\boot.baf` came out at
exactly 1,820,672 B **byte-identical** to the offline golden decode; no
flood, no crash. The stream is the whole file [0x800, 0xA6000) with END
at 0xA5E0D (note 0009 was right; an earlier "padding" reading this
session was falsified by the experiment). Wall now = the game's stream
byte-accounting starves reads **exactly one 0x8000 segment** short of
the END token (phantom refill charge `budget -= [+0x3C]` on the
consume path) under our/xemu kernel answers — find the answer
hardware gives differently, host-side. Beyond that: events
`0x48000001/04/06` still never signalled post-decode (next layer).

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

Padded diagnostic: `scripts/run.sh 45 RECOMP_WATCHDOG_SECS=40
RECOMP_KERNEL_LOG_BUDGET=0 RECOMP_EVENT_LOG=1` with `game/boot.baf`
temporarily +0x8000 (reverted; sha `b6e5751c…` confirmed)
→ `logs/run-20261121-175351.log`: **HANG (post-decode idle)** — decode
completed byte-exact, cache 1.8 MB, no crash; main polls
`0x48000004`, worker ctx `0x00021AD0` waits INFINITE on {1,6}; only
`0x48000002` ever set. Prior cap-off run
(`logs/run-20261121-175151.log`) HANG 4.1 GB: no natural termination
when reads are starved.

## Solved / established this session (note 0010)

1. **Stream boundary** *(superseded by item 7 below — tail is real
   stream)*: reads/prefetch/budget close at 0x9E000 under our kernel
   answers; disc XDVFS entry size 0xA6000, LBA 0x4BFD; guest output
   matches the offline decoder byte-exact to output 0x16BAF6 (where the
   starved runs diverge).
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
7. **CORRECTION (post-script 3 of note 0010, decisive):** padding
   `game/boot.baf` +0x8000 (diagnostic, since reverted — sha back to
   `b6e5751c…`) made the decode **complete**: `Z:\boot.baf` = 1,820,672
   B, `cmp` byte-exact vs golden; reads covered the whole file
   (final `@657408 want=22528`). The 0x9E000 read-stop is the game's
   accounting being exactly 0x8000 short (phantom charge), not a file
   boundary; the tail is real stream. Post-decode the title still
   waits on unsignalled events `0x48000001/04/06` — the next wall.

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
2. **H15 (new lead): the hardware read path must reach file end
   (0xA6000) and the END token at 0xA5E0D** — then output == header
   exactly (note 0009's exact-size result is the hardware path, not a
   coincidence). Our budget starves exactly 0x8000 short via one
   phantom refill charge (double-charge structure at 0x2236F/0x223D1,
   `budget -= [+0x3C]` on both the consume and post-read paths). The
   question narrows to: which host-visible answer (read return,
   position write, IoStatus.Information) makes the game's own
   accounting stop charging early?
3. **H13b/H13c dead**: the xemu HDD image holds only an erased
   `boot.baf` cache-dir entry; xemu runs the title from the same ISO
   XDVFS (size 0xA6000) we do — hence byte-identical read totals.
   Unlimited-cap run (cache cap off): the decoder writes forever
   (4.1 GB @ 45 s) — no natural termination exists on this geometry.
4. H9 (FATX mount/geometry) still queued behind these.

## Queue (next concrete actions)

1. **Event-creation trace (patch 0014):** log `NtCreateEvent`/
   `NtOpenEvent` with token + caller VA; re-run the padded diagnostic
   (revert file after!) and map who creates `0x48000001/04/06` and
   which code path should signal them post-decode → frame the next wall.
2. **Phantom 0x8000 hunt (static):** full disasm of `sub_00022340`
   (0x22340–0x223F0 captured in note 0010) + find every `call 0x22160`
   site (capstone scan for E8 rel32) — the decoder object carries
   events 0x48000009/0A at +0x8/+0xC and T=0x8000 (size arg 0x18000),
   so its construction site is not the 0x1f328 one; reconcile the
   refill double-charge with the observed slot history. Question:
   which kernel-visible quantity feeds the off-by-one-segment math.
3. If no kernel-answer explanation survives: presenting the file with
   +0x8000 (or size-0xA6000-with-slack semantics) is an
   architectural/legally-tinted decision → STOP and ask per gate.
4. Keep the offline tool as the oracle: `--eof 0xA6000` is now the
   correct clamp (whole file); golden = 1,820,672 B confirmed by guest.
5. Cleanup between runs: `rm -f ~/.local/share/xboxrecomp/Cache/*`
   (cap-off runs write GBs).

## Blockers / open questions

- Which kernel-visible answer produces the phantom 0x8000 charge in
  the game's refill accounting (us and xemu both starve reads one
  segment short; hardware evidently doesn't). Static CFG + call-site
  hunt queued.
- Post-decode events `0x48000001/04/06`: who creates them, who should
  signal them, and what stage they gate (event-creation trace queued).
- Whether the 786 MB of `Z:\boot.baf` written before disk-full is even
  correct behaviour — now largely explained (starved decoder eating
  ring residue), but the verdict stays reserved until the pipeline
  completes against the reference.

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
| 2026-11-21 s4f | **boot archive decoded end-to-end (note 0010)**: padded-size diagnostic (+0x8000, reverted) → `Z:\boot.baf` = 1,820,672 B byte-exact vs golden, no flood/crash; wall = game's byte accounting starves reads exactly one 0x8000 segment (phantom refill charge) under our+xemu kernel answers; QINFO patch 0011 (size answers correct), watch fixes 0012/0013, core final-state recipe (`obj = esp+0x78`), tool `--eof/--tokens`, xemu-HDD H13b dead, cap-off run proves no natural termination (4.1 GB/45 s); next: events 0x48000001/04/06 unsignalled post-decode | (this commit) |
