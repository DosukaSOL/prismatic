// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/composite.hpp"

namespace prismatic {

namespace {

// Sample one background layer at screen pixel (sx, sy). Returns false if the
// resolved palette index is transparent (0) or the layer is disabled.
bool sampleBackground(const BackgroundLayer& bg, int sx, int sy, Color& out) {
    if (!bg.enabled || bg.widthTiles == 0 || bg.heightTiles == 0) return false;
    int wpx = bg.widthTiles * kTileSize;
    int hpx = bg.heightTiles * kTileSize;
    // Wrap scroll (tile hardware wraps the map).
    int wx = ((sx + bg.scrollX) % wpx + wpx) % wpx;
    int wy = ((sy + bg.scrollY) % hpx + hpx) % hpx;
    int tx = wx / kTileSize, ty = wy / kTileSize;
    int ix = wx % kTileSize, iy = wy % kTileSize;
    const TileRef& ref = bg.cell(tx, ty);
    if (ref.tileIndex >= bg.tileset.size()) return false;
    int px = ref.flipH ? (kTileSize - 1 - ix) : ix;
    int py = ref.flipV ? (kTileSize - 1 - iy) : iy;
    uint8_t idx = bg.tileset[ref.tileIndex].at(px, py);
    if (idx == 0) return false;  // transparent
    if (bg.palettes.empty()) return false;
    const Palette& pal = bg.palettes[ref.paletteBank < bg.palettes.size() ? ref.paletteBank : 0];
    if (idx >= pal.size()) return false;
    out = pal[idx];
    return out.a != 0;
}

// Sample a sprite at screen pixel. Returns false if outside or transparent.
bool sampleSprite(const Sprite& sp, int sx, int sy, Color& out) {
    if (!sp.enabled) return false;
    int lx = sx - sp.x, ly = sy - sp.y;
    if (lx < 0 || ly < 0 || lx >= sp.width || ly >= sp.height) return false;
    int fx = sp.flipH ? (sp.width - 1 - lx) : lx;
    int fy = sp.flipV ? (sp.height - 1 - ly) : ly;
    int tileCol = fx / kTileSize, tileRow = fy / kTileSize;
    int tileIdx = tileRow * sp.tilesWide() + tileCol;
    if (tileIdx < 0 || tileIdx >= (int)sp.tiles.size()) return false;
    uint8_t idx = sp.tiles[tileIdx].at(fx % kTileSize, fy % kTileSize);
    if (idx == 0 || idx >= sp.palette.size()) return false;
    out = sp.palette[idx];
    return out.a != 0;
}

Color blendOver(Color dst, Color src) {
    if (src.a == 255) return src;
    if (src.a == 0) return dst;
    float a = src.a / 255.0f;
    return Color{
        (uint8_t)std::lround(src.r * a + dst.r * (1 - a)),
        (uint8_t)std::lround(src.g * a + dst.g * (1 - a)),
        (uint8_t)std::lround(src.b * a + dst.b * (1 - a)),
        255};
}

}  // namespace

Image compositeNative(const StructuredFrame& frame) {
    Image img(frame.screenWidth, frame.screenHeight, frame.backdrop);

    // Optional NDS 3D layer sits at the back (behind priority-0 tiles) unless a
    // backend indicates otherwise; for our synthetic content it is absent.
    if (frame.nds3d.present && frame.nds3d.color.width == img.width &&
        frame.nds3d.color.height == img.height) {
        for (int y = 0; y < img.height; ++y)
            for (int x = 0; x < img.width; ++x)
                img.at(x, y) = blendOver(img.at(x, y), frame.nds3d.color.at(x, y));
    }

    for (int prio = 0; prio < kMaxPriority; ++prio) {
        // Backgrounds at this priority, higher index first (BG0 ends on top).
        std::vector<const BackgroundLayer*> bgs;
        for (const auto& bg : frame.backgrounds)
            if (bg.enabled && bg.priority == prio) bgs.push_back(&bg);
        std::sort(bgs.begin(), bgs.end(),
                  [](const BackgroundLayer* a, const BackgroundLayer* b) { return a->index > b->index; });

        for (const BackgroundLayer* bg : bgs) {
            for (int y = 0; y < img.height; ++y) {
                for (int x = 0; x < img.width; ++x) {
                    // Respect window layer masks if any window is enabled.
                    Color c;
                    if (sampleBackground(*bg, x, y, c))
                        img.at(x, y) = blendOver(img.at(x, y), c);
                }
            }
        }

        // Sprites at this priority (win ties over BG), lower id first.
        std::vector<const Sprite*> sprites;
        for (const auto& sp : frame.sprites)
            if (sp.enabled && sp.priority == prio) sprites.push_back(&sp);
        std::sort(sprites.begin(), sprites.end(),
                  [](const Sprite* a, const Sprite* b) { return a->id < b->id; });

        for (const Sprite* sp : sprites) {
            int x0 = std::max(0, sp->x), y0 = std::max(0, sp->y);
            int x1 = std::min(img.width, sp->x + sp->width);
            int y1 = std::min(img.height, sp->y + sp->height);
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    Color c;
                    if (sampleSprite(*sp, x, y, c))
                        img.at(x, y) = blendOver(img.at(x, y), c);
                }
            }
        }
    }
    return img;
}

}  // namespace prismatic
