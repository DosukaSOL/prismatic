<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# PRISMATIC — Final Report

Evidence-based engineering report. Commit `ccab679`. Host: macOS (Apple Silicon),
AppleClang 21, CMake 3.22.1, NDK 27.1.12297006, JDK 17, Gradle 8.9 / AGP 8.7.2.

Labels used throughout: **Passed-executed**, **Blocked**, **Designed-not-executed**,
**Skipped**. No claim of "complete", "production ready", "universal", "fully
working", "bug-free", or "100% compatible" is made anywhere in this project.

---

## 1. Executive summary

PRISMATIC is a profile-driven HD-2.5D enhancement platform and emulator frontend
for pixel-based handhelds (GB/GBC/GBA/DS), targeting the AYN Thor Max first. This
session delivered a **genuinely working, tested vertical slice**:

- A first-party C++20 **enhancement core** with **no vendored third-party source**
  (own SHA-256, JSON, PNG). It composites a native ground-truth frame, reconstructs
  a lightweight 2.5D scene from the game's own tiles/sprites, relights and regrades
  it, and upscales it. **Built + 7/7 tests pass.**
- A first-party **synthetic DS backend** (dual 256×192, tilemaps, OAM sprites,
  priorities, scrolling, trailing follower, touch UI) so the whole pipeline runs
  deterministically **without any ROM**. **Built + tested.**
- A **headless runner** that produced 10 native-vs-enhanced PNGs + an HTML report,
  **visually confirming real enhancement** (upscale, contact shadows, sprite
  relief, day/night, grading, lantern point light). **Executed.**
- Real **GLSL shaders compiled to SPIR-V** via the NDK `glslc`. **Compiled.**
- A **Kotlin/NDK Android app** whose native library **reuses the exact tested
  core** through JNI, producing an **installable arm64-v8a APK**. **Built +
  packaged** (not yet run on hardware).

What is **not** done, stated plainly: no real emulator core (melonDS/mGBA) is
integrated, so **no commercial game — including Pokémon SoulSilver — runs yet**;
the APK is **not device-verified**; the Vulkan runtime path is designed, not
implemented. See §12 and §16.

## 2. Objectives vs. outcome

| Objective (brief) | Outcome |
|---|---|
| Research, design, implement, compile, run, test, debug, optimize, document | Done for the core + synthetic slice + Android build; documented here. |
| HD-2.5D enhancement that never invents art | Enforced: enhancement only relights the backend's own pixels; no image generation exists in the code. |
| Optimize first for AYN Thor Max (arm64/Adreno) | APK is arm64-v8a; Vulkan shaders authored; **on-device run Blocked (no hardware).** |
| Proof-of-concept: Pokémon SoulSilver (DS) | **Blocked/Deferred** — needs a real DS adapter + user ROM (never bundled). Synthetic DS stands in. |
| Do not stop at plan/scaffold/pseudocode/nonfunctional UI | Went through implementation → compilation → execution → tests → APK. |

## 3. Research summary

Full notes in [research/](research). Key conclusions that shaped the design:

- **Emulator cores** ([research/EMULATOR_RENDERING_REFERENCES.md](research/EMULATOR_RENDERING_REFERENCES.md),
  [research/RELATED_PROJECTS.md](research/RELATED_PROJECTS.md)): **mGBA** (MPL-2.0)
  for GB/GBC/GBA and **melonDS** (GPL-3.0) for DS are the practical cores. melonDS'
  GPL-3.0 forces the whole frontend to GPL-3.0-or-later (§5).
- **DS graphics model** ([research/POKEMON_DS_REVERSE_ENGINEERING.md](research/POKEMON_DS_REVERSE_ENGINEERING.md)):
  tile/OAM/priority/window/blend model informed the `StructuredFrame` capture
  format and the native compositor.
- **HD-2D technique** ([research/SHADER_REFERENCE_CATALOG.md](research/SHADER_REFERENCE_CATALOG.md)):
  relight + bloom + filmic tonemap + depth-of-field-ish grade over 2D art; drove
  the preset and lighting design.
- **Adreno/Vulkan on mobile** ([research/MOBILE_VULKAN_RESEARCH.md](research/MOBILE_VULKAN_RESEARCH.md))
  and **dual display** ([research/ANDROID_DUAL_DISPLAY_RESEARCH.md](research/ANDROID_DUAL_DISPLAY_RESEARCH.md)):
  `DisplayManager` + `Presentation` for the second panel; Vulkan for the render
  path on the Thor.
- **AYN Thor** ([research/AYN_THOR_RESEARCH.md](research/AYN_THOR_RESEARCH.md)):
  SD 8 Gen 2 / Adreno 740, dual AMOLED, Android — arm64 target, dual-display design.

## 4. gen1recomp findings

Analyzed in [research/GEN1RECOMP_ANALYSIS.md](research/GEN1RECOMP_ANALYSIS.md).
gen1recomp (MIT) is **not** an emulator or a transpiler; it is a hand-written Lua
engine re-creation on LÖVE that **extracts graphics/data from the user's own
verified ROM at runtime into a private cache**, and offers optional 2.5D/voxel
presentation mods. Takeaways adopted by PRISMATIC (ideas, **not** code):

1. **Runtime asset derivation + private cache** is the correct copyright-safe
   pattern — verify identity by hash, extract at runtime, never ship or transmit
   assets. PRISMATIC's profile/Fidelity-Lock/cache design mirrors this.
2. **Hash-gated identity** (SHA-1/SHA-256 whitelist) before any game-specific
   behavior — PRISMATIC keys profiles/caches on a ROM SHA-256 and strips it on
   copyright-safe export.
3. **Behavior decoupled from data** — PRISMATIC separates *emulation correctness*
   (the adapter) from *enhancement* (the profile/preset), just as gen1recomp
   separates rulesets from extracted assets.

Crucially, gen1recomp is **Gen-1-specific and not a DS solution**; it validates
the *policy*, not a path to running SoulSilver. Static recompilation of arbitrary
DS ROMs is not viable; **emulation via melonDS remains the route** for DS.

## 5. Licensing

**GPL-3.0-or-later** ([LICENSE](../LICENSE)). melonDS (GPL-3.0) forces it; mGBA
(MPL-2.0) and ImGui (MIT) are compatible. The **code as built vendors no
third-party source** — the tested core is entirely first-party — so the license is
adopted up-front to avoid relicensing when the GPL/MPL backends are added later.
Details: [LEGAL_AND_LICENSING.md](LEGAL_AND_LICENSING.md),
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md),
[research/LICENSING_MATRIX.md](research/LICENSING_MATRIX.md).

## 6. Architecture overview

Adapter-centric. A backend implements `EmulatorAdapter` and exposes a
`StructuredFrame` (backgrounds, sprites, palettes, priorities, scroll, blend,
optional NDS-3D layer). The core turns that into an enhanced image:

```
Backend (synthetic | mGBA* | melonDS*)
        │  StructuredFrame (tiles/sprites/palettes/priority)
        ▼
compositeNative ──► reconstructScene ──► shadeScene ──► post-process ──► upscale
  (ground truth)     (albedo/height/       (ambient+key+     (bloom, ACES,
                      normal/depth/          point+rim,        contrast/sat/
                      emissive/id/material)  contact AO)       grade, fog, vignette)
```

Conventions (PRISMATIC-wide): priority 0 = backmost, larger = front; within a
priority level backgrounds draw before sprites (sprites win ties); palette index 0
= transparent. See [ARCHITECTURE_DECISIONS.md](ARCHITECTURE_DECISIONS.md) (ADR-1..10).

## 7. Component inventory (built this session)

| Area | Files | Status |
|---|---|---|
| Core primitives | `core/include/prismatic/{types,hash,json,png}.hpp` | Built + tested (hash/json/png) |
| Adapter API | `core/include/prismatic/adapter.hpp` | Built |
| Compositor | `core/{include/prismatic/composite.hpp,src/composite.cpp}` | Built + tested via pipeline |
| Materials | `core/…/materials.{hpp,cpp}` | Built + tested |
| Scene reconstruction | `core/…/scene.{hpp,cpp}` | Built + tested via pipeline |
| Environment / time-of-day | `core/…/environment.{hpp,cpp}` | Built + tested |
| Lighting | `core/…/lighting.{hpp,cpp}` | Built |
| Camera (parallax, gameplay-safe) | `core/…/camera.{hpp,cpp}` | Built + tested |
| Presets ×10 | `core/…/presets.{hpp,cpp}` | Built + tested |
| Profile engine (JSON, precedence, copyright-safe) | `core/…/profile.{hpp,cpp}` | Built + tested |
| Software renderer + debug views | `core/…/renderer_software.{hpp,cpp}` | Built |
| Pipeline facade | `core/…/pipeline.{hpp,cpp}` | Built |
| Synthetic DS backend | `emulation/synthetic/*` | Built + tested |
| Headless runner | `tools/headless-runner/main.cpp` | Executed |
| Shaders | `shaders/{fullscreen.vert,enhance.frag,bloom.frag}` | Compiled to SPIR-V |
| Android app | `android/app/src/main/{java,cpp}/…` | Built into APK |
| Tests | `tests/**` | 7/7 Passed-executed |

## 8. What was actually implemented and tested

**Passed-executed:** the full CPU enhancement pipeline, 10 presets, the profile
engine (JSON round-trip, rule precedence, copyright-safe stripping, validation,
migration), material classification + cache, environment day/night, gameplay-safe
camera, SHA-256/JSON/PNG, the synthetic DS backend, determinism, the headless
runner (10 PNGs + report), the shader→SPIR-V compile, and the Android APK build.

Full labeled matrix: [TEST_REPORT.md](TEST_REPORT.md).

## 9. Build & test results

- Desktop: `cmake --build build` compiles clean (`-Wall -Wextra`). **Passed-executed.**
- Tests: `100% tests passed, 0 tests failed out of 7` (0.23 s). **Passed-executed.**
- Shaders: 3/3 GLSL → SPIR-V. **Passed-executed.**
- Android: `:app:assembleDebug` → `BUILD SUCCESSFUL in 44s`, 40 tasks, incl.
  `buildCMakeDebug[arm64-v8a]`. **Passed-executed.**

## 10. Rendering evidence

`local_data/validation_output/` (from `prismatic_headless`): for each of 10
scenarios, a native composite PNG and an enhanced PNG, plus `report.html`. Visual
inspection confirms genuine enhancement vs. native:

- `hero_native.png` vs `hero_enhanced.png` — 3× upscale, contact shadows under
  canopy, sprite relief, grade.
- `night_glow_enhanced.png` — night ambient + warm **point light (lantern)** with
  attenuation lighting the player and follower.
- `hero_normal.png` — reconstructed normal-map debug view (extruded walls, sprite
  relief).

Observed software render time (256×192): ~34 ms first frame, ~16–20 ms after.

## 11. APK location, checksum & contents

- Path: `android/app/build/outputs/apk/debug/app-debug.apk`
- Size: **3,971,111 bytes** (3.97 MB)
- **SHA-256: `798fcb7096e4e363508ca05eb88f3fc51c08cf60acce17dd63d0352841241706`**
- Contents: `classes.dex` (2,308,976 B), `lib/arm64-v8a/libprismaticnative.so`
  (1,609,944 B, ELF AArch64), `AndroidManifest.xml`.
- Badging: package `com.prismatic.app` v0.1.0, launchable `.MainActivity`,
  native-code `arm64-v8a`, minSdk 30 / targetSdk 34 / compileSdk 36.

> The checksum is specific to this build; rebuilding may differ due to timestamps.

## 12. Per-system status

| System | Enhancement pipeline | Real emulation | Runs a commercial game? |
|---|---|---|---|
| Synthetic DS fixture | **Working (tested)** | n/a (first-party) | Shows first-party demo scene |
| Nintendo DS (melonDS) | Designed to consume its frames | **Designed-not-executed** | **No** (adapter not built; ROM-blocked) |
| GBA (mGBA) | Designed to consume its frames | **Designed-not-executed** | **No** |
| GB / GBC (mGBA) | Designed to consume its frames | **Designed-not-executed** | **No** |
| **Pokémon SoulSilver (DS)** | Would use the DS path | **Blocked** | **No — unproven, ROM-blocked by design** |

## 13. Install & usage

```bash
# Desktop core + validation
export PATH="$HOME/Library/Android/sdk/cmake/3.22.1/bin:$PATH"
cmake -S . -B build -G "Unix Makefiles" && cmake --build build -j
ctest --test-dir build --output-on-failure
./build/tools/headless-runner/prismatic_headless   # → local_data/validation_output/report.html

# Android APK
cd android && JAVA_HOME="$(/usr/libexec/java_home -v 17)" ./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.prismatic.app/.MainActivity
```

In-app: tap left/right of the top screen to change preset; use the Preset/Time/
Weather/Lantern buttons; touch the bottom screen for DS touch input. Dual displays
route the bottom screen to the second panel. See [ANDROID_BUILD.md](ANDROID_BUILD.md).

## 14. Performance notes

- Software renderer at native DS resolution (256×192) is ~16–20 ms/frame steady on
  the host CPU — adequate for validation, not the on-device target.
- The intended on-device path is Vulkan on the Adreno 740 (shaders authored,
  runtime renderer **Designed-not-executed**). GPU numbers are therefore **not**
  measured; no performance claim is made for the Thor yet.

## 15. Security & content integrity

Anti-DoS JSON depth bound, first-party dependency-free core, PNG CRC re-validation,
no network permission, and the hard rule that **art is never invented** — only the
backend's own pixels are relit. Copyright-safe profile export strips ROM identity.
Details: [SECURITY.md](SECURITY.md).

## 16. Known limitations

No real emulator core; SoulSilver/commercial games do not run (ROM-blocked); APK
not device-verified; dual-display and Vulkan runtime not device-verified; built
only on macOS; audio/save-UI/netplay/on-device profile editor not implemented.
Full list: [KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md).

## 17. User actions required

Run the APK on a device/AYN Thor Max and report back; supply a legally-dumped ROM
(never bundled) once real adapters exist; optionally install an emulator system
image; provide a Vulkan target for the GPU path. Details:
[USER_ACTIONS_REQUIRED.md](USER_ACTIONS_REQUIRED.md).

## 18. Exact commands run (key)

```bash
# Shaders
./scripts/test_rendering.sh                        # glslc → SPIR-V (3/3)
# Core build + tests
cmake -S . -B build -G "Unix Makefiles"
cmake --build build -j14
ctest --test-dir build --output-on-failure         # 7/7
# Android wrapper + APK
gradle wrapper --gradle-version 8.9 --distribution-type bin   # (in a temp project)
JAVA_HOME="$(/usr/libexec/java_home -v 17)" ./gradlew :app:assembleDebug   # BUILD SUCCESSFUL
# Verification
shasum -a 256 android/app/build/outputs/apk/debug/app-debug.apk
aapt dump badging  …/app-debug.apk
```

## 19. Files changed / added (this session)

Added: `THIRD_PARTY_NOTICES.md`, root `CMakeLists.txt`, entire `core/`,
`emulation/synthetic/`, `tools/headless-runner/`, `tests/`, `shaders/`, the whole
`android/` project, `scripts/test_rendering.sh`, `README.md`, and docs
(`BUILDING.md`, `ANDROID_BUILD.md`, `TESTING.md`, `TEST_REPORT.md`, `SECURITY.md`,
`LEGAL_AND_LICENSING.md`, `KNOWN_LIMITATIONS.md`, `USER_ACTIONS_REQUIRED.md`, this
report). Updated `docs/TASK_LEDGER.md`. Commits `b5b26e8` (core) and `ccab679`
(Android).

## 20. Next highest-value tasks

1. **Implement the melonDS adapter** behind `EmulatorAdapter` (capture tiles/OAM/
   priorities into `StructuredFrame`), guarded by hash-gated ROM identity and the
   Fidelity Lock. This is the single step that turns the platform from
   "synthetic-only" into "runs a real DS game" (with a user-supplied ROM).
2. **Run the APK on the AYN Thor Max** and verify launch, render loop, touch, and
   **dual-display** routing; capture logcat.
3. **Implement the Vulkan runtime renderer** using the compiled SPIR-V (G-buffer
   → `enhance.frag` → `bloom.frag`) and benchmark on the Adreno 740.
4. **On-device profile editor** (per-tileset material overrides, live preset tuning).
5. **mGBA adapter** for GB/GBC/GBA to make the platform genuinely multi-system.
6. **CI on Linux/Windows** to confirm portability of the first-party core.

---

*Every "working"/"tested" statement above corresponds to a command that was
executed on the host; every unproven item is labeled Blocked or
Designed-not-executed. Nothing here is described as complete, universal, or
production-ready.*
