```
███████╗██╗   ██╗██████╗ ███████╗██╗  ██╗██╗
██╔════╝██║   ██║██╔══██╗██╔════╝██║ ██╔╝██║
███████╗██║   ██║██║  ██║█████╗  █████╔╝ ██║
╚════██║██║   ██║██║  ██║██╔══╝  ██╔═██╗ ██║
███████║╚██████╔╝██████╔╝███████╗██║  ██╗██║
╚══════╝ ╚═════╝ ╚═════╝ ╚══════╝╚═╝  ╚═╝╚═╝
██████╗ ███████╗ ██████╗ ██████╗ ███╗   ███╗██████╗
██╔══██╗██╔════╝██╔════╝██╔═══██╗████╗ ████║██╔══██╗
██████╔╝█████╗  ██║     ██║   ██║██╔████╔██║██████╔╝
██╔══██╗██╔══╝  ██║     ██║   ██║██║╚██╔╝██║██╔══╝
██║  ██╗███████╗╚██████╗╚██████╔╝██║ ╚═╝ ██║██║
╚═╝  ╚═╝╚══════╝ ╚═════╝ ╚═════╝ ╚═╝     ╚═╝╚═╝
```

**Sudeki** (Xbox, XDK 5849) as a native Linux executable — static
recompilation, not emulation: every game instruction lifted to C, linked
against a kernel/HAL compatibility layer, running at native CPU speed.

**Bring your own disc.** Nothing from the disc enters this repo — no
bytes, no captures, no analysis artefacts.

---

## Status

| Stage | Status | Proof |
|---|---|---|
| **Lift** | ✅ done | 14,741 functions / 1.12M instructions lifted; 432 stubs counted |
| **Entry** | ✅ done | guest runs its own CRT + startup; 119 kernel ordinals exercised |
| **Threads** | ✅ done | every thread start dispatched; no silent exit |
| **Menu** | ✅ reached | title-screen frame dumps ("SUDEKI / Press START"), movies play, audio up |
| **Gameplay** | 🔶 in progress | level + wavebank streaming ✔, AC'97/MCPX audio ✔, real Xbox pad via OHCI ✔, attract/level scene animating ✔ · remaining: menu navigation to a level, saves, crash-free soak |

```
Progress to playable:  ██████████████░░░░░░  ~70 %
```

Walls toppled so far (each with a written note *before* the fix — 23
notes, 29 toolkit patches, all reproducible from the disc alone):
async-IO return convention ✔ · boot-archive decoder starvation ✔ ·
unresolved-ICALL stack drift ✔ · GPU→CPU software-method wake chain ✔ ·
host ke-shadow lock race freezing the attract demo ✔ · AC'97
bus-master trap on x86-64 (audio) ✔ · OHCI pad enumeration ✔ ·
flip-settle chain ✔ · post-attract "freeze" = the game's own
level-loading state, not a hang ✔.

## ETA to a playable release

Sessions here are autonomous engineering sprints, not calendar days —
each has cleared 1–3 walls. At the current pace:

* **first playable level, on a pad**: ≈ 2–4 more sessions
* **"playable release" grade** (saves, audio rock-solid, early game
  end-to-end, long soak clean): ≈ +3–5 sessions after that
* **Vulkan back-end parity** (staged end-state renderer): ≈ +2–3, gated
  on pixel-identical frame dumps vs the GL path
* **next-gen features** (texture packs → HDR → RT — opt-in only, after
  vanilla parity): parked roadmap

Artefacts back every claim: `CHANGELOG.md` and the note series in
`docs/notes/`.

## Run it (you need the disc)

```bash
# clone the toolkit at the pinned commit (see THIRD_PARTY.md), then:
scripts/doctor.sh           # pins + XBE digest check — a FAIL line is your first task
scripts/pipeline.sh         # XBE → disasm → func_id → abi → gen tree (from default.xbe alone)
scripts/build.sh            # toolkit @ pin + our patch series, applied in order
scripts/run.sh              # the game, with watchdog + one-line classification
```

## House rules

1. Never edit generated code — fixes are host-side overrides by address.
2. Nothing disc-derived in git. Frame evidence = file dumps, never screenshots.
3. Every anomaly gets a written note before its fix; one variable at a time.
4. Vanilla first, features later — enhancement code is opt-in and never
   touches the default path.

*Built with the [xboxrecomp](https://github.com/sp00nznet/xboxrecomp)
toolkit (pinned v0.18.0, audited per `THIRD_PARTY.md`) — not affiliated
with Climax or Microsoft; preservation/bring-up project.*
