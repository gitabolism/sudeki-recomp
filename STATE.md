# STATE.md — live working state

> Read me first, every session. Update me at the end of every session,
> even interrupted ones. Protocol in `AGENTS.md`.

**Last updated:** 2026-11-21 (session 4i)
**Stage:** LIFT complete, ENTRY strong. **Boot runs past the intro movies,
audio engine now initializes (patch 0022), first-level + wavebank streaming
completes** (`SOLWORLDM.gex`, `z:\wavebanks\*` incl. `speech_buki`, ~1250
flips). Two walls: (i) GPU draw coverage — executor skips all vertex-program
batches, so menu/game frames stay black; (ii) NEW **wild-pointer wall**:
game allocator (`sub_0011F070`) splices a free-list node reading
`0xFE087E8D` (NV2A register-aperture value) — fully deterministic. The
audio init gap (H21) is CLOSED: without the AC'97 bus-master trap DDS9's
master open returned DSERR_NODRIVER, the 'SdaD' packet create was skipped,
and a null-voice 24 KB wavebank copy trampled the TIB page (note 0017).
Flip-completion semantics solved by the settle chain (patch 0017).

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
  `RECOMP_WATCH` arm fixes (errno, multi-page window), `0014` async-IO
  return convention = the boot-archive fix, `0015` event create/wait/set
  caller trace, `0016` `0x1D70` honor-`0x1D6C`-offset fallback +
  `RECOMP_SEM_TRACE`, **`0017` flip-done settle chain**
  (`nv2a_pb_set_flip_done_chain`, ack-tick clear at DMA_GET==DMA_PUT),
  `0018` watch arming below `XBOX_BASE_ADDRESS`, `0019` watch
  guest-stack + host-rip attribution, `0020` watch TF single-step re-arm
  + `RECOMP_WATCH_PANIC`, `0021` `RECOMP_LOG_ORDS` never-budgeted ordinal
  trace, **`0022` AC'97 bus-master write trap on Linux x86-64 = the audio
  fix** (POSIX arm guard widened; decoder+routing already existed).
  Table in THIRD_PARTY.md.
  Manual overrides: `--exclude-manual src/recomp_manual.c` now wired into
  `scripts/pipeline.sh`; wrappers = `sub_00022340` (stream refill),
  `sub_0026C79D` (error-slot setter + TLS guard), `sub_00021CD0/
  sub_000220E0` (posters), `sub_0028EA91`/`sub_002854B3` (audio census);
  gen tree emits bodies as `*_gen`, stubs still 432.
- XBE: pinned SHA-256 ✔. Gen tree regenerated session 4g with
  `--exclude-manual` (one body renamed `sub_00022340_gen`; 432 stubs).
- Skills: five pinned, digest gate green.

## Last run

Session 4i (final cycles): **audio wall closed by patch 0022** —
`env RECOMP_VBLANK=1 RECOMP_PB_EXEC=1 RECOMP_PB_WALK=dma
RECOMP_KERNEL_LOG_BUDGET=0 RECOMP_AC97_READY=1 ./build/sudeki` →
`logs/run-ac97d-1-*.out`/`-2`: APU up (SDL2), "AC97: bus-master writes
trapped at 0xFEC00000 (POSIX)", wavebanks incl. `speech_buki` stream,
1225/1211 flips, deterministic SIGSEGV `0xFE087E99` at `0x1A9C1D`
(real `sub_001A9B60` body; reported `sub_001A9A50+0x1CD`) — H23. With
`RECOMP_DS_INIT=1 RECOMP_DS_VOICE=1` (`logs/run-ac97c-214452.out`): all
five DDS9 sub-inits -> 0, zero `DS-VOICE!`/`DS-COPY!` (probe wrapper skips
the init tail so that run dies earlier at `sub_0028A5A4+0x184` — wrapper
artifact, not a product bug). Pre-fix runs: sub 3 = `0x88780078`
DSERR_NODRIVER (`logs/run-dsinit1-213937.out`).

Session 4i (mid cycles): audio-wall anatomy via manual wrappers —
`env RECOMP_VBLANK=1 RECOMP_PB_EXEC=1 RECOMP_PB_WALK=dma
RECOMP_KERNEL_LOG_BUDGET=0 RECOMP_DS_COPY=1 RECOMP_DS_VOICE=1
./build/sudeki` → `logs/run-dsvoice-212823.out`: one `[DS-VOICE!]
this=029A00F4 slot14=00000000 ret=00285689` then one `[DS-COPY!]
this=0 … off=00000818 dst=00000818 src=027F3698 ret=002893C0` —
the wavebank chunk (0x6168 B, from `buf=0x027F2E80 want=24936` read)
copies onto guest 0x818..0x6978 → TIB page zeroed → guarded TLSSET
(`torn-down TLS` fires once) → surviving crash moves to
`sub_000B08F0+0xDFB2` writing through `esi=0x89D8458B` during
`spot_sounds.xwb`/`UISoundScript.xsb` phase. Note 0017 outcome filled
in; the TLS-lifetime reading is falsified.

Earlier 4i: `env RECOMP_VBLANK=1 RECOMP_PB_EXEC=1 RECOMP_PB_WALK=dma
RECOMP_KERNEL_LOG_BUDGET=0 ./build/sudeki` (settle chain registered in
`src/main.c`, TLS guard in `src/recomp_manual.c`) →
`logs/run-tlsguard-204217.out`: **PROGRESS** — movies play (331 flips),
then the title streams first-level assets: `SOLWORLDM.gex`,
`z:\wavebanks\BS_brightwater/Elco_weapons/S_missile.xwb`,
`D:\sound\missiles.xsb`, second `badger.plap` pass. No crash (TLS guard
fired once: `[TLSSET] torn-down TLS (fs=00004000 arr=00000000) — store
skipped`). State at 120 s: main active in `sub_0011D460` (allocator
scan; PC advances between cores = progress, not spin), NV2A USER
PUT==GET (`0x000B9018`), vblank DPC re-inserts ~48/s (ICALL
`FE0001A0` = `KeInsertQueueDpc` slot), **no flips after the movies**
(menu draws are all vertex-program batches → skipped), zero
`NtDeviceIoControlFile` (pad device never opened yet).
`DECODE-EXACT` still holds.

Earlier 4i runs: `logs/run-fd1-202904.out` (first crash site, TLS
fault), `logs/run-walk1-*.out`/`run-sem1-3` (fence/KickOff/semaphore
anatomy), `logs/run-watch4k2-203539.out` (4-byte watch on 0x4004 via
patch 0018), `logs/run-tls1-203840.out` (461 healthy TLSSET then
arr=0).

## Solved this session (4i; notes 0016, 0017)

1. **Post-movie Present/ring-rearm wait** (`sub_002D3CC0` spinning on
   `[ctx+0x8700]==1`, ctx=`*([0x2E3638]+0x934)`=0xFD000000, stable
   across cores): the GPU ring-conpletion word. Patch 0017 =
   `nv2a_pb_set_flip_done_chain` + ack-tick clear when DMA_GET==DMA_PUT
   (measured chain, registered in `main.c`) → level streaming reached.
   The `0x1D70` releases are ring counters whose target resolves via
   the DMA instance (`0x1D6C` programs offset 0), so the
   honor-the-offset fallback (patch 0016) never fires; kept as trace.
2. **TLS teardown crash** at `sub_0026C79D` (XDK error-slot setter,
   `slot=[[fs:4]+(-37)*4`, tls_index@0x376620=0xFFFFFFDB): after a
   thread's TLS teardown the TIB array word holds poison (0 /
   0xC0000000) yet guest code still runs with that `g_fs_base`.
   Toolkit thread-lifecycle gap (note 0017); probe guard in the manual
   wrapper skips the store (`RECOMP_TLS_GUARD` default on,
   `RECOMP_TLS_TRACE=1` census).
   **CORRECTED later same session (note 0017 outcome):** no thread
   teardown involved — a DSOUND/XACT packet copy with `this=NULL`
   (voice child slot +0x14 never initialized) writes a 24 KB wavebank
   chunk to guest 0x818, zeroing the TIB page as a side effect. Guard
   kept as insurance; real wall = audio init gap (H21).
3. Tools: `RECOMP_WATCH` arms below `XBOX_BASE_ADDRESS` (patch 0018);
   4-byte watch caught `fs:[4]` init (`00770094→00F80094` =
   `bridge_layout_tls`) and the teardown zero; TLSSET wrapper census;
   `RECOMP_PB_WALK=dma` is part of the working GPU recipe.

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
5. **H21 CLOSED (patch 0022):** the audio init gap was the AC'97
   bus-master write trap never arming on Linux x86-64 (arm compiled for
   aarch64 only; the VEH fallback is a NULL stub). With the trap armed +
   `RECOMP_AC97_READY=1` the toolkit MCPX APU (SDL2, VP passthrough) comes
   up, all five DDS9 sub-inits return 0, and null-voice submits go to 0.
   The 24 KB copy onto the TIB page and the `sub_0028F8C3`-family poison
   chain are gone. `RECOMP_AC97_READY=1` now belongs to the working recipe.
6. **H22 CLOSED:** the `sub_000B08F0` wild crash was the same audio
   poison chain; it no longer occurs.
7. **H23 (NEW wall — deterministic wild pointer in the game allocator):**
   with audio up, three runs crash identically (same fault VA `0xFE087E99`)
   at real code `0x1A9C1D` (dispatch-misattributed to `sub_001A9A50+0x1CD`;
   real head `sub_001A9B60`): `mov [ebx+0x1c],eax`, `ebx=0xFE087E81`, free
   chain walked via `[eax+0x18]` from the heap control at `obj+0x1C`
   (game allocator `sub_0011F070`, `this`=obj with markers 0x3299FC/
   0x329A00). The link value `0xFE087E8D` is NV2A-register-aperture space
   (pushbuffer-like). Next: identify the heap-control object (stable VA?),
   arm `RECOMP_WATCH` on `[obj+0x1C]` / re-arm from a wrapper when
   `sub_0011F070` first reads the poison, and catch the writer.

## Queue (next concrete actions)

1. **H23 hunt (primary):** the poison link `0xFE087E8D` is deterministic —
   find its writer. Cheapest first: a manual wrapper on `sub_0011F070`
   (game allocator) that, when `MEM32(this+0x1C)` first reads
   `>= 0xFE000000`, (a) prints `this`, the control block
   (+0x10/+0x18/+0x1C) and the guest return chain, and (b) if the toolkit
   exposes an arm-from-C watch entry (`xbox_WatchArm`?), traps the last
   good control word going forward. If the object VA is stable across
   runs, pre-arm from env instead.
2. Cross-check `0xFE087E8D` against NV2A semantics: dump recorded
   pushbuffer methods near that offset (`nv2a_pb_exec_report`) — a
   *legitimate* pushbuffer VA sitting in a heap link means the game
   queued it deliberately and the allocator misuse is downstream of our
   skipped-vertex batches.
3. **Unhandled-method inventory (GPU wall):** enable the executor's
   unhandled table (already counting: 150 distinct) and dump
   method+count+last_param; cross-reference `nv2a_regs.h`. Identify the
   minimal set the intro movie/Bink blit needs (image blit vs triangle
   path).
4. Decide where method coverage lands: it is host NV2A work in the
   toolkit (`nv2a_pb_exec.c`) — a **new subsystem the brief didn't
   contemplate** → architectural gate (see Blockers). Do NOT hand-edit
   gen/; any fix is executor-side.
5. Keep the frame-dump recipe as the Menu-stage artefact once draws
   land: `RECOMP_PB_EXEC=1 RECOMP_VBLANK=1 RECOMP_FB_WINDOW=1
   RECOMP_FB_DUMP=reference/fb/frame`; side-by-side needs an xemu
   reference of the same intro moment (blocked on xemu geometry —
   Blockers).
6. Seeds file `logs/seeds-icall-0011C640.json` is gitignored (analysis
   artefact). The measured seed VAs (now seven: `0011C640 01E330 0273EEF
   0550A0 0FD7D0 135AA0 092010`) are recorded in notes 0011/0017 so
   `pipeline.sh --seed` stays reproducible without committing them.
7. Cleanup between runs: `rm -rf ~/.local/share/xboxrecomp/Cache/*`.

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
  `RECOMP_WATCH_LEN`/`RECOMP_WATCH_PANIC` (patches 0013/0018/0020),
  `RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_WATCHDOG_SECS`;
  **session-4h set that reaches flips:** `RECOMP_PB_EXEC=1
  RECOMP_VBLANK=1 RECOMP_FB_WINDOW=1 RECOMP_FB_DUMP=reference/fb/frame
  RECOMP_FB_WINDOW_DUMP_EVERY=60`; `RECOMP_POST_TRACE=1` (poster
  wrappers), `RECOMP_REFILL_TRACE=1` (refill wrapper), `RECOMP_TLS_TRACE=1`
  / `RECOMP_TLS_GUARD` (error-slot setter), `RECOMP_DS_COPY=1`
  (`sub_0028EA91` packet-copy census, `!` lines always logged),
  `RECOMP_DS_VOICE=1` (`sub_002854B3` voice-state census),
  `RECOMP_DS_INIT=1` (`sub_0028F8C3` DDS9 master-open census; the probe
  skips the init tail — diagnostics only), `RECOMP_LOG_ORDS=44,98,...`
  (patch 0021), **`RECOMP_AC97_READY=1` now part of the working recipe**
  (MCPX APU + AC97 bus-master trap; audio initialises). Seeds file for
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
| 2026-11-21 s4i | **level streaming reached + audio wall root-caused (notes 0016/0017)**: flip-done settle chain (patch 0017 + `main.c` chain `0x2E3638→+0x934→ctx+0x8700`) clears post-movie wait → `SOLWORLDM.gex`/wavebanks/`BS_brightwater.xwb` stream; 4 measured seeds more (`0550A0 0FD7D0 135AA0 092010`) lift remaining boot ICALL misses; the "TLS teardown poison" crash **falsified and root-caused**: DSOUND voice `sub_002854B3(this=029A00F4)` slot +0x14=0 → `sub_0028EA91` 24 KB wavebank copy to guest 0x818 tramples exception page + TIB page; TLS guard kept, audio census wrappers (`RECOMP_DS_COPY/DS_VOICE`, `!`=impossible-address auto-log); watch upgraded (guest-stack chain 0019, TF single-step + panic core 0020); surviving crash = `sub_000B08F0+0xDFB2` wild write `esi=0x89D8458B` (same audio subsystem, H22) | (this commit) |
| 2026-11-21 s4j | **audio wall CLOSED (patch 0022)**: DDS9 master open `sub_0028F8C3` needs AC97 bus-master trap; upstream armed it only on aarch64 (x86-64 Linux fell to NULL-VEH stub) → DSERR_NODRIVER → 'SdaD' create skipped → null-voice 24 KB copy = the whole note-0017 chain. Patch widens POSIX arm to x86-64; with `RECOMP_AC97_READY=1` MCPX APU (SDL2) + AC97 trap up, all 5 sub-inits 0, DS-VOICE!/DS-COPY! 0, wavebanks stream, ~1250 flips. New wall H23: deterministic allocator crash — free-list link `0xFE087E8D` (NV2A aperture value) spliced at real `0x1A9C1D` (`sub_001A9B60` family, misattributed `sub_001A9A50+0x1CD`); census wrappers `sub_0028F8C3/sub_002885AE/sub_0028AF42`, patch 0021 `RECOMP_LOG_ORDS` | (this commit) |
