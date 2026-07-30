# PRISMATIC — Implementation Plan

This plan is deliberately honest about what is achievable and **verifiable in
this environment** versus what requires hardware or proprietary files the agent
cannot supply.

## Guiding reality check

The full product described in the brief is a multi-year, multi-engineer effort
(two production emulator cores, a bespoke mobile Vulkan HD-2.5D deferred
renderer, an on-device authoring suite, and a per-map SoulSilver remaster). A
single autonomous session cannot legitimately complete all of it. Therefore the
plan targets **the strongest genuinely-working, fully-tested vertical slice**,
with real code (no placeholder stubs standing in for logic), and documents the
remainder truthfully.

## What will be built and TESTED on this machine (primary deliverables)

1. **Emulator Adapter API** (versioned C++), capability flags, structured
   graphics capture types.
2. **Synthetic DS backend** — deterministic dual-screen frames with real tile
   maps, palettes, sprites/OAM, priorities, scroll registers, day/night, and a
   moving player + follower. This exercises the whole pipeline without a ROM.
3. **Scene reconstruction** — tilemap → geometry (flat / elevated / extruded
   prism / billboard), stable object IDs, depth, priority preservation.
4. **Material system** — deterministic, edge-aware procedural normal / height /
   roughness / emissive derivation with content-hash caching.
5. **Lighting** — layered light-source priority, attenuation, contact shadows,
   highlight protection.
6. **Environment + time-of-day + weather** — location tags, smooth day→night
   interpolation, fog/exposure/bloom curves.
7. **Camera** — presets + **Gameplay Safe** bounds clamping.
8. **Profile engine** — JSON schema, parse, validate, migrate, rule precedence
   (tile → tileset → map → game), **copyright-safe export** verification,
   **Fidelity Lock**.
9. **Software renderer** — a deterministic CPU rasterizer that renders the
   reconstructed scene (native + enhanced + depth + object-id + emissive + light
   debug views) to PNG. Proves the pipeline end-to-end without a GPU.
10. **Shader preset system** — all 10 presets as data-driven parameter sets.
11. **Headless runner** — loads fixtures, advances deterministic frames, injects
    input, captures screenshots, writes an **HTML report**.
12. **Real GLSL shaders** compiled to **SPIR-V** with the NDK `glslc` (compile
    gate proves they are valid, not decorative).
13. **Tests** — unit + integration + deterministic visual + a parser fuzz test,
    all run under CTest here.

## What will be built as real code but only PARTIALLY verifiable here

14. **Android app** — Kotlin activity, `DisplayManager` dual-display +
    `Presentation`, `SurfaceView`s, JNI bridge, **Vulkan** renderer in C++
    (compiled by NDK). An `assembleDebug` APK build is **attempted**; success
    depends on Gradle/AGP downloads. Runtime behavior on the AYN Thor requires
    the user's device.

## What is explicitly NOT built (documented, not faked)

- Integration of real **melonDS / mGBA** cores. Their licenses, sizes and build
  systems are analyzed and the adapter is designed to host them, but wiring a
  production core is out of scope for one session and would not be testable
  without ROMs/firmware. See `docs/KNOWN_LIMITATIONS.md`.
- A complete per-map **SoulSilver** remaster. The profile **foundation**
  (identity, schema, rule precedence, environment tags) is built; authored map
  geometry is not, and requires the user's ROM for validation.
- On-device **profile editor** UI (the profile *engine* and format are built;
  the touch editor is scaffolded and documented as planned).

## Gate mapping

| Gate | Status target this session |
|---|---|
| 0 Environment audit | Done + reported |
| 1 Research | Focused, real, documented |
| 2 Architecture + licensing | Decided + recorded |
| 3 Reproducible build | Desktop CMake + Android Gradle scaffolding |
| 4 Synthetic backend | Implemented + tested |
| 5 Renderer baseline | Software renderer + presets tested; Vulkan code written |
| 6 Dual display | Android code written; logic unit-tested; device-blocked runtime |
| 7 Structured graphics | Implemented + inspected in reports |
| 8 True 2.5D | Implemented + tested via fixtures |
| 9 Lighting/environment | Implemented + tested |
| 10 Profile system | Implemented + tested |
| 11 GB/GBC/GBA backend | Adapter + synthetic; real core deferred (documented) |
| 12 DS backend | Adapter + synthetic dual-screen; real core deferred (documented) |
| 13 SoulSilver foundation | Identity + schema + tags; ROM-blocked validation |
| 14 Optimization | Budgets/quality modes in renderer; device-blocked profiling |
| 15 QA | CTest suite executed |
| 16 Packaging | APK assemble attempted; checksums generated |

## Definition of done for this session

The desktop core builds cleanly, the full CTest suite passes, the headless
runner produces screenshots + an HTML report from synthetic fixtures, shaders
compile to SPIR-V, and the Android project is complete enough to attempt an APK.
Everything not completed is listed in `docs/KNOWN_LIMITATIONS.md` and
`docs/USER_ACTIONS_REQUIRED.md`.
