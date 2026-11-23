# Note 0025 — level-load never completes: streaming throttle `cur/done` wedged, one unit re-read (H31)

Date: 2026-11-22. State: supersedes H30 (note 0024 CLOSED as cause of
the current wall); regime = frz92+ (v4 marks + `RECOMP_TITLE_KEVENTS`,
all three event mirrors live, ring writer healthy, engine ~10 fps).

## Observed (runs frz93/frz94; logs `logs/run-frz93-192142.out`,
`logs/run-frz94-193422.out`; cores `sudeki.frz93.core`,
`sudeki.frz94.core`; watcher v2 acc-aware, so these are TRUE plateaus)

* Navigation works: 8 STARTs (pad-enumeration-gated) -> New Game ->
  level load (speech banks `speech_hask*/shadow/TSA.xwb` stream,
  58-82 MB each, wavebank header tag `57 42 4E 44`).
* GATE census (`sub_002C3200` = streaming/frame throttle, note 0023
  anatomy): at plateau `tot` (+0x270) climbs +30/s and `acc` (+0x2B4)
  +28/s (gap ~130), but `cur` (+0x280) is FROZEN at 0x000E5D5C
  (941,404 — a byte offset), `done` (+0x284)=0 and `tick` (+0x27C)=0
  for the entire plateau; `bud` (+0x338)=`cons` (+0x33C)=0. Budget
  math `(tot-done)*1000*sc18/sc14` vs `bud/cons` keeps returning the
  SKIP decision -> render segment never runs -> presented frame stays
  the 99.6 %-black loading card for 600-900 s (frz94: 9,964
  presented window frames, all card).
* Everything else is ALIVE at kill (frz94 census timeline): ring
  writer (`[PRES]` to line 147,288), mark latches (`[SWM]` 147,532),
  completions `KeSetEvent 4C from 0x2DB0BE` (147,454), DPCs (150,832),
  engine gate calls (155.8 M), IO (`[READ]` 150,674). Watch on the
  4C object: quiescent (20 value transitions all run) - mirror
  coherence is NOT the issue.
* The IO tail is suspicious: repeated identical `want=116 got=116`
  reads returning ZERO bytes, same buffer 0x827AFC60, caller
  0x00271057 (seq-read site), thousands of times
  (`@seq` form lacked path/offset - the datum the wall needs).

## Hypothesis (one variable)

The loader's *current streaming unit* (byte window 941,404 in some
archive) never completes because its reads land on zero-filled slack
(or a mis-sized view) at our FATX/XDVFS layer, so the pump retries it
forever; `done` never advances and the throttle skips rendering
forever. Falsifier: if the seq-read path/offset (patch 0047) shows
the stuck reads hitting a *real data* window, the zero content is a
guest-side decode rejection, not our geometry -> pivot to the
consumer (which pump consumes the unit: `sub_002C0C50/C0980/C31A0`).

## Planned change (host-side only)

* **Patch 0047** (instrumentation, `patches/xboxrecomp/`): `[READ]`
  seq form gains `sz=` + `path=` (rpath/rsz already computed
  upstream). frz96 = same regime, 2500 s: names the stuck file and
  (with the `@` form) its offset; compare against the Redump ISO
  entry sizes (XDVFS table, gitignored) to see whether we are
  serving slack-as-data.
* Control frz95 (no presses, mirror regime): confirms the mirrors
  still reach title/attract normally (guards against a nav-free
  regression).

## Outcome

* frz101 core (killed 20 s after the last read): the HMF manager
  class singleton (global `0x397668` -> `0xFD9550`) is NOT one of the
  three pumped objects: its queue still holds an UNSERVICED node —
  `+04 begin=0xFB97A0`, cursor `+08=0xFB97C4` (one taken),
  `+0C end=0xFBA9E8` (~177 nodes of stride 0x24 queued). Node0:
  `81000000 00010000 00010000 00000001 <ptr 010B1EA0> ...` —
  identical shape in the frz100 core (end=0xFBB354): a standing
  backlog whose servicing stopped. NOTE: the `<ptr>` and node fields
  decoded so far landed on `PC_TOTALstartup` debug blocks (tags
  0x336734/0x3367AC recur 164x heap-wide = allocator/vector tags),
  so node-field semantics stay UNRESOLVED — but queue-with-backlog
  vs drained pumped managers is solid, and the decisive probe is
  `[MTICK]`: which object class does `sub_001EDA20` tick, and which
  gate closes.

### frz103: the load pipeline is SERIAL over ~40 per-archive managers; the LAST one never finishes

* Full `[PUMP]` census (167 entries): managers are per-archive
  objects `0x018A0C00..0x018CF200`; the load runs as a SERIAL
  pipeline — each manager's stage-field walks `1->2->4->8->0xF->
  ...->sentinel` (e.g. `018A0C00 st=484D4601` = the boot HMF,
  `018A2400/018B7800` = shared/startup BAFs, wavebank/gex-era
  `018AAxxx..018CFxxx`), and the NEXT manager starts pumping only
  when the previous reaches its sentinel. Pump counts per manager
  are tiny (3-5 samples) — the pipeline is a relay, not a pool.
* The LAST pump EVER (x167, line 37,217) is manager `018CF100`
  with `id=5678 st=00970002 node=00010040`, pumping repeatedly
  (x159..x167, ~3 log lines apart) — it never reaches its sentinel
  and NO later manager ever starts. Its neighbours `018CF000`
  (id=001F) and `018CF200` (id=FFFF, node=010E3050) finish;
  `018CF100` is the head of the queue that never drains.
* Era file windows: L36,100-37,219 reads are `startup.baf`
  (a SECOND startup.baf phase — first phase at L~1,200-19,223);
  post-stall reads are ONLY the attract movie ring (`Sudeki
  Atttract Mode.bik`, 1,816 reads) — no game-data stream resumes.
* => H31 final shape: name the archive owned by manager
  `018CF100` and what its `id=5678` stage waits on. Next probes:
  `[EF1D0]` era-loop entry counter (pipeline alive?); core dump of
  `018CF100` object + its node list (frz103 core, matched bin).

### frz105: H31.4 REFUTED — E0210/POOL ran ONCE at boot, both succeeded

* `[E0210] x1 ok flag=1 ret=1`; `[POOL] x0 -> ret=0`; `[PALLOC] x0 -> 0`
  — the engine-frame early-exit branch (`0x001E0387`) never taken in
  the whole run; `0x397110` stays 1. E0210 is a boot/session-init
  driver, not a per-frame tick. The 60+ `sub_001EE7D0` call sites are
  NOT gated off by that flag.
* Census masks were too coarse to prove chain silence (`[EF1D0] x1`
  only bounded entries <4096). frz105 reproduced the stall exactly
  again (314 baf reads, movie loop) — deterministic, no regression
  from the new probes.
* frz103-core pre-check: job-table entry `0x37A3D0[0x5678] = 0` at
  kill, while manager `018CF100`'s current-job id is still `0x5678`
  (dangling). The job was freed at/after the last pump; nobody looked
  it up again afterwards (guest would have null-dereffed
  `sub_001ECC40`'s table path — it did not crash).
* **H31.5 (working model): the relay's current manager COMPLETES its
  last job, gets freed-ish, and the DRIVER loop stops advancing to
  the next archive manager — the next manager in the serial relay is
  never pumped.** `sub_001ECC40` has no null guard (guest would crash
  on a dangling id lookup), so post-stall tick must be skipping the
  lookup entirely — i.e. the chain above tick stopped passing this
  obj.
* frz106 census (tight masks: `[MTICK]` /64, `[EF1D0]` /256, plus
  `[JOB]` id->job+state and `[HANDLE]` h->node, printed on arg/result
  transitions): decides which link stops carrying the relay — tick
  entries vs job-lookup verdicts vs pump entries, co-registered with
  the last baf `[READ]`. Also `[EF0C0]` mode/args and `[REG]`/`[DEREG]`
  relay membership.

### frz114-115: seq-read positions revealed; installed-tail polling is the loop (H31.10)

* patch 0048 (`[READSEQ] fd pos before->after`) + 0049
  (`RECOMP_IO_WRITE_MAX`): frz114 = first run with fd positions.
* Boot era reads the DISC `game/startup.baf` (8,585,216 B) with
  positioned reads (install); the load era streams the INSTALLED
  cache copy (27,762,688 B) via offset-less seq reads — and
  positioned reads on the SAME host fd leave the fd seeked (bridge
  lseeks for positioned reads, never restores), so "seq" positions
  JUMP around (24608864 -> 24668256 -> 24674400 -> 27621472 ...):
  guest-visible file pointer semantics vs host fd = open bug class
  (NT: positioned IO must not move the handle FPO).
* The final-leg loop polls 5120-byte records at the installed-file
  TAIL (~24.6M-27.6M of 27.76M); those blocks contain real mixed
  data (427-1332 nonzero bytes per 5120) but the record headers
  start with zeros; trailer dword (buf+6140) = `3EF04C91`, bit4
  clear -> F9720 sync branch every time.
* No level `.baf` is EVER opened (read census: only startup.baf,
  movies, soundbanks, shared/SOLWORLDM) -> relay dies before level
  archives.
* frz115: full `[WRITE]` census (MAX=0) maps whether the guest
  WROTE the polled tail regions during install (holes = lost/mis-
  placed writes on our side vs genuine content on the chain).
* Installed file vs disc copy: 0/2096 identical 4K blocks (install
  expands+relocates; byte-compare against disc is not a valid check).

### frz113: the real manager, the trailer dword, and the zero buffer (H31.9)

* PUMP probe arg-slot bug fixed (bypass site `1EDB5E` pushes ONE arg;
  `ebp = entry esp+4` — the old probe read esp+8 = stale frame). The
  "mgr `018CF100` id=5678 st=00970002" line was the WRONG object
  (adjacent array slot). The true final-leg manager (frz113) =
  `018CF14C`: `+0x18` job id `0A260002` (slot 2 alive), `+0x1C` size
  `0x1400`, `+0x8` descriptor `00F95738`, `+0` tag `00336794`.
* All boot-era pumps x1..x166: sentinel `id=FFFF` (array poll,
  eax=0, eax-less branch), from the tick MAIN site (`1EDA75`).
  Only x167 = bypass site (`1EDB68`, eax=scratch-ptr `02611EA4` ->
  handle obj `010E3178` via `E1750`), job id `0x0002` LIVE.
* Inside x167 (NULL-job... actually the handle-object path): vtable
  thunk `047CE4` -> scratch alloc `1145D` -> 6144-byte read into
  `0x02E77650` (real data: tail filled, head zeros), then trailer
  dword at `buf+6140` = **`3EF04C91`** (LO8 `0x91`, bit4 CLEAR) ->
  pump took the F9720 SYNC branch (matches S9720 activity); then
  loc_0023BA71 issued a 5120-byte read from `startup.baf` into the
  UNCACHED buffer `0x81759A80` — and the core shows that buffer
  ALL ZEROS (head/tail/past-end). frz103 showed the same shape
  (buffer `0x8174FA80` all zero). The `[READ]` says got=5120 st=0.
* Open question = delivery vs file content: are the file bytes at
  that seq position really zero (padded archive), or does the read
  land wrong? Patch 0048 adds `[READSEQ] fd pos before->after` for
  every offset-less read; frz114 = first run with it.
* Event layer update: `sub_00240390` (sole F99D0 caller seen; boot-era
  submit with scratch tag `0x33A0B4`, `ebx=0x100` = the ONE S99D0
  line) is NOT the archive relay path — relay legs run through the
  pump's F9720 sync processor with trailer-command masks.
* JOBFREE (`sub_001ECB10`, sole clearer of a `0x37A3D0` slot) fired
  ZERO times across a full run: no job-slot teardown at all; the
  earlier "job 5678 freed" note traced to the wrong-slot mgr dump.

### frz109-110: the wait cycle runs, the events never fire, RESADD never fires (H31.7)

* frz109 (EF1D0 per-event, killed t+449): only **66 walks session-
  wide** vs EF0C0 ~476 — the walk is NOT cycling hundreds of times
  (earlier "≈255" was the EC590/EC6B0 registrar count, not walks).
  Walk a1s = load-table ctx VAs striding 0x3C; last boot-era walks
  find legs (`-> esi=010B1EC8` repeats), stall-era walk
  `a1=018AA4D4 a2=001F15FD -> esi=0` (no leg matches that ctx).
* frz110 (event census + `RECOMP_LOG_ORDS=123,138,145,205,225`,
  deterministic 5th replay: baf=314):
  - `[EACYC]` (load wait cycle `EAAB0`) kept looping (x64+) and its
    waits return **`0x102` = WAIT_TIMEOUT** — the loader thread is
    NOT parked; it polls, pumps the relay chain (last pump x167 ran
    ON THIS THREAD: stack `00021B1B -> EAC88 -> EE160 -> EDB68`),
    and times out forever.
  - `KeSetEvent` fired ONLY for the swap-era tokens (`002E2F4C`
    1546x, `00F7FC7C` 1x) — the load events `0x37A118/0x37A114`
    were NEVER Set/Reset/Pulsed in any run.
  - `RESADD` (`EF510`, sole caller = F99D0 processor at 1F9DAF)
    = **0 events**; `RESREL` kept firing (walk ref-releases drain
    legs post-stall — legs die of old age, not of completion).
* Pump processor dispatch decoded (`sub_0023B830` @23B8D2..23B9E7):
  sentinel `MEM16(ebp+0x18)==0xFFFF` -> skip; else `ECC40(job id
  mgr+0x18)`: job!=NULL -> `edx = job`; job==NULL -> vtable +
  **trailer dword `MEM32(buf_end-4)`** as edx. `test edx, 4`:
  bit set -> F99D0 (the async submitter/reservation-adder), clear
  -> F9720 (sync path). F99D0 refuses at `1F9B5C` UNLESS its
  bit-mask guards pass (`popcount(ebx)==1 && popcount(ecx)==1 &&
  ebx==ecx`, masks = `1<<n` windows shifted by `MEM16(mgr+0x2E)`)
  — refusal happens BEFORE the RESADD site, so RESADD=0 is
  consistent with F99D0 running and refusing every time.
* frz106 [JOB] facts: mgr `018CF100+0x28` id `00970002` -> job
  `02977A78` NON-NULL st=0 at kill (job alive; frz106 core).
  frz106 job objects (`02977A78/79AC/B50/B80`) all begin
  `00000000 FFFFFFFF 0x??10B21...` pattern.
* frz111 census: `[S99D0]` entry ebx/ecx + return eax per call
  (ra names which of 3 call sites), `[S9720]` entry. Verdict tree:
  S99D0 never called -> bit4 of edx (job-ptr low byte 0x78 HAS
  bit4 set! -> should call) never evaluated => pump path differs;
  S99D0 called but RESADD=0 -> guard refusal -> read ebx/ecx;
  S99D0 called + RESADD fires -> look past the processor.

### frz108 + static close-in: the load-completion EVENT layer (H31.6 candidate)

* frz108 (full-build census, killed t+828 by watcher): funnel totals
  EF0C0 ~476 / EC590 ~257 / EC6B0 ~257 / EF1D0 ~255 — the walk DOES
  cycle (mask hid it); pump x167 again (deterministic 4th replay);
  core at t+828: ALL queues drained (the walk ref-releases queued
  legs — frz107@t+465 had 30 bucketed, frz108@t+828 has 0).
* Caller tags decode: EF0C0 mode arg = upper-nibble tag
  (0x20000000 era-loop via EC130, 0x60000000 via EBF20) + low bits
  mode — earlier `&3` read was the wrong field; a1 values stride
  0x3C (load-context ARRAY); legs filter-match on `obj+0x10 == ctx`.
* **Event layer:** the load session wait is `sub_00021AD0` (vtable
  handler via 0x0026D456) -> `sub_001EABD0`: creates/opens two event
  handles -> globals `0x37A118` (kernel handle `0x48000006` at kill)
  and `0x37A114`; `sub_001EABD0`/`sub_001EAAB0`/`sub_001EE190(flag=1)`
  WAIT on them (`0x0026D77A`=Wait(h,ms), `0x0026D78C`=WaitMultiple);
  `sub_001EE190` path ends in `Wait(0x37A118, INFINITE)`.
  Producer gate `0x396E91` = 1 at kill (not a flag-out blocker).
  Reservation helpers `sub_001EF4C0/510` (byte counters `0x3975A8/AC`)
  had ZERO census events in all runs so far (wrappers added).
* frz110 plan: `[EACYC]` (wait-cycle counts: frozen => thread parked)
  + `[RESREL]/[RESADD]` (reservation traffic) + `RECOMP_LOG_ORDS=
  123,138,145,205,225` (KePulse/KeReset/KeSet/NtPulse/NtSet) to see
  whether the load-complete event ever fires host-side.

### frz106: post-stall the dispatcher keeps polling; producer-side is the open end

* Full-census run (`logs/run-frz106-220143.out`): pump x167 again, 314
  baf reads — deterministic third reproduction (frz103/104/106).
* Old `[PUMP] n=... from=00F7FA90 m=0` post-stall lines = the H30-era
  D3D SWAP-SITE probe (tag collision), NOT `sub_0023B830` — the pump
  silence after the stall stands.
* Post-stall `[EF0C0] x469 a1=018CF14C a1+8=00F95738 mode=0`: the
  dispatcher is STILL cycling with a static load-table ctx, but
  `EF1D0` (<256) / tick (<192) / pump (167) all frozen -> servicing
  finds nothing to register (list heads empty).
* Core (base `0x7fb659000000`): all 32 buckets + all 3 mode-lists
  EMPTY; byte-pending globals `0x3975A8/0x3975AC` = 0 (no reservation
  leak; EC590 gate not closed by backlog); leg node `014BE274`
  still holds `+0x10 = ctx 018AA200` + next `014BD11C` while
  unbucketed; `REG x200 obj=014BE274`, `DEREG x200 obj=018BD0AC`.
* New statics: `0x3975A8/AC` = engine-wide pending-byte counters
  (accounting helpers `sub_001EF4C0/510`); `EC590` = if-not-registered
  -> `new 0x1C` node -> `EC2C0` register; producer API `EABD0` has a
  single caller `sub_00021AD0` (a teardown/unload context).
* frz107 census adds `[ENQ]` (`EE190` list/queue), `[REQ]` (`EABD0`),
  and `[PUMP] -> ret` to name whether the PRODUCER stopped (engine
  waiting on last-leg completion) or the leg-completion signal died
  host-side.

### H31.5 static relay lifecycle (decoded 22:0x, before frz106)

* `sub_001EF0C0` mode-switch: arg1+8 set && 3rd arg set -> list
  `0x3D75FC`; else mode `arg2&3`: {0,1} -> `0x3D7668`, 2 ->
  `0x3D7620`, 3 -> **no servicing at all**.
* `sub_001EC590(context, list)` (called ONLY from EF0C0) REGISTERs the
  next leg: sole caller of `sub_001EC2C0` REGISTER (insert into bucket
  `0x37A120[((epoch>>6)&31)]`, obj fields `+0x10` epoch, `+0x18` next,
  tag `0x33677C`).
* `sub_001EF1D0` walks the bucket list, services objs, and releases
  refs (9 call sites of `sub_001EC3E0` = ref-release: decrement
  `obj+0xC`, at zero -> `sub_0023AC90` + `sub_001EC4E0` DEREG + free).
* frz103 core at kill: ALL 32 buckets + all three mode-lists EMPTY.
  Last pump x167 (`018CF100`, st `00970002`, not sentineled) followed
  by ZERO pumps => leg deregistered mid-work with no re-REG.
  **Prediction for frz106: a `[DEREG] obj=018CF100` line right after
  the last `[PUMP] x167`, with NO subsequent `[REG]`; `[JOB]` shows
  id 5678's state in flight; if DEREG happens while the job is not
  complete, the release path is the abort — why the ref dropped to 0
  is the open question the census will pair.**

### frz105: H31.4 REFUTED — E0210/POOL ran ONCE at boot, both succeeded

* `[EF1D0]` (mask 0xFFF) printed `x1` only: `sub_001EF1D0` entered
  <4,096 times for the whole run — the load-pipeline chain is NOT
  entering-and-rejecting, it stops being called. frz103 re-checked:
  identical census (pump x167 last, 314 baf reads) — deterministic.
* Static decode of the driver chain (`EE7D0 -> EE2A0 -> EC130 ->
  EF0C0 -> EF1D0 -> EE110 -> EDA20 tick -> 23B830 pump`):
  **`sub_001EE7D0` (60+ call sites across the engine) returns
  immediately when guest global byte `0x397110 == 0`.**
* `sub_001E0210` (engine frame driver, single call site 0x00012D20)
  zeroes `0x397110` on ENTRY and restores it at its normal exit
  (0x001E0378, flag=ebx). Its **EARLY exit at 0x001E0387 returns 0
  WITHOUT restoring** — flag stays 0 -> every EE7D0-driven subsystem
  (load pipeline, manager tick, pump) becomes a no-op, while Bink
  movie + present keep running (separate path). Exactly the observed
  census shape.
* The branch is taken when `sub_002D2340` returns != 0. 2D2340 sets
  pool params (`0x2E47E8`=cap 0x300000, `0x2E47E4`=blk 0x8000) and
  calls `sub_002D85D0` (sole caller; pool alloc/grow). Negative
  signed return -> cleanup + error return. So: **H31.4 — the pool
  allocation behind `0x2E1190` fails, the engine frame early-exits,
  leaves 0x397110=0, and the whole load pipeline is gated off
  permanently (deterministic byte-position stall follows because
  the same request fails at the same byte of the stream every run).**
* frz105 probe: `[E0210]` flag before/after + ret; `[POOL]` args/ret
  + pool globals; `[PALLOC]` signed status from 2D85D0. Predicted:
  E0210 keeps entering every frame post-stall with flag 0->0, POOL
  ret negative every frame; the status code names the allocator
  failure (candidate: pool exhaustion — backlog nodes hold the
  blocks, and the jobs that would free them are gated by the flag:
  self-lock).

### frz100/101: the stall is DETERMINISTIC and the pump TICK stops

* frz100 reproduced the read-back stop at the EXACT byte: seq head
  `0x00AD5D09` = 11,363,693 B (40.9 %), identical to frz97; frz98/99
  land ~0.5 % off (different log eras). The container itself: the
  X: install is a TRANSFORM — source `game/startup.baf` is 8,585,216 B
  (`HMF\x01` + 8-B offset/flag entries; trailer `baf\x04`), installed
  image 27,762,688 B, 8,449,669 bytes differ. The stall offset sits
  past all 66 header-entry offsets, in a `00 00 FF 00`-pattern region
  whose neighbours are small-int record tables (`00 00 FF 02 | 00 00
  FF 05 | ...`) — structured content, not slack.
* frz101 `[PUMP]` census: 119 pump entries total, 3 manager objects;
  every entry from BOTH call sites (001EDA75 main, 001EDB68 worker).
  The read-back manager `018A2400` shows stage-field 7 -> 0xF ->
  0xFFFF (0xFFFF = the pump's own "final inline" branch). Its LAST
  pump ticks (x117 x118 x119, lines 16080/17602/19282) are the last
  era of `startup.baf` reads; after line 19284 NOTHING pumps again
  for the rest of the run. => the pump is not idling — the manager
  TICK (`sub_001EDA20`) stops being entered (or its entry gates
  close). Probe added: `[MTICK]` wrapper on `sub_001EDA20` logging
  which gate fails (`+0x28 & 0x200` / `+0x04` null / head byte 0).
* frz100 core: BAF manager 0x00FD9550 (global 0x397668) with ONE
  active node (stride 0x24), node flags `81000000/00010000` pending;
  a `PC_TOTALstartup`-named request object (tags 0x336734/0x3367AC —
  allocator/request tags, present 164x heap-wide). Core-object field
  mapping stays untrusted until [MTICK] lands.

### frz98-100 consumer anatomy: the job-pump chain is named end-to-end

* `sub_00270FBD` (helper) call sites: `sub_0001E170` (stream read
  method: lock cs-table `0x3156C0/C4` -> seek `sub_00271204` -> read
  helper -> cursor += n) reached via thunk `sub_0001E0A0` from
  vtable `0x00320BE4` slot +0x18; second family `sub_00020900`
  (`.gex` table walker).
* Phase ordering settled from `[FILE]`/`[WRDUMP]`/`[READ]` interleaving
  (frz97, fresh cache): install = D:\*.Baf open + Z:\*.baf create
  (disp 3) + 61,440-B writes, COMPLETES (27,762,688 B), then
  read-back opens (disp 1 opts 0x60) and the pump starts. Read-back
  is the stream that dies at ~11.33-11.36 MB (40.8 %) in frz97/99.
* frz99 flip timeline (runtime `fps (` lines): flips 225->652->1159
  ->1752 (menu/attract era), 1823->1824 one flip across the whole
  plateau, +25 at t=525 (2.02 fps) just before ABRT. All `[READ]`
  traffic stops at line 37,799 (3 s before kill) — the attract movie
  keeps decoding from its in-memory ring (mdec 3495 vs mpr 3196).
* `[RD]` stack fingerprints on the LAST startup.baf reads are the
  pump chain: `0001E1BC | 0023BA71 | 00272470 | 0001145D | 0001152A
  | 0047CE4` — i.e. `sub_0023B830` (job-pump: lock cs, reentrancy
  slot `this+0xC0E4`, walks node list, per-node vtbl[2] open /
  vtbl[1] read (site `0023BA71`) / vtbl[0] release), entered through
  `sub_001F9720` (pool alloc `sub_001FEAE0`, popcount==1 size gate,
  gate `sub_001F96C0` = type range-check `eax-16 <= 0x31` against
  2-entry table at `0x1F96DC`).
* Next measurement (frz101): `[PUMP]` census wrapper on
  `sub_0023B830` (thisptr, node-list head `+0x10`, tail `+0x0C`,
  queue slot, caller fingerprint) -> distinguishes pump-stops-running
  vs runs-with-empty-list vs nodes-never-read.

### frz97 write-forensics: serving layer exonerated, wall is guest-side

* frz96 (watcher-fired ABRT t=868 s): kill-state — engine alive
  (T1 inside submit chain `sub_002D5C80<-...<-sub_00207400`), nv2a
  PB exec active, XAPI workers idle-waiting (T14/T17 chains under
  `sub_00021B90/sub_00021AD0`), movie mid first lap (mdec 2276).
* frz97 (RECOMP_IO_WRITE_LOG + WRITE_DUMP): `[WRDUMP]` stream replay
  (`wreplay.py`) vs live Cache files: **startup.baf 27.7 MB 0
  mismatched bytes** (within dump-cap), boot.baf 0, Partition5/P0/
  badger/SOLXM 0. The 4 MB of zero runs inside Cache/startup.baf
  were written as zeros BY THE GAME (its own pre-allocated slack).
  Our write path is faithful; read path already proven exact
  (443/443 disc sizes + got=want).
* => H31 is purely guest consumer logic: the BAF config reader
  (through `sub_00270FBD`) stops asking at ~11.36 MB. Next
  measurement built: `RECOMP_RD_TRACE=1` manual wrapper on
  `sub_00270FBD` logging thisptr+args+EBP caller chain per read
  (frz98) -> name the consumer and the branch it stops taking.

### frz96 partial (patch 0047 live, ~9 min in, `logs/run-frz96-201101.out`) — the REAL load census

Phase IO census (path= per read, `io_phase_census.py`):
* PRE-LOAD: MS/ClimaxLogo biks, Cache/boot.baf, startup.baf,
  SOLWORLDM.gex — normal boot.
* LOAD-era: `Movies/FMA01_poem.bik` read **exactly 1.00x of its
  111,181,980 B** (whole opening FMV, once); every sfx/speech wave
  bank read 1.00x (complete); then IO collapses to attract-movie
  looping + intermittent `Cache//startup.baf`.
* `Cache//startup.baf` (27,762,688 B — the game's own X: install
  target, page-multiple 27112 KiB, created by the game at boot
  20:11:42, disc `startup.baf` is the 8,585,216 B source) is read
  **0.41x total (11.3 MB) with burst+33-KiB-strided-seek access,
  and goes fully silent mid-run** — that IS the load-progress
  stream, and it stalls at 41 %.
* Cache BAFs (boot/startup/shared) are all page-multiples and much
  larger than disc versions = game-written sparse containers (zero
  runs = its own slack, not our extraction: disc tree 443/443
  size-exact).
* Wavebanks unpacked to X: at 20:13, then quiet.
* No short reads on startup.baf (all got=want) — reads that happen
  all succeed; it is the CONSUMER side that stops requesting.

### CORRECTION (BINK anatomy, frz94 core): the GATE census never measured level loading

frz94 core + section map: ctx 0x82648040 is a **BINK struct** (Bink
player sections live at 0x002C0880-0x002CCFFB — so `sub_002C3200/
sub_002C0C50/sub_002C0980/sub_002CA6E0/sub_002C31A0` are all Bink
online-mode internals). Decoded fields (from core + bodies):
+0x270=frames decoded (grew +30/s = real-time decode; 2296 at kill =
exactly the movie's NumFrames; frz93's 3499 = second loop),
+0x2B4=frames presented (+28/s, lags ~130 = the online buffer),
+0x280/274=tick timestamps, +0x278/27C/28C=last tick (global
0x3969C8 matches), +0x028=file read pos (EOF-8 at kill),
+0x160/164/188=file size (EOF=0x1C4CD94), +0x14/18=fps 2997/100,
+0x020=496 KiB ring (slots matched 16 KiB at stride 0x4000).
IO-ring content-match (core): all 16 slots =
`Movies/Sudeki Atttract Mode.bik` @ 0x1BCE158-0x1C46158 (file size
29,674,900 = matches XDVFS table exactly; extraction tree verified
443/443 entries byte-size-exact vs disc).
=> "load stuck at 90 % with stream position frozen at 0xE5D5C" was
MOVIE pacing mislabeled: the attract movie (640x480, 76.6 s, 2296
frames) streams to EOF, finishes, and LOOPS under the loading card.
No load-progress census exists; H31 must be re-posed as: what event
does the game await after entering the load state? (frz96 path= per
phase is the decisive dataset: does non-movie IO persist?)

### frz95 (no-press control, 900 s, `logs/run-frz95-195504.out`) — control PASSED, wall sharpened

* Mirror regime reaches title/attract normally (no regression): rich
  attract frames to dump #85 (~30-37k colors, luma ~85), engine
  ~10 fps (`pass≈rend≈9.2k`), 16,077 reads, no crash lines.
* **The stall signature appears WITHOUT navigation**: at kill, the
  same GATE ctx shows `cur` frozen at 0x000D0194 (852,372),
  `done=0`, `tot`/`acc` still climbing; last 3 window dumps pure
  black (24 colors, luma 0) — same terminal shape as the nav runs.
  So H31 is era-general (title-era tail at ~t+600 s), not
  level-load-specific.
* `cur` semantics corrected: in the healthy era it WALKS a stream —
  0x000152FD -> 0x0006D7E2 -> 0x0009C5BD -> 0x000D0194 (monotone,
  with momentary 0 resets between samples) = a stream byte position.
  Stall = position stops advancing. `done=0` occurs in healthy eras
  too — census `done` label is NOT load progress (re-derive later).
* Watch on 4C: 7,694 writes, ZERO value transitions in the whole
  healthy title era, rich frames flowed anyway -> the 4C swap-slot
  event is not what feeds healthy rendering; the 20-pulse census in
  frz94 was incidental. (frz93's "parked on 4C" = terminal detail.)
