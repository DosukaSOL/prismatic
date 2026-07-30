// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/profile.hpp"
#include "prismatic/json.hpp"
#include <cstdlib>

namespace prismatic {

Material materialFromString(const std::string& s) {
    if (s == "Ground") return Material::Ground;
    if (s == "Path") return Material::Path;
    if (s == "Water") return Material::Water;
    if (s == "Foliage") return Material::Foliage;
    if (s == "Wood") return Material::Wood;
    if (s == "Stone") return Material::Stone;
    if (s == "Cloth") return Material::Cloth;
    if (s == "Skin") return Material::Skin;
    if (s == "Metal") return Material::Metal;
    if (s == "Emissive") return Material::Emissive;
    return Material::Unknown;
}
std::string materialToString(Material m) { return materialName(m); }

static const char* scopeToString(RuleScope s) {
    switch (s) {
        case RuleScope::Game: return "game";
        case RuleScope::Map: return "map";
        case RuleScope::Tileset: return "tileset";
        case RuleScope::Tile: return "tile";
        default: return "default";
    }
}
static RuleScope scopeFromString(const std::string& s) {
    if (s == "game") return RuleScope::Game;
    if (s == "map") return RuleScope::Map;
    if (s == "tileset") return RuleScope::Tileset;
    if (s == "tile") return RuleScope::Tile;
    return RuleScope::Default;
}
static System systemFromString(const std::string& s) {
    if (s == "GB") return System::GB;
    if (s == "GBC") return System::GBC;
    if (s == "GBA") return System::GBA;
    if (s == "NDS") return System::NDS;
    if (s == "Synthetic") return System::Synthetic;
    return System::Unknown;
}

Profile defaultProfile() {
    Profile p;
    p.version = kProfileVersion;
    p.basePreset = "HD-2.5D BALANCED";
    p.environment.timeOfDay = 12.0f;
    return p;
}

std::string serializeProfile(const Profile& p, bool copyrightSafe) {
    JsonValue j = JsonValue::makeObject();
    j.set("version", p.version);
    j.set("system", std::string(systemName(p.system)));
    j.set("gameCode", p.gameCode);
    if (!copyrightSafe) {
        j.set("romSha256", p.romSha256);
        j.set("title", p.title);
    }
    j.set("basePreset", p.basePreset);
    if (p.hasPresetOverride) j.set("presetOverride", presetToJson(p.presetOverride));

    JsonValue env = JsonValue::makeObject();
    env.set("timeOfDay", p.environment.timeOfDay);
    env.set("weather", std::string(weatherName(p.environment.weather)));
    env.set("tag", std::string(locationTagName(p.environment.tag)));
    j.set("environment", env);

    JsonValue fid = JsonValue::makeObject();
    fid.set("enabled", p.fidelity.enabled);
    fid.set("maxLuminanceShift", p.fidelity.maxLuminanceShift);
    fid.set("preserveSilhouette", p.fidelity.preserveSilhouette);
    j.set("fidelityLock", fid);

    JsonValue rules = JsonValue::makeArray();
    for (const auto& r : p.rules) {
        JsonValue rj = JsonValue::makeObject();
        rj.set("scope", std::string(scopeToString(r.scope)));
        rj.set("key", r.key);
        if (r.hasMaterial) rj.set("material", materialToString(r.material));
        if (r.hasEmissive) rj.set("emissiveBoost", r.emissiveBoost);
        if (r.hasHeight) rj.set("heightScale", r.heightScale);
        rules.push(rj);
    }
    j.set("rules", rules);
    return j.dump(2);
}

static Weather weatherFromString(const std::string& s) {
    if (s == "Rain") return Weather::Rain;
    if (s == "Fog") return Weather::Fog;
    if (s == "Snow") return Weather::Snow;
    return Weather::Clear;
}
static LocationTag tagFromString(const std::string& s) {
    if (s == "Interior") return LocationTag::Interior;
    if (s == "Cave") return LocationTag::Cave;
    if (s == "Water") return LocationTag::Water;
    return LocationTag::Overworld;
}

bool parseProfile(const std::string& json, Profile& out, std::string& error) {
    JsonValue j;
    try {
        j = JsonParser::parse(json);
    } catch (const std::exception& e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    }
    if (!j.isObject()) { error = "profile root must be an object"; return false; }

    Profile p = defaultProfile();
    p.version = j.get("version").asInt(0);
    p.system = systemFromString(j.get("system").asString("Unknown"));
    p.gameCode = j.get("gameCode").asString("");
    p.romSha256 = j.get("romSha256").asString("");
    p.title = j.get("title").asString("");
    p.basePreset = j.get("basePreset").asString("HD-2.5D BALANCED");
    if (j.has("presetOverride")) { p.hasPresetOverride = true; p.presetOverride = presetFromJson(j.get("presetOverride")); }

    const JsonValue& env = j.get("environment");
    if (env.isObject()) {
        p.environment.timeOfDay = (float)env.get("timeOfDay").asNumber(12.0);
        p.environment.weather = weatherFromString(env.get("weather").asString("Clear"));
        p.environment.tag = tagFromString(env.get("tag").asString("Overworld"));
    }
    const JsonValue& fid = j.get("fidelityLock");
    if (fid.isObject()) {
        p.fidelity.enabled = fid.get("enabled").asBool(true);
        p.fidelity.maxLuminanceShift = (float)fid.get("maxLuminanceShift").asNumber(0.4);
        p.fidelity.preserveSilhouette = fid.get("preserveSilhouette").asBool(true);
    }
    const JsonValue& rules = j.get("rules");
    if (rules.isArray()) {
        for (const auto& rj : *rules.arr) {
            if (!rj.isObject()) continue;
            ProfileRule r;
            r.scope = scopeFromString(rj.get("scope").asString("default"));
            r.key = rj.get("key").asString("");
            if (rj.has("material")) { r.hasMaterial = true; r.material = materialFromString(rj.get("material").asString()); }
            if (rj.has("emissiveBoost")) { r.hasEmissive = true; r.emissiveBoost = (float)rj.get("emissiveBoost").asNumber(0); }
            if (rj.has("heightScale")) { r.hasHeight = true; r.heightScale = (float)rj.get("heightScale").asNumber(0); }
            p.rules.push_back(r);
        }
    }
    migrateProfile(p);
    auto issues = validateProfile(p);
    if (!issues.empty()) { error = issues.front(); return false; }
    out = p;
    return true;
}

void migrateProfile(Profile& p) {
    if (p.version < 1) {
        // v0 -> v1: assume defaults for anything unset.
        if (p.basePreset.empty()) p.basePreset = "HD-2.5D BALANCED";
        p.version = 1;
    }
    if (p.version > kProfileVersion) p.version = kProfileVersion;  // forward-clamp
}

std::vector<std::string> validateProfile(const Profile& p) {
    std::vector<std::string> issues;
    if (p.version < 0 || p.version > kProfileVersion)
        issues.push_back("unsupported profile version");
    if (p.environment.timeOfDay < 0 || p.environment.timeOfDay >= 24.0f)
        issues.push_back("environment.timeOfDay out of range [0,24)");
    if (p.fidelity.maxLuminanceShift < 0 || p.fidelity.maxLuminanceShift > 1)
        issues.push_back("fidelityLock.maxLuminanceShift out of range [0,1]");
    for (const auto& r : p.rules) {
        if ((r.scope == RuleScope::Tile || r.scope == RuleScope::Tileset ||
             r.scope == RuleScope::Map) && r.key.empty())
            issues.push_back("rule with scoped key is missing 'key'");
    }
    return issues;
}

static void mergeRule(const ProfileRule& r, ResolvedTileConfig& c) {
    if (r.hasMaterial) { c.hasMaterial = true; c.material = r.material; }
    if (r.hasEmissive) c.emissiveBoost = r.emissiveBoost;
    if (r.hasHeight) { c.hasHeight = true; c.heightScale = r.heightScale; }
}

ResolvedTileConfig resolveTile(const Profile& p, const std::string& tileHashHex,
                               const std::string& tilesetId, const std::string& mapName) {
    ResolvedTileConfig c;
    // Precedence low -> high: default, game, map, tileset, tile.
    const RuleScope order[] = {RuleScope::Default, RuleScope::Game, RuleScope::Map,
                               RuleScope::Tileset, RuleScope::Tile};
    for (RuleScope scope : order) {
        for (const auto& r : p.rules) {
            if (r.scope != scope) continue;
            bool match = false;
            switch (scope) {
                case RuleScope::Default:
                case RuleScope::Game: match = true; break;
                case RuleScope::Map: match = (r.key == mapName); break;
                case RuleScope::Tileset: match = (r.key == tilesetId); break;
                case RuleScope::Tile: match = (r.key == tileHashHex); break;
            }
            if (match) mergeRule(r, c);
        }
    }
    return c;
}

void applyProfileToCache(const Profile& p, MaterialCache& cache) {
    for (const auto& r : p.rules) {
        if (r.scope == RuleScope::Tile && r.hasMaterial && !r.key.empty()) {
            uint64_t h = std::strtoull(r.key.c_str(), nullptr, 16);
            cache.overrideMaterial(h, r.material);
        }
    }
}

}  // namespace prismatic
