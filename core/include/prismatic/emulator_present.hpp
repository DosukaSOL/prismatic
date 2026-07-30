// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — emulator-frame presentation.
//
// Real emulator cores (melonDS) expose only a flat, already-composited
// framebuffer: final pixels, no tiles, no depth, no geometry. From that input
// the ONLY honest operations are (a) faithful passthrough, (b) a *geometric*
// 2.5D presentation (a real perspective transform of the screen plane — NOT a
// per-pixel luminance guess, which smears), and (c) a post-process "shader"
// overlay (a fully user-tweakable colour grade / bloom / scanlines). These
// three are independent:
//   base (faithful)  ->  [2.5D]  ->  [shader]
// Either layer can be on or off; the shader always overlays whatever the 2.5D
// layer produced. No game art is ever invented or replaced.
#pragma once
#include "prismatic/types.hpp"

namespace prismatic {

// Continuous, user-editable shader controls (a "make your own look" surface,
// like tweaking a filter on the desktop emulator). Defaults are neutral: applied
// as-is they leave the image unchanged. Values are clamped where it matters.
struct ShaderParams {
    float brightness     = 0.0f;   // -1..1  additive lift
    float exposure       = 1.0f;   //  0..2  multiplicative gain (luminance of light)
    float contrast       = 1.0f;   //  0..2  around mid grey
    float saturation     = 1.0f;   //  0..2  0 = greyscale
    float temperature    = 0.0f;   // -1..1  cool <-> warm
    float tint           = 0.0f;   // -1..1  green <-> magenta
    float gamma          = 1.0f;   // 0.4..2.6
    float vignette       = 0.0f;   //  0..1  edge darkening
    float bloom          = 0.0f;   //  0..1  glow on highlights
    float bloomThreshold = 0.75f;  //  0..1  where bloom starts
    float scanline       = 0.0f;   //  0..1  CRT scanlines
    float lcdGrid        = 0.0f;   //  0..1  handheld pixel grid
    float sharpen        = 0.0f;   //  0..1  unsharp mask
};

constexpr int kShaderParamCount = 13;  // fields above, in declaration order

struct PresentationOptions {
    bool  enable25D = false;     // geometric diorama tilt (independent layer)
    bool  enableShader = false;  // post-process overlay (independent layer)
    float tilt = 0.5f;           // 0..1 strength of the 2.5D perspective
    bool  lantern = false;       // warm centre light ("lantern")
    bool  antialias = false;     // FXAA edge smoothing on the final image
    ShaderParams shader;         // used when enableShader is set
};

// Built-in professional looks (HD-2D reference grade). These simply fill a
// ShaderParams the user can then edit and save as their own.
int          shaderPresetCount();
const char*  shaderPresetName(int index);
ShaderParams shaderPreset(int index);

// Pack/unpack ShaderParams to a flat float vector (declaration order) for the
// JNI/UI transport. Buffers must hold kShaderParamCount elements.
void         shaderParamsToArray(const ShaderParams& p, float* out);
ShaderParams shaderParamsFromArray(const float* in);

// Enhance one already-composited screen. Input and output are native
// resolution (e.g. 256x192); on-device the view scales the result, so no CPU
// upscale is done here (keeps the per-frame cost low).
Image renderEmulatorScreen(const Image& framebuffer, const PresentationOptions& opt);

// Depth-aware variant. When 2.5D is enabled and a matching-resolution depth map
// is supplied (normalised 0=near .. 1=far), the 2.5D layer is driven by genuine
// per-pixel depth (real parallax + depth-of-field + depth darkening) instead of
// the flat geometric tilt. Pass depth==nullptr to fall back to the geometric
// path. Everything else (shader, AA) is identical.
Image renderEmulatorScreen(const Image& framebuffer, const FloatBuffer* depth,
                           const PresentationOptions& opt);

}  // namespace prismatic
