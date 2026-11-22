#!/usr/bin/env python3
"""Resolve [UNH] method numbers (patch 0029) to NV2A register names.

Usage: tools/unh_names.py logs/run-frz16-*.out [more logs...]

Reads #define NV097_* (any class prefix) from the toolkit headers, then
prints each run's LAST [UNH] table (cumulative counters) with the name
column and a class tag from the method-number ranges:
  0x000-0x0FF object/init  0x100-0x1FF software  0x200-0x2FF context
  0x300-0x5FF state (attrs/blend/tex)  0x600-0x8FF textures/PALETTE
  0x900-0xCFF shaders (VSH/FP/programs)  0xE00-0xFFF attributes/vertex
Method numbers in the table are `method & 0x1FFF` from the pushbuffer
subchannel header (count+inc form may alias; the name table is best-effort).
"""
import re
import sys
import glob

HDRS = [
    "xboxrecomp/src/nv2a/nv2a_regs.h",
    "xboxrecomp/src/kernel/nv2a_pb_exec.c",
]

def load_names():
    names = {}
    for h in HDRS:
        try:
            txt = open(h, errors="replace").read()
        except OSError:
            continue
        for m in re.finditer(r"#\s*define\s+(NV097_[A-Z0-9_]+)\s+0x([0-9A-Fa-f]+)\b", txt):
            name = m.group(1)
            val = int(m.group(2), 16) & 0x1FFF
            if val and name not in names:
                names.setdefault(val, name)
    return names

def cls(m):
    if m <= 0x0FF: return "obj"
    if m <= 0x1FF: return "sw"
    if m <= 0x2FF: return "ctx"
    if m <= 0x5FF: return "state"
    if m <= 0x8FF: return "tex"
    if m <= 0xCFF: return "prog"
    return "attr"

def main():
    names = load_names()
    for pat in sys.argv[1:]:
        for log in sorted(glob.glob(pat)):
            tables, cur = [], []
            for line in open(log, errors="replace"):
                if "[UNH]" in line:
                    m = re.search(r"\[UNH\] m=([0-9A-Fa-f]{4}) n=(\d+)\s+last=([0-9A-Fa-f]{8})", line)
                    if m:
                        cur.append((int(m.group(1), 16), int(m.group(2)), int(m.group(3), 16)))
                elif cur and ("[GPU]" in line or "[NOPSW]" in line):
                    tables.append(cur); cur = []
            if cur:
                tables.append(cur)
            print(f"=== {log}: {len(tables)} tables ===")
            if not tables:
                continue
            for m, n, lp in sorted(tables[-1], key=lambda t: -t[1]):
                nm = names.get(m, "?")
                print(f"  {m:#06x} {cls(m):5s} n={n:<9d} last={lp:#010x}  {nm}")

if __name__ == "__main__":
    main()
