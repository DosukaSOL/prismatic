// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — emulator-frame presentation (see header for the contract).
#include "prismatic/emulator_present.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace prismatic {
namespace {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int   clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0 + 1e-6f), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct F3 { float r, g, b; };
inline F3 toF(const Color& c) { return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f}; }
inline Color toC(const F3& f) {
    return Color{(uint8_t)std::lround(clampf(f.r, 0, 1) * 255.0f),
                 (uint8_t)std::lround(clampf(f.g, 0, 1) * 255.0f),
                 (uint8_t)std::lround(clampf(f.b, 0, 1) * 255.0f), 255};
}
inline float luma(const F3& c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }

// Bilinear sample of an RGBA image with edge clamp.
Color sampleBilinear(const Image& img, float fx, float fy) {
    int x0 = clampi((int)std::floor(fx), 0, img.width - 1);
    int y0 = clampi((int)std::floor(fy), 0, img.height - 1);
    int x1 = clampi(x0 + 1, 0, img.width - 1);
    int y1 = clampi(y0 + 1, 0, img.height - 1);
    float tx = fx - std::floor(fx), ty = fy - std::floor(fy);
    auto mix = [](const Color& a, const Color& b, float t) {
        return F3{lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t)};
    };
    F3 top = mix(img.at(x0, y0), img.at(x1, y0), tx);
    F3 bot = mix(img.at(x0, y1), img.at(x1, y1), tx);
    F3 c{lerpf(top.r, bot.r, ty), lerpf(top.g, bot.g, ty), lerpf(top.b, bot.b, ty)};
    return Color{(uint8_t)std::lround(clampf(c.r, 0, 255)),
                 (uint8_t)std::lround(clampf(c.g, 0, 255)),
                 (uint8_t)std::lround(clampf(c.b, 0, 255)), 255};
}

// Separable box blur over an F3 buffer (radius in pixels).
std::vector<F3> boxBlur(const std::vector<F3>& in, int W, int H, int r) {
    if (r <= 0) return in;
    std::vector<F3> tmp(in.size()), out(in.size());
    const float inv = 1.0f / (2 * r + 1);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            F3 s{0, 0, 0};
            for (int k = -r; k <= r; ++k) {
                int xx = clampi(x + k, 0, W - 1);
                const F3& p = in[(size_t)y * W + xx];
                s.r += p.r; s.g += p.g; s.b += p.b;
            }
            tmp[(size_t)y * W + x] = {s.r * inv, s.g * inv, s.b * inv};
        }
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            F3 s{0, 0, 0};
            for (int k = -r; k <= r; ++k) {
                int yy = clampi(y + k, 0, H - 1);
                const F3& p = tmp[(size_t)yy * W + x];
                s.r += p.r; s.g += p.g; s.b += p.b;
            }
            out[(size_t)y * W + x] = {s.r * inv, s.g * inv, s.b * inv};
        }
    return out;
}

// ---- Layer 1: geometric 2.5D (a real perspective transform, not a smear) ----
//
// Maps the flat screen onto a plane that recedes toward the top, exactly like
// viewing a card tilted away from you. The vertical texture coordinate uses the
// standard rational perspective form so the top foreshortens correctly; the
// horizontal width narrows with depth. A tilt-shift blur on the far/near bands
// sells the miniature-diorama look. Nothing is invented — every output pixel is
// a resample of a real input pixel (or the dark ground behind the card).
Image apply25D(const Image& src, float tilt) {
    const int W = src.width, H = src.height;
    Image out(W, H, Color{6, 7, 10, 255});   // dark diorama ground
    const float a = 1.0f - 0.30f * clampf(tilt, 0.0f, 1.0f);   // top width scale
    const float cx = (W - 1) * 0.5f;

    // Pre-blur once for the tilt-shift bands.
    std::vector<F3> lin((size_t)W * H);
    for (int i = 0; i < W * H; ++i) lin[i] = toF(src.pixels[i]);
    std::vector<F3> blur = boxBlur(lin, W, H, 2);

    for (int oy = 0; oy < H; ++oy) {
        float p = (float)oy / (H - 1);                    // 0 top .. 1 bottom
        float vTex = (a * p) / (1.0f - (1.0f - a) * p);   // perspective foreshorten
        float sy = vTex * (H - 1);
        float rowScale = a + (1.0f - a) * p;              // narrower at top
        // Tilt-shift: sharp around the lower-centre focus band, soft at edges.
        float focus = 0.62f;
        float blurW = smoothstep(0.16f, 0.5f, std::fabs(vTex - focus));
        for (int ox = 0; ox < W; ++ox) {
            float sx = (ox - cx) / rowScale + cx;
            if (sx < 0 || sx > W - 1 || sy < 0 || sy > H - 1) continue;  // ground
            Color sharp = sampleBilinear(src, sx, sy);
            if (blurW <= 0.001f) { out.at(ox, oy) = sharp; continue; }
            int bx = clampi((int)std::lround(sx), 0, W - 1);
            int by = clampi((int)std::lround(sy), 0, H - 1);
            const F3& b = blur[(size_t)by * W + bx];
            F3 sh = toF(sharp);
            out.at(ox, oy) = toC({lerpf(sh.r, b.r, blurW), lerpf(sh.g, b.g, blurW),
                                  lerpf(sh.b, b.b, blurW)});
        }
    }
    return out;
}

// ---- Layer 2: shader overlay (fully user-tweakable post-process) -------------
// Every control below maps to a slider in the on-device editor. Order of
// operations is a conventional grading chain; neutral params leave the image
// unchanged so "shader on, all defaults" == faithful.
Image applyShader(const Image& src, const ShaderParams& p, bool lantern) {
    const int W = src.width, H = src.height;
    std::vector<F3> c((size_t)W * H);
    for (int i = 0; i < W * H; ++i) c[i] = toF(src.pixels[i]);

    // Unsharp mask source (blurred copy), only if requested.
    std::vector<F3> soft;
    if (p.sharpen > 0.001f) soft = boxBlur(c, W, H, 1);

    // Bloom source: bright areas, blurred, added back.
    std::vector<F3> bright;
    if (p.bloom > 0.001f) {
        bright.assign((size_t)W * H, F3{0, 0, 0});
        float thr = clampf(p.bloomThreshold, 0.0f, 1.0f);
        for (int i = 0; i < W * H; ++i) {
            float l = luma(c[i]);
            if (l > thr) {
                float k = (l - thr) / (1.0f - thr + 1e-4f);
                bright[i] = {c[i].r * k, c[i].g * k, c[i].b * k};
            }
        }
        bright = boxBlur(bright, W, H, 2);
        bright = boxBlur(bright, W, H, 4);
    }

    const float exposure = clampf(p.exposure, 0.0f, 4.0f);
    const float contrast = clampf(p.contrast, 0.0f, 4.0f);
    const float saturation = clampf(p.saturation, 0.0f, 4.0f);
    const float temp = clampf(p.temperature, -1.0f, 1.0f);
    const float tint = clampf(p.tint, -1.0f, 1.0f);
    const float invGamma = 1.0f / clampf(p.gamma, 0.1f, 6.0f);
    const float vignette = clampf(p.vignette, 0.0f, 1.0f);
    const float scan = clampf(p.scanline, 0.0f, 1.0f);
    const float grid = clampf(p.lcdGrid, 0.0f, 1.0f);
    const float sharpen = clampf(p.sharpen, 0.0f, 1.0f);
    const float bloom = clampf(p.bloom, 0.0f, 1.0f);

    Image out(W, H);
    const float cxn = (W - 1) * 0.5f, cyn = (H - 1) * 0.5f;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = (size_t)y * W + x;
            F3 v = c[i];
            // Sharpen (unsharp mask).
            if (sharpen > 0.001f) {
                const F3& b = soft[i];
                v = {v.r + (v.r - b.r) * sharpen, v.g + (v.g - b.g) * sharpen,
                     v.b + (v.b - b.b) * sharpen};
            }
            // Exposure then brightness (the "luminance of light").
            v = {v.r * exposure + p.brightness * 0.5f, v.g * exposure + p.brightness * 0.5f,
                 v.b * exposure + p.brightness * 0.5f};
            // Bloom.
            if (bloom > 0.001f) { v.r += bright[i].r * bloom; v.g += bright[i].g * bloom; v.b += bright[i].b * bloom; }
            // Contrast around mid grey.
            v = {(v.r - 0.5f) * contrast + 0.5f, (v.g - 0.5f) * contrast + 0.5f,
                 (v.b - 0.5f) * contrast + 0.5f};
            // Saturation.
            float l = luma(v);
            v = {lerpf(l, v.r, saturation), lerpf(l, v.g, saturation), lerpf(l, v.b, saturation)};
            // White balance: temperature (R<->B) and tint (G<->magenta).
            v = {v.r * (1.0f + 0.20f * temp), v.g * (1.0f - 0.16f * tint),
                 v.b * (1.0f - 0.20f * temp)};
            // Gamma.
            v = {std::pow(clampf(v.r, 0, 1), invGamma), std::pow(clampf(v.g, 0, 1), invGamma),
                 std::pow(clampf(v.b, 0, 1), invGamma)};
            // Lantern warm centre light.
            if (lantern) {
                float dx = (x - cxn) / W, dy = (y - cyn) / H;
                float d = std::sqrt(dx * dx + dy * dy);
                float g = (1.0f - smoothstep(0.05f, 0.5f, d)) * 0.35f;
                v = {v.r + g, v.g + g * 0.72f, v.b + g * 0.38f};
            }
            // Scanlines (CRT) — darken alternate native rows.
            if (scan > 0 && (y & 1)) v = {v.r * (1 - scan), v.g * (1 - scan), v.b * (1 - scan)};
            // LCD pixel grid — faint dark border on alternate columns/rows.
            if (grid > 0 && ((x & 1) || (y & 1)))
                v = {v.r * (1 - grid), v.g * (1 - grid), v.b * (1 - grid)};
            // Vignette.
            if (vignette > 0) {
                float nx = (x / (float)W - 0.5f) * 2.0f, ny = (y / (float)H - 0.5f) * 2.0f;
                float vg = 1.0f - vignette * clampf(nx * nx + ny * ny, 0.0f, 1.0f);
                v = {v.r * vg, v.g * vg, v.b * vg};
            }
            out.at(x, y) = toC(v);
        }
    return out;
}

// ---- Built-in professional looks (HD-2D reference grade) --------------------
struct NamedPreset { const char* name; ShaderParams p; };
const NamedPreset kPresets[] = {
    // Flagship HD-2D look: warm key, gentle filmic contrast, soft bloom + vignette
    // (inspired by modern HD-2D titles). Faithful colours, just cinematic.
    {"HD-2D", [] { ShaderParams p; p.exposure = 1.02f; p.contrast = 1.10f;
        p.saturation = 1.08f; p.temperature = 0.12f; p.gamma = 1.02f; p.vignette = 0.18f;
        p.bloom = 0.32f; p.bloomThreshold = 0.70f; return p; }()},
    {"CRT", [] { ShaderParams p; p.contrast = 1.06f; p.saturation = 1.04f;
        p.temperature = 0.06f; p.vignette = 0.20f; p.bloom = 0.35f; p.bloomThreshold = 0.65f;
        p.scanline = 0.16f; return p; }()},
    {"LCD", [] { ShaderParams p; p.contrast = 1.08f; p.saturation = 1.06f;
        p.temperature = -0.03f; p.vignette = 0.06f; p.lcdGrid = 0.10f; p.sharpen = 0.18f;
        return p; }()},
    {"Night", [] { ShaderParams p; p.exposure = 0.86f; p.contrast = 1.06f;
        p.saturation = 0.98f; p.temperature = -0.16f; p.gamma = 1.05f; p.vignette = 0.28f;
        p.bloom = 0.26f; p.bloomThreshold = 0.60f; return p; }()},
    {"Vivid", [] { ShaderParams p; p.exposure = 1.03f; p.contrast = 1.12f;
        p.saturation = 1.26f; p.vignette = 0.10f; p.bloom = 0.16f; p.sharpen = 0.10f;
        return p; }()},
};

}  // namespace

int shaderPresetCount() { return (int)(sizeof(kPresets) / sizeof(kPresets[0])); }

const char* shaderPresetName(int index) {
    if (index < 0 || index >= shaderPresetCount()) return "";
    return kPresets[index].name;
}

ShaderParams shaderPreset(int index) {
    if (index < 0 || index >= shaderPresetCount()) return ShaderParams{};
    return kPresets[index].p;
}

void shaderParamsToArray(const ShaderParams& p, float* o) {
    o[0] = p.brightness; o[1] = p.exposure; o[2] = p.contrast; o[3] = p.saturation;
    o[4] = p.temperature; o[5] = p.tint; o[6] = p.gamma; o[7] = p.vignette;
    o[8] = p.bloom; o[9] = p.bloomThreshold; o[10] = p.scanline; o[11] = p.lcdGrid;
    o[12] = p.sharpen;
}

ShaderParams shaderParamsFromArray(const float* in) {
    ShaderParams p;
    p.brightness = in[0]; p.exposure = in[1]; p.contrast = in[2]; p.saturation = in[3];
    p.temperature = in[4]; p.tint = in[5]; p.gamma = in[6]; p.vignette = in[7];
    p.bloom = in[8]; p.bloomThreshold = in[9]; p.scanline = in[10]; p.lcdGrid = in[11];
    p.sharpen = in[12];
    return p;
}

Image renderEmulatorScreen(const Image& framebuffer, const PresentationOptions& opt) {
    if (framebuffer.pixels.empty()) return framebuffer;
    // Layer 0: faithful passthrough (default) — correct colours, no processing.
    Image img = framebuffer;
    // Layer 1: geometric 2.5D (independent).
    if (opt.enable25D) img = apply25D(img, opt.tilt);
    // Layer 2: shader overlay on top (independent).
    if (opt.enableShader) img = applyShader(img, opt.shader, opt.lantern);
    return img;
}

}  // namespace prismatic
