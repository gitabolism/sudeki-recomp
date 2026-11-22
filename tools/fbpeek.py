#!/usr/bin/env python3
"""Framebuffer-dump forensics for RECOMP_FB_DUMP artefacts (rule 5:
file dumps, never screenshots).

Usage:
  tools/fbpeek.py reference/fb/frz17                # stats for all matching dumps
  tools/fbpeek.py reference/fb/frz17 --png 12       # + extract dump 12 as PNG
  tools/fbpeek.py reference/fb/frz17 --diff         # per-frame change % vs previous

Prints per dump: distinct colors, dominant color share, mean luma, and
(with --diff) the fraction of pixels changed vs the previous dump — the
cheap signature that separates a frozen frame (0 %) from a live scene.
"""
import glob
import os
import struct
import sys
import zlib
import collections


def open_bmp(path):
    d = open(path, "rb").read()
    if d[:2] != b"BM":
        raise SystemExit(f"{path}: not a BMP")
    off = struct.unpack_from("<I", d, 10)[0]
    w = struct.unpack_from("<i", d, 18)[0]
    h = struct.unpack_from("<i", d, 22)[0]
    return d, off, w, abs(h)


def px(d, off, w, h, x, y):  # BMP rows bottom-up per our dumps
    i = off + (h - 1 - y) * w * 3 + x * 3
    return d[i], d[i + 1], d[i + 2]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    pat = args[0] if args else sys.exit(__doc__)
    if not pat.endswith(".bmp") and not glob.glob(pat):
        pat += "*.bmp" if not pat.endswith("*") else ""
    files = sorted(glob.glob(pat) + glob.glob(pat + "*.bmp"))
    files = [f for f in files if f.endswith(".bmp")]
    if not files:
        sys.exit(f"no dumps match {pat}")
    want_png = None
    do_diff = "--diff" in sys.argv
    if "--png" in sys.argv:
        want_png = int(sys.argv[sys.argv.index("--png") + 1])
    prev = None
    for f in files:
        d, off, w, h = open_bmp(f)
        cols = collections.Counter(d[i:i + 3] for i in range(off, len(d) - 2, 3))
        top, tn = cols.most_common(1)[0]
        n = w * h
        mean = sum((r + g + b) * c / 3 for (r, g, b), c in cols.items()) / n
        line = (f"{os.path.basename(f)}  colors={len(cols):6d} "
                f"top={top.hex()} {100.0 * tn / n:5.1f}%  luma={mean:6.1f}")
        if do_diff and prev is not None:
            step = max(1, n // 50000)  # subsample for speed
            changed = tot = 0
            for i in range(off, min(off + n * 3, len(d) - 2), 3 * step):
                tot += 1
                if d[i:i + 3] != prev[i:i + 3]:
                    changed += 1
            line += f"  d={100.0 * changed / tot:5.2f}%"
        prev = d
        print(line)
        if want_png is not None and f == files[want_png]:
            rows = b""
            for y in range(h - 1, -1, -1):
                row = d[off + y * w * 3: off + y * w * 3 + w * 3]
                rows += b"\x00" + bytes(b for rgb in
                                        zip(row[0::3], row[1::3], row[2::3])
                                        for b in rgb)

            def chunk(t, p):
                c = struct.pack(">I", len(p)) + t + p
                return c + struct.pack(">I", zlib.crc32(t + p) & 0xffffffff)
            ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
            out = "/tmp/opencode/" + os.path.basename(f) + ".png"
            open(out, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
                                  + chunk(b"IDAT", zlib.compress(rows))
                                  + chunk(b"IEND", b""))
            print(f"  png -> {out}")


if __name__ == "__main__":
    main()
