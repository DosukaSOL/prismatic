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
> installable Android **APK**, but it does **not yet run any commercial game** —
> the real emulator adapters are designed, not implemented. See
> [What actually works](#-what-actually-works) and
> [Known limitations](docs/KNOWN_LIMITATIONS.md). No claim of "complete",
> "production ready", or "100% compatible" is made anywhere.

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
- **No vendored third‑party source** in the tested core — SHA‑256, JSON and PNG
  are implemented from scratch.
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
