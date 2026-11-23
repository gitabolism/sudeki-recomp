#!/usr/bin/env python3
"""tools/ramdiff.py — compare guest globals between our core and an
xemu VA-mapped (paging:true) ELF dump, at the same logical moment.
Both sides addressed by GUEST VA; no anchor math needed on the xemu side.

usage: tools/ramdiff.py XEMU_PAGED.elf CORE [VA ...]
"""
import struct, sys


def xemu_elf(path):
    d = open(path, "rb").read()
    e_phoff = struct.unpack_from("<Q", d, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", d, 0x36)[0]
    e_phnum = struct.unpack_from("<H", d, 0x38)[0]
    segs = []
    for i in range(e_phnum):
        o = e_phoff + i * e_phentsize
        if struct.unpack_from("<I", d, o)[0] == 1:
            off = struct.unpack_from("<Q", d, o + 8)[0]
            va = struct.unpack_from("<Q", d, o + 0x10)[0]
            fsz = struct.unpack_from("<Q", d, o + 0x20)[0]
            segs.append((va, fsz, off))
    def rd(v):
        for va, fsz, off in segs:
            if va <= v < va + fsz:
                return struct.unpack_from("<I", d, off + (v - va))[0]
        return None
    return rd, segs


def core_base(path):
    c = open(path, "rb").read()
    pair = struct.pack("<II", 0x0037306C, 0x00373074)
    b = c.find(pair) - 0x3975E4
    def rd(v):
        if 0 <= b + v < len(c):
            return struct.unpack_from("<I", c, b + v)[0]
        return None
    return rd


DEFAULT_VAS = [0x397110, 0x3DC83C, 0x3DD7F0, 0x3DD7F4, 0x3DC7F8, 0x3DC7FC,
               0x3DD84C, 0x3DD850, 0x3DD8CC, 0x3DC7BC, 0x3DD898, 0x397564,
               0x396E8C, 0x396E98, 0x396E91, 0x397920, 0x397924, 0x3970C8,
               0x3D7620, 0x3D7644, 0x3D7668, 0x3D768C, 0x3D771C, 0x3D7740,
               0x3977FC, 0x397800, 0x37A118]


def main():
    elf, core = sys.argv[1], sys.argv[2]
    vas = [int(v, 16) for v in sys.argv[3:]] or DEFAULT_VAS
    xrd, segs = xemu_elf(elf)
    print("xemu segs:", ["%08X+%06X" % (a, f) for a, f, _ in segs[:4]])
    crd = core_base(core)
    n_same = n_diff = n_na = 0
    for v in vas:
        a, b = xrd(v), crd(v)
        tag = "SAME" if (a is not None and a == b) else ("NA" if a is None or b is None else "DIFF")
        n_same += tag == "SAME"; n_diff += tag == "DIFF"; n_na += tag == "NA"
        print("[%06X] xemu=%s core=%s %s" % (v, "----" if a is None else "%08X" % a,
                                             "----" if b is None else "%08X" % b, tag))
    print("same=%d diff=%d na=%d" % (n_same, n_diff, n_na))


if __name__ == "__main__":
    main()
