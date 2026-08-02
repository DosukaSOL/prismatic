<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<div align="center">

<img src="assets/prismatic-logo.png" alt="Prismatic" width="760">

### A game-aware launcher, emulator and mod platform for classic handhelds

Import your own **Game Boy · Game Boy Color · Game Boy Advance · Nintendo DS**
games, verify them, mod them safely and play them — clean ROMs are never
modified, and no game content is ever bundled or invented.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-7C3AED.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](core)
[![Kotlin](https://img.shields.io/badge/Kotlin-Android-A97BFF.svg)](android)
[![Tests](https://img.shields.io/badge/tests-9%2F9%20passing-brightgreen.svg)](tests)
[![Status](https://img.shields.io/badge/status-pre--alpha-orange.svg)](docs/KNOWN_LIMITATIONS.md)

[**Product model**](docs/PRODUCT_MODEL.md) ·
[**Quick start**](#-quick-start-desktop) ·
[**Android build**](docs/ANDROID_BUILD.md) ·
[**Roadmap**](docs/roadmap/ROADMAP.md)

</div>

---

**PRISMATIC** is a game-focused platform for user-owned classic games: a clean
library and launcher, per-game management pages, hash-verified ROM import,
private mod installations (gen1recomp-style: clean ROM + selected mods → a
generated private build, launched automatically), per-game saves and save
states, camera and performance settings, and an accurate emulator underneath
(melonDS-backed for DS). First supported game family: **Pokémon HeartGold /
SoulSilver**, including the [Visual+ mod](https://github.com/DosukaSOL/pokemon-hgss-visual-mod)
as a one-tap install. Nothing is upscaled by an AI model and no artwork is ever
fabricated — every byte the player sees derives from their own ROM.

Game transformation/reconstruction experiments (2D→2.5D conversion) live in the
separate **[Foldscape](https://github.com/DosukaSOL/Foldscape)** project;
Prismatic can load Foldscape packages but does not implement them. A game-aware
shader/lighting engine is in development on a separate branch.

Primary hardware target: **AYN Thor Max** (Snapdragon 8 Gen 2, dual AMOLED,
Android) with full dual-screen DS routing.

> [!IMPORTANT]
> Pre-alpha engineering project, not a finished product. Every capability
> below states its real verification status. On-device (AYN Thor) checks that
> cannot run in this build environment are labeled **manual required** — never
> claimed as passing.

## How it works

```
your clean ROM  +  selected mods (e.g. Visual+)
        │  import: SHA-256 → identify title/region/revision → verify
        ▼
private installation            ← the clean ROM is NEVER modified
  ├─ source.nds   (pristine copy, hash-verified on demand)
  ├─ builds/      (one generated build per mod profile, SHA-256-pinned)
  ├─ saves/       (ONE battery save shared by all builds — mods never fork saves)
  └─ states/      (machine snapshots, separate from cartridge saves)
        ▼
launch — enabled mods are active automatically
```

Mod packages are fetched from their **canonical repositories** (Visual+ lives at
[DosukaSOL/pokemon-hgss-visual-mod](https://github.com/DosukaSOL/pokemon-hgss-visual-mod)),
hash-verified before and after application by Prismatic's built-in clean-room
VCDIFF/xdelta decoder (RFC 3284, validated byte-identical to xdelta3 on all
eight Visual+ artifacts). See
[docs/VISUAL_PLUS_INTEGRATION.md](docs/VISUAL_PLUS_INTEGRATION.md).

## What works today

Verified by execution on the build host (macOS, Apple Silicon):

| Capability | Status | Evidence |
|---|---|---|
| DS emulation of real commercial dumps (melonDS core, JIT/interpreter) | **Executed** | HGSS boots + runs at ~1.6 ms/frame on host |
| ROM identity: header + SHA-256 + verdicts (Verified/Identified/Modified) | **Executed + tested** | `prism identify`, `test_game_platform` |
| Private installs + hash-pinned mod profile builds (Visual+ 4 variants × HG/SS) | **Executed** | patched builds hash-match the canonical manifest and boot |
| Clean-room VCDIFF decoder (RFC 3284) | **Executed + tested** | byte-identical to xdelta3 on 8/8 real patches |
| Battery saves + full-machine save states (desktop + Android JNI) | **Built**; save-path pinning exercised on host | adapter savestates drive the HGSS dev workflow |
| Game-focused Android UI: library home, HG/SS game pages, mods/saves/states/installation/compatibility/diagnostics | **Built (APK compiles)** | on-device UX check: manual required |
| Android lifecycle: Recents dismissal fix, guaranteed save flush, clean exit | **Built** | [docs/android/RECENTS_AND_EXIT_FIX.md](docs/android/RECENTS_AND_EXIT_FIX.md) — device check manual required |
| AYN Thor lower screen: Prismatic logo on menus, DS touch UI in gameplay | **Built** | [docs/android/LOWER_SCREEN_BEHAVIOR.md](docs/android/LOWER_SCREEN_BEHAVIOR.md) — device check manual required |
| Deterministic enhancement core + presets + profiles | **Built + tested** | `ctest` 9/9 |
| Release safety: no ROM/game data can be tracked or shipped | **Executed** | `scripts/check_no_rom_data.sh` |

## 🚀 Quick start (desktop)

```bash
export PATH="$HOME/Library/Android/sdk/cmake/3.22.1/bin:$PATH"   # or any CMake ≥ 3.22
cmake -S . -B build-melon -DPRISMATIC_ENABLE_MELONDS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-melon -j
ctest --test-dir build-melon --output-on-failure                 # → 9/9 passed

# game platform CLI
./build-melon/prism identify /path/to/your.nds
./build-melon/prism import   /path/to/your.nds
./build-melon/prism profile  heartgold_xxxxxxxx visual-plus     # builds + verifies
```

## 📱 Quick start (Android APK)

```bash
cd android
JAVA_HOME="$(/usr/libexec/java_home -v 17)" ./gradlew :app:assembleRelease
adb install -r app/build/outputs/apk/release/app-release.apk
```

The app reuses the same tested C++ core through JNI. More detail:
[docs/ANDROID_BUILD.md](docs/ANDROID_BUILD.md).

## 📁 Repository layout

| Path | Contents |
|---|---|
| [core/](core) | First-party C++20 core: adapter API, game library, VCDIFF decoder, mod packages, profiles, presets, lighting, pipeline, dependency-free SHA-256 / JSON / PNG. |
| [emulation/melonds/](emulation/melonds) | melonDS-backed DS adapter (framebuffers, audio, saves, save states, scene stream). |
| [emulation/synthetic/](emulation/synthetic) | First-party synthetic backend for CI (no copyrighted assets). |
| [tools/prism-cli/](tools/prism-cli) | Game platform CLI (identify / import / mods / profile / apply). |
| [tools/nds-pilot/](tools/nds-pilot) | Scriptable ROM driver for development (savestates, RAM probes). |
| [android/](android) | Kotlin/NDK app: game library UI, game pages, dual-screen routing. |
| [compatibility/](compatibility) | ROM identity + compatibility databases (metadata only). |
| [patches/](patches) | Local melonDS patch set (depth + layer-mask exposure). |
| [docs/](docs) | Product model, integration, Android lifecycle, roadmap, legal. |

## 📚 Documentation

| | |
|---|---|
| [Product model](docs/PRODUCT_MODEL.md) | [Roadmap](docs/roadmap/ROADMAP.md) |
| [Visual+ integration](docs/VISUAL_PLUS_INTEGRATION.md) | [Compatibility](docs/COMPATIBILITY.md) |
| [Android build](docs/ANDROID_BUILD.md) | [Building (desktop)](docs/BUILDING.md) |
| [Recents & exit fix](docs/android/RECENTS_AND_EXIT_FIX.md) | [Lower screen behaviour](docs/android/LOWER_SCREEN_BEHAVIOR.md) |
| [Known limitations](docs/KNOWN_LIMITATIONS.md) | [Legal & licensing](docs/LEGAL_AND_LICENSING.md) |

## ⚖️ Licensing

**GPL-3.0-or-later** (see [LICENSE](LICENSE)) — required by the melonDS
(GPL-3.0) integration. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[docs/LEGAL_AND_LICENSING.md](docs/LEGAL_AND_LICENSING.md).

**PRISMATIC ships no ROMs, BIOS, or game artwork, and never will.** Game data is
only ever read at runtime from a ROM the user legally owns; mod packages contain
patches and metadata, never game content. Trademarks (Pokémon, Nintendo, Game
Boy, AYN, Thor, Snapdragon, Adreno, …) belong to their respective owners;
references here are descriptive only and imply no affiliation.

<div align="center">
<sub>Built with a first-party, dependency-free core · Honest status, evidence-based docs</sub>
</div>
