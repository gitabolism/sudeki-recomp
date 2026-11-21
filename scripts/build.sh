#!/usr/bin/env bash
# build.sh — configure + build the Linux host executable. Incremental.
source "$(dirname "$0")/env.sh"
cmake -S "$ROOT" -B "$ROOT/build" >/dev/null
cmake --build "$ROOT/build" -j"$(nproc)" 2>&1 \
    | grep -vE 'warning:|^ +[0-9]+ \||^ +\||^ +\^|note:|In function|In file included' \
    | tail -5
[ -x "$ROOT/build/sudeki" ] && echo "[build] ok: build/sudeki" || die "build failed"
