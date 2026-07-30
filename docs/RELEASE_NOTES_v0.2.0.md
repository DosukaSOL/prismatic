# PRISMATIC v0.2.0

Faithful by default. Presentation you actually control.

The Nintendo DS path was rebuilt so real ROMs render **accurately and fast**
first, with the 2.5D and shader effects as **independent, opt‑in layers** — the
old baked‑in "smudge" is gone from the real‑ROM path.

## Highlights

- **Faithful rendering by default** — real ROMs show the core's own framebuffer
  (correct colours, native speed). No more luminance‑guess reconstruction.
- **2.5D and shaders are separate layers** — run 2.5D only, shader only, both, or
  neither. They are never fused.
  - *2.5D*: an honest **geometric perspective tilt + tilt‑shift** (tabletop /
    diorama look). It is **not** a true voxel diorama — that needs a game's real
    map/tile data (a decompilation), which can't come from a flat framebuffer.
  - *Shader*: 5 styles (**CRT, LCD, Warm, Night, Vivid**) + day/night grade +
    optional night lantern.
- **Sound** — DS SPU audio decoded and played at 48 kHz stereo.
- **Speed toggle** — *Fast (JIT)* or *Compatible*, per game.
- **On‑device saves + auto‑resume** — battery saves (SRAM) written to a visible
  folder `Android/data/com.prismatic.app/files/saves/`, auto‑loaded on boot. With
  **Auto‑load last game** on, relaunching boots your last ROM.
- **Real emulator UI** — full‑screen game (both DS screens stacked, or bottom on a
  second display). Press **Back** / pad **Mode** for a translucent pause menu.
  No always‑on overlay.

## Honesty

These effects are **stylistic overlays** on the final image. They improve the
look; they do not reconstruct true 3D geometry from a DS framebuffer. Booting a
**commercial** ROM and the audio A/B are **device tests** the maintainer runs on
real hardware — no ROM/BIOS is bundled. See `docs/KNOWN_LIMITATIONS.md`.

## Install

- `prismatic-0.2.0-arm64-v8a.apk` — arm64‑v8a only (AYN Thor Max / Snapdragon 8
  Gen 2 class). Debug‑keystore signed for sideloading.
- sha256: `e887d5c807f41f90f9c80fc4a1d90a2e4e8a4782848f1dc3329be7133eb86f8c`

Load a ROM from the pause menu (**Load ROM…**). Toggle **2.5D depth** and
**Shader overlay** independently. Your progress is kept via in‑game saving.
