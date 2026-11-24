#!/usr/bin/env bash
# run_window.sh — plain (no-gdb) census run of N seconds, PAD-driven nav,
# SIGABRT at window end => systemd core for forensics. Usage: run_window.sh N [frzN]
set -u
if [ -z "${RW_SNAP:-}" ]; then
  mkdir -p /tmp/opencode
  ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
  export RW_SNAP=1 ROOT
  SN="/tmp/opencode/.snap-$(basename "$0")-$$"
  cp "$0" "$SN"
  exec bash "$SN" "$@"
fi
N="${1:-900}"
TAG="${2:-frz$RANDOM}"
LOG="$ROOT/logs/run-$TAG-$(date +%H%M%S).out"
PAD=/tmp/opencode/pad-$TAG
: > "$PAD"
cd "$ROOT" || exit 1
env \
  RECOMP_VBLANK=1 RECOMP_PB_EXEC=1 RECOMP_PB_WALK=dma \
  RECOMP_KERNEL_LOG_BUDGET=0 RECOMP_AC97_READY=1 RECOMP_USB=1 \
  RECOMP_ARM_UNTIL_SEEN=0 RECOMP_TITLE_KEVENTS=1 \
  RECOMP_IO_READ_LOG=1 RECOMP_IO_WRITE_LOG=1 RECOMP_IO_WRITE_MAX=0 \
  RECOMP_GATE_TRACE=1 RECOMP_PRES_TRACE=1 RECOMP_EVENT_LOG=1 \
  RECOMP_GSVC_TRACE=1 RECOMP_RD_TRACE=1 RECOMP_MISSING_FILES=all \
  RECOMP_POST_TRACE=1 RECOMP_LOG_ORDS=123,138,145,205,225 \
  RECOMP_PAD_LIVE="$PAD" \
  build/sudeki > "$LOG" 2>&1 &
GAME=$!
until grep -qE "vblank.*ISR claimed|vblank 600|\[PUMP\] n=1 " "$LOG" 2>/dev/null; do
  kill -0 $GAME 2>/dev/null || { echo "died pre-nav; log=$LOG"; exit 1; }
  sleep 2
done
for i in $(seq 1 $((N/8))); do
  kill -0 $GAME 2>/dev/null || break
  echo start >> "$PAD"
  sleep 8
done
kill -ABRT $GAME 2>/dev/null
wait $GAME; code=$?
echo "run-$TAG log=$LOG code=$code"
ls -t /var/lib/systemd/coredump/ 2>/dev/null | grep -m1 "core.sudeki" || echo "(core via coredumpctl)"
