#!/usr/bin/env bash
# gdb_probe.sh — attach gdb to the game with scripted, non-interactive commands.
# Usage: scripts/gdb_probe.sh <timeout_secs> <gdb-commands-file> [ENV=VAL ...]
# Output -> logs/gdb-<timestamp>.log (gitignored). Read-only probe: no writes
# to guest or generated code; gdb `run`s the host exe exactly like run.sh.
source "$(dirname "$0")/env.sh"

TMO="${1:?timeout}"; shift
CMDS="${1:?gdb commands file}"; shift || true
TS="$(date +%Y%m%d-%H%M%S)"
LOG="$LOGS/gdb-$TS.log"

env "$@" timeout --signal=KILL "$TMO" \
    gdb -batch -q -x "$CMDS" --args "$ROOT/build/sudeki" >"$LOG" 2>&1 || true

echo "[gdb] $(basename "$LOG")"
grep -c 'probe>' "$LOG" | sed 's/^/[gdb] probe lines: /'
