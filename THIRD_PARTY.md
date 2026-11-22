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

### Toolkit forks considered (as base/feature sources) — not adopted, 2026-11-22

Reviewed on user question "why aren't we using xboxrecomp-vulkan?". Base
changes are an architectural gate (AGENTS.md); recorded so the next
session doesn't re-litigate:

- `almost-drowned-rat/xboxrecomp-vulkan` (toolkit of THPS2X-Recompiled;
  14 commits, 1 star): fork frozen at **v0.13.1**, i.e. five releases
  *behind* our v0.18.0 pin. It is the upstream `vulkan-backend` branch
  state: Vulkan NV2A renderer (`src/nv2a_vk/`, from antoxa2584x/
  burnout3-sw) + SDL3 + an XDK HLE layer (`src/xdk/`, design after
  TGP482). Not adopted: (i) base-switch loses v0.14-v0.18 kernel/lifter
  work our boot depends on and rebases our 26-patch series; (ii) the
  current GPU wall is NV2A *semantics* coverage (pb-exec methods, VSH,
  the sw-method interrupt chain, note 0021), backend-independent; the
  GL executor already flips/draws/decodes. Mainline v0.18.0 has no
  `src/nv2a_vk/` — Vulkan is a separate branch there too.
- `almost-drowned-rat/xboxrecomp-option-c`: same family, "per-thread
  __restrict register model in the lifter" — a lifter change that would
  invalidate the whole gen tree; no evidence we need it (432 stubs).
- `AntAir267/antbox`: fork of takedawn's xboxrecomp with a Vulkan
  renderer + frame capture; different lineage, frame-capture need
  already met by `RECOMP_FB_DUMP`.
- Revisit trigger: **direction approved by user 2026-11-22** — Vulkan is
  the accepted end-state backend, but only as Stage B *after* vanilla
  gameplay under the current GL back end (STATE.md queue 8). Execution
  = cherry-pick/backport upstream's `vulkan-backend` **branch** onto the
  v0.18.0 pin (with a re-audited pin entry), accepted at parity of
  frame-dump artefacts and gameplay; never a base-switch to the fork,
  never mid-wall.

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

## Reverse-engineering toolchain (Ghidra) — outside the repo, for static analysis

Approved by the user 2026-11-22 ("one more weapon in your arsenal"). Software
downloads remain an approval-gated act; this is the recorded approval. The
install is reproducible on a fresh machine by `scripts/analysis_tools.sh`:
pinned URLs + digests mirrored below, digest gate BEFORE extraction (the
digest is the trust anchor, the URL is env-overridable for mirrors — a
tampered archive refuses to extract), installs under `~/opt` only, no sudo,
nothing on PATH, smoke-tests through `scripts/ghidra.sh`, marker file
`~/opt/.sudeki-analysis-tools`. `scripts/doctor.sh` check 8 FAILs on drift.

| component | version / build | identity |
|---|---|---|
| Ghidra | 12.2 public (tag `Ghidra_12.2_build`, built 2026-10-16), **Apache-2.0** (NSA-published), 569,712,708 B | sha256 `2c2cd776db3c1f6a097ba418922378ac3649cadd5e844a6f5f088ae628a7be67` — matches the digest published in the GitHub release body |
| Temurin JDK | `jdk-25.0.5.1+2` (jdk_x64_linux_hotspot), **GPLv2 + Classpath Exception**, 141,362,230 B | sha256 `760f91b22c3efc9bd1b64ff950598231be82928cc807e30691a0e7a2995a62c0` — matches Adoptium's published `.sha256.txt`. Ghidra 12.2 requires Java 25 (`application.java.min=25`); a JDK 21 tarball was fetched first and discarded — do not downgrade |

Layout: `~/opt/ghidra_12.2_PUBLIC/` + `~/opt/jdk-25.0.5.1+2/`. The only Java
linkage on the machine is `JAVA_HOME_OVERRIDE=` inside Ghidra's own
`support/launch.properties` (rewritten by the installer); the launcher
bootstraps a `java` lookup BEFORE reading that file, which is why
`scripts/ghidra.sh` (exports the pinned JDK) is the sanctioned entry point.
Uninstall: `rm -rf ~/opt/ghidra_12.2_PUBLIC ~/opt/jdk-25.0.5.1+2` — the
machine is untouched.

Usage policy: Ghidra project DBs and any decompiled pseudocode are analysis
artefacts → `/tmp/opencode` (or `~/opt`), **never git** — same law as every
disc-derived artefact. XBE import strategy pending: raw LE binary at the
image base with the section map from `default_analysis.json`, or a community
XBE-loader extension — any extension would be a new download = new pin +
audit entry here. Banner house style for README sections is reproducible
via `tools/banner.py` (figlet kerned ANSI Shadow, glyph blocks joined by one
space).

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
| `0024-vsh-io-trace.patch` | `nv2a_vsh_interp.c`: `nv2a_vsh_run` wrapper; `RECOMP_VSH_IO=1` prints first runs' v0..v2 inputs + oPos (input-shaped filter) | separate wrong vertex data from wrong program emulation when batches report no valid coordinates (note 0021 side census: S32K decode matches xemu; Sudeki's short-space positions are legitimate). |
| `0025-pgraph-swmethod-latch.patch` | `nv2a_pb_exec.c` + `kernel_hal.c` (+ census): at `NV097_NO_OPERATION` with non-zero param the executor latches the aperture words `[0xFD000100] |= 0x1000` (PMC PGRAPH source = the vblank-DPC gate), `[0xFD400100] = 1` (software-method bit), `[0xFD400704] = 0x100` (trap method), `[0xFD400108]/[0x400708] = param`; a 64-deep FIFO queues marks while the trap register is busy (hardware serialises NOPs; dropping lost Sudeki's only ring-space mark); `dpc_run_here()` acks (clears w1c words, re-arms next queued) after each drain. Census: `[NOPSW]` class histogram + latch counters, `[VBLDBG]` tick/DPC-depth line. Env `RECOMP_PB_GFAULT` (on with executor, `=0` off) | D3D8LTCG wakes its ring/present waits via GPU software-method NOPs (DPC `sub_002DAE40` → pump `sub_002DB250` → `sub_002DB4A0` jump table keyed `(param&0x1F)-1`; class 5 → `KeSetEvent(0x2E2F5C)`); the executor consumed NOPs and raised nothing → attract-demo ring wait froze at `PUT==GET` (note 0021). |
| `0026-swmethod-ack-ownership.patch` | `xbox_memory_layout.c` + `kernel_bridge.c`: (1) the aperture ack pass (`NV2A_ACK` busy-bit table, "nothing raises GPU interrupts") would zero the latched `PGRAPH_INTR` bit 0 / `PMC_INTR_0` bit 12 before any service reads them — mask them out **only from latch until the guest's service chain reads the words once** (`nv2a_pb_swmethod_hold()`; `seen` set by manual wrappers at `sub_002DAE40` DPC entry / `sub_002DB250` pump entry). v1 held "while pending" instead — that wedged the guest's PGRAPH service loop (which spins until the word reads clear; guest w1c is a RAM no-op, the ack pass is the only clearer) inside the timer thread: freeze at 179 flips, gfault8; (2) `kernel_drain_dpcs()` (timer-thread drain, the TICK-mode path) performs the swmethod ack — if seen: clear + pop FIFO; if unseen: re-arm the same param without popping (mark was ack-eaten, retry; FIFO drains only at real service rate) | gfault6/7 race: only 75 of 2437 marks ever seen (`ack=0`, class-5 queued never serviced); gfault8 wedge above (note 0021). |
| `0027-ke-shadow-once-init.patch` | `kernel_bridge.c`: `ke_shadow_init()` guarded by `InitOnceExecuteOnce` (compat impl blocks late callers until the callback completes) — replaces the unguarded `if (g_ke_shadow_init) return;` gate | frz4 core: guest ring-writer (`sub_002D57D0`→`bridge_KeWaitForSingleObject`→`ke_shadow_lookup`) blocked forever in `EnterCriticalSection` on an orphaned heap mutex while `g_ke_shadow_cs` read all-zero with init flag 1 — double `InitializeCriticalSection` (memset + mutex swap) at first use; the stochastic H27 freeze (note 0022). |
| `0028-read-log-gate.patch` | `kernel_bridge.c`: the `[READ]` completion trace (added by 0008/0010) is now env-gated `RECOMP_IO_READ_LOG=1`, **default OFF**; the per-read `w32_handle_path` + `stat()` + `fflush` run only when armed | Level-streaming phases issue 5k+ 16 KiB reads per load (frz14: 5517 in one stall window); unconditional tracing taxed the very load durations we measure (note 0023 H28 anatomy). |
| `0029-unhandled-table.patch` | `nv2a_pb_exec.c`: ranked top-40 unhandled-method table (`[UNH] m= n= last=`) in `nv2a_pb_exec_report`, env `RECOMP_PB_UNH_TABLE=1` | H19 inventory (note 0023): implement-in-order list; result — all top methods benign, coverage gap phantom; the table also caught `BACK_END_WRITE_SEMAPHORE_RELEASE ×5903` falling through, framing H29. |
| `0030-rering-free-relatch.patch` | `nv2a_pb_exec.c` + `nv2a_pb_scan.c`: store the last ring-free (class 5) NOP param when seen; when the DMA walk ends a segment (`GET` reaches `PUT` with work pending) call `nv2a_pb_rering_free_drained()` and re-latch that mark through the sw-method chain; census `[RERING]` + `[NOPSW] rering calls/latched` + `[PB] walk calls: work=/drained=` | H29 (note 0023, frz20 core): ring writer `sub_002D57D0` parks in `KeWaitForSingleObject(0x2E2F5C)` — D3D8LTCG pushes its class-5 NOP exactly ONCE per run (census seen `5:1` of 2891); without hardware's per-drain ring-free re-raise the standing arm never signals again → prologue plateau hang. (v1 hooked the ack-thread poll instead: drains finish inside one tick, edge never observed — frz23 saw `5:1` unchanged.) Verdict: fires faithfully but INSUFFICIENT — the class-5 NOP is not D3D's real ring-free protocol; see 0031. |
| `0031-bew-release-channel-block.patch` | `nv2a_pb_exec.c`: `0x1D70` BACK_END_WRITE_SEMAPHORE_RELEASE with small offsets (`off < 0x800`, incl. D3D8LTCG's offset 0) lands in the NV2A aperture **channel block** (`xbox_Nv2aRegPtr(0x800000+off)` — same block whose `+0x30/+0x44` the walk publishes as USER DMA PUT/GET) instead of being rejected; `[SEM] release -> CHANNEL` trace | H29 (note 0023, frz25/26 censuses): D3D programs `SET_SEMAPHORE_OFFSET(0)` and releases per kick (counters 5,7,9…; 5903 in frz16) — all rejected by 0016's `off!=0` guest-RAM guard; channel-block `+0/+4` words read ZERO at every plateau while `sh8700` holds constant poison. The BEW landing IS the ring-progress feedback D3D8LTCG waits on. |
| `0033-finite-gpu-get-lag.patch` | `nv2a_pb_scan.c` + `xbox_memory_layout.c` + exec census switch: `RECOMP_PB_GET_LAG=<bytes>` publishes DMA_GET behind the walk's consumption trail (snapshot ring follows JUMP/CALL); `nv2a_pb_true_idle()` keeps the 0017 flip-done settle honest while the register lags; `RECOMP_RINGFREE_EVENT=0` A/B-disables the 0032 signal; probes: guest-GET-move counter + lag got/pub census | H29 (note 0023, frz31/32): hypothesis "GPU infinitely fast inverts D3D's free-space gate" — FALSIFIED at the register: the title writes its own optimistic GET into the channel block ~once per kick (`guest-GET moves=5036`≈walk calls 5039), so D3D's arithmetic reads its own bookkeeping, not our register. Kept as instrumentation + a hardware-faithful default-OFF knob. |
| `0032-ring-free-host-event.patch` | `kernel_bridge.c` + `nv2a_pb_exec.c`: exported host-callable `xbox_host_signal_event(va)` (SignalState + shadow `SetEvent`, lock-guarded); executor registers the D3D ring-free event object (`nv2a_pb_set_ring_free_event`, main.c: ctx+0x1DCC = 0x2E2F5C) and fires it on EVERY walk drain edge (0030 hook); census `[NOPSW] rering ... ev=` | H29 (note 0023, frz29 lift+core): D3D's writer parks in `while (KeWaitForSingleObject(0x2E2F5C,6,1,0,0))` (INFINITE, synchronization event, no re-check) when its post-submit free calc sees GET stale-behind; hardware re-raises PM ring-free per drain, our runtime signaled once (single class-5 NOP) → engine-submit thread frozen forever = the terminal plateau. Patch 0030's NOP re-latch delivered zero second wakes; wire the drain edge straight to the event object. |
| `0034-trap-idle-after-service.patch` | `nv2a_pb_exec.c`: `nv2a_pb_trap_idle()` idles the PGRAPH trap quartet (PARAM/TRAP/TRAP_DATA) after a SEEN service AND on abandoned-mark ack passes (nothing latched, FIFO empty, TRAP live); counters `idle=/rearm=/seen_l=/seen_n=` | H29 (note 0023, frz34 core + frz35 census): plateau core left `INTR=0 TRAP=0x100 TRAP_DATA=0x0DF80101` forever; hardware TRAP auto-idles on the service read (same coalescing rule as 0025's latch gate), our model left the words live so the guest pump (sub_002DB250, site 0x002DB2E9) re-dispatched stale marks 228k times, arm-0 ring bookkeeping (loc_002DB4C4) is additive -> D3D swap-completion indices drift -> event 0x2E2F4C never signaled -> engine parks INFINITE. frz37: fires (idle=1067) but storm survives (consume too rare); frz38: pump 1687->4340, counters prove seen_l=606,716 vs ack=4,840. |
| `0035-consume-seen-mark-at-first-read.patch` | `nv2a_pb_exec.c` + `kernel_bridge.c`: a latched mark is CONSUMED (`done()`: clear INTR/PMC + trap quartet per 0034, pop the pending FIFO) synchronously when the guest service chain first READS it — the `s_sw_seen` 0→1 edge in `nv2a_pb_swmethod_seen()` (manual-wrapper-called); `nv2a_pb_swmethod_service_consume()` also runs every `kernel_drain_dpcs()` tick as insurance; same 0026 ownership (hold until SEEN); `RECOMP_ACK_CONSUME=0` opts out. v3: `s_swm_cs` INIT_ONCE critical section around FIFO enqueue/pop — the seen-edge consume made done() multi-threaded (guest + ack thread + walk) and the unlocked ring could swallow marks. v4 (frz49 core): the unseen branch NO LONGER acks/re-arms — it leaves the words armed (`RECOMP_ARM_UNTIL_SEEN=0` restores churn); frz49 measured re-arm churn 107/344 acks racing the guest read, freezing swap marks unserviced -> INFINITE 4C wait + IO-reader pool deadlock at 226 flips | H29 (note 0023, frz38/39): consume ran 100x rarer than service (seen_l=743,480 vs ack=7,489); frz39 caught 84,884 consecutive re-services INSIDE a single guest call (pump `loc_002DB2EB` re-dispatch loop, site 0x002DB2E9) — no host tick can interleave, so the ack-tick variant (v1) was inert. Hardware analogue: TRAP auto-idles at the service READ, so the pump's re-loop gate fails after ONE service. |
| `0036-kewait-shadow-census.patch` | `kernel_bridge.c`: `RECOMP_KEWAIT_TRACE=1` logs each distinct (guest event object, caller) pair entering a `RECOMP_TITLE_KEVENTS` shadowed wait, with timeout | H29 follow-up (note 0023): with TITLE_KEVENTS=1 the XAPILIB USB stack never programs HcControl (frz34-43 emit zero `[OHCI0]` bracket lines vs frz16-33 operational-after-12 s) — a hand-built event in the USB chain now BLOCKS where it used to spin; the census names the stalled chain's objects and callsites from the boot log. |
| `0037-ohci-operational-patience.patch` | `ohci.c`: the controller thread waits for HcControl-operational indefinitely by default (`RECOMP_OHCI_WAIT_S=<n>` restores a deadline) and the give-up now logs loudly | H29 follow-up (note 0023): the old silent 1500-tick (~12 s) break expired before the title's HcControl run-write landed once `RECOMP_TITLE_KEVENTS=1` shifted boot timing (frz34/35/38-43: zero `[OHCI0]` lines, scripted pad dead); frz44 census rules out a shadowed USB-event block — only the two known 4C swap waits exist at boot. |
| `0038-swmark-bit6-gate.patch` | `nv2a_pb_exec.c`: `nv2a_pb_swmethod_latch` does not arm a software-method mark whose PARAM word has bit6 set — the guest's own service gate (pump `sub_002DB250` `loc_002DB2CF`: `test LO8([regs+0x400108]),0x40 ; jne exit`) refuses to dispatch it, so the quartet sits armed forever and queues the real swap/ring-free marks behind it (frz56 core: boot swap chain deadlocked at 1 flip; the same junk marks dominate the rearm census). `RECOMP_SWMARK_GATE=0` opts out | note 0023 (H29): frz56 core anatomy + lifted pump body. |
| `0039-swm-queue-depth-2048.patch` | `nv2a_pb_exec.c`: sw-method pending queue 64 -> 2048 deep | note 0023 (frz62 drop=609 / frz65 drop=1546): the overflow drops swap-completion marks = the terminal plateau signature; healthy runs always drop=0. |
| `0040-swmark-lifecycle-one-owner.patch` | `nv2a_pb_exec.c` (after 0039 depth): strict one-owner mark lifecycle — quartet/INTR/PMC cleared ONLY by the guest service read; an ack pass finding an UNREAD mark restores its flag and changes nothing; queue advances only when idle | note 0023: frz66 (this lifecycle, shallow queue) = deepest run ever (2039+ flips, 5.7 fps) — pair with 0039 depth to kill its residual drop=1016; frz64's 1-flip sample stays open. |
| `0041-class5-drain-gate.patch` | `nv2a_pb_exec.c`: class-5 generator back-pressure — the walk drain edge does not re-latch while a mark is armed-unconsumed (census `gate=`); seen-branch pops only when idle | note 0023 (frz68 enq=1659 vs consume=335; frz69 rering calls=3552): the unconditional drain edge outran the guest's single-trap service rate. |
| `0042-pb-walk-park-at-nop.patch` | `nv2a_pb_exec.c` + `nv2a_pb_scan.c`: hardware-style channel suspension — while a software-method mark is armed-unconsumed, the next trapped NOP does not execute: `pb_consume` leaves its param word unconsumed and the walk stops (`park=`/`s_park_stops` census, `RECOMP_PB_PARK=0` off) | note 0023 (frz70 FINAL: 1807 marks generated vs 462 serviced, backlog starved the swap-slot credit gate, plateau at 1807 with drop=0 — queue depth cannot fix generation/service mismatch). |
| `0043-swmark-irq-post.patch` | `nv2a_pb_exec.c`: `nv2a_pb_swmethod_latch` posts the NV2A PCI line (`xbox_IrqPost(0)`) when it arms the trap quartet — hardware asserts the line at TRAP_DATA time, so the guest graphics ISR queues the GPU DPC promptly (`RECOMP_SWMARK_IRQPOST=0` off) | note 0023 (frz71): with 0042's suspension the channel throttled to the guest's coincidental service rate (~0.5/s, 262 flips). |
