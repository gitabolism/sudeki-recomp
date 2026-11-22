#!/usr/bin/env bash
# doctor.sh — verify the pinned world before trusting anything downstream.
# Any FAIL line makes that the first task of the session (see AGENTS.md).
source "$(dirname "$0")/env.sh"

fail=0
ok()   { printf 'ok    %s\n' "$*"; }
bad()  { printf 'FAIL  %s\n' "$*"; fail=1; }
warn() { printf 'warn  %s\n' "$*"; }

# 1. toolkit pin
if [ -d "$TOOLKIT/.git" ]; then
    head_="$(git -C "$TOOLKIT" rev-parse HEAD)"
    if [ "$head_" = "$PINNED_COMMIT" ]; then
        ok "toolkit at pinned v0.18.0 ($head_)"
    else
        bad "toolkit HEAD $head_ != pinned $PINNED_COMMIT"
    fi
else
    bad "toolkit missing at $TOOLKIT (git clone + checkout v0.18.0, see THIRD_PARTY.md)"
fi

# 2. XBE identity
if [ -f "$GAME/default.xbe" ]; then
    sha="$(sha256sum "$GAME/default.xbe" | cut -d' ' -f1)"
    if [ "$sha" = "$XBE_SHA256" ]; then
        ok "game/default.xbe matches pinned SHA-256"
    else
        bad "game/default.xbe SHA-256 $sha != pinned $XBE_SHA256"
    fi
else
    bad "game/default.xbe missing (extract with: python3 -m tools.xiso get <iso> default.xbe -o game/)"
fi

# 3. python env
if [ -x "$VENV" ] && "$VENV" -c 'import capstone' 2>/dev/null; then
    ok "venv + capstone"
else
    bad "venv broken or capstone missing (python3 -m venv .venv && .venv/bin/pip install capstone)"
fi

# 4. analysis outputs (order matters: disasm -> func_id -> abi)
for f in tools/disasm/output/functions.json \
         tools/func_id/output/identified_functions.json \
         tools/abi_analysis/output/abi_functions.json; do
    [ -f "$TOOLKIT/$f" ] && ok "$f" || bad "$f missing (run scripts/pipeline.sh)"
done

# 5. generated tree vs XBE staleness
if [ -d "$GEN" ] && [ -f "$GEN/recomp_dispatch.c" ]; then
    n_chunks=$(ls "$GEN"/recomp_[0-9]*.c 2>/dev/null | wc -l)
    newest_gen=$(find "$GEN" -name '*.c' -newer "$GAME/default.xbe" -print -quit)
    [ -n "$newest_gen" ] && ok "gen tree: $n_chunks chunks" \
                         || warn "gen tree older than the XBE — re-run scripts/pipeline.sh"
else
    bad "generated tree missing (scripts/pipeline.sh)"
fi

# 6. host executable
if [ -x "$ROOT/build/sudeki" ]; then
    ok "build/sudeki exists ($(stat -c %s "$ROOT/build/sudeki") bytes)"
else
    warn "build/sudeki missing (scripts/build.sh)"
fi

# 7. third-party skill digests (supply-chain drift)
if "$ROOT/scripts/skills_check.sh" >/dev/null 2>&1; then
    ok "installed skill digests match THIRD_PARTY.md"
else
    bad "skill drift — run scripts/skills_check.sh and re-vet before trusting skills"
fi

# 8. pinned RE toolchain (Ghidra + JDK) — optional, but drift is a FAIL
OPT="${OPT:-$HOME/opt}"
GHIDRA_DIR="$OPT/ghidra_12.2_PUBLIC"
JDK_DIR="$OPT/jdk-25.0.5.1+2"
if [ -x "$GHIDRA_DIR/support/analyzeHeadless" ]; then
    if [ -x "$JDK_DIR/bin/java" ] \
       && grep -q "^JAVA_HOME_OVERRIDE=$JDK_DIR\$" "$GHIDRA_DIR/support/launch.properties"; then
        ok "analysis toolchain: Ghidra 12.2 @ pinned JDK 25 (~/opt, outside git)"
    else
        bad "analysis toolchain drift — re-run scripts/analysis_tools.sh (re-verifies digests)"
    fi
else
    warn "analysis toolchain not installed (optional: scripts/analysis_tools.sh)"
fi

exit $fail
