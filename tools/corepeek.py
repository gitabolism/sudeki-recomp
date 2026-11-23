#!/usr/bin/env python3
"""tools/corepeek.py — read guest-memory records from a sudeki core dump.

The runtime maps guest VA space identity (MEM32(gva) works in wrappers),
so a guest VA like 0x02967A8C is also the host VA: find the core PT_LOAD
covering it and index directly.

usage:
  coredumpctl dump sudeki-frzNN.bin > /tmp/opencode/sudeki.frzNN.core   (once)
  tools/corepeek.py CORE dump 0x02967A8C 0x80        # hexdump at VA
  tools/corepeek.py CORE words 0x018BF14C 0x40       # VA-labelled dwords
  tools/corepeek.py CORE find 0x100000 0x4000000 --dword 0x00336794      # scan
  tools/corepeek.py CORE phys 0xF004000              # show segment map
"""
import struct, sys


def loads(path):
    d = open(path, "rb").read()
    e_phoff = struct.unpack_from("<Q", d, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", d, 0x36)[0]
    e_phnum = struct.unpack_from("<H", d, 0x38)[0]
    segs = []
    for i in range(e_phnum):
        o = e_phoff + i * e_phentsize
        if struct.unpack_from("<I", d, o)[0] != 1:
            continue
        p_offset, p_vaddr = struct.unpack_from("<QQ", d, o + 0x08)
        p_filesz = struct.unpack_from("<Q", d, o + 0x20)[0]
        segs.append((p_vaddr, p_filesz, p_offset))
    return d, segs


def read(d, segs, va, n):
    for v, fs, off in segs:
        if v <= va < v + fs and va + n <= v + fs:
            return d[off + (va - v): off + (va - v) + n]
    return None


def main():
    a = sys.argv[1:]
    core, cmd = a[0], a[1]
    d, segs = loads(core)
    if cmd == "phys":
        for v, fs, off in sorted(segs):
            if fs:
                print("seg va=%010X filesz=%08X" % (v, fs))
        return
    if cmd in ("find", "anchor"):
        if cmd == "anchor":
            # locate g_xbox_mem_offset: search a unique XBE image byte run.
            # guest VA of file offset F in .text == F - 0x1000 + 0x10000.
            xbe = open(a[2], "rb").read()
            pat = xbe[0x336480 - 0x10000 + 0x1000: 0x3364A0 - 0x10000 + 0x1000]
            guest = 0x336480
            lo, hi = 0, 0x7FFFFFFFFFFF
        else:
            lo, hi = int(a[2], 0), int(a[3], 0)
            pat = struct.pack("<I", int(a[a.index("--dword") + 1], 0))
            guest = None
        n = 0
        for v, fs, off in segs:
            s, e = max(v, lo), min(v + fs, hi)
            if e <= s:
                continue
            blk = d[off + (s - v): off + (e - v)]
            if len(blk) < len(pat):
                continue  # zero-size/file-size mismatch guard
            i = 0
            while True:
                j = blk.find(pat, i)
                if j < 0:
                    break
                if guest is not None and not (j % 4):
                    print("hit guest=%06X host=%012X offset=%012X"
                          % (guest, s + j, (s + j) - guest))
                    n += 1
                elif guest is None and not j % 4:
                    print("%08X" % (s + j))
                    n += 1
                if n > 60:
                    return
                i = j + 1
        return
    va, n = int(a[2], 0), int(a[3], 0)
    if "--off" in a:
        va += int(a[a.index("--off") + 1], 0)
    b = read(d, segs, va, n)
    if b is None:
        sys.exit("VA %X not in any PT_LOAD" % va)
    if cmd == "dump":
        for r in range(0, n, 16):
            row = b[r:r + 16]
            print("%08X  %-47s  %s" % (va + r,
                  " ".join("%02X" % c for c in row),
                  "".join(chr(c) if 32 <= c < 127 else "." for c in row)))
    elif cmd == "words":
        for r in range(0, n, 4):
            print("  +%02X [%08X] = %08X" % (r, va + r, struct.unpack_from("<I", b, r)[0]))


if __name__ == "__main__":
    main()
