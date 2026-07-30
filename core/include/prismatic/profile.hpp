// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — profile engine.
//
// A profile is a versioned, copyright-safe JSON document that customises the
// enhancement for a game WITHOUT ever embedding game assets. It carries: a base
// preset (+ optional overrides), environment defaults, a Fidelity Lock, and a
// list of rules resolved by precedence:  tile > tileset > map > game > default.
// Rules may override the inferred material, emissive boost or extrusion height
// for a tile identified by its content hash.
#pragma once
#include <string>
#include <vector>
#include "prismatic/adapter.hpp"
#include "prismatic/materials.hpp"
#include "prismatic/presets.hpp"
#include "prismatic/environment.hpp"

namespace prismatic {

constexpr int kProfileVersion = 1;

// Invariant that protects original gameplay readability.
struct FidelityLock {
    bool enabled = true;
    float maxLuminanceShift = 0.4f;  // clamp on how far a pixel's luma may move
    bool preserveSilhouette = true;  // never erase a native opaque pixel
};

enum class RuleScope { Default, Game, Map, Tileset, Tile };

struct ProfileRule {
    RuleScope scope = RuleScope::Default;
    std::string key;  // tile-hash hex / tileset id / map name (empty for game/default)
    bool hasMaterial = false;
    Material material = Material::Unknown;
    bool hasEmissive = false;
    float emissiveBoost = 0.0f;
    bool hasHeight = false;
    float heightScale = 0.0f;
};

struct Profile {
    int version = kProfileVersion;
    System system = System::Unknown;
    std::string gameCode;
    std::string romSha256;    // for matching only; stripped on copyright-safe export
    std::string title;        // stripped on copyright-safe export
    std::string basePreset = "HD-2.5D BALANCED";
    bool hasPresetOverride = false;
    Preset presetOverride;
    EnvironmentState environment;
    FidelityLock fidelity;
    std::vector<ProfileRule> rules;
};

struct ResolvedTileConfig {
    bool hasMaterial = false;
    Material material = Material::Unknown;
    float emissiveBoost = 0.0f;
    bool hasHeight = false;
    float heightScale = 0.0f;
};

Profile defaultProfile();

// Parse + validate. On failure returns false and fills `error`.
bool parseProfile(const std::string& json, Profile& out, std::string& error);

// Serialize. copyrightSafe strips identity fields derived from the ROM.
std::string serializeProfile(const Profile& p, bool copyrightSafe = true);

// Migrate an older profile in place to the current version.
void migrateProfile(Profile& p);

// Structural validation; returns issues (empty = valid).
std::vector<std::string> validateProfile(const Profile& p);

// Resolve overrides for a tile by precedence.
ResolvedTileConfig resolveTile(const Profile& p, const std::string& tileHashHex,
                               const std::string& tilesetId, const std::string& mapName);

// Apply a profile's material overrides into a cache (by tile hash).
void applyProfileToCache(const Profile& p, MaterialCache& cache);

Material materialFromString(const std::string& s);
std::string materialToString(Material m);

}  // namespace prismatic
