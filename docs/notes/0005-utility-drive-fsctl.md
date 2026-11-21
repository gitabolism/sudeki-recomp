# Note 0005 — utility-drive FSCTL and the unbounded cache write

Date: 2026-11-21 (session 4). Builds on note 0004. Instrumentation added:
`patches/xboxrecomp/0002-event-signal-trace.patch` (`RECOMP_EVENT_LOG=1`)
and `0003-io-write-csv.patch` (`RECOMP_IO_WRITE_CSV=<path>`: every write as
CSV + periodic guest backtrace from the guest stack).

## Observed (run-20261121-045654, 45 s, and -050143 with backtrace)

1. **The wait is a symptom, not the wall.** `RECOMP_EVENT_LOG=1` over a whole
   run emits exactly one signal: `NtSetEvent token=0x48000002 from=0x0026D5BA`.
   The waited event (`0x48000004` / host `0x5A000024`) is **never signalled**
   by anything.
2. **Full write census (CSV, 94,674 writes in 45 s):**

   | target | caller | count | length | offsets |
   |---|---|---|---|---|
   | `Partition0.img` | 0x00270AAF | 1 | 512 | @2048 |
   | `Partition5.img` | 0x002719B9/1A4E/1A9C | 29 | 4096 | 0…118,784 contiguous (FATX boot region) |
   | `badger.plap` | 0x00271138 | 1 | 512 | sequential |
   | `Cache//boot.baf` | **0x002710FF** | **94,643** | **61,440 each** | contiguous, one 32-bit wrap (`stride -4,294,905,856 = -(2^32-61,440)`), >5.8 GB, still running at watchdog |

3. **Who loops it — the game, not XAPILIB.** Backtrace lines from the guest
   stack (`#scan,20000` and `#scan,40000`, identical):
   `0x000222E0` (the call site into `sub_002710AA`, a XAPI file-write wrapper)
   ← `0x226D5` ← `0x22234` ← `0x1F335` ← `0x1F0E8` ← `0x1E60A` ← `0x21C1F`
   ← `0x26D456` (XAPI thread startup). So SOL engine code in `.text`
   0x1E000–0x22000 drives the write loop through an XAPI helper.
4. **The loop bound is independent of the source.** Reads total exactly
   679,936 B (= whole `D:\boot.Baf`) and then stop, while writes continue
   ~100,000 blocks further. So the game is not "copying until EOF": it sized
   the write from something else, and that something is enormous.
5. **The utility drive's FATX superblock is absurd.** After the format pass,
   `~/.local/share/xboxrecomp/Partition5.img` offset 0:
   `46 41 54 58 | 80 43 92 4F 20 00 00 00 | 01 00 00 00 | 00 00 ff ff | ff ff ff ff …`
   i.e. bytes_per_sector=1, sectors_per_cluster=0, reserved=0, num_fats=0xFFFF,
   **cluster_count=0xFFFFFFFF**. `Partition1/2/3/4.img` are all zero (the
   toolkit allocates images but never formats them).
6. **Every `NtFsControlFile` is refused**: the toolkit stub answers
   `STATUS_NOT_SUPPORTED` for any code. The call at `0x00271AD0` uses
   `0x90020` = `CTL_CODE(FILE_DEVICE_FILE_SYSTEM(9), function 8,
   METHOD_BUFFERED, FILE_ANY_ACCESS)` — the FATX **mount** family — with all
   in/out buffers zero-length, and the guest *ignores the status* and goes
   straight to `NtClose`.

## Hypothesis

**H7:** `IoMountVolume` (FSCTL 0x90020) failing makes XAPI take its
format/fallback path, which leaves the volume geometry uninitialised
(cluster_count 0xFFFFFFFF). The game then sizes its utility-drive cache write
from that geometry and writes 61,440-byte blocks forever; because the copy
never completes, the launch-complete event (`0x48000004`) is never set and
the poller loops in 500 ms waits.

**Falsifier:** answer `0x90020` (and the FATX family) with
`STATUS_SUCCESS` — if the write census still runs unbounded, H7 is dead and
the bound is game-side sizing (then measure the volume-info answers:
`NtQueryVolumeInformationFile`, `GetDiskFreeSpace`, disk IOCTLs).

## Planned change (host-side only, probe)

`patches/xboxrecomp/0004-fsctl-probe.patch`: env-gated `RECOMP_FSCTL_OK=1`
makes `bridge_NtFsControlFile` return `STATUS_SUCCESS` for FATX codes and log
`code/caller/in/out` lengths. Default behaviour unchanged (still the honest
stub), so the probe cannot be mistaken for the fix.

## Outcome (probes run, same day)

**H7 falsified.** `RECOMP_FSCTL_OK=1` (patch 0004) answering the
FILE_DEVICE_FILE_SYSTEM family with success changed nothing: write census
still unbounded (81,671 writes in 40 s), superblock still garbage, event
`0x48000004` still never signalled. The FSCTL stub is a real gap but not this wall.

**The loop's exit is a failing write.** The writer
(`sub_00022270`, driven by `sub_000223F0`) calls the XAPI write helper
`sub_002710AA` and returns `0x80004005` (E_FAIL) when
`bytes_written != requested`. With a host directory behind `Z:` there is no
capacity, every write succeeds, and the loop never ends. Hardware has a
fixed-size FATX volume, so the title hits disk-full and moves on.

**Probe (patch 0005, env `RECOMP_CACHE_CAP_BYTES=16000000`):** writes stopped
at 292, and the launch path **advanced**: new ordinals (187 `NtClose`, 190,
184, 67) appeared that never had during the loop.

**Real fix landed as patch 0005 (`0005-cache-volume-capacity.patch`):**
`recomp_cache_capacity_check()` gives the cache volume (host dir behind
`X:/Y:/Z:`, backed by `Partition3-5.img`) a capacity = largest backing image
size, tracks usage by write high-water marks, and answers
`STATUS_DISK_FULL` past it. Verified at the derived capacity with no env set
(`logs/run-20261121-050812.log`):

```
[FILE] cache volume full (used 786371584 +61440, cap 786432000):
       .../Cache//boot.baf @786370560 +61440 from 0x002710FF
```

**The wall moved, and a new one is now visible.** After the cache branch the
same thread crashes touching **Xbox VA 0x84000000**:

```
[CRASH] signal 11 at rip=…, fault addr=… (unmapped)
  Xbox VA of fault: 0x84000000   in sub_000223F0+0x2F3B
  frames: 0x95800 0x22234 0x1BC800 0x1F335 0xA5800 0x9E000 0x1F0E8 0x1E60A 0x21C1F 0x26D456
```

which is the **contiguous window the loader failed to map at startup**
(`WARNING: contiguous window at 0x80000000 (native 0x800002800000) failed
(error 8); pinned physical allocations will fault` — the occupant line shows
the guest RAM mirror's own reservation sitting on that native address). The
same startup log also reports NV2A / MCPX / flash / tiled apertures unmapped.

Open questions kept honest:
- The capacity emulation is at the write layer. The deeper gap — `IoMountVolume`
  and FATX geometry, which is why the guest's own superblock says
  `cluster_count=0xFFFFFFFF` — remains, and a title that trusts that geometry
  could still mis-size. Patch 0005 is therefore marked as capacity emulation,
  not FATX implementation.
- Only cache volumes are capped; TDATA/UDATA (Partition1) are not yet.
- Whether the game's post-disk-full behaviour is "correct" cannot be judged
  without the xemu reference (rule 6): the fact recorded here is that it
  advances and then faults on an unmapped host window.
