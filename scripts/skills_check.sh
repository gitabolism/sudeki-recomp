#!/usr/bin/env bash
# skills_check.sh — detect drift in installed third-party skills.
# Recomputes the per-folder digest and compares with THIRD_PARTY.md.
source "$(dirname "$0")/env.sh"

fail=0
for s in gdb assembly-x86 valgrind reverse-engineering diagnosing-bugs; do
    d="$ROOT/.opencode/skills/$s"
    [ -d "$d" ] || { echo "FAIL  skill missing: $s"; fail=1; continue; }
    cur=$(cd "$d" && find . -type f -exec sha256sum {} + | sort -k2 | sha256sum | cut -c1-16)
    if grep -q "\`$cur" "$ROOT/THIRD_PARTY.md"; then
        echo "ok    $s digest $cur"
    else
        echo "FAIL  $s digest $cur not recorded in THIRD_PARTY.md (modified? bumped?)"
        fail=1
    fi
done
exit $fail
