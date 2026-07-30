// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/renderer_software.hpp"
#include <cmath>

namespace prismatic {

namespace {
Vec3 mul(const Vec3& a, const Vec3& b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 scl(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float luma(const Vec3& v) { return 0.2126f * v.x + 0.7152f * v.y + 0.0722f * v.z; }

Vec3 acesFilmic(const Vec3& x) {
    auto f = [](float c) {
        return clampf((c * (2.51f * c + 0.03f)) / (c * (2.43f * c + 0.59f) + 0.14f), 0.0f, 1.0f);
    };
    return {f(x.x), f(x.y), f(x.z)};
}

// Separable box blur on a Vec3 field.
std::vector<Vec3> boxBlur(const std::vector<Vec3>& src, int W, int H, int r) {
    std::vector<Vec3> tmp(src.size()), out(src.size());
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            Vec3 s{0, 0, 0}; int n = 0;
            for (int dx = -r; dx <= r; ++dx) {
                int xx = clampi(x + dx, 0, W - 1);
                s = add(s, src[(size_t)y * W + xx]); ++n;
            }
            tmp[(size_t)y * W + x] = scl(s, 1.0f / n);
        }
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            Vec3 s{0, 0, 0}; int n = 0;
            for (int dy = -r; dy <= r; ++dy) {
                int yy = clampi(y + dy, 0, H - 1);
                s = add(s, tmp[(size_t)yy * W + x]); ++n;
            }
            out[(size_t)y * W + x] = scl(s, 1.0f / n);
        }
    return out;
}

Color idColor(int id) {
    if (id == 0) return Color{20, 20, 26};
    unsigned h = (unsigned)id * 2654435761u;
    return Color{(uint8_t)(80 + (h & 0x7F)), (uint8_t)(80 + ((h >> 8) & 0x7F)),
                 (uint8_t)(80 + ((h >> 16) & 0x7F)), 255};
}

Image upscaleNearest(const Image& src, int f) {
    if (f <= 1) return src;
    Image out(src.width * f, src.height * f);
    for (int y = 0; y < out.height; ++y)
        for (int x = 0; x < out.width; ++x)
            out.at(x, y) = src.at(x / f, y / f);
    return out;
}
Image upscaleBilinear(const Image& src, int f) {
    if (f <= 1) return src;
    Image out(src.width * f, src.height * f);
    for (int y = 0; y < out.height; ++y)
        for (int x = 0; x < out.width; ++x) {
            float sx = (x + 0.5f) / f - 0.5f, sy = (y + 0.5f) / f - 0.5f;
            int x0 = clampi((int)std::floor(sx), 0, src.width - 1);
            int y0 = clampi((int)std::floor(sy), 0, src.height - 1);
            int x1 = clampi(x0 + 1, 0, src.width - 1), y1 = clampi(y0 + 1, 0, src.height - 1);
            float fx = sx - x0, fy = sy - y0;
            auto lerpC = [](Color a, Color b, float t) {
                return Color{(uint8_t)std::lround(a.r + (b.r - a.r) * t),
                             (uint8_t)std::lround(a.g + (b.g - a.g) * t),
                             (uint8_t)std::lround(a.b + (b.b - a.b) * t), 255};
            };
            Color top = lerpC(src.at(x0, y0), src.at(x1, y0), fx);
            Color bot = lerpC(src.at(x0, y1), src.at(x1, y1), fx);
            out.at(x, y) = lerpC(top, bot, fy);
        }
    return out;
}
}  // namespace

static RenderResult renderFromScene(const ReconstructedScene& scene, const Image& nativeImage,
                                    const RenderRequest& req) {
    const Preset& preset = req.preset;
    RenderResult res;
    res.nativeImage = nativeImage;
    const int W = res.nativeImage.width, H = res.nativeImage.height;
    res.width = W; res.height = H;

    EnvLighting env = computeEnvLighting(req.environment);
    LitScene lit = shadeScene(scene, env, preset, req.lights);

    CameraConfig cam = CameraConfig::fromPreset(preset);
    FloatBuffer offY = computeParallaxOffsetY(scene.heightMap, cam);

    // Warp + post to native-res enhanced.
    std::vector<Vec3> hdr((size_t)W * H);
    FloatBuffer warpDepth(W, H, 1.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int srcY = clampi(y - (int)std::lround(offY.at(x, y)), 0, H - 1);
            hdr[(size_t)y * W + x] = lit.hdr[(size_t)srcY * W + x];
            warpDepth.at(x, y) = scene.depth.at(x, srcY);
        }

    // Bloom.
    std::vector<Vec3> bright((size_t)W * H, Vec3{0, 0, 0});
    if (preset.bloomIntensity > 0) {
        for (size_t i = 0; i < hdr.size(); ++i) {
            float l = luma(hdr[i]);
            if (l > preset.bloomThreshold) bright[i] = scl(hdr[i], (l - preset.bloomThreshold) / (l + 1e-4f));
        }
        bright = boxBlur(bright, W, H, 3);
        bright = boxBlur(bright, W, H, 6);
    }

    float exposure = preset.exposure * env.exposureBias;
    Vec3 grade = mul(preset.grade, env.gradeMul);
    float density = env.fogDensity * preset.fogScale;

    Image enhN(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            Vec3 c = scl(hdr[i], exposure);
            c = add(c, scl(bright[i], preset.bloomIntensity * env.bloomBias));
            c = acesFilmic(c);  // -> ~display linear 0..1
            // Contrast around mid-grey.
            auto ct = [&](float v) { return clampf((v - 0.5f) * preset.contrast + 0.5f, 0.0f, 1.0f); };
            c = {ct(c.x), ct(c.y), ct(c.z)};
            // Saturation.
            float l = luma(c);
            c = {lerpf(l, c.x, preset.saturation), lerpf(l, c.y, preset.saturation), lerpf(l, c.z, preset.saturation)};
            // Grade.
            c = mul(c, grade);
            // Fog by depth (farther = more fog).
            if (density > 0) {
                float fogF = clampf((1.0f - warpDepth.at(x, y)) * density * 4.0f, 0.0f, 0.85f);
                c = add(scl(c, 1.0f - fogF), scl(env.fogColor, fogF));
            }
            // Vignette.
            if (preset.vignette > 0) {
                float nx = (x / (float)W - 0.5f) * 2.0f, ny = (y / (float)H - 0.5f) * 2.0f;
                float v = 1.0f - preset.vignette * clampf(nx * nx + ny * ny, 0.0f, 1.0f);
                c = scl(c, v);
            }
            enhN.at(x, y) = linearToSrgb(c);
        }
    res.enhancedNative = enhN;

    int f = req.upscale > 0 ? req.upscale : preset.upscale;
    res.enhanced = (preset.pixelSharpness >= 0.5f) ? upscaleNearest(enhN, f) : upscaleBilinear(enhN, f);

    if (req.emitDebug) {
        // Depth view (normalized).
        float dmin = 1e9f, dmax = -1e9f;
        for (float d : scene.depth.data) { dmin = std::min(dmin, d); dmax = std::max(dmax, d); }
        float drange = std::max(1e-4f, dmax - dmin);
        res.depthView = Image(W, H);
        res.normalView = Image(W, H);
        res.objectIdView = Image(W, H);
        res.emissiveView = Image(W, H);
        res.lightView = Image(W, H);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                size_t i = (size_t)y * W + x;
                uint8_t dv = (uint8_t)std::lround((scene.depth.data[i] - dmin) / drange * 255.0f);
                res.depthView.at(x, y) = Color{dv, dv, dv, 255};
                const Vec3& n = scene.normal[i];
                res.normalView.at(x, y) = Color{
                    (uint8_t)std::lround((n.x * 0.5f + 0.5f) * 255.0f),
                    (uint8_t)std::lround((n.y * 0.5f + 0.5f) * 255.0f),
                    (uint8_t)std::lround((n.z * 0.5f + 0.5f) * 255.0f), 255};
                res.objectIdView.at(x, y) = idColor(scene.objectId[i]);
                uint8_t ev = (uint8_t)std::lround(clampf(scene.emissive.data[i], 0, 1) * 255.0f);
                res.emissiveView.at(x, y) = Color{ev, (uint8_t)(ev * 3 / 4), 0, 255};
                res.lightView.at(x, y) = linearToSrgb(acesFilmic(lit.lightOnly[i]));
            }
    }
    return res;
}

RenderResult renderStructured(const StructuredFrame& frame, MaterialCache& cache,
                              const RenderRequest& req) {
    const Preset& preset = req.preset;
    ReconstructOptions ro;
    ro.heightScale = preset.heightScale;
    ro.normalStrength = preset.normalStrength;
    Image nativeImage = compositeNative(frame);
    ReconstructedScene scene = reconstructScene(frame, cache, ro);
    return renderFromScene(scene, nativeImage, req);
}

RenderResult renderFramebuffer(const Image& image, MaterialCache& /*cache*/,
                               const RenderRequest& req) {
    const Preset& preset = req.preset;
    ReconstructOptions ro;
    ro.heightScale = preset.heightScale;
    ro.normalStrength = preset.normalStrength;
    ReconstructedScene scene = reconstructSceneFromImage(image, ro);
    return renderFromScene(scene, image, req);
}

}  // namespace prismatic
