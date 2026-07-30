# Architecture Decisions

Status legend: **ACCEPTED** decisions are implemented or being implemented this
session; **PLANNED** are designed but not built here.

## ADR-1 Adapter-centric architecture — ACCEPTED
Emulation and enhancement are separated by a **versioned `EmulatorAdapter`**
interface (`core/include/prismatic/adapter.hpp`). Backends (synthetic, mGBA,
melonDS) implement it; the enhancement stack consumes only the adapter's
`StructuredFrame` + capability flags. An enhancement failure can never mutate
emulated state.

## ADR-2 Language/stack — ACCEPTED
- **C++20** for core, capture, reconstruction, materials, lighting, environment,
  camera, profiles, software renderer, headless runner.
- **Kotlin + NDK/JNI** for the Android app; **Vulkan 1.1** renderer in C++ with
  **GLES 3.1** fallback.
- **CMake** for native; **Gradle** (wrapper) for Android. No large game engine.

## ADR-3 Deterministic, testable core — ACCEPTED
All enhancement logic is pure/deterministic and unit-tested on desktop. A
**software rasterizer** renders reconstructed scenes to RGBA/PNG so the pipeline
is verifiable without a GPU. Randomness (particles) uses seeded PRNGs.

## ADR-4 Backends as modules; DS path is GPL-3.0 — ACCEPTED (policy) / PLANNED (wiring)
mGBA (MPL-2.0) and melonDS (GPL-3.0) are integrated as libraries behind the
adapter. The synthetic backend is fully implemented now; real cores are wired in
a later gate (documented as deferred). The adapter is process-agnostic so a
GPL-isolating subprocess is possible.

## ADR-5 Four compatibility levels surfaced at runtime — ACCEPTED
`Level0 Native`, `Level1 Enhanced2D`, `Level2 Structured2.5D`, `Level3 Authored`
are explicit enums; the active level is always reported and never overstated.

## ADR-6 Fidelity Lock is a core invariant — ACCEPTED
When enabled, only source-derived assets/geometry/explicit-profile meshes may be
shown; grading/bloom clamped; DoF cannot touch UI; camera clamped to validated
bounds. Enforced in the reconstruction + compositor, not just UI.

## ADR-7 Profiles are versioned JSON with rule precedence — ACCEPTED
Precedence **tile → tileset → map → game → default**. Schema validated;
migrations versioned; export is copyright-safe (no ROM-derived pixels).

## ADR-8 Dual-display via runtime probe + strategy selection — ACCEPTED (logic) / PLANNED (device)
`DisplayRouter` chooses among two-display `Presentation` and single-surface
splits based on a runtime `DisplayTopology` probe. Touch mapping computed from
actual surface rects. Logic unit-tested; on-device behavior is device-blocked.

## ADR-9 Renderer parameters are data-driven presets — ACCEPTED
10 presets as parameter sets; environment profiles modulate per-location; the
two compose (style × location) rather than a single global grade.

## ADR-10 Security posture — ACCEPTED
Offline by default; no telemetry; defensive parsing of all imported profiles/
presets/archives; no code execution from profile packages; ROM/BIOS/firmware/
saves never leave the device or enter source control.
