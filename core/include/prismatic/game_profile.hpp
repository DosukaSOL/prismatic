// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — scene-aware game profiles (.prismprofile).
//
// A game profile is copyright-safe metadata that turns the generic scene
// stream into game-aware rendering: which ROMs it applies to, where to read
// game state (RAM probes), how in-game time maps to lighting environments,
// and where real light sources sit in the game's 3D world space. Profiles
// contain rules only — never ROM-derived assets.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "prismatic/json.hpp"

namespace prismatic {

constexpr int kGameProfileVersion = 1;

// A typed read of emulated RAM (evaluated through EmulatorAdapter::peek).
struct StateProbe {
    std::string id;        // e.g. "mapId"
    uint32_t addr = 0;
    int bytes = 2;         // 1, 2 or 4 (little-endian unsigned)
};

// Named day segment: [startHour, endHour) wrapping midnight when start > end.
struct TimeRange {
    std::string name;      // "night", "day", "dusk"...
    int startHour = 0;
    int endHour = 0;
    bool contains(int hour) const {
        return startHour <= endHour ? (hour >= startHour && hour < endHour)
                                    : (hour >= startHour || hour < endHour);
    }
};

// A world-anchored light source. Position is in the game's own 3D world
// space (the space the captured view matrix transforms from), authored by
// unprojecting reference pixels — so anchors stay glued to geometry as the
// camera scrolls.
struct LightAnchor {
    std::string id;
    std::string type = "point";           // point | spot | directional
    float pos[3] = {0, 0, 0};             // world space
    float color[3] = {1, 1, 1};           // linear RGB 0..1
    float luminance = 100.0f;             // intensity at source
    float range = 48.0f;                  // world units; 0 = unbounded
    std::string attenuation = "inverse_square_soft";
    std::vector<int> maps;                // active map IDs (empty = all)
    std::vector<std::string> times;       // active time ranges (empty = always)
    bool castsShadows = false;
    float flicker = 0.0f;                 // 0..1 amplitude
};

// Ambient/tone environment for a set of maps and times.
struct SceneEnvironment {
    std::string id;
    std::vector<int> maps;                // empty = all maps
    std::vector<std::string> times;       // empty = all times
    bool indoor = false;
    float ambientColor[3] = {1, 1, 1};    // linear multiplier on albedo
    float ambientIntensity = 1.0f;
    float exposure = 1.0f;
    float bloomThreshold = 0.80f;
    float bloomIntensity = 0.35f;
};

struct GameProfile {
    int version = kGameProfileVersion;
    std::string id;                       // e.g. "pokemon-hgss-us"
    std::string name;
    std::string system;                   // "NDS"
    std::vector<std::string> gameCodes;   // e.g. IPKE, IPGE
    std::vector<std::string> romSha256;   // exact validated dumps
    std::vector<StateProbe> probes;
    std::vector<TimeRange> timeRanges;
    std::vector<LightAnchor> lights;
    std::vector<SceneEnvironment> environments;

    const StateProbe* findProbe(const std::string& pid) const {
        for (auto& p : probes) if (p.id == pid) return &p;
        return nullptr;
    }
    // Name of the time range containing `hour` ("" if none).
    std::string timeName(int hour) const {
        for (auto& t : timeRanges) if (t.contains(hour)) return t.name;
        return "";
    }
    // Does this profile apply to the given ROM (hash preferred, code fallback)?
    bool matches(const std::string& sha256, const std::string& gameCode) const {
        for (auto& h : romSha256) if (h == sha256) return true;
        for (auto& c : gameCodes) if (!c.empty() && c == gameCode) return true;
        return false;
    }
};

// JSON round-trip. parse returns false + error on malformed/unsupported input.
JsonValue gameProfileToJson(const GameProfile& p);
bool parseGameProfile(const std::string& json, GameProfile& out, std::string& error);
std::string serializeGameProfile(const GameProfile& p);
bool loadGameProfileFile(const std::string& path, GameProfile& out, std::string& error);

}  // namespace prismatic
