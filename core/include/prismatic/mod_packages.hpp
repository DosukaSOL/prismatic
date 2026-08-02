// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — game packages and mod profiles.
//
// A .prismod package is a JSON manifest describing an optional game
// modification (never a ROM). For patch-based mods (like Visual+) the
// manifest lists per-edition VCDIFF patches, source hashes and expected
// output hashes; Prismatic applies them privately with vcdiff.cpp. Mod
// profiles bundle a named combination (VANILLA / VISUAL+ / CONSERVATIVE /
// CUSTOM) that maps to one generated private build per install.
#pragma once
#include <string>
#include <vector>
#include "prismatic/json.hpp"
#include "prismatic/game_library.hpp"

namespace prismatic {

constexpr int kPrismodVersion = 1;

// One selectable option inside a mod (e.g. camera variant).
struct ModOption {
    std::string id;             // "camera"
    std::string name;           // "Camera"
    std::vector<std::string> values;  // ["original","conservative","full"]
    std::string defaultValue;
};

// A concrete patch artifact for one edition + option combination.
struct ModArtifact {
    std::string edition;        // "heartgold" | "soulsilver"
    std::string variant;        // e.g. "full", "conservative-camera"
    std::string patchFile;      // relative to the package directory
    std::string patchSha256;
    std::string sourceRomSha256;   // required clean ROM
    std::string patchedRomSha256;  // expected output (verified after apply)
};

struct ModPackage {
    int version = kPrismodVersion;
    std::string id;             // "visual-plus-hgss"
    std::string name;           // "Visual+ (English port)"
    std::string modVersion;     // "1.0.0"
    std::string author;
    std::string license;
    std::string sourceRepo;     // canonical upstream, e.g. DosukaSOL/pokemon-hgss-visual-mod
    std::string family;         // "pokemon-hgss"
    std::string kind = "rom-patch";  // rom-patch | runtime | texture
    bool restartRequired = true;
    std::string saveImpact = "none"; // none | states-invalid | saves-affected
    std::vector<std::string> components;  // human-readable feature list
    std::vector<ModOption> options;
    std::vector<ModArtifact> artifacts;
    std::string packageDir;    // where the manifest was loaded from
};

bool parseModPackage(const std::string& json, ModPackage& out, std::string& error);
bool loadModPackageFile(const std::string& manifestPath, ModPackage& out,
                        std::string& error);
std::string serializeModPackage(const ModPackage& m);

// Pick the artifact matching an edition + variant ("" variant = first match).
const ModArtifact* findArtifact(const ModPackage& m, const std::string& edition,
                                const std::string& variant);

// ---- profiles ----------------------------------------------------------------

// A named, per-game mod combination. Building a profile produces (and hash-
// verifies) a private ROM build; the clean source is never modified.
struct ModProfile {
    std::string id;        // "vanilla" | "visual-plus" | "conservative" | custom
    std::string name;
    std::string modId;     // "" for vanilla
    std::string variant;   // artifact variant when modId set
};

// Built-in profiles for the HGSS family (mirrors the Visual+ release set).
std::vector<ModProfile> hgssBuiltinProfiles();

// Ensure the private build for `profile` exists inside install.installDir
// (builds/<profile>.nds), applying + verifying patches as needed. Updates
// install.playRomPath/activeProfile on success.
bool buildProfile(GameInstall& install, const ModProfile& profile,
                  const std::vector<ModPackage>& packages, std::string& error);

}  // namespace prismatic
