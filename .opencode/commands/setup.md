---
description: Idempotent from-zero setup for a fresh machine (was /first_prompt)
---

Bring a fresh machine to the current state. This replaces the original
first prompt; run it from an empty clone of this repo.

0. Prereqs on PATH: python3, cmake, gcc, SDL2, libepoxy, pkg-config
   (toolkit's tools/linux/install_deps.sh covers Debian/Arch/Fedora
   families — sudo, ask the user before running it).
1. Fetch the toolkit at the pinned commit:
   `git clone https://github.com/sp00nznet/xboxrecomp.git && cd xboxrecomp && git checkout 37b61d589168adee3680fabd51805ee81427bf07`
2. `.venv`: `python3 -m venv .venv && .venv/bin/pip install capstone`
3. Disc (user-owned Redump ISO): extract `game/default.xbe` first,
   verify SHA-256 `1e28c3db033a3de471ba78ccdd13a824ebddd17fdd0479bf69a795f63bd92419`
   BEFORE anything else, then `tools.xiso unpack` the full tree into
   `game/` (all gitignored).
4. `scripts/pipeline.sh && scripts/build.sh` — expect ~14,741/14,741
   translated and the 432-stub tally (scripts/stubs.sh must print
   "agree").
5. `scripts/doctor.sh` all green, then report the stage per CHANGELOG.md
   and hand off to STATE.md's queue. Never edit src/recomp/gen/, never
   commit disc-derived or translated bytes, never push (config denies).
