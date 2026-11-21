#!/usr/bin/env bash
# Common environment for the harness scripts. Source me, don't run me.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="$ROOT/.venv/bin/python"
TOOLKIT="$ROOT/xboxrecomp"
GEN="$ROOT/src/recomp/gen"
GAME="$ROOT/game"
LOGS="$ROOT/logs"

PINNED_COMMIT="37b61d589168adee3680fabd51805ee81427bf07"   # v0.18.0, see THIRD_PARTY.md
XBE_SHA256="1e28c3db033a3de471ba78ccdd13a824ebddd17fdd0479bf69a795f63bd92419"

mkdir -p "$LOGS"

py() {   # run a toolkit tool with the venv, from inside the toolkit clone
  (cd "$TOOLKIT" && "$VENV" -m "$@")
}

die() { echo "FATAL: $*" >&2; exit 1; }
