// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — tile-extruded voxel diorama renderer (implementation).
#include "prismatic/voxel_diorama.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace prismatic {
namespace {

inline Color shade(Color c, float m) {
    return Color{(uint8_t)clampi((int)std::lround(c.r * m), 0, 255),
                 (uint8_t)clampi((int)std::lround(c.g * m), 0, 255),
                 (uint8_t)clampi((int)std::lround(c.b * m), 0, 255), c.a};
}

inline float luma(Color c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; }

// Resolve a BG cell's pixel (with flip) to a palette color.
inline Color bgPixel(const BackgroundLayer& bg, const TileRef& cell, int px, int py) {
    int sx = cell.flipH ? 7 - px : px;
    int sy = cell.flipV ? 7 - py : py;
    if (cell.tileIndex >= bg.tileset.size()) return Color{0, 0, 0, 255};
    uint8_t idx = bg.tileset[cell.tileIndex].at(sx, sy);
    if (cell.paletteBank >= bg.palettes.size() || idx >= bg.palettes[cell.paletteBank].size())
        return Color{0, 0, 0, 255};
    return bg.palettes[cell.paletteBank][idx];
}

// Average color of a tile cell (used for side faces + classification).
Color cellAverage(const BackgroundLayer& bg, const TileRef& cell) {
    int r = 0, g = 0, b = 0;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            Color c = bgPixel(bg, cell, x, y);
            r += c.r; g += c.g; b += c.b;
        }
    return Color{(uint8_t)(r / 64), (uint8_t)(g / 64), (uint8_t)(b / 64), 255};
}

// A crude "vertical edge energy" — how much the tile changes top-to-bottom.
// Tiles that are busy vertically (walls, furniture edges, tree trunks) read as
// taller than flat, uniform ground tiles.
float cellStructure(const BackgroundLayer& bg, const TileRef& cell) {
    float e = 0;
    for (int x = 0; x < 8; ++x)
        for (int y = 1; y < 8; ++y)
            e += std::fabs(luma(bgPixel(bg, cell, x, y)) - luma(bgPixel(bg, cell, x, y - 1)));
    return e / 56.0f;  // avg per-pixel delta
}

}  // namespace

Image renderVoxelDiorama(const StructuredFrame& frame, const VoxelOptions& opt) {
    const int scale = std::max(1, opt.scale);
    const int SW = frame.screenWidth > 0 ? frame.screenWidth : 160;
    const int SH = frame.screenHeight > 0 ? frame.screenHeight : 144;
    const int outW = SW * scale;
    const int outH = SH * scale;

    Image img(outW, outH, opt.sky);
    if (frame.backgrounds.empty()) return img;
    const BackgroundLayer& bg = frame.backgrounds[0];
    if (bg.map.empty() || bg.tileset.empty() || bg.widthTiles <= 0) return img;

    const int cols = SW / 8;   // visible tile columns (20)
    const int rows = SH / 8;   // visible tile rows (18)
    const int fracX = ((bg.scrollX % 8) + 8) % 8;
    const int fracY = ((bg.scrollY % 8) + 8) % 8;
    const int startTX = bg.scrollX >> 3;
    const int startTY = bg.scrollY >> 3;

    auto cellAt = [&](int c, int r) -> const TileRef& {
        int wx = ((startTX + c) % bg.widthTiles + bg.widthTiles) % bg.widthTiles;
        int wy = ((startTY + r) % bg.heightTiles + bg.heightTiles) % bg.heightTiles;
        return bg.cell(wx, wy);
    };

    // ---- Height classification -------------------------------------------
    // Ground = the most common tiles (floors/grass/paths cover the map). Object
    // tiles rise; the busiest/darkest ones rise most (walls, trees, buildings).
    std::unordered_map<uint16_t, int> freq;
    for (int r = -2; r < rows + 1; ++r)
        for (int c = 0; c < cols; ++c) freq[cellAt(c, r).tileIndex]++;
    int maxFreq = 1;
    for (auto& kv : freq) maxFreq = std::max(maxFreq, kv.second);

    auto heightOf = [&](const TileRef& cell) -> int {
        int f = freq.count(cell.tileIndex) ? freq[cell.tileIndex] : 0;
        // Common tiles are ground.
        if (f * 100 >= maxFreq * 55) return 0;
        float structure = cellStructure(bg, cell);
        Color avg = cellAverage(bg, cell);
        float dark = 1.0f - luma(avg) / 255.0f;
        int h = 1;
        if (structure > 0.16f || dark > 0.5f) h = 2;
        if (structure > 0.28f && dark > 0.45f) h = 3;
        return std::min(h, opt.maxHeight);
    };

    // Projection helpers (oblique / cabinet — crisp, no perspective warp).
    const float TW = 8.0f * scale;              // tile width in diorama px
    const float TD = 8.0f * scale * opt.pitch;  // foreshortened tile depth
    const float ZH = opt.heightUnit * scale / 3.0f;  // px per height level (scaled)
    const float originY = outH * 0.30f;         // push near edge toward the bottom

    auto groundY = [&](float ny) { return originY + ny * scale * opt.pitch; };

    // Soft contact shadow buffer (accumulated, then composited under nothing —
    // we just darken the ground before drawing raised faces over it).
    // ---- Draw back (top) to front (bottom) --------------------------------
    for (int r = -2; r < rows; ++r) {
        float rowShade = 1.0f;
        if (opt.backShade < 1.0f) {
            float t = (float)(rows - r) / (float)(rows + 2);  // 1 far .. 0 near
            rowShade = 1.0f - (1.0f - opt.backShade) * t;
        }
        for (int c = 0; c < cols; ++c) {
            const TileRef& cell = cellAt(c, r);
            int h = heightOf(cell);

            float nx0 = (float)(c * 8 - fracX);
            float ny0 = (float)(r * 8 - fracY);
            float x0 = nx0 * scale;
            float baseTopY = groundY(ny0);            // ground-level top edge of this footprint
            float topFaceTopY = baseTopY - h * ZH;    // raised top face
            float footBottomY = baseTopY + TD;        // ground footprint bottom

            int ix0 = (int)std::lround(x0);
            int ixw = (int)std::lround(TW);

            // Side face (front) — from top face bottom down to footprint bottom.
            if (h > 0 && opt.sideShade < 1.0f) {
                int sideTop = (int)std::lround(topFaceTopY + TD);
                int sideBot = (int)std::lround(footBottomY);
                for (int dx = 0; dx < ixw; ++dx) {
                    int X = ix0 + dx;
                    if (X < 0 || X >= outW) continue;
                    int px = std::min(7, dx * 8 / std::max(1, ixw));
                    Color col = shade(bgPixel(bg, cell, px, 7), opt.sideShade * rowShade);
                    for (int Y = sideTop; Y < sideBot; ++Y)
                        if (Y >= 0 && Y < outH) img.at(X, Y) = col;
                }
            }

            // Top face — the actual 8x8 tile pixels, foreshortened.
            int tfTop = (int)std::lround(topFaceTopY);
            int tfH = (int)std::lround(TD);
            for (int dy = 0; dy < tfH; ++dy) {
                int Y = tfTop + dy;
                if (Y < 0 || Y >= outH) continue;
                int py = std::min(7, dy * 8 / std::max(1, tfH));
                for (int dx = 0; dx < ixw; ++dx) {
                    int X = ix0 + dx;
                    if (X < 0 || X >= outW) continue;
                    int px = std::min(7, dx * 8 / std::max(1, ixw));
                    Color col = bgPixel(bg, cell, px, py);
                    if (rowShade < 1.0f) col = shade(col, rowShade);
                    img.at(X, Y) = col;
                }
            }
        }
    }

    // ---- Sprites as upright billboards ------------------------------------
    if (opt.billboardSprites) {
        // Draw sprites sorted by ground row so nearer ones overlap farther ones.
        std::vector<const Sprite*> order;
        order.reserve(frame.sprites.size());
        for (const auto& s : frame.sprites) order.push_back(&s);
        std::sort(order.begin(), order.end(),
                  [](const Sprite* a, const Sprite* b) { return (a->y + a->height) < (b->y + b->height); });

        for (const Sprite* sp : order) {
            float groundNy = (float)(sp->y + sp->height);   // feet contact (native px)
            float feetY = groundY(groundNy);
            float leftX = (float)sp->x * scale;

            // Drop shadow ellipse at feet.
            if (opt.dropShadows) {
                int cxp = (int)std::lround(leftX + sp->width * scale * 0.5f);
                int cyp = (int)std::lround(feetY);
                int rxp = (int)std::lround(sp->width * scale * 0.5f);
                int ryp = std::max(2, (int)std::lround(sp->width * scale * 0.22f));
                for (int dy = -ryp; dy <= ryp; ++dy)
                    for (int dx = -rxp; dx <= rxp; ++dx) {
                        float e = (float)(dx * dx) / (rxp * rxp) + (float)(dy * dy) / (ryp * ryp);
                        if (e > 1.0f) continue;
                        int X = cxp + dx, Y = cyp + dy;
                        if (X >= 0 && X < outW && Y >= 0 && Y < outH)
                            img.at(X, Y) = shade(img.at(X, Y), 0.55f);
                    }
            }

            // Upright billboard: sprite stands vertically, feet on the ground.
            for (int py = 0; py < sp->height; ++py) {
                for (int px = 0; px < sp->width; ++px) {
                    int sx = sp->flipH ? sp->width - 1 - px : px;
                    int sy = sp->flipV ? sp->height - 1 - py : py;
                    int tileRow = sy / 8;
                    if ((size_t)tileRow >= sp->tiles.size()) continue;
                    uint8_t idx = sp->tiles[tileRow].at(sx & 7, sy & 7);
                    if (idx == 0 || idx >= sp->palette.size()) continue;
                    Color col = sp->palette[idx];
                    int X0 = (int)std::lround(leftX + px * scale);
                    int Y0 = (int)std::lround(feetY - (sp->height - py) * scale);
                    for (int yy = 0; yy < scale; ++yy)
                        for (int xx = 0; xx < scale; ++xx) {
                            int X = X0 + xx, Y = Y0 + yy;
                            if (X >= 0 && X < outW && Y >= 0 && Y < outH) img.at(X, Y) = col;
                        }
                }
            }
        }
    }

    return img;
}

// ---- Data-driven heightfield renderer (Crystal recomp) --------------------
// gen1recomp-style HD-2D "tilt": the flat map is a ground plane rotated about
// the horizontal axis through the view centre and viewed through a perspective
// camera (far rows recede & shrink, near rows come closer & grow); tall tiles
// (walls/buildings, trees) stand up as voxel columns and sprites as billboards.
namespace {

// World-pixel heights per archetype (cell = 16px), matching the voxel-mod scale
// where a wall/tree is ~one cell tall, not an arbitrary stack of levels.
constexpr float H_WATER = -2.0f;   // water recesses so shorelines show a lip
constexpr float H_LEDGE = 6.0f;    // ledge / step-down
constexpr float H_TREE = 15.0f;    // tree canopy dome peak (rounded, ~cell-wide)
constexpr float H_ROCK = 7.0f;     // boulder lump peak (squat)
constexpr float H_SIGN = 13.0f;    // sign board top (upright post + panel)

// Per-cell geometry archetype (the voxel mod's Structures classification).
enum class Arch : uint8_t { Flat, Water, Grass, Ledge, Foliage, Rock, Sign, Building };

struct P2 { float x, y; };
struct GP { float x, y, s; };  // projected ground point + perspective scale
// A building footprint (in cells) + the door cell so the facade recesses there.
struct Region { int cx0, cy0, cx1, cy1; int doorCx, doorCy; };

// Average colour of a map cell + whether it reads as foliage. Foliage is judged
// by the FRACTION of green-dominant pixels (robust to brown trunks / dark tree
// outlines that would drag a simple average toward grey).
inline Color cellArt(const Image& map, int X0, int Y0, int cell, bool& green) {
    long r = 0, g = 0, b = 0; int n = 0, greenPx = 0;
    for (int y = 0; y < cell; ++y) {
        int my = Y0 + y; if (my < 0 || my >= map.height) continue;
        for (int x = 0; x < cell; ++x) {
            int mx = X0 + x; if (mx < 0 || mx >= map.width) continue;
            const Color& c = map.at(mx, my); r += c.r; g += c.g; b += c.b; ++n;
            if (c.g > c.r + 4 && c.g > c.b + 4) ++greenPx;  // leafy pixel
        }
    }
    if (n == 0) { green = false; return Color{0, 0, 0, 255}; }
    const int avgR = (int)(r / n), avgG = (int)(g / n), avgB = (int)(b / n);
    // A real canopy is BOTH mostly leafy pixels AND solidly green on average —
    // the second test rejects grey props (signs, boulders) whose speckle trips
    // the per-pixel leaf test but whose mean colour is near-neutral.
    green = (float)greenPx / (float)n > 0.22f && avgG > avgR + 16 && avgG > avgB + 16;
    return Color{(uint8_t)avgR, (uint8_t)avgG, (uint8_t)avgB, 255};
}

// gen1recomp Tilt.groundPoint: rotate the flat plane about its horizontal centre
// axis and view through a pinhole (d = focal * viewH).
inline GP projGround(float fx, float fy, float VW, float VH,
                     float sinA, float cosA, float F) {
    const float u = fx - VW * 0.5f;
    const float w = fy - VH * 0.5f;
    const float d = F * VH;
    float denom = d - w * sinA;
    if (denom < d * 0.15f) denom = d * 0.15f;  // clamp at the horizon
    const float s = d / denom;
    return {VW * 0.5f + u * s, VH * 0.5f + w * cosA * s, s};
}

// Affine textured triangle fill (nearest-sampled, painter's overwrite). Samples
// tex at (tox+u, toy+v), u/v in tile-local pixels; shd multiplies rgb.
inline void fillTexTri(Image& img, const Image& tex, int tox, int toy, int tw, int th,
                       P2 a, P2 b, P2 c, P2 ua, P2 ub, P2 uc, float shd) {
    int minX = (int)std::floor(std::min({a.x, b.x, c.x}));
    int maxX = (int)std::ceil(std::max({a.x, b.x, c.x}));
    int minY = (int)std::floor(std::min({a.y, b.y, c.y}));
    int maxY = (int)std::ceil(std::max({a.y, b.y, c.y}));
    minX = std::max(0, minX); minY = std::max(0, minY);
    maxX = std::min(img.width - 1, maxX); maxY = std::min(img.height - 1, maxY);
    const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (std::fabs(area) < 1e-5f) return;
    const float inv = 1.0f / area;
    for (int y = minY; y <= maxY; ++y) {
        const float py = y + 0.5f;
        for (int x = minX; x <= maxX; ++x) {
            const float px = x + 0.5f;
            const float w0 = ((b.x - px) * (c.y - py) - (c.x - px) * (b.y - py)) * inv;
            const float w1 = ((c.x - px) * (a.y - py) - (a.x - px) * (c.y - py)) * inv;
            const float w2 = 1.0f - w0 - w1;
            if (w0 < -0.001f || w1 < -0.001f || w2 < -0.001f) continue;
            float u = w0 * ua.x + w1 * ub.x + w2 * uc.x;
            float v = w0 * ua.y + w1 * ub.y + w2 * uc.y;
            const int su = std::clamp((int)u, 0, tw - 1);
            const int sv = std::clamp((int)v, 0, th - 1);
            const int sx = std::clamp(tox + su, 0, tex.width - 1);
            const int sy = std::clamp(toy + sv, 0, tex.height - 1);
            Color col = tex.at(sx, sy);
            if (shd != 1.0f) col = shade(col, shd);
            img.at(x, y) = Color{col.r, col.g, col.b, 255};
        }
    }
}

}  // namespace

Image renderVoxelHeightfield(const HeightfieldScene& scene, const VoxelOptions& opt) {
    const Image& map = scene.map;
    const int tilesX = map.width / 8;
    const int tilesY = map.height / 8;
    if (tilesX <= 0 || tilesY <= 0) return Image(1, 1, opt.sky);

    // Optional crop window (in 8px-tile units) → the zoomed framing.
    const int tx0 = std::clamp(scene.cropW > 0 ? scene.cropX : 0, 0, tilesX);
    const int ty0 = std::clamp(scene.cropH > 0 ? scene.cropY : 0, 0, tilesY);
    const int tx1 = std::clamp(scene.cropW > 0 ? scene.cropX + scene.cropW : tilesX, tx0, tilesX);
    const int ty1 = std::clamp(scene.cropH > 0 ? scene.cropY + scene.cropH : tilesY, ty0, tilesY);
    const int nX = tx1 - tx0, nY = ty1 - ty0;
    if (nX <= 0 || nY <= 0) return Image(1, 1, opt.sky);

    const float VW = nX * 8.0f, VH = nY * 8.0f;
    const float a = opt.tiltDeg * 3.14159265f / 180.0f;
    const float sinA = std::sin(a), cosA = std::cos(a);
    const float F = std::max(0.2f, opt.focal);
    const float outScale = std::max(1.0f, (float)opt.scale) * std::max(0.25f, opt.zoom);
    const float hScale = std::max(0.05f, opt.heightScale);
    const float curveK = opt.curve;
    const float cxc = VW * 0.5f, cyc = VH * 0.5f;
    const int cell = std::max(1, scene.cell);

    auto classAt = [&](int tx, int ty) -> uint8_t {
        int cx = (tx * 8) / cell;
        int cy = (ty * 8) / cell;
        if (cx < 0 || cy < 0 || cx >= scene.cols || cy >= scene.rows) return 0;
        return scene.classGrid[cy * scene.cols + cx];
    };

    // Project a local flat point (0..VW, 0..VH) raised by hpx WORLD pixels, with
    // the optional Animal-Crossing world curve (far-from-focus vertices sag).
    auto proj = [&](float lx, float ly, float hpx) -> GP {
        GP g = projGround(lx, ly, VW, VH, sinA, cosA, F);
        if (curveK != 0.0f) {
            const float dx = lx - cxc, dy = ly - cyc;
            g.y += curveK * (dx * dx + dy * dy) / VH * g.s;
        }
        g.y -= hpx * hScale * g.s;
        return g;
    };

    // Fit the output to the projected extent (ground corners + tallest voxel).
    const float maxRise = std::max(H_TREE, 52.0f);
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (int j = 0; j <= nY; ++j)
        for (int i = 0; i <= nX; ++i) {
            GP g = proj(i * 8.0f, j * 8.0f, 0.0f);
            minX = std::min(minX, g.x); maxX = std::max(maxX, g.x);
            maxY = std::max(maxY, g.y); minY = std::min(minY, g.y);
            minY = std::min(minY, proj(i * 8.0f, j * 8.0f, maxRise).y);
        }
    const float pad = 10.0f, spriteHead = 24.0f;  // room for billboards up top
    minY -= spriteHead;
    const int outW = (int)std::ceil((maxX - minX + 2 * pad) * outScale);
    const int outH = (int)std::ceil((maxY - minY + 2 * pad) * outScale);
    Image img(std::max(1, outW), std::max(1, outH), opt.sky);

    auto toPx = [&](GP g) -> P2 {
        return {(g.x - minX + pad) * outScale, (g.y - minY + pad) * outScale};
    };

    // Contact-shadow ellipse under a standing element.
    auto contactShadow = [&](float cx, float cy, float rx, float ry, float str) {
        int y0 = (int)std::floor(cy - ry), y1 = (int)std::ceil(cy + ry);
        int x0 = (int)std::floor(cx - rx), x1 = (int)std::ceil(cx + rx);
        for (int Y = std::max(0, y0); Y <= std::min(img.height - 1, y1); ++Y)
            for (int X = std::max(0, x0); X <= std::min(img.width - 1, x1); ++X) {
                float nx = (X - cx) / rx, ny = (Y - cy) / ry;
                float dd = nx * nx + ny * ny;
                if (dd > 1.0f) continue;
                float al = str * (1.0f - dd);
                Color& p = img.at(X, Y);
                p = Color{(uint8_t)(p.r * (1 - al)), (uint8_t)(p.g * (1 - al)),
                          (uint8_t)(p.b * (1 - al)), 255};
            }
    };
    // Grounding shadow from a footprint's near edge (in local coords).
    auto groundShadow = [&](float lxa, float lya, float lxb, float lyb, float str, float ryf) {
        P2 pL = toPx(proj(lxa, lyb, 0)), pR = toPx(proj(lxb, lyb, 0));
        float gx = (pL.x + pR.x) * 0.5f, gy = (pL.y + pR.y) * 0.5f;
        float rx = 0.5f * std::hypot(pR.x - pL.x, pR.y - pL.y) * 1.05f;
        contactShadow(gx, gy, std::max(2.0f, rx), std::max(2.0f, rx * ryf), str);
        (void)lya;
    };

    // ---- Per-cell archetype classification (Structures.lua analog) ----------
    // Buildings are found by their DOOR cells and grown into the connected,
    // non-green wall block, bounded in size — so a house is never merged with the
    // forest border (which would flatten it into domes) and the trees beside a
    // house are never swallowed by a box. Every remaining tall Wall cell becomes
    // its own dome: green → tree, non-green → rock.
    const int cCols = scene.cols, cRows = scene.rows;
    std::vector<uint8_t> arche((size_t)cCols * cRows, (uint8_t)Arch::Flat);
    std::vector<int> regionOf((size_t)cCols * cRows, -1);
    std::vector<Region> buildings;
    if (!scene.archeGrid.empty()) {
        // Ground truth from the baker: per-cell archetype + explicit building
        // footprints (from warps + tileset palette + bg-event signs). No pixel
        // guessing, so trees/signs/grass/houses never bleed into each other.
        for (size_t i = 0; i < arche.size() && i < scene.archeGrid.size(); ++i)
            arche[i] = scene.archeGrid[i];
        for (const auto& b : scene.buildings)
            buildings.push_back({b.cx0, b.cy0, b.cx1, b.cy1, b.doorCx, b.doorCy});
    } else {
        auto cls = [&](int cx, int cy) -> uint8_t {
            if (cx < 0 || cy < 0 || cx >= cCols || cy >= cRows) return 255;
            return scene.classGrid[cy * cCols + cx];
        };
        std::vector<uint8_t> greenCell((size_t)cCols * cRows, 0);
        std::vector<Color> avgCell((size_t)cCols * cRows);
        for (int cy = 0; cy < cRows; ++cy)
            for (int cx = 0; cx < cCols; ++cx) {
                bool g = false;
                avgCell[cy * cCols + cx] = cellArt(map, cx * cell, cy * cell, cell, g);
                greenCell[cy * cCols + cx] = g ? 1 : 0;
            }
        // Base archetypes straight from the collision class.
        for (int cy = 0; cy < cRows; ++cy)
            for (int cx = 0; cx < cCols; ++cx)
                switch ((TerrainClass)cls(cx, cy)) {
                    case TerrainClass::Water: arche[cy * cCols + cx] = (uint8_t)Arch::Water; break;
                    case TerrainClass::Ledge: arche[cy * cCols + cx] = (uint8_t)Arch::Ledge; break;
                    case TerrainClass::Grass: arche[cy * cCols + cx] = (uint8_t)Arch::Grass; break;
                    case TerrainClass::Tree:  arche[cy * cCols + cx] = (uint8_t)Arch::Foliage; break;
                    default: break;
                }
        auto isWall = [&](int cx, int cy) {
            uint8_t c = cls(cx, cy);
            return c == (uint8_t)TerrainClass::Wall || c == (uint8_t)TerrainClass::Door;
        };
        // Door-anchored building detection: a real house is a SMALL, ISOLATED,
        // compact block of non-green wall reached from its door. A door embedded
        // in a big tree/cliff border (a route or cave entrance) floods into a
        // huge component and is rejected — so the border stays trees/rocks and is
        // never carved into a fake "stone-roof" building.
        std::vector<uint8_t> claimed((size_t)cCols * cRows, 0);
        std::vector<uint8_t> seen((size_t)cCols * cRows, 0);
        struct IC { int x, y; };
        for (int cy = 0; cy < cRows; ++cy)
            for (int cx = 0; cx < cCols; ++cx) {
                if (cls(cx, cy) != (uint8_t)TerrainClass::Door || seen[cy * cCols + cx]) continue;
                std::vector<IC> stack{{cx, cy}}, cells;
                seen[cy * cCols + cx] = 1;
                int x0 = cx, y0 = cy, x1 = cx, y1 = cy, wallCount = 0;
                bool tooBig = false;
                while (!stack.empty()) {
                    IC p = stack.back(); stack.pop_back();
                    cells.push_back(p);
                    if (cls(p.x, p.y) == (uint8_t)TerrainClass::Wall) ++wallCount;
                    x0 = std::min(x0, p.x); y0 = std::min(y0, p.y);
                    x1 = std::max(x1, p.x); y1 = std::max(y1, p.y);
                    if ((int)cells.size() > 40) { tooBig = true; break; }   // border/cave
                    const int dx4[4] = {1, -1, 0, 0}, dy4[4] = {0, 0, -1, 1};
                    for (int k = 0; k < 4; ++k) {
                        int nx = p.x + dx4[k], ny = p.y + dy4[k];
                        if (nx < 0 || ny < 0 || nx >= cCols || ny >= cRows) continue;
                        int ni = ny * cCols + nx;
                        if (seen[ni]) continue;
                        bool w = cls(nx, ny) == (uint8_t)TerrainClass::Wall && !greenCell[ni];
                        bool dr = cls(nx, ny) == (uint8_t)TerrainClass::Door;
                        if (!w && !dr) continue;
                        seen[ni] = 1;
                        stack.push_back({nx, ny});
                    }
                }
                const int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
                const bool isBuilding = !tooBig && wallCount >= 3 &&
                                        (int)cells.size() <= 16 && bw <= 7 && bh <= 7;
                if (!isBuilding) continue;   // leave these cells for the dome pass
                int rid = (int)buildings.size();
                buildings.push_back({x0, y0, x1, y1, cx, cy});
                for (int yy = y0; yy <= y1; ++yy)          // claim the whole footprint
                    for (int xx = x0; xx <= x1; ++xx) {    // so nothing pokes through the roof
                        int i = yy * cCols + xx;
                        claimed[i] = 1;
                        arche[i] = (uint8_t)Arch::Building;
                        regionOf[i] = rid;
                    }
            }
        // Every unclaimed Wall cell = its own prop. Green → little tree dome;
        // a non-green cell standing alone (no wall/door neighbour) is a sign or
        // post; non-green cells that touch other walls are boulders.
        for (int cy = 0; cy < cRows; ++cy)
            for (int cx = 0; cx < cCols; ++cx) {
                int i = cy * cCols + cx;
                if (claimed[i] || cls(cx, cy) != (uint8_t)TerrainClass::Wall) continue;
                if (greenCell[i]) { arche[i] = (uint8_t)Arch::Foliage; continue; }
                bool lone = !isWall(cx - 1, cy) && !isWall(cx + 1, cy) &&
                            !isWall(cx, cy - 1) && !isWall(cx, cy + 1);
                arche[i] = lone ? (uint8_t)Arch::Sign : (uint8_t)Arch::Rock;
            }
    }

    // ---- Object drawers -----------------------------------------------------
    // A rounded per-cell dome (the "cylinder" archetype): a leafy tree ball or a
    // low boulder lump, built as CRISP voxel columns — the source pixel art rides
    // the top faces untouched, side faces are flat-shaded (no smearing blur).
    auto drawDome = [&](int ccx, int ccy, bool tree, float rowShade) {
        const float X0 = ccx * cell, Y0 = ccy * cell;
        const float lx0 = X0 - tx0 * 8, ly0 = Y0 - ty0 * 8;
        constexpr int SUB = 8;
        const float step = (float)cell / SUB;
        const float Htop = tree ? H_TREE : H_ROCK;
        const float ex = tree ? 0.5f : 0.9f;   // hemisphere vs flatter lump
        float hc[SUB][SUB];
        for (int j = 0; j < SUB; ++j)
            for (int i = 0; i < SUB; ++i) {
                float u = (i + 0.5f) / SUB * 2 - 1, v = (j + 0.5f) / SUB * 2 - 1;
                float r2 = u * u + v * v;
                hc[i][j] = r2 >= 1.0f ? 0.0f : Htop * std::pow(1.0f - r2, ex);
            }
        groundShadow(lx0, ly0, lx0 + cell, ly0 + cell, 0.30f, 0.42f);
        const float topS = rowShade, frontS = 0.70f * rowShade, sideS = 0.58f * rowShade;
        const float sf = (float)std::max(1, (int)step);
        for (int j = 0; j < SUB; ++j)          // north → south (painter's order)
            for (int i = 0; i < SUB; ++i) {
                float h = hc[i][j];
                if (h < 1.0f) continue;        // outside the canopy: reveal ground
                float ax = lx0 + i * step, bx = ax + step;
                float ay = ly0 + j * step, by = ay + step;
                int tox = (int)(X0 + i * step), toy = (int)(Y0 + j * step);
                int sw = (int)sf;
                P2 T00 = toPx(proj(ax, ay, h)), T10 = toPx(proj(bx, ay, h));
                P2 T11 = toPx(proj(bx, by, h)), T01 = toPx(proj(ax, by, h));
                fillTexTri(img, map, tox, toy, sw, sw, T00, T10, T11, {0, 0}, {sf, 0}, {sf, sf}, topS);
                fillTexTri(img, map, tox, toy, sw, sw, T00, T11, T01, {0, 0}, {sf, sf}, {0, sf}, topS);
                float hs = (j + 1 < SUB) ? hc[i][j + 1] : 0.0f;   // south (front) face
                if (hs < h - 0.4f) {
                    P2 b0 = toPx(proj(ax, by, hs)), b1 = toPx(proj(bx, by, hs));
                    fillTexTri(img, map, tox, toy, 1, 1, b0, b1, T11, {0, 0}, {0, 0}, {0, 0}, frontS);
                    fillTexTri(img, map, tox, toy, 1, 1, b0, T11, T01, {0, 0}, {0, 0}, {0, 0}, frontS);
                }
                float he = (i + 1 < SUB) ? hc[i + 1][j] : 0.0f;   // east face
                if (he < h - 0.4f) {
                    P2 b0 = toPx(proj(bx, ay, he)), b1 = toPx(proj(bx, by, he));
                    fillTexTri(img, map, tox, toy, 1, 1, b0, b1, T11, {0, 0}, {0, 0}, {0, 0}, sideS);
                    fillTexTri(img, map, tox, toy, 1, 1, b0, T11, T10, {0, 0}, {0, 0}, {0, 0}, sideS);
                }
                float hw = (i - 1 >= 0) ? hc[i - 1][j] : 0.0f;    // west face
                if (hw < h - 0.4f) {
                    P2 b0 = toPx(proj(ax, ay, hw)), b1 = toPx(proj(ax, by, hw));
                    fillTexTri(img, map, tox, toy, 1, 1, b0, b1, T01, {0, 0}, {0, 0}, {0, 0}, sideS);
                    fillTexTri(img, map, tox, toy, 1, 1, b0, T01, T00, {0, 0}, {0, 0}, {0, 0}, sideS);
                }
            }
    };

    // A short raised ledge/step (the "top" archetype): art on top + a front lip.
    auto drawTopBox = [&](int ccx, int ccy, float H, float rowShade) {
        const float X0 = ccx * cell, Y0 = ccy * cell;
        const float lx0 = X0 - tx0 * 8, ly0 = Y0 - ty0 * 8, lx1 = lx0 + cell, ly1 = ly0 + cell;
        P2 t00 = toPx(proj(lx0, ly0, H)), t10 = toPx(proj(lx1, ly0, H));
        P2 t11 = toPx(proj(lx1, ly1, H)), t01 = toPx(proj(lx0, ly1, H));
        P2 g01 = toPx(proj(lx0, ly1, 0)), g11 = toPx(proj(lx1, ly1, 0));
        float fs = opt.sideShade * rowShade, cf = (float)cell;
        fillTexTri(img, map, (int)X0, (int)Y0, cell, cell, g01, g11, t11, {0, cf - 1}, {cf, cf - 1}, {cf, 0}, fs);
        fillTexTri(img, map, (int)X0, (int)Y0, cell, cell, g01, t11, t01, {0, cf - 1}, {cf, 0}, {0, 0}, fs);
        fillTexTri(img, map, (int)X0, (int)Y0, cell, cell, t00, t10, t11, {0, 0}, {cf, 0}, {cf, cf}, rowShade);
        fillTexTri(img, map, (int)X0, (int)Y0, cell, cell, t00, t11, t01, {0, 0}, {cf, cf}, {0, cf}, rowShade);
    };

    // Tall grass: a scatter of short upright blades, each bottom-anchored to the
    // ground so it adds texture without floating (the old raised box did float).
    auto drawGrass = [&](int ccx, int ccy, float rowShade) {
        const float X0 = ccx * cell, Y0 = ccy * cell;
        const float lx0 = X0 - tx0 * 8, ly0 = Y0 - ty0 * 8;
        static const float bx[7] = {3, 7, 11, 14, 5, 9, 12};
        static const float bz[7] = {12, 7, 13, 9, 4, 10, 5};
        static const float bh[7] = {6, 7, 5, 6, 7, 5, 6};
        for (int k = 0; k < 7; ++k) {                 // south → north within the cell
            float x = lx0 + bx[k], z = ly0 + bz[k], h = bh[k];
            float s = (0.82f + 0.10f * (k & 1)) * rowShade;    // slight blade variety
            P2 l = toPx(proj(x - 1.2f, z, 0.0f)), r = toPx(proj(x + 1.2f, z, 0.0f));
            P2 tip = toPx(proj(x + 0.4f, z, h));
            fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, l, r, tip, {0, 0}, {0, 0}, {0, 0}, s);
        }
    };

    // A sign / post (the "sign" archetype): a slim upright panel on a short post,
    // the panel wearing the tile art so route signs read as signs, not lumps.
    auto drawSign = [&](int ccx, int ccy, float rowShade) {
        const float X0 = ccx * cell, Y0 = ccy * cell;
        const float lx0 = X0 - tx0 * 8, ly0 = Y0 - ty0 * 8;
        const float mx = lx0 + cell * 0.5f, my = ly0 + cell * 0.62f;  // stand mid-front
        const float halfW = cell * 0.34f, dep = 2.0f;                 // panel half-width, depth
        const float postH = H_SIGN * 0.42f;                           // post height
        groundShadow(mx - halfW, my - dep, mx + halfW, my + dep, 0.34f, 0.5f);
        // Post: a slim dark pillar from the ground to the panel underside.
        {
            const float pw = 2.0f;
            P2 f0 = toPx(proj(mx - pw, my + dep, 0.0f)),   f1 = toPx(proj(mx + pw, my + dep, 0.0f));
            P2 f2 = toPx(proj(mx + pw, my + dep, postH)),  f3 = toPx(proj(mx - pw, my + dep, postH));
            fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, f0, f1, f2, {0, 0}, {0, 0}, {0, 0}, 0.32f * rowShade);
            fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, f0, f2, f3, {0, 0}, {0, 0}, {0, 0}, 0.32f * rowShade);
        }
        // Panel: an upright board wearing the tile art (bright south face) plus a
        // thin shaded east edge and a top cap for real thickness.
        const float b0 = postH, b1 = H_SIGN, uw = (float)cell, uv1 = (float)cell * 0.7f;
        P2 s00 = toPx(proj(mx - halfW, my + dep, b1)), s10 = toPx(proj(mx + halfW, my + dep, b1));
        P2 s11 = toPx(proj(mx + halfW, my + dep, b0)), s01 = toPx(proj(mx - halfW, my + dep, b0));
        fillTexTri(img, map, (int)X0, (int)Y0, cell, cell, s00, s10, s11, {0, 0}, {uw, 0}, {uw, uv1}, rowShade);
        fillTexTri(img, map, (int)X0, (int)Y0, cell, cell, s00, s11, s01, {0, 0}, {uw, uv1}, {0, uv1}, rowShade);
        P2 e0 = toPx(proj(mx + halfW, my + dep, b1)), e1 = toPx(proj(mx + halfW, my - dep, b1));
        P2 e2 = toPx(proj(mx + halfW, my - dep, b0)), e3 = toPx(proj(mx + halfW, my + dep, b0));
        fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, e0, e1, e2, {0, 0}, {0, 0}, {0, 0}, 0.5f * rowShade);
        fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, e0, e2, e3, {0, 0}, {0, 0}, {0, 0}, 0.5f * rowShade);
        P2 c0 = toPx(proj(mx - halfW, my - dep, b1)), c1 = toPx(proj(mx + halfW, my - dep, b1));
        P2 c2 = toPx(proj(mx + halfW, my + dep, b1)), c3 = toPx(proj(mx - halfW, my + dep, b1));
        fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, c0, c1, c2, {0, 0}, {0, 0}, {0, 0}, 0.82f * rowShade);
        fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, c0, c2, c3, {0, 0}, {0, 0}, {0, 0}, 0.82f * rowShade);
    };

    // A building: a real little house. Tall folded facade (door/windows aligned),
    // a gable roof with an overhanging eave — the eave has visible THICKNESS and
    // casts a shadow band on the wall — a bright sun-side slope vs a dark
    // shade-side slope, and a recessed door. These are the cues that read as 3D.
    auto drawBuilding = [&](const Region& rg, float rowShade) {
        const float X0 = rg.cx0 * cell, Y0 = rg.cy0 * cell;
        const float X1 = (rg.cx1 + 1) * cell, Y1 = (rg.cy1 + 1) * cell;
        const float lx0 = X0 - tx0 * 8, ly0 = Y0 - ty0 * 8;
        const float lx1 = X1 - tx0 * 8, ly1 = Y1 - ty0 * 8;
        const float W = X1 - X0, D = Y1 - Y0;
        const int rowsN = std::max(1, (int)std::lround(D / (float)cell));
        const float Hw = clampf(rowsN * 12.0f, 15.0f, 30.0f);   // wall height (world px)
        const float Hr = clampf(W * 0.34f, 11.0f, 21.0f);       // ridge rise above eaves (steeper gable)
        const float eave = 4.5f, slab = 5.0f, mid = D * 0.5f;   // deeper overhang + thicker fascia
        const int roofArtRows = std::clamp((int)std::lround(D * 0.5f), 6, (int)D - 4);
        const int wallArtRows = std::max(4, (int)D - roofArtRows);
        auto roofTop = [&](float dy) {                          // gable, ridge E-W
            float t = 1.0f - std::fabs(dy - mid) / std::max(1.0f, mid);
            return Hw + Hr * t;
        };
        const float SS = 4.0f;
        // Grounding AO — a soft shadow hugging the whole footprint so the house
        // reads as ROOTED to the ground (fixes the "floating" look), plus a
        // stronger directional spill to the SE (sun from the NW).
        {
            P2 fl = toPx(proj(lx0 - 1, ly1 + 2, 0)), fr = toPx(proj(lx1 + 3, ly1 + 2, 0));
            P2 bc = toPx(proj((lx0 + lx1) * 0.5f, ly0 - 1, 0));
            float gx = (fl.x + fr.x) * 0.5f, gy = (fl.y + fr.y) * 0.5f;
            float rx = 0.5f * std::hypot(fr.x - fl.x, fr.y - fl.y) * 1.14f;
            float ry = std::max(5.0f, std::fabs(gy - bc.y) * 0.62f);
            contactShadow(gx, gy, std::max(5.0f, rx), ry, 0.30f);                              // wrap
            contactShadow(gx + rx * 0.16f, gy + ry * 0.28f, rx * 0.74f, ry * 0.52f, 0.26f);    // SE spill
        }
        // North (back) wall — mostly hidden; closes the silhouette, darkest.
        {
            P2 bl = toPx(proj(lx0, ly0, 0)), br = toPx(proj(lx1, ly0, 0));
            P2 tr = toPx(proj(lx1, ly0, Hw)), tl = toPx(proj(lx0, ly0, Hw));
            float s = 0.42f * rowShade;
            fillTexTri(img, map, (int)X0, (int)Y0, (int)W, 1, bl, br, tr, {0, 0}, {W, 0}, {W, 0}, s);
            fillTexTri(img, map, (int)X0, (int)Y0, (int)W, 1, bl, tr, tl, {0, 0}, {W, 0}, {0, 0}, s);
        }
        // E/W side walls, rising all the way to the roofline (fills the flanks).
        auto side = [&](bool east) {
            float lx = east ? lx1 : lx0;
            int sampleX = east ? (int)(X1 - 1) : (int)X0;
            float s = (east ? 0.56f : 0.66f) * rowShade;
            for (float dy = 0; dy < D; dy += SS) {
                float dy2 = std::min(D, dy + SS);
                P2 g1 = toPx(proj(lx, ly0 + dy, 0)), g2 = toPx(proj(lx, ly0 + dy2, 0));
                P2 r2 = toPx(proj(lx, ly0 + dy2, roofTop(dy2))), r1 = toPx(proj(lx, ly0 + dy, roofTop(dy)));
                fillTexTri(img, map, sampleX, (int)Y0, 1, (int)D, g1, g2, r2, {0, dy}, {0, dy2}, {0, dy2}, s);
                fillTexTri(img, map, sampleX, (int)Y0, 1, (int)D, g1, r2, r1, {0, dy}, {0, dy2}, {0, dy}, s);
            }
        };
        side(false); side(true);
        // Roof: two slopes off the E-W ridge, overhanging by 'eave' on every side.
        // North slope darker (shade), south slope brighter (sun). Texture = roof art.
        auto roofQuad = [&](float dyA, float dyB, float shd) {
            float hA = (dyA <= 0 ? Hw : roofTop(std::min(dyA, D)));
            float hB = (dyB >= D ? Hw : roofTop(std::max(dyB, 0.0f)));
            float vA = std::clamp(dyA, 0.0f, D) / std::max(1.0f, D) * roofArtRows;
            float vB = std::clamp(dyB, 0.0f, D) / std::max(1.0f, D) * roofArtRows;
            for (float dx = -eave; dx < W + eave; dx += SS) {
                float dx2 = std::min(W + eave, dx + SS);
                float uA = std::clamp(dx, 0.0f, W), uB = std::clamp(dx2, 0.0f, W);
                P2 A = toPx(proj(lx0 + dx, ly0 + dyA, hA)), B = toPx(proj(lx0 + dx2, ly0 + dyA, hA));
                P2 C = toPx(proj(lx0 + dx2, ly0 + dyB, hB)), Dd = toPx(proj(lx0 + dx, ly0 + dyB, hB));
                fillTexTri(img, map, (int)X0, (int)Y0, (int)W, roofArtRows, A, B, C, {uA, vA}, {uB, vA}, {uB, vB}, shd);
                fillTexTri(img, map, (int)X0, (int)Y0, (int)W, roofArtRows, A, C, Dd, {uA, vA}, {uB, vB}, {uA, vB}, shd);
            }
        };
        for (float dy = -eave; dy < mid; dy += SS) roofQuad(dy, std::min(mid, dy + SS), 0.60f * rowShade);   // shade slope (back)
        for (float dy = mid; dy < D + eave; dy += SS) roofQuad(dy, std::min(D + eave, dy + SS), 1.02f * rowShade);  // sun slope (front)
        // Crisp lit ridge cap along the E-W peak so the gable reads as a hard edge
        // (the single strongest depth cue) — a bright thin strip riding the ridge.
        {
            const float hR = Hw + Hr;
            P2 p0 = toPx(proj(lx0 - eave, ly0 + mid - 0.9f, hR)), p1 = toPx(proj(lx1 + eave, ly0 + mid - 0.9f, hR));
            P2 p2 = toPx(proj(lx1 + eave, ly0 + mid + 0.9f, hR)), p3 = toPx(proj(lx0 - eave, ly0 + mid + 0.9f, hR));
            fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, p0, p1, p2, {0, 0}, {0, 0}, {0, 0}, 1.22f * rowShade);
            fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, p0, p2, p3, {0, 0}, {0, 0}, {0, 0}, 1.22f * rowShade);
        }
        // South (front) facade — folds the wall/door art upright, full brightness.
        {
            int toy = (int)(Y0 + roofArtRows);
            P2 bl = toPx(proj(lx0, ly1, 0)), br = toPx(proj(lx1, ly1, 0));
            P2 tr = toPx(proj(lx1, ly1, Hw)), tl = toPx(proj(lx0, ly1, Hw));
            float wa = (float)wallArtRows, s = 1.0f * rowShade;
            fillTexTri(img, map, (int)X0, toy, (int)W, wallArtRows, bl, br, tr, {0, wa}, {W, wa}, {W, 0}, s);
            fillTexTri(img, map, (int)X0, toy, (int)W, wallArtRows, bl, tr, tl, {0, wa}, {W, 0}, {0, 0}, s);
        }
        // Eave shadow the overhang casts across the top of the facade (AO band).
        {
            P2 a0 = toPx(proj(lx0, ly1, Hw)), a1 = toPx(proj(lx1, ly1, Hw));
            P2 b0 = toPx(proj(lx1, ly1, Hw - 4.0f)), b1 = toPx(proj(lx0, ly1, Hw - 4.0f));
            float s = 0.60f * rowShade;
            fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, a0, a1, b0, {0, 0}, {0, 0}, {0, 0}, s);
            fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, a0, b0, b1, {0, 0}, {0, 0}, {0, 0}, s);
        }
        // South eave lip + fascia — the roof's overhang & THICKNESS in front of the wall.
        {
            float he = Hw;
            P2 l0 = toPx(proj(lx0 - eave, ly1, he)), l1 = toPx(proj(lx1 + eave, ly1, he));
            P2 e0 = toPx(proj(lx0 - eave, ly1 + eave, he)), e1 = toPx(proj(lx1 + eave, ly1 + eave, he));
            float sl = 0.88f * rowShade;                        // lip top (roof-lit)
            fillTexTri(img, map, (int)X0, (int)(Y0 + roofArtRows - 1), (int)W, 1, l0, l1, e1, {0, 0}, {W, 0}, {W, 0}, sl);
            fillTexTri(img, map, (int)X0, (int)(Y0 + roofArtRows - 1), (int)W, 1, l0, e1, e0, {0, 0}, {W, 0}, {0, 0}, sl);
            P2 f0 = toPx(proj(lx0 - eave, ly1 + eave, he - slab)), f1 = toPx(proj(lx1 + eave, ly1 + eave, he - slab));
            float sf = 0.38f * rowShade;                        // fascia (thickness)
            fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, e0, e1, f1, {0, 0}, {0, 0}, {0, 0}, sf);
            fillTexTri(img, map, (int)X0, (int)Y0, 1, 1, e0, f1, f0, {0, 0}, {0, 0}, {0, 0}, sf);
        }
        // (The door is part of the folded facade art — no separate overlay, which
        // previously produced an L-shaped seam around the doorway.)
    };

    auto drawBillboard = [&](const HeightfieldScene::Billboard& b) {
        GP g = proj((b.tileX - tx0) * 8.0f, (b.tileY - ty0) * 8.0f, 0.0f);
        P2 foot = toPx(g);
        const float sw = b.img.width * outScale * g.s * b.scale;
        const float sh = b.img.height * outScale * g.s * b.scale;
        if (opt.dropShadows)
            contactShadow(foot.x, foot.y, sw * 0.42f, std::max(2.0f, sh * 0.12f), 0.4f);
        const int left = (int)std::lround(foot.x - sw * 0.5f);
        const int top = (int)std::lround(foot.y - sh);
        const int iw = (int)std::lround(sw), ih = (int)std::lround(sh);
        for (int dy = 0; dy < ih; ++dy) {
            int Y = top + dy;
            if (Y < 0 || Y >= img.height) continue;
            int sy = std::min(b.img.height - 1, dy * b.img.height / std::max(1, ih));
            for (int dx = 0; dx < iw; ++dx) {
                int X = left + dx;
                if (X < 0 || X >= img.width) continue;
                int sx = std::min(b.img.width - 1, dx * b.img.width / std::max(1, iw));
                const Color& c = b.img.at(sx, sy);
                if (c.a == 0) continue;
                img.at(X, Y) = Color{c.r, c.g, c.b, 255};
            }
        }
    };

    // Ground-source per cell: a raised-object cell borrows the nearest walkable
    // ground tile for its floor, so an object never leaves a flat "twin" of its
    // own art on the ground (multi-source BFS out from Flat/Grass/Ledge cells).
    std::vector<int> groundSrc((size_t)cCols * cRows, -1);
    {
        auto isGround = [&](uint8_t a) {
            return a == (uint8_t)Arch::Flat || a == (uint8_t)Arch::Grass || a == (uint8_t)Arch::Ledge;
        };
        std::vector<int> q;
        q.reserve((size_t)cCols * cRows);
        for (int i = 0; i < cCols * cRows; ++i)
            if (isGround(arche[i])) { groundSrc[i] = i; q.push_back(i); }
        const int dx4[4] = {1, -1, 0, 0}, dy4[4] = {0, 0, 1, -1};
        for (size_t h = 0; h < q.size(); ++h) {
            int i = q[h], cx = i % cCols, cy = i / cCols;
            for (int k = 0; k < 4; ++k) {
                int nx = cx + dx4[k], ny = cy + dy4[k];
                if (nx < 0 || ny < 0 || nx >= cCols || ny >= cRows) continue;
                int ni = ny * cCols + nx;
                if (groundSrc[ni] < 0) { groundSrc[ni] = groundSrc[i]; q.push_back(ni); }
            }
        }
    }

    // ---- Pass 1: flat ground (far → near), water recessed & tinted ----------
    for (int ty = ty0; ty < ty1; ++ty) {
        const int lj = ty - ty0;
        const float rowDepth = nY > 1 ? (float)lj / (nY - 1) : 1.0f;
        const float rowShade = 0.82f + 0.18f * rowDepth;
        for (int tx = tx0; tx < tx1; ++tx) {
            const int li = tx - tx0;
            const bool water = classAt(tx, ty) == (uint8_t)TerrainClass::Water;
            const int tox = tx * 8, toy = ty * 8;
            const float gh = water ? H_WATER : 0.0f;
            P2 g00 = toPx(proj(li * 8.0f, lj * 8.0f, gh));
            P2 g10 = toPx(proj((li + 1) * 8.0f, lj * 8.0f, gh));
            P2 g11 = toPx(proj((li + 1) * 8.0f, (lj + 1) * 8.0f, gh));
            P2 g01 = toPx(proj(li * 8.0f, (lj + 1) * 8.0f, gh));
            const float gShade = rowShade * (water ? 0.90f : 1.0f);
            // Raised-object cells borrow a neighbouring ground tile for their floor
            // so the object's own art doesn't also appear flat beneath it.
            int stox = tox, stoy = toy;
            {
                int cxi = std::clamp((tx * 8) / cell, 0, cCols - 1);
                int cyi = std::clamp((ty * 8) / cell, 0, cRows - 1);
                uint8_t a = arche[cyi * cCols + cxi];
                if (a == (uint8_t)Arch::Foliage || a == (uint8_t)Arch::Rock ||
                    a == (uint8_t)Arch::Sign || a == (uint8_t)Arch::Building) {
                    int gs = groundSrc[cyi * cCols + cxi];
                    if (gs >= 0) {
                        stox = std::clamp(tox + (gs % cCols - cxi) * cell, 0, map.width - 8);
                        stoy = std::clamp(toy + (gs / cCols - cyi) * cell, 0, map.height - 8);
                    }
                }
            }
            fillTexTri(img, map, stox, stoy, 8, 8, g00, g10, g11, {0, 0}, {8, 0}, {8, 8}, gShade);
            fillTexTri(img, map, stox, stoy, 8, 8, g00, g11, g01, {0, 0}, {8, 8}, {0, 8}, gShade);
        }
    }

    // ---- Pass 2: raised objects (far → near by cell row), sprites interleaved
    int ccx0 = std::clamp((tx0 * 8) / cell, 0, cCols - 1);
    int ccx1 = std::clamp(((tx1 * 8) - 1) / cell, 0, cCols - 1);
    int ccy0 = std::clamp((ty0 * 8) / cell, 0, cRows - 1);
    int ccy1 = std::clamp(((ty1 * 8) - 1) / cell, 0, cRows - 1);
    std::vector<uint8_t> bldDrawn(buildings.size(), 0);
    for (int ccy = ccy0; ccy <= ccy1; ++ccy) {
        const float cd = ccy1 > ccy0 ? (float)(ccy - ccy0) / (ccy1 - ccy0) : 1.0f;
        const float rowShade = 0.82f + 0.18f * cd;
        for (int ccx = ccx0; ccx <= ccx1; ++ccx) {
            switch ((Arch)arche[ccy * cCols + ccx]) {
                case Arch::Foliage: drawDome(ccx, ccy, true, rowShade); break;
                case Arch::Rock: drawDome(ccx, ccy, false, rowShade); break;
                case Arch::Sign: drawSign(ccx, ccy, rowShade); break;
                case Arch::Grass: drawGrass(ccx, ccy, rowShade); break;
                case Arch::Ledge: drawTopBox(ccx, ccy, H_LEDGE, rowShade); break;
                default: break;  // buildings handled once per region, below
            }
        }
        // Draw each building once, at its (clamped-into-view) front row — even when
        // its top-left anchor cell is scrolled off the crop.
        for (int rid = 0; rid < (int)buildings.size(); ++rid) {
            if (bldDrawn[rid]) continue;
            const Region& bd = buildings[rid];
            if (ccy != std::clamp(bd.cy1, ccy0, ccy1)) continue;
            if (bd.cx1 < ccx0 || bd.cx0 > ccx1) continue;
            drawBuilding(bd, rowShade);
            bldDrawn[rid] = 1;
        }
        for (const auto& b : scene.sprites) {
            if (((int)std::floor(b.tileY) * 8) / cell != ccy) continue;
            if (b.tileX < tx0 || b.tileX >= tx1) continue;
            drawBillboard(b);
        }
    }

    return img;
}

}  // namespace prismatic
