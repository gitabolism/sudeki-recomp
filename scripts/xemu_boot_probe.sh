#!/usr/bin/env bash
# scripts/xemu_boot_probe.sh — bounded xemu boot + QMP guest-RAM dump.
# Boots the configured ISO, waits SETTLE secs, dumps guest memory via QMP
# for offline analysis (rule 5: evidence is a file dump).
#
# usage: scripts/xemu_boot_probe.sh [BIN] [SETTLE] [TAG]
#   BIN    xemu binary (default: the pinned upstream AppImage)
set -u
XEMU_DIR="$HOME/Applications/xemu"
case "${1:-upstream}" in
    upstream) BIN="$XEMU_DIR/xemu-0.8.139-x86_64.AppImage" ;;
    fork)     BIN="$XEMU_DIR/xemu-0.8.136-0-g458730bf5373-x86_64.AppImage" ;;
    *) BIN="$HOME/Applications/xemu/${1}" ;;
esac
SETTLE="${2:-24}"
TAG="${3:-probe}"
QMP_PORT=44559
SECS=$(( SETTLE + 36 ))

scripts/xemu_kill.sh >/dev/null
mkdir -p logs "/tmp/opencode"

EXTRA_ARGS=()
if [ -n "${XEMU_EXTRA:-}" ]; then
    read -r -a EXTRA_ARGS <<< "$XEMU_EXTRA"
fi

timeout --signal=INT "$SECS" "$BIN" \
    -qmp "telnet:127.0.0.1:${QMP_PORT},server,nowait" "${EXTRA_ARGS[@]}" \
    > "logs/xemu-${TAG}-$(date +%H%M%S).log" 2>&1 &

python3 - "$QMP_PORT" "$TAG" "$SETTLE" <<'PYEOF'
import json, socket, sys, time
port, tag, settle = int(sys.argv[1]), sys.argv[2], int(sys.argv[3])
try:
    s = None
    for _ in range(30):
        try:
            s = socket.create_connection(("127.0.0.1", port), 2)
            break
        except OSError:
            time.sleep(0.5)
    if s is None:
        sys.exit("qmp never came up")
except OSError as e:
    sys.exit(f"qmp connect failed: {e}")
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
time.sleep(settle)
print("caps:", qmp({"execute": "qmp_capabilities"}, 0.5)[:80])
r = qmp({"execute": "dump-guest-memory",
         "arguments": {"protocol": f"file:///tmp/opencode/ram-{tag}.elf",
                       "paging": False, "format": "elf"}}, 15)
print("dump:", r.strip()[-140:])
PYEOF

wait
python3 - <<PYEOF
import struct
p = "/tmp/opencode/ram-${TAG}.elf"
try:
    d = open(p, "rb").read()
except OSError:
    print("no dump produced"); raise SystemExit(1)
e_phoff = struct.unpack_from("<Q", d, 0x20)[0]
e_phentsize = struct.unpack_from("<H", d, 0x36)[0]
e_phnum = struct.unpack_from("<H", d, 0x38)[0]
for i in range(e_phnum):
    o = e_phoff + i * e_phentsize
    if struct.unpack_from("<I", d, o)[0] == 1:
        off, va, pa, fsz = struct.unpack_from("<QQQQ", d, o + 0x08)
        nz = sum(1 for j in range(0, min(fsz, 0x4000000), 4096)
                 if d[off + j:off + j + 4096].strip(b"\0"))
        print(f"pa={pa:#010x} size={fsz:#x} nonzero4K={nz}")
for pat in (b"Sudeki", b"SOL.exe"):
    i = d.find(pat)
    print(pat, "->", hex(i) if i >= 0 else "not found")
PYEOF
