#!/usr/bin/env python3
"""tools/xemu_rampeek.py — read Xbox guest memory from an xemu
`dump-guest-memory` ELF and print engine-global state, comparable to
our host-core reads (same VAs as the gdb probes in note 0025).

usage: tools/xemu_rampeek.py RAM.elf VA [VA...]   (prints 1 dword each)
       tools/xemu_rampeek.py RAM.elf --blk VA:NCOUNT  (blocks)
Xbox RAM PT_LOAD carries paddr == physical; game VAs map to PA as
PA = VA & 0x0FFFFFFF (RAM window), plus the 0x80000000 kernel mirror —
we search every PT_LOAD for the physical address.
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
        if struct.unpack_from("<I", d, o)[0] == 1:
            p_off = struct.unpack_from("<Q", d, o + 0x8)[0]
            p_vaddr = struct.unpack_from("<Q", d, o + 0x10)[0]
            p_filesz = struct.unpack_from("<Q", d, o + 0x20)[0]
            segs.append((p_vaddr, p_filesz, p_off))
    return d, segs

def main():
    args = sys.argv[1:]
    d, segs = loads(args[0])
    print("PT_LOADs:", [(hex(s[0]), hex(s[1])) for s in segs])
    def read(pa):
        for base, size, off in segs:
            if base <= pa < base + size:
                o = off + (pa - base)
                return struct.unpack_from("<I", d, o)[0]
        return None
    for a in args[1:]:
        if a.startswith("--blk"):
            spec = args[args.index(a) + 1]
            va, n = spec.rsplit(":", 1)
            va, n = int(va, 0), int(n)
            pa = va & 0x0FFFFFFF
            vals = [read(pa + i * 4) for i in range(n)]
            print("blk %08X x%d:" % (va, n),
                  " ".join("%08X" % v if v is not None else "????????" for v in vals))
        else:
            va = int(a, 0)
            v = read(va & 0x0FFFFFFF)
            print("%08X = %s" % (va, "%08X" % v if v is not None else "?"))

if __name__ == "__main__":
    main()
