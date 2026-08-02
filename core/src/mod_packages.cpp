// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/mod_packages.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "prismatic/hash.hpp"
#include "prismatic/vcdiff.hpp"

namespace prismatic {
namespace fs = std::filesystem;
namespace {

std::string fileSha256(const std::string& path, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open " + path; return ""; }
    Sha256 h;
    std::vector<uint8_t> buf(1 << 20);
    while (f) {
        f.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)buf.size());
        std::streamsize got = f.gcount();
        if (got > 0) h.update(buf.data(), (size_t)got);
    }
    return h.hexDigest();
}

}  // namespace

bool parseModPackage(const std::string& json, ModPackage& out, std::string& error) {
    JsonValue j;
    try {
        j = JsonParser::parse(json);
    } catch (const std::exception& e) {
        error = std::string("JSON: ") + e.what();
        return false;
    }
    if (!j.isObject()) { error = "manifest root must be an object"; return false; }
    ModPackage m;
    m.version = j.get("version").asInt(0);
    if (m.version <= 0 || m.version > kPrismodVersion) {
        error = "unsupported prismod version";
        return false;
    }
    m.id = j.get("id").asString("");
    m.name = j.get("name").asString("");
    m.modVersion = j.get("modVersion").asString("");
    m.author = j.get("author").asString("");
    m.license = j.get("license").asString("");
    m.sourceRepo = j.get("sourceRepo").asString("");
    m.family = j.get("family").asString("");
    m.kind = j.get("kind").asString("rom-patch");
    m.restartRequired = j.get("restartRequired").asBool(true);
    m.saveImpact = j.get("saveImpact").asString("none");
    const JsonValue& comps = j.get("components");
    if (comps.isArray())
        for (auto& c : *comps.arr)
            if (c.isString()) m.components.push_back(c.str);
    const JsonValue& opts = j.get("options");
    if (opts.isArray())
        for (auto& o : *opts.arr) {
            ModOption op;
            op.id = o.get("id").asString("");
            op.name = o.get("name").asString("");
            op.defaultValue = o.get("default").asString("");
            const JsonValue& vals = o.get("values");
            if (vals.isArray())
                for (auto& v : *vals.arr)
                    if (v.isString()) op.values.push_back(v.str);
            m.options.push_back(op);
        }
    const JsonValue& arts = j.get("artifacts");
    if (arts.isArray())
        for (auto& a : *arts.arr) {
            ModArtifact ar;
            ar.edition = a.get("edition").asString("");
            ar.variant = a.get("variant").asString("");
            ar.patchFile = a.get("patchFile").asString("");
            ar.patchSha256 = a.get("patchSha256").asString("");
            ar.sourceRomSha256 = a.get("sourceRomSha256").asString("");
            ar.patchedRomSha256 = a.get("patchedRomSha256").asString("");
            m.artifacts.push_back(ar);
        }
    if (m.id.empty()) { error = "missing id"; return false; }
    out = std::move(m);
    return true;
}

bool loadModPackageFile(const std::string& manifestPath, ModPackage& out,
                        std::string& error) {
    std::ifstream f(manifestPath, std::ios::binary);
    if (!f) { error = "cannot open " + manifestPath; return false; }
    std::ostringstream ss;
    ss << f.rdbuf();
    if (!parseModPackage(ss.str(), out, error)) {
        error = manifestPath + ": " + error;
        return false;
    }
    out.packageDir = fs::path(manifestPath).parent_path().string();
    return true;
}

std::string serializeModPackage(const ModPackage& m) {
    JsonValue j = JsonValue::makeObject();
    j.set("version", m.version);
    j.set("kind", m.kind);
    j.set("id", m.id);
    j.set("name", m.name);
    j.set("modVersion", m.modVersion);
    j.set("author", m.author);
    j.set("license", m.license);
    j.set("sourceRepo", m.sourceRepo);
    j.set("family", m.family);
    j.set("restartRequired", m.restartRequired);
    j.set("saveImpact", m.saveImpact);
    JsonValue comps = JsonValue::makeArray();
    for (auto& c : m.components) comps.push(JsonValue(c));
    j.set("components", comps);
    JsonValue opts = JsonValue::makeArray();
    for (auto& o : m.options) {
        JsonValue jo = JsonValue::makeObject();
        jo.set("id", o.id);
        jo.set("name", o.name);
        jo.set("default", o.defaultValue);
        JsonValue vals = JsonValue::makeArray();
        for (auto& v : o.values) vals.push(JsonValue(v));
        jo.set("values", vals);
        opts.push(jo);
    }
    j.set("options", opts);
    JsonValue arts = JsonValue::makeArray();
    for (auto& a : m.artifacts) {
        JsonValue ja = JsonValue::makeObject();
        ja.set("edition", a.edition);
        ja.set("variant", a.variant);
        ja.set("patchFile", a.patchFile);
        ja.set("patchSha256", a.patchSha256);
        ja.set("sourceRomSha256", a.sourceRomSha256);
        ja.set("patchedRomSha256", a.patchedRomSha256);
        arts.push(ja);
    }
    j.set("artifacts", arts);
    return j.dump(2);
}

const ModArtifact* findArtifact(const ModPackage& m, const std::string& edition,
                                const std::string& variant) {
    for (const auto& a : m.artifacts)
        if (a.edition == edition && (variant.empty() || a.variant == variant))
            return &a;
    return nullptr;
}

std::vector<ModProfile> hgssBuiltinProfiles() {
    return {
        {"vanilla", "Vanilla", "", ""},
        {"visual-plus", "Visual+", "visual-plus-hgss", "full"},
        {"conservative", "Conservative", "visual-plus-hgss", "conservative-camera"},
        {"visual-only", "Visual Only", "visual-plus-hgss", "visual-only"},
        {"safe", "Safe", "visual-plus-hgss", "safe"},
    };
}

bool buildProfile(GameInstall& install, const ModProfile& profile,
                  const std::vector<ModPackage>& packages, std::string& error) {
    fs::path buildDir = fs::path(install.installDir) / "builds";
    std::error_code ec;
    fs::create_directories(buildDir, ec);

    // Vanilla: the verbatim private copy made at import.
    if (profile.modId.empty()) {
        fs::path vanilla = buildDir / "vanilla.nds";
        if (!fs::exists(vanilla)) {
            fs::copy_file(install.sourceRomPath, vanilla,
                          fs::copy_options::overwrite_existing, ec);
            if (ec) { error = "cannot restore vanilla build"; return false; }
        }
        install.playRomPath = vanilla.string();
        install.activeProfile = profile.id;
        install.enabledMods.clear();
        return true;
    }

    const ModPackage* pkg = nullptr;
    for (const auto& p : packages)
        if (p.id == profile.modId) { pkg = &p; break; }
    if (!pkg) { error = "mod package not installed: " + profile.modId; return false; }

    const ModArtifact* art = findArtifact(*pkg, install.edition, profile.variant);
    if (!art) {
        error = "no " + profile.variant + " artifact for " + install.edition;
        return false;
    }
    if (!art->sourceRomSha256.empty() && art->sourceRomSha256 != install.sourceSha256) {
        error = "mod requires a different clean ROM revision (hash mismatch)";
        return false;
    }

    fs::path out = buildDir / (profile.id + ".nds");
    if (fs::exists(out) && !art->patchedRomSha256.empty()) {
        std::string herr;
        if (fileSha256(out.string(), herr) == art->patchedRomSha256) {
            install.playRomPath = out.string();     // cached build still valid
            install.activeProfile = profile.id;
            install.enabledMods = {pkg->id + "@" + profile.variant};
            return true;
        }
    }

    fs::path patch = fs::path(pkg->packageDir) / art->patchFile;
    std::string herr;
    if (!art->patchSha256.empty()) {
        std::string got = fileSha256(patch.string(), herr);
        if (got.empty()) { error = herr; return false; }
        if (got != art->patchSha256) { error = "patch file corrupted (hash mismatch)"; return false; }
    }
    if (!vcdiffApplyFile(install.sourceRomPath, patch.string(), out.string(), error))
        return false;
    if (!art->patchedRomSha256.empty()) {
        std::string got = fileSha256(out.string(), herr);
        if (got != art->patchedRomSha256) {
            fs::remove(out, ec);
            error = "patched output failed verification";
            return false;
        }
    }
    install.playRomPath = out.string();
    install.activeProfile = profile.id;
    install.enabledMods = {pkg->id + "@" + profile.variant};
    return true;
}

}  // namespace prismatic
