#!/usr/bin/env python3
"""banner.py — house-style ANSI Shadow banners for README sections.

Reproduces the kerned box-drawing banners the README uses (SUDEKI RECOMP,
SKILLS, GUARDRAILS): figlet `ANSI Shadow` per glyph, kerning mode, each
glyph block kept at native width, blocks joined with a single space
column, line-ends trimmed. Requires figlet with the `ANSI Shadow.flf`
font (present at /usr/share/figlet/fonts/ on this host).

    python3 tools/banner.py GHIDRA

The earlier README banners were rendered by hand before this script
existed; the output here is style-identical, not byte-identical.
"""
import subprocess
import sys

FONT = "/usr/share/figlet/fonts/ANSI Shadow.flf"


def glyph(ch: str) -> list[str]:
    out = subprocess.run(
        ["figlet", "-k", "-f", FONT, ch], capture_output=True, text=True
    ).stdout
    lines = out.split("\n")[:6]
    width = max(len(l) for l in lines)
    return [l.ljust(width) for l in lines]


def banner(word: str) -> str:
    glyphs = [glyph(c) for c in word]
    return "\n".join(
        " ".join(g[i] for g in glyphs).rstrip() for i in range(6)
    )


if __name__ == "__main__":
    if len(sys.argv) != 2 or not sys.argv[1].strip():
        sys.exit("usage: banner.py WORD")
    print(banner(sys.argv[1].strip().upper()))
