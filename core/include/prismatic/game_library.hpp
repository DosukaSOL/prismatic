// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — game library: ROM identity, verification and private
// installations.
//
// The gen1recomp-style contract: the user's clean ROM is read, hashed and
// identified, then never touched again. Every playable configuration (vanilla
// or modded) is a separate, privately generated working copy under the app's
// data directory. Records are JSON; no ROM bytes ever enter the repository.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "prismatic/json.hpp"

namespace prismatic {

// ---- ROM identity -----------------------------------------------------------

enum class RomVerdict {
    VerifiedNative,   // exact hash in the database, native/known mapping
    Identified,       // header identifies a known game, hash not in database
    Modified,         // header matches but hash differs from a known clean set
    Unknown,          // not recognised at all
};

const char* romVerdictName(RomVerdict v);

struct RomIdentity {
    std::string path;
    uint64_t sizeBytes = 0;
    std::string sha256;
    // NDS header fields.
    std::string title;      // 12-char cartridge title, trimmed
    std::string gameCode;   // e.g. IPKE
    uint8_t revision = 0;   // header 0x1E
    std::string language;   // derived from 4th game-code letter
    std::string region;     // derived from 4th game-code letter
    bool trimmed = false;   // file smaller than header-declared capacity
    RomVerdict verdict = RomVerdict::Unknown;
    std::string family;     // e.g. "pokemon-hgss"
    std::string edition;    // "heartgold" | "soulsilver"
    std::string displayName;
};

// Parse the header + hash the file. Returns false only on I/O failure.
bool identifyRom(const std::string& path, RomIdentity& out, std::string& error);

// ---- known-game database ----------------------------------------------------

struct KnownRom {
    std::string family;
    std::string edition;
    std::string displayName;
    std::string gameCode;
    uint8_t revision = 0;
    std::string language;
    std::string region;
    std::string sha256;    // empty = identify by code+revision only
    bool nativeVerified = false;
};

// Built-in HGSS identity set (extend via compatibility/hgss-rom-database.json).
std::vector<KnownRom> builtinHgssDatabase();

// Apply database knowledge to a parsed identity (sets verdict/family/edition).
void classifyRom(RomIdentity& id, const std::vector<KnownRom>& db);

// Load extra entries from a JSON database file (merged after builtin).
bool loadRomDatabase(const std::string& path, std::vector<KnownRom>& out,
                     std::string& error);

// ---- private installations --------------------------------------------------

// One generated working configuration of a game (vanilla or modded).
struct GameInstall {
    std::string id;             // stable: <edition>_<sha8>
    std::string family;
    std::string edition;
    std::string displayName;
    std::string sourceRomPath;  // user's clean ROM (never modified)
    std::string sourceSha256;
    std::string installDir;     // private working directory
    std::string activeProfile;  // mod profile id, e.g. "vanilla", "visual-plus"
    std::string runtimeMode;    // "emulation" | "native-beta"
    std::vector<std::string> enabledMods;
    std::string playRomPath;    // the ROM actually launched (generated copy)
    std::string lastPlayedIso;  // ISO-8601 timestamp ("" = never)
    int64_t playSeconds = 0;
};

JsonValue gameInstallToJson(const GameInstall& g);
bool gameInstallFromJson(const JsonValue& j, GameInstall& out);

// Library = every installed game, persisted as JSON in <dataDir>/library.json.
class GameLibrary {
public:
    explicit GameLibrary(std::string dataDir) : dataDir_(std::move(dataDir)) {}

    bool load(std::string& error);
    bool save(std::string& error) const;

    // Import a verified ROM: creates the install record + private directory
    // (vanilla configuration = a verbatim private copy of the ROM).
    bool importRom(const RomIdentity& id, GameInstall& out, std::string& error);

    bool remove(const std::string& installId, std::string& error);
    GameInstall* find(const std::string& installId);
    const std::vector<GameInstall>& games() const { return games_; }
    std::vector<GameInstall>& games() { return games_; }

    std::string installsRoot() const;

private:
    std::string dataDir_;
    std::vector<GameInstall> games_;
};

}  // namespace prismatic
