#!/usr/bin/env bash
# tools/hangwatch.sh — abort a sudeki run whose vblank clock died (frz145
# regime law, v2): the true stall signature is TOO FEW kernel calls and a
# frozen "vblank N (tick)" count (healthy = ~45/s, printed every 600).
# v1's high-ordinal-161 threshold was convicted: healthy eras spin
# 160/161 at >100k/s by design (48M per normal run).
# usage: hangwatch.sh LOG PID [GRACE_S]  (abort if vblank_count < elapsed*10
# after GRACE_S, default 90; also aborts if 0 ticks printed after GRACE_S)
set -u
LOG="$1"; PID="$2"; GRACE="${3:-90}"
T0=$(date +%s)
while kill -0 "$PID" 2>/dev/null; do
    sleep 5
    ELAPSED=$(( $(date +%s) - T0 ))
    [ "$ELAPSED" -lt "$GRACE" ] && continue
    V=$(grep -o "vblank [0-9]* (tick)" "$LOG" 2>/dev/null | tail -1 | awk '{print $2}')
    V=${V:-0}
    if [ "$V" -lt $(( ELAPSED * 10 )) ]; then
        echo "hangwatch: vblank=$V < elapsed*10 ($ELAPSED s) -> SIGABRT $PID"
        kill -ABRT "$PID"
        exit 0
    fi
done
echo "hangwatch: pid exited before trigger"
