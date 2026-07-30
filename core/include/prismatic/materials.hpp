// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — material classification.
//
// Materials are inferred at runtime from the actual tile/sprite pixels supplied
// by the backend (never invented). Classification is deterministic and content
// driven: it looks at colour statistics and edge structure of each tile's
// graphic. Results are cached by a content hash so a repeated tile is analysed
// once. Profiles may override the inferred material per tile hash.
#pragma once
#include <cstdint>
#include <unordered_map>
#include "prismatic/adapter.hpp"
#include "prismatic/types.hpp"

namespace prismatic {

enum class Material : uint8_t {
    Unknown = 0, Ground, Path, Water, Foliage, Wood, Stone, Cloth, Skin, Metal, Emissive
};

const char* materialName(Material m);

struct TileFeatures {
    float lumaMean = 0;
    float satMean = 0;
    float edgeDensity = 0;
    float redBias = 0, greenBias = 0, blueBias = 0;
    float brightFrac = 0;   // fraction of near-white pixels
    float warmBrightFrac = 0;  // bright AND warm (lamp/window signal)
    float coverage = 0;     // fraction of non-transparent pixels
};

// Per-material shading parameters used by the reconstructor and lighting.
struct MaterialParams {
    float roughness = 0.6f;
    float metalness = 0.0f;
    float normalStrength = 1.0f;   // procedural bump amount
    float heightScale = 0.0f;      // extrusion height (0..1)
    float emissiveScale = 0.0f;
    bool billboardable = false;    // sprites become upright billboards
};

// Compute colour/edge statistics for a tile graphic under a palette.
TileFeatures computeTileFeatures(const Tile& tile, const Palette& pal);

// Classify a tile from its features (pure function; deterministic).
Material classifyFeatures(const TileFeatures& f);

// Default shading parameters for a material.
MaterialParams paramsFor(Material m);

// Content hash of a tile graphic + its palette (for caching / profile keys).
uint64_t tileContentHash(const Tile& tile, const Palette& pal);

// Cache of hash -> (material, features). Not thread-safe by design (per-frame,
// single-threaded reconstruction path).
class MaterialCache {
public:
    struct Entry { Material material; TileFeatures features; };
    const Entry& lookup(const Tile& tile, const Palette& pal);
    void overrideMaterial(uint64_t hash, Material m);  // profile hook
    size_t size() const { return map_.size(); }
    void clear() { map_.clear(); overrides_.clear(); }

private:
    std::unordered_map<uint64_t, Entry> map_;
    std::unordered_map<uint64_t, Material> overrides_;
};

}  // namespace prismatic
