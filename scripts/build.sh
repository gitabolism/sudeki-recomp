#!/usr/bin/env bash
# build.sh — configure + build the Linux host executable. Incremental.
# Applies our tracked local toolkit patches first (patches/xboxrecomp/*.patch)
# so the tree stays reproducible from the pin + patch files (THIRD_PARTY.md).
source "$(dirname "$0")/env.sh"

for p in "$ROOT"/patches/xboxrecomp/*.patch; do
    [ -e "$p" ] || continue
    if git -C "$TOOLKIT" apply --check --reverse "$p" 2>/dev/null; then
        : # already applied
    elif git -C "$TOOLKIT" apply --check "$p" 2>/dev/null; then
        git -C "$TOOLKIT" apply "$p" && echo "[build] applied $(basename "$p")"
    else
        die "patch $(basename "$p") does not apply to toolkit $PINNED_COMMIT — re-check pin"
    fi
done

cmake -S "$ROOT" -B "$ROOT/build" >/dev/null
cmake --build "$ROOT/build" -j"$(nproc)" 2>&1 \
    | grep -vE 'warning:|^ +[0-9]+ \||^ +\||^ +\^|note:|In function|In file included' \
    | tail -5
[ -x "$ROOT/build/sudeki" ] && echo "[build] ok: build/sudeki" || die "build failed"
