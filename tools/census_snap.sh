#!/usr/bin/env bash
# tools/census_snap.sh LOG — one-call fingerprint of a frzNN census run
# (note 0023 protocol: pin the exact log path, never ls -t | head).
set -u
L=${1:?log path required}
[ -f "$L" ] || { echo "no such log: $L"; exit 1; }
echo "== $(basename "$L")"
echo "-- fps tail"; grep " fps (" "$L" | tail -4
echo "-- marks";  grep "NOPSW\] latch"  "$L" | tail -1
echo "-- ring";   grep "NOPSW\] rering" "$L" | tail -1
echo "-- classes";grep "NOPSW\] seen="  "$L" | tail -1
echo "-- swap";   grep "\[SWAP\]"       "$L" | tail -1
echo "-- swap2";  grep "\[SWAP2\]"      "$L" | tail -1
echo "-- gate";   grep "\[GATE\]"       "$L" | tail -1
echo "-- drops";  grep -c "\[GFAULT\]"  "$L"
echo "-- kewaits";grep "\[KEWAIT\]"     "$L" | head -6
echo "-- setevents (last 3)"; grep "\[EVENT\]" "$L" | tail -3
