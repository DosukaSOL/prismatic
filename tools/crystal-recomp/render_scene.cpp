// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — Crystal recomp scene renderer.
// Loads a .pvx scene blob (baked by tools/crystal-recomp/build_scene.py from the
// gitignored pokecrystal data) and renders it as a 2.5D voxel diorama, with an
// optional "shaders" pass (supersampled AA + soft AO + bloom + daytime grade +
// vignette). Also supports placing overworld sprite billboards (e.g. Suicune and
// the player) for scene shots.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "prismatic/png.hpp"
#include "prismatic/voxel_diorama.hpp"

using namespace prismatic;

namespace {

bool loadScene(const std::string& path, HeightfieldScene& scene) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[4];
    f.read(magic, 4);
    const bool v4 = std::memcmp(magic, "PVX4", 4) == 0;
    const bool v3 = std::memcmp(magic, "PVX3", 4) == 0;
    if (!v4 && !v3 && std::memcmp(magic, "PVX2", 4) != 0) return false;
    uint32_t hdr[5];
    f.read(reinterpret_cast<char*>(hdr), sizeof(hdr));
    const uint32_t mapW = hdr[0], mapH = hdr[1];
    scene.cell = (int)hdr[2];
    scene.cols = (int)hdr[3];
    scene.rows = (int)hdr[4];
    scene.classGrid.resize((size_t)scene.cols * scene.rows);
    f.read(reinterpret_cast<char*>(scene.classGrid.data()), scene.classGrid.size());
    if (v4) {  // ground-truth archetype grid follows the collision grid
        scene.archeGrid.resize((size_t)scene.cols * scene.rows);
        f.read(reinterpret_cast<char*>(scene.archeGrid.data()), scene.archeGrid.size());
    }
    scene.map = Image((int)mapW, (int)mapH, Color{0, 0, 0, 255});
    std::vector<uint8_t> rgba((size_t)mapW * mapH * 4);
    f.read(reinterpret_cast<char*>(rgba.data()), rgba.size());
    if (!f) return false;
    for (size_t i = 0; i < (size_t)mapW * mapH; ++i)
        scene.map.pixels[i] = Color{rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2], rgba[i * 4 + 3]};
    if (v3 || v4) {
        uint32_t nSprites = 0;
        f.read(reinterpret_cast<char*>(&nSprites), sizeof(nSprites));
        for (uint32_t s = 0; s < nSprites && f; ++s) {
            float pos[3];
            uint32_t wh[2];
            f.read(reinterpret_cast<char*>(pos), sizeof(pos));
            f.read(reinterpret_cast<char*>(wh), sizeof(wh));
            HeightfieldScene::Billboard b;
            b.tileX = pos[0]; b.tileY = pos[1]; b.scale = pos[2];
            b.img = Image((int)wh[0], (int)wh[1], Color{0, 0, 0, 0});
            std::vector<uint8_t> sp((size_t)wh[0] * wh[1] * 4);
            f.read(reinterpret_cast<char*>(sp.data()), sp.size());
            for (size_t i = 0; i < (size_t)wh[0] * wh[1]; ++i)
                b.img.pixels[i] = Color{sp[i * 4], sp[i * 4 + 1], sp[i * 4 + 2], sp[i * 4 + 3]};
            scene.sprites.push_back(std::move(b));
        }
    }
    if (v4) {  // explicit building footprints from the baker
        uint32_t nB = 0;
        f.read(reinterpret_cast<char*>(&nB), sizeof(nB));
        for (uint32_t i = 0; i < nB && f; ++i) {
            int32_t r[6];
            f.read(reinterpret_cast<char*>(r), sizeof(r));
            HeightfieldScene::Building b;
            b.cx0 = r[0]; b.cy0 = r[1]; b.cx1 = r[2]; b.cy1 = r[3];
            b.doorCx = r[4]; b.doorCy = r[5];
            scene.buildings.push_back(b);
        }
    }
    return true;
}

inline uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

// Box-downsample by an integer factor (clean supersampled AA — no FXAA blur).
Image downsample(const Image& src, int f) {
    if (f <= 1) return src;
    Image out(src.width / f, src.height / f, Color{0, 0, 0, 255});
    for (int y = 0; y < out.height; ++y)
        for (int x = 0; x < out.width; ++x) {
            int r = 0, g = 0, b = 0;
            for (int dy = 0; dy < f; ++dy)
                for (int dx = 0; dx < f; ++dx) {
                    const Color& c = src.at(x * f + dx, y * f + dy);
                    r += c.r; g += c.g; b += c.b;
                }
            const int n = f * f;
            out.at(x, y) = Color{clamp8(r / n), clamp8(g / n), clamp8(b / n), 255};
        }
    return out;
}

// A tasteful daytime post-process for the diorama: soft bloom on highlights, a
// warm grade with lifted-blue shadows, gentle saturation, and a vignette.
void applyDioramaShaders(Image& img) {
    const int W = img.width, H = img.height;
    auto lum = [](const Color& c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; };

    // 1) Bloom: threshold bright pixels, blur, add back.
    std::vector<float> br(W * H * 3, 0.0f);
    for (int i = 0; i < W * H; ++i) {
        const Color& c = img.pixels[i];
        float l = lum(c);
        float t = l > 180.0f ? (l - 180.0f) / 75.0f : 0.0f;  // 0..1 above threshold
        if (t > 1.0f) t = 1.0f;
        br[i * 3] = c.r * t; br[i * 3 + 1] = c.g * t; br[i * 3 + 2] = c.b * t;
    }
    auto blur = [&](int radius) {
        std::vector<float> tmp(W * H * 3, 0.0f);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int ch = 0; ch < 3; ++ch) {
                    float s = 0; int n = 0;
                    for (int dx = -radius; dx <= radius; ++dx) {
                        int xx = x + dx;
                        if (xx < 0 || xx >= W) continue;
                        s += br[(y * W + xx) * 3 + ch]; ++n;
                    }
                    tmp[(y * W + x) * 3 + ch] = s / std::max(1, n);
                }
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int ch = 0; ch < 3; ++ch) {
                    float s = 0; int n = 0;
                    for (int dy = -radius; dy <= radius; ++dy) {
                        int yy = y + dy;
                        if (yy < 0 || yy >= H) continue;
                        s += tmp[(yy * W + x) * 3 + ch]; ++n;
                    }
                    br[(y * W + x) * 3 + ch] = s / std::max(1, n);
                }
    };
    blur(3);

    const float cx = W * 0.5f, cy = H * 0.5f;
    const float maxd = std::sqrt(cx * cx + cy * cy);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int i = y * W + x;
            float r = img.pixels[i].r, g = img.pixels[i].g, b = img.pixels[i].b;

            // 2) Add bloom.
            r += br[i * 3] * 0.55f; g += br[i * 3 + 1] * 0.55f; b += br[i * 3 + 2] * 0.55f;

            // 3) Saturation + warm daytime grade.
            float l = 0.299f * r + 0.587f * g + 0.114f * b;
            const float sat = 1.14f;
            r = l + (r - l) * sat; g = l + (g - l) * sat; b = l + (b - l) * sat;
            r *= 1.05f; g *= 1.02f; b *= 0.97f;       // warm highlights
            r += 2.0f; g += 3.0f; b += 8.0f;          // lifted cool shadows

            // 4) Vignette.
            float dx = (x - cx), dy = (y - cy);
            float d = std::sqrt(dx * dx + dy * dy) / maxd;
            float vig = 1.0f - 0.28f * d * d;
            r *= vig; g *= vig; b *= vig;

            img.pixels[i] = Color{clamp8((int)std::lround(r)), clamp8((int)std::lround(g)),
                                  clamp8((int)std::lround(b)), 255};
        }
}

}  // namespace

int main(int argc, char** argv) {
    std::string scenePath, outPath = "scene";
    int scale = 3;
    bool shaded = false, plain = true;
    int rBx = -1, rBy = 0, rBw = 0, rBh = 0;  // crop region in block coords
    float tilt = 50.0f, focal = 1.05f, zstep = 7.0f;
    float curve = 0.0f, height = 1.15f, zoom = 1.0f;
    bool haveTilt = false, haveFocal = false, haveZ = false;
    bool haveCurve = false, haveHeight = false, haveZoom = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--scene") scenePath = next();
        else if (a == "--out") outPath = next();
        else if (a == "--scale") scale = std::atoi(next());
        else if (a == "--shader") shaded = true;
        else if (a == "--no-plain") plain = false;
        else if (a == "--tilt") { tilt = (float)std::atof(next()); haveTilt = true; }
        else if (a == "--focal") { focal = (float)std::atof(next()); haveFocal = true; }
        else if (a == "--zstep") { zstep = (float)std::atof(next()); haveZ = true; }
        else if (a == "--curve") { curve = (float)std::atof(next()); haveCurve = true; }
        else if (a == "--height") { height = (float)std::atof(next()); haveHeight = true; }
        else if (a == "--zoom") { zoom = (float)std::atof(next()); haveZoom = true; }
        else if (a == "--region") {  // BX,BY,BW,BH in block units (32px / 4 tiles)
            std::string s = next();
            std::sscanf(s.c_str(), "%d,%d,%d,%d", &rBx, &rBy, &rBw, &rBh);
        }
    }
    if (scenePath.empty()) { std::printf("usage: --scene X.pvx --out PREFIX [--scale N] [--shader] [--region BX,BY,BW,BH] [--tilt DEG] [--focal F] [--height S] [--curve K] [--zoom Z]\n"); return 2; }

    HeightfieldScene scene;
    if (!loadScene(scenePath, scene)) { std::printf("failed to load %s\n", scenePath.c_str()); return 1; }
    if (rBx >= 0 && rBw > 0 && rBh > 0) {   // blocks -> 8px tiles (x4)
        scene.cropX = rBx * 4; scene.cropY = rBy * 4;
        scene.cropW = rBw * 4; scene.cropH = rBh * 4;
    }
    std::printf("  scene %dx%d cells, map %dx%d px, %zu sprite(s)\n",
                scene.cols, scene.rows, scene.map.width, scene.map.height, scene.sprites.size());

    VoxelOptions opt;
    opt.scale = scale;
    if (haveTilt) opt.tiltDeg = tilt;
    if (haveFocal) opt.focal = focal;
    if (haveZ) opt.zStep = zstep;
    if (haveCurve) opt.curve = curve;
    if (haveHeight) opt.heightScale = height;
    if (haveZoom) opt.zoom = zoom;
    std::printf("  tilt %.0f deg  focal %.2f  height %.2f  curve %.2f  zoom %.2f  scale %d\n",
                opt.tiltDeg, opt.focal, opt.heightScale, opt.curve, opt.zoom, scale);

    if (plain) {
        Image flat = renderVoxelHeightfield(scene, opt);
        std::string p = outPath + ".png";
        std::printf("  wrote %s (%dx%d) %s\n", p.c_str(), flat.width, flat.height, writePng(p, flat) ? "ok" : "FAIL");
    }
    if (shaded) {
        VoxelOptions ss = opt;
        ss.scale = scale * 2;                       // supersample for clean AA
        Image big = renderVoxelHeightfield(scene, ss);
        Image img = downsample(big, 2);
        applyDioramaShaders(img);
        std::string p = outPath + "_shaded.png";
        std::printf("  wrote %s (%dx%d) %s\n", p.c_str(), img.width, img.height, writePng(p, img) ? "ok" : "FAIL");
    }
    return 0;
}
