# 0010 — boot.baf: the real LZ77 stream ends at file offset 0x9E002; the last 30,720 bytes are padding

Observed 2026-11-21 (session 4f), before any fix. Facts first (rule 6: no
behaviour verdicts without the reference capture; xemu+SpiritBoot is the
reference environment we have).

## What was observed

1. **Read sequence is fixed and identical in both worlds** (ours and the
   xemu STOP RAM dump, note 0009): 21 `NtReadFile` calls against
   `D:\boot.Baf` — header 2,048 B @0, then 19 × 32,768 B chunks, then a
   final 22,528 B @624,640. Payload delivered = **647,168 B = 0x9E000**,
   file position stops at **0x9E000**, never 0xA6000. The xemu object at
   PA 0x7b9ec30 carries the same quartet: budget init 0xA5800,
   `total-read slot = 0x9E000`, last-chunk slot `0x5800`.
2. **Budget accounting closes exactly** with the header read charged once
   into the game's byte-counter: 0x800 + 0x10000 (first chunk op) + … the
   refill (`sub_00022340`, 0x2236F/0x223D1) charges the *position delta*
   twice in one path and the constructor charges 0x800; observed slot
   history: budget slot initialised to 0xA5800, charged in 0x8000 units
   (prefetch 3×0x8000 visible as init 0x8D800), final op 0x5800 → 0 at
   the same instant reads stop. No kernel-side truncation, no lost read:
   `[READ]` shows every op, `st=0`, `got=want`, `sz=679936`,
   `path=game/boot.baf`.
3. **The offline decoder (tools/baf_decode_offline.py) matches the guest
   byte-for-byte to output offset 0x16Baf6 = 1,489,654 B.** That output
   offset corresponds to input offset ≈ **0x9E000** — the game's EOF.
4. Clamping the decoder to 0x9E000 (hard EOF) terminates the stream with
   output **0x16BAF7 (1,489,655) B** — one byte more than the guest-verified
   common prefix. The "END token at 0xA5E0D, output exactly 0x1BC800"
   result from note 0009 is therefore **a false positive: my parser
   ingested padding bytes 0x9E000..0xA6000 as if they were tokens** and
   the exact-size coincidence means the padding (likely zeros/other file
   data) happens to re-terminate cleanly under the loose 2-byte grammar.
   File bytes at 0x9E000: `2a 01 00 01 f0 …` — decodable garbage.
5. **After input EOF the decoder does not stop**: the guest wrote
   `Z:\boot.baf` monotonically to the cache cap (patch 0005,
   786,432,000 B) in 61,440 B writer chunks; the decoder's output cursor
   then overran its contiguous window (`+0x18 = 0x84000000` at fault;
   xemu: cursor 0x84FB8C94, STOP 0x7E at EIP 0x226D8 — same overrun,
   note 0009). Decoder disassembly confirms there is **no
   "empty-buffer" termination path**: the only exit is the END token
   (`je 0x226ED` at 0x2265D). With budget exhausted, refill
   (`sub_00022340` loc_00022379 → `je 0x223E1`) returns without feeding,
   and the byte iterator keeps reading past the segment end into adjacent
   heap — and keeps writing output.
6. `QINFO` (patch 0011) proves the kernel answered size queries correctly
   (`eof = 0xA6000` for `game/boot.baf`, class 34). The 0x9E000 read-stop
   is game-derived, not kernel-derived. Disc XDVFS directory entry for
   `boot.baf`: LBA 0x4BFD, **size field = 0xA6000** — the padding is
   inside the recorded file on the disc itself.

## Established

- The **data side** of the boot-archive wall is solved: real stream =
  file [0x800, 0x9E002), decoded output ≈ 1,489,655 B of the manifest;
  everything past 0x9E000 is padding, and the true stream's terminator
  region is at the EOF the game itself enforces.
- Our emulation of reads, sizes, budget, ring refill and decoded bytes
  is **bit-identical to the xemu reference state**. The remaining wall is
  post-EOF decoder termination, identical in both environments.

## Hypotheses for the non-termination (H13 family)

- **H13a (cheap, test first): the disk-full flag is the divergence.**
  Patch 0005's 786 MB cache cap triggers `STATUS_DISK_FULL (0xC000007F)`
  in the stream flush (`sub_00022270` error path sets bit0 of `+0x44`;
  afterwards every flush returns without sliding the output window →
  cursor overrun → our SEGV / xemu's 0x7E bugcheck). If the volume were
  large enough to hold the (padding-inflated) output the game produces,
  the decoder would keep consuming: with reads capped at 0x9E000 it would
  still overrun the heap-adjacent bytes — but the padding region *was*
  read into the ring (the last segment covers 0x98800..0x9E000 and the
  reader did fill 0x9E800-of-file slices); the decoder must be re-checked
  for whether it consumes the ring tail plus heap-garbage to a *natural*
  END before the window overruns when writes never fail. Test:
  `RECOMP_CACHE_CAP_BYTES=0x40000000` (or ≥128 MB) run and classify.
- **H13b: hardware's `boot.baf` differs.** If hardware's `GetFileSize`
  for this file returned 0x9E800 (SpiritBoot-style re-pack, tail
  stripped), budget would be 0x9E000, reads 0x9D800 payload — matching
  this run exactly — and the stream would end at EOF with the same
  runaway unless the END token sits at 0x9E001. Disc says 0xA6000 though;
  the budget math closes only with the 0xA5800 budget + double-charge
  quirk, so this is weaker than it looked.
- **H13c: the decoder does terminate naturally** after consuming the ring
  + contiguous-window tail, *if* flushes never fail; both failed worlds
  had disk-full (xemu's HDD image also fills). This is the same as H13a
  with opposite emphasis — a bigger volume is the knob either way.

## Post-script (same session, after the probes)

- **1 GiB cache cap falsified the cheap H13a**: `RECOMP_CACHE_CAP_BYTES
  =1073741824` → identical CRASH, rip = `sub_000223F0` (+0x2609de), the
  decoder output cursor overrun of the contiguous window happens whether
  or not writes can fail. The disk-full flag (`+0x44` bit0) is an
  amplifier, not the trigger.
- Ring geometry at crash fully reconstructed (core, `g_contig_memory`):
  segment A @0x80034000 = file 0x98800..0x9E000 (last, 0x5800 read) with
  residue from the previous generation (0x8E000..0x90800) beyond its
  valid end; segment B @0x8003C000 = file 0x90800..0x98800. No padding
  bytes (>= 0x9E000) ever entered the ring — the reader stopped exactly
  at the game's EOF and the budget closed (prefetch 3×0x8000 visible).
- **Divergence byte is exactly at the first padding position**: my
  unclamped parse consumed padding 0x9E000 as stream literals/matches;
  the guest consumed *ring residue / stale heap* past its valid end and
  diverged at output 0x16BAF6 (the byte my parse assigns to input
  0x9E000). The guest's first divergent bytes `9c 00 00 21 00 00
  55 55 57 5c` are NOT in boot.baf and are NOT a verbatim history copy;
  they exist inside the contiguous arena (output-window region
  0x80047370, and two other arenas) — i.e. they are already-garbage
  guest output re-consumed. Token-level ordering unknown.
- The decoder (both worlds) has no EOF termination path (only the END
  token exits); refill at budget==0 returns without data and the byte
  iterator reads whatever follows `+0x14` in the buffer. Completion
  therefore depends on hitting a dist==0 token in non-file bytes.
- **One accounting anomaly remains**: 20 budget charges of 0x8000 but
  19 chunk reads (0x8000 of budget charged without a `[READ]`); the
  flag-2 branch of the refill (0x2234D → 0x2236F, `budget -= [+0x3C]`)
  is the prime suspect. It is identical on xemu (same read totals), so
  it is not a lift artifact of ours.

## Next probe (ordered)

1. **gdb token trace of the guest decoder** around the divergence:
   conditional breakpoints at the match-copy loop (guest 0x226C0,
   host `sudeki+0x2609de+(0x226C0-0x223F0)`) and the literal loop
   (0x226A0), condition = object output-cursor field (host
   `[rbp-0xA4]+0x18`) in `[0x80004000+0x16A000, +0x16E000]`; dump
   esi (n), edi (dist), +0x10, +0x14 per hit; diff against
   `tools/baf_decode_offline.py --tokens` (mode added, 268k tokens for
   the clamped stream).
2. Re-run the budget/position slot watch picking the right stack parity
   from a 12 s probe run's crash esp (parities seen: 0x02691, 0x02692,
   0x02681 pages), 16-byte window at `object+0x30`, to see the exact
   refill op that charges 0x8000 without IO.
3. If the phantom charge is the flag-2 branch: that is game code
   semantics shared with xemu — accept read totals as game behaviour and
   focus on why the real stream has no END token at its logical EOF
   (maybe END is only reachable if reads cover the full 0xA6000 — the
   hardware-size hypothesis H13b with the SpiritBoot HDD image as the
   cheap xemu-side cross-check: dump the HDD image's boot.baf entry).
