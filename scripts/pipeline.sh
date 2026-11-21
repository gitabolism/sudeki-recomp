#!/usr/bin/env bash
# pipeline.sh — regenerate everything from the XBE, in dependency order.
# Usage: scripts/pipeline.sh [--seed measured_addresses.json]
# Seeds must be MEASURED addresses (icall_feedback / run-log rets), never
# inferred categories — tools.disasm's own help explains why.
source "$(dirname "$0")/env.sh"

SEED_ARG=()
if [ "${1:-}" = "--seed" ]; then
    [ -f "${2:-}" ] || die "--seed needs a file"
    SEED_ARG=(--seed-functions "$2")
fi

"$ROOT/scripts/doctor.sh" >/dev/null || {
    echo "doctor says the inputs are not trustworthy — fix it first:"
    "$ROOT/scripts/doctor.sh" || true
    exit 1
}

echo "[pipeline] 1/5 xbe_parser"
py tools.xbe_parser ../game/default.xbe --json ../game/default_analysis.json

echo "[pipeline] 2/5 disasm"
py tools.disasm ../game/default.xbe -v "${SEED_ARG[@]}"

echo "[pipeline] 3/5 func_id"
py tools.func_id ../game/default.xbe -v

echo "[pipeline] 4/5 abi_analysis  (must run after func_id — it reads its output)"
py tools.abi_analysis ../game/default.xbe -v

echo "[pipeline] 5/5 recomp -> src/recomp/gen"
py tools.recomp ../game/default.xbe --all --split 250 \
    --gen-dir ../src/recomp/gen --game-name sudeki -v

echo "[pipeline] done. stub tally:"
"$ROOT/scripts/stubs.sh"
