# Note 0008 — where do the boot.baf reads actually land?

Date: 2026-11-21 (session 4). Follows note 0007. Tests H12 (aliasing) and
supports H10 (input delivery) for the decoder wall.

## Question

The 22 `NtReadFile` calls that stream `boot.baf` (679,936 B) deliver
byte-exact data per the `[READ]` sample — but the sample is read back
from `XBOX_TO_NATIVE(buffer_va)`, so it only proves the host buffer
bytes, not that the **decoder's source region** is the same bytes. If
the game reads through one alias (heap / RAM mirror) and decodes
through another (the contiguous window, which the toolkit deliberately
backs with *separate* storage), the decoder runs on garbage forever —
exactly the observed behaviour (unbounded 61,440-byte output chunks).

## Change (instrumentation only)

`patches/xboxrecomp/0008-read-buffer-va.patch`: the `[READ]` line also
prints `buf=0x%08X` (the guest buffer VA). Then:

1. run with `RECOMP_CONTIG_ALIAS=1`, short watchdog (~15 s) so the core
   is captured while the flow is still early in the copy phase;
2. from the run, read the 22 `buf=` targets and classify the VA region
   (heap / RAM / contiguous window / mirror);
3. in the core, dump those guest VAs and diff against the corresponding
   `game/boot.baf` chunks, and check whether the decoder's source
   fields (frame `+0x3C/+0x40`, seen at `0x8003c000`-family addresses
   in earlier cores) reference the same or a different mapping.

## Outcome (run `logs/run-0008-probe.log`, 18 s, SIGABRT before exit)

1. **The 21 `boot.baf` reads land inside the contiguous window, alternating
   double buffers:** `buf=0x80034000` / `buf=0x8003C000`, stride 32,768,
   last one 22,528 B at @624640 — the same 192 KB arena (`[CONTIG]` alloc
   #5) whose pointers the decoder's object fields reference. So
   **H12 (read-through-one-alias, decode-through-another) is dead**:
   read target and decoder source are the same mapping, and the `[READ]`
   sample proves the bytes are the disc bytes.
2. **The output is valid decoded data.** `Z:\boot.baf` starts
   `4a 8d 03 00 0f 00 00 00 15 bc 05 00 26 00 00 00 …` — the disc
   compressed stream at @2048 is `40 4a 8d 03 00 0f 00 ff 15 10 bc 05
   00 26 00 08 …`: token bytes expanded into 8-byte table entries. The
   decoder works; its output is not garbage (at least initially), and
   chunks at write indices 0,1,2,5,50,500 are all distinct, so it is
   not blindly re-emitting one buffer.
3. `sub_00022340` (refill) was read: it computes `remaining = +0x40 -
   +0x3C`, reads `min(remaining, +0x34)` bytes through the XAPI helper
   `sub_00270FBD`, updates file position `+0x28`, and — **every return
   path yields `eax = 0`**, including the `+0x40 == 0` path that skips
   the read (`loc_000223E1`). The caller cannot tell "refilled" from
   "exhausted" by return value; termination must come from the decoder
   hitting its own source-end check (`+0x10 == +0x14` with the new
   values) or a terminator token in the stream.

So the remaining question (H10 refined): after the last 22,528-byte
chunk is consumed, what does the refill set `+0x10/+0x14` to, and why
does the bit iterator keep running? The object lives on the worker
thread's stack, but the run log prints each worker's **stack top**
(`spawned worker … stack top 0x02682FF0`), and in the crash core the
object sat at `top-0x158` (cursor `+0x18` at `top-0x140`). With one
env-identical run to learn the top, a second run can pre-arm
`RECOMP_WATCH` on the right slot — the earlier stack-base jitter was
across *different* env sets; verify stability across identical ones.

## Next

- Two-run watch: learn worker #2's stack top from run A's log, arm
  `RECOMP_WATCH=<top-0x148 or +0x140>` in run B; watch source cursor
  `+0x10` (not destination `+0x18` — the latter updates per output byte
  and would trap ~10^6/s).
- Identify the `.baf` format offline from the disc bytes and the
  observed expansion (triplet → 8-byte entry, `0x29` runs); if a python
  decoder terminates cleanly at 1,820,672 bytes, compare token streams
  to find which token our state diverges on.
