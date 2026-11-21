#!/usr/bin/env bash
# scripts/xemu_standalone.sh — run xemu WITHOUT the agent running, to test
# whether the inference engine's VRAM affects the black-window behaviour.
# Run this while strata is stopped. It boots the Sudeki ISO for 60 s and
# dumps guest RAM through QMP for later analysis. Watch the window.
cd ~/Applications/xemu || exit 1
timeout --signal=INT 60 ./xemu-0.8.139-x86_64.AppImage \
    -qmp telnet:127.0.0.1:44558,server,nowait \
    > /tmp/opencode/xemu-standalone.log 2>&1 &
XPID=$!
sleep 20
python3 - <<'EOF'
import socket, json, time
try:
    s = socket.create_connection(("127.0.0.1", 44558), 5)
except OSError as e:
    print("QMP not reachable (still fine, just no RAM dump):", e)
    raise SystemExit
s.settimeout(25)
s.recv(4096)
def qmp(o, wait=12):
    s.sendall((json.dumps(o) + "\n").encode()); time.sleep(wait)
    try: return s.recv(65536).decode(errors="replace")
    except socket.timeout: return "TIMEOUT"
qmp({"execute": "qmp_capabilities"}, 0.5)
r = qmp({"execute": "dump-guest-memory",
         "arguments": {"protocol": "file:///tmp/opencode/ram-standalone.elf",
                       "paging": False, "format": "elf"}}, 14)
print("RAM dump:", "saved" if "return" in r else r[:120])
EOF
wait $XPID
echo "done — send me /tmp/opencode/xemu-standalone.log and /tmp/opencode/ram-standalone.elf (if created), then restart strata"
