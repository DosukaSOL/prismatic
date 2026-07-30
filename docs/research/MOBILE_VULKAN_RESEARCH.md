# Mobile Vulkan Research

> Honesty note: vendor GPU optimization pages (Qualcomm Adreno guides) were
> **not** independently fetched this session. The guidance below is standard,
> widely-documented mobile-Vulkan practice from general knowledge and is applied
> conservatively. Any Adreno-specific tuning will be gated behind **measured
> evidence on the real device** (the brief forbids folklore GPU hacks).

## Renderer decision
- **Primary:** Vulkan 1.1 (broad arm64 Android coverage). **Fallback:** OpenGL
  ES 3.1.
- Own the surface, swapchain, frame pacing, pipeline cache, render passes and
  shader modules directly (no heavyweight game engine).

## Practices applied (safe, non-folklore)
1. **Pipeline cache** persisted to disk; **shader warmup** at load to avoid
   first-use hitching.
2. **Tiler-friendly render passes**: use `LOAD_OP_CLEAR`/`STORE_OP_DONT_CARE`
   appropriately; keep transient attachments (depth, intermediate) as
   `LAZILY_ALLOCATED`/`DONT_CARE` so a tile GPU need not write them to main
   memory.
3. **Subpasses** for post chains where inputs are same-pixel (bloom threshold →
   composite) to stay on-tile.
4. **Batching + instancing** for tile prisms and sprite billboards (one draw per
   material/atlas).
5. **Async transfers** for atlas/geometry uploads on a transfer queue.
6. **Light culling** (tiled/clustered-lite) and frustum culling; **bounded**
   particle counts.
7. **Dynamic resolution** + dynamic shadow/fog/reflection quality driven by a
   frame-time governor and thermal status.
8. Prefer **half-precision** in fragment math where it doesn't harm the art
   (color grade, fog) — validated visually.

## Frame pacing vs. emulation timing
- Emulation runs at the **core's native rate** (e.g. DS ~59.83 Hz). Presentation
  may duplicate frames / use `Choreographer`/`AChoreographer` and per-display
  mode selection to pace to the panel (e.g. 120 Hz upper) **without** speeding
  up game logic. This separation is a hard rule.

## Validation strategy in this environment
- Desktop has **no Vulkan ICD**, so Vulkan is **not executed** here. Instead:
  - Shaders are **compiled to SPIR-V** with the NDK `glslc` as a correctness
    gate (`scripts/test_rendering.sh`, `tests/shader_compile`).
  - The Vulkan renderer C++ is compiled for Android by the NDK when the APK is
    built.
  - The **deterministic software renderer** validates the *pipeline logic*
    (geometry, depth, lighting, compositing) on desktop and in CI.
- Real GPU timings are a **device-blocked** task (`../PERFORMANCE.md`).
