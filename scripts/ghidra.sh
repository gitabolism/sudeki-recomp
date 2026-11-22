#!/usr/bin/env bash
# ghidra.sh — run the pinned Ghidra with its pinned JDK. The JDK lives in
# ~/opt and is on nobody's PATH (see scripts/analysis_tools.sh); Ghidra's
# launcher bootstraps a `java` lookup before it honors launch.properties,
# so this wrapper is the sanctioned entry point.
#   Usage: scripts/ghidra.sh <analyzeHeadless args...>
# Ghidra project dirs belong in /tmp/opencode — never in git.
set -euo pipefail
OPT="${OPT:-$HOME/opt}"
export JAVA_HOME="$OPT/jdk-25.0.5.1+2"
exec "$OPT/ghidra_12.2_PUBLIC/support/analyzeHeadless" "$@"
