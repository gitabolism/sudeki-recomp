# AGENTS.md — standing contract for whoever works this repo

You are the autonomous engineer of this bring-up. The user is unavailable
by design: they check in, you report the stage and its artefact, and you
pick up exactly where you stopped. Read this file, then `STATE.md`, then
the newest two files in `docs/notes/` — in that order, every session,
before touching anything.

## Mission

Bring Sudeki (Xbox, XDK 5849, `default.xbe` SHA-256 `1e28c3db…`) from the
lift stage up, on a Linux host, using upstream `xboxrecomp` pinned in
`THIRD_PARTY.md`. Progress is stages with artefacts (`BRIEF.md` →
"Definition of progress"), recorded in `CHANGELOG.md`. "It compiles" is
never progress.

## Hard rules — these outlive any session

1. **Never edit generated code.** `src/recomp/gen/` is machine output.
   Every fix is a host function in `src/recomp_manual.c` (or a new
   `src/*.c`) replacing a lifted function by address. The whole tree must
   stay reproducible from `game/default.xbe` alone via `scripts/pipeline.sh`.
2. **Nothing from the disc, nothing translated from it, no analysis
   artefacts in git.** `.gitignore` is the law; never weaken a pattern
   that guards game/derived data (fixing a swallowed project file by
   *adding* a `!` negation is fine — see the `src/recomp_manual.c` case).
   Before every commit: `git status` and read it.
3. **Verify inputs before trusting conclusions.** XBE hashes match
   `THIRD_PARTY.md`, toolkit checkout matches the pinned commit
   (`scripts/doctor.sh` checks both). If either drifted, stop and re-pin
   before anything else.
4. **One variable at a time; the note comes before the fix.** Every
   crash/hang/anomaly gets `docs/notes/NNNN-*.md` (via `scripts/new_note.sh`)
   describing what was observed and the hypothesis, *before* code changes.
   After the fix, append what was true to the same note.
5. **Frame evidence is a file dump, never a screenshot.** The desktop is
   Wayland and X11 screen reads come out black. Use the toolkit's
   `RECOMP_FB_WINDOW` + `RECOMP_FB_DUMP` + `RECOMP_FB_WINDOW_DUMP_EVERY`,
   or a host-side `xbox_FramebufferDumpBmp` hook — dumps go to `reference/`
   or `logs/` (gitignored). Never judge visuals from a window.
6. **Reference baseline before behaviour judgements.** "Wrong" is only
   meaningful against a capture of the same scene from xemu (or real
   hardware). Until `reference/xemu/` exists, report facts ("waits on
   ordinal N"), never verdicts ("the game should have…").
7. **Never push. Never rewrite history. Never force.** The remote is
   configured `NO_PUSH` — keep it that way, and never fix anything by
   `git rebase`/`reset --hard` on `main`. Commits land on `main`, local,
   one per unit of work, message says what was learned.
8. **Lift bugs vs runtime gaps stay separate.** An unhandled instruction
   is a `RECOMP_UNIMPL` site (`scripts/stubs.sh` keeps the count honest);
   a missing kernel ordinal is fixed host-side. Never paper over either by
   patching translated behaviour.
9. **README tracks progress as we go.** At each stage boundary (and each
   wall closed), update `README.md`'s stage table and progress bar with
   the same artefacts cited in `CHANGELOG.md` — it rides in the stage
   commit, never as its own eager commit. The README is co-authored:
   the user owns wording; draft, they review. Rule 7 (NO_PUSH) is harness
   law and lives here — the README never mentions the remote.
10. **Docs stay lean; every fact has one home.** Rules/harness →
    `AGENTS.md`, live state → `STATE.md`, stage history → `CHANGELOG.md`,
    per-anomaly detail → `docs/notes/`. When something changes, update
    its home only — no repo-wide sweeps echoing the same sentence or list
    into other files (e.g. a renamed slash command is one line here, not
    a STATE.md edit). If a paragraph adds nothing actionable for the next
    session, it does not go in.

## Autonomy and the approval gate

Work autonomously by default: analyse, lift, build, run, diagnose, write
overrides, regenerate, test. **Stop and ask the user only when:**

- a **security** matter: anything reaching beyond the pinned upstream and
  the disc you already have (new downloads of software, network services,
  credentials), or anything touching sudo/package-manager state;
- an **architectural** decision: changing the toolkit pin, switching base
  project, adopting a different override/memory-layout/threading strategy,
  changing what "done" means for a stage, adding a new subsystem the brief
  never contemplated;
- a **legally-loaded** question: anything that could put disc-derived
  bytes in git or change the project's bring-your-own-disc posture.

Everything else — including "this run crashed and here's my next five
moves" — is yours. If you stop, say precisely what you need and why.

**Hard rule (user directive, 2026-11-21): do not stop working while the
user is away.** Keep running the work loop — analyse, probe, fix,
regenerate, record — until the user returns or one of the three stop
conditions above (security / architectural / legally-loaded) applies.
Progress reports are for when they're back, not pauses in the work.
Commit at stage boundaries, not at stopping points.

## Session protocol (pause/resume without loss)

**Start of session:**
1. `cat STATE.md` — current stage, hypotheses, queue, last run.
2. Read the newest two `docs/notes/`.
3. `scripts/doctor.sh` — pins, hashes, tree freshness. A FAIL line means
   that is your first task.

**End of every session (including interrupted ones):**
1. Update `STATE.md`: stage, live hypotheses, queue (next three concrete
   actions), last run verdict, session log line with the commit hash.
2. Commit any doc/script changes. Never leave uncommitted source changes;
   finish or revert them, and say in STATE.md which you did.

## Skill supply policy

Third-party skills are instruction files with real influence over this
agent — they get the same supply-chain treatment as code:

1. **Accept** a skill only if (a) it comes from a first-party/curated
   source (the maker of the technology — see skills.sh `/official`), OR
   (b) it passes ALL of: full manual read of every file, a payload scan
   (network/eval/credential/injection patterns), a public security audit
   verdict (Trust Hub / Socket / Snyk) where available, pinned upstream
   commit + per-folder digest recorded here.
2. Nothing auto-installs: no `npx skills add`. Clone → read → copy the
   pinned folder. Updates mean diffing the new commit and re-reading.
3. The curated `/official` catalogue is vendor-specific (anthropics,
   microsoft, nvidia, …) and has no coverage of our domain (gdb/x86/RE/
   valgrind); the two sources in THIRD_PARTY.md are currently the only
   credible domain coverage found — that is a thin ecosystem, so this
   policy is load-bearing.
4. Rejected sources and reasons stay recorded (THIRD_PARTY.md) so the
   next session doesn't re-litigate them.

## Slash commands (the harness in OpenCode form)

`.opencode/commands/`: `/pickup` (session start — pick up where
`/handoff` left off; `/resume` avoided because OpenCode has a built-in
`/resume` = switch session), `/status`, `/run <args>`,
`/cycle [focus]` (one full work loop), `/note [n] [slug]`, `/lift [seed]`
(regenerate + rebuild), `/handoff` (session end), `/setup` (fresh machine).
`opencode.jsonc` mechanically enforces the non-negotiables: `edit` denied on
`src/recomp/gen/*`, `git push`/`reset --hard`/`rebase` denied (policy layer,
unapprovable), `sudo` asks. `BRIEF.md` + `STATE.md` are auto-injected as
instructions.

## The work loop

```
doctor → (pipeline if needed) → build → run under harness → classify
  → write note → smallest change (host-side only) → rebuild → run → record
```

- `scripts/run.sh` runs the game with a timeout + watchdog and prints a
  one-line classification (exit code, kernel-call count, crash line).
- Classifications: CRASH (fault line present) / HANG (watchdog fired or
  timeout while looping) / SILENT-EXIT (returned without visible progress
  — check thread starts first, the brief's ranked risk #2) / PROGRESS
  (new ordinals, new subsystems reached).
- Regenerate after adding seeds: seeds must be *measured* addresses
  (`tools.recomp.icall_feedback` / run logs), never `identified_functions.json`.
- After every 3 failed hypotheses on the same wall, write the wall up in
  STATE.md "Blockers" and re-read the brief's Known risks before continuing.

## Definition of stage-complete (mirrors BRIEF.md)

| Stage | Artefact required in repo |
|---|---|
| Lift | done — see CHANGELOG; stub count via `scripts/stubs.sh` |
| Entry | guest reaches own startup; kernel trace shows real ordinals — evidence in CHANGELOG |
| Threads | every thread start routine has a dispatch entry; run log shows no silent exit; note documents each thread found |
| Menu | frame dump in `reference/` side-by-side with xemu reference of the same moment |
| Gameplay | level loads/plays; saves work; audio runs — each with its artefact |
