# Third-party components

## xboxrecomp (toolkit + runtime)

- Upstream: <https://github.com/sp00nznet/xboxrecomp>
- Pinned tag: `v0.18.0`
- Pinned commit: `37b61d589168adee3680fabd51805ee81427bf07` (2026-11-20)
- Location: `xboxrecomp/` — fetched, not vendored; the directory is in
  `.gitignore`. Clone it at the pinned commit:

  ```bash
  git clone https://github.com/sp00nznet/xboxrecomp.git
  cd xboxrecomp && git checkout v0.18.0
  ```

- License: see `xboxrecomp/LICENSE` and `xboxrecomp/LICENSES/` in the clone
  (the runtime embeds subsystems extracted from xemu; upstream carries the
  notices).
- Host target built here: Linux x86-64 (GCC 16.2.1, SDL2 2.32.74,
  libepoxy 1.5.10). Graphics via the toolkit's OpenGL 3.3 back end
  (`src/d3d/d3d8_gl.c`).

## Pipeline tools

The Python pipeline (`tools.xbe_parser`, `tools.disasm`, `tools.func_id`,
`tools.abi_analysis`, `tools.recomp`, `tools.xiso`) ships with the toolkit
clone above. Python environment: `python3` + `capstone 5.0.7` (local venv
in `.venv/`, gitignored).

## Game files

`game/` holds files extracted from the owned disc
(`Sudeki (World) (En,Fr,De,Es,It).iso`, Redump, 3,812,491,264 bytes). It is
gitignored and must never be committed. The one file the pipeline needs is
`game/default.xbe`, SHA-256
`1e28c3db033a3de471ba78ccdd13a824ebddd17fdd0479bf69a795f63bd92419`.

## Reference emulator (xemu) — outside the repo, for baseline captures

Installed under `~/Applications/xemu/` (never in git; nothing here feeds the
pipeline). Approved by the user 2026-11-21 as the reference-baseline
prerequisite (brief rule 6). No sudo used.

| component | version / build | identity |
|---|---|---|
| xemu (upstream) | v0.8.139, commit `aa54c34cae1c09410d74c69a44210a9867fe2d16`, AppImage x86_64 | sha256 `220282aacf09b30b35c55ba900a774fb6940c70deca1408bc70a4c9cc67c322f` (matches GitHub release asset digest) |
| xemu fork (SpiritBoot-qualified) | Mainkill1/xemu `v0.8.136-0-g458730bf5373` AppImage | sha256 OK vs release `SHA256SUMS`; kept as fallback — ignores new config keys, `-m 128` passthrough needed, `-qmp` never opens |
| flash image (BIOS) | **SpiritBoot v1.1.0 official releases** — `SpiritBoot-v1.1.0-64MB.bin` (524288 B, sha256 `75f0680d7a32e8264c3f496eb48c071bd4a9fe8b7e9285e59461f6e35f194f4e`) — THE WORKING PROFILE (xemu default 64 MB) — plus `SpiritBoot-v1.1.0-128MB-64MB-game-memory.bin` (262144 B, sha256 `c403570c4ea613bb0d64fed745c6f08ba0ca118a734a32c3556d24bb907c62a0`, experimental 128-backed/64-visible) | digests verified against the v1.1.0 release SHA256SUMS/asset digests. **No Microsoft BIOS/MCPX bytes were downloaded** — clean-room firmware keeps our posture identical to the recomp side. The earlier `optimized-xemu/SpiritBoot-release.bin` / `-checked.bin` were unofficial dev builds (256 KiB profiles requiring 128 MB) — superseded; they caused the `STOP 0x7E` (see note 0019) |
| HDD image | `xemu-project/xemu-dashboard` `xbox_hdd.qcow2` (1,638,400 B) | sha256 `00d7df7a2bc235f8801764f00b7f40e194d1e392f7a9619d6b2396c89770f6dd`; working copy in `~/Applications/xemu/firmware/` grows with use — local SHA256SUMS.txt tracks current state; verified pristine 2026-11-21 (`6aaa6a37b36bb053dcd875039bfad20005cdd588a0d3ba8abe828c2af4dacdda`) |
| EEPROM | xemu auto-generated (`~/.local/share/xemu/xemu/eeprom.bin`) | not a disc/BIOS derivative |

Boot state (corrected 2026-11-21, note 0019): **SpiritBoot v1.1.0-64MB +
xemu 64 MB (default) boots Sudeki normally** — loading cards, intro FMVs,
title reachable. The old `STOP 0x7E` was the unofficial 256 KiB SpiritBoot
dev profiles forcing 128 MB and reporting 128 MB to a 64-MB-budget title
(known failure class: SpiritBoot #75 Def Jam — `GlobalMemoryStatus`-derived
allocations exceed the 64 MB allocation domain). `scripts/xemu_ref.sh` /
`scripts/xemu_boot_probe.sh` drive captures; standalone (agent-off)
experiment: `scripts/xemu_standalone.sh`. FB-reconstruction VRAM offset
0x3C00000/pitch 720 (note 0009) matches the old ROM layout — re-measure
against the v1.1.0-64MB dump before relying on it.

## Agent skills (fetched, reviewed, pinned)

Installed under `.opencode/skills/` (OpenCode auto-discovers them). Each was
reviewed in full before install: pure instructional Markdown, no payloads,
no network calls; the `diagnosing-bugs` skill explicitly mandates secret
redaction. Pins are the commits the review was performed at — when bumping,
re-read before accepting.

Third-party audit status (skills.sh `/audits` data: Gen Agent Trust Hub,
Socket, Snyk): `gdb` — **Pass ×3** (source repo 296 stars, first seen
2026-02). Verdicts for the other four are on skills.sh pages our fetch
proxy blocked at check time — re-check before any bump (the audit endpoint
also needs a Vercel OIDC token).

| skill | source @ commit | folder digest (path-canonical sha256) | why |
|---|---|---|---|
| gdb | `mohitmishra786/low-level-dev-skills` @ `18f29518a47fda070e6ec1268cb577692fae9a10` | `eff839c95b0f4aa8…` | host crash work: watchpoints, multi-thread, core dumps |
| assembly-x86 | same repo | `f67e97fbbca4aed3…` | reading objdump/GDB asm, ABI questions |
| valgrind | same repo | `8b42151431d1faa1…` | host-side memory errors in the runtime glue |
| reverse-engineering | same repo | `67811f1e842d8509…` | Ghidra/radare2 workflows; pairs with toolkit's `ghidra_naming` |
| diagnosing-bugs | `mattpocock/skills` @ `abeeaf95660ba9e8b7c753eb38f441ca707a8823` | `4a5a6a4c2468fdea…` | methodology: tight feedback loops — matches the run.sh work loop |

Digests are computed by `scripts/skills_check.sh` (`cd <skill>; find . -type f
| sha256sum`, path-canonical) and verified byte-for-byte against the pinned
commits via `diff -r` before being recorded. (An earlier digest column used
repo-root-relative hashing and was meaningless; replaced 2026-11-21 after
the drift check correctly flagged its own tooling bug.)

Vetted and rejected: `wshobson/agents@protocol-reverse-engineering`
(network-protocol scope, out of mission), `gavmor/x86-cpp-reversing-skill`
(9 installs, unvetted reputation), copycat `systematic-debugging` mirrors
(obra's is fine but duplicative of diagnosing-bugs).

## Local toolkit patches (applied by scripts/build.sh)

We keep the toolkit at the pinned upstream commit; local changes live as
`.patch` files under `patches/xboxrecomp/` and are (re)applied by
`scripts/build.sh` — the checkout stays verifiable against the pin, and
each patch is one reviewed, documented diff.

| patch | what | why / status |
|---|---|---|
| `0001-io-write-trace.patch` | `[WRITE]` trace in `bridge_NtWriteFile`, env `RECOMP_IO_WRITE_LOG=1` (first 60 calls, then every 10,000th) | mirror of the existing `[READ]` line; settled the boot.Baf write-loop diagnosis (note 0004). Candidate for upstream PR. |
| `0002-event-signal-trace.patch` | `[EVENT]` trace of every guest-visible event signal (`KeSetEvent`, `KePulseEvent`, `NtSetEvent`, `NtPulseEvent`, I/O-completion `SetEvent`), env `RECOMP_EVENT_LOG=1` | proved `0x48000004` is never signalled — the wait was a symptom, not the wall (note 0005). |
| `0003-io-write-csv.patch` | per-write CSV (`RECOMP_IO_WRITE_CSV=<path>`) + periodic guest-stack backtrace (`#scan` lines) | the census (94,643 × 61,440 B contiguous, one 32-bit wrap) and the backtrace that named the game's own writer loop. |
| `0004-fsctl-probe.patch` | env-gated (`RECOMP_FSCTL_OK=1`) success answer for FILE_DEVICE_FILE_SYSTEM FSCTLs + logs code/in/out/caller | **probe, kept for the logging**: H7 (mount failure causes the loop) was falsified with it. Default behaviour unchanged. |
| `0005-cache-volume-capacity.patch` | `recomp_cache_volume_capacity()` + `STATUS_DISK_FULL` past the capacity of the cache volume (`X:/Y:/Z:`, backed by `Partition3-5.img`); capacity = largest backing image size, `RECOMP_CACHE_CAP_BYTES` overrides, `0` disables | **the fix for the boot write-loop wall** (note 0005): a host directory has no capacity, so the title's cache writer never saw the disk-full condition that ends it on hardware. Capacity *emulation* at the write layer — the FATX mount/geometry gap remains. Cache volumes only; TDATA/UDATA (Partition1) not yet capped. |
| `0006-span-window-fits.patch` | after the OS-chosen guest-span base, probe whether `base+0xFF000000` is mappable; if not, re-place the span at a low hint (256/16/4 GB, 512 MB, `MAP_FIXED_NOREPLACE` so hints can't clobber) | insurance (note 0006): Linux places mappings top-down; at the top of the address space the high guest windows (contiguous/tiled/NV2A/MCPX/flash) fall outside the canonical range and all five map-fail nondeterministically. Not the wall: with windows mapped, the title still faults at the window end. |
| `0007-contig-alias.patch` | env `RECOMP_CONTIG_ALIAS=1`: second view of the contiguous mapping at `0x84000000` (hardware decodes the aperture past 64 MB) | probe, kept as bring-up tool (note 0007): removes the fault at `0x84000000` and turns CRASH back into the original HANG — proving the mapping was not the wall. Default off. |
| `0008-read-buffer-va.patch` | `[READ]` trace now prints `buf=0x%08X` (guest buffer VA) | settled note 0008: `boot.baf` reads alternate `0x80034000`/`0x8003C000` inside the same contiguous arena the decoder reads — killed the aliasing hypothesis (H12). |
| `0009-write-content-dump.patch` | `RECOMP_IO_WRITE_DUMP=<path>`: append cache-volume write contents (cap 32 MiB) + `[WRDUMP]` index lines | enabled the byte-exact diff of the guest's `Z:\boot.baf` stream against the offline decoder output (note 0009/0010: common prefix 0x16BAF6). |
| `0010-read-path-size.patch` | `[READ]` trace now prints `sz=` (host file size) and `path=` | proved reads are served from `game/boot.baf` (679,936 B) byte-exact; moved the `sys/stat.h` include under `#ifndef _WIN32` at the top of the block. |
| `0011-qinfo-trace.patch` | `[QINFO]` trace in `bridge_NtQueryInformationFile` (env `RECOMP_QINFO_LOG=1`): class, length, status, returned bytes incl. EndOfFile/AllocationSize/attrs, handle path | settled the size-query question (note 0010): `game/boot.baf` answers `eof=0xA6000` correctly — the game's 0x9E000 read-stop is its own budget logic, not the kernel. |
| `0012-watch-mprotect-errno.patch` | `RECOMP_WATCH` arm: report `errno` when the guard `mprotect` fails | diagnostic hygiene; exposed that single-page arming can't cover windows spanning host pages. |
| `0013-watch-multi-page-window.patch` | `RECOMP_WATCH` window spans as many host pages as `RECOMP_WATCH_LEN` needs (guest RAM is one contiguous mapping) | lets one arming cover the worker-stack object across run-to-run stack parities (0x02681/0x02691/0x02692 pages). |
| `0014-async-io-default.patch` | `bridge_async_io_enabled()` default ON (`RECOMP_ASYNC_IO=0` opts out): reads on async handles return `STATUS_PENDING` as a real kernel does | **ends the phantom 0x8000 budget charge** — the boot-archive reader double-charges under synchronous SUCCESS; with PENDING, decode completes byte-exact on the disc-exact file (note 0011). |
| `0015-event-create-wait-caller.patch` | `[BRIDGE]` event create/wait/set lines gain the caller `from=` return address | named the three-way event deadlock's participants (note 0011); hygiene kept. |
| `0016-1d70-honor-set-semaphore-offset.patch` | `0x1D70` (SET_SEMAPHORE) with no registered target falls back to the last `0x1D6C` (SET_SEMAPHORE_OFFSET) register value; `RECOMP_SEM_TRACE` | D3D8LTCG releases ring counters through the DMA-instance semaphore; kept as fallback/trace — the real wall needed 0017 (note 0016). |
| `0017-flip-done-settle-chain.patch` | `nv2a_pb_set_flip_done_chain(dev_ptr_va, off1, off2)`: clears the title's Present flip-pending word `*(*(dev)+o1)+o2` at the executor's ack tick when DMA_GET==DMA_PUT | **solves the post-movie Present/ring-rearm wait** (`ctx+0x8700` spin) → level streaming reached (note 0016). Registered from `src/main.c` with measured offsets. |
| `0018-watch-low-map.patch` | `RECOMP_WATCH` accepts windows inside `[XBOX_MAP_START, XBOX_BASE_ADDRESS)` when the RAM map starts low | lets the watch arm on the fake-TIB pages (`0x4000`-family) used by the TLS census (note 0017). |
| `0019-watch-guest-stack.patch` | watch hits also report the guest-stack window (`g_esp` scan) and host-`rip` dladdr hits for the faulting thread | names the functions behind a trapped store on x86-64 Linux. |
| `0020-watch-tf-step.patch` | watch re-arms via TF single-step after each hit; `RECOMP_WATCH_PANIC=1` raises SIGABRT with a core on Nth hit | turns the one-shot page watch into a repeated trap and captures a core at the poison write (note 0017). |
| `0021-log-ords.patch` | `RECOMP_LOG_ORDS=44,98,...`: per-ordinal call trace without the `RECOMP_KERNEL_LOG_BUDGET` limit | targeted ordinal census for the audio hunt. |
| `0022-ac97-trap-x86_64.patch` | widen the AC'97 bus-master write trap arm from aarch64-only to `(POSIX && aarch64) || (Linux x86-64)`; the POSIX VEH-fallback is replaced with the real SIGSEGV-routed handler path | **the audio fix**: on Linux x86-64 the trap never armed, DSOUND master open answered DSERR_NODRIVER, and the null-voice fallback produced the whole note-0017 poison chain. With `RECOMP_AC97_READY=1` the MCPX APU + AC97 trap come up and all five DDS9 sub-inits return 0 (note 0017/session 4j). |
| `0023-pad-start-bit16.patch` | `usb_gamepad.c`: scripted/synthetic digital masks widen to the real 16-bit report field (bytes 2..3): `start`=0x0200, `back`=0x0100 (were 0x10/0x20, reserved low-byte bits); applies through `out[2]`/`out[3]` split | the virtual pad's START was inlined at a bit no XDK title reads: Sudeki's enumerated pad received 8 "start" reports and ignored them (note 0020 evidence run `logs/run-pad2-*.out`); host-pad path was already correct. |
