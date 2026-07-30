// SPDX-License-Identifier: GPL-3.0-or-later
#include "test_util.hpp"
#include "prismatic/materials.hpp"
using namespace prismatic;

static Tile solidTile(uint8_t idx) {
    Tile t; for (auto& v : t.px) v = idx; return t;
}

static void test_classification() {
    // Water: blue palette.
    Palette water{Color{0, 0, 0, 0}, Color{40, 88, 190}};
    CHECK_EQ(classifyFeatures(computeTileFeatures(solidTile(1), water)), Material::Water);
    // Foliage: green palette.
    Palette green{Color{0, 0, 0, 0}, Color{60, 170, 70}};
    CHECK_EQ(classifyFeatures(computeTileFeatures(solidTile(1), green)), Material::Foliage);
    // Path: tan/desaturated warm.
    Palette tan{Color{0, 0, 0, 0}, Color{198, 170, 110}};
    Material m = classifyFeatures(computeTileFeatures(solidTile(1), tan));
    CHECK(m == Material::Path || m == Material::Ground);
    // Emissive: warm bright.
    Palette warm{Color{0, 0, 0, 0}, Color{255, 240, 200}};
    CHECK_EQ(classifyFeatures(computeTileFeatures(solidTile(1), warm)), Material::Emissive);
}

static void test_cache() {
    MaterialCache c;
    Palette green{Color{0, 0, 0, 0}, Color{60, 170, 70}};
    Tile t = solidTile(1);
    const auto& e1 = c.lookup(t, green);
    CHECK_EQ(e1.material, Material::Foliage);
    CHECK_EQ(c.size(), (size_t)1);
    c.lookup(t, green);  // same content -> cache hit, no growth
    CHECK_EQ(c.size(), (size_t)1);
    // Override changes result.
    uint64_t hsh = tileContentHash(t, green);
    c.clear();
    c.overrideMaterial(hsh, Material::Stone);
    CHECK_EQ(c.lookup(t, green).material, Material::Stone);
}

int main() {
    RUN(test_classification);
    RUN(test_cache);
    return REPORT();
}
