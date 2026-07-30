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

// ---- Layer 2: shader overlay (colour grade / bloom / scanlines) --------------
// A tasteful post-process applied on top of whatever layer 1 produced. Styles
// are deliberately restrained so colours stay faithful.
Image applyShader(const Image& src, int style, float timeOfDay, bool lantern) {
    const int W = src.width, H = src.height;
    std::vector<F3> c((size_t)W * H);
    for (int i = 0; i < W * H; ++i) c[i] = toF(src.pixels[i]);

    // Day/night grade (subtle): dusk warms, night cools + dims.
    float t = timeOfDay;
    float night = smoothstep(6.0f, 2.0f, t) + smoothstep(18.0f, 22.0f, t);  // 0 day..1 night
    night = clampf(night, 0.0f, 1.0f);
    F3 nightTint{0.80f, 0.86f, 1.08f};
    float nightExposure = lerpf(1.0f, 0.82f, night);

    // Style parameters.
    float exposure = nightExposure, contrast = 1.0f, saturation = 1.0f, vignette = 0.12f;
    float bloomThresh = 0.75f, bloomAmt = 0.0f, scan = 0.0f, grid = 0.0f;
    F3 grade{1, 1, 1};
    switch (style) {
        case 0:  // CRT
            bloomAmt = 0.35f; scan = 0.18f; vignette = 0.18f; grade = {1.04f, 1.0f, 0.96f};
            break;
        case 1:  // LCD (handheld)
            grid = 0.10f; contrast = 1.06f; saturation = 1.05f; vignette = 0.08f;
            break;
        case 2:  // Warm cinematic
            bloomAmt = 0.30f; vignette = 0.20f; grade = {1.08f, 1.0f, 0.92f}; saturation = 1.06f;
            break;
        case 3:  // Night
            exposure *= 0.9f; bloomAmt = 0.28f; vignette = 0.26f; grade = {0.88f, 0.94f, 1.12f};
            break;
        default: // Vivid
            contrast = 1.12f; saturation = 1.22f; bloomAmt = 0.18f; vignette = 0.10f;
            break;
    }

    // Bloom source.
    std::vector<F3> bright((size_t)W * H, F3{0, 0, 0});
    if (bloomAmt > 0) {
        for (int i = 0; i < W * H; ++i) {
            float l = luma(c[i]);
            if (l > bloomThresh) {
                float k = (l - bloomThresh) / (1.0f - bloomThresh + 1e-4f);
                bright[i] = {c[i].r * k, c[i].g * k, c[i].b * k};
            }
        }
        bright = boxBlur(bright, W, H, 2);
        bright = boxBlur(bright, W, H, 4);
    }

    Image out(W, H);
    float cxn = (W - 1) * 0.5f, cyn = (H - 1) * 0.5f;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            F3 v = c[i];
            // Day/night.
            v = {lerpf(v.r, v.r * nightTint.r, night), lerpf(v.g, v.g * nightTint.g, night),
                 lerpf(v.b, v.b * nightTint.b, night)};
            v = {v.r * exposure, v.g * exposure, v.b * exposure};
            // Bloom.
            if (bloomAmt > 0) { v.r += bright[i].r * bloomAmt; v.g += bright[i].g * bloomAmt; v.b += bright[i].b * bloomAmt; }
            // Contrast around mid grey.
            v = {(v.r - 0.5f) * contrast + 0.5f, (v.g - 0.5f) * contrast + 0.5f, (v.b - 0.5f) * contrast + 0.5f};
            // Saturation.
            float l = luma(v);
            v = {lerpf(l, v.r, saturation), lerpf(l, v.g, saturation), lerpf(l, v.b, saturation)};
            // Grade.
            v = {v.r * grade.r, v.g * grade.g, v.b * grade.b};
            // Lantern warm centre light (night).
            if (lantern) {
                float dx = (x - cxn) / W, dy = (y - cyn) / H;
                float d = std::sqrt(dx * dx + dy * dy);
                float g = (1.0f - smoothstep(0.05f, 0.5f, d)) * 0.35f * (0.4f + 0.6f * night);
                v = {v.r + g * 1.0f, v.g + g * 0.72f, v.b + g * 0.38f};
            }
            // Scanlines (CRT) — darken alternate native rows; stays crisp when
            // the view nearest-scales the frame.
            if (scan > 0 && (y & 1)) { v = {v.r * (1 - scan), v.g * (1 - scan), v.b * (1 - scan)}; }
            // LCD pixel grid — faint dark border on alternate columns/rows.
            if (grid > 0 && ((x & 1) || (y & 1))) { v = {v.r * (1 - grid), v.g * (1 - grid), v.b * (1 - grid)}; }
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

}  // namespace

Image renderEmulatorScreen(const Image& framebuffer, const PresentationOptions& opt) {
    if (framebuffer.pixels.empty()) return framebuffer;
    // Layer 0: faithful passthrough (default) — correct colours, no processing.
    Image img = framebuffer;
    // Layer 1: geometric 2.5D (independent).
    if (opt.enable25D) img = apply25D(img, opt.tilt);
    // Layer 2: shader overlay on top (independent).
    if (opt.enableShader) img = applyShader(img, opt.shaderStyle, opt.timeOfDay, opt.lantern);
    return img;
}

}  // namespace prismatic
