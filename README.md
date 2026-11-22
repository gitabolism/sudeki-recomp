```
███████╗██╗   ██╗██████╗ ███████╗██╗  ██╗██╗  ██████╗ ███████╗ ██████╗ ██████╗ ███╗   ███╗██████╗
██╔════╝██║   ██║██╔══██╗██╔════╝██║ ██╔╝██║  ██╔══██╗██╔════╝██╔════╝██╔═══██╗████╗ ████║██╔══██╗
███████╗██║   ██║██║  ██║█████╗  █████╔╝ ██║  ██████╔╝█████╗  ██║     ██║   ██║██╔████╔██║██████╔╝
╚════██║██║   ██║██║  ██║██╔══╝  ██╔═██╗ ██║  ██╔══██╗██╔══╝  ██║     ██║   ██║██║╚██╔╝██║██╔═══╝
███████║╚██████╔╝██████╔╝███████╗██║  ██╗██║  ██║  ██║███████╗╚██████╗╚██████╔╝██║ ╚═╝ ██║██║
╚══════╝ ╚═════╝ ╚═════╝ ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝  ╚═╝╚══════╝ ╚═════╝ ╚═════╝ ╚═╝     ╚═╝╚═╝
```

**Sudeki** (Xbox, XDK 5849) as a native Linux executable — static
recompilation, not emulation: every game instruction lifted to C, linked
against a kernel/HAL compatibility layer, running at native CPU speed.

**Bring your own disc.** Nothing from the disc enters this repo — no
bytes, no captures, no analysis artefacts.

---

## Status

| Stage | Status | Proof |
|---|---|---|
| **Lift** | ✅ done | 14,741 functions / 1.12M instructions lifted; 432 stubs counted |
| **Entry** | ✅ done | guest runs its own CRT + startup; 119 kernel ordinals exercised |
| **Threads** | ✅ done | every thread start dispatched; no silent exit |
| **Menu** | ✅ reached | title-screen frame dumps ("SUDEKI / Press START"), movies play, audio up |
| **Gameplay** | 🔶 in progress | level + wavebank streaming ✔, AC'97/MCPX audio ✔, real Xbox pad via OHCI ✔, New Game reachable (title → loading card) ✔, attract/level scene animating ✔ · remaining: **first presented level frame** (one wall left: H29 swap-completion chain, mechanism identified), saves, crash-free soak |

```
Progress to playable:  ██████████████░░░░░░  ~70 %
```

Walls toppled so far (each with a written note *before* the fix — 23
notes, 29 toolkit patches, all reproducible from the disc alone):
async-IO return convention ✔ · boot-archive decoder starvation ✔ ·
unresolved-ICALL stack drift ✔ · GPU→CPU software-method wake chain ✔ ·
host ke-shadow lock race freezing the attract demo ✔ · AC'97
bus-master trap on x86-64 (audio) ✔ · OHCI pad enumeration ✔ ·
flip-settle chain ✔ · post-attract "freeze" = the game's own
level-loading state, not a hang ✔.

## ETA to a playable release

Sessions here are autonomous engineering sprints, not calendar days —
each has cleared 1–3 walls. At the current pace:

* **first playable level, on a pad**: gated on one wall (H29 — swap-completion
  marks dying in the GPU service chain; mechanism identified end-to-end, fix
  candidates queued): ≈ 1–3 more sessions
* **"playable release" grade** (saves, audio rock-solid, early game
  end-to-end, long soak clean): ≈ +3–5 sessions after the first frame
* **Vulkan back-end parity** (staged end-state renderer): ≈ +2–3, gated
  on pixel-identical frame dumps vs the GL path
* **next-gen features** (texture packs → HDR → RT — opt-in only, after
  vanilla parity): parked roadmap

Artefacts back every claim: `CHANGELOG.md` and the note series in
`docs/notes/`.

## Run it (you need the disc)

```bash
# clone the toolkit at the pinned commit (see THIRD_PARTY.md), then:
scripts/doctor.sh           # pins + XBE digest check — a FAIL line is your first task
scripts/pipeline.sh         # XBE → disasm → func_id → abi → gen tree (from default.xbe alone)
scripts/build.sh            # toolkit @ pin + our patch series, applied in order
scripts/run.sh              # the game, with watchdog + one-line classification
```

## How the agent works — the standing contract (`AGENTS.md`)

```
 █████╗   ██████╗  ███████╗ ███╗   ██╗ ████████╗ ███████╗
██╔══██╗ ██╔════╝  ██╔════╝ ████╗  ██║ ╚══██╔══╝ ██╔════╝
███████║ ██║  ███╗ █████╗   ██╔██╗ ██║    ██║    ███████╗
██╔══██║ ██║   ██║ ██╔══╝   ██║╚██╗██║    ██║    ╚════██║
██║  ██║ ╚██████╔╝ ███████╗ ██║ ╚████║    ██║    ███████║
╚═╝  ╚═╝  ╚═════╝  ╚══════╝ ╚═╝  ╚═══╝    ╚═╝    ╚══════╝
```

This repo is worked by an autonomous engineering agent. `AGENTS.md` is its
contract; the rules below are what keep sixty-session-old work trustworthy
when nobody is watching. Several of them are deliberately **not** left to
the agent's good behaviour — they are guardrails designed around the
OpenCode harness itself (see `opencode.jsonc` below):

1. **Never edit generated code.** `src/recomp/gen/` is machine output.
   Every fix is a host function (in `src/recomp_manual.c` or a new `src/*.c`)
   replacing a lifted function *by address*, with the original still
   reachable. The whole tree must stay reproducible from `default.xbe`
   alone via `scripts/pipeline.sh`.
2. **Nothing disc-derived in git.** No bytes, no translated code, no
   analysis artefacts — `.gitignore` is the law, never weakened. `git
   status` is read before every commit.
3. **Verify inputs before trusting conclusions.** The XBE digest and the
   toolkit pin are checked (`scripts/doctor.sh`) at every session start;
   a FAIL line becomes the first task.
4. **One variable at a time; the note comes before the fix.** Every
   crash/hang/anomaly gets a numbered note in `docs/notes/` describing
   what was observed and the hypothesis *before* code changes; the
   outcome is appended to the same note afterwards.
5. **Frame evidence is a file dump, never a screenshot.** The desktop is
   Wayland and X11 screen reads come out black, so visuals are judged
   only through the toolkit's framebuffer-dump hooks (`RECOMP_FB_DUMP`)
   — artefacts land in `reference/` (gitignored).
6. **Reference baseline before behaviour judgements.** "Wrong" is only
   meaningful against a capture of the same scene from a working
   reference (xemu / hardware). Until then, the log reports facts, not
   verdicts.
7. **Lift bugs and runtime gaps stay separate.** An unhandled guest
   instruction is a counted `RECOMP_UNIMPL` stub site; a missing kernel
   ordinal is fixed host-side. Neither is ever papered over by patching
   translated behaviour.
8. **Docs stay lean; every fact has one home.** Rules → `AGENTS.md`,
   live state → `STATE.md`, stage history → `CHANGELOG.md`, per-anomaly
   detail → `docs/notes/`. `README.md` tracks progress at stage
   boundaries with the same artefacts cited in `CHANGELOG.md`.

**Autonomy.** The agent works the full loop by default — analyse, lift,
build, run, diagnose, write overrides, regenerate, test — and stops for
the human only at three gates: a **security** matter (anything reaching
beyond the pinned upstream and the disc you already have), an
**architectural** decision (toolkit pin, base project, override/memory/
threading strategy, redefining a stage), or a **legally-loaded** question
(anything that could put disc-derived bytes in git). Everything else,
including "this run crashed, here are my next five moves", is the
agent's. Sessions never idle-wait: dependent steps are chained into a
single background unit and analysis happens inside in-flight windows.

**Session protocol.** Start: `STATE.md` → two newest notes → `doctor.sh`.
End: update `STATE.md` (stage, hypotheses, queue, last-run verdict,
session log line) and commit — never leave uncommitted source changes.

## Skills — the instruction supply chain (`.opencode/skills/`)

```
███████╗ ██╗  ██╗ ██╗ ██╗      ██╗      ███████╗
██╔════╝ ██║ ██╔╝ ██║ ██║      ██║      ██╔════╝
███████╗ █████╔╝  ██║ ██║      ██║      ███████╗
╚════██║ ██╔═██╗  ██║ ██║      ██║      ╚════██║
███████║ ██║  ██╗ ██║ ███████╗ ███████╗ ███████║
╚══════╝ ╚═╝  ╚═╝ ╚═╝  ╚═════╝ ╚══════╝ ╚══════╝
```

Third-party "skills" are instruction files OpenCode auto-discovers from
`.opencode/skills/` and that then steer the agent — so they get the same
supply-chain treatment as code: first-party sources only, or a full
manual read of every file + a payload scan (network / eval / credential
/ injection patterns) + a pinned upstream commit + a per-folder digest
recorded in `THIRD_PARTY.md`. Nothing auto-installs, and
`scripts/skills_check.sh` fails the check when a folder drifts from its
digest.

Five are installed — chosen because the curated vendor catalogues
(anthropics, microsoft, nvidia, …) cover nothing in this repo's actual
domain: x86 assembly, GDB, valgrind, binary reverse-engineering:

| Skill | Source (pinned) | What it earns us |
|---|---|---|
| `gdb` | `mohitmishra786/low-level-dev-skills` | watchpoints, multi-threaded stepping, core dumps — the post-mortem recipe that closed the H27 lock race |
| `assembly-x86` | same repo | reading lifted/objdump asm and calling-convention forensics — located the 4-byte `esp` drift from an ICALL skip (note 0018) |
| `valgrind` | same repo | memcheck + cache profiling for host-side glue faults where core dumps don't reach |
| `reverse-engineering` | same repo | Ghidra/radare2 triage workflows, paired with the toolkit's own naming pass |
| `diagnosing-bugs` | `mattpocock/skills` | methodology: a tight, red-capable repro before any hypothesis — the mirror image of the note-before-fix loop |

Audit verdicts (Trust Hub / Socket / Snyk where available) and the
vetted-and-rejected candidates — off-mission scope, unvetted reputation,
duplicative — are recorded in `THIRD_PARTY.md`, so the next session does
not re-litigate them. Pins and digests live there too; the README stays
opinion, `THIRD_PARTY.md` stays evidence.

## Ghidra — the static-analysis bench (`scripts/analysis_tools.sh`)

```
 ██████╗  ██╗  ██╗ ██╗ ██████╗  ██████╗   █████╗
██╔════╝  ██║  ██║ ██║ ██╔══██╗ ██╔══██╗ ██╔══██╗
██║  ███╗ ███████║ ██║ ██║  ██║ ██████╔╝ ███████║
██║   ██║ ██╔══██║ ██║ ██║  ██║ ██╔══██╗ ██╔══██║
╚██████╔╝ ██║  ██║ ██║ ██████╔╝ ██║  ██║ ██║  ██║
 ╚═════╝  ╚═╝  ╚═╝ ╚═╝ ╚═════╝  ╚═╝  ╚═╝ ╚═╝  ╚═╝
```

Cross-referencing statically-linked libraries (D3D8LTCG is a big anonymous
blob) and recovering structure for the long tail of Gameplay work: the
bench now includes **Ghidra 12.2** (NSA, Apache-2.0) with the Temurin JDK
25 it demands (GPLv2+CE) — installed with the user's explicit approval on
2026-11-22; software downloads stay an approval-gated act.

The install is reproducible on a fresh machine and supply-chained like
everything else: `scripts/analysis_tools.sh` downloads the pinned archives
(versions + SHA-256 mirrored in `THIRD_PARTY.md`), **verifies the digest
before extracting** — the digest is the trust anchor, not the transport, so
mirror URLs are allowed but a tampered archive refuses to land — installs
under `~/opt` only, and wires the JDK into Ghidra's own launcher alone:
nothing on PATH, no system Java, no sudo, uninstall is `rm -rf`.
`scripts/ghidra.sh` is the entry point, `scripts/doctor.sh` fails the
check when the pins drift.

What it does not change: Ghidra's project database and any decompiled
pseudocode are analysis artefacts — they live in `/tmp/opencode`, never in
git, same law as every disc-derived byte. Section banners in this README
are house-style by recipe: `python3 tools/banner.py WORD` (kerned figlet
ANSI Shadow).

## Guardrails designed around OpenCode (`opencode.jsonc` + `.opencode/commands/`)

```
 ██████╗  ██████╗  ███████╗ ███╗   ██╗  ██████╗  ██████╗  ██████╗  ███████╗
██╔═══██╗ ██╔══██╗ ██╔════╝ ████╗  ██║ ██╔════╝ ██╔═══██╗ ██╔══██╗ ██╔════╝
██║   ██║ ██████╔╝ █████╗   ██╔██╗ ██║ ██║      ██║   ██║ ██║  ██║ █████╗
██║   ██║ ██╔═══╝  ██╔══╝   ██║╚██╗██║ ██║      ██║   ██║ ██║  ██║ ██╔══╝
╚██████╔╝ ██║      ███████╗ ██║ ╚████║ ╚██████╗ ╚██████╔╝ ██████╔╝ ███████╗
 ╚═════╝  ╚═╝      ╚══════╝ ╚═╝  ╚═══╝  ╚═════╝  ╚═════╝  ╚═════╝  ╚══════╝
```

Some of this project's guardrails are **built into OpenCode itself**, not
left to convention: the non-negotiables from the contract are wired into
the harness's permission, policy, and command surfaces, so they hold even
in a session that ignores the prose. A denied permission stops the tool
call before it runs; the `policies` layer sits *below* approvals, so no
"allow always" click can loosen those rules — they are unapprovable, not
just discouraged:

| Layer | Rule | Why |
|---|---|---|
| Injected instructions | `BRIEF.md` + `STATE.md` ride every session (plus `AGENTS.md` auto-discovered) | the agent starts each session already knowing the target, the rules, and the live state |
| `permissions`: edit deny | `src/recomp/gen/*` | rule 1, mechanically — generated code cannot be hand-edited |
| `permissions`: shell deny | `git push *`, `git reset --hard *`, `git rebase *` | never push, never rewrite history |
| `policies`: hard deny | the same three at the policy layer | "allow always" clicks cannot loosen them — these are unapprovable |
| `permissions`: shell ask | `sudo *`, `su *` | system/package state is an approval-gated action |
| `.gitignore` | aggressive, never weakened | the disc-stays-out rule; adding a `!` negation to restore a tracked source file is allowed, removing a guard is not |

The session protocol are defined as slash
commands (`.opencode/commands/`) — `/pickup` and `/handoff` *are* the
start/end checklists, `/note` fires before any fix, `/cycle` runs the
whole loop:

| Command | What it does |
|---|---|
| `/pickup` | session start: read `STATE.md` + newest notes, run `doctor.sh`, propose the next action |
| `/status` | one-line report, no work: stage, current wall, queue head, last-run verdict |
| `/run <args>` | run the game through `scripts/run.sh` (timeout + watchdog) and print the classification: CRASH / HANG / SILENT-EXIT / PROGRESS |
| `/cycle [focus]` | one full work loop: diagnose → note → smallest host-side change → rebuild → run → record (blank focus = `STATE.md` queue item #1) |
| `/note [n] [slug]` | scaffold the next numbered `docs/notes/NNNN-*.md` *before* any fix, pre-filled from the newest run log |
| `/lift [seed]` | regenerate the whole tree from the XBE (optionally with a measured-seed file), rebuild, verify the stub count agrees |
| `/handoff` | session end: update `STATE.md`, commit, state the exact resume point |
| `/setup` | idempotent from-zero bring-up on a fresh machine |

## House rules

1. Never edit generated code — fixes are host-side overrides by address.
2. Nothing disc-derived in git. Frame evidence = file dumps, never screenshots.
3. Every anomaly gets a written note before its fix; one variable at a time.
4. Vanilla first, features later — enhancement code is opt-in and never
   touches the default path.

*Built with the [xboxrecomp](https://github.com/sp00nznet/xboxrecomp)
toolkit (pinned v0.18.0, audited per `THIRD_PARTY.md`) — not affiliated
with Climax or Microsoft; preservation/bring-up project.*
