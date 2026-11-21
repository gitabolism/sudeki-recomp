---
description: One-line status — stage, wall, queue head, last run verdict
---

Report status, no work:

!`scripts/stubs.sh 2>/dev/null | head -2; echo '--- last run ---'; L=$(ls -t logs/run-*.log 2>/dev/null | head -1); echo "${L:-none}"; [ -n "$L" ] && grep -m1 WATCHDOG "$L" || true; [ -n "$L" ] && grep -m1 '\[CRASH\]' "$L" || true; [ -n "$L" ] && grep -m1 'summary:' "$L" || true; echo '--- git ---'; git log --oneline -3; git status --short | head -5`

Answer from that plus STATE.md: stage reached (with artefact), current
wall in one sentence, last run verdict, and the next queued action.
Keep it under ten lines.
