<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<div align="center">

<img src="assets/prismatic-logo.png" alt="Prismatic" width="760">

### HD‑2.5D enhancement platform & emulator frontend for pixel handhelds

Relight and reimagine **Game Boy · Game Boy Color · Game Boy Advance · Nintendo DS**
games in a modern "HD‑2D" style — using **only the game's own pixels**, never
invented or fabricated art.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-7C3AED.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](core)
[![Kotlin](https://img.shields.io/badge/Kotlin-Android-A97BFF.svg)](android)
[![Vulkan](https://img.shields.io/badge/Vulkan-SPIR--V-AC162C.svg)](shaders)
[![Tests](https://img.shields.io/badge/tests-7%2F7%20passing-brightgreen.svg)](docs/TEST_REPORT.md)
[![Status](https://img.shields.io/badge/status-pre--alpha-orange.svg)](docs/KNOWN_LIMITATIONS.md)

[**Getting started**](#-quick-start-desktop-core) ·
[**Android build**](docs/ANDROID_BUILD.md) ·
[**Architecture**](docs/ARCHITECTURE_DECISIONS.md) ·
[**Final report**](docs/FINAL_REPORT.md)

</div>

---

**PRISMATIC** captures a game's own tiles, sprites and palettes at runtime,
reconstructs a lightweight 2.5D scene from them, and relights and regrades it into
a modern, cinematic presentation. Nothing is upscaled by an AI model and no
artwork is ever fabricated or replaced — every enhanced pixel derives from the
user's own ROM (or, for development, from a first‑party synthetic backend).

First hardware target: **AYN Thor Max** (Snapdragon 8 Gen 2, Adreno 740, dual
AMOLED, Android). First proof‑of‑concept: a DS‑shaped dual‑screen scene.

> [!IMPORTANT]
> This is an **in‑progress engineering project (pre‑alpha)**, not a finished
> product. It ships a genuinely working, tested enhancement **core** and an
> installable Android **APK**. A **real Nintendo DS backend (melonDS)** is now
> integrated end‑to‑end: it compiles, links, and **boots + runs** on the desktop
> host and inside the arm64 Android `.so` (verified with a first‑party test ROM).
> Booting a **commercial** ROM has **not** been verified on hardware yet — that
> gate is a device test the maintainer will run on the Thor. The GBA (mGBA)
> adapter is still designed, not implemented. See
> [What actually works](#-what-actually-works) and
> [Known limitations](docs/KNOWN_LIMITATIONS.md). No claim of "complete",
> "production ready", or "100% compatible" is made anywhere.

## 🆕 v0.2.0 — faithful by default, layers you control

The DS path was rebuilt to be **accurate and fast first**, with presentation as
**independent, opt‑in layers** — no more baked‑in "smudge".

- **Faithful rendering by default.** Real ROMs now display the emulator's own
  framebuffer directly (correct colours, native speed). The old luminance‑guess
  reconstruction that stretched/smeared DS frames is **no longer on the real‑ROM
  path**.
- **2.5D and shaders are separate layers.** Each toggles on its own — you can run
  **2.5D only**, **shader only**, **both**, or **neither**. They are never fused.
  - *2.5D* is an honest **geometric perspective tilt + tilt‑shift** (a tabletop /
    diorama look). It is **not** a true Octopath‑style voxel diorama: that needs a
    game's actual map/tile/entity data (a decompilation), which cannot be
    recovered from a flat emulator framebuffer. We do not pretend otherwise.
  - *Shader* is a tasteful post overlay with 5 styles: **CRT, LCD, Warm, Night,
    Vivid**, plus a day/night grade and an optional night **lantern**.
- **Sound.** DS audio is decoded from the core's SPU and played at 48 kHz stereo.
- **Speed toggle.** *Fast (JIT)* or *Compatible* — pick per game.
- **On‑device saves + auto‑resume.** Battery saves (SRAM) are written to a visible
  folder — `Android/data/com.prismatic.app/files/saves/` — and auto‑loaded when a
  ROM boots. With **Auto‑load last game** on, relaunching the app boots your last
  ROM so you continue where you left off.
- **Real emulator UI.** Full‑screen game (both DS screens stacked, or the bottom
  routed to a second display). Press **Back** (or the pad's **Mode**) for a
  translucent **pause menu** — there is no always‑on overlay anymore.

> Honesty note: these presentation effects are stylistic overlays applied to the
> final image. They enhance the look; they do not reconstruct true 3D geometry
> from a DS framebuffer.

## ✨ Gallery

All frames below are produced by the deterministic pipeline from the **first‑party
synthetic DS backend** (no copyrighted assets). Left: the native ground‑truth
composite. Right: the same frame after PRISMATIC enhancement (3× upscale, relight,
contact shadows, grade).

<div align="center">

| Native (ground truth) | Enhanced (PRISMATIC) |
|:---:|:---:|
| <img src="assets/showcase-native.png" width="380"> | <img src="assets/showcase-enhanced.png" width="380"> |
| **Night + lantern point light** | **Reconstructed normals (debug view)** |
| <img src="assets/showcase-night.png" width="380"> | <img src="assets/showcase-normal.png" width="380"> |

</div>

## 🎯 Highlights

- **Art is never invented.** Enhancement is *relighting and regrading* of the
  game's own tiles/sprites — there is no image‑generation model in the codebase.
- **Deterministic, testable core.** The whole pipeline runs on the CPU and is
  compared pixel‑exactly to a native ground‑truth compositor (**7/7 tests pass**).
- **Adapter‑centric.** Emulation is decoupled from enhancement behind a stable
  `EmulatorAdapter` API; synthetic, mGBA and melonDS backends are peers.
- **Real DS core.** The melonDS backend feeds true 256×192 framebuffers through
  the enhancement pipeline; runs the interpreter by default (JIT compiled in).
- **No vendored third‑party source** in the tested enhancement core — SHA‑256,
  JSON and PNG are implemented from scratch. melonDS is a pinned **submodule**,
  built only for the DS backend (`git submodule update --init --recursive`).
- **10 tunable presets** + a JSON **profile engine** with per‑tile/tileset/map
  material overrides and copyright‑safe export.
- **Vulkan‑ready.** GLSL shaders compile to SPIR‑V for the on‑device Adreno path.

## ✅ What actually works

Verified by execution on the build host (macOS, Apple Silicon):

| Capability | Status | Evidence |
|---|---|---|
| First‑party C++20 core (no vendored third‑party source) | **Built** | `cmake --build build` |
| Deterministic pipeline: composite → reconstruct → light → post | **Built + tested** | `ctest` 7/7 |
| 10 enhancement presets + profile engine (JSON) | **Built + tested** | `test_profile` |
| Software renderer + debug views (native/depth/normal/id/emissive/light) | **Built** | `local_data/validation_output/*.png` |
| Synthetic DS backend (first‑party art, dual screen, sprites, priorities) | **Built + tested** | `test_pipeline` |
| Headless validation runner (10 shots + HTML report) | **Executed** | `report.html` |
| GLSL → SPIR‑V shaders (Vulkan target) | **Compiled** | `scripts/test_rendering.sh`, `shader_compile` |
| Android app (Kotlin + JNI reusing the core, NDK build) | **Built** | `:app:assembleDebug` |
| Installable APK (arm64‑v8a) | **Built + packaged** | `app-debug.apk` — [checksum](docs/TEST_REPORT.md) |
| On‑device run on AYN Thor Max | **Blocked (no device)** | — |
| Real melonDS / mGBA adapter (real ROMs) | **Designed, not built** | requires user ROM/BIOS |

Full labeled results: [docs/TEST_REPORT.md](docs/TEST_REPORT.md).

## 🚀 Quick start (desktop core)

```bash
export PATH="$HOME/Library/Android/sdk/cmake/3.22.1/bin:$PATH"   # or any CMake ≥ 3.22
cmake -S . -B build -G "Unix Makefiles"
cmake --build build -j
ctest --test-dir build --output-on-failure                       # → 7/7 passed
./build/tools/headless-runner/prismatic_headless                 # writes local_data/validation_output/
```

Then open `local_data/validation_output/report.html`. More detail:
[docs/BUILDING.md](docs/BUILDING.md).

## 📱 Quick start (Android APK)

```bash
cd android
JAVA_HOME="$(/usr/libexec/java_home -v 17)" ./gradlew :app:assembleDebug
# → android/app/build/outputs/apk/debug/app-debug.apk  (arm64-v8a)
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.prismatic.app/.MainActivity
```

The app's native library **reuses the exact same tested core** through JNI, so the
on‑device image is produced by the identical, already‑validated pipeline. More
detail: [docs/ANDROID_BUILD.md](docs/ANDROID_BUILD.md).

## 🧩 How it works

```
Backend (synthetic | mGBA* | melonDS*)          *designed, not yet implemented
        │  StructuredFrame  (tiles / sprites / palettes / priority / scroll)
        ▼
compositeNative ─► reconstructScene ─► shadeScene ─► post-process ─► upscale
  (ground truth)    (albedo, height,     (ambient +     (bloom, ACES filmic,
                     normal, depth,        key + point     contrast / saturation
                     emissive, id,         + rim, contact   / grade, fog,
                     material)             shadow / AO)     vignette)
```

Conventions PRISMATIC‑wide: priority `0` = backmost, larger = front; within a
priority level backgrounds draw before sprites (sprites win ties); palette index
`0` = transparent.

## 📁 Repository layout

| Path | Contents |
|---|---|
| [core/](core) | First‑party C++20 core: adapter API, materials, scene, lighting, environment, camera, presets, profiles, software renderer, pipeline, plus dependency‑free SHA‑256 / JSON / PNG. |
| [emulation/synthetic/](emulation/synthetic) | First‑party synthetic DS backend (no copyrighted assets). |
| [tools/headless-runner/](tools/headless-runner) | Headless validation runner → PNGs + HTML report. |
| [tests/](tests) | Unit + integration tests (own tiny harness). |
| [shaders/](shaders) | GLSL shaders compiled to SPIR‑V for the Vulkan path. |
| [android/](android) | Kotlin/NDK Android app that reuses the core through JNI. |
| [docs/](docs) | Architecture, research, build, testing, security, legal, limitations, and the final report. |

## 📚 Documentation

| | |
|---|---|
| [Building (desktop)](docs/BUILDING.md) | [Android build](docs/ANDROID_BUILD.md) |
| [Testing](docs/TESTING.md) | [Test report](docs/TEST_REPORT.md) |
| [Architecture decisions](docs/ARCHITECTURE_DECISIONS.md) | [Final report](docs/FINAL_REPORT.md) |
| [Security & content integrity](docs/SECURITY.md) | [Legal & licensing](docs/LEGAL_AND_LICENSING.md) |
| [Known limitations](docs/KNOWN_LIMITATIONS.md) | [User actions required](docs/USER_ACTIONS_REQUIRED.md) |

## ⚖️ Licensing

**GPL‑3.0‑or‑later** (see [LICENSE](LICENSE)). This is required by the intended
melonDS (GPL‑3.0) integration; mGBA (MPL‑2.0) and Dear ImGui (MIT) are compatible.
The code as built vendors **no** third‑party source. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[docs/LEGAL_AND_LICENSING.md](docs/LEGAL_AND_LICENSING.md).

**PRISMATIC ships no ROMs, BIOS, or game artwork, and never will.** Real game
graphics are only ever read at runtime from a ROM the user legally owns. Trademarks
(Pokémon, Nintendo, Game Boy, AYN, Thor, Snapdragon, Adreno, …) belong to their
respective owners; references here are descriptive only and imply no affiliation.

<div align="center">
<sub>Built with a first‑party, dependency‑free core · Honest status, evidence‑based docs</sub>
</div>
