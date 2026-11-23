#!/usr/bin/env python3
"""tools/xemu_fbpeek.py — reconstruct the NV2A framebuffer from an xemu
`dump-guest-memory` ELF (rule 5: file dumps, never screenshots).

Recipe home: notes 0009 (VRAM mirror = PT_LOAD @ pa 0xF0000000; old-ROM
FB at VRAM off 0x3C00000, pitch 720 px, BGRA) and 0019 (v1.1.0 kernel:
RE-MEASURE — this script's auto-scan is that re-measurement).

usage:
  tools/xemu_fbpeek.py RAM.elf OUT_PREFIX [--w W] [--h H] [--off OFF]
Auto-scan (no --w/--h/--off): stride candidate FB windows by 4096,
score BGRA plausibility (alpha==0xFF fraction + row-pitch self-
correlation), write the best hit as BMP.
"""
import struct, sys

W_CAND = (640, 720, 768, 800, 1024)
H_CAND = (480, 576)

def loads(path):
    d = open(path, "rb").read()
    e_phoff = struct.unpack_from("<Q", d, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", d, 0x36)[0]
    e_phnum = struct.unpack_from("<H", d, 0x38)[0]
    segs = []
    for i in range(e_phnum):
        o = e_phoff + i * e_phentsize
        if struct.unpack_from("<I", d, o)[0] == 1:  # PT_LOAD
            p_offset, p_paddr = struct.unpack_from("<QQ", d, o + 0x8), None
            p_off = struct.unpack_from("<Q", d, o + 8)[0]
            p_vaddr = struct.unpack_from("<Q", d, o + 0x10)[0]
            p_filesz = struct.unpack_from("<Q", d, o + 0x20)[0]
            segs.append((p_vaddr, p_filesz, p_off))  # QEMU sets paddr==vaddr
    return d, segs

def score(buf, off, w, h):
    """fraction of sampled pixels with alpha 0xFF; -1 if window absent"""
    need = off + w * h * 4
    if need > len(buf):
        return -1.0
    n = 0; good = 0
    for y in range(0, h, max(1, h // 48)):
        base = off + y * w * 4
        for x in range(0, w, max(1, w // 64)):
            n += 1
            if buf[base + x * 4 + 3] == 0xFF:
                good += 1
    return good / n if n else -1.0

def bmp(path, buf, off, w, h):
    rows = []
    pad = (4 - (w * 3) % 4) % 4
    for y in range(h - 1, -1, -1):
        row = buf[off + y * w * 4: off + (y + 1) * w * 4]
        rows.append(b"".join(row[x:x + 3] for x in range(0, len(row), 4)) + b"\x00" * pad)
    data = b"".join(rows)
    ds = 54 + len(data)
    open(path, "wb").write(
        b"BM" + struct.pack("<IHHI", ds, 0, 0, 54) +
        struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0) + data)

def _split16(x):
    x &= 0xFFFF
    x = (x | (x << 8)) & 0x00FF00FF
    x = (x | (x << 4)) & 0x0F0F0F0F
    x = (x | (x << 2)) & 0x33333333
    x = (x | (x << 1)) & 0x55555555
    return x

def bmp_z(path, buf, off, w, h):
    """NV2A CROLL Z-order (Morton) decode: pixel (x,y) lives at
    interleave(x,y)*4 — the layout the Xbox display engine uses when the
    front buffer is swizzled (linear attempts read as horizontal garbage)."""
    lo = [_split16(v) for v in range(0x10000)]
    rows = []
    pad = (4 - (w * 3) % 4) % 4
    for y in range(h - 1, -1, -1):
        out = bytearray()
        for x in range(w):
            m = lo[x] | (lo[y] << 1)
            p = buf[off + m * 4: off + m * 4 + 3]
            out += p
        rows.append(bytes(out) + b"\x00" * pad)
    data = b"".join(rows)
    ds = 54 + len(data)
    open(path, "wb").write(
        b"BM" + struct.pack("<IHHI", ds, 0, 0, 54) +
        struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0) + data)

def uniformity(buf, off, w, h):
    """fraction of sampled pixels equal to the modal color (menus/titles
    have large flat fields; video/torn frames do not)"""
    from collections import Counter
    c = Counter()
    for y in range(0, h, max(1, h // 64)):
        base = off + y * w * 4
        for x in range(0, w, max(1, w // 64)):
            c[buf[base + x * 4: base + x * 4 + 4]] += 1
    n = sum(c.values())
    return max(c.values()) / n if n else 0.0

def diffmap(d1, d2, vram1, vram2, size):
    """Byte-difference map between two VRAM buffers (same guest, later t).
    The front buffer animates on a title screen; most textures don't."""
    a, b = vram1[:size], vram2[:size]
    return bytes(x ^ y for x, y in zip(a, b))

def diff_score(dm, off, w, h):
    # fraction of CHANGED 4-byte pixels in a candidate window
    n = 0; ch = 0
    for y in range(h):
        base = off + y * w * 4
        for x in range(0, w * 4, 4):
            n += 1
            if dm[base + x:base + x + 4] != b"\x00\x00\x00\x00":
                ch += 1
    return ch / n if n else 0.0

def main():
    args = sys.argv[1:]
    elf, outp = args[0], args[1]
    opts = {}
    it = iter(args[2:])
    for a in it:
        if a == "--diff":
            opts[a] = next(it)
        elif a.startswith("--") and a not in ("--top", "--zorder"):
            opts[a] = int(next(it), 0)
        else:
            opts[a] = True
    d, segs = loads(elf)
    vram = max((s for s in segs if s[0] >= 0xF0000000), key=lambda s: s[1])
    print("VRAM seg pa=0x%X size=0x%X" % (vram[0], vram[1]))
    buf = d[vram[2]:vram[2] + vram[1]]
    if "--diff" in opts:
        d2, segs2 = loads(opts["--diff"])
        v2 = max((s for s in segs2 if s[0] >= 0xF0000000), key=lambda s: s[1])
        buf2 = d2[v2[2]:v2[2] + min(v2[1], vram[1])]
        dm = diffmap(None, None, buf, buf2, len(buf2))
        step = 0x10000 if len(buf) > 0x2000000 else 0x1000
        cands = []
        for w, h in ((640, 480), (720, 480), (640, 576), (720, 576)):
            for off in range(0, len(dm) - w * h * 4, step):
                ch = 0
                for y in range(0, h, 4):
                    row = dm[off + y * w * 4: off + (y + 4) * w * 4]
                    ch += sum(1 for i in range(0, len(row), 16)
                              if row[i:i + 4] != b"\x00\x00\x00\x00")
                frac = ch / ((h // 4) * (w // 4))
                if 0.02 < frac <= 0.75:
                    cands.append((frac, off, w, h))
        cands.sort(reverse=True)
        for frac, off, w, h in cands[:10]:
            print("diff-cand changed=%.3f off=0x%X %dx%d" % (frac, off, w, h))
        for i, (frac, off, w, h) in enumerate(cands[:3]):
            bmp("%s-d%d.bmp" % (outp, i), buf, off, w, h)
        return
    if "--off" in opts and "--w" in opts and "--h" in opts:
        if "--zorder" in opts:
            bmp_z(outp + ".bmp", buf, opts["--off"], opts["--w"], opts["--h"])
            print("wrote %s.bmp off=0x%X %dx%d Z-ORDER" % (outp, opts["--off"], opts["--w"], opts["--h"]))
        else:
            bmp(outp + ".bmp", buf, opts["--off"], opts["--w"], opts["--h"])
            print("wrote %s.bmp off=0x%X %dx%d" % (outp, opts["--off"], opts["--w"], opts["--h"]))
        return
    if "--top" in opts:
        # uniformity-first scan (menus/titles beat video/torn frames);
        # stride candidates only, coarse offset steps to stay cheap
        cands = []
        for w in W_CAND:
            h = 480 if w in (1024,) else 576
            for off in range(0, len(buf) - w * h * 4, 0x10000):
                u = uniformity(buf, off, w, h)
                a = score(buf, off, w, h)
                if a > 0.9:
                    cands.append((u, a, off, w, h))
        cands.sort(reverse=True)
        for u, a, off, w, h in cands[:8]:
            print("cand u=%.3f a=%.3f off=0x%X %dx%d" % (u, a, off, w, h))
        for i, (u, a, off, w, h) in enumerate(cands[:3]):
            bmp("%s-%d.bmp" % (outp, i), buf, off, w, h)
            print("wrote %s-%d.bmp off=0x%X %dx%d" % (outp, i, off, w, h))
        return
    best = (-1.0, 0, 0, 0)
    for w in W_CAND:
        for h in H_CAND:
            for off in range(0, len(buf) - w * h * 4, 0x1000):
                s = score(buf, off, w, h)
                if s > best[0] + 0.02:
                    best = (s, off, w, h)
                    if s > 0.97:
                        break
            if best[0] > 0.99:
                break
        if best[0] > 0.99:
            break
    s, off, w, h = best
    print("best: score=%.3f off=0x%X %dx%d" % (s, off, w, h))
    if s > 0.85:
        bmp(outp + ".bmp", buf, off, w, h)
        print("wrote %s.bmp (off=0x%X w=%d h=%d — re-measure law in note 0009)" % (outp, off, w, h))
    else:
        print("no confident FB; widen candidates or pass --off/--w/--h")

if __name__ == "__main__":
    main()
