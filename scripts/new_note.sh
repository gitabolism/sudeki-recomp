#!/usr/bin/env bash
# new_note.sh NNNN slug — scaffold the next docs/notes entry (rule 4: the
# note is written before the fix). Prints the path to fill in.
source "$(dirname "$0")/env.sh"

n="${1:-}"
slug="${2:-untitled}"
if [ -z "$n" ]; then
    n=$(ls "$ROOT/docs/notes" 2>/dev/null | grep -oE '^[0-9]{4}' | sort -n | tail -1)
    n=$(printf '%04d' $((10#${n:-0} + 1)))
fi
f="$ROOT/docs/notes/$n-$slug.md"
[ -e "$f" ] && die "$f already exists"

cat > "$f" <<EOF
# Note $n — $slug

Date: $(date +%Y-%m-%d). State: (link STATE.md facts / previous notes).

## Observed
(exact log lines / artefacts, with the run.sh log path)

## Hypothesis
(what is suspected, and what evidence would falsify it — one variable)

## Planned change
(host-side only; never generated code)

## Outcome
(fill in AFTER the experiment: what was true)
EOF
echo "$f"
