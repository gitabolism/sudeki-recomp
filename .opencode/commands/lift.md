---
description: Regenerate the tree from the XBE (optionally with a measured seed file), rebuild, verify stub agreement
---

Full regeneration. Seed file (measured addresses only — icall_feedback
or run-log derived, NEVER identified_functions.json): $ARGUMENTS

!`scripts/doctor.sh`

If doctor is green: run `scripts/pipeline.sh` (add `--seed <file>` when
a seed was given), then `scripts/build.sh`. Report: functions
translated / failed, the stubs.sh agreement line, and any change in the
stub tally vs 432. If anything drifted (XBE hash, toolkit commit),
STOP — re-pin first, do not regenerate over a dirty input.
