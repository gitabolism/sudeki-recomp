#!/usr/bin/env bash
# scripts/xemu_ref.sh — bounded xemu reference boot with VRAM preflight
# and frame dumps through the QEMU monitor (`screendump`).
#
# Rule 5 of the brief: evidence is a file dump, never a screenshot. The
# monitor socket gives us exact emulator-frame captures without touching
# the Wayland desktop.
#
# Usage: scripts/xemu_ref.sh [SECS] [TAG]
#   SECS  total emulator run time (default 80)
#   TAG   filename prefix (default ref)
#
# Env: XEMU_BIN, XEMU_MON_PORT, XEMU_REF_DIR, XEMU_VRAM_MIN_MIB

set -u
XEMU_BIN="${XEMU_BIN:-$HOME/Applications/xemu/xemu-0.8.139-x86_64.AppImage}"
MON_PORT="${XEMU_MON_PORT:-44557}"
OUT="${XEMU_REF_DIR:-reference/xemu}"
SECS="${1:-80}"
TAG="${2:-ref}"
VRAM_MIN="${XEMU_VRAM_MIN_MIB:-600}"

mkdir -p "$OUT" logs

# --- VRAM preflight: the inference engine reserves ~700 MiB on purpose;
# xemu at 1x native needs ~300-500 MiB. Refuse rather than risk a driver
# reset on a saturated card.
FREE=$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits 2>/dev/null | head -1)
if [ -z "$FREE" ] || [ "$FREE" -lt "$VRAM_MIN" ]; then
    echo "VRAM preflight FAIL: ${FREE:-?} MiB free (<${VRAM_MIN} MiB). Close VRAM users or lower the engine, then retry." >&2
    exit 1
fi
echo "VRAM preflight: ${FREE} MiB free — ok"

STAMP=$(date +%Y%m%d-%H%M%S)
LOG="logs/xemu-${TAG}-${STAMP}.log"

timeout --signal=INT "$SECS" "$XEMU_BIN" \
    -qmp "telnet:127.0.0.1:${MON_PORT},server,nowait" \
    ${XEMU_MEM:+-m "$XEMU_MEM"} \
    > "$LOG" 2>&1 &
XPID=$!

python3 - "$MON_PORT" "$OUT" "$TAG" "$SECS" <<'PYEOF'
import socket, sys, time, json
port, out, tag, secs = int(sys.argv[1]), sys.argv[2], sys.argv[3], int(sys.argv[4])
s = None
for _ in range(120):
    try:
        s = socket.create_connection(("127.0.0.1", port), 2)
        break
    except OSError:
        time.sleep(0.5)
if s is None:
    sys.exit("monitor never came up")

# QMP telnet is NOT an HMP console: the first line is the greeting JSON,
# then qmp_capabilities, then commands only (the old `info status` HMP
# line here desynced the socket — note 0019 follow-up fix).
def qmp(obj):
    s.sendall((json.dumps(obj) + "\n").encode())
    time.sleep(0.5)
    s.settimeout(2.0)
    try:
        return s.recv(65536)
    except socket.timeout:
        return b""
try:
    s.settimeout(3.0)
    print("greeting:", s.recv(65536).decode(errors="replace").strip()[:160])
except socket.timeout:
    pass
print("qmp_caps:", qmp({"execute": "qmp_capabilities"}).decode(errors="replace").strip()[:200])
t0 = time.time()
for m in (5, 15, 30, 45, 60, 75):
    if m > secs - 3:
        break
    while time.time() - t0 < m:
        time.sleep(0.2)
    r = qmp({"execute": "screendump",
             "arguments": {"filename": f"{out}/{tag}-{m:03d}s.ppm"}})
    print(f"dump@{m}s: {r.decode(errors='replace').strip()[:200]}")
print("status:", qmp({"execute": "query-status"}).decode(errors="replace").strip()[:200])
s.close()
PYEOF

wait $XPID
echo "xemu exit=$?  log: $LOG"

shopt -s nullglob
for p in "$OUT"/"$TAG"-*.ppm; do
    magick "$p" "${p%.ppm}.png" && rm "$p"
done
ls -la "$OUT"/"$TAG"-* 2>/dev/null
