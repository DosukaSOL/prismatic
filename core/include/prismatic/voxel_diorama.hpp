// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — tile-extruded voxel diorama renderer.
//
// Turns a StructuredFrame (BG tilemap + tiles + palettes + OAM sprites) into a
// tilted "tabletop" diorama: the 2D tilemap becomes a heightfield, tall tiles
// (walls, trees, furniture, buildings) extrude upward as crisp voxel blocks with
// shaded sides, and sprites stand up as billboards. This is the honest analog of
// the gen1recomp / DramaticShape voxel mods — built ONLY from the game's own
// tiles, with nearest-neighbour sampling so it stays pixel-crisp (no DIBR blur).
//
// It consumes the generic StructuredFrame, so it works identically for the
// SameBoy (real Crystal) backend and the synthetic overworld test bed.
#pragma once
#include <cstdint>
#include <vector>

#include "prismatic/adapter.hpp"
#include "prismatic/types.hpp"

namespace prismatic {

struct VoxelOptions {
    int scale = 3;             // output ≈ (160*scale) x (144*scale)
    float pitch = 0.62f;       // ground foreshortening: 1=top-down, smaller=more tilt
    float heightUnit = 12.0f;  // diorama pixels of rise per height level (pre-scaled by 'scale')
    int maxHeight = 3;         // height-level clamp
    float sideShade = 0.55f;   // multiplier for extruded side faces (darker = deeper)
    float backShade = 0.86f;   // subtle far-row darkening for depth (per 18 rows)
    bool billboardSprites = true;
    bool dropShadows = true;   // soft contact shadow under raised blocks & sprites
    Color sky{28, 26, 40, 255};

    // --- Perspective "tilt" camera (gen1recomp-style HD-2D diorama) ---------
    // The flat map is treated as a ground plane rotated about the horizontal
    // axis and viewed through a perspective camera (rows above centre recede &
    // shrink, rows below come closer & grow); tall tiles stand up as voxels.
    float tiltDeg = 50.0f;     // camera tilt: 0 = flat top-down, ~50 = strong 3D
    float focal = 1.05f;       // perspective strength (smaller = more dramatic)
    float zStep = 7.0f;        // legacy: flat-canvas px of rise per height level

    // --- Shape/voxel model (gen1recomp + DramaticShape voxel mod parity) -----
    // Heights are now measured in world pixels (cell = 16px). Trees & rocks are
    // carved as per-cell rounded domes (the "cylinder" archetype), buildings as
    // a body box with a folded facade + sloped roof, ground stays flat.
    float heightScale = 1.15f; // world-px height -> canvas px (bigger = taller pop)
    float curve = 0.0f;        // Animal-Crossing world curve (0 = flat, ~0.9 = round)
    float zoom = 1.0f;         // extra magnification of the whole diorama
};

// Render one structured frame as a diorama. 'screen' picks the BG layer's frame;
// pass the StructuredFrame you already captured from the adapter.
Image renderVoxelDiorama(const StructuredFrame& frame, const VoxelOptions& opt = {});

// Terrain class per collision cell, decoded from the game's own collision data
// by the Crystal recomp preprocessor. Drives exact voxel heights.
enum class TerrainClass : uint8_t {
    Ground = 0, Grass = 1, Water = 2, Wall = 3, Tree = 4, Ledge = 5, Door = 6,
};

// A data-driven heightfield scene: a composited RGBA map plus a per-cell terrain
// class grid. Unlike renderVoxelDiorama (which infers heights heuristically from
// a live frame), this extrudes using the game's REAL collision classes.
struct HeightfieldScene {
    Image map;                       // composited map, native pixels
    int cell = 16;                   // pixels per class cell
    int cols = 0, rows = 0;          // class-grid dimensions
    std::vector<uint8_t> classGrid;  // cols*rows terrain classes

    // Final per-cell voxel archetype, decided by the baker from ground truth
    // (collision + tileset palette + warps + signs). When present the renderer
    // uses it verbatim and skips all pixel-based guessing. Values match Arch in
    // voxel_diorama.cpp: 0 Flat,1 Water,2 Grass,3 Ledge,4 Foliage,5 Rock,6 Sign,
    // 7 Building.
    std::vector<uint8_t> archeGrid;

    // Explicit building footprints (cell coords) from the baker's warp+roof pass.
    struct Building {
        int cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;  // footprint bounding box
        int doorCx = 0, doorCy = 0;              // the warp/door cell
    };
    std::vector<Building> buildings;

    // Overworld sprite billboards (Suicune, the player, ...) placed on the map.
    struct Billboard {
        Image img;                   // RGBA; alpha 0 = transparent
        float tileX = 0, tileY = 0;  // ground footprint centre, in 8px-tile units
        float scale = 1.0f;          // extra size multiplier (1 = native pixels)
    };
    std::vector<Billboard> sprites;

    // Optional crop window, in 8px-tile units (w/h == 0 => render the whole map).
    int cropX = 0, cropY = 0, cropW = 0, cropH = 0;
};

Image renderVoxelHeightfield(const HeightfieldScene& scene, const VoxelOptions& opt = {});

}  // namespace prismatic
