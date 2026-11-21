# Sudeki recomp — working brief

Scratch notes for whoever works on this. Not documentation; it is a starting
position and a list of rules. Rewrite it as real docs once there is something
true to write down.

## Target

One title, one release. Pin the digest before trusting anything else.

| | |
|---|---|
| Title | `Sudeki` |
| Title ID | `0x4D53000A` |
| XDK | `5849` (LTCG build: links `D3D8LTCG`, `XGRAPHCL`) |
| XBE built | 2004-06-26 18:00:49 UTC |
| Internal build path | `d:\sol\code\game\sol\XMaster\SOL.exe` — the engine is called SOL |
| `default.xbe` SHA-1 | `24e532b1aed6aed9412a089f5736fc3bdaa0fa38` |
| `default.xbe` SHA-256 | `1e28c3db033a3de471ba78ccdd13a824ebddd17fdd0479bf69a795f63bd92419` |
| Disc | `Sudeki (World) (En,Fr,De,Es,It).iso` (Redump), 3,812,491,264 bytes |
| Entry point | `0x0026C703` |
| Image size | 4,288,384 bytes; `.text` 2,452,204; 34 sections |
| Kernel imports | 119 ordinals |
| Statically linked libs | D3D8LTCG, DSOUND, LIBCMT, LIBCPMT, XACTENLT, XAPILIB, XBOXKRNL, XGRAPHCL |
| TLS | present, zero-fill 144 bytes |

Measured on this machine, 2026-11-21, with no title-specific seeds: disassembly
finds **15,441 functions** (13,169 in `.text`) over 1,125,376 instructions,
`0 rejected`. Kernel call counts are unusually cranky about locking: 1,275
`RtlLeaveCriticalSection`, 1,076 `RtlEnterCriticalSection`, 905
`RtlInitializeCriticalSection`. This title is genuinely multi-threaded.

Disc has ~24 MB voice banks split as `sound/speech/speech.N.sf`; asset formats
are undocumented. Expect that to be a long tail of the work, not the first wall.

## Base: upstream xboxrecomp, not fci-recomp

Start from `xboxrecomp/templates/new-game`, with upstream pinned at v0.18.0 or
newest tag, and build the **Linux host** (SDL2 + OpenGL 3.3).

Do not fork `~/Projects/fci-recomp` as the base. That project is hard-wired to
two Far Cry Instincts builds: `tools/titles.py` allowlists images by SHA-256,
`data/*.seeds.json` holds 1,777 hand-curated function starts for those images,
and `src/fci_*.c` is CryEngine-specific glue. All of it is dead weight here, and
its vendored `src/xbr` is pinned five releases behind.

Fetched, not vendored: `xboxrecomp/` is in `.gitignore`. Record the pinned commit
in `THIRD_PARTY.md` when it exists.

## Rules, so the game stays the game

This is a childhood game. The way these projects get ruined is not by failing to
boot; it is by quietly changing behaviour and never noticing. Non-negotiable:

1. **Never edit generated code.** Every override is a host function replacing a
   lifted one by address, with the original still reachable (`__imp__sub_X` in
   the TGP482 layout, `recomp_lookup_manual` in xboxrecomp's). Regeneration must
   always be able to reproduce the tree from the XBE alone.
2. **Reference baseline before any change.** Captures from a working reference —
   real hardware, cxbx-Reloaded, or xemu — for the same scene, at the same
   point in the game. Without it, "looks wrong" cannot be separated from
   "recomp is wrong", and every debugging session turns into guessing.
3. **One variable at a time, and keep the logs.** Each crash gets a written
   note in `docs/notes/` before the fix, not after.
4. **Distinguish lift bugs from runtime gaps.** An unhandled instruction lifts to
   a bare `/* mnemonic */` comment — grep the generated tree for those early and
   count them. Do not fix a missing kernel ordinal by patching game code.
5. **Frame captures over screenshots.** Present the picture through a capture
   hook (`RECOMP_FCI_XBR_CAPTURE`-style), because on a Wayland desktop the X11
   screen reads black and you will otherwise trust a black window.
6. **Nothing from the disc and no generated C in git.** The `.gitignore` in this
   repo is deliberately aggressive. Check `git status` before every commit.

## Pipeline

From `docs/GETTING_STARTED.md` in the toolkit, Linux form:

```bash
python3 -m tools.xbe_parser  game/default.xbe --json game/default_analysis.json
python3 -m tools.disasm      game/default.xbe            # code sections; --extra-sections as needed
python3 -m tools.func_id     game/default.xbe -v
python3 -m tools.abi_analysis game/default.xbe -v        # skipping this makes every signature a guess
cmake -S . -B build && cmake --build build              # toolkit: libraries only, never an .exe
python3 -m tools.recomp      game/default.xbe --all --split 250 --gen-dir ../sudeki/src/recomp/gen
cmake -S . -B build && cmake --build build              # the game project builds the .exe
```

Expect the first run to crash at or near the entry point. That is the normal
state, not a failure. Order of work after that: boot → stub hardware → resolve
ICALLs → add kernel/D3D/input as the game asks for it.

Useful tooling in the toolkit: `tools.kernel_audit` (kernel import coverage per
title), `tools.seed_from_log` (turn a run's discovered addresses into seeds, with
two gates that must both pass), `tools.conformance` (lifted C vs the real CPU —
run it before believing any lifter change), `tools/recomp.icall_feedback`.

## Known risks, ranked

1. **D3D8LTCG.** Sudeki links the LTCG variants of the D3D and XGRAPH libraries,
   where library functions take arguments in registers and byte signatures match
   worse than for non-LTCG images. Measured with `fci-recomp`'s stripped-down
   matcher, only 53 of ~235 XDK 5849 signatures land on this image, against 192
   for Far Cry Instincts. That matcher omits the constants fallback the upstream
   matcher uses, so treat 53 as a floor. The fix is `xbe_symbols --sign`, which
   builds signatures out of a corpus of complete configs — this repo's config,
   once complete, helps the next title too.
2. **Threading.** The lock counts above. Thread start routines are invisible to
   static analysis: they are only ever pushed as an argument to
   `PsCreateSystemThreadEx`, get no dispatch entry, and the game then exits
   cleanly having done nothing. This is documented as the single most common
   "it built and did nothing" cause. Check it before debugging anything else.
3. **Undocumented asset formats** (`.sf` speech, CAFF-era audio, the level
   containers). Long tail, no shortcuts.
4. **No Xbox Live / system link.** Out of scope.

## Prior art — check this before spending a day on anything

- Upstream: <https://github.com/sp00nznet/xboxrecomp> — 16 community ports in its
  table, several Playable. Discord `discord.gg/CRpzGWZFcu`. Its own README says
  to ask there **before duplicating effort**. Search that Discord for "Sudeki"
  first; my web searches found nothing, which is weak evidence, not proof.
- The other recompiler: <https://github.com/TGP482/XboxXBERecomp> — used for
  Splinter Cell: Double Agent, BLACK, 007 ×2, Red Dead Revolver. Has per-XDK
  `layout.toml` + `signatures.toml` for 4831/5659/5849 and the `--sign` tool.
- Catalogue: <https://github.com/justxor/awesome-decomp-games-ru/blob/main/catalog/platformy/xbox.md>
- Reference: **Conker: Live & Reloaded already has at least three working
  recomps** (one playable at 60 fps from the European disc). Nobody asked that
  question first. Sudeki appears unclaimed as of 2026-11-21.

## Definition of progress

Not "compiles". Stages, in order, each with an artefact that proves it:

| Stage | Proof |
|---|---|
| Lift | generated tree exists; count of `/* mnemonic */` stubs recorded |
| Entry | guest reaches its own startup code; kernel-call trace shows real ordinals |
| Threads | all thread start routines have dispatch entries; game does not exit silently |
| Menu | frame capture showing the title screen, side by side with the reference |
| Gameplay | a level loads and plays; saves work; audio runs |

Record the stage reached, with its artefact, in `CHANGELOG.md`.
