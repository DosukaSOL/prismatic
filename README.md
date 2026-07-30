<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# PRISMATIC

A universal, profile-driven **HD-2.5D enhancement platform and emulator frontend**
for pixel-based handheld games (Game Boy / Game Boy Color / Game Boy Advance /
Nintendo DS). PRISMATIC captures the game's own tiles, sprites and palettes at
runtime, reconstructs a lightweight 2.5D scene from them, and relights and
regrades it into a modern "HD-2D"-style presentation — **without inventing,
replacing, or fabricating any game artwork**. Every enhanced pixel derives from
the user's own ROM (or, for development, from a first-party synthetic backend).

First hardware target: **AYN Thor Max** (Snapdragon 8 Gen 2, Adreno 740, dual
AMOLED, Android). First proof-of-concept content: a DS-shaped dual-screen scene.

> **Honesty note.** This repository is an in-progress engineering project, not a
> finished product. See [What actually works](#what-actually-works) and
> [docs/KNOWN_LIMITATIONS.md](docs/KNOWN_LIMITATIONS.md) for an evidence-based
> status. No claim of "complete", "production ready", or "100% compatible" is made.

## What actually works

Verified by execution on the build host (macOS, Apple Silicon):

| Capability | Status | Evidence |
|---|---|---|
| First-party C++20 core (no vendored third-party source) | **Built** | `cmake --build build` |
| Deterministic pipeline: composite → reconstruct → light → post | **Built + tested** | `ctest` 7/7 |
| 10 enhancement presets + profile engine (JSON) | **Built + tested** | `test_profile` |
| Software renderer + debug views (native/depth/normal/id/emissive/light) | **Built** | `local_data/validation_output/*.png` |
| Synthetic DS backend (first-party art, dual screen, sprites, priorities) | **Built + tested** | `test_pipeline` |
| Headless validation runner (10 shots + HTML report) | **Executed** | `report.html` |
| GLSL → SPIR-V shaders (Vulkan target) | **Compiled** | `scripts/test_rendering.sh`, `shader_compile` test |
| Android app (Kotlin + JNI reusing the core, NDK build) | **Built** | `:app:assembleDebug` |
| Installable APK (arm64-v8a) | **Built + packaged** | `app-debug.apk`, SHA-256 in [docs/TEST_REPORT.md](docs/TEST_REPORT.md) |
| On-device run on AYN Thor Max | **Blocked (no device)** | — |
| Real melonDS / mGBA adapter (real ROMs) | **Designed, not built** | requires user ROM/BIOS |

## Repository layout

| Path | Contents |
|---|---|
| [core/](core) | First-party C++20 core: adapter API, materials, scene, lighting, environment, camera, presets, profiles, software renderer, pipeline, plus dependency-free SHA-256/JSON/PNG. |
| [emulation/synthetic/](emulation/synthetic) | First-party synthetic DS backend (no copyrighted assets). |
| [tools/headless-runner/](tools/headless-runner) | Headless validation runner → PNGs + HTML report. |
| [tests/](tests) | Unit + integration tests (own tiny harness). |
| [shaders/](shaders) | GLSL shaders compiled to SPIR-V for the Vulkan path. |
| [android/](android) | Kotlin/NDK Android app that reuses the core through JNI. |
| [docs/](docs) | Architecture, research, build, testing, security, legal, limitations, and the final report. |

## Quick start (desktop core)

```bash
export PATH="$HOME/Library/Android/sdk/cmake/3.22.1/bin:$PATH"   # or any CMake ≥ 3.22
cmake -S . -B build -G "Unix Makefiles"
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/tools/headless-runner/prismatic_headless        # writes local_data/validation_output/
```

Then open `local_data/validation_output/report.html`.

More detail: [docs/BUILDING.md](docs/BUILDING.md).

## Quick start (Android APK)

```bash
cd android
JAVA_HOME="$(/usr/libexec/java_home -v 17)" ./gradlew :app:assembleDebug
# → android/app/build/outputs/apk/debug/app-debug.apk
```

More detail: [docs/ANDROID_BUILD.md](docs/ANDROID_BUILD.md).

## Design principles

- **Adapter-centric.** Enhancement is decoupled from emulation behind a stable
  `EmulatorAdapter` API. Synthetic, mGBA, and melonDS backends are peers.
- **First-party tested core.** No third-party source is vendored into the tested
  core; SHA-256, JSON and PNG are implemented from scratch.
- **Deterministic + testable.** The whole enhancement runs on the CPU so it can
  be unit-tested and compared pixel-exactly to a native ground-truth composite.
- **Art is never invented.** Visuals derive only from tiles/sprites the backend
  exposes. See [docs/SECURITY.md](docs/SECURITY.md) and
  [docs/LEGAL_AND_LICENSING.md](docs/LEGAL_AND_LICENSING.md).

## Licensing

GPL-3.0-or-later. See [LICENSE](LICENSE), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
and [docs/LEGAL_AND_LICENSING.md](docs/LEGAL_AND_LICENSING.md).

## Documentation index

- [docs/ARCHITECTURE_DECISIONS.md](docs/ARCHITECTURE_DECISIONS.md)
- [docs/BUILDING.md](docs/BUILDING.md) · [docs/ANDROID_BUILD.md](docs/ANDROID_BUILD.md)
- [docs/TESTING.md](docs/TESTING.md) · [docs/TEST_REPORT.md](docs/TEST_REPORT.md)
- [docs/SECURITY.md](docs/SECURITY.md) · [docs/LEGAL_AND_LICENSING.md](docs/LEGAL_AND_LICENSING.md)
- [docs/KNOWN_LIMITATIONS.md](docs/KNOWN_LIMITATIONS.md) · [docs/USER_ACTIONS_REQUIRED.md](docs/USER_ACTIONS_REQUIRED.md)
- [docs/FINAL_REPORT.md](docs/FINAL_REPORT.md) — full evidence-based report.
