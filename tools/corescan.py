#!/usr/bin/env python3
"""corescan.py — guest-memory forensics on a run core (rule 5: file dumps).

usage: tools/corescan.py CORE [--anchor-g VA --anchor-pair W1,W2] VA...

Re-anchors the guest flat image inside the core (default: identity pair
0037306C,00373074 at guest 0x3975E4, the frz-era law), then for each hex
VA argument prints:
  * the dword stored AT that guest VA (global read), and
  * every guest VA whose dword EQUALS the argument (runtime-table hunt).
"""
import struct
import sys


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    core = open(args[0], 'rb').read()
    rest = args[1:]
    ag, pair = 0x3975E4, (0x0037306C, 0x00373074)
    if '--anchor-g' in rest:
        i = rest.index('--anchor-g')
        ag = int(rest[i + 1], 16)
        pair = tuple(int(x, 16) for x in rest[i + 3].split(','))
        rest = rest[:i] + rest[i + 4:]
    targets = [int(x, 16) for x in rest]

    pat = struct.pack('<II', *pair)
    pos = core.find(pat)
    if pos < 0:
        print('anchor pair not found — re-derive for this era')
        return 1
    base = pos - ag
    print('anchor at core+0x%X -> guest base 0x%X' % (pos, base))

    def rd(g):
        try:
            return struct.unpack_from('<I', core, base + g)[0]
        except Exception:
            return None

    for v in targets:
        at = rd(v)
        hits = []
        pat = struct.pack('<I', v)
        i = 0
        while len(hits) < 12:
            j = core.find(pat, i)
            if j < 0:
                break
            g = j - base
            if 0x10000 <= g < 0x8000000:
                hits.append(g)
            i = j + 1
        print('[%08X]=%s  stored-at: %s' % (
            v, '%08X' % at if at is not None else '??',
            ' '.join('%08X' % h for h in hits) or 'none'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
