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
