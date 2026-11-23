#!/usr/bin/env bash
# tools/hangwatch.sh — abort a sudeki run that enters the IRQL-spin hang
# regime (frz145 law: KfRaiseIrql/KfLowerIrql ordinals 160/161 climbing
# by ~100k per summary while PUMP stays tiny). Sends SIGABRT (core via
# coredumpctl) instead of waiting for the timeout KILL (no core).
# usage: hangwatch.sh LOG PID [THRESH]   (THRESH = ord-161 count, default 20000)
set -u
LOG="$1"; PID="$2"; THRESH="${3:-20000}"
while kill -0 "$PID" 2>/dev/null; do
    n=$(grep -o "ordinal 161 x[0-9]*" "$LOG" 2>/dev/null | tail -1 | grep -o "[0-9]*$")
    if [ -n "${n:-}" ] && [ "$n" -ge "$THRESH" ]; then
        echo "hangwatch: ord161 x$n >= $THRESH -> SIGABRT $PID"
        kill -ABRT "$PID"
        exit 0
    fi
    sleep 5
done
echo "hangwatch: pid exited before threshold"
