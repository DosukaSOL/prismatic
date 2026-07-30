// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — software renderer.
//
// Deterministic CPU renderer: composites the native reference, reconstructs the
// scene, lights it, applies height parallax (gameplay-safe), and runs a post
// chain (exposure, bloom, filmic tonemap, contrast/saturation/grade, fog,
// vignette, integer upscale). Also emits debug views (native, depth, normals,
// object-id, emissive, light-only). This validates the entire pipeline without
// a GPU and produces the exact frames the Vulkan path targets on device.
#pragma once
#include "prismatic/adapter.hpp"
#include "prismatic/scene.hpp"
#include "prismatic/lighting.hpp"
#include "prismatic/camera.hpp"
#include "prismatic/presets.hpp"
#include "prismatic/environment.hpp"
#include "prismatic/composite.hpp"

namespace prismatic {

struct RenderRequest {
    Preset preset = getPreset("HD-2.5D BALANCED");
    EnvironmentState environment;
    LightingInput lights;
    int upscale = 0;      // 0 => use preset.upscale
    bool emitDebug = true;
};

struct RenderResult {
    int width = 0, height = 0;      // native resolution
    Image nativeImage;             // ground-truth composite
    Image enhancedNative;          // enhanced at native res (deterministic ref)
    Image enhanced;                // final, upscaled
    Image depthView;
    Image normalView;
    Image objectIdView;
    Image emissiveView;
    Image lightView;
};

RenderResult renderStructured(const StructuredFrame& frame, MaterialCache& cache,
                              const RenderRequest& req);

// Enhance a flat, already-composited framebuffer (real emulator cores that only
// expose pixels). Re-shades the real image via a luminance-derived pseudo
// G-buffer; never invents or replaces game art.
RenderResult renderFramebuffer(const Image& image, MaterialCache& cache,
                               const RenderRequest& req);

}  // namespace prismatic
