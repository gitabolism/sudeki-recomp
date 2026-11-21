#!/usr/bin/env bash
# build.sh — configure + build the Linux host executable. Incremental.
# Applies our tracked local toolkit patches first (patches/xboxrecomp/*.patch)
# so the tree stays reproducible from the pin + patch files (THIRD_PARTY.md).
source "$(dirname "$0")/env.sh"

# The toolkit checkout is kept at the pinned commit; our changes are the patch
# files. Rebuild the patched state from scratch every time: applying the series
# in order onto a clean pin is deterministic, whereas testing each patch
# individually fails once a later patch edits an earlier patch's own lines
# (0002 does exactly that to 0001).
if ! git -C "$TOOLKIT" diff --quiet -- src 2>/dev/null; then
    echo "[build] toolkit tree was dirty — resetting to the pinned commit before applying patches"
fi
git -C "$TOOLKIT" checkout -q -- src 2>/dev/null || true

for p in "$ROOT"/patches/xboxrecomp/*.patch; do
    [ -e "$p" ] || continue
    if git -C "$TOOLKIT" apply "$p"; then
        echo "[build] applied $(basename "$p")"
    else
        die "patch $(basename "$p") does not apply to toolkit $PINNED_COMMIT — re-check pin"
    fi
done

cmake -S "$ROOT" -B "$ROOT/build" >/dev/null
cmake --build "$ROOT/build" -j"$(nproc)" 2>&1 \
    | grep -vE 'warning:|^ +[0-9]+ \||^ +\||^ +\^|note:|In function|In file included' \
    | tail -5
[ -x "$ROOT/build/sudeki" ] && echo "[build] ok: build/sudeki" || die "build failed"
