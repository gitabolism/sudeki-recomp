#!/usr/bin/env python3
"""tools/ra2fn.py — symbolize return addresses (x86): find the containing
function by scanning backwards for a plausible prologue after an int3 gap,
then list pointer sites (vtable slots / table refs) for the entry.

usage: tools/ra2fn.py RA [RA ...]     (hex, e.g. 0x15B640)
"""
import struct
import sys

D = open("game/default.xbe", "rb").read()


def off(va):
    return va - 0x10000  # toolkit law: VA = file_off + 0x10000 (xemu-verified)


def entry_of(ra):
    # walk back for 0xCC gap then first non-CC after the gap = entry
    o = off(ra)
    p = o
    while p > 0x1000 and D[p - 1] != 0xCC:
        p -= 1
    while D[p] == 0xCC:
        p += 1
    return 0x10000 + p if 0x1000 < p < 0x1000 + 0x25754C else None


def ptr_sites(target):
    p = struct.pack("<I", target)
    out = []
    i = 0
    while True:
        j = D.find(p, i)
        if j < 0:
            break
        out.append(j)          # XBE file offset; VA = j + 0x10000
        i = j + 1
    return out


def main():
    for ra in sys.argv[1:]:
        ra = int(ra, 16)
        e = entry_of(ra)
        print("ra %06X -> entry %s" % (ra, "%06X" % e if e else "?"))
        if e:
            for s in ptr_sites(e)[:8]:
                print("   ptr at file %06X (VA %06X)" % (s, s + 0x10000))


if __name__ == "__main__":
    main()
