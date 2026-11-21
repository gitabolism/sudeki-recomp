# Note 0006 — the guest window is placed where the high apertures cannot exist

Date: 2026-11-21 (session 4). Follows note 0005 (which moved the wall here).

## Observed

Startup (`logs/run-20261121-050812.log`):

```
Xbox memory mapped. Offset: 0x7FFF82800000
WARNING: contiguous window at 0x80000000 (native 0x800002800000) failed (error 8);
         pinned physical allocations will fault
  occupant: base 0x800002800000 alloc-base (nil) size 0xffff7ffffd800000 state 0x10000 type 0x0
WARNING: NV2A aperture at 0xFD000000 failed (error 8); D3D register access will fault
WARNING: MCPX aperture at 0xFE800000 failed (error 8); USB/audio register access will fault
WARNING: flash aperture at 0xFF000000 failed (error 8); a title reading flash will fault
WARNING: tiled aperture at 0xF0000000 failed (error 8); rendering writes will fault
```

and the run then faults exactly there (note 0005):

```
[CRASH] … Xbox VA of fault: 0x84000000   in sub_000223F0+0x2F3B
```

## Diagnosis

`xbox_MemoryLayoutInit` places every fixed guest window at
`g_memory_offset + guest VA`. On POSIX it reserves one span for RAM + mirrors
(`xbox_memory_layout.c:2640`, `VirtualAlloc(NULL, span, MEM_RESERVE)`) and the
span is deliberately capped below `0x80000000`, so the high windows
(contiguous `0x80000000`, tiled `0xF0000000`, NV2A `0xFD000000`, MCPX
`0xFE800000`, flash `0xFF000000`) are mapped **outside** the span at
`offset + guest VA`.

The OS chose the span base top-down: `0x7FFF82800000`. The contiguous window
then asks for native `0x7FFF82800000 + 0x80000000 = 0x800002800000`, which is
**above the x86-64 canonical user limit (`0x7FFFFFFFFFFF`)** — `mmap` returns
`EINVAL`, the compat layer reports `ERROR_NOT_ENOUGH_MEMORY` (8), and the
"occupant" line is just the compat layer's free-space descriptor (alloc-base
nil, size ≈ rest of address space). So it is not a collision: the whole upper
2 GB of the guest address space is unrepresentable at that offset, which is
why *five* windows failed at once.

## Planned change (host-side, one variable)

`patches/xboxrecomp/0006-span-window-fits.patch`: after the OS-chosen span
base is returned, probe whether `base + 0xFF000000 (+1 MB)` is mappable; if
not, re-place the span at a low hint (16 GB, 4 GB, 512 MB — tried in order
with the compat `MAP_FIXED_NOREPLACE` path, so a hint can never overwrite
anything) and keep the OS choice if no hint works. One change: placement only,
no window semantics touched.

## Outcome

Patch 0006 applied (`0006-span-window-fits.patch`). It is **insurance, not
the wall**: whether the OS-chosen span base happens to host the high
windows varies run to run (ASLR); with the windows fully mapped
(`logs/run-20261121-051112.log`: "Contiguous window: 64 MB at Xbox VA
0x80000000" + NV2A/MCPX/flash/tiled all present) the guest still faulted
at exactly `0x84000000` — the window *boundary*, not a gap at 0x80000000.
H8 as stated is dead; the follow-up is note 0007.

Also established here: **post-mortem core analysis works** with this
runtime even though live gdb/ptrace does not — systemd-coredump →
`coredumpctl dump <pid> -o core` → `gdb build/sudeki core` gives full
host backtraces (`sub_000223F0 ← sub_000221C0 ← sub_0001F0F0 ← …`),
instruction-level crash sites, and guest-memory reads
(`x/32xw (char*)g_xbox_mem_offset + guest_va`).
