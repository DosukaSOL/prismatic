// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — enhancement presets.
//
// A preset is a data-driven bundle of lighting, post-processing, camera and
// style parameters. The ten built-in presets span "almost native" to
// "cinematic". CUSTOM is the editable baseline. Presets round-trip through JSON
// so profiles can carry a tuned preset.
#pragma once
#include <string>
#include <vector>
#include "prismatic/types.hpp"
#include "prismatic/json.hpp"

namespace prismatic {

struct Preset {
    std::string name = "CUSTOM";

    // Lighting.
    float ambientStrength = 1.0f;
    float sunStrength = 1.0f;
    float normalStrength = 1.0f;
    float heightScale = 1.0f;
    float contactShadowStrength = 0.5f;
    float rimStrength = 0.4f;
    float highlightProtection = 0.6f;  // 0..1 soft-knee on bright albedo

    // Post.
    float exposure = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float bloomThreshold = 0.75f;
    float bloomIntensity = 0.3f;
    float vignette = 0.15f;
    float fogScale = 1.0f;
    Vec3 grade{1, 1, 1};

    // Camera / style.
    float parallax = 0.5f;      // height-based vertical lift (px per unit height)
    float pitch = 0.3f;
    bool gameplaySafe = true;
    float pixelSharpness = 0.5f;  // 1 = crisp nearest, 0 = smooth
    float scanline = 0.0f;
    int upscale = 3;              // integer upscale factor for output
};

// Built-in presets.
std::vector<std::string> presetNames();
Preset getPreset(const std::string& name);   // returns CUSTOM if unknown
std::vector<Preset> allPresets();

// JSON round-trip.
JsonValue presetToJson(const Preset& p);
Preset presetFromJson(const JsonValue& j);

}  // namespace prismatic
