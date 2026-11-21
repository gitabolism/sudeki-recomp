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

| skill | source @ commit | folder digest (sha256 of files) | why |
|---|---|---|---|
| gdb | `mohitmishra786/low-level-dev-skills` @ `18f29518a47fda070e6ec1268cb577692fae9a10` | `f60d52bfe3b88d58…` | host crash work: watchpoints, multi-thread, core dumps |
| assembly-x86 | same repo | `aa9db3fa1bdc0961…` | reading objdump/GDB asm, ABI questions |
| valgrind | same repo | `f6ff5ac6eef907c8…` | host-side memory errors in the runtime glue |
| reverse-engineering | same repo | `589000b10fa30a12…` | Ghidra/radare2 workflows; pairs with toolkit's `ghidra_naming` |
| diagnosing-bugs | `mattpocock/skills` @ `abeeaf95660ba9e8b7c753eb38f441ca707a8823` | `d4232388a3b66c60…` | methodology: tight feedback loops — matches the run.sh work loop |

Vetted and rejected: `wshobson/agents@protocol-reverse-engineering`
(network-protocol scope, out of mission), `gavmor/x86-cpp-reversing-skill`
(9 installs, unvetted reputation), copycat `systematic-debugging` mirrors
(obra's is fine but duplicative of diagnosing-bugs).
