# 0018 — Unresolved-ICALL skip corrupts the caller's stack (H23 root cause)

**Observed (session 4j, after patch 0022 closed the audio wall):** with
audio up, three runs crash *deterministically* — identical fault VA
`0xFE087E99` — inside the game's block allocator (`sub_001A9A50` lift,
real fault `0x1A9C1D`, dispatch-misattributed to `sub_001A9A50+0x1CD`).
The allocator splices a free-list node whose link reads `0xFE087E8D`:
an unmapped address between the NV2A and MCPX apertures.

**Hunt (wrappers, this note's tooling):** census wrappers on
`sub_0011F070/sub_0011F0C0` (stream-arena manager), `sub_001A9A50`
(splice core), `sub_001A99A0` (its float-clamp callee), `sub_001A9B60`
(its walk callee) — `logs/run-splice*/run-stomp/run-spesp/run-b60-*`.

Facts established:

1. The stream manager's control global `[0x3DD864]` is written exactly
   once (watch, patch-0018 arming) — no memory poison.
2. The splice core returned with **esp +4, expected +8** on its first
   call per manager pass; every later epilogue `POP32` then read its
   saved registers one dword off: `edi=0x32000` (a size value), and the
   `esi=[edi+0x1C]` load at `0x11F161` pulled the "poison" out of static
   .text bytes (`[0x3201C]=0x41C83`). The wild chain was *stack drift*,
   not a write.
3. All direct callees measured balanced (+8): `99A0` ×N, `B60` ×84.
4. The imbalance source: inside `sub_001A99A0`, the vtable call
   `call dword [eax+4]` at `0x1A9A00` targets **0x00027430** (a real
   thiscall method, `ret 4` — verified by disasm), but `0x27430` was a
   func_id miss → no dispatch entry → `RECOMP_ICALL_SAFE_AT_CC` took its
   skip path `g_esp += 4` (assumes caller cleanup because the lift emits
   `add esp,0x10` after the site). Real callee cleans 4, so the skip
   left the whole caller chain **4 bytes short** — exactly the observed
   drift. `logs/run-b60-221755.out` shows
   `[ICALL] Failed to resolve VA 0x00027430` interleaved with the last
   balanced `B60` and the first short `SPLICEesp`.

**Fix (host-side, pipeline-reproducible):** seed the *measured* address
`0x00027430` (from the run's ICALL feedback) into
`logs/seeds-icall-0011C640.json` (now 11 entries; four more measured the
same session: `0x4FCD0 0x20B710 0x22BA00` + the earlier family) and
regenerate via disasm `--seed-functions` → func_id → abi_analysis →
recomp. With the real lift in the tree the ICALL resolves and the
convention is whatever the code says — skips stop participating.

**Result (logs/run-seed27430-222216.out, run-seed11-222748.out):**
allocator crash gone at 170 s and 240 s; 3429 flips, **152 000 GPU
draws**; wavebank + `SOLWORLDM.gex` streaming; and with
`RECOMP_FB_WINDOW=1 RECOMP_FB_DUMP=reference/fb/frame`: frames finally
carry content — `reference/fb/frame00016.bmp` is the **title screen**
("SUDEKI / Press START / © 2004 Microsoft"), 219 distinct colors. The
"black frames" symptom of H19 was partly downstream of this stack drift:
with the allocator healthy, the FFP subset the executor *does* handle
renders the movie and title screen. Vertex-program batches are still
skipped (the 150-distinct unhandled list stands), but the Menu-stage
artefact exists.

**Lesson:** an unresolved ICALL skip is only safe if the site's cleanup
convention matches reality; `caller_cleans` is a static guess at
indirect sites. A func_id miss can therefore corrupt the stack *without*
any "unresolved" appearing at the crash site — the drift surfaces later,
far away, as a wild pointer. Cross-check every
`[ICALL] Failed to resolve VA` line (they are cheap) instead of trusting
the skip.

**Tools this note:** esp-delta probes (`[SPLICEesp]/[99A0]/[B60esp]`),
corridor snapshot diffs, census wrappers `sub_0011F070/sub_0011F0C0/
sub_001A9A50/sub_001A99A0/sub_001A9B60` (envs `RECOMP_ALLOC_TRACE`,
`RECOMP_STREAM_TRACE`, `RECOMP_SPLICE_TRACE`, `RECOMP_WALK99A0`,
`RECOMP_B60_TRACE`).

## Outcome

True as written: seeding the measured `0x27430` (+ the three later ones)
removed the deterministic allocator crash; the same run then produced
the first content-bearing frame dumps and a full title-screen artefact.
Four further unresolved ICALLs (`0x53740 0x91860 0xAFBF0 0x11AF30`,
logs/run-seed11-222748.out) are the next whack-a-mole batch: seed them
measured, same protocol.
