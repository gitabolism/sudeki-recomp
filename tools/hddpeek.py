#!/usr/bin/env python3
"""tools/hddpeek.py — read files out of xemu's qcow2 Xbox HDD (FATX),
read-only, no external deps (qcow2 v2/v3 L1/L2 walk + FATX dir walk).

usage:
  tools/hddpeek.py QCOW2                # probe: header + FX partitions
  tools/hddpeek.py QCOW2 --ls [PREFIX]  # list files (walks all partitions)
  tools/hddpeek.py QCOW2 --get NAME OUT # extract first file whose path contains NAME
"""
import struct, sys

class Qcow2:
    def __init__(self, path):
        self.f = open(path, "rb")
        h = self.f.read(0x100)
        assert h[:4] == b"QFI\xfb", "not qcow2"
        self.ver = struct.unpack_from("<I", h, 4)[0]
        self.cluster_bits = struct.unpack_from("<I", h, 0x24)[0]
        if self.ver >= 3:
            self.vsize = struct.unpack_from("<Q", h, 0x28)[0]
            self.l1_size = struct.unpack_from("<Q", h, 0x48)[0]
            self.l1_off = struct.unpack_from("<Q", h, 0x30)[0]
            self.ptr = 8
        else:
            self.vsize = struct.unpack_from("<I", h, 0x28)[0]
            self.l1_size = 0
            self.l1_off = struct.unpack_from("<Q", h, 0x30)[0]
            self.ptr = 4
        if "--dbg" in path.split() or True:
            import os
            if os.environ.get("QCOW_DBG"):
                for o in range(0x00, 0x68, 8):
                    print("hdr+%02X u64 %s" % (o, hex(struct.unpack_from("<Q", h, o)[0])))
        self.csize = 1 << self.cluster_bits

    def _l2e_off(self, vc):
        self.f.seek(self.l1_off + vc * self.ptr)
        e = int.from_bytes(self.f.read(self.ptr), "little")
        if self.ver >= 3:
            used = e & 0x00FFFFFFFE000000
            comp = (e >> 62) & 1
        else:
            used = e & 0xFFFFFFE0
            comp = (e >> 31) & 1
        return used, comp

    def read(self, off, n):
        out = b""
        while len(out) < n:
            v = off + len(out)
            if v >= self.vsize:
                out += b"\0" * (n - len(out)); break
            vc = v >> self.cluster_bits
            co = v & (self.csize - 1)
            chunk = min(self.csize - co, n - len(out))
            l1, comp1 = self._l2e_off(vc)
            if l1 == 0 or comp1:
                out += b"\0" * chunk; continue
            idx = (vc % (self.csize // self.ptr))
            self.f.seek(l1 + idx * self.ptr)
            e = int.from_bytes(self.f.read(self.ptr), "little")
            used = e & 0x00FFFFFFFE000000 if self.ver >= 3 else e & 0xFFFFFE00
            comp = (e >> (62 if self.ver >= 3 else 31)) & 1
            if used == 0 or comp:
                out += b"\0" * chunk; continue
            self.f.seek(used + co)
            out += self.f.read(chunk)
        return out[:n]

def fatx_probe(img, p):
    b = img.read(p, 0x400)
    if b[:3] != b"FX!": return None
    d = {
        "sect": struct.unpack_from("<H", b, 8)[0],
        "spc": b[10],
        "total_clusters": struct.unpack_from("<I", b, 12)[0],
        "fat_sect": struct.unpack_from("<I", b, 16)[0],
        "root_sect": struct.unpack_from("<I", b, 20)[0],
        "dir_sect": struct.unpack_from("<I", b, 24)[0],
        "state1": struct.unpack_from("<I", b, 32)[0],
        "name": b[48:52],
    }
    return d

def fatx_files(img, base, part, prefix=""):
    sect, spc = part["sect"], part["spc"]
    fat_off = base + part["fat_sect"] * sect
    def fat(cl):
        return struct.unpack_from("<I", img.read(fat_off + cl * 4, 4), 0)[0]
    def walk(sector_off):
        entries = []
        sec = sector_off
        while True:
            blk = img.read(base + sec * sect, sect)
            if not blk: break
            for i in range(0, len(blk), 64):
                e = blk[i:i + 64]
                state = e[0]
                if state == 0: return entries
                attr = e[1]
                name = ""
                for j in range(0x10):
                    ch = struct.unpack_from("<H", e, 4 + j * 2)[0]
                    if ch == 0: break
                    name += chr(ch)
                cl, size = struct.unpack_from("<II", e, 0x24)
                if state in (1, 2) and name:
                    entries.append((name, attr, cl, size))
            sec += 1
            if sec - sector_off > 64: break
        return entries
    def cluster_data(cl, size):
        out = b""
        c = cl
        while len(out) < size:
            off = base + (part["dir_sect"] + (c - 2) * spc) * sect
            out += img.read(off, spc * sect)
            c2 = fat(c)
            if c2 >= 0xFFFFFFF0: break
            c = c2
        return out[:size]
    res = []
    stack = [(base + part["root_sect"] * sect, prefix)]
    for name, attr, cl, size in walk(base + part["root_sect"] * sect):
        pass
    def rec(dir_sect, pfx):
        for name, attr, cl, size in walk(dir_sect):
            path = pfx + "/" + name
            if attr & 0x10:
                # dir: its first cluster holds its entries
                # entry's cluster sector = dir_sect + (cl-2)*spc
                rec(base + (part["dir_sect"] + (cl - 2) * spc) * sect, path)
            else:
                res.append((path, cl, size))
    rec(base + part["root_sect"] * sect, prefix)
    return res, (fat, cluster_data)

def main():
    args = sys.argv[1:]
    img = Qcow2(args[0])
    print("qcow2 v%d vsize=%s cluster=%d" % (img.ver, hex(img.vsize), img.csize))
    blob = img.read(0, min(img.vsize, 0x800000))
    parts, pos = [], 0
    while True:
        i = blob.find(b"FX!", pos)
        if i < 0: break
        p = fatx_probe(img, i)
        if p and p["sect"] in (128, 256, 512, 1024, 2048, 4096):
            parts.append((i, p))
        pos = i + 1
    for i, p in parts:
        print("FX @%s %s sect=%d spc=%d clusters=%d fat=%s root=%s dir=%s" %
              (hex(i), p["name"], p["sect"], p["spc"], p["total_clusters"],
               hex(p["fat_sect"]), hex(p["root_sect"]), hex(p["dir_sect"])))
    if "--ls" in args or "--get" in args:
        pref = args[args.index("--ls") + 1] if "--ls" in args else ""
        for i, p in parts:
            files, _ = fatx_files(img, i, p, p["name"].decode(errors="replace"))
            for path, cl, size in files:
                if pref in path:
                    print("%10d  %s" % (size, path))
        if "--get" in args:
            gi = args.index("--get")
            want, outp = args[gi + 1], args[gi + 2]
            for i, p in parts:
                files, (fat, cd) = fatx_files(img, i, p, p["name"].decode(errors="replace"))
                for path, cl, size in files:
                    if want in path:
                        open(outp, "wb").write(cd(cl, size))
                        print("wrote", outp, path, size)
                        return

if __name__ == "__main__":
    main()
