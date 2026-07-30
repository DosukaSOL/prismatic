// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — scene reconstruction.
//
// Turns a flat StructuredFrame into a per-pixel 2.5D scene: albedo, extruded
// height, edge-aware normals, per-object stable IDs, material IDs, emissive and
// a monotonic depth field. Geometry class per source:
//   ground/path/water  -> flat
//   walls/roofs/canopy -> extruded / elevated
//   trunks             -> extruded prism
//   sprites            -> upright billboards (receive rim light + contact AO)
#pragma once
#include <vector>
#include "prismatic/adapter.hpp"
#include "prismatic/materials.hpp"
#include "prismatic/types.hpp"

namespace prismatic {

enum LayerKind : uint8_t { LK_Backdrop = 0, LK_Background = 1, LK_Sprite = 2, LK_Overhead = 3, LK_Nds3D = 4 };

struct ReconstructOptions {
    float heightScale = 1.0f;      // global extrusion multiplier
    float normalStrength = 1.0f;   // global procedural-bump multiplier
};

struct ReconstructedScene {
    int width = 0, height = 0;
    Image albedo;
    FloatBuffer heightMap;
    FloatBuffer emissive;
    FloatBuffer depth;
    FloatBuffer occupancy;
    FloatBuffer roughness;
    std::vector<Vec3> normal;
    std::vector<int> objectId;
    std::vector<uint8_t> materialId;   // Material
    std::vector<uint8_t> layerKind;    // LayerKind

    const Vec3& normalAt(int x, int y) const { return normal[(size_t)y * width + x]; }
    int objectAt(int x, int y) const { return objectId[(size_t)y * width + x]; }
    LayerKind kindAt(int x, int y) const { return (LayerKind)layerKind[(size_t)y * width + x]; }
};

// Reconstruct a scene from one screen's structured frame.
ReconstructedScene reconstructScene(const StructuredFrame& frame, MaterialCache& cache,
                                    const ReconstructOptions& opt = {});

}  // namespace prismatic
