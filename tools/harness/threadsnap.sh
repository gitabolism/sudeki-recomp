#!/usr/bin/env bash
# threadsnap.sh - plain run to nav-era, then gdb attach + bt of all threads.
# Usage: threadsnap.sh [seconds-after-nav]
set -u
SNAPKEY=TS_SNAP
if [ -z "${!SNAPKEY:-}" ]; then
  mkdir -p /tmp/opencode
  ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
  export ROOT
  SN=/tmp/opencode/snap-threadsnap-$$
  cp "$0" "$SN"
  export TS_SNAP=1
  exec bash "$SN" "$@"
fi
WAIT="${1:-60}"
cd "$ROOT" || exit 1
LOG="$ROOT/logs/threadsnap-$(date +%H%M%S).out"
PAD=/tmp/opencode/pad-ts
: > "$PAD"
env \
  RECOMP_VBLANK=1 RECOMP_PB_EXEC=1 RECOMP_PB_WALK=dma \
  RECOMP_KERNEL_LOG_BUDGET=0 RECOMP_AC97_READY=1 RECOMP_USB=1 \
  RECOMP_ARM_UNTIL_SEEN=0 RECOMP_TITLE_KEVENTS=1 \
  RECOMP_MISSING_FILES=all RECOMP_LOG_ORDS=123,138,145,205,225 \
  RECOMP_PAD_LIVE="$PAD" \
  build/sudeki > "$LOG" 2>&1 &
GAME=$!
until grep -qE "\[PUMP\] n=1 " "$LOG" 2>/dev/null; do
  kill -0 $GAME 2>/dev/null || { echo "died pre-nav; log=$LOG"; exit 1; }
  sleep 2
done
sleep "$WAIT"
gdb -batch -p $GAME \
  -ex "set pagination off" \
  -ex "thread apply all bt 6" \
  > "$LOG.bt" 2>&1
kill $GAME 2>/dev/null; wait $GAME 2>/dev/null
echo "done log=$LOG bt=$LOG.bt"
