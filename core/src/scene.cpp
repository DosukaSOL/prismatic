// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/scene.hpp"
#include "prismatic/composite.hpp"
#include <cmath>

namespace prismatic {

namespace {

struct Hit {
    bool hit = false;
    uint8_t kind = LK_Backdrop;
    Material mat = Material::Unknown;
    MaterialParams mp{};
    int objectId = 0;
    Color color{0, 0, 0, 0};
    float billboardT = 0.0f;  // 0 at feet, 1 at top (sprites)
};

int stableBgId(int worldTX, int worldTY, int layerIndex) {
    unsigned h = (unsigned)(worldTX * 73856093) ^ (unsigned)(worldTY * 19349663) ^
                 (unsigned)(layerIndex * 83492791);
    return (int)(h & 0x7fffffff);
}

bool sampleBg(const BackgroundLayer& bg, int sx, int sy, MaterialCache& cache, Hit& out) {
    if (!bg.enabled || bg.widthTiles == 0 || bg.heightTiles == 0) return false;
    int wpx = bg.widthTiles * kTileSize, hpx = bg.heightTiles * kTileSize;
    int wx = ((sx + bg.scrollX) % wpx + wpx) % wpx;
    int wy = ((sy + bg.scrollY) % hpx + hpx) % hpx;
    int tx = wx / kTileSize, ty = wy / kTileSize;
    int ix = wx % kTileSize, iy = wy % kTileSize;
    const TileRef& ref = bg.cell(tx, ty);
    if (ref.tileIndex >= bg.tileset.size()) return false;
    int px = ref.flipH ? (kTileSize - 1 - ix) : ix;
    int py = ref.flipV ? (kTileSize - 1 - iy) : iy;
    const Tile& tile = bg.tileset[ref.tileIndex];
    uint8_t idx = tile.at(px, py);
    if (idx == 0) return false;
    const Palette& pal = bg.palettes.empty() ? Palette{} :
        bg.palettes[ref.paletteBank < bg.palettes.size() ? ref.paletteBank : 0];
    if (idx >= pal.size() || pal[idx].a == 0) return false;
    const auto& entry = cache.lookup(tile, pal);
    out.hit = true;
    out.color = pal[idx];
    out.mat = entry.material;
    out.mp = paramsFor(entry.material);
    out.kind = (bg.priority >= 2) ? LK_Overhead : LK_Background;
    out.objectId = stableBgId(wx / kTileSize, wy / kTileSize, bg.index);
    out.billboardT = 0.0f;
    return true;
}

bool sampleSprite(const Sprite& sp, int sx, int sy, MaterialCache& cache, Hit& out) {
    if (!sp.enabled) return false;
    int lx = sx - sp.x, ly = sy - sp.y;
    if (lx < 0 || ly < 0 || lx >= sp.width || ly >= sp.height) return false;
    int fx = sp.flipH ? (sp.width - 1 - lx) : lx;
    int fy = sp.flipV ? (sp.height - 1 - ly) : ly;
    int tileIdx = (fy / kTileSize) * sp.tilesWide() + (fx / kTileSize);
    if (tileIdx < 0 || tileIdx >= (int)sp.tiles.size()) return false;
    const Tile& tile = sp.tiles[tileIdx];
    uint8_t idx = tile.at(fx % kTileSize, fy % kTileSize);
    if (idx == 0 || idx >= sp.palette.size() || sp.palette[idx].a == 0) return false;
    const auto& entry = cache.lookup(tile, sp.palette);
    Material m = entry.material;
    MaterialParams mp = paramsFor(m);
    mp.billboardable = true;  // sprites always stand up
    out.hit = true;
    out.color = sp.palette[idx];
    out.mat = m;
    out.mp = mp;
    out.kind = LK_Sprite;
    out.objectId = 1000000 + sp.id;
    out.billboardT = 1.0f - (float)ly / (float)std::max(1, sp.height - 1);
    return true;
}

// Resolve the front-most opaque surface at a pixel (mirrors compositor order).
Hit resolvePixel(const StructuredFrame& f, int x, int y, MaterialCache& cache) {
    Hit best;  // backdrop
    for (int prio = 0; prio < kMaxPriority; ++prio) {
        std::vector<const BackgroundLayer*> bgs;
        for (const auto& bg : f.backgrounds)
            if (bg.enabled && bg.priority == prio) bgs.push_back(&bg);
        std::sort(bgs.begin(), bgs.end(),
                  [](const BackgroundLayer* a, const BackgroundLayer* b) { return a->index > b->index; });
        for (const BackgroundLayer* bg : bgs) {
            Hit h; if (sampleBg(*bg, x, y, cache, h)) best = h;
        }
        std::vector<const Sprite*> sprites;
        for (const auto& sp : f.sprites)
            if (sp.enabled && sp.priority == prio) sprites.push_back(&sp);
        std::sort(sprites.begin(), sprites.end(),
                  [](const Sprite* a, const Sprite* b) { return a->id < b->id; });
        for (const Sprite* sp : sprites) {
            Hit h; if (sampleSprite(*sp, x, y, cache, h)) best = h;
        }
    }
    return best;
}

float luma(Color c) { return (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b) / 255.0f; }

}  // namespace

ReconstructedScene reconstructScene(const StructuredFrame& frame, MaterialCache& cache,
                                    const ReconstructOptions& opt) {
    const int W = frame.screenWidth, H = frame.screenHeight;
    ReconstructedScene s;
    s.width = W; s.height = H;
    s.albedo = Image(W, H, frame.backdrop);
    s.heightMap = FloatBuffer(W, H, 0.0f);
    s.emissive = FloatBuffer(W, H, 0.0f);
    s.depth = FloatBuffer(W, H, 1.0f);
    s.occupancy = FloatBuffer(W, H, 0.0f);
    s.roughness = FloatBuffer(W, H, 0.8f);
    s.normal.assign((size_t)W * H, Vec3{0, 0, 1});
    s.objectId.assign((size_t)W * H, 0);
    s.materialId.assign((size_t)W * H, (uint8_t)Material::Unknown);
    s.layerKind.assign((size_t)W * H, LK_Backdrop);

    FloatBuffer rawHeight(W, H, 0.0f);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            Hit h = resolvePixel(frame, x, y, cache);
            size_t i = (size_t)y * W + x;
            if (!h.hit) {
                s.albedo.at(x, y) = frame.backdrop;
                continue;
            }
            s.albedo.at(x, y) = h.color;
            s.occupancy.data[i] = 1.0f;
            s.materialId[i] = (uint8_t)h.mat;
            s.layerKind[i] = h.kind;
            s.objectId[i] = h.objectId;
            s.roughness.data[i] = h.mp.roughness;

            float hgt;
            if (h.kind == LK_Sprite) {
                hgt = 0.35f + 0.30f * h.billboardT;  // upright billboard
            } else if (h.kind == LK_Overhead) {
                hgt = std::max(0.5f, h.mp.heightScale);
            } else {
                hgt = h.mp.heightScale;  // flat for ground/path/water
            }
            rawHeight.at(x, y) = hgt * opt.heightScale;

            // Emissive: material-declared emitters, plus warm highlight glow.
            float e = 0.0f;
            if (h.mat == Material::Emissive)
                e = luma(h.color) * (h.mp.emissiveScale > 0 ? h.mp.emissiveScale : 1.0f);
            else if (luma(h.color) > 0.9f && h.color.r >= h.color.b)
                e = 0.25f;
            s.emissive.data[i] = e;
        }
    }

    // Edge-aware height smoothing (bevel) within same object id.
    FloatBuffer smoothH(W, H, 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            float sum = rawHeight.at(x, y); int cnt = 1;
            int id = s.objectId[i];
            const int off[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto& o : off) {
                int nx = x + o[0], ny = y + o[1];
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                if (s.objectId[(size_t)ny * W + nx] == id) { sum += rawHeight.at(nx, ny); ++cnt; }
            }
            smoothH.at(x, y) = sum / cnt;
        }
    s.heightMap = smoothH;

    // Normals from height gradient + edge-aware procedural bump from albedo.
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            int id = s.objectId[i];
            auto hAt = [&](int xx, int yy) {
                xx = clampi(xx, 0, W - 1); yy = clampi(yy, 0, H - 1);
                return s.heightMap.at(xx, yy);
            };
            float dhx = hAt(x + 1, y) - hAt(x - 1, y);
            float dhy = hAt(x, y + 1) - hAt(x, y - 1);
            // Procedural micro-bump, suppressed across object edges.
            auto lumAt = [&](int xx, int yy) {
                xx = clampi(xx, 0, W - 1); yy = clampi(yy, 0, H - 1);
                return luma(s.albedo.at(xx, yy));
            };
            auto sameObj = [&](int xx, int yy) {
                xx = clampi(xx, 0, W - 1); yy = clampi(yy, 0, H - 1);
                return s.objectId[(size_t)yy * W + xx] == id;
            };
            float bx = 0, by = 0;
            if (sameObj(x + 1, y) && sameObj(x - 1, y)) bx = lumAt(x + 1, y) - lumAt(x - 1, y);
            if (sameObj(x, y + 1) && sameObj(x, y - 1)) by = lumAt(x, y + 1) - lumAt(x, y - 1);
            MaterialParams mp = paramsFor((Material)s.materialId[i]);
            float ns = mp.normalStrength * opt.normalStrength;
            Vec3 n{-(dhx * 6.0f + bx * 0.8f * ns), -(dhy * 6.0f + by * 0.8f * ns), 1.0f};
            s.normal[i] = normalize(n);

            // Depth: lower on screen + taller = closer.
            s.depth.data[i] = 1.0f - 0.0007f * y - 0.25f * s.heightMap.at(x, y);
        }

    return s;
}

ReconstructedScene reconstructSceneFromImage(const Image& image, const ReconstructOptions& opt) {
    const int W = image.width, H = image.height;
    ReconstructedScene s;
    s.width = W; s.height = H;
    s.albedo = image;
    s.heightMap = FloatBuffer(W, H, 0.0f);
    s.emissive = FloatBuffer(W, H, 0.0f);
    s.depth = FloatBuffer(W, H, 1.0f);
    s.occupancy = FloatBuffer(W, H, 1.0f);      // a real frame fully covers the screen
    s.roughness = FloatBuffer(W, H, 0.7f);
    s.normal.assign((size_t)W * H, Vec3{0, 0, 1});
    s.objectId.assign((size_t)W * H, 0);
    s.materialId.assign((size_t)W * H, (uint8_t)Material::Unknown);
    s.layerKind.assign((size_t)W * H, (uint8_t)LK_Background);

    // Height proxy from perceptual luminance; emissive from a soft bright-pass so
    // in-game light sources (windows, lamps, sky) drive bloom without inventing art.
    FloatBuffer rawHeight(W, H, 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float L = luma(image.at(x, y));
            rawHeight.at(x, y) = L * opt.heightScale;
            float e = clampf((L - 0.80f) / 0.20f, 0.0f, 1.0f);
            s.emissive.data[(size_t)y * W + x] = e * L;
        }

    // Edge-preserving-ish smoothing of the height proxy (4-neighbour average).
    FloatBuffer smoothH(W, H, 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float sum = rawHeight.at(x, y); int cnt = 1;
            const int off[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto& o : off) {
                int nx = x + o[0], ny = y + o[1];
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                sum += rawHeight.at(nx, ny); ++cnt;
            }
            smoothH.at(x, y) = sum / cnt;
        }
    s.heightMap = smoothH;

    const MaterialParams mp = paramsFor(Material::Unknown);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            auto hAt = [&](int xx, int yy) {
                xx = clampi(xx, 0, W - 1); yy = clampi(yy, 0, H - 1);
                return s.heightMap.at(xx, yy);
            };
            float dhx = hAt(x + 1, y) - hAt(x - 1, y);
            float dhy = hAt(x, y + 1) - hAt(x, y - 1);
            float ns = mp.normalStrength * opt.normalStrength;
            Vec3 n{-(dhx * 6.0f) * ns, -(dhy * 6.0f) * ns, 1.0f};
            s.normal[i] = normalize(n);
            s.depth.data[i] = 1.0f - 0.0007f * y - 0.25f * s.heightMap.at(x, y);
        }

    return s;
}

}  // namespace prismatic
