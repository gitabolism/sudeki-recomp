# 0020 — input wall: the USB OHCI model exists but was never armed

**Observed (session 4l/4m).** Our recompile reaches the title screen
(`reference/fb/frame00016.bmp` = "SUDEKI / Press START") but idles there
forever: `logs/run-seed18-225928.out` shows **zero** `NtDeviceIoControlFile`
calls after boot and no pad-device opens (only DVD IOCTLs 0x70000/0x74004
are answered). xemu (the working reference, note 0019) delivers pad input,
sound, and vibration for the same disc, so the title's input path is
normal, not exotic.

**Static facts.**
- Sudeki imports **no input-specific kernel ordinals** (119-import census,
  note in session log). It links `XAPILIB` LTCG'd into `.text`; XAPILIB's
  XInput is a full **USB host stack in the title itself**, driving the
  MCPX OHCI controllers at `0xFED00000`/`0xFED08000` via MMIO (it imports
  `HalReadWritePCISpace` + `KeConnectInterrupt`, the PCI probe + IRQ claim
  that stack uses).
- Zero real `int 0x2E` sites in the image (2426 `\xcd\x2e` byte pairs are
  all code-slide false positives; disasm output has none) — the sysfile
  path is not used here.
- The toolkit **already contains** a complete model: `src/usb/ohci.c`
  (OHCI 1.0a host controllers, root hub, ED/TD walker, `ohci_thread`) and
  `src/usb/usb_gamepad.c` (Microsoft Xbox pad, interface class 0x58/0x42,
  20-byte input report, wired to SDL input via `input_map`). The symbols
  are linked into `build/sudeki` (`ohci_read`, `ohci_walk_eds`,
  `usb_gamepad_report`, …) — and **`xbox_OhciInit` is never called by our
  project** (grep: no call sites outside `src/usb/`). `xbox_memory_layout.c`
  already routes faults in the MCPX span to `xbox_OhciHandleMmio`
  (VEH/POSIX routers at lines ~879/907), and `OHCI_TRAP_POSIX` compiles
  in on `__x86_64__ && __linux__` (ohci.c line 31 — the header comment
  predating that is stale; this is the same trap-coverage family as the
  AC97 bug fixed by patch 0022, but here x86-64 Linux IS covered).
- Meanwhile `xbox_memory_layout.c` backs the whole MCPX span (incl.
  0xFED00000) as **plain committed RAM** ("ponytail: no register
  semantics"). So today the title's OHCI probe reads `HcRevision == 0`,
  concludes no controller, and gives up silently — no fault, no log line.
  That matches the run evidence exactly (zero USB-related activity).

**Hypothesis.** Calling `xbox_OhciInit()` from our host `main.c` (after
memory layout, like the fence mirror) plus `RECOMP_USB=1` arms the model:
the two 4 KB register blocks get `PROT_NONE` + fault-routed, the title's
XAPILIB stack enumerates the root hub, gets the synthetic pad on a
downstream port, and `XInputGetState`-equivalent polling flows through
`usb_gamepad_report` → SDL. `xbox_OhciInit` is env-gated (`RECOMP_USB`)
and idempotent, so nothing changes without the flag — one variable.

**Fix applied (smallest).** `src/main.c`: extern-prototype + call
`xbox_OhciInit()` after the APU block; `xbox_OhciReport()` registered via
`atexit` so the census prints at exit/timeout.

**Test.** Same working recipe + `RECOMP_USB=1 RECOMP_USB_TRACE=1`. Look
for `[OHCI]` trace lines (did the driver touch the registers at all —
confirms the hypothesis), enumeration progress, and any new ordinals/
ICALLs from the XAPI stack.

## Outcome (so far; discriminating run pending)

1. **Arming works.** `src/main.c` calls `xbox_OhciInit()` (note-0020 edit);
   `logs/run-usb1-*.out`: the title's OHCI init is real — HcRevision 0x10,
   reset, HCCA at `HcHCCA`, control+periodic+bulk enables, and **full
   enumeration**: `80 06` GET_DESCRIPTOR(device) → `00 05` SET_ADDRESS 1 →
   `80 06` GET_DESCRIPTOR(config) → `00 09` SET_CONFIGURATION → vendor
   `C1 06 value 4200` GET_XID_DESCRIPTOR (answered: gamepad S, 20 B in /
   6 B out) → `A1 01 value 0100 len 20` class status poll. Pad sits on
   HC0 port 3 (`+0x5C` reads 0x00100103 = CCS|PES|PPS after enable).
   `KeConnectInterrupt: vector 1 -> routine 0x0031428B` = the XDK USB0
   line; the model's `raised -> ISR claimed it` confirms the guest ISR
   path (and the ISR's cli/sti hit harmless `UNIMPL` no-ops).
2. **Race seen (pad1 run):** one boot stalled with a garbage frame-table
   slot (`periodic slot 0 -> ED 454C4140`) and no SETUP phase; later runs
   enumerated cleanly. Heap jitter dependent — noted, not chased.
3. **Report bit bug found and patched (patch 0023).** Scripted
   `start` pressed 8× through `RECOMP_PAD_LIVE` at the toolkit's
   `0x10` (report byte 2, a *reserved* bit on real Xbox hardware, where
   the digital field is 16-bit LE at bytes 2..3 with START = 0x0200):
   the title ignored it (`logs/run-pad2-*.out`, title static across the
   press window). Patch widens the script/synth mask to the true 16-bit
   field (`start`=0x0200, `back`=0x0100). Presses re-fed with
   `digital 0x0200` — title *still* static (`logs/run-pad3-*.out`).
   So the report reaches the model's DMA path (the PAD step lines prove
   `usb_gamepad_report` runs at press time) but something between the
   OHCI DMA write and the title's input consumer is still missing.
4. **Next discriminator:** press DURING the intro movies (t≈15–45 s):
   Bink playback skips on a real pad press, so a working pipeline
   advances the title screen to an earlier frame index than baseline
   (frame 16). Movie-skip/no-skip separates "XAPI consumer wired" from
   "title-level gating" without guessing at the title's front-end code.

## Outcome (input wall CLOSED)

The discriminator chain resolved it with two runs:

- **Baseline, zero input, 420 s** (`logs/run-base420-*.out`): movies play,
  title arrives ~205 s, game sits at "Press START" until 420 s — **no
  auto-attract demo**, so any later "Buki loading card" cannot be a
  front-end timeout artifact.
- **Skip run** (presses 17–41 s during the FMVs, then every 15 s): movies
  skipped, and the title accepted a START press → **Buki loading card**
  (`reference/fb/skipframe00003.bmp`) — title → New Game → prologue load,
  exactly the flow the user observed on xemu.
- **Title-press run** (20 presses only after the title could be up): the
  front-end again responded — the title cycled into its attract demo:
  `badger.plap` + `S2_woluf/S_Woluf/speech_buki` wavebanks +
  `fma01_poem.bik` replay in the `[FILE]` log tail. Flips then froze
  (0.00 fps at flip 2244) because the demo's 3D scene is drawn through
  the vertex-program batches our NV2A executor skips — that is the GPU
  wall, not input.

The pad3 "six presses, no advance" was timing: its presses (wall ~160–265 s)
straddled the baseline title arrival (~205 s); the ones that mattered
likely landed on the static loading card. Press *density* across the whole
post-boot window is the bring-up lesson; it's in the two runs above.

**Static map established on the way** (for future input work):
- XPP section (`0x30F600–0x3156B8`, size 0x60B8, identical in the JP
  build) = the XDK player-peripheral glue: OHCI DPC `sub_0031495F`
  (registered at device+0x440 via `KeInitializeDpc`),
  ISR `sub_0031428B`; game-side glue calls `0x31046B` (connection
  bit-diff), `0x3104D8` (user open), `0x31052E→0x310DBC` and
  `0x31053A` (state fetch: `rep movsd` of the handle's cached 20-byte
  report at `[handle+0x14]` into the game struct).
- The pad's status report rides the interrupt ED (`periodic slot 0 ->
  ED 00B41490 info=00201081`) and the class control poll `A1 01`; the
  `PAD:` census lines prove the report runs at press time.

`RECOMP_USB=1` now belongs to the working recipe. Patch 0023 corrected
the bring-up script/synth button bits to real hardware layout.
Remaining walls after input: **GPU 3D draw coverage** (demo/level scenes;
150 distinct unhandled methods, VSH batches skipped) and the xemu
title-screen side-by-side capture.
