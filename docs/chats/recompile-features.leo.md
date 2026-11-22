> **SCOPE BANNER (added by the agent, 2026-11-22):** brainstorming record
> only — **not** current scope. Nothing in this chat may influence the
> bring-up stages in `BRIEF.md`. Triage + decisions live in `STATE.md`
> queue item 8 and `THIRD_PARTY.md` ("Toolkit forks considered"). Known
> factual errors in this chat: calls Sudeki a RenderWare game (it is the
> custom SOL engine); claims `RECOMP_GL_SCALE`/`RECOMP_VK_ANISO` exist
> at our v0.18.0 pin (they do not — no Vulkan backend in mainline).
> User directive, same date: **vanilla first, features after** — see
> STATE.md queue 8 for the A/B/C staging.

# User

sudeki pc versus xbox

# Leo

**Sudeki PC and Xbox versions are nearly identical in core content, but the **PC port** offers superior **60fps performance**, **4K resolution** support, and **mouse-controlled camera** functionality, while the **original Xbox** release is considered more stable with **no audio bugs** and includes **fog effects** absent in the PC version.

The PC version, available on **Steam** and **GOG**, suffers from a notorious **late-game audio bug** (a loud waterfall sound) and missing environmental fog, though community patches can address some graphical lag. Conversely, the Xbox version features **Dolby Digital** audio, **HDTV 480p** output, and a **hard drive** for streaming, but is limited by **cramped camera controls** and potential **frame rate drops** in dense areas.

*   **PC Advantages**: Mouse aiming, higher frame rates, modern display support, and easier combat chaining.
*   **Xbox Advantages**: Bug-free audio, original visual effects (fog), and native hardware optimization for its era.
*   **Common Issues**: Both versions share a **short playtime** (~20 hours), **linear design**, and **mixed narrative reception**.
*   **Emulation**: **Xemu** emulation is currently noted as having significant issues, making the native PC port or original Xbox disc the preferred methods for modern play.

# User

Lets say I got a lot of token to spend. Which one should I recompile for linux? The PC version or the xbox version. The PC version does not support gamepad neither... Should I feed my ai with both version of the game?

# Leo

A few things to sort out here:

**The PC version already runs on Linux** — no recompilation needed. ProtonDB reports it "plays perfectly at 60fps" and "works like native" on SteamOS and Nobara. You can just launch it through Steam.

**The PC version does support gamepads.** It uses XInput (`xinput1_2.dll`), so any Xbox 360/One controller works out of the box. The SteamDB note about "not defaulting to external controllers on SteamOS" just means you may need to manually switch the active controller in Steam's Quick Access Menu — it's not a lack of support. The GOG forum confirms a 360 pad works perfectly.

**If you want a true native Linux port (no Wine/Proton), the Xbox version is the one to recompile.** The [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) toolkit (updated yesterday) does exactly this: it takes the Xbox XBE binary, translates every instruction to C, and builds a native x86-64 Linux executable with SDL2/OpenGL (or Vulkan). The Xbox version also has the fog effects and lacks the PC audio bug, so it's the "definitive" build to target.

**On feeding your AI both versions:** that's overkill. The PC port was made by the same Climax team directly from the Xbox codebase — the logic, assets, and structure are essentially identical. The differences are superficial (fog toggle, audio mix, input layer). One version is enough to understand the game's systems. If you're building a recompile, use the Xbox binary as your source of truth and the PC version only as a reference for how Climax handled the input/display layer.

**TL;DR:** Play the PC version on Linux via Proton today (gamepad works). If you want to build a native port, recompile the **Xbox** version with `xboxrecomp`. You don't need both codebases.

# User

Is it possible to boast the texture for 4K and add next gen feature to the game such as RTX, HDR and stuff once recompiled?

# Leo

Yes, absolutely — and it's already been done for other Xbox titles. The rendering layer in xboxrecomp is a **native D3D11/Vulkan/OpenGL backend** that's completely separate from the game logic, so you can enhance it independently. Here's what's been demonstrated on other ports:

| Feature | Precedent | Effort |
|---------|-----------|--------|
| **4K / resolution scaling** | [Buffy Chaos Bleeds](https://recompendium.com/en/consola/xbox/) runs at 4K; [Burnout 3](https://github.com/antoxa2584x/burnout3-sw) has 1x–2.5x scale | Low — mostly a render target size change |
| **HDR10 output** | [TGP482/BLACK-XBERecomp](https://github.com/TGP482/black-xberecomp), [RDR-XBERecomp](https://github.com/TGP482/RDR-XBERecomp) both ship HDR10 | Medium — tonemapping + output pipeline |
| **HD texture replacement** | [BLACK-2006-Recomp](https://github.com/portingpete/Black-2006-Recomp) (HD pack folder), [XML1-recomp](https://github.com/GTTeancum/XML1-recomp) (PNG swap), [TGP482/BLACK](https://github.com/TGP482/black-xberecomp) (dump + replace) | Low — intercept texture uploads, swap files |
| **DLSS / upscaling** | BLACK-2006-Recomp has DLSS/DLAA | Medium — hook into NVIDIA SDK |
| **Ray tracing** | [papand7/XboxXBERecomp](https://github.com/papand7/XboxXBERecomp) ("Vulkan renderer with ray tracing"), BLACK-2006-Recomp (experimental RT) | High — new RT passes, geometry capture, denoiser |
| **Anisotropic filtering, FXAA, mipmap control** | Burnout 3 has all of these as in-game options | Low |

**For Sudeki specifically**, the practical path would be:

1. **Start with texture replacement** — the easiest win. Dump the textures (the renderer already unswizzles them), upscale with an AI tool, drop replacements in a folder. This alone makes a huge visual difference since the Xbox textures are 256×256.

2. **Resolution + anisotropic filtering** — already in the toolkit's config (`RECOMP_GL_SCALE`, `RECOMP_VK_ANISO`).

3. **HDR** — port the tonemapping/output code from TGP482's BLACK or RDR ports. Sudeki's RenderWare pipeline is simpler than RenderWare 7, so it should be straightforward.

4. **Ray tracing** — this is the hardest. You'd need to intercept geometry draws, build an acceleration structure, and add RT shadow/reflection passes in the Vulkan renderer. papand7's fork is the closest reference. For a game with Sudeki's scale (small levels, few draw calls), it's feasible but a multi-week project.

You don't need to do all of it at once. Texture swap + 4K + anisotropic filtering gets you 80% of the visual uplift with minimal code changes to the renderer.

