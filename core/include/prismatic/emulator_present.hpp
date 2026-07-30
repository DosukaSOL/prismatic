// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — emulator-frame presentation.
//
// Real emulator cores (melonDS) expose only a flat, already-composited
// framebuffer: final pixels, no tiles, no depth, no geometry. From that input
// the ONLY honest operations are (a) faithful passthrough, (b) a *geometric*
// 2.5D presentation (a real perspective transform of the screen plane — NOT a
// per-pixel luminance guess, which smears), and (c) a post-process "shader"
// overlay (colour grade / bloom / scanlines). These three are independent:
//   base (faithful)  ->  [2.5D]  ->  [shader]
// Either layer can be on or off; the shader always overlays whatever the 2.5D
// layer produced. No game art is ever invented or replaced.
#pragma once
#include "prismatic/types.hpp"

namespace prismatic {

struct PresentationOptions {
    bool enable25D = false;      // geometric diorama tilt (independent layer)
    bool enableShader = false;   // post-process overlay (independent layer)
    int  shaderStyle = 0;        // 0 CRT, 1 LCD, 2 Warm, 3 Night, 4 Vivid
    float tilt = 0.5f;           // 0..1 strength of the 2.5D perspective
    float timeOfDay = 12.0f;     // drives a subtle day/night grade when shading
    bool  lantern = false;       // warm centre light for night scenes
};

// Enhance one already-composited screen. Input and output are native
// resolution (e.g. 256x192); on-device the view scales the result, so no CPU
// upscale is done here (keeps the per-frame cost low).
Image renderEmulatorScreen(const Image& framebuffer, const PresentationOptions& opt);

}  // namespace prismatic
