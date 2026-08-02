// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — game-aware lighting engine.
//
// Consumes the structured scene stream (real 3D depth, camera matrices, layer
// provenance) plus a game profile (world-anchored lights, environments) and
// relights the emulator's own pixels:
//
//   depth + inverse clip matrix  -> per-pixel world position
//   world-position gradients     -> per-pixel normals
//   profile light anchors        -> physically-attenuated point lights,
//                                   gated by map ID and in-game time
//   layer mask                   -> UI/text pixels pass through untouched
//   lit HDR                      -> scene-sourced bloom (UI excluded) -> tonemap
//
// No pixel is invented: albedo is always the game's own framebuffer color.
// Pixels without genuine depth fall back to the unmodified native image.
#pragma once
#include <string>
#include <vector>
#include "prismatic/types.hpp"
#include "prismatic/scene_stream.hpp"
#include "prismatic/game_profile.hpp"

namespace prismatic {

// Snapshot of resolved game state used to gate lights/environments.
struct GameState {
    int mapId = -1;        // -1 = unknown
    int hour = -1;         // emulated RTC hour, -1 = unknown
    std::string timeName;  // resolved from profile timeRanges
};

// One light after condition evaluation, in world space.
struct ResolvedLight {
    const LightAnchor* anchor = nullptr;
    Vec3 pos;
    Vec3 color;      // linear RGB * luminance
    float range = 0; // 0 = unbounded
};

// User-tweakable engine parameters (the Render Lab edits these live).
struct GameLightingParams {
    float lightScale = 1.0f;       // global multiplier on all anchor lights
    float ambientScale = 1.0f;     // multiplier on environment ambient
    float exposure = 1.0f;         // multiplier on environment exposure
    float bloomScale = 1.0f;       // multiplier on environment bloom intensity
    float normalStrength = 1.0f;   // 0 disables N.L shaping (pure distance falloff)
    bool  enableBloom = true;
    bool  debugLightsOnly = false; // show the lighting term instead of lit albedo
};

struct GameLitFrame {
    Image color;                  // final presented image (native res)
    int litPixels = 0;            // pixels that received scene lighting
    int uiPixels = 0;             // pixels passed through via layer mask
    std::vector<ResolvedLight> activeLights;
    GameState state;
};

// Invert a row-major 4x4 (returns false if singular).
bool invert4x4(const float m[16], float out[16]);

// Reconstruct the world-space position of pixel (x,y) using its absolute
// depth and the frame's clip matrix. Returns false when depth/camera are
// missing or the unprojection is degenerate.
bool unprojectPixel(const SceneStream& s, int x, int y, Vec3& outWorld);

// Evaluate profile conditions against the current game state.
std::vector<ResolvedLight> resolveLights(const GameProfile& profile,
                                         const GameState& state,
                                         uint64_t frameIndex);

// Pick the best-matching environment (most specific map/time match wins).
const SceneEnvironment* resolveEnvironment(const GameProfile& profile,
                                           const GameState& state);

// Relight one screen. `framebuffer` is the native emulator output for the
// screen `stream` was captured from.
GameLitFrame renderGameLit(const Image& framebuffer, const SceneStream& stream,
                           const GameProfile& profile, const GameState& state,
                           const GameLightingParams& params = {});

}  // namespace prismatic
