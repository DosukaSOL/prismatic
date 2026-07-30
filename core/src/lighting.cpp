// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/lighting.hpp"
#include <cmath>

namespace prismatic {

Vec3 srgbToLinear(const Color& c) {
    auto f = [](uint8_t v) { float s = v / 255.0f; return std::pow(s, 2.2f); };
    return {f(c.r), f(c.g), f(c.b)};
}
Color linearToSrgb(const Vec3& v) {
    auto f = [](float s) { return (uint8_t)std::lround(clampf(std::pow(clampf(s, 0, 1), 1.0f / 2.2f), 0, 1) * 255.0f); };
    return Color{f(v.x), f(v.y), f(v.z), 255};
}

namespace {
Vec3 mul(const Vec3& a, const Vec3& b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 scl(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float len2(float x, float y) { return std::sqrt(x * x + y * y); }
}  // namespace

LitScene shadeScene(const ReconstructedScene& scene, const EnvLighting& env,
                    const Preset& preset, const LightingInput& lin) {
    const int W = scene.width, H = scene.height;
    LitScene out;
    out.width = W; out.height = H;
    out.hdr.assign((size_t)W * H, Vec3{0, 0, 0});
    out.lightOnly.assign((size_t)W * H, Vec3{0, 0, 0});
    out.ao = FloatBuffer(W, H, 1.0f);

    // ---- Screen-space contact shadow / AO from height field ----------------
    // March toward the light; if a taller pixel blocks the path, occlude.
    float lx = -env.sunDir.x, ly = -env.sunDir.y;  // toward light (screen)
    float ll = len2(lx, ly);
    if (ll > 1e-4f) { lx /= ll; ly /= ll; }
    const int kSteps = 10;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            float h0 = scene.heightMap.data[i];
            float occ = 0.0f;
            for (int s = 1; s <= kSteps; ++s) {
                int sx = x + (int)std::lround(lx * s);
                int sy = y + (int)std::lround(ly * s);
                if (sx < 0 || sy < 0 || sx >= W || sy >= H) break;
                float hs = scene.heightMap.at(sx, sy);
                float need = h0 + 0.06f * s;  // required blocker height at this step
                if (hs > need) occ = std::max(occ, (hs - need) * (1.0f - (float)s / (kSteps + 2)));
            }
            // Ambient occlusion from local height rise.
            float maxN = h0;
            const int off[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto& o : off) {
                int nx = clampi(x + o[0], 0, W - 1), ny = clampi(y + o[1], 0, H - 1);
                maxN = std::max(maxN, scene.heightMap.at(nx, ny));
            }
            float aoLocal = clampf((maxN - h0) * 0.8f, 0.0f, 0.4f);
            float shadow = clampf(occ * 1.5f, 0.0f, 1.0f) * preset.contactShadowStrength;
            out.ao.data[i] = clampf(1.0f - shadow - aoLocal, 0.15f, 1.0f);
        }
    }

    Vec3 toSun = normalize(scl(env.sunDir, -1.0f));
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            LayerKind kind = (LayerKind)scene.layerKind[i];
            if (kind == LK_Backdrop) {
                // Sky/backdrop: flat ambient sky tint.
                Vec3 sky = scl(env.ambientSky, env.ambientIntensity);
                out.hdr[i] = sky;
                out.lightOnly[i] = sky;
                continue;
            }
            Vec3 albedo = srgbToLinear(scene.albedo.at(x, y));
            const Vec3& N = scene.normal[i];
            float ao = out.ao.data[i];

            // Hemisphere ambient (up faces sky).
            float up = clampf(0.5f - 0.5f * N.y, 0.0f, 1.0f);
            Vec3 ambient = scl(add(scl(env.ambientGround, 1.0f - up), scl(env.ambientSky, up)),
                               env.ambientIntensity * preset.ambientStrength * ao);

            // Key light.
            float ndl = std::max(0.0f, dot(N, toSun));
            Vec3 sun = scl(env.sunColor, env.sunIntensity * preset.sunStrength * ndl * ao);

            // Point lights (priority: higher priority lights are added first and
            // slightly suppress lower ones to avoid additive blowouts).
            Vec3 pointSum{0, 0, 0};
            float dominated = 0.0f;
            // Sort-free: two passes by priority threshold is overkill; accumulate
            // with a soft cap driven by the strongest contributor.
            for (const Light& L : lin.lights) {
                if (L.type != Light::Point) continue;
                float dx = L.pos.x - x, dy = L.pos.y - y;
                float d = std::sqrt(dx * dx + dy * dy + (L.height * 40.0f) * (L.height * 40.0f));
                float atten = clampf(1.0f - d / L.radius, 0.0f, 1.0f);
                atten *= atten;
                if (atten <= 0) continue;
                Vec3 Ldir = normalize(Vec3{dx, dy, L.height * 40.0f});
                float ndl2 = std::max(0.0f, dot(N, Ldir));
                float w = 1.0f - dominated * 0.5f;
                pointSum = add(pointSum, scl(L.color, L.intensity * atten * ndl2 * w));
                dominated = std::max(dominated, atten);
            }

            Vec3 lightTerm = add(add(ambient, sun), pointSum);

            // Rim light on upright billboards (sprites): edges catch the light.
            if (kind == LK_Sprite) {
                float rim = std::pow(clampf(1.0f - N.z, 0.0f, 1.0f), 2.0f) * preset.rimStrength;
                lightTerm = add(lightTerm, scl(env.sunColor, rim * (0.4f + 0.6f * env.sunIntensity)));
            }

            // Simple specular (Blinn) modulated by roughness.
            float rough = scene.roughness.data[i];
            Vec3 H3 = normalize(add(toSun, Vec3{0, 0, 1}));
            float nh = std::max(0.0f, dot(N, H3));
            float shininess = lerpf(4.0f, 64.0f, 1.0f - rough);
            float spec = std::pow(nh, shininess) * (1.0f - rough) * env.sunIntensity * 0.4f * ndl;

            Vec3 color = add(mul(albedo, lightTerm), scl(env.sunColor, spec));

            // Emissive.
            float em = scene.emissive.data[i];
            if (em > 0) color = add(color, scl(albedo, em * 1.5f));

            // Highlight protection: soft-knee compress very bright albedo so the
            // original art's bright pixels keep detail instead of clipping.
            float hp = preset.highlightProtection;
            if (hp > 0) {
                auto knee = [hp](float c) {
                    float kneePt = lerpf(1.2f, 0.75f, hp);
                    if (c <= kneePt) return c;
                    float over = c - kneePt;
                    return kneePt + over / (1.0f + over);
                };
                color = {knee(color.x), knee(color.y), knee(color.z)};
            }

            out.hdr[i] = color;
            out.lightOnly[i] = lightTerm;
        }
    }
    return out;
}

}  // namespace prismatic
