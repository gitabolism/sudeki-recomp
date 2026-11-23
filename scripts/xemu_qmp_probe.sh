#!/usr/bin/env bash
# scripts/xemu_qmp_probe.sh — enumerate the QMP command set of the pinned
# xemu AppImage (the F12/screendump question, notes 0009/0019). Boots xemu
# with the standard title, waits for dashboard, then prints query-commands.
# NEVER run concurrently with a sudeki run (note 0019 GPU contention) or
# with another xemu (QMP port is fixed).
set -u
XEMU_DIR="$HOME/Applications/xemu"
BIN="$XEMU_DIR/xemu-0.8.139-x86_64.AppImage"
QMP_PORT=44559
TMO="${1:-90}"

scripts/xemu_kill.sh >/dev/null
mkdir -p logs

timeout --signal=INT "$TMO" "$BIN" \
    -qmp "telnet:127.0.0.1:${QMP_PORT},server,nowait" \
    > "logs/xemu-qmp-probe-$(date +%H%M%S).log" 2>&1 &

python3 - "$QMP_PORT" <<'PYEOF'
import json, socket, sys, time
port = int(sys.argv[1])
s = None
for _ in range(30):
    try:
        s = socket.create_connection(("127.0.0.1", port), 2)
        break
    except OSError:
        time.sleep(0.5)
if s is None:
    sys.exit("qmp never came up")
s.settimeout(15)
buf = ""
def qmp(o, timeout=15):
    global buf
    s.sendall((json.dumps(o) + "\n").encode())
    t0 = time.time()
    while time.time() - t0 < timeout:
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            if not line.strip():
                continue
            try:
                j = json.loads(line)
            except ValueError:
                continue
            if "event" in j:
                continue
            return line
        try:
            chunk = s.recv(65536)
        except socket.timeout:
            return "TIMEOUT"
        if not chunk:
            return "CLOSED"
        buf += chunk.decode(errors="replace")
    return "TIMEOUT"
qmp({"execute": "qmp_capabilities"}, 10)
r = qmp({"execute": "query-commands"})
if r == "TIMEOUT" or r == "CLOSED":
    print("query-commands:", r)
else:
    try:
        names = sorted(x["name"] for x in json.loads(r)["return"])
        print("QMP commands (%d):" % len(names))
        for n in names:
            print("  ", n)
    except Exception:
        print(r[:4000])
PYEOF

wait
