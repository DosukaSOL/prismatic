// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — lighting.
//
// Shades a reconstructed scene into a linear HDR buffer using: hemisphere
// ambient (sky/ground), an environmental key light, a priority-ordered set of
// point lights with distance attenuation, rim light on billboard sprites,
// screen-space contact shadows / AO from the height field, and highlight
// protection (soft-knee) so bright original pixels never blow out.
#pragma once
#include <vector>
#include "prismatic/scene.hpp"
#include "prismatic/environment.hpp"
#include "prismatic/presets.hpp"

namespace prismatic {

struct Light {
    enum Type { Directional, Point } type = Point;
    Vec3 dir{0, 0, -1};   // travel direction (Directional)
    Vec2 pos{0, 0};       // screen position in px (Point)
    float height = 0.4f;  // height above ground (Point)
    Vec3 color{1, 1, 1};
    float intensity = 1.0f;
    float radius = 64.0f;
    int priority = 0;     // higher dominates in layered resolve
};

struct LightingInput {
    std::vector<Light> lights;  // extra artificial lights (lanterns, signs...)
};

struct LitScene {
    int width = 0, height = 0;
    std::vector<Vec3> hdr;        // linear HDR color
    std::vector<Vec3> lightOnly;  // diffuse lighting term (debug)
    FloatBuffer ao;               // occlusion / contact shadow (debug)
};

LitScene shadeScene(const ReconstructedScene& scene, const EnvLighting& env,
                    const Preset& preset, const LightingInput& lin = {});

// sRGB <-> linear helpers (approximate 2.2 gamma).
Vec3 srgbToLinear(const Color& c);
Color linearToSrgb(const Vec3& v);

}  // namespace prismatic
