#!/usr/bin/env bash
# stubs.sh — the lift-stage honesty check: count RECOMP_UNIMPL sites in the
# generated tree and compare against the lifter's own summary.json.
source "$(dirname "$0")/env.sh"

tree=$(grep -h 'RECOMP_UNIMPL("' "$GEN"/recomp_[0-9]*.c 2>/dev/null | wc -l)
summary="$TOOLKIT/tools/recomp/output/summary.json"

if [ -f "$summary" ]; then
    claimed=$("$VENV" -c 'import json;s=json.load(open("'"$summary"'"));print(sum(len(v) for v in s["unimplemented"].values()))')
else
    claimed="?"
fi

echo "[stubs] tree=$tree lifter-summary=$claimed"
[ "$tree" = "$claimed" ] && echo "[stubs] agree" || echo "[stubs] MISMATCH — regeneration drift?"

echo "[stubs] per-mnemonic:"
grep -ho 'RECOMP_UNIMPL("[a-z0-9]*' "$GEN"/recomp_[0-9]*.c 2>/dev/null \
    | cut -d'"' -f2 | sort | uniq -c | sort -rn
