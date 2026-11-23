#!/usr/bin/env bash
# scripts/xemu_gdbtrace.sh — dynamic trace of the REFERENCE game (rule 6).
# Boots the pinned xemu with the QEMU gdb stub attached and auto-logs
# every hit of a breakpoint list on guest VAs (regs + stack window),
# using host gdb. This answers "who calls X in the healthy game" that
# static xref scans cannot (pure [obj+disp] vtable dispatch).
#
# NEVER run while a sudeki run is live (note 0019 GPU contention) or
# while another xemu holds the ports.
#
# usage: scripts/xemu_gdbtrace.sh VAS [GDBSECS] [BOOTWAIT] [TAG]
#   VAS      comma-separated guest VAs, e.g. 0x15B640,0x159710
#            env BP_CMDS=1 to make breakpoints COMMAND-ONLY (silent auto-
#            continue); default is the same (commands blocks continue).
# output:  /tmp/opencode/gdbtrace-<TAG>.out  (hits + esp windows)
set -u
XEMU_DIR="$HOME/Applications/xemu"
BIN="$XEMU_DIR/xemu-0.8.139-x86_64.AppImage"
VAS="${1:?usage: xemu_gdbtrace.sh VAS [GDBSECS] [BOOTWAIT] [TAG]}"
GDBSECS="${2:-480}"
BOOTWAIT="${3:-30}"
TAG="${4:-trace}"
QMP_PORT=44561
GDB_PORT=12345
GDBSECS2=$(( GDBSECS + 300 ))   # xemu window: gdb + paged dump tail

scripts/xemu_kill.sh >/dev/null
mkdir -p logs /tmp/opencode

# build the gdb command file (soft int3 breaks; each hit logs regs and
# the 14-dword stack window: [retaddr, args...], then continues)
python3 - > "/tmp/opencode/gdbtrace-$TAG.cmds" <<EOF
bps = "$VAS".replace("0x", "").split(",")
print("""set confirm off
set pagination off
set architecture i386
target remote 127.0.0.1:${GDB_PORT}""")
for va in bps:
    va = va.strip()
    print("break *0x%s" % va)
    print("commands")
    print("silent")
    print('printf "[HIT @%s] eip=%%x eax=%%x ecx=%%x edx=%%x esp=%%x ebp=%%x\\n",\$eip,\$eax,\$ecx,\$edx,\$esp,\$ebp' % va)
    print("x/14xw \$esp")
    print("continue")
    print("end")
print("info break")
print("continue")
EOF

LOG="logs/xemu-gdbtrace-$TAG-$(date +%H%M%S).out"
echo "log=$LOG gdb=/tmp/opencode/gdbtrace-$TAG.out"
timeout --signal=INT "$GDBSECS2" "$BIN" \
    -qmp "telnet:127.0.0.1:${QMP_PORT},server,nowait" \
    -gdb "tcp::${GDB_PORT}" > "$LOG" 2>&1 &
XPID=$!

sleep "$BOOTWAIT"
timeout -s INT "$GDBSECS" gdb -batch -x "/tmp/opencode/gdbtrace-$TAG.cmds" \
    > "/tmp/opencode/gdbtrace-$TAG.out" 2>&1
echo "gdb done rc=$?"

# tail of the window: VA-correct paged RAM dump for offline diffs
sleep 20
python3 - "$QMP_PORT" "$TAG" <<'PYEOF'
import json, socket, sys, time
port, tag = int(sys.argv[1]), sys.argv[2]
s = None
for _ in range(20):
    try:
        s = socket.create_connection(("127.0.0.1", port), 2); break
    except OSError:
        time.sleep(0.5)
if not s:
    sys.exit("qmp not up")
try: s.recv(4096)
except Exception: pass
def q(o, w=20):
    s.sendall((json.dumps(o) + "\n").encode()); time.sleep(w)
    try: return s.recv(65536).decode(errors="replace")
    except socket.timeout: return "TIMEOUT"
q({"execute": "qmp_capabilities"}, 0.5)
print("dump:", q({"execute": "dump-guest-memory",
      "arguments": {"protocol": f"file:///tmp/opencode/ram-{tag}.elf",
                    "paging": True, "format": "elf"}}, 25).strip()[-140:])
PYEOF

wait $XPID
grep -c "\[HIT" "/tmp/opencode/gdbtrace-$TAG.out" 2>/dev/null | xargs -I{} echo "hits={}"
