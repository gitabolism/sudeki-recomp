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
