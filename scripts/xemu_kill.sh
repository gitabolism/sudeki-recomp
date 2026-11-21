#!/usr/bin/env bash
# scripts/xemu_kill.sh — stop every xemu instance and wait for the HDD lock
# to be released. (Pattern lives in this file so -f matching never hits the
# caller's own command line.)
PAT='xemu-0\.[0-9].*\.AppImage'
pkill -TERM -f "$PAT" 2>/dev/null
for _ in $(seq 1 20); do
    pgrep -f "$PAT" >/dev/null || { echo "xemu stopped"; exit 0; }
    sleep 1
done
pkill -9 -f "$PAT" 2>/dev/null
sleep 2
pgrep -f "$PAT" >/dev/null && { echo "xemu STILL RUNNING"; exit 1; }
echo "xemu killed"
