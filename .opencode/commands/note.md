---
description: Scaffold a numbered note BEFORE any fix, pre-filled from the newest run log
---

Scaffold a note: `scripts/new_note.sh $1 $2`
($1 = number, optional — auto-increments; $2 = slug).

Pre-fill the Observed section with the decisive lines from the newest
logs/run-*.log (WATCHDOG/CRASH/FILE/WAIT/READ lines and the last
[KERNEL] summary block — grep, don't dump the file). Write a real
Hypothesis with a falsifier, not a wish. Planned change must be
host-side only (never src/recomp/gen/). Stop before changing code:
show me the note first.
