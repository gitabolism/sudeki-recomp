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

* NT file-pointer semantics violation (patch 0050): bridge
  positioned-reads `lseek` the shared fd and never restore it; the
  guest relies on positioned IO not moving the handle FPO (the
  `[READSEQ]` jumps ARE this violation). frz116 = first 0050 run.
* Open semantic gap: per-handle FPO vs per-fd position — if two
  guest handles to one file map to one host fd (check
  `w32_handle_fd` dup), their file pointers cross (NT keeps them
  separate).
* frz113 core forensics for the record: `018CF14C` fields
  `+0x18` id `0A260002`, `+0x1C` `0x1400`, `+0x20` `0x1000`,
  `+0x8` desc `00F95738`; handle `02611EA4` -> obj `010E3178`
  (`+0` `0x31C8`, `+4` `0x248`, `+8` data `010E3198`).

### Raw-bytes decode (capstone direct from XBE; map: file_off = VA - 0x11000 + 0x1000 for .text)

Byte-verified law: guest VA -> file offset via analysis-json section
fields directly (`raw + (VA_abs - virtual_addr)`); an extra base
subtract decodes garbage (that mistake once produced a bogus vtable
decode — deleted). The pump `0x23B830` prologue bytes match the lift
exactly. Verified raw decode of the pump + siblings:
* Pump `sub_0023B830(mgr)`: eax=0 tick path -> `HI8(mgr+0x28)&2`
  selects scratch = `mgr+4` handle; **esi = E1750(scratch)** (hash
  lookup by node key `mgr+0x04`, e.g. key `EA3D4B48` -> obj
  `010E3178`); NULL obj -> return 0.
* `word[mgr+0x18] == 0xFFFF` -> sentinel STREAM pump (vtable +0x14/
  +4/+0 calls); else job path: `job = ECC40(LO16(id))`; job exists
  -> descriptor `edx = job[+8]` (packed: bits20-23/24-27 shifts,
  16-19 count, **bit2 = async flag**).
* **Async/sync branch = `test dl,4`**: SET -> `F99D0` (4 args);
  CLEAR -> `F9720` (8 args). frz113's "trailer `3EF04C91` bit4" was
  a mislabel: the branch input is `job[+8]` (`0x06610B21`, bit2
  clear -> sync, matches S9720).
* After processor ret!=0: lock `[0x397668]+0xC164` via `[0x3156C0]`,
  guard-counter `+0xC0E4` (init via `1FE690/1FE730`), then sentinel
  -> `rep movsd` record commit into `ebp[0]`; job -> obj vtable
  `[+8]`, `[+4]`, `[0]` commits; unlock `[0x3156C4]`; return 1.
* `F9620` = record-size calculator (type switch 0xC-0xF special,
  else imul chain) — not a gate.
* Tick `sub_001EDA20`: gate `mgr[0x28]&0x100`; `job=ECC40(mgr+0x18)`;
  **pump runs only when `MEM8(job)==0`** (job busy byte) — frz117
  core: job `02977A9C` byte0 = 0, state 0 -> gate OPEN.
* frz117 core dump of the terminal state: manager `018CF14C`
  `{+0x00:00336794, +0x04:EA3D4B48 (node key), +0x18:id 0A260002,
  +0x1C:0x1400, +0x28:00010120 (bit8 set = tick-eligible),
  +0x2E:1}`; job `02977A9C` = triplet array `{0, FFFFFFFF, 06610B21}`
  xN; bucket list `0x37A120..37A19C` = ALL ZERO; global `0x37A1C0`
  table = per-manager ptr array (contains 018CF14C + siblings);
  scratch record at `0x02611640` holds `{tag 0114D7, 00376CC4,
  014B3F38, 00047DB2, 0, mgr, 001E1774 (hash fn), key EA3D4B48}`.
* F9720 guest call sites: `1EB9F4, 1F838E, 239565, 239637, 23B18E,
  23B9DC (pump), 23D7EA`. The stall-era S9720 ra = `239637` =
  **sub_002395E0**, a generic single-shot sync-process helper
  (pool-alloc 0x3c, EEBE0, 8-arg F9720, `ED920`/`2392B0` commits)
  — runs 192+ times during stall for OTHER streams (attract movie).
* Static callers of the tick `EDA20`: `1EDE9B/1EE13D/1EE160`
  (per-object driver family) and `1EF490/1EF4B5` (bucket driver);
  EF0C0 (bucket registrar/driver) callers = `1EBF44/1EC184/1FD69E/
  208F0D/208F22` (registration/completion APIs).
* **Terminal contradiction to resolve:** job alive + byte0=0 +
  mgr bit8 set + obj resolvable = pump gate fully OPEN, yet EDA20
  is called ZERO times after x167 (MTICK 167 session-total, none in
  stall era). The servicing DRIVER stops; the data structures are
  consistent. frz121 (fresh-install + missing-file census + all
  probes) decides whether an unopenable file or a driver gate is
  the stop cause.

### frz123: CQ pushes stop at x167 too; wait-wake mechanics decoded (slot 86 = RtlTryEnterCriticalSection)

* `[CQ]` = `sub_0023AD50` (list-append to queue obj edi, NO event
  Set inside): fired 512+ times during load era (queues
  `0x3D76B0`-family, args `0x3D75FC/0x3D7668`), last x512 at line
  55117 = 16 lines BEFORE pump x167 (55133); no x768 -> CQ total
  <768, silent after. EE110 (mod-256 gated) last entry-log x59 at
  12366 but calls 60..255 unlogged -> timing indeterminate;
  frz124 logs it per-event.
* Loader wait mechanics fully decoded (raw): `26D54D`=CreateEvent,
  `26D5AE`=SetEvent, `26D5CE`=ResetEvent, `26D77A`=WaitForSingle,
  `26D78C`=WaitForMultiple. `EABD0` holds CS `0x37A0F8` across its
  INFINITE wait on {`[0x37A118]=48000007`, `[0x397920]`};
  `EAAB0`'s "poll" = `[0x315818]` = **thunk slot 86 =
  RtlTryEnterCriticalSection(0x37A0F8)** (slot 0=`[0x3156C0]`=
  RtlEnterCS, slot 45=`[0x315774]`=NtWaitForSingleObjectEx — the
  `0xFE000000+i*4` image placeholders in `[0x3156xx]` ARE the
  kernel thunk VAs, not uninitialized slots).
* Worker queue: `F0F80` moves pending-list `[0x3D7720]` -> queue
  `[0x3D771C]` (F0DF0, sole writers) and processes obj vtable[+4]
  + CQ-append; returns 1 while `[0x3D7740]|[0x3D771C]` non-null,
  Sets `[0x37A118]` in its handshake (1F0FBF/1F103F) = the 5 event
  Sets == the 5 loader pulses. After x167 both globals null ->
  F0F80 idle -> no Sets -> EABD0 parks INFINITE.
* `EF0C0` walk still fires 475x total, deep into stall era (line
  103k), but walks the `018B79xx` container family (attract/video
  streams), all mode=0 — NOT the load chain's `0x3D7620` list.
* Model: servicing = 5 event-driven pulses; each pulse walks the
  container arrays and pumps ready managers (all 167 MTICK via
  EE110); queue producers (EF0C0 callers `EBF44/EC184/FD69E/
  208F0D`) stopped feeding the LOAD list long before x167; the
  17-position READSEQ cycling is the attract-movie stream, not
  level data (frz124 confirms). Job `0A260002` (size one 5120-B
  slot) waits for a next-leg registration that never happens:
  next question = which API registers the next leg, and which
  gate refused it (EBF44/EC184 callers of EF0C0).

### frz121-122: regime split REFUTED; loader wait anatomy decoded raw; ENQ(EE190) producer = 5 calls EVER

* frz121 (fresh cache, R1): SAME terminal pump x167 (`018CF14C`
  id 0A260002 node EA3D4B48, backtrace `001EDB68 <- 001EE160 <-
  001EE221`), baf=1080 reads — identical to frz119. frz116's "x119
  freeze" was kill-timing. One regime.
* Loader stack decoded raw (`1EABD0` = game load-wait API, single
  caller `21B1B`): creates two autoreset events via `26D54D` ->
  handles `[0x37A118]=48000007`, `[0x37A114]=48000004`; waits
  INFINITE `WaitForMultipleObjectsEx`; between wakes: `F0F80` spin
  (returns while `[0x3D7740]|[0x3D771C]` work-queue non-null;
  processes queue `[0x3D771C]` via obj vtable[+4] + `23AD50`
  commit) then `EE190(list [0x3D7620], ctx 0x3975A8, [0x3D768C])`.
* `EAACC0` per-frame helper: iterates `[0x397564]` callback list,
  `F1170`, then `EAAB0()` + `EE190(list [0x3D7668], ctx 0x3975AC,
  [0x3D7644])` gated on `[0x3D7668] != 0`.
* **[ENQ] wrapper IS EE190** (frz106 vintage): EE190 fired exactly
  FIVE times in the whole session (x0-x4; x4 immediately after pump
  x167, ret 0). So during the stall era the heartbeat `EAACC0 ->
  EE190` stops running entirely, while `EAAB0` (EACYC) still loops
  from other sites (`1EF27C/1F153E/1F1938`) -> `F0F80` spin sees
  empty queues (`[0x3D7740]==0`, `[0x3D771C]==0` -> ret 0).
* NtSetEvent (ordinal 225, logged) fires on 48000007 (5x) and
  48000004 (3x) from `26D5BA` (CRT signal helper reached from the
  loaders' own exit paths) — the loader events DO get Set; the
  loader wakes, finds nothing, re-parks. "Events never Set" corrected.
* Installed-file content check at the 17 cycled READSEQ positions:
  24602720/24608864/24617056/25131104/26138720/27594848/27621472 =
  ALL ZERO (64-B probe); 24631392/25315424/27709440 have data
  (delta-compressed + strings `Hide from quest log`/`Element4`).
  Write-map proved 0..27,762,688 written once, no gaps — the guest
  itself installed those zeros, then polls those slots for content
  that never arrives. Job size `0x1400` = 5120 = exactly one slot.
* frz123 probes: `[EE110]` (container+arr+cnt) + `[CQ]` (sub_0023AD50
  completion-queue push, queues `0x3D75FC`/`0x3D76F8`) -> names the
  heartbeat link that dies after x167.

### frz117: tick runs, pump gate refuses after x167; EED70 stream-swaps keep cycling

* R2 replay #7 (deterministic): pump x167 = same final bypass pump
  (`018CF14C` id=0002 eax=scratch `02611EA4`).
* `[MTICK]` per-event: tick ran 167 times total — after x167 the
  tick still ENTERS (its `23A4F0`->`F9620` leg fires 256+ times) but
  never reaches a pump: at `1EDB17` the tick's own `ECC40(mgr+0x18)`
  now returns NONZERO -> pump skipped (pump fires only when the job
  lookup is NULL); the follow-on branch requires `D550(mgr)==0` —
  `D550` fired only 2x ever (boot era, both -> 1).
* `[EED70]` (>=257x incl. stall era; sites 1F92C9/1F99A9/1F95C3-tail):
  it is NOT job completion — it is the double-buffer STREAM SWAP
  (swap `obj+0x10 <-> obj+0x34`, ring helpers `23BAF0/BB30/BBC0`,
  tail-call thunk `[0x3156C4]` with free-buffer `0x37A3B0`). The
  stream ring keeps cycling under the stall.
* Model refinement: after x167 the final job `0A260002` sits with
  state&3 != 0 (in flight). Only the service processor can advance
  it; the processor consumes RECORDS from the stream; the record
  chain cycles 17 positions with zero-padded headers. Deadlock is
  data-shaped, not gate-shaped — hence frz118: fresh-install write
  map (`RECOMP_IO_WRITE_MAX=0`) to test whether the installed tail
  regions were ever WRITTEN with the real data (bridge write bug)
  or installed as zeros by the guest itself (installer output wrong
  -> decoder/stream-assembly bug, note 0010 machinery).

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

### frz105: H31.4 hypothesis — flag `0x397110` gating (REFUTED by the probe below at line ~99)

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

## frz125 — wake-producer census: the handshake pair decoded (H31 re-pose x10)

* `[SMEVT]` (26D5AE SetEvent wrapper, ra+handle) 110 Sets, exactly 5
  guest call-site families: `21CD0`→h=48000001 x51, `220E0`→48000002
  x45, `2C5CE0+370`→48000013/0F x6, `1EAB90`→48000006 x5,
  `11D200+1863`→48000004 x3. Host `[EVENT]` census: every
  `NtSetEvent` is `from=0x0026D5BA` (the wrapper) — **48000007 is
  NEVER Set via NtSetEvent**; the 5 loader pulses = the 5
  `SetEvent(48000006)` calls from `1EAB90`.
* Loader wait-handle array (frz125 core): `[0x37A114]=48000007`,
  `[0x37A118]=48000006` — corrects the older `{48000007,[0x397920]}`
  wording (`[0x397920]=48000001` is the CONSUMER loop's own ack
  event, `21AD0` calls `EABD0` then `ResetEvent(0x397920)`).
* `21CD0` (task poster, wrapped `[POST] q1`) raw decode = routing
  law on `task[+8]` word: bits {15,9,10,11,12} set -> tail-call
  `220E0` (q2, Sets 48000002); else sorted insert into q1 list
  `[0x396E8C]` keyed by `task[+0xC]` + `SetEvent([0x397920])`.
  q2 entries = 45, ALL dead at log-line 15022 (boot era; q2 tasks
  carry flag 0x8001, stall-era q1 tasks carry 0x4001 = bit14, not
  routed to q2). q1 runs to line 52291 (96 entries).
* Consumer thread body = `sub_00021AD0`: loop = `EABD0(evt)` wait ->
  `ResetEvent(0x397920)` -> CS 36CC54 -> drain q1 popping tail
  `[0x396E8C]`: call vtable slot1 (type tag; `3381B0` -> `20E520`
  returns "MATR"), call slot0 `(1)` = `220A70` (bit0 arg = don't
  destruct; Run for this type only swaps vtable `3381B0 -> 33818C`).
  The two last tasks (018CA250/A0) ran and drained; queue null at
  kill. The engine poster for q1 tasks = site `18EE11` (inside
  `18EDA0`, an async-complete callback of object vtable `332C54`;
  it allocs a 0x50 task via `125A80` (XOR-obfuscated fields vs
  `[0x39E650]` table) and posts via `21F90`).
* **Wake pair law (raw decode of `1EAAB0`/`1EAB90`):** evt7 =
  `[0x37A114]` "loader run", evt6 = `[0x37A118]` "engine parked";
  flags `[0x37A0F4]` request-pending, `[0x37A0F5]` engine-waiting.
  `EAAB0` (engine load-wait, caller = `EF1D0` bucket walk): if F4
  set -> ret; WFSO evt6 0; `1FE8A0`/`1FE690` on `[0x397668]`;
  `ResetEvent(evt7)`; F5=1; `TryEnterCS(37A0F8)`: success ->
  `SetEvent(evt7)`, F4=1; fail -> <=1000x{WFSO evt6, WFMO evt7
  5ms} retry, and if evt6 ever Set -> `SetEvent(evt7)` WITHOUT CS.
  `1EAB90` (ack; SMEVT-proven ran exactly 5x): if F4 -> CS-leave +
  clear F4; WFSO evt6; if evt6 unset -> `SetEvent(evt6)`, F5=0.
* `[EACYC]` census (old probe, entry gate `n<20 || n%64==0` — mod
  gate again): EAAB0 called >=65x, last logged entry x64 at line
  25849 (~29k lines BEFORE pump x167), every logged return =
  `0x102` WAIT_TIMEOUT. The engine load-wait stops even ATTEMPTING
  long before the servicing chain's last pump.
* frz125 core at kill: F4=F5=0, CS `37A0F8` all-zero (free — the
  "loader holds CS across INFINITE wait" claim is WRONG for the
  terminal state), q1 null, buckets empty, loader thread 6 inside
  `xbox_NtWaitForMultipleObjectsEx` <- `26D78C` <- `EABD0`.
* Model: 5 pulses = 5 `1EAB90` acks; after the 5th, EAAB0 never
  fires again (EF1D0 never reaches it — buckets empty), loader
  parks, and nothing in the engine re-requests. The question moves
  one step up: WHO calls `EF1D0`/feeds its buckets, and who is
  supposed to re-arm the request after the 5th pulse. frz126 adds
  `[EACK]` (1EAB90 ra census) + state-rich `[EACYC]` (f4/f5/e6/e7
  per call, every call printed).

## frz126 — EACK/EACYC state census: the heartbeat module decoded (H31 re-pose x11)

* `[EACK]` (1EAB90, the ONLY producer of loader wake evt6=48000006):
  exactly 5 calls the whole run, ALL from `1EACC0 + 2152` (its ack
  branch). Lines 13058,13401,52726,53105,**53913** (16 lines before
  pump x167=53929). After the 5th ack, evt6 never Set again ->
  loader WFMO{evt7,evt6} parks.
* `[EACYC]` (EAAB0) state-rich, 73 calls: EVERY one returns `0x102`
  immediately with f4=0 f5=0 = early-exit "evt6 not Set" (EAAB0 only
  proceeds while evt6 is Set, i.e. while a loader pulse is live).
  Last call x72 at 53897 (32 lines before x167).
* `[ENQ]` (EE190) = 8 calls: 4 boot-era, then 53114,53481,53926,
  53936 — two straddle x167; its loader-wake branch (WFSO evt7 ->
  Reset evt6 -> **Set evt7** -> WFSO evt6 INFINITE, gated on the
  `EC880(container)` flag byte) was NEVER taken (zero h=48000007
  Sets in SMEVT).
* `[REQ]` (EABD0) = 20 entries; late-era 52690/52912 AND one at
  106936 = the loader thread keeps servicing ATTRACT-era work; it
  is not dead, it just never gets evt6/evt7 for the LOAD module.
* frz126 core at kill: EVERY async-load module global is NULL —
  container `[0x3D7668]=0`, queues `[0x3D7620]/[0x3D768C]/
  [0x3D7740]/[0x3D771C]=0`, `[0x3D7644]=0`, service-CB list
  `[0x397564]=0`, `[0x3975AC]=0`. The module is IDLE even though
  manager `018CF14C` still carries job `0A260002` (frz123). The
  job's issuing context is no longer hooked into the module.
* Thread census (BRIEF risk #2 re-check): 17 `PsCreateSystemThread
  Ex` — 3 boot (`26C68F`,`21AD0`,`21B90`) + 14x worker `2C5AC0`
  (async IO workers, boot..load era). EVERY start routine has a
  dispatch entry. Clean.
* Heartbeat module raw decode: `1EACC0` = loader busy-check +
  service: iterates CB list `[0x397564]`, `1F1170`; if async
  container `[0x3D7668]` -> `EAAB0` + `EE190(ctx 3975AC, [0],
  [0], 3D7668)`; ELSE ack branch if F5|3D7620|3D768C|3D7740|
  3D771C any nonzero -> `EAB90`; returns `WFSO(evt6,0)!=0`.
  `1EAD70` = engine load-wait session (single caller `11AF57`,
  main loop): while EACC0 busy: service ctx 397668 + Sleep(16),
  up to 300x. `1F1470`/`1F1870` = async issue-and-service loops
  (CS `37A4D0`, queue selector `ctx+8` in {3D7740,3D771C,3D76F8,
  3D7764}, issue `1F06C0(ctx)`, wake attempt `EAAB0`, Sleep(5)
  retry; main-thread checks `[0x379EA0]`, flags 3970E5/E6/E7).
* Model x11: engine-side load state machine goes IDLE (all module
  globals NULL, heartbeat never busy, ack branch skipped) while a
  job is still registered on its manager. The next-leg issue
  through `1F1470/1F1870` never happens; frz127 censuses EBUSY/
  EDRV/ISSU1/ISSU2 to name whether those loops exited or spin with
  ctx==0, and whether EAD70 wait-sessions ever return nonzero.

### frz126 addendum — the loader's own worker loop `1EAC20` + EE190 arg law

* `EE190(container-ctx ecx, list eax; a1, FLAG stack args)`: the wake
  branch is gated by the caller's FLAG byte (`[esp+0x1c]` = 2nd stack
  arg), not by `EC880`. Only TWO call sites exist, both in the loader
  module: `1EAC83` — FLAG=1, (ctx `3975A8`, list `3D7620`, a1
  `3D768C`) — and `1EAD08` — FLAG=0 (EACC0 busy branch, ctx `3975AC`,
  container `3D7668`). frz126 `[ENQ]` census shows all 8 calls carry
  list=3D7620/a1=3D768C = the `1EAC83` site, FLAG=1.
* With FLAG=1 the branch is: `WFSO(evt7,0)`; if evt7 NOT Set ->
  `ResetEvent(evt6) + SetEvent(evt7)`; then `WFSO(evt6,INFINITE)` —
  the request/ack rendezvous. But SMEVT saw ZERO `h=48000007` Sets,
  which means every one of the 8 calls found evt7 ALREADY Set
  (`je 0x1ee1e2` skip to the CS-walk tail) — yet no Set of evt7 was
  ever logged either. One of those two readings is wrong; frz128
  must log at `[ENQ]` entry: flag + pre-state evt7/evt6.
* `sub_001EAC20` decoded (loader-side loop): local WFMO over event
  array (esp+8, INFINITE), then `call F0F80` spin-until-0 (worker
  queues), then `if [0x3D7620]: EE190(FLAG=1)` and repeat. So
  `3D7620` pending-list non-empty == the pulse-request trigger, and
  `F0F80` (only 11 calls whole-run!) sits IN the loader loop between
  waits. The 21AD0-vs-1EAC20 role split is still fuzzy: both are
  loader-thread bodies; EABD0 (REQ x20) is the wait helper.

## Outcome — session 5a close (2026-11-23 02:5x)

What was TRUE at session end (evidence = frz121–127 logs + cores
frz117/121/123/125/126/127 + raw capstone decode of the XBE):

* The servicing model IS event-pulsed, and the pulse supply is tiny:
  loader WFMO{evt7=[0x37A114]48000007, evt6=[0x37A118]48000006};
  **evt6 was Set exactly 5x whole-run, all inside `1EAB90`, all
  called from `1EACC0`'s ack branch**; evt7 was NEVER Set (SMEVT
  census on the only guest Set wrapper `26D5AE`; every host-side
  `NtSetEvent` in the EVENT_LOG is `from=0x0026D5BA`, the wrapper).
* The engine-side async-load module goes IDLE at the wall: all
  module globals (`3D7620/3D768C/3D7740/3D771C/3D7668/3D7644`,
  CB list `397564`, ctx `3975AC`) are NULL at kill, while manager
  `018CF14C` still carries job `0A260002` (size `0x1400`) with the
  tick pump gate OPEN (`MEM8(job)==0`, frz117/123/126 cores).
* The async issue-and-service loops are the dead edge: `1F1470`
  called **once** in a whole run (boot era, ctx `014B6300`,
  `ctx+0x34`=2), `1F1870` **zero**. `EAD70` load-wait sessions: 4,
  all instant non-busy returns. `EACC0` heartbeat runs all run
  (x4096+ calls, engine-frame driven) and always sees idle after
  the ~x1555 window (one transient `q20=014BCA58 al=01`).
* Loader thread is NOT dead and host-side loss is unevidenced: it
  re-enters `EABD0` in the attract era (REQ line 106936 frz126) and
  services attract IO; the CS `37A0F8` is free at kill; 17/17 guest
  threads dispatched (14x `2C5AC0` async workers alive).
* Decoded law (raw, this session): `21CD0` task routing (flag bits
  {15,9,10,11,12} -> `220E0` q2 route, else sorted q1 insert by
  `task[+0xC]`; q2 tasks flag 0x8001 = boot-era only); consumer
  `21AD0` drain body; `EE190(ctx,list;a1,FLAG)` wake gated by the
  caller FLAG arg (only sites `1EAC83` FLAG=1 and `1EAD08` FLAG=0);
  loader worker loop `1EAC20` (`F0F80` spin -> `[0x3D7620]` ->
  EE190 FLAG=1); `EAAB0` only proceeds while evt6 is Set (all 73
  census calls early-exit `0x102`); `1F1470/1F1870` issue loops
  (CS `37A4D0`, selector `ctx+8` in {3D7740,3D771C,3D76F8,3D7764},
  issue `1F06C0`, wake `EAAB0`, Sleep(5)); the lone `1F1470` call
  came from the `1F3310` (display-mode setter) region.
* REFUTED this session: R1/R2 regime split (frz121); trailer-dword
  bit4 branch story (raw pump decode `test dl,4` on `job[+8]`);
  "loader events never Set" (evt6: 5 Sets); "task poster died at
  x167" (q1 posts alive to 52291; the gap is the NEVER-re-issued
  leg, upstream of tasks).
* STILL OPEN (single contradiction): 8 `EE190` calls from `1EAC83`
  carry FLAG=1, whose branch must `SetEvent(evt7)` unless evt7 was
  already Set — yet zero `48000007` Sets are logged anywhere.
  Either the FLAG slot read is off (stack-frame math) or evt7 was
  Set via a path invisible to both censuses. frz128 probes are in
  the tree (committed): `[ENQ]` logs flag+evt state, `[SRST]`
  census on `26D5CE` ResetEvent (the wake branch Resets evt6 as a
  unit with the Set).
* Host-side exoneration NOT yet proven either way — every producer
  that should have fired is guest code whose entry census shows it
  was never called; no host path swallowed a Set/queue entry found
  so far.

## frz129 pre-work — static leg-issuer anatomy (session 5b, lifted-C + raw scan, no probes yet)

* ISSU1 `1F1470` has exactly TWO static call sites: `sub_001F0410`
  (thiscall vtable leaf: `1F1470(obj+0x14, 0)`) and `sub_001F1280`
  (site `1F12F2`). frz128 symbolization (matched bin, host base
  `0x55c14e4c5000`) CORRECTS the session-5a attribution: the lone
  ISSU1 call ran through `sub_001F1280+0x974` — the engine issue
  path — NOT the vtable leaf. Supporting symbolized sites (frz128):
  REQ = `21AD0+0x4cf` (consumer body -> EABD0); EACYC caller =
  `EF1D0+0xe5e` (bucket walk -> EAAB0, matches frz125); CQ push =
  `EF1D0+0x240a`; both PUMP ra's = `EDA20_gen` (tick main+bypass).
  The `0x001F0410` vtable leaf (slot 4 of `0x00336620`,
  "Secret"/"Blur" class) exists but was never taken.
* `1F1280` has ONE caller: `sub_001EF770` (site `1EF7B9`), gated by
  (a) `MEM8(0x397110) != 0` — the H31.4 engine-frame flag, refuted
  as a stall cause but still the gate on THIS path; (b)
  `sub_001F8670(0) == 0` — session lookup: walks the 32-bucket
  table at `[0x397620]` under CS `[0x3156C0]`, calls each entry's
  vtable[+4], matches key; FOUND (ret!=0) -> issue SKIPPED. `EF770`
  itself has one caller: `sub_00157B10` (game layer, site `157C84`).
* ISSU2 `1F1870` has ONE static caller chain: `sub_0023E540`
  (leaf at `23E56C`); its callers: `sub_00208C80` (3 sites, the
  ranged-read driver: vtable `+0x18` open, descriptor scan on
  `obj[+0x10]` array for mask `0x1000000`, range math via
  `202750`, then 23E540 closes slots), plus `2304C2/23E1D6/23E824`.
  `208C80` callers = asset-API era sites `2021EA/202678/20DCCF/
  20E31B/211596/21195F/211CA2/211ECA`. Adjacent `sub_00208E60`
  holds the live `EF0C0` registrar sites (`208F0D/208F22`) — same
  module, so the module IS alive at the wall while the 23E540
  leg never runs.
* frz129 probe shortlist (one variable = leg issuer): `[EF770]`
  entry + `397110` state + `F8670` return + caller-frame key args
  (does the engine issue-driver run, and which gate refuses);
  `[OPEN2]` (`208C80`) entry + descriptor-scan hit count;
  `[SESS]` (`F8670`) entry + found-bucket. Pick AT MOST two.

## frz128 — ENQ/SRST verdict: the evt7 contradiction resolves to a WAIT census, not a Set census

Run `logs/run-frz128-050223.out` (58,395 lines, watcher ABRT 30 s after
pump x167 at line 52125, core `/tmp/opencode/sudeki.frz128.core`,
matched bin `sudeki-frz128.bin`, host base 0x55c14e4c5000). Nav was
VOIDED this run (harness gate matched the boot-era NV2A vblank "ISR
claimed it" line, presses landed before enumeration) — terminal shape
is era-general (frz95 control), so the census stands.

* Terminal shape REPRODUCED bit-for-bit vs frz126/127: PUMP x167 last
  (mgr `018CF14C` id `0002` eax-scratch `02611EA4` node `EA3D4B48`,
  backtrace `1EDB68 1EE160 1EE221 1EAC88 21B1B` — the terminal pump
  runs INSIDE EE190 called from the loader loop); ISSU1 x1 (ctx
  `014B6300` out=2, via the vtable leaf — ra `5040d871`), ISSU2 0,
  RESADD 0, RESREL 21, REQ 20 (late REQ = attract servicing), MTICK
  167, CQ last at x512, EACYC last logged x72 -> 0x102.
* `[SRST]` (guest `26D5CE` ResetEvent, gate n<400, exhaustive 41):
  Reset(48000006=evt6) x5 ONLY, all `sub_001EABD0_gen+0xf1c` (the
  loader loop's own pre-WFMO hygiene); Reset(evt7) zero; consumer
  resets = `21B90` h=48000002, `21AD0+0x57e` h=48000001 (evt1 ack).
* `[SMEVT]` (guest `26D5AE`, exhaustive 110): Set(evt6) x5 all
  `sub_001EAB90_gen+0x44b`; **Set(evt7)=zero**, consistent with host
  EVENT census (110 Sets, no `48000007`; that census IS complete —
  2,941 kernel ordinal lines, no cap).
* DECISIVE NEW FACT: `[BRIDGE] NtCreateEvent` line 186 — evt7
  (host `0x5A000034` = guest token `48000007`) is created **init=1
  (SIGNALED)**; evt6 (`0x5A000030`) init=0. So EE190's FLAG=1 wake
  block CAN legitimately skip on a signalled evt7 (`WFSO(evt7,0)==0
  -> je`) — the first call consumes the initial signal.
* BUT raw decode (capstone, byte-exact) of `1EE190`: prologue is 5
  pushes; the FLAG byte read at `1EE1A0` is `MOV AL,[ESP+0x1C]` =
  entry `esp+8` = exactly the slot the ENQ wrapper proved = 1; the
  block `1EE1AB..1EE1E2` is WFSO(evt7,0)/Reset(evt6)/Set(evt7)/
  WFSO(evt6,INFINITE) and EVERY path except `WFSO(evt7,0)==0` must
  log Reset(evt6) through `26D5CE` (exhaustively censused: zero from
  the EE190 region). => either every EE190 call found evt7 Set
  (needs an invisible re-Signal — host-side, since guest Set census
  is complete), or the wait never reached the bridge.
* TRAP FOUND (why prior "no wait on evt7 ever" claims were vacuous):
  the bridge wait-token census in `kernel_bridge.c` prints only the
  FIRST 300 calls (`static int logged` cap) — boot-era polling ate
  the budget at line ~600; all later WFSO/WFMO token dumps are
  suppressed. Absence of `token=0x48000007` lines after line 600
  proves NOTHING. (Early census shows boot WFMO{48000006,48000001}
  INFINITE from `26D705` — the array law needs re-verification too:
  frz125 core said `{evt7,evt6}`, this census line shows
  `{evt6,evt1}` — different wait sites.)
* frz129 (ONE variable): manual wrappers on the guest wait wrappers
  `[W1SO]` `26D77A` (h,ms -> ret) and `[W2MO]` `26D78C`
  (array,count,all,ms -> ret), print ra+token(s)+timeout+ret gated
  to tokens {48000001,48000006,48000007} or INFINITE (bounded set;
  the census that the 300-cap bridge log cannot give). Settles:
  does EE190 ever Wait on evt7, and does evt7 re-signal host-side.

## frz129 — wait census (W1SO) verdict + W2MO probe bug (my own)

Run `logs/run-frz129-051623.out` (62,788 lines; nav WORKED this run —
gate fixed to `[OHCI0] raised 00000002`, 8 STARTs fired; pump x167 at
line 55449, mgr `018BF14C` id `0002` node `EA3D4B48` — same shape,
heap addresses shifted by the nav era). Core
`/tmp/opencode/sudeki.frz129.core`, bin `sudeki-frz129.bin`, base
`0x56388dd3e000` (.text runtime `0x56388de4ce00`).

* `[W1SO]` (exhaustive guest-WFSO census, tokens {1,2,4,6,7}):
  evt6 ms=0: 2735 polls, 2719 timeouts, 16 successes;
  **evt7 ms=0: 131 polls, 126 returned SIGNALED(0), 5 returned
  0x102**; pollers (symbolized): `EE190_gen+0x43d` (=1EE1B4, the
  FLAG=1 wake gate), `EE190_gen+0x1389` (=1EE260, a SECOND evt7
  gate inside the per-entry servicing loop), `F0F80_gen+0x292`
  (=1F0FA3) and `+0xf2d` (=1F1024, the worker-queue handshake).
* Raw decode settles the wake-branch polarity (EE190 AND F0F80 carry
  the same unit): `WFSO(evt7,0); if evt7 SET -> skip; else
  Reset(evt6)+Set(evt7)+WFSO(evt6,INFINITE)` — the park path fires
  ONLY when evt7 is cleared. evt7 = notification event (type 0)
  **created SIGNALED** (bridge create line 186: init=1), guest
  censuses show it is never Set and never Reset => stays signalled
  => the wake/park rendezvous is permanently short-circuited, and
  that is GUEST-COHERENT given the semantics — not a host loss.
  frz126's "OPEN contradiction" was a branch-polarity misread.
* 5 evt7 timeout polls (lines 13385,14029,54490,55156,**55456 = last
  poll before terminal pump x167**) stay unexplained (no guest Reset
  in exhaustive SRST) — edge effect, parked.
* **W2MO=0 was MY probe bug**: `26D78C` entry args are
  `(count, hArray, waitAll, ms)` (`26D6A8` validates `[ebp+8]` vs
  0x40 = the count); the frz129 wrapper read them swapped, so the
  token filter never matched. Fixed in `recomp_manual.c`. Census
  law re-affirmed: decode the wrapper's raw arg order before
  trusting any zero count.
* EE190 second wake gate (per-entry: 1EE250 `test byte[esp+0x18]` =
  same FLAG, then 1EE258 poll) — with evt7 signalled, EE190 never
  yields mid-list; it services buckets directly — that is where
  pumps x62..x167 ran from.

frz130 = fixed `[W2MO]` (which tokens the loader loop really parks
on and what those waits return) + W1SO control. One variable:
multi-wait census.

## frz130 — fixed W2MO truth + issuer-edge census; issuer CHAIN static-complete

Run `logs/run-frz130-052530.out` (62,807 lines; pump x167 at line
55975, mgr `018CF14C`, node `EA3D4B48` — frz127 shape exactly). Bin
`/tmp/opencode/sudeki-frz130.bin`, core `sudeki.frz130.core`, guest
ram base `0x7f082d200000`.

* `[W2MO]` (fixed args): loader thread parks in
  `26D78C{48000006,48000001}` INFINITE at `EABD0_gen+0xbc9` (x14+3)
  and `+0x1090` (x3): 17 wakeups return evt1 (the REQ event —
  matches `[REQ]` x20), 4 return evt6; final wait x2227 AFTER pump
  x167. Loader thread = healthy-and-parked, not lost, not spinning.
* `[ISSDRV]` (1EF770 engine session-issue driver): **2 calls whole-
  run, both boot-era**, flag `397110`=1, returned object `01CA0420`.
  Sole call site (raw scan): `0x157C7F` inside session method
  `sub_00157B10` (guest; single caller `0x7D278`).
* Raw decode law (`1EF770`): flag397110 -> `1EF720` acquire ->
  `F8670(0)` session lookup — **if session EXISTS (ret!=0), skip
  new issue**; else `1F1280(ecx=0x40)` -> ISSU1. So ISSU1 x2 ==
  ISSDRV x2; the session-exists gate is NOT the blocker — EF770
  itself is never re-called.
* `[SESS]` (1F8670): 32 calls, ALL return 0 (no active session) —
  2 from EF770, 30 from `F1650_gen+0x19b0`. Corroborates: no
  session exists at the wall; nobody creates one.
* `[OPEN2]` (208C80, sole ISSU2 route): 0 calls — consistent.
* Hot-cache integrity: `[WRITE236]` map = 452 segs, gap-free to
  27,762,688 (`startup.baf`); `[READSEQ]` max-pos census across ALL
  paths normal (`shared.baf` read to 129 MB; every xwb/sf/bik read
  fine). Zero-sea at 25,137,248 is guest-written (<64K runs; may be
  legit padding — NOT a host IO failure).
* Static issuer chain (raw-scan, complete):
  vtable slot (LE ref at file `0x3229E8`) -> `6AD00` -> `74F10` ->
  `7D1E0` -> `157B10` -> branch on 5th-arg bit0: drain path ->
  `EF7C0` (1EF7C0, 12 call sites incl. slot-sweep `4E1F0` with
  6/9/7/3/5 slot arrays, offset sentinel `0x7FFFF`) or new-session
  path -> `EF770` -> `F1280` -> ISSU1.

**H31 re-pose x13: some link of the session-issue chain goes quiet
after boot.** frz131 = ONE census run: `[ISSMTH]` (157B10 entry,
ra + flag arg5), `[EFSUB]` (1EF7C0 entry, ra), `[SWEEP]` (4E1F0
entry, ra + thiscall obj) — settles which link stops: sweep vs
drain-submit vs session method.

## frz131 — issuer-chain census: the submit path DOES run; model corrected

Run `logs/run-frz131-053806.out` (62,544 lines; pump x167 line 55697,
mgr `018BF14C` id `0002` node `EA3D4B48` — same terminal shape).

* `[ISSMTH]` (157B10): **17 calls** whole-run, all `arg5=1` (bit0 SET)
  — so every call takes the DRAIN branch (`EF7C0` at 157BFA/157C2E),
  never the new-session branch.
* `[EFSUB]` (1EF7C0): ~16 calls — one per session method; the
  engine's submit path runs, then EE190 services.
* `[SWEEP]` (4E1F0): **0 calls** — the 5-array slot sweep never runs.
* `[ISSDRV]` (1EF770): 2 (boot-era only), object returned `01C90420`.
* Core (frz130): session table `[0x397620]=0x01CA04A0` allocated with
  **11 sessions, all healthy** (state 1/2, vtable 338B64, hash keys);
  flag `[0x397110]=1`. All 32 `F8670` lookups returned 0 on KEY
  MISS, not table emptiness. No host-side data failure: every
  `game/startup.baf` source read returned `got=want`, zero zero-head
  source reads; cache install write-map gap-free.
* Terminal read plan (`Cache//startup.baf`, fd=33, from=271057):
  24.69M..24.72M steps, jump to 27.56M, 27.61-27.64M, back to
  24.72M, then 24.60M x2 (DESCENDING), then 25,137,248 (5120 B, head
  zeros) = LAST READ EVER; then loader parks (W2MO x2227 INFINITE).
  Non-monotonic plan = container navigation, not linear install.

**Model correction: legs ARE submitted and serviced (17 drains, 167
pumps); the session cursor simply stops advancing after the read at
installed offset 25,137,248.** The standing question is now WHAT the
session was navigating and what it waits for after that read — the
job `id=0002` on manager `018BF14C` and its pump inputs.

## xemu reference capture — pipeline live (Menu artefact unblocked)

`scripts/xemu_titlecap.sh` -> QMP `dump-guest-memory` ->
`tools/xemu_fbpeek.py`: FB auto-scan hit at **VRAM pa 0xF0000000 +
off 0x1A3B000, 640x576 BGRA, score 0.996** (re-measures note 0009's
old-ROM law 0x3C00000/pitch720 — the v1.1.0 layout law). Artefact:
`reference/xemu/title90.png` (xemu at t=90 s; capture pipeline proven;
same-moment pairing with `reference/fb/frz35*` still owed).

Side quests settled en route: xemu RAM image is paged+encrypted —
VA-anchored global reads from RAM dumps are NOT viable (dead end,
don't retry); xemu HDD is qcow2 (header parse pending) and its guest
cache drive is `Z:` — HDD byte-diff parked until the qcow2 L1/L2
walk works.

## frz132 — session-state census: state-3 is NOT the discriminator; loader pump law decoded raw

Run `logs/run-frz132-060206.out` (pump x167 at line 54638 — same
terminal shape). `[SESSN]` (table walk from heartbeat, `x%64`,
change-triggered): x0 `n=0`; x320 `n=12 (9x1,1x2,1x3)`; x1792
`n=31 (26x1,3x2,1x3)` — then NO change until kill. A state-3 session
exists in our runs too (xemu "healthy state-3" signal is not unique);
and session creation itself freezes early (last change x1792).

Loader pump-loop law (raw `1EAC50..1EACBB`, the W2MO site era):
`W2MO(evt6,evt1) -> loop: F0F80 drain while al -> if [0x3D7620]!=0:
EE190(ecx=0x3975A8, eax=[0x3D7620], args 0x3D768C,1) -> jmp loop;
else ResetEvent(evt6=[0x37A118]) -> W2MO again.`
=> servicing is gated SOLELY on the static pending-chain head
`[0x3D7620]` being non-null. Terminal park = chain went null while
job `0A260002` is still registered on manager `018BF14C`.

frz131 core forensics (base `0x7f42f3e00000`):
* manager records (stride 0x3C, vtable `0x336794`): `+4` node key,
  `+8/+C/+10` descriptor heads (F95xxx/F9Bxxx/FBBxxx), `+18` id,
  `+1C/+20` offset/size (0x1400/0x1000), `+28` state word, `+38`
  back-link (terminal `018BD3A4`). Terminal `+28..+30`:
  `00010120 / 1 / 00010080` vs COMPLETED siblings
  `00010184 / 3 / 00010040` — done managers end `state=3`.
* descriptors (16 B): `(type 0x00040001, RAM-OFFSET dest, count
  0x48CD/0x48CB, key 0x??10B29)` — terminal dest `0x01759A80` =
  the exact buffer VA `0x81759A80` of the last read; keys share
  suffix `10B29`; cache dword at `0x17F905C` = `0x06610B21` vs
  job key `0x06610B29` (bit3 differs). FBB block terminal:
  `(0x81759A80, 0x1400, 0x00C87000, 1)` — third word = ~13 MB
  REMAINING while completed siblings show `0x1400`.
* FMH container note: cursor `0x17F9060` is NOT a corrupt hole —
  index record ends at `0x17F905C`, sparse zero padding follows,
  real payload resumes at `0x17F915C`. Install integrity fine.

frz133 = chain lifecycle: uncapped `[CQ]` (23AD50 push census with
queue-head before/after) + `[QHEAD]` static-queue timeline
(3D75FC/3D7620/3D7644/3D7668/3D768C/3D76B0/3D76F8) — settles WHO
de-chains the manager and whether any push re-chains it at the wall.

## frz133 — chain census: completions RUN to the end; harness START-SPAM confounder exposed

Run `logs/run-frz133-060756.out` (pump x167 at line 56780 — same
terminal shape). Full uncapped censuses:

* `[CQ]` (23AD50 pushes): **522 total** — the frz128 "last x512" was
  effectively the real total. Per-queue: `3D76B0 x226, 3D768C x95,
  3D75FC x95, 3D76D4 x46, 3D7764 x31, 3D76F8 x29` — **ZERO pushes
  ever target the pump chain `3D7620` or busy chain `3D7668`**;
  those are fed only by the splice path inside the completion
  engine (1EF112/1EC590/1EC6B0) off REQUEST arrivals.
* `[EF0C0]` (1EF0C0 relay): 398 calls, **ALL mode=0** (queue 5FC);
  mode 1/68C and 2/620 NEVER requested whole-run.
* Terminal burst (lines 56803+): CQ x520-522 = completion ladder of
  one object (`014ACA58`: `620 -> 68C -> 5FC -> 6B0`), then
  `EF0C0 x465..x469` dispatches completions over the manager array
  (`018A7A78..B2C`, stride 0x3C) **including terminal manager
  `018BF14C`** — the machinery COMPLETES and goes quiet; a
  post-hoc `W1SO(evt6)` poll times out and the loader parks.
* So in the terminal era the job set FINISHES; what never happens
  is a NEW issue from the engine side. Consistent with frz42's
  "load ABORTED back to attract" reading.

**Harness confounder (my own, law-worthy):** nav_run2x/3x feeds
`start` every 10 s (8x). frz42 convicted exactly this: **START =
cancel/abort in load & dialog contexts** — our terminal shape may be
START-spam-induced load abort, not (only) an engine gap. frz18's
proven law: 5 FMV-skip starts t=15-40, then New Game starts
t=60/75/90, then TOTAL SILENCE.

frz134 (A/B, one variable = press script, binary identical to
frz133): frz18-exact sequence (5 starts 8 s apart anchored at pad
enumeration, 12 s gap, 3 starts 15 s apart, then ZERO presses),
NO early-kill watcher, full 1150 s window. If the level then
streams (level .baf OPEN, new EF770/EF0C0 activity, level FB dumps)
the "stall" was self-inflicted; if the terminal shape persists
press-free, H31 stands and the issuer hunt resumes.

## frz134 — clean-input A/B: wall is NOT input-induced; H31 stands (re-pose x14)

Run `logs/run-frz134-061615.out` (binary byte-identical to frz133 —
`cmp` proved same-as-frz133; only the press script changed:
frz18-exact 5 starts 8 s apart at pad enumeration, 12 s gap, 3
starts 15 s apart, then TOTAL SILENCE; no early-kill watcher, full
1150 s window, timeout-KILL at the end).

Verdict: `[PUMP] x167` terminal appears at line 55017 with the
IDENTICAL shape (mgr `018CF14C`, id `0002`, `st=00025120`, node
`EA3D4B48`, same backtrace) and `[ISSDRV]` stays x2, `[EF0C0]` stays
all-mode-0 (x422). Buki-era file opens reached at line ~9806
(buki.xwb, Buki_sfx.xwb, SOLWORLDM.gex). So the harness START-spam
was NOT the cause of the terminal shape — the wall reproduces with
frz18-clean input. H31 re-pose x14 stands: engine-side stop-of-issue
after the first job set completes.

Note: this run also confirms the press script (nav_run36a) still
reaches the Buki load era — navigation itself is healthy.

## Static law: only TWO code paths ever feed the pump chain (EF0C0 mode=2)

Raw+lift census of every `call 0x1EF0C0` site (5 total):

| site | enclosing fn | mode arg | runtime (frz133/134 EF0C0 census) |
|---|---|---|---|
| `1EBF3F` | `1EBF20` (EBF20 relay) | computed (all ran as mode 0) | x-part of 398 |
| `1EC17F` | `1EC130` relay | computed (mode 0) | x-part of 398 |
| `1FD699` | **`sub_001FD660`** | **2 (literal)** | never ran |
| `208F08` | **`sub_00208E60`** | **2 (literal)** | never ran |
| `208F1D` | `sub_00208E60` alt branch | (mode not literal) | never ran |

`sub_001FD660(edi=obj, arg1=[esp+8])` = BULK SUBMIT: calls `1FD3A0`,
then loops the request array `[[edi+0x3C]+4]` x `[edi+0x3C][0]` and
calls EF0C0(element, array, **2**, &edi+0x18, index, ...) — i.e. THE
feeder of pending-chain `3D7620` the loader services. Its 3 static
callers: `1FC782` (in `sub_001FC760`, dispatch-entry only — pointer
reachable), `1FCD88` (in `sub_001FC8E0`), `1FDAD2` (in `sub_001FD950`
region). `sub_00208E60(ecx,edx,eax)` mode-2 branch needs
`[esp+0x20]!=0` (an out-param supplied by caller) and a non-null
`[[edi+0x54]+4][esi]` slot; callers `2035D8`, `208E49`, `20FE7D`.

So "EF0C0 mode 1/2 never requested" is structural: the two feeder
FUNCTIONS were never exercised past their gates. frz135 = `[SUB2A]`
(1FD660) + `[SUB2B]` (208E60) entry census settles whether they run
AT ALL (boot-era bulk submits may use them — if x0 whole-run, the
engine's request module never issues follow-up chunks and the cursor
law at 25,137,248 is explained from the requester side).
