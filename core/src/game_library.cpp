// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/game_library.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "prismatic/hash.hpp"

namespace prismatic {
namespace fs = std::filesystem;
namespace {

// 4th game-code letter -> language/region (standard NDS convention).
void regionFromCode(const std::string& code, std::string& lang, std::string& region) {
    char c = code.size() >= 4 ? code[3] : '?';
    switch (c) {
        case 'E': lang = "English";  region = "USA"; break;
        case 'P': lang = "English";  region = "Europe"; break;
        case 'J': lang = "Japanese"; region = "Japan"; break;
        case 'K': lang = "Korean";   region = "Korea"; break;
        case 'D': lang = "German";   region = "Europe"; break;
        case 'F': lang = "French";   region = "Europe"; break;
        case 'I': lang = "Italian";  region = "Europe"; break;
        case 'S': lang = "Spanish";  region = "Europe"; break;
        default:  lang = "Unknown";  region = "Unknown"; break;
    }
}

std::string nowIso() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

}  // namespace

const char* romVerdictName(RomVerdict v) {
    switch (v) {
        case RomVerdict::VerifiedNative: return "Verified";
        case RomVerdict::Identified:     return "Identified";
        case RomVerdict::Modified:       return "Modified";
        case RomVerdict::Unknown:        return "Unknown";
    }
    return "Unknown";
}

bool identifyRom(const std::string& path, RomIdentity& out, std::string& error) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { error = "cannot open " + path; return false; }
    std::streamsize sz = f.tellg();
    if (sz < 0x200) { error = "file too small to be an NDS ROM"; return false; }
    f.seekg(0);

    out = RomIdentity{};
    out.path = path;
    out.sizeBytes = (uint64_t)sz;

    // Hash the whole file in 1 MiB chunks.
    Sha256 sha;
    std::vector<uint8_t> buf(1 << 20);
    std::streamsize left = sz;
    while (left > 0) {
        std::streamsize take = std::min<std::streamsize>(left, (std::streamsize)buf.size());
        if (!f.read(reinterpret_cast<char*>(buf.data()), take)) {
            error = "read failed: " + path;
            return false;
        }
        sha.update(buf.data(), (size_t)take);
        left -= take;
    }
    out.sha256 = sha.hexDigest();

    // Header fields.
    f.clear();
    f.seekg(0);
    uint8_t hdr[0x200];
    if (!f.read(reinterpret_cast<char*>(hdr), sizeof(hdr))) {
        error = "cannot read header";
        return false;
    }
    char title[13] = {0};
    std::memcpy(title, hdr, 12);
    for (int i = 11; i >= 0 && (title[i] == ' ' || title[i] == 0); --i) title[i] = 0;
    out.title = title;
    char code[5] = {0};
    std::memcpy(code, hdr + 0x0C, 4);
    out.gameCode = code;
    out.revision = hdr[0x1E];
    regionFromCode(out.gameCode, out.language, out.region);
    // Header 0x14: chip capacity = 128KB << n. Smaller file = trimmed dump.
    uint64_t capacity = 0x20000ull << hdr[0x14];
    out.trimmed = out.sizeBytes < capacity;
    return true;
}

std::vector<KnownRom> builtinHgssDatabase() {
    // Hashes listed here are dumps verified on this project's own hardware
    // pipeline; header identification covers the rest of the retail set.
    std::vector<KnownRom> db;
    auto add = [&](const char* ed, const char* name, const char* code, uint8_t rev,
                   const char* sha, bool verified) {
        KnownRom k;
        k.family = "pokemon-hgss";
        k.edition = ed;
        k.displayName = name;
        k.gameCode = code;
        k.revision = rev;
        regionFromCode(code, k.language, k.region);
        k.sha256 = sha ? sha : "";
        k.nativeVerified = verified;
        db.push_back(k);
    };
    // Verified dumps (validated against Visual+ release manifest hashes).
    add("heartgold", "Pokémon HeartGold", "IPKE", 0,
        "65f02a56842b75aa92d775d56d657a56fe3fa993550b04dc20704ab82d760105", true);
    add("soulsilver", "Pokémon SoulSilver", "IPGE", 0,
        "51d0f94a16af7d77c067b4cb7d821ba890a13203a2e2c76049623332c0582e20", true);
    // Header-identified retail family (all regions; hash unknown = Identified).
    for (const char* c : {"IPKJ", "IPKP", "IPKD", "IPKF", "IPKI", "IPKS", "IPKK"})
        add("heartgold", "Pokémon HeartGold", c, 0, nullptr, false);
    for (const char* c : {"IPGJ", "IPGP", "IPGD", "IPGF", "IPGI", "IPGS", "IPGK"})
        add("soulsilver", "Pokémon SoulSilver", c, 0, nullptr, false);
    return db;
}

void classifyRom(RomIdentity& id, const std::vector<KnownRom>& db) {
    id.verdict = RomVerdict::Unknown;
    // Pass 1: exact hash.
    for (const auto& k : db)
        if (!k.sha256.empty() && k.sha256 == id.sha256) {
            id.verdict = RomVerdict::VerifiedNative;
            id.family = k.family;
            id.edition = k.edition;
            id.displayName = k.displayName;
            return;
        }
    // Pass 2: header identity. A known code whose clean hash is known but
    // different => Modified; hash unknown => Identified.
    for (const auto& k : db)
        if (k.gameCode == id.gameCode && k.revision == id.revision) {
            bool haveClean = !k.sha256.empty();
            id.verdict = haveClean ? RomVerdict::Modified : RomVerdict::Identified;
            id.family = k.family;
            id.edition = k.edition;
            id.displayName = k.displayName;
            return;
        }
}

bool loadRomDatabase(const std::string& path, std::vector<KnownRom>& out,
                     std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open " + path; return false; }
    std::ostringstream ss;
    ss << f.rdbuf();
    JsonValue j;
    try {
        j = JsonParser::parse(ss.str());
    } catch (const std::exception& e) {
        error = std::string("JSON: ") + e.what();
        return false;
    }
    const JsonValue& arr = j.get("roms");
    if (!arr.isArray()) { error = "missing 'roms' array"; return false; }
    for (const auto& o : *arr.arr) {
        KnownRom k;
        k.family = o.get("family").asString("");
        k.edition = o.get("edition").asString("");
        k.displayName = o.get("displayName").asString("");
        k.gameCode = o.get("gameCode").asString("");
        k.revision = (uint8_t)o.get("revision").asInt(0);
        k.language = o.get("language").asString("");
        k.region = o.get("region").asString("");
        k.sha256 = o.get("sha256").asString("");
        k.nativeVerified = o.get("nativeVerified").asBool(false);
        out.push_back(k);
    }
    return true;
}

// ---- installs ----------------------------------------------------------------

JsonValue gameInstallToJson(const GameInstall& g) {
    JsonValue j = JsonValue::makeObject();
    j.set("id", g.id);
    j.set("family", g.family);
    j.set("edition", g.edition);
    j.set("displayName", g.displayName);
    j.set("sourceRomPath", g.sourceRomPath);
    j.set("sourceSha256", g.sourceSha256);
    j.set("installDir", g.installDir);
    j.set("activeProfile", g.activeProfile);
    j.set("runtimeMode", g.runtimeMode);
    JsonValue mods = JsonValue::makeArray();
    for (auto& m : g.enabledMods) mods.push(JsonValue(m));
    j.set("enabledMods", mods);
    j.set("playRomPath", g.playRomPath);
    j.set("lastPlayedIso", g.lastPlayedIso);
    j.set("playSeconds", (double)g.playSeconds);
    return j;
}

bool gameInstallFromJson(const JsonValue& j, GameInstall& out) {
    if (!j.isObject()) return false;
    out.id = j.get("id").asString("");
    out.family = j.get("family").asString("");
    out.edition = j.get("edition").asString("");
    out.displayName = j.get("displayName").asString("");
    out.sourceRomPath = j.get("sourceRomPath").asString("");
    out.sourceSha256 = j.get("sourceSha256").asString("");
    out.installDir = j.get("installDir").asString("");
    out.activeProfile = j.get("activeProfile").asString("vanilla");
    out.runtimeMode = j.get("runtimeMode").asString("emulation");
    const JsonValue& mods = j.get("enabledMods");
    if (mods.isArray())
        for (auto& m : *mods.arr)
            if (m.isString()) out.enabledMods.push_back(m.str);
    out.playRomPath = j.get("playRomPath").asString("");
    out.lastPlayedIso = j.get("lastPlayedIso").asString("");
    out.playSeconds = (int64_t)j.get("playSeconds").asNumber(0);
    return !out.id.empty();
}

std::string GameLibrary::installsRoot() const {
    return (fs::path(dataDir_) / "installs").string();
}

bool GameLibrary::load(std::string& error) {
    games_.clear();
    fs::path file = fs::path(dataDir_) / "library.json";
    std::ifstream f(file, std::ios::binary);
    if (!f) return true;  // empty library is not an error
    std::ostringstream ss;
    ss << f.rdbuf();
    JsonValue j;
    try {
        j = JsonParser::parse(ss.str());
    } catch (const std::exception& e) {
        error = std::string("library.json: ") + e.what();
        return false;
    }
    const JsonValue& arr = j.get("games");
    if (arr.isArray())
        for (const auto& o : *arr.arr) {
            GameInstall g;
            if (gameInstallFromJson(o, g)) games_.push_back(std::move(g));
        }
    return true;
}

bool GameLibrary::save(std::string& error) const {
    std::error_code ec;
    fs::create_directories(dataDir_, ec);
    JsonValue j = JsonValue::makeObject();
    j.set("version", 1);
    JsonValue arr = JsonValue::makeArray();
    for (const auto& g : games_) arr.push(gameInstallToJson(g));
    j.set("games", arr);
    fs::path file = fs::path(dataDir_) / "library.json";
    std::ofstream o(file, std::ios::binary | std::ios::trunc);
    if (!o) { error = "cannot write " + file.string(); return false; }
    o << j.dump(2) << "\n";
    return (bool)o;
}

bool GameLibrary::importRom(const RomIdentity& id, GameInstall& out, std::string& error) {
    if (id.family.empty()) { error = "ROM is not a recognised game"; return false; }
    std::string iid = id.edition + "_" + id.sha256.substr(0, 8);
    if (find(iid)) { error = "already installed"; return false; }

    GameInstall g;
    g.id = iid;
    g.family = id.family;
    g.edition = id.edition;
    g.displayName = id.displayName;
    g.sourceRomPath = id.path;
    g.sourceSha256 = id.sha256;
    g.installDir = (fs::path(installsRoot()) / iid).string();
    g.activeProfile = "vanilla";
    g.runtimeMode = "emulation";
    g.lastPlayedIso = "";

    std::error_code ec;
    fs::create_directories(fs::path(g.installDir) / "saves", ec);
    fs::create_directories(fs::path(g.installDir) / "states", ec);
    fs::create_directories(fs::path(g.installDir) / "builds", ec);
    if (ec) { error = "cannot create install dir: " + g.installDir; return false; }

    // Vanilla build = verbatim private copy (source ROM stays untouched, and
    // later mod builds never overwrite it).
    fs::path vanilla = fs::path(g.installDir) / "builds" / "vanilla.nds";
    fs::copy_file(id.path, vanilla, fs::copy_options::overwrite_existing, ec);
    if (ec) { error = "cannot copy ROM into private install"; return false; }
    g.playRomPath = vanilla.string();

    games_.push_back(g);
    out = games_.back();
    (void)nowIso;
    return true;
}

bool GameLibrary::remove(const std::string& installId, std::string& error) {
    auto it = std::find_if(games_.begin(), games_.end(),
                           [&](const GameInstall& g) { return g.id == installId; });
    if (it == games_.end()) { error = "no such install"; return false; }
    // Deliberately keep the install directory (saves!) — the record alone is
    // removed. Directory cleanup is a separate, explicit user action.
    games_.erase(it);
    return true;
}

GameInstall* GameLibrary::find(const std::string& installId) {
    for (auto& g : games_)
        if (g.id == installId) return &g;
    return nullptr;
}

}  // namespace prismatic
