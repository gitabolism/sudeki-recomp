#!/usr/bin/env bash
# scripts/xemu_shotseq.sh — one xemu boot, FB snapshots at several wall
# times (the t=90 single-shot was a torn/FMV frame; selection by stats
# needs multiple samples). Rule 5: evidence is file dumps. NEVER run
# concurrently with a sudeki run (note 0019 GPU contention).
#
# usage: scripts/xemu_shotseq.sh "t1 t2 t3 ..." [TAG]
set -u
XEMU_DIR="$HOME/Applications/xemu"
BIN="$XEMU_DIR/xemu-0.8.139-x86_64.AppImage"
TIMES="${1:-60 90 120 150 180 240}"
TAG="${2:-title}"
QMP_PORT=44559
LAST=$(echo "$TIMES" | tr ' ' '\n' | sort -n | tail -1)
SECS=$(( LAST + 40 ))

scripts/xemu_kill.sh >/dev/null
mkdir -p logs "/tmp/opencode" reference/xemu

timeout --signal=INT "$SECS" "$BIN" \
    -qmp "telnet:127.0.0.1:${QMP_PORT},server,nowait" \
    > "logs/xemu-${TAG}-seq-$(date +%H%M%S).log" 2>&1 &

python3 - "$QMP_PORT" "$TAG" $TIMES <<'PYEOF'
import json, socket, sys, time
port, tag = int(sys.argv[1]), sys.argv[2]
times = sorted(int(t) for t in sys.argv[3:])
s = None
for _ in range(30):
    try:
        s = socket.create_connection(("127.0.0.1", port), 2)
        break
    except OSError:
        time.sleep(0.5)
if s is None:
    sys.exit("qmp never came up")
s.settimeout(30)
try:
    s.recv(4096)
except Exception:
    pass
def qmp(o, wait=12):
    s.sendall((json.dumps(o) + "\n").encode())
    time.sleep(wait)
    try:
        return s.recv(65536).decode(errors="replace")
    except socket.timeout:
        return "TIMEOUT"
print("caps:", qmp({"execute": "qmp_capabilities"}, 0.5)[:80])
prev = 0
for t in times:
    time.sleep(t - prev)
    prev = t
    r = qmp({"execute": "dump-guest-memory",
             "arguments": {"protocol": f"file:///tmp/opencode/ram-{tag}-{t}.elf",
                           "paging": False, "format": "elf"}}, 15)
    print(f"t={t}:", r.strip()[-100:])
PYEOF

wait
for t in $TIMES; do
    RAM="/tmp/opencode/ram-${TAG}-${t}.elf"
    [ -f "$RAM" ] || { echo "t=$t no dump"; continue; }
    python3 tools/xemu_fbpeek.py "$RAM" "reference/xemu/${TAG}-t${t}" || true
done
