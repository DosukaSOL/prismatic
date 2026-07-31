// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — mod registry.
//
// A mod is a versioned, dependency-free JSON manifest that advertises a unit of
// functionality (a renderer, a preset pack, a palette, a scene source, …)
// WITHOUT embedding any game assets. Manifests round-trip through json.hpp just
// like presets and profiles, so the same copyright-safe, deterministic rules
// apply. The registry discovers mods on disk (mods/<id>/manifest.json), tracks
// which are enabled, validates them, and resolves an enabled load order by
// dependency precedence. The built-in voxel diorama renderer ships as the
// reference graphics mod.
#pragma once
#include <string>
#include <vector>
#include "prismatic/json.hpp"

namespace prismatic {

constexpr int kModManifestVersion = 1;

// The subsystem a mod plugs into. Unknown is rejected by validation.
enum class ModKind { Unknown, Graphics, Preset, Profile, Palette, Scene, Audio };

const char* modKindName(ModKind k);
ModKind modKindFromString(const std::string& s);

struct ModManifest {
    int version = kModManifestVersion;
    std::string id;                        // stable unique id, e.g. "prismatic.voxel-diorama"
    std::string name;                      // human-readable title
    std::string modVersion = "0.1.0";      // semver of the mod itself
    std::string author;
    std::string license = "GPL-3.0-or-later";
    std::string description;
    ModKind kind = ModKind::Unknown;
    std::string entry;                     // implementation hook (symbol / lib path / data file)
    std::vector<std::string> provides;     // capability tags this mod exposes
    std::vector<std::string> dependencies; // ids of mods that must load first
    bool enabledByDefault = true;
    bool builtin = false;                  // shipped in-tree (may be disabled, never removed)
};

// JSON round-trip (mirrors presetToJson / parseProfile).
JsonValue modManifestToJson(const ModManifest& m);
bool parseModManifest(const std::string& json, ModManifest& out, std::string& error);
std::string serializeModManifest(const ModManifest& m);

// Structural validation of a single manifest; returns issues (empty = valid).
std::vector<std::string> validateModManifest(const ModManifest& m);

struct LoadedMod {
    ModManifest manifest;
    std::string path;      // directory containing manifest.json ("" for programmatic/builtin)
    bool enabled = true;
};

// Discovers, tracks and orders mods. Purely data-driven: no mod behaviour lives
// here, only its manifest and enable state.
class ModRegistry {
public:
    // Load a single manifest.json. On success appends a LoadedMod; on failure
    // fills `error` and returns false. Rejects a duplicate id.
    bool loadManifestFile(const std::string& file, std::string& error);

    // Scan a directory whose children are mod folders (each with a manifest.json).
    // Returns the number of mods loaded, or -1 if the directory can't be read.
    // Per-mod failures are collected in `errors` and skipped (scan continues).
    int scanDirectory(const std::string& dir, std::vector<std::string>& errors);

    // Programmatic insertion (tests / built-ins). Ignores duplicates.
    bool add(const ModManifest& m, const std::string& path = "", bool enabled = true);

    const std::vector<LoadedMod>& mods() const { return mods_; }
    const LoadedMod* find(const std::string& id) const;

    // Toggle a mod's enable flag. Returns false if the id is unknown.
    bool setEnabled(const std::string& id, bool on);

    // Persisted enable state: a JSON object { "<id>": true/false }. A missing
    // file is not an error (defaults from each manifest's enabledByDefault stand).
    bool loadState(const std::string& file);
    bool saveState(const std::string& file) const;

    // All issues across the registry (bad manifests, duplicate ids, deps that
    // point at a missing or disabled mod). Empty = healthy.
    std::vector<std::string> validate() const;

    // Enabled mods in dependency order. Returns false + error on a dependency
    // cycle or a dependency on a missing/disabled mod.
    bool resolveOrder(std::vector<const LoadedMod*>& out, std::string& error) const;

private:
    std::vector<LoadedMod> mods_;
};

}  // namespace prismatic
