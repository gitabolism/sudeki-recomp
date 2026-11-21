#!/usr/bin/env python3
"""tools/baf_decode_offline.py — offline re-implementation of the Sudeki
boot-archive decoder (guest sub_000223F0 + refill sub_00022340 semantics)
transcribed label-by-label from the lifted code, with file-backed
refill/flush no-ops (we preload the whole stream, so the chunking/refill
mechanics of 0x22340 collapse into buffer growth).

Grammar recovered from the lift (note 0009 / this file):
  * bit queue ebx: tag byte T -> ebx = T|0xFF00; HI8(ebx)==0xFF means the
    tag queue has bits; shifts consume LSB-first; when HI8 becomes 0 the
    next token reads a fresh tag byte.
  * tag bit == 0 -> literal (run: copy while tag still has bits).
  * tag bit == 1 -> match: opcode b; b < 0x60: dist = 0x100-b, n = 2;
    else: second byte c; dist = (b&0xF)<<8 | c; class = b>>4 (6..F);
    n = class==5 ? (third byte + 8) : class + 3.
  * dist == 0  -> END: flush(used + 0x1000) and return  [loc_000226ED].
  * output copy is self-referential (back-references into dst).

usage: baf_decode_offline.py [boot.baf] [--start 2048] [--out FILE]
"""
import sys

def u32(x): return x & 0xFFFFFFFF

def decode(data, start=2048, dst_limit=0x2000000, max_iters=20_000_000):
    # stream object state (guest field -> host var)
    src = start          # +0x10 src cursor (index into data)
    src_end = len(data)  # +0x14 src end
    dst = 0              # +0x18 dst cursor (index into out)
    dst_end = dst_limit  # +0x1C dst end
    out = bytearray()    # out grows with dst; dst index == len(out) in practice
    ebx = 0              # tag queue

    def rd8(pos):
        return data[pos] if pos < len(data) else 0

    iters = 0
    flushes = []
    pos_at = []          # (iters, src, dst) trace every 100k
    while True:
        iters += 1
        if iters > max_iters:
            return out, "MAX_ITERS", src, dst, flushes
        # --- margin checks (0x22400/0x22413): both below -> slow path.
        # Offline, slow path == same token processor (refill/flush no-ops
        # because the whole file is preloaded); we keep ONE processor.
        # --- 0x22424: ensure tag
        if (ebx >> 8) & 0xFF == 0:            # HI8(ebx)==0: fresh tag
            ebx = rd8(src) | 0xFF00
            src += 1
        else:
            ebx >>= 1
            if (ebx >> 8) & 0xFF == 0:
                ebx = rd8(src) | 0xFF00
                src += 1
        bit = ebx & 0xFF
        if bit == 0:
            # 0x22441/0x22450 literal run: copy while tag still has bits
            while True:
                out += bytes([rd8(src)]); src += 1; dst += 1
                ebx >>= 1
                if (ebx >> 8) & 0xFF == 0:   # tag exhausted -> leave run
                    break
            continue
        if (bit & 1) == 0:
            # 0x22479 single literal (tag has remaining bits, bit0==0)
            out += bytes([rd8(src)]); src += 1; dst += 1
            continue
        # 0x22493 match token — REAL semantics (disasm 0x22493-0x224e4):
        #   b >= 0x60: short match dist=0x100-b, n=2, never END.
        #   b <  0x60: dist=(b&0xF)<<8|c ; class=b>>4 (0..5); END if dist==0.
        b = rd8(src); src += 1
        if b >= 0x60:
            dist = 0x100 - b
            n = 2
        else:
            c = rd8(src); src += 1
            dist = ((b & 0xF) << 8) | c
            cls = b >> 4
            if dist == 0:
                # loc_000226ED: END — flush used + 0x1000 and return
                flushes.append((dst, "END+0x1000"))
                return out, "END", src, dst, flushes
            if cls == 5:
                n = rd8(src) + 8
                src += 1
            else:
                n = cls + 3
        # 0x224E4 copy n bytes from dst-dist (self-referential)
        base = dst - dist
        if n == 0 or dist > dst:
            continue
        for k in range(n):
            out += bytes([out[base + k]]); dst += 1
        if dst >= dst_limit:
            return out, "DST_LIMIT", src, dst, flushes
        if iters % 100_000 == 0:
            pos_at.append((iters, hex(src), hex(dst)))
    # unreachable

def main():
    args = sys.argv[1:]
    path = "game/boot.baf"
    start = 2048
    outp = None
    i = 0
    while i < len(args):
        if args[i] == "--start": start = int(args[i+1], 0); i += 2
        elif args[i] == "--out": outp = args[i+1]; i += 2
        else: path = args[i]; i += 1
    data = open(path, "rb").read()
    hdr = int.from_bytes(data[0:4], "little")
    print(f"{path}: size={len(data)} header u32={hdr} (0x{hdr:08x}) start={start}")
    # multi-segment: after END, next segment at the next 2048-aligned offset
    off = start
    total = bytearray()
    seg = 0
    while off < len(data) - 8:
        out, why, src, dst, _ = decode(data, off)
        seg += 1
        print(f"seg{seg}: start=0x{off:06x} out={len(out)} stop={why} "
              f"consumed_to=0x{src:06x} head={out[:12].hex()}")
        total += out
        if why != "END":
            break
        if len(out) == 0:
            print("zero-length segment -> desync, stop")
            break
        if len(total) >= hdr:
            break
        off = src                       # contiguous segments (no alignment)
    print(f"TOTAL out={len(total)} vs header {hdr}; segments={seg}")
    if outp:
        open(outp, "wb").write(total)
        print(f"wrote {outp}")

if __name__ == "__main__":
    main()
