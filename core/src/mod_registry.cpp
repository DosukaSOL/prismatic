// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/mod_registry.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace prismatic {
namespace fs = std::filesystem;

// ---- kind <-> string -------------------------------------------------------

const char* modKindName(ModKind k) {
    switch (k) {
        case ModKind::Graphics: return "graphics";
        case ModKind::Preset:   return "preset";
        case ModKind::Profile:  return "profile";
        case ModKind::Palette:  return "palette";
        case ModKind::Scene:    return "scene";
        case ModKind::Audio:    return "audio";
        case ModKind::Unknown:  break;
    }
    return "unknown";
}

ModKind modKindFromString(const std::string& s) {
    if (s == "graphics") return ModKind::Graphics;
    if (s == "preset")   return ModKind::Preset;
    if (s == "profile")  return ModKind::Profile;
    if (s == "palette")  return ModKind::Palette;
    if (s == "scene")    return ModKind::Scene;
    if (s == "audio")    return ModKind::Audio;
    return ModKind::Unknown;
}

// ---- JSON round-trip -------------------------------------------------------

static JsonValue stringArray(const std::vector<std::string>& v) {
    JsonValue a = JsonValue::makeArray();
    for (const auto& s : v) a.push(JsonValue(s));
    return a;
}

static std::vector<std::string> readStringArray(const JsonValue& j) {
    std::vector<std::string> out;
    if (j.isArray())
        for (const auto& e : *j.arr)
            if (e.isString()) out.push_back(e.str);
    return out;
}

JsonValue modManifestToJson(const ModManifest& m) {
    JsonValue j = JsonValue::makeObject();
    j.set("version", m.version);
    j.set("id", m.id);
    j.set("name", m.name);
    j.set("modVersion", m.modVersion);
    j.set("author", m.author);
    j.set("license", m.license);
    j.set("description", m.description);
    j.set("kind", std::string(modKindName(m.kind)));
    j.set("entry", m.entry);
    j.set("provides", stringArray(m.provides));
    j.set("dependencies", stringArray(m.dependencies));
    j.set("enabledByDefault", m.enabledByDefault);
    j.set("builtin", m.builtin);
    return j;
}

bool parseModManifest(const std::string& json, ModManifest& out, std::string& error) {
    JsonValue j;
    try {
        j = JsonParser::parse(json);
    } catch (const std::exception& e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    }
    if (!j.isObject()) { error = "mod manifest root must be an object"; return false; }

    ModManifest m;
    m.version = j.get("version").asInt(0);
    m.id = j.get("id").asString("");
    m.name = j.get("name").asString("");
    m.modVersion = j.get("modVersion").asString(m.modVersion);
    m.author = j.get("author").asString("");
    m.license = j.get("license").asString(m.license);
    m.description = j.get("description").asString("");
    m.kind = modKindFromString(j.get("kind").asString("unknown"));
    m.entry = j.get("entry").asString("");
    m.provides = readStringArray(j.get("provides"));
    m.dependencies = readStringArray(j.get("dependencies"));
    m.enabledByDefault = j.get("enabledByDefault").asBool(true);
    m.builtin = j.get("builtin").asBool(false);
    out = std::move(m);
    return true;
}

std::string serializeModManifest(const ModManifest& m) {
    return modManifestToJson(m).dump(2);
}

std::vector<std::string> validateModManifest(const ModManifest& m) {
    std::vector<std::string> issues;
    if (m.version <= 0 || m.version > kModManifestVersion)
        issues.push_back("unsupported manifest version " + std::to_string(m.version));
    if (m.id.empty())
        issues.push_back("id is required");
    if (m.name.empty())
        issues.push_back("name is required");
    if (m.kind == ModKind::Unknown)
        issues.push_back("kind is missing or unknown");
    for (const auto& d : m.dependencies)
        if (d == m.id) issues.push_back("mod depends on itself: " + m.id);
    return issues;
}

// ---- registry --------------------------------------------------------------

const LoadedMod* ModRegistry::find(const std::string& id) const {
    for (const auto& lm : mods_)
        if (lm.manifest.id == id) return &lm;
    return nullptr;
}

bool ModRegistry::add(const ModManifest& m, const std::string& path, bool enabled) {
    if (!m.id.empty() && find(m.id)) return false;  // duplicate id
    LoadedMod lm;
    lm.manifest = m;
    lm.path = path;
    lm.enabled = enabled;
    mods_.push_back(std::move(lm));
    return true;
}

bool ModRegistry::loadManifestFile(const std::string& file, std::string& error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) { error = "cannot open " + file; return false; }
    std::ostringstream ss;
    ss << in.rdbuf();

    ModManifest m;
    if (!parseModManifest(ss.str(), m, error)) {
        error = file + ": " + error;
        return false;
    }
    auto issues = validateModManifest(m);
    if (!issues.empty()) {
        error = file + ": " + issues.front();
        return false;
    }
    if (find(m.id)) {
        error = file + ": duplicate mod id '" + m.id + "'";
        return false;
    }
    add(m, fs::path(file).parent_path().string(), m.enabledByDefault);
    return true;
}

int ModRegistry::scanDirectory(const std::string& dir, std::vector<std::string>& errors) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return -1;

    // Deterministic order: sort child directory names before loading.
    std::vector<fs::path> children;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) return -1;
        if (entry.is_directory()) children.push_back(entry.path());
    }
    std::sort(children.begin(), children.end());

    int loaded = 0;
    for (const auto& child : children) {
        fs::path manifest = child / "manifest.json";
        if (!fs::exists(manifest)) continue;
        std::string err;
        if (loadManifestFile(manifest.string(), err))
            ++loaded;
        else
            errors.push_back(err);
    }
    return loaded;
}

bool ModRegistry::setEnabled(const std::string& id, bool on) {
    for (auto& lm : mods_)
        if (lm.manifest.id == id) { lm.enabled = on; return true; }
    return false;
}

bool ModRegistry::loadState(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return true;  // no state yet: manifest defaults stand
    std::ostringstream ss;
    ss << in.rdbuf();
    JsonValue j;
    try {
        j = JsonParser::parse(ss.str());
    } catch (const std::exception&) {
        return false;
    }
    if (!j.isObject()) return false;
    for (auto& lm : mods_) {
        const JsonValue& v = j.get(lm.manifest.id);
        if (v.isBool()) lm.enabled = v.b;
    }
    return true;
}

bool ModRegistry::saveState(const std::string& file) const {
    JsonValue j = JsonValue::makeObject();
    for (const auto& lm : mods_)
        j.set(lm.manifest.id, JsonValue(lm.enabled));
    fs::path p(file);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << j.dump(2) << "\n";
    return (bool)out;
}

std::vector<std::string> ModRegistry::validate() const {
    std::vector<std::string> issues;
    std::unordered_set<std::string> seen;
    for (const auto& lm : mods_) {
        for (auto& e : validateModManifest(lm.manifest))
            issues.push_back(lm.manifest.id + ": " + e);
        if (!lm.manifest.id.empty() && !seen.insert(lm.manifest.id).second)
            issues.push_back("duplicate mod id: " + lm.manifest.id);
    }
    // An enabled mod may only depend on another enabled, present mod.
    for (const auto& lm : mods_) {
        if (!lm.enabled) continue;
        for (const auto& dep : lm.manifest.dependencies) {
            const LoadedMod* d = find(dep);
            if (!d)
                issues.push_back(lm.manifest.id + ": missing dependency '" + dep + "'");
            else if (!d->enabled)
                issues.push_back(lm.manifest.id + ": dependency '" + dep + "' is disabled");
        }
    }
    return issues;
}

bool ModRegistry::resolveOrder(std::vector<const LoadedMod*>& out, std::string& error) const {
    out.clear();
    std::unordered_map<std::string, const LoadedMod*> byId;
    for (const auto& lm : mods_)
        if (lm.enabled) byId[lm.manifest.id] = &lm;

    enum class Mark { None, Temp, Done };
    std::unordered_map<std::string, Mark> mark;

    // Iterative depth-first topological sort with cycle detection.
    struct Frame { const LoadedMod* mod; size_t next; };
    for (const auto& lm : mods_) {
        if (!lm.enabled || mark[lm.manifest.id] == Mark::Done) continue;
        std::vector<Frame> stack{{&lm, 0}};
        while (!stack.empty()) {
            Frame& f = stack.back();
            const std::string& id = f.mod->manifest.id;
            if (f.next == 0) {
                if (mark[id] == Mark::Done) { stack.pop_back(); continue; }
                mark[id] = Mark::Temp;
            }
            const auto& deps = f.mod->manifest.dependencies;
            if (f.next < deps.size()) {
                const std::string dep = deps[f.next++];
                auto it = byId.find(dep);
                if (it == byId.end()) {
                    error = "mod '" + id + "' depends on missing/disabled mod '" + dep + "'";
                    out.clear();
                    return false;
                }
                Mark dm = mark[dep];
                if (dm == Mark::Temp) {
                    error = "dependency cycle involving '" + dep + "'";
                    out.clear();
                    return false;
                }
                if (dm == Mark::None) stack.push_back({it->second, 0});
                continue;
            }
            mark[id] = Mark::Done;
            out.push_back(f.mod);
            stack.pop_back();
        }
    }
    return true;
}

}  // namespace prismatic
