# PRISMATIC — Task Ledger

Living record of work. Newest entries at the bottom of each gate. Status keys:
`DONE`, `WIP`, `BLOCKED(reason)`, `DEFERRED(reason)`.

## Gate 0 — Environment audit
- DONE Inspect OS/CPU/mem/disk/git — see `ENVIRONMENT_REPORT.md`
- DONE Inspect clang/cmake/jdk/ndk/sdk/glslc/adb/python/node
- DONE Verify network (github, gradle, maven)
- DONE Create repo skeleton, `.gitignore`, `local_data/` structure
- DONE Create ENVIRONMENT_REPORT / IMPLEMENTATION_PLAN / TASK_LEDGER

## Gate 1 — Research
- DONE 11 research notes under `docs/research/` (emulator cores, licensing, HD-2D
  technique, DS graphics model, Adreno/Vulkan, AYN Thor, dual-display).

## Gate 2 — Architecture & licensing
- DONE `docs/ARCHITECTURE_DECISIONS.md` (ADR-1..10), `LICENSE` (GPL-3.0-or-later),
  `THIRD_PARTY_NOTICES.md`. Adapter-centric, first-party-only tested core.

## Gate 3+ — Implementation (executed on host, macOS arm64)
- DONE Core lib `prismatic_core` (types, SHA-256, JSON, PNG, adapter API,
  composite, materials, scene reconstruction, environment, lighting, camera,
  presets×10, profile engine, software renderer, pipeline). Builds clean.
- DONE `prismatic_synthetic` DS backend (first-party art only, dual 256×192,
  tilemaps, OAM sprites, priorities, scroll, trailing follower, touch UI).
- DONE Headless runner → 10 PNG shots + `report.html` in
  `local_data/validation_output` (visually verified enhanced vs native).
- DONE CTest suite 7/7 PASS: test_hash, test_json, test_png, test_materials,
  test_profile, test_pipeline, shader_compile.
- DONE GLSL → SPIR-V: `shaders/fullscreen.vert`, `enhance.frag`, `bloom.frag`
  compiled by NDK glslc via `scripts/test_rendering.sh`.
- DONE Android app `android/` (Kotlin dual-display frontend + JNI bridge reusing
  `prismatic_core`, NDK CMake). `./gradlew :app:assembleDebug` → installable
  `app-debug.apk` (arm64-v8a, 4.08 MB, launchable `com.prismatic.app/.MainActivity`).
- BLOCKED(no device) On-device run on AYN Thor Max — no hardware attached; APK
  built + packaged but not runtime-verified. Dual-display + Vulkan-optimised
  path DESIGNED, not device-verified.
- DONE melonDS DS backend integrated behind `EmulatorAdapter` (pinned submodule
  `third_party/melonDS`, custom `melonDS::Platform` layer, `NdsAdapter` feeding
  real 256×192 BGRA framebuffers into the enhancement pipeline). Builds + links
  on desktop and Android arm64.
- DONE Desktop ROM harness `prismatic_nds_harness` boots + runs a first-party
  test `.nds` (60 frames @ ~2.5 ms/frame interpreter), extracts framebuffers,
  writes native + enhanced PNGs. Full runtime chain (threading/timing Platform
  code) exercised.
- DONE Android APK links melonDS into `libprismaticnative.so` (arm64-v8a); ROM
  picker (Storage Access Framework), physical gamepad mapping, and touch wired
  to `NdsAdapter`. Signed **release** APK: 6.3 MB.
- BLOCKED(no device/ROM) Commercial DS ROM boot on the Thor — maintainer device
  test with their own ROM; nothing bundled.
- DEFERRED(no ROM) Real mGBA (GB/GBC/GBA) adapter — designed; requires user
  ROM/BIOS, never bundled.

