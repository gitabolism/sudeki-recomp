---
description: Fresh-eyes state audit — verify artefacts, prune stale hypotheses, propose compaction
---

Audit this repo's working state with fresh eyes (read-only; propose,
never edit). Order:

1. Read `BRIEF.md`, `AGENTS.md`, `STATE.md`, then the two newest
   `docs/notes/` — exactly what a new session would read.
2. Spot-verify, don't trust: does every artefact cited in STATE.md
   actually exist (`reference/`, `logs/`, `patches/xboxrecomp/`,
   cores under `/tmp/opencode/`)? Do cited commit hashes resolve
   (`git cat-file -e`)? Do pins match `scripts/doctor.sh`?
2b. **Data-leak audit (mandatory, every run)**: derive the machine's
    username (`id -un`), hostname (`hostname`) and any personal email
    at runtime — NEVER hardcode identifier values in repo files. Scan
    HEAD's tracked content (`git grep`), all reachable history
    (`git rev-list --all` + `git grep`), commit author/committer
    metadata, and `git fsck` dangling objects for those values and
    for absolute home paths (`/home/`, `/Users/`, `C:\Users`). Any hit
    = report file + commit list as a FINDING at the top of the audit
    verdict (privacy outranks direction). Also confirm the fast guard
    is installed: `scripts/doctor.sh` LEAK check green and
    `git config core.hooksPath` = `.githooks`.
3. Hunt drift: hypotheses marked ACTIVE that evidence already closed;
   walls described in more than one home; queue items that current
   evidence made moot; session/last-run headers without wall-clock
   times; paragraphs that add nothing actionable for the next session
   (rule 10).
4. Answer the direction question explicitly: given BRIEF's stage
   artefacts, is the next artefact what the current queue is actually
   aiming at, or is the work chasing the most interesting wall rather
   than the closest one?
5. Output: verdict on direction; stale list to prune; a compaction
   plan (STATE.md target ≤ ~300 lines, one home per fact); anything
   claimed-but-unverified labelled as such.
6. Update `README.md` to match verified reality: the stage table
   (Status + Proof cells, citing the same artefacts this audit
   verified), the `Progress to playable` bar and percentage (state
   the evidence for the number), and the `### ETA to a playable
   release` / `### Feature roadmap` subsections of `## Status`
   (windows and walls, never vibes). Only that section's hunks are
   agent-committable (AGENTS rules 9/11): stage exactly them
   (partial add) and commit them alone; any other README
   change stays an uncommitted draft for the user.
