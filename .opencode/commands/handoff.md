---
description: End-of-session ritual — update STATE.md, commit, state the exact resume point
---

Close the session per AGENTS.md:

1. Update STATE.md: stage, wall (one sentence), live hypotheses, queue
   (next three concrete actions), Last run, and a session-log row.
2. Every note touched this session must have its Outcome section
   filled (what was true, not what was attempted).
3. `git status` and READ it — no game files, no generated C, no
   analysis artefacts (rule 2). Commit docs/src/scripts changes with a
   message that says what was learned.
4. Finish or revert any half-done source change; say in STATE.md which.
5. End with exactly: `Next action on resume: <the queue item #1, with
   its command line>` so the next session starts cold and correct.
