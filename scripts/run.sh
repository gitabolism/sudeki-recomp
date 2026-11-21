#!/usr/bin/env bash
# run.sh — run the game with a hard timeout + watchdog, log everything, and
# print a one-line classification (AGENTS.md: CRASH/HANG/SILENT-EXIT/PROGRESS).
#
# Usage: scripts/run.sh [timeout_secs] [ENV=VAL ...]
#   e.g. scripts/run.sh 30 RECOMP_WATCHDOG_SECS=20
# Logs land in logs/run-<timestamp>.log (gitignored).
source "$(dirname "$0")/env.sh"

TMO="${1:-30}"; shift || true
TS="$(date +%Y%m%d-%H%M%S)"
LOG="$LOGS/run-$TS.log"

# Defaults: watchdog armed at 60% of the timeout unless caller set it.
declare -a EXTRA=()
have_wd=0
for kv in "$@"; do case "$kv" in RECOMP_WATCHDOG_SECS=*) have_wd=1;; esac; done
[ $have_wd -eq 0 ] && EXTRA+=(RECOMP_WATCHDOG_SECS=$(( TMO * 60 / 100 > 0 ? TMO * 60 / 100 : 1 )))

code=0
env "${EXTRA[@]}" "$@" timeout --signal=KILL "$TMO" \
    "$ROOT/build/sudeki" >"$LOG" 2>&1 || code=$?

kernel_calls=$(grep -c '^  \[KERNEL\] #' "$LOG" || true)
crash_line=$(grep -m1 '\[CRASH\]' "$LOG" || true)
wd_line=$(grep -m1 -i 'watchdog' "$LOG" || true)

case $code in
    124|137) verdict="HANG (timeout ${TMO}s)";;
    0)       if [ "${kernel_calls:-0}" -lt 50 ]; then verdict="SILENT-EXIT"; else verdict="returned-0"; fi;;
    134)     verdict="ABORT (watchdog fired)" ;;
    *)       verdict="exit=$code";;
esac
[ -n "$wd_line" ] && verdict="HANG (watchdog abort)"
[ -n "$crash_line" ] && verdict="CRASH"

# Ordinal census: what the guest asked for, most-called first.
census=$(grep -oE 'ordinal [0-9]+' "$LOG" | sort | uniq -c | sort -rn | head -6 | tr '\n' ';' | sed 's/  */ /g')

echo "[run] $(basename "$LOG") code=$code $verdict kernel_calls=$kernel_calls"
[ -n "$census" ] && echo "[run] ordinals: $census"
[ -n "$crash_line" ] && echo "[run] $crash_line"
[ -n "$wd_line" ] && echo "[run] $wd_line"
exit $code
