<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# PRISMATIC v0.4.0

**Genuine depth-based 2.5D, a top-screen-only pipeline, a transparent Shader
Studio, full button remapping, and a scan-a-folder games library.** This release
makes the 2.5D real, keeps the DS touch screen honest, and turns the app into
something you point at a folder and play from.

## Highlights

### 🧊 Genuine depth-based 2.5D (DIBR)
When a game renders through the DS 3D engine (HGSS / SoulSilver / Platinum
overworld), Prismatic now reads melonDS's real **per-pixel depth buffer** and
displaces pixels by true scene depth — actual parallax, not a flat perspective
tilt. Where a frame has no 3D depth, it falls back to the honest geometric tilt.

### 🖥️ Top-screen only — the bottom screen stays faithful
The shader grade, the 2.5D depth and FXAA now apply to the **top screen only**.
The DS **bottom screen** (menus, map, text, HUD) is presented untouched, so
tweaking shaders no longer overexposes or smears the touch UI.

### 🫥 Transparent Shader Studio
The editor is now a **translucent drawer** — you keep a live, unobstructed preview
of the running game while you tune every parameter.

### ⬇️ Preset dropdown (your saved looks included)
Presets moved into a single **dropdown menu**. The moment you save a custom look
it appears in that same list, prefixed ★, ready to re-apply.

### 🎨 New reference-grade presets
Added **Octopath**, **Lumen**, and **Diorama**. Diorama is tuned toward the warm,
bright, miniature-model "DramaticShape voxel mod" look and is auto-applied to
HGSS / SoulSilver / Platinum on top of genuine depth.

### ✨ Optional anti-aliasing (FXAA)
A conservative, edge-only FXAA pass you can toggle — on only when it helps, never
softening the whole frame.

### 🎮 Full button mapping
A **Button mapping** menu (open it with **Back**) remaps every AYN Thor button,
sets a dedicated **fast-forward** key, and lets you use L2/R2 (great for macros —
the DS has no back buttons). Speed cycles **1x → 2x → 5x**.

### 📁 Games folder library
Point Prismatic at a **folder of ROMs**; it scans every `.nds`, lists them in the
menu, and labels each one **Compatible** or **Untested** by reading the cartridge
code from the ROM header.

### 📋 Pokémon Platinum added
Platinum (USA `CPUE` / EUR `CPUP` / JPN `CPUJ`) joins the compatible list with the
same genuine depth 2.5D + Diorama profile.

## Honesty notes

- **Genuine depth only exists for 3D-engine pixels.** 2D text, menus, sprites and
  HUD carry no scene depth and ride the base plane. The **bottom screen** is
  deliberately left faithful.
- **The Diorama preset is an analog, not a voxel reconstruction.** It combines
  genuine depth + tilt-shift + depth-of-field to *evoke* the DramaticShape look.
  It does not extrude voxels from map/tile data — that needs a game's decompiled
  assets, not a flat emulator framebuffer.
- **DS Pokémon output is unverified on hardware here.** The maintainer's tooling
  has no commercial DS Pokémon ROM and no device capture, so HGSS / SoulSilver /
  Platinum have not been visually confirmed against the reference on real
  hardware. On-device confirmation is a maintainer device test.
- **"Playable" in the list** means the title boots and runs the path here; it is
  not a hardware-certified full playthrough.
- Prismatic ships **no ROMs or BIOS**. Bring your own dump.

## Install

- `prismatic-0.4.0-arm64-v8a.apk` — arm64-v8a only (AYN Thor Max / Snapdragon 8
  Gen 2). Signed with a debug key for sideloading; uninstall older builds first if
  you hit a signature mismatch.

## Build from source

```bash
cd android && JAVA_HOME="$(/usr/libexec/java_home -v 17)" ./gradlew :app:assembleRelease
```

See [docs/ANDROID_BUILD.md](ANDROID_BUILD.md) for the full toolchain setup.
