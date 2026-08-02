// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/game_lighting.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace prismatic {
namespace {

inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float srgb2lin(uint8_t c) {
    float f = c / 255.0f;
    return f <= 0.04045f ? f / 12.92f : std::pow((f + 0.055f) / 1.055f, 2.4f);
}
inline uint8_t lin2srgb(float f) {
    f = clamp01(f);
    float s = f <= 0.0031308f ? f * 12.92f : 1.055f * std::pow(f, 1.0f / 2.4f) - 0.055f;
    return (uint8_t)std::lround(s * 255.0f);
}

// Multiply row-major 4x4 by (x,y,z,1); returns w in outW.
inline void mul44(const float m[16], float x, float y, float z, float out[3], float& outW) {
    out[0] = m[0] * x + m[4] * y + m[8]  * z + m[12];
    out[1] = m[1] * x + m[5] * y + m[9]  * z + m[13];
    out[2] = m[2] * x + m[6] * y + m[10] * z + m[14];
    outW   = m[3] * x + m[7] * y + m[11] * z + m[15];
}

}  // namespace

bool invert4x4(const float m[16], float inv[16]) {
    // Standard cofactor expansion (row-major).
    inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

    float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (std::fabs(det) < 1e-12f) return false;
    float id = 1.0f / det;
    for (int i = 0; i < 16; ++i) inv[i] *= id;
    return true;
}

bool unprojectPixel(const SceneStream& s, int x, int y, Vec3& outWorld) {
    if (!s.camera.valid || !s.depthAbs) return false;
    if (x < 0 || y < 0 || x >= s.width || y >= s.height) return false;

    static thread_local float cachedClip[16] = {};
    static thread_local float cachedInv[16] = {};
    static thread_local bool cacheOk = false;
    if (!cacheOk || std::memcmp(cachedClip, s.camera.clip, sizeof(cachedClip)) != 0) {
        std::memcpy(cachedClip, s.camera.clip, sizeof(cachedClip));
        cacheOk = invert4x4(s.camera.clip, cachedInv);
    }
    if (!cacheOk) return false;

    // DS rasteriser depth is z/w remapped to 0..1 (0xFFFFFF full scale).
    float zbuf = s.depthAbs->at(x, y);
    float ndcX = (x + 0.5f) / s.width * 2.0f - 1.0f;
    float ndcY = 1.0f - (y + 0.5f) / s.height * 2.0f;
    float ndcZ = zbuf * 2.0f - 1.0f;

    // World = inv(clip) * ndc, perspective divide. melonDS matrices are
    // row-vector convention (v' = v * M), so use the transposed multiply.
    const float* m = cachedInv;
    float wx = ndcX * m[0] + ndcY * m[4] + ndcZ * m[8]  + m[12];
    float wy = ndcX * m[1] + ndcY * m[5] + ndcZ * m[9]  + m[13];
    float wz = ndcX * m[2] + ndcY * m[6] + ndcZ * m[10] + m[14];
    float ww = ndcX * m[3] + ndcY * m[7] + ndcZ * m[11] + m[15];
    if (std::fabs(ww) < 1e-9f) return false;
    outWorld = Vec3{wx / ww, wy / ww, wz / ww};
    return true;
}

std::vector<ResolvedLight> resolveLights(const GameProfile& profile,
                                         const GameState& state,
                                         uint64_t frameIndex) {
    std::vector<ResolvedLight> out;
    for (const auto& l : profile.lights) {
        if (!l.maps.empty() && state.mapId >= 0 &&
            std::find(l.maps.begin(), l.maps.end(), state.mapId) == l.maps.end())
            continue;
        if (!l.times.empty() && !state.timeName.empty() &&
            std::find(l.times.begin(), l.times.end(), state.timeName) == l.times.end())
            continue;
        ResolvedLight r;
        r.anchor = &l;
        r.pos = Vec3{l.pos[0], l.pos[1], l.pos[2]};
        float lum = l.luminance;
        if (l.flicker > 0.0f) {
            // Deterministic per-frame flicker (two incommensurate sines).
            float t = (float)(frameIndex % 3600);
            float f = 0.5f + 0.5f * (0.6f * std::sin(t * 0.31f) + 0.4f * std::sin(t * 0.113f));
            lum *= 1.0f - l.flicker * 0.5f * f;
        }
        r.color = Vec3{l.color[0] * lum, l.color[1] * lum, l.color[2] * lum};
        r.range = l.range;
        out.push_back(r);
    }
    return out;
}

const SceneEnvironment* resolveEnvironment(const GameProfile& profile,
                                           const GameState& state) {
    const SceneEnvironment* best = nullptr;
    int bestScore = -1;
    for (const auto& e : profile.environments) {
        int score = 0;
        if (!e.maps.empty()) {
            if (state.mapId < 0 ||
                std::find(e.maps.begin(), e.maps.end(), state.mapId) == e.maps.end())
                continue;
            score += 2;
        }
        if (!e.times.empty()) {
            if (state.timeName.empty() ||
                std::find(e.times.begin(), e.times.end(), state.timeName) == e.times.end())
                continue;
            score += 1;
        }
        if (score > bestScore) { bestScore = score; best = &e; }
    }
    return best;
}

GameLitFrame renderGameLit(const Image& fb, const SceneStream& s,
                           const GameProfile& profile, const GameState& state,
                           const GameLightingParams& params) {
    GameLitFrame out;
    out.state = state;
    out.color = fb;
    const int W = fb.width, H = fb.height;
    if (W <= 0 || H <= 0) return out;

    out.activeLights = resolveLights(profile, state, s.frameIndex);
    const SceneEnvironment* env = resolveEnvironment(profile, state);
    SceneEnvironment defEnv;
    if (!env) env = &defEnv;

    const bool haveScene = (s.depthAbs != nullptr) && s.camera.valid &&
                           s.width == W && s.height == H;
    if (!haveScene) return out;  // honest fallback: untouched native frame

    // 1) World positions for every genuine 3D pixel.
    std::vector<Vec3> world((size_t)W * H);
    std::vector<uint8_t> lit((size_t)W * H, 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            uint8_t m = s.layerMask ? s.layerMask[i] : (uint8_t)LM_3D;
            if (!maskIs3D(m)) { ++out.uiPixels; continue; }  // UI exclusion
            if (unprojectPixel(s, x, y, world[i])) lit[i] = 1;
        }

    // 2) Normals from world-position gradients (central differences).
    std::vector<Vec3> normal((size_t)W * H, Vec3{0, 1, 0});
    auto worldAt = [&](int x, int y) -> const Vec3& {
        x = std::clamp(x, 0, W - 1); y = std::clamp(y, 0, H - 1);
        return world[(size_t)y * W + x];
    };
    auto litAt = [&](int x, int y) -> bool {
        x = std::clamp(x, 0, W - 1); y = std::clamp(y, 0, H - 1);
        return lit[(size_t)y * W + x] != 0;
    };
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            if (!lit[i]) continue;
            int xl = litAt(x - 1, y) ? x - 1 : x, xr = litAt(x + 1, y) ? x + 1 : x;
            int yu = litAt(x, y - 1) ? y - 1 : y, yd = litAt(x, y + 1) ? y + 1 : y;
            if (xl == xr || yu == yd) continue;
            Vec3 dx{worldAt(xr, y).x - worldAt(xl, y).x,
                    worldAt(xr, y).y - worldAt(xl, y).y,
                    worldAt(xr, y).z - worldAt(xl, y).z};
            Vec3 dy{worldAt(x, yd).x - worldAt(x, yu).x,
                    worldAt(x, yd).y - worldAt(x, yu).y,
                    worldAt(x, yd).z - worldAt(x, yu).z};
            Vec3 n{dy.y * dx.z - dy.z * dx.y,
                   dy.z * dx.x - dy.x * dx.z,
                   dy.x * dx.y - dy.y * dx.x};
            float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if (len > 1e-9f) normal[i] = Vec3{n.x / len, n.y / len, n.z / len};
        }

    // 3) Shade: albedo * (ambient + sum(lights)) in linear HDR.
    std::vector<Vec3> hdr((size_t)W * H);
    const float ambR = env->ambientColor[0] * env->ambientIntensity * params.ambientScale;
    const float ambG = env->ambientColor[1] * env->ambientIntensity * params.ambientScale;
    const float ambB = env->ambientColor[2] * env->ambientIntensity * params.ambientScale;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            const Color& c = fb.pixels[i];
            Vec3 alb{srgb2lin(c.r), srgb2lin(c.g), srgb2lin(c.b)};
            if (!lit[i]) { hdr[i] = alb; continue; }

            float lr = ambR, lg = ambG, lb = ambB;
            for (const auto& L : out.activeLights) {
                Vec3 d{L.pos.x - world[i].x, L.pos.y - world[i].y, L.pos.z - world[i].z};
                float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
                if (L.range > 0 && dist2 > L.range * L.range) continue;
                float dist = std::sqrt(dist2);
                // inverse-square with soft core (avoids the 1/0 singularity)
                float atten = 1.0f / (1.0f + dist2 * 0.05f);
                if (L.range > 0) {
                    float t = dist / L.range;         // smooth window to zero at range
                    float w = clamp01(1.0f - t * t * t * t);
                    atten *= w * w;
                }
                float ndl = 1.0f;
                if (params.normalStrength > 0 && dist > 1e-6f) {
                    const Vec3& n = normal[i];
                    float raw = (n.x * d.x + n.y * d.y + n.z * d.z) / dist;
                    // Half-lambert keeps DS low-poly facets from going pitch black.
                    float hl = clamp01(raw * 0.5f + 0.5f);
                    ndl = (1.0f - params.normalStrength) + params.normalStrength * hl * hl;
                }
                float k = atten * ndl * params.lightScale;
                lr += L.color.x * k;
                lg += L.color.y * k;
                lb += L.color.z * k;
            }
            if (params.debugLightsOnly) {
                hdr[i] = Vec3{lr - ambR, lg - ambG, lb - ambB};
            } else {
                hdr[i] = Vec3{alb.x * lr, alb.y * lg, alb.z * lb};
            }
            ++out.litPixels;
        }

    // 4) Scene-sourced bloom: threshold lit-scene luminance only (UI never
    // contributes), quarter-res separable blur, add back.
    const float bloomI = env->bloomIntensity * params.bloomScale;
    if (params.enableBloom && bloomI > 0.0f) {
        const int bw = W / 4, bh = H / 4;
        std::vector<Vec3> bright((size_t)bw * bh, Vec3{});
        const float th = env->bloomThreshold;
        for (int by = 0; by < bh; ++by)
            for (int bx = 0; bx < bw; ++bx) {
                Vec3 acc{}; int cnt = 0;
                for (int sy = 0; sy < 4; ++sy)
                    for (int sx = 0; sx < 4; ++sx) {
                        int x = bx * 4 + sx, y = by * 4 + sy;
                        size_t i = (size_t)y * W + x;
                        if (!lit[i]) continue;
                        const Vec3& h = hdr[i];
                        float lum = 0.2126f * h.x + 0.7152f * h.y + 0.0722f * h.z;
                        if (lum > th) {
                            float k = (lum - th) / std::max(lum, 1e-6f);
                            acc.x += h.x * k; acc.y += h.y * k; acc.z += h.z * k;
                        }
                        ++cnt;
                    }
                if (cnt > 0) bright[(size_t)by * bw + bx] =
                    Vec3{acc.x / cnt, acc.y / cnt, acc.z / cnt};
            }
        // two-pass 5-tap blur
        auto blur = [&](std::vector<Vec3>& src, bool horiz) {
            std::vector<Vec3> dst(src.size());
            const float k[5] = {0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f};
            for (int y = 0; y < bh; ++y)
                for (int x = 0; x < bw; ++x) {
                    Vec3 acc{};
                    for (int t = -2; t <= 2; ++t) {
                        int sx = horiz ? std::clamp(x + t, 0, bw - 1) : x;
                        int sy = horiz ? y : std::clamp(y + t, 0, bh - 1);
                        const Vec3& v = src[(size_t)sy * bw + sx];
                        acc.x += v.x * k[t + 2]; acc.y += v.y * k[t + 2]; acc.z += v.z * k[t + 2];
                    }
                    dst[(size_t)y * bw + x] = acc;
                }
            src.swap(dst);
        };
        blur(bright, true); blur(bright, false);
        blur(bright, true); blur(bright, false);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                size_t i = (size_t)y * W + x;
                if (!lit[i]) continue;  // bloom never bleeds onto UI pixels
                // bilinear sample of the quarter-res bloom
                float fx = (x + 0.5f) / 4.0f - 0.5f, fy = (y + 0.5f) / 4.0f - 0.5f;
                int x0 = std::clamp((int)fx, 0, bw - 1), y0 = std::clamp((int)fy, 0, bh - 1);
                int x1 = std::min(x0 + 1, bw - 1), y1 = std::min(y0 + 1, bh - 1);
                float tx = clamp01(fx - x0), ty = clamp01(fy - y0);
                auto S = [&](int xx, int yy) -> const Vec3& { return bright[(size_t)yy * bw + xx]; };
                Vec3 b{
                    ((S(x0,y0).x*(1-tx)+S(x1,y0).x*tx)*(1-ty) + (S(x0,y1).x*(1-tx)+S(x1,y1).x*tx)*ty),
                    ((S(x0,y0).y*(1-tx)+S(x1,y0).y*tx)*(1-ty) + (S(x0,y1).y*(1-tx)+S(x1,y1).y*tx)*ty),
                    ((S(x0,y0).z*(1-tx)+S(x1,y0).z*tx)*(1-ty) + (S(x0,y1).z*(1-tx)+S(x1,y1).z*tx)*ty)};
                hdr[i].x += b.x * bloomI;
                hdr[i].y += b.y * bloomI;
                hdr[i].z += b.z * bloomI;
            }
    }

    // 5) Exposure + extended-Reinhard tonemap, back to sRGB. UI pixels are
    // copied through byte-identical.
    const float ex = env->exposure * params.exposure;
    const float whitePt = 4.0f, wp2 = whitePt * whitePt;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            if (!lit[i]) { out.color.pixels[i] = fb.pixels[i]; continue; }
            Vec3 h{hdr[i].x * ex, hdr[i].y * ex, hdr[i].z * ex};
            float lum = 0.2126f * h.x + 0.7152f * h.y + 0.0722f * h.z;
            if (lum > 1e-6f) {
                float mapped = lum * (1.0f + lum / wp2) / (1.0f + lum);
                float k = mapped / lum;
                h.x *= k; h.y *= k; h.z *= k;
            }
            out.color.pixels[i] = Color{lin2srgb(h.x), lin2srgb(h.y), lin2srgb(h.z), 255};
        }
    return out;
}

}  // namespace prismatic
