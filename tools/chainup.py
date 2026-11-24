#!/usr/bin/env python3
"""chainup.py — static caller-chain walker (E8 direct calls + dword ptr refs).

Law: .text VA = file offset + 0x10000; slice t = xb[0x1000:0x1000+0x25754C],
site VA = 0x11000 + index into t.

Usage: python3 tools/chainup.py 0x58810 [depth]
Prints, per level upward: function, its direct-call callers (with owner fn),
and dword pointer references (vtable slots) with owner of the containing fn.
"""
import json
import struct
import sys

XB = open('game/default.xbe', 'rb').read()
TEXT = XB[0x1000:0x1000 + 0x25754C]
_FD = json.load(open('xboxrecomp/tools/disasm/output/functions.json'))
ITEMS = _FD if isinstance(_FD, list) else list(_FD.get('functions', []))


def owner(ra):
    for f in ITEMS:
        if not isinstance(f, dict) or 'start' not in f:
            continue
        s = int(f['start'], 16)
        e = int(f.get('end', s), 16)
        if s <= ra < e:
            return f['start']
    return None


def callers(target):
    out = []
    for i in range(len(TEXT) - 5):
        if TEXT[i] == 0xE8:
            rel = struct.unpack_from('<i', TEXT, i + 1)[0]
            site = 0x11000 + i
            if site + 5 + rel == target:
                out.append(site)
    return out


def ptrs(target):
    p = struct.pack('<I', target)
    hits, i = [], 0
    while True:
        j = TEXT.find(p, i)
        if j < 0:
            break
        hits.append(0x11000 + j)
        i = j + 1
    return hits


def main():
    fn = int(sys.argv[1], 16)
    depth = int(sys.argv[2]) if len(sys.argv) > 2 else 4
    seen = set()
    frontier = [fn]
    for lvl in range(depth):
        nxt = []
        for f in frontier:
            if f in seen:
                continue
            seen.add(f)
            cs = callers(f)
            pr = ptrs(f)
            print('L%d %06X  call-callers=%s' % (
                lvl, f,
                [(hex(h), owner(h)) for h in cs]))
            if pr:
                print('        ptrrefs=%s' % [(hex(h), owner(h)) for h in pr[:8]])
            nxt.extend(int(owner(h), 16) for h in cs if owner(h))
        if not nxt:
            print('(chain ends at L%d)' % (lvl + 1))
            break
        frontier = nxt


if __name__ == '__main__':
    main()
