#!/usr/bin/env bash
# tools/harness/ours_run_trace.sh — run OUR sudeki under host gdb with breaks
# on lifted guest functions (host symbols). No regen census needed for one-off
# reachability questions. Light recipe (no FB dumps, bounded logs).
set -u
ROOT=/SR
TMO="${1:-480}"
LOG="$ROOT/logs/ours-trace-$(date +%H%M%S).out"
PAD=/tmp/opencode/ours-trace.pad
: > "$PAD"
cd "$ROOT" || exit 1
env \
  RECOMP_VBLANK=1 RECOMP_PB_EXEC=1 RECOMP_PB_WALK=dma \
  RECOMP_KERNEL_LOG_BUDGET=200000 RECOMP_AC97_READY=1 RECOMP_USB=1 \
  RECOMP_ARM_UNTIL_SEEN=0 RECOMP_TITLE_KEVENTS=1 \
  RECOMP_GATE_TRACE=1 RECOMP_PRES_TRACE=1 RECOMP_EVENT_LOG=1 \
  RECOMP_RD_TRACE=1 \
  RECOMP_PAD_LIVE="$PAD" \
  gdb -batch -x "$ROOT/tools/harness/ours_trace.cmds" \
    -ex run --args build/sudeki > "$LOG" 2>&1 &
GAME=$!
until grep -qE "vblank.*ISR claimed|vblank 600|\[PUMP\] n=1 " "$LOG" 2>/dev/null; do
  kill -0 $GAME 2>/dev/null || { echo "died pre-nav"; exit 1; }
  sleep 2
done
for i in $(seq 1 12); do
  kill -0 $GAME 2>/dev/null || break
  echo start >> "$PAD"
  sleep 8
done
# window over: SIGINT the gdb, which forwards to the guest (SIGINT passed through)
kill -INT $GAME 2>/dev/null
sleep 5
pkill -x sudeki 2>/dev/null
sleep 2
kill -9 $GAME 2>/dev/null
wait $GAME; code=$?
echo "ours-trace log=$LOG code=$code hits=$(grep -cE '\[F8240|\[F8C10\]|\[199DF0\]|\[F9D40\]|\[33170\]' "$LOG")"
tail -30 "$LOG"
