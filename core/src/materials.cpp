// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/materials.hpp"
#include <cmath>

namespace prismatic {

const char* materialName(Material m) {
    switch (m) {
        case Material::Ground: return "Ground";
        case Material::Path: return "Path";
        case Material::Water: return "Water";
        case Material::Foliage: return "Foliage";
        case Material::Wood: return "Wood";
        case Material::Stone: return "Stone";
        case Material::Cloth: return "Cloth";
        case Material::Skin: return "Skin";
        case Material::Metal: return "Metal";
        case Material::Emissive: return "Emissive";
        default: return "Unknown";
    }
}

static void rgbToHsl(const Color& c, float& h, float& s, float& l) {
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
    l = (mx + mn) * 0.5f;
    float d = mx - mn;
    if (d < 1e-6f) { h = 0; s = 0; return; }
    s = l > 0.5f ? d / (2.0f - mx - mn) : d / (mx + mn);
    if (mx == r) h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (mx == g) h = (b - r) / d + 2.0f;
    else h = (r - g) / d + 4.0f;
    h *= 60.0f;
}

TileFeatures computeTileFeatures(const Tile& tile, const Palette& pal) {
    TileFeatures f;
    int n = 0;
    float lumaSum = 0, satSum = 0, rSum = 0, gSum = 0, bSum = 0;
    int bright = 0, warmBright = 0;
    // Resolve to color grid (transparent -> skip for stats).
    Color grid[8][8];
    bool opaque[8][8];
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            uint8_t idx = tile.at(x, y);
            bool op = idx != 0 && idx < pal.size() && pal[idx].a != 0;
            opaque[y][x] = op;
            grid[y][x] = op ? pal[idx] : Color{0, 0, 0, 0};
            if (op) {
                Color c = pal[idx];
                float h, s, l; rgbToHsl(c, h, s, l);
                lumaSum += l; satSum += s;
                rSum += c.r; gSum += c.g; bSum += c.b;
                if (l > 0.85f) {
                    ++bright;
                    if (h < 70.0f || h > 330.0f) ++warmBright;  // warm bright
                }
                ++n;
            }
        }
    if (n == 0) return f;
    f.coverage = n / 64.0f;
    f.lumaMean = lumaSum / n;
    f.satMean = satSum / n;
    float total = rSum + gSum + bSum + 1e-6f;
    f.redBias = rSum / total;
    f.greenBias = gSum / total;
    f.blueBias = bSum / total;
    f.brightFrac = bright / (float)n;
    f.warmBrightFrac = warmBright / (float)n;
    // Edge density: count 4-neighbour luminance discontinuities.
    int edges = 0, cmp = 0;
    auto lum = [](Color c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; };
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            if (!opaque[y][x]) continue;
            if (x + 1 < 8 && opaque[y][x + 1]) { ++cmp; if (std::fabs(lum(grid[y][x]) - lum(grid[y][x + 1])) > 40) ++edges; }
            if (y + 1 < 8 && opaque[y + 1][x]) { ++cmp; if (std::fabs(lum(grid[y][x]) - lum(grid[y + 1][x])) > 40) ++edges; }
        }
    f.edgeDensity = cmp > 0 ? edges / (float)cmp : 0.0f;
    return f;
}

Material classifyFeatures(const TileFeatures& f) {
    // Emissive: strongly warm-bright content.
    if (f.warmBrightFrac > 0.18f && f.lumaMean > 0.55f) return Material::Emissive;
    // Water: blue-dominant, fairly saturated, low-to-mid edges.
    if (f.blueBias > 0.40f && f.blueBias > f.greenBias && f.satMean > 0.25f)
        return Material::Water;
    // Foliage: green-dominant.
    if (f.greenBias > 0.42f && f.greenBias > f.redBias)
        return Material::Foliage;
    // Wood: warm brown (red>green>blue, mid luma, mid sat).
    if (f.redBias > 0.38f && f.greenBias > f.blueBias && f.blueBias < 0.28f &&
        f.lumaMean < 0.6f && f.satMean > 0.2f)
        return Material::Wood;
    // Path/ground: tan, low saturation, warm.
    if (f.satMean < 0.35f && f.redBias >= f.blueBias && f.lumaMean > 0.45f)
        return Material::Path;
    // Stone/wall: desaturated, high edge density.
    if (f.satMean < 0.25f && f.edgeDensity > 0.12f)
        return Material::Stone;
    // Cloth/skin fallbacks for sprite-like saturated small content.
    if (f.satMean > 0.45f) return Material::Cloth;
    if (f.redBias > 0.36f && f.lumaMean > 0.6f && f.satMean < 0.4f) return Material::Skin;
    return Material::Ground;
}

MaterialParams paramsFor(Material m) {
    MaterialParams p;
    switch (m) {
        case Material::Water:    p = {0.08f, 0.0f, 0.4f, 0.0f, 0.0f, false}; break;
        case Material::Foliage:  p = {0.75f, 0.0f, 1.2f, 0.55f, 0.0f, false}; break;
        case Material::Wood:     p = {0.65f, 0.0f, 1.0f, 0.5f, 0.0f, false}; break;
        case Material::Stone:    p = {0.85f, 0.0f, 1.3f, 0.8f, 0.0f, false}; break;
        case Material::Path:     p = {0.9f,  0.0f, 0.6f, 0.0f, 0.0f, false}; break;
        case Material::Ground:   p = {0.9f,  0.0f, 0.7f, 0.0f, 0.0f, false}; break;
        case Material::Cloth:    p = {0.7f,  0.0f, 0.9f, 0.4f, 0.0f, true}; break;
        case Material::Skin:     p = {0.5f,  0.0f, 0.7f, 0.4f, 0.0f, true}; break;
        case Material::Metal:    p = {0.3f,  1.0f, 1.0f, 0.3f, 0.0f, false}; break;
        case Material::Emissive: p = {0.5f,  0.0f, 0.5f, 0.2f, 1.0f, false}; break;
        default:                 p = {0.7f,  0.0f, 0.8f, 0.0f, 0.0f, false}; break;
    }
    return p;
}

uint64_t tileContentHash(const Tile& tile, const Palette& pal) {
    // FNV-1a over tile indices + resolved palette bytes.
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint8_t b) { h ^= b; h *= 1099511628211ull; };
    for (uint8_t v : tile.px) mix(v);
    for (const Color& c : pal) { mix(c.r); mix(c.g); mix(c.b); mix(c.a); }
    return h;
}

const MaterialCache::Entry& MaterialCache::lookup(const Tile& tile, const Palette& pal) {
    uint64_t h = tileContentHash(tile, pal);
    auto it = map_.find(h);
    if (it != map_.end()) return it->second;
    TileFeatures f = computeTileFeatures(tile, pal);
    Material m = classifyFeatures(f);
    auto ov = overrides_.find(h);
    if (ov != overrides_.end()) m = ov->second;
    auto res = map_.emplace(h, Entry{m, f});
    return res.first->second;
}

void MaterialCache::overrideMaterial(uint64_t hash, Material m) { overrides_[hash] = m; }

}  // namespace prismatic
