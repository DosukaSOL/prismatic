<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# PRISMATIC v0.3.0

**Shader Studio, a real branded menu, a compatibility list, and a proper
close-game flow.** This release makes the presentation layer something you fully
control and gives the app a front-end that feels like a real emulator.

## Highlights

### 🎛️ Shader Studio — make your own look
A live, on-device shader editor (like the filter panels in desktop melonDS), with
**13 tweakable parameters** plus the two independent layers:

- Brightness · Exposure · Contrast · Saturation · Temperature · Tint · Gamma
- Vignette · Bloom · Bloom threshold · Scanline · LCD grid · Sharpen
- **2.5D depth tilt** and **Lantern glow** toggles + depth-tilt slider

Drag a slider and the running game updates **instantly**. It's a right-side drawer
so you keep a live preview of the game while you tune.

### 💾 Save & re-apply your looks
Craft a look, **name it, save it**, and load it again after rebooting the game.
Saved looks live in `files/shaders/` on the device.

### 🎨 Professional preset pack
Five hand-tuned presets that look like they belong: **HD-2D, CRT, LCD, Night,
Vivid**. The HD-2D preset goes for that warm, softly-bloomed Octopath mood.

### 🏠 Branded home screen
The app now opens on a real menu — Prismatic logo, brand colours, animated cards:
**Open Game · Compatible Games · Shader Studio · Speed · Quit**.

### ⏹️ Close the game properly
The pause menu adds **Save & Close** and **Close (no save)**, both returning you
to the home screen. **Back on the home screen exits Prismatic.** (Previously there
was no way to fully close a game.)

### 🎮 Correct button mapping
The pad's **A/B/X/Y now map directly to the DS's A/B/X/Y** — what you press is
what the game sees. (D-pad, L/R, Start/Select unchanged.)

### 📋 Compatibility list
A built-in **Compatible Games** view (and
[docs/COMPATIBILITY.md](COMPATIBILITY.md)) lists verified titles, starting with
**Pokémon HeartGold & SoulSilver** (USA/EUR/JPN). Loading a listed game
auto-applies its recommended HD-2D profile — unless you have your own saved look
active.

## Honesty notes

- The **2.5D depth is still a geometric tilt**, not a true per-object diorama. A
  real diorama needs a game's map/entity data (a decompilation) or the emulator's
  3D **depth buffer**. The depth-buffer path is now **documented** as concrete
  future work (`GPU3D_Soft.h:476`) but is **not shipped** — it needs on-hardware
  tuning first.
- Presets and Shader Studio are a **screen-space post pipeline**: they re-grade
  the finished image, they don't change emulation.
- **"Playable" in the list** means the title boots and runs the HD-2D path here;
  it is not a hardware-certified full playthrough. Commercial-ROM verification
  remains a maintainer device test on the AYN Thor.
- Prismatic ships **no ROMs or BIOS**. Bring your own dump.

## Install

- `prismatic-0.3.0-arm64-v8a.apk` — arm64-v8a only (AYN Thor Max / Snapdragon 8
  Gen 2). Signed with a debug key for sideloading; uninstall older builds first if
  you hit a signature mismatch.

## Build from source

```bash
cd android
JAVA_HOME="$(/usr/libexec/java_home -v 17)" ./gradlew :app:assembleRelease
```

See [docs/ANDROID_BUILD.md](ANDROID_BUILD.md).
