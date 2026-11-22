# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-21 (session 4h)
**Stage:** LIFT complete, ENTRY strong. **Boot reaches the title's own
intro/movie and produces the first GPU flips.** The 0x9E000 read-stop and
post-decode stalls are fully explained (patch 0014 async-IO + measured
seeds). With the host fence mirror + `RECOMP_VBLANK=1` + `RECOMP_PB_EXEC=1`
the game flips at several Hz (640x480, surface `0x00828000`). Frames are
solid black — the XDK programmable-vertex path's methods are unhandled by
the toolkit FFP executor. New wall = GPU draw-method coverage (note 0011
post-script 5).

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
  `RECOMP_WATCH` arm fixes (errno, multi-page window), **`0014` async-IO
  return convention = the boot-archive fix**. Table in THIRD_PARTY.md.
  Manual overrides: `--exclude-manual src/recomp_manual.c` now wired into
  `scripts/pipeline.sh`; first wrapper = `sub_00022340` (stream refill) +
  `RECOMP_REFILL_TRACE=1`; gen tree emits the body as `sub_00022340_gen`,
  stubs still 432.
- XBE: pinned SHA-256 ✔. Gen tree regenerated session 4g with
  `--exclude-manual` (one body renamed `sub_00022340_gen`; 432 stubs).
- Skills: five pinned, digest gate green.

## Last run

Session 4h: `env RECOMP_VBLANK=1 RECOMP_PB_EXEC=1 RECOMP_PB_SCAN=1
RECOMP_FB_WINDOW=1 RECOMP_FB_DUMP=reference/fb/frame
RECOMP_FB_WINDOW_DUMP_EVERY=60 RECOMP_KERNEL_LOG_BUDGET=200
./build/sudeki` (fence mirror compiled into `src/main.c`) →
`logs/run-exec1-195554.out`: **PROGRESS** — game boots past the boot
archive, decodes `startup.baf`, opens `D:\movies\MS Game Studios
Logo-final.bik`, and **flips**: 7 frame dumps (`reference/fb/*.bmp`,
gitignored), 640x480, surface `0x00828000` pitch 2560, 234 clears,
several flips/s. All frames solid black: 8716 unhandled methods across
150 distinct (NV2A programmable-vertex path). `DECODE-EXACT` still holds.
Recipe that unblocked the post-decode wall:
`xbox_Nv2aMirrorFence(0x2E3638,0x2C,0x30)` (compiled) **+ VBLANK=1 +
PB_EXEC=1**. Default env (no PB_EXEC) still HANGS at `sub_002D3CC0`
waiting `ctx+0x8700→0` (the flip semaphore the inert executor never
writes).

## Solved this session (session 4h, note 0011 post-scripts 3-5)

1. **The post-decode "3-way deadlock" was a lift gap, not events:** the
   q2 task-completion notify `ICALL 0x0011C640` was unresolved (skipped).
   `0x11C5F0/0x11C610/0x11C640` are real func_id misses (int3-padded,
   tail-jmp `0x11D200`). Seeded the measured one → completion notify ran
   → token `0x48000004` set **for the first time** → main proceeded.
2. Second/third misses surfaced (`0x001E330`, `0x00273EEF`) — same
   seed-file mechanism; gen tree now has **zero unresolved ICALLs** at
   boot (`logs/seeds-icall-0011C640.json`, local per gitignore law;
   addresses recorded in note 0011).
3. **Boot reaches the movie:** `startup.baf`, `SOLXM.fubi`,
   `badger.plap` (FNT/FMH/PSH/SHX chunks), 26 MB VRAM arena,
   `D:\movies\MS Game Studios Logo-final.bik`.
4. **Flip wall solved:** XGRAPHCL flip-pending flag `ctx+0x8700` is
   GPU-written; runtime needs `RECOMP_PB_EXEC=1` (method executor, off
   by default upstream) + `RECOMP_VBLANK=1` (guest ISR at vector 3
   claimed) + our `xbox_Nv2aMirrorFence(0x2E3638,0x2C,0x30)` in
   `src/main.c`. First frame dumps exist (black).
5. Tools: poster wrappers `sub_00021CD0/sub_000220E0` +
   `RECOMP_POST_TRACE=1` (settled the task/event anatomy); PC-sampling
   via two timed cores distinguishes spin vs progress; watchdog
   regs/NV2A PUT-GET lines are a cheap first diagnostic.

## Solved earlier this session (4g, note 0011 core)

1. **The phantom 0x8000 = the sync-read return convention (patch 0014).**
   First manual-override wrapper (toolkit `--exclude-manual` contract,
   now in `pipeline.sh`; `RECOMP_REFILL_TRACE=1`) showed refill #1
   charging 0x8000 AND init (`sub_000221C0` @0x22215) charging it again —
   double-charge of the prefetch segment, only reachable when NtReadFile
   answers `STATUS_SUCCESS` synchronously on an **async** handle (open
   options `0x4C`: no FILE_SYNCHRONOUS bit; bridge already records such
   handles). Hardware answers `STATUS_PENDING`; the game's flag-2 consume
   path then charges once. Patch 0014 = upstream `RECOMP_ASYNC_IO`
   default-ON, `=0` opts out.
2. Consequence: boot archive decodes **byte-exact on the disc-exact file**
   with default env (44 refill calls vs 9,948; no flood, no crash). The
   note-0010 padded-file result was compensating errors, not a lever.
   H14/H15 closed; no file-slack decision needed (queue item 3 dropped).
3. xemu's identical 0x9E000 stop is xemu answering the same convention
   wrongly — consistent, not corroborating. Reference-comparison caveat
   updated in note 0011.

## Established in session 4f (note 0010)

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

1. **H14-H18 CLOSED:** async-IO return convention (patch 0014) + three
   measured seed lifts (`0x11C640/1E330/273EEF`) + host fence mirror
   (`src/main.c`) + `RECOMP_PB_EXEC=1` + `RECOMP_VBLANK=1` take the boot
   from the phantom-charge stall all the way to real flips and the title
   intro movie. Decode still byte-exact.
2. **H19 (NEW primary wall — GPU draw coverage):** frames flip but are
   solid black. Executor reports 8716 unhandled methods / 150 distinct —
   this XDK 5849 D3D build drives the NV2A programmable vertex path
   (VSH program/constant uploads, indexed vertex buffers, image-blit
   classes) that the toolkit's FFP executor doesn't implement. First
   move: dump the unhandled-method table (`nv2a_pb_exec_report`) and
   classify. Expect this is the Menu-stage work and a long tail.
3. **H20 (cheap, verify first):** confirm `0x48000004`-set boot tasks
   don't re-starve later subsystems; the ordinal profile with PB_EXEC is
   NtWaitForSingle+NtReleaseMutant ×19M/160s on a D3D worker — check it
   is the movie player, not a lock regression.
4. **H9 (FATX mount/geometry)** still queued, likely moot now (file
   opens past the boot archive all succeed).

## Queue (next concrete actions)

1. **Unhandled-method inventory:** enable the executor's unhandled table
   (already counting: 150 distinct) and dump method+count+last_param;
   cross-reference `nv2a_regs.h`. Identify the minimal set the intro
   movie/Bink blit needs (image blit vs triangle path).
2. Decide where method coverage lands: it is host NV2A work in the
   toolkit (`nv2a_pb_exec.c`) — a **new subsystem the brief didn't
   contemplate** → architectural gate (see Blockers). Do NOT hand-edit
   gen/; any fix is executor-side.
3. Keep the frame-dump recipe as the Menu-stage artefact once draws
   land: `RECOMP_PB_EXEC=1 RECOMP_VBLANK=1 RECOMP_FB_WINDOW=1
   RECOMP_FB_DUMP=reference/fb/frame`; side-by-side needs an xemu
   reference of the same intro moment (blocked on xemu geometry —
   Blockers).
4. Seeds file `logs/seeds-icall-0011C640.json` is gitignored (analysis
   artefact). The three measured seed VAs are recorded in note 0011 so
   `pipeline.sh --seed` stays reproducible without committing them.
5. Cleanup between runs: `rm -f ~/.local/share/xboxrecomp/Cache/*`.

## Blockers / open questions

- **GPU draw coverage (new primary wall):** frames flip but black;
  150 distinct unhandled NV2A methods. Inventory first (queued). If
  coverage means writing a programmable-vertex/VSH executor into the
  toolkit, that is a large upstream feature — note it as a possible
  architectural-gate moment, but the inventory itself is pure analysis
  and proceeds autonomously.
- xemu's bugcheck (note 0009) must be re-checked against the async
  semantics before using its STOP screen as a "same wall" baseline —
  xemu likely mis-answers the same convention; a corrected xemu
  reference may be possible (config/quirks) or may need its own patch.

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
  `RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_WATCHDOG_SECS`;
  **session-4h set that reaches flips:** `RECOMP_PB_EXEC=1
  RECOMP_VBLANK=1 RECOMP_FB_WINDOW=1 RECOMP_FB_DUMP=reference/fb/frame
  RECOMP_FB_WINDOW_DUMP_EVERY=60`; `RECOMP_POST_TRACE=1` (poster
  wrappers), `RECOMP_REFILL_TRACE=1` (refill wrapper). Seeds file for
  pipeline `--seed` lives at `logs/seeds-icall-0011C640.json` (local).
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
| 2026-11-21 s4g | **THE phantom 0x8000 solved (note 0011): patch 0014 async-IO PENDING default = upstream-correct return convention on async handles → decode completes byte-exact with default env on disc-exact boot.baf (22 reads, 1.8 MB cache, HANG not CRASH)**; manual-override contract wired (`--exclude-manual` + `sub_00022340` wrapper + REFILL_TRACE); refill/init double-charge proven; patch 0015 event create/wait/set caller trace; SIGABRT-core thread-map recipe; post-decode wall reframed = 3-way event deadlock (worker pump {7,1}, worker B polls 4, main INFINITE on its own-consumed 2) | `7faa86f` |
| 2026-11-21 s4h | **Boot reaches the title intro + first GPU flips (note 0011 post-scripts 3-5):** post-decode "deadlock" was an unresolved task-completion ICALL `0x0011C640` (measured seed; siblings 1E330/273EEF same cause) → token 4 set → main proceeds; XGRAPHCL flip-pending `ctx+0x8700` solved by `RECOMP_PB_EXEC=1`+`RECOMP_VBLANK=1`+ host `xbox_Nv2aMirrorFence(0x2E3638,0x2C,0x30)` in `src/main.c`; opens `MS Game Studios Logo-final.bik`; 7 frame dumps (black) = first framebuffer artefacts; poster wrappers + RECOMP_POST_TRACE; new wall = NV2A draw-method coverage (150 distinct unhandled) | (this commit) |
