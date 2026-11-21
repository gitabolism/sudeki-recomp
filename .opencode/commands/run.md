---
description: Run the game through the harness with timeout/env args and classify the result
---

Run the harness: `scripts/run.sh $ARGUMENTS`
(blank args = default: 30 s timeout, watchdog armed at 60%).

Report: the one-line classification run.sh printed, the ordinal census,
and — if CRASH or a new subsystem appears — the decisive log lines
(the [CRASH]/[WATCHDOG]/[FILE]/[WAIT] lines that carry the information).
Compare against the "Last run" facts in STATE.md and say explicitly
what moved and what did not. If this was a diagnostic run with no code
change, no commit; just tell me which note number will document it.
