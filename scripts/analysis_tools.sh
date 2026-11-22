#!/usr/bin/env bash
# analysis_tools.sh — reproducibly install the pinned reverse-engineering
# toolchain (Ghidra + the JDK it requires) under ~/opt. Outside the repo,
# no sudo, nothing on PATH: the JDK is visible only to Ghidra's own
# launcher via launch.properties. Idempotent; digest gate on every
# download. Pins mirror THIRD_PARTY.md — bump both together.
#
# Approved by the user 2026-11-22 (software-download gate). Ghidra is
# NSA-published Apache-2.0; Temurin JDK is GPLv2+Classpath Exception.
# Analysis artefacts (Ghidra project DBs, decompiled pseudocode) live in
# /tmp/opencode or ~/opt — NEVER in git, same law as every disc-derived
# artefact.
set -euo pipefail

OPT="${OPT:-$HOME/opt}"

# Pinned URLs/digests (mirror of THIRD_PARTY.md). URL/DIR are env-overridable
# for offline installs (file:// or a local mirror) — the digest gate is the
# trust anchor, not the transport, so a mirror cannot smuggle a different tree.
GHIDRA_VERSION="12.2"
GHIDRA_DIR="${GHIDRA_DIR:-ghidra_12.2_PUBLIC}"
GHIDRA_URL="${GHIDRA_URL:-https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.2_build/ghidra_12.2_PUBLIC_20261016.zip}"
GHIDRA_SHA256="2c2cd776db3c1f6a097ba418922378ac3649cadd5e844a6f5f088ae628a7be67"  # digest published in the GitHub release body

JDK_VERSION="25.0.5.1+2"
JDK_DIR="${JDK_DIR:-jdk-25.0.5.1+2}"
JDK_URL="${JDK_URL:-https://github.com/adoptium/temurin25-binaries/releases/download/jdk-25.0.5.1%2B2/OpenJDK25U-jdk_x64_linux_hotspot_25.0.5.1_2.tar.gz}"
JDK_SHA256="760f91b22c3efc9bd1b64ff950598231be82928cc807e30691a0e7a2995a62c0"  # Adoptium-published sha256.txt
# Ghidra 12.2 requires JDK 25 (Ghidra/application.properties:
# application.java.min=25). A JDK 21 tarball was fetched first and
# discarded — do not "helpfully" downgrade it.

MARKER="$OPT/.sudeki-analysis-tools"
want="ghidra=$GHIDRA_VERSION jdk=$JDK_VERSION"
if [ -f "$MARKER" ] && grep -qF "$want" "$MARKER" \
   && [ -x "$OPT/$GHIDRA_DIR/support/analyzeHeadless" ] \
   && [ -x "$OPT/$JDK_DIR/bin/java" ]; then
    echo "ok    analysis toolchain already installed ($want) at $OPT"
    exit 0
fi

fetch_verify() { # url sha256 dest
    local url="$1" sha="$2" dest="$3" got
    if [ -f "$dest" ]; then
        got="$(sha256sum "$dest" | cut -d' ' -f1)"
        [ "$got" = "$sha" ] && { echo "ok    $(basename "$dest") cached, digest verified"; return; }
        rm -f "$dest"
    fi
    echo "fetch $url"
    curl -sL --fail --max-time 1800 -o "$dest" "$url"
    got="$(sha256sum "$dest" | cut -d' ' -f1)"
    if [ "$got" != "$sha" ]; then
        rm -f "$dest"
        echo "FAIL  $(basename "$dest") digest $got != pinned $sha — refusing to extract" >&2
        exit 1
    fi
    echo "ok    $(basename "$dest") digest verified"
}

mkdir -p "$OPT"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# JDK first: Ghidra's launcher validates the JDK before anything else.
fetch_verify "$JDK_URL" "$JDK_SHA256" "$TMP/jdk.tar.gz"
if [ ! -x "$OPT/$JDK_DIR/bin/java" ]; then
    tar xzf "$TMP/jdk.tar.gz" -C "$OPT"
    echo "ok    JDK $JDK_VERSION extracted to $OPT/$JDK_DIR (not on PATH)"
fi

fetch_verify "$GHIDRA_URL" "$GHIDRA_SHA256" "$TMP/ghidra.zip"
if [ ! -x "$OPT/$GHIDRA_DIR/support/analyzeHeadless" ]; then
    unzip -q "$TMP/ghidra.zip" -d "$OPT"
    echo "ok    Ghidra $GHIDRA_VERSION extracted to $OPT/$GHIDRA_DIR"
fi

# The only Java linkage on the machine: one line in Ghidra's own launcher.
lp="$OPT/$GHIDRA_DIR/support/launch.properties"
sed -i "s|^JAVA_HOME_OVERRIDE=.*|JAVA_HOME_OVERRIDE=$OPT/$JDK_DIR|" "$lp"
grep -q "^JAVA_HOME_OVERRIDE=$OPT/$JDK_DIR\$" "$lp"

# Smoke test: the launcher bootstraps a `java` lookup BEFORE it reads
# launch.properties, so it must run through scripts/ghidra.sh, which
# exports the pinned JDK as JAVA_HOME. That wrapper is the sanctioned
# entry point; nothing lands on PATH.
repo="$(cd "$(dirname "$0")/.." && pwd)"
smoke="$("$repo/scripts/ghidra.sh" 2>&1 || true)"   # usage banner exits non-zero
if ! printf '%s' "$smoke" | grep -q "Headless Analyzer Usage"; then
    echo "FAIL  analyzeHeadless smoke test — launcher or JDK broken" >&2
    exit 1
fi

printf '%s installed=%s\n' "$want" "$(date -u +%FT%TZ)" > "$MARKER"
echo "ok    analysis toolchain ready ($want); Ghidra project dirs belong in /tmp/opencode, never in git"
