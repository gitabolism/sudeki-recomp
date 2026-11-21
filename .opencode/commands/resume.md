---
description: Start a session — read state, verify pins, propose the next action
---

Session start per AGENTS.md. Current pins/harness health:

!`scripts/doctor.sh 2>&1; echo '--- last commits ---'; git log --oneline -3; echo '--- status ---'; git status --short`

Then: read STATE.md (already in context) and the two newest files in
docs/notes/. Summarize in five lines max: stage, wall, live hypotheses,
queue item #1, and any doctor FAIL. Do not touch anything else until I
say go — if FAIL lines exist, fixing those IS the first action.
