<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Known limitations

Honest, evidence-based list of what PRISMATIC does **not** do (yet) or cannot be
claimed to do. Nothing here is labeled "complete", "production ready", or "100%
compatible".

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
- **JIT is compiled but disabled at runtime by default** (the interpreter runs
  instead) to avoid Android W^X / executable-memory failures on untested
  devices. Enabling the A64 JIT is a one-line change once basic play is
  confirmed.

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
