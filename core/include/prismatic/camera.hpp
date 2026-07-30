// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — camera.
//
// The 2.5D look is produced by a height-driven vertical parallax: taller pixels
// are lifted, giving elevated geometry apparent depth without hiding anything.
// "Gameplay Safe" clamps the maximum displacement so the original playfield
// stays fully readable (nothing critical is pushed off-screen or occluded).
#pragma once
#include "prismatic/types.hpp"
#include "prismatic/presets.hpp"

namespace prismatic {

struct CameraConfig {
    float parallax = 0.5f;   // px of lift per unit height
    float pitch = 0.3f;      // subtle global vertical tilt
    bool gameplaySafe = true;
    float safeMarginY = 7.0f;  // max |displacement| when gameplay-safe (px)

    static CameraConfig fromPreset(const Preset& p) {
        CameraConfig c;
        c.parallax = p.parallax;
        c.pitch = p.pitch;
        c.gameplaySafe = p.gameplaySafe;
        return c;
    }
};

// Per-pixel vertical sample offset (negative = lifted up on screen).
FloatBuffer computeParallaxOffsetY(const FloatBuffer& height, const CameraConfig& c);

// Largest absolute displacement in an offset buffer (for gameplay-safe checks).
float maxAbsOffset(const FloatBuffer& offset);

}  // namespace prismatic
