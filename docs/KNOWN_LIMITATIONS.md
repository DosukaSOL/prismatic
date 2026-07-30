<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Known limitations

Honest, evidence-based list of what PRISMATIC does **not** do (yet) or cannot be
claimed to do. Nothing here is labeled "complete", "production ready", or "100%
compatible".

## v0.3.0 presentation — Shader Studio, presets, and the depth ceiling

- **Shader Studio is real and live.** The 13 shader parameters, the 2.5D depth
  tilt, and the lantern are all user-tweakable on-device, apply instantly to the
  running frame, and can be saved/loaded as named looks. This is a **screen-space
  post pipeline** — it re-grades the finished image; it does not change emulation.
- **The five presets (HD-2D/CRT/LCD/Night/Vivid) are tuning, not new geometry.**
  The "HD-2D" preset is a warm, softly-bloomed grade tuned to *evoke* an
  Octopath-like mood; it is **not** an Octopath renderer.
- **The 2.5D depth is still geometric (a tilt), not a per-object diorama.** See
  the boundary below — this did not change in v0.3.0, and the depth-aware path is
  documented but **not shipped**.
- **A concrete path to *real* per-pixel depth exists but is unverified.** melonDS's
  software rasteriser keeps a real per-pixel **depth buffer** for 3D-engine pixels
  (`third_party/melonDS/src/GPU3D_Soft.h:476`, `u32 DepthBuffer[...]`, written at
  `GPU3D_Soft.cpp:598`). Exposing it would let the 2.5D layer displace pixels by
  true scene depth for HGSS's 3D overworld. It is **not** implemented because it
  requires patching melonDS across `Renderer3D`/`GPU3D`/`GPU` to surface the
  buffer, only covers 3D-engine pixels (not 2D text/menus), and needs on-hardware
  tuning the maintainer hasn't done yet. Shipping it unverified would violate the
  honesty rule, so it stays a documented future step.
- **Per-game auto-profiles are metadata, not a compatibility guarantee.** The
  compatibility registry (`assets/games.json`) maps a cartridge code to a
  recommended look; it does **not** assert a title has been fully played through
  on hardware here.

## v0.2.0 presentation — what the "2.5D" is and is not

- **The real-ROM "2.5D" is a geometric perspective tilt + tilt-shift, not a true
  diorama.** It resamples the finished frame onto a receding plane and softens
  the near/far bands (a tabletop / miniature look). It does **not** rebuild real
  3D geometry, per-tile depth, sprite slabs, or a shadow map. A genuine
  Octopath-style voxel diorama requires a game's actual **map/tile/entity data**
  (i.e. a decompilation such as pret/pokered), which **cannot** be recovered from
  a flat emulator framebuffer. This is a hard, honest boundary.
- **Shaders are a stylistic post overlay**, not colour-accurate emulation. Styles
  (CRT/LCD/Warm/Night/Vivid), the day/night grade, and the lantern are opt-in and
  do not change gameplay.
- **2.5D and shaders are independent layers** (either / both / neither). The old
  fused "smudge" path is not used for real ROMs.
- **Audio** is decoded from the melonDS SPU at 48 kHz stereo. It has **not** been
  A/B'd against hardware on a commercial title here (device-blocked).
- **Saves are the game's own battery save (SRAM)**, written to
  `Android/data/com.prismatic.app/files/saves/` and auto-loaded on boot.
  **Save-states (resume at an exact frame) are not implemented** — "continue where
  you left off" relies on in-game saving plus optional auto-loading of the last
  ROM.

## Emulation

- **The DS backend (melonDS) is real and running, but not hardware-proven on a
  commercial ROM.** melonDS is integrated end-to-end behind `EmulatorAdapter`:
  it compiles, links, and **boots + runs frames** on the desktop host and inside
  the arm64 Android `.so`. This was verified with a **first-party test ROM**
  (a minimal spin-loop `.nds`), whose true 256×192 framebuffers flow through the
  enhancement pipeline. Booting a **commercial** game (e.g. Pokémon SoulSilver)
  has **not** been run — that is a **device test the maintainer performs on the
  Thor with their own ROM**. No ROM/BIOS is bundled.
- **mGBA (GB/GBC/GBA) is still designed, not implemented.**
- **SoulSilver compatibility is unproven.** The core can now technically load and
  run DS ROMs, but no commercial title has been executed here, so no
  compatibility claim is made. This is **hardware/ROM-blocked**, by design.
- **JIT is user-selectable at runtime** via the pause menu (*Speed: Fast (JIT)*
  vs *Compatible*). Fast/JIT is the default on Android arm64; switch to
  Compatible (interpreter) if a game misbehaves. The setting applies on the next
  ROM load/reset. JIT is not compiled on the desktop host build.

## Rendering

- The **software renderer** is the reference path and is fully exercised. The
  **Vulkan/GPU path** is represented by real GLSL shaders that **compile to
  SPIR-V**, but no runtime Vulkan renderer draws with them on a device GPU in this
  environment (no desktop Vulkan ICD; Android GPU not run here). GPU output is
  therefore **not pixel-verified**.
- Scene reconstruction infers height/normals/depth heuristically from 2D tiles.
  It is deliberately conservative (gameplay-safe camera clamp), but per-game
  tuning via profiles will be needed for best results on real content.

## Android app

- The APK **builds and packages** (arm64-v8a) but has **not been run on an AYN
  Thor Max or any physical device/emulator** here. Startup, the render loop, touch
  input, and especially **dual-display routing** are **not device-verified**.
- The on-device renderer currently presents frames produced by the CPU pipeline
  via a `SurfaceView`/`Canvas` blit (guaranteed to work across GPUs). A native
  **Vulkan** presenter for the Adreno 740 is **designed, not implemented**.
- Only `arm64-v8a` is built. No app icon densities beyond a single vector; no
  localization; no settings persistence.

## Platform / environment

- Built and tested only on macOS (Apple Silicon). Not yet built on Linux/Windows
  (expected to work — no OS-specific code in the core — but unverified).
- Host has only the `android-36` SDK platform and no `cmdline-tools`, so no AVD
  system image is installed; that is why no emulator run was attempted.

## Scope not yet started

- Audio, save-state UI, netplay, real-ROM identification/fingerprinting, and the
  on-device profile editor UI are **not implemented**.

See [USER_ACTIONS_REQUIRED.md](USER_ACTIONS_REQUIRED.md) for how to move past the
blocked items, and [ROADMAP-style next steps in FINAL_REPORT.md](FINAL_REPORT.md).
