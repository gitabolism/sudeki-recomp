#!/usr/bin/env bash
# scripts/xemu_titlecap.sh — xemu reference title-screen capture (Menu
# artefact side-by-side, rule 6). Boots the reference ISO, settles,
# dumps guest RAM over QMP, reconstructs the NV2A FB offline.
# NEVER run while a sudeki run is live (GPU contention; note 0019).
#
# usage: scripts/xemu_titlecap.sh [SETTLE] [TAG]
set -u
SETTLE="${1:-90}"
TAG="${2:-title}"
scripts/xemu_boot_probe.sh upstream "$SETTLE" "$TAG" || exit 1
RAM="/tmp/opencode/ram-${TAG}.elf"
[ -f "$RAM" ] || { echo "no RAM dump"; exit 1; }
python3 tools/xemu_fbpeek.py "$RAM" "reference/xemu/${TAG}"
