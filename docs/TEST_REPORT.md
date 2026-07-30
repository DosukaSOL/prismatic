<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Test report

Evidence-based results from execution on the build host. Every line is labeled:
**Passed-executed**, **Failed-executed**, **Skipped**, **Blocked**, or
**Designed-not-executed**.

- Host: macOS (Apple Silicon), AppleClang 21, CMake 3.22.1.
- Repository commit at time of report: `ccab679`.
- Source size: ~3,986 lines C/C++ (core + synthetic backend + shaders + JNI) and
  387 lines Kotlin (Android app).

## Desktop build

| Item | Result |
|---|---|
| `cmake -S . -B build -G "Unix Makefiles"` | **Passed-executed** |
| `cmake --build build -j` (all targets) | **Passed-executed** — compiles clean with `-Wall -Wextra`. |

## CTest suite — 7/7 Passed-executed

```
1/7 test_hash ......... Passed
2/7 test_json ......... Passed
3/7 test_png .......... Passed
4/7 test_materials .... Passed
5/7 test_profile ...... Passed
6/7 test_pipeline ..... Passed
7/7 shader_compile .... Passed
100% tests passed, 0 tests failed out of 7   (0.23 s)
```

| Test | Result | Notes |
|---|---|---|
| `test_hash` | **Passed-executed** | SHA-256 FIPS-180-4 vectors. |
| `test_json` | **Passed-executed** | round-trip + depth-bound rejection. |
| `test_png` | **Passed-executed** | chunk/CRC re-validation incl. large image. |
| `test_materials` | **Passed-executed** | classification + cache. |
| `test_profile` | **Passed-executed** | 10 presets, JSON round-trip, precedence, day>night, camera clamp. |
| `test_pipeline` | **Passed-executed** | backend structure, determinism, full render + debug views. |
| `shader_compile` | **Passed-executed** | GLSL→SPIR-V via NDK glslc. |

## Shaders (GLSL → SPIR-V)

**Passed-executed** — `glslc` (shaderc v2022.3, ndk-r24):

| Shader | Output | Size |
|---|---|---|
| `shaders/fullscreen.vert` | `fullscreen.vert.spv` | 916 B |
| `shaders/enhance.frag` | `enhance.frag.spv` | 4,932 B |
| `shaders/bloom.frag` | `bloom.frag.spv` | 2,796 B |

## Headless validation runner

**Passed-executed** — `prismatic_headless` wrote 10 shots + `report.html` to
`local_data/validation_output/`. Native vs enhanced PNGs visually confirm genuine
enhancement (3× upscale, contact shadows, sprite relief, day/night, grading, night
+ lantern point light). Observed software render time for the hero shot (256×192):
~34 ms first frame, ~16–20 ms subsequently.

## Android APK

| Item | Result |
|---|---|
| `./gradlew :app:assembleDebug` | **Passed-executed** — `BUILD SUCCESSFUL in 44s`, 40 tasks, native `buildCMakeDebug[arm64-v8a]` included. |
| APK produced | **Passed-executed** — `android/app/build/outputs/apk/debug/app-debug.apk` |
| APK size | 3,971,111 bytes (3.97 MB) |
| APK SHA-256 | `798fcb7096e4e363508ca05eb88f3fc51c08cf60acce17dd63d0352841241706` |
| Contents | `classes.dex` (2,308,976 B), `lib/arm64-v8a/libprismaticnative.so` (1,609,944 B), `AndroidManifest.xml` |
| `.so` type | ELF 64-bit LSB, ARM aarch64, stripped — **Passed-executed** (`file`). |
| Badging | package `com.prismatic.app` v0.1.0, launchable `.MainActivity`, native-code `arm64-v8a`, minSdk 30 / targetSdk 34 / compileSdk 36 — **Passed-executed** (`aapt dump badging`). |

## Not executed (honest)

| Item | Label | Reason |
|---|---|---|
| App launch / render loop / touch on device | **Blocked** | No physical device or emulator system image on host. |
| Dual-display routing on AYN Thor Max | **Blocked** | No dual-display hardware here. |
| Runtime Vulkan rendering with the shaders | **Designed-not-executed** | No Vulkan renderer implemented; no ICD/GPU run. |
| Real DS game (e.g. SoulSilver) enhancement | **Blocked** | No emulator adapter + no user ROM (never bundled). |
| melonDS / mGBA adapter tests | **Designed-not-executed** | Adapters not implemented. |
| Linux/Windows desktop build | **Skipped** | Not attempted; no OS-specific code, but unverified. |
