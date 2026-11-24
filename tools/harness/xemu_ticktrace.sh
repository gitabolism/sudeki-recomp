#!/usr/bin/env bash
# tools/harness/xemu_ticktrace.sh — lean healthy-xemu tick-tier caller trace.
# No QMP, no RAM dumps (light footprint by user directive 2026-11-23).
# Probe-attach loop -> scripted START pad -> breakpoint window -> hard kill.
set -u
# self-snapshot law (see ours_run_trace.sh)
if [ -z "${XTT_SNAP:-}" ]; then
  mkdir -p /tmp/opencode
  ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
  export XTT_SNAP=1 ROOT
  SN="/tmp/opencode/.snap-$(basename "$0")-$$"
  cp "$0" "$SN"
  exec bash "$SN" "$@"
fi
BIN=$HOME/Applications/xemu/xemu-0.8.139-x86_64.AppImage
GDBPORT=12353
LOG="$ROOT/logs/xemu-ticktrace-$(date +%H%M%S).out"
echo "log=$LOG"
"$BIN" -gdb "tcp::$GDBPORT" > "$LOG" 2>&1 &
XPID=$!
cat > /tmp/opencode/mini.cmds <<EOF
set confirm off
set architecture i386
target remote 127.0.0.1:$GDBPORT
printf "EIPOK %x\n",\$eip
detach
EOF
OK=0
for i in 1 2 3 4 5 6; do
  sleep 30
  timeout 60 gdb -batch -x /tmp/opencode/mini.cmds > /tmp/opencode/mini.out 2>&1
  if grep -q EIPOK /tmp/opencode/mini.out; then echo "attach ok attempt $i"; OK=1; break; fi
  echo "attach attempt $i: $(tail -1 /tmp/opencode/mini.out)"
done
[ $OK -eq 1 ] || { kill -9 $XPID 2>/dev/null; pkill -9 -x xemu 2>/dev/null; echo NO-ATTACH; exit 1; }
python3 -u "$ROOT/tools/uinput_pad.py" create 6 > /tmp/opencode/uinput_pad.out 2>&1 &
PADPID=$!
timeout -s INT "${XT_TM:-300}" gdb -batch -x "$ROOT/tools/harness/ticktrace.cmds" > /tmp/opencode/ticktrace.out 2>&1
echo "gdb rc=$? lines=$(grep -c "" /tmp/opencode/ticktrace.out)"
kill $PADPID 2>/dev/null
kill -9 $XPID 2>/dev/null
pkill -9 -x xemu 2>/dev/null
grep -oE "\[TSVC|\[TICK1\]|\[TKL2\]|\[FRM ctrl|\[S33E\]" /tmp/opencode/ticktrace.out | sort | uniq -c
echo TICKTRACE-DONE
