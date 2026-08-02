// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — game platform CLI.
//
//   prism identify <rom>                       hash + classify a ROM
//   prism import <rom> [--data DIR]            add to the library (private copy)
//   prism list [--data DIR]                    show the library
//   prism mods [--data DIR]                    show installed mod packages
//   prism profile <installId> <profileId>      build/switch a mod profile
//   prism apply <src> <patch.xdelta> <out>     raw VCDIFF apply (testing)
//
// Mod packages live in <data>/packages/<id>/manifest.prismod.json.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "prismatic/game_library.hpp"
#include "prismatic/mod_packages.hpp"
#include "prismatic/nitrofs.hpp"
#include "prismatic/vcdiff.hpp"

using namespace prismatic;
namespace fs = std::filesystem;

namespace {

std::vector<ModPackage> loadPackages(const std::string& dataDir) {
    std::vector<ModPackage> out;
    fs::path root = fs::path(dataDir) / "packages";
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return out;
    for (auto& e : fs::directory_iterator(root, ec)) {
        if (!e.is_directory()) continue;
        fs::path man = e.path() / "manifest.prismod.json";
        if (!fs::exists(man)) continue;
        ModPackage p;
        std::string err;
        if (loadModPackageFile(man.string(), p, err))
            out.push_back(std::move(p));
        else
            std::fprintf(stderr, "warning: %s\n", err.c_str());
    }
    return out;
}

int cmdIdentify(const std::string& rom) {
    RomIdentity id;
    std::string err;
    if (!identifyRom(rom, id, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    classifyRom(id, builtinHgssDatabase());
    std::printf("file:      %s\n", id.path.c_str());
    std::printf("size:      %llu bytes%s\n", (unsigned long long)id.sizeBytes,
                id.trimmed ? " (trimmed)" : "");
    std::printf("sha256:    %s\n", id.sha256.c_str());
    std::printf("title:     %s\n", id.title.c_str());
    std::printf("gameCode:  %s rev %d\n", id.gameCode.c_str(), id.revision);
    std::printf("language:  %s (%s)\n", id.language.c_str(), id.region.c_str());
    std::printf("verdict:   %s\n", romVerdictName(id.verdict));
    if (!id.displayName.empty())
        std::printf("game:      %s [%s]\n", id.displayName.c_str(), id.edition.c_str());
    return 0;
}

int cmdImport(const std::string& rom, const std::string& dataDir) {
    RomIdentity id;
    std::string err;
    if (!identifyRom(rom, id, err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    classifyRom(id, builtinHgssDatabase());
    if (id.verdict == RomVerdict::Unknown) {
        std::fprintf(stderr, "error: unrecognised ROM (only HGSS supported here)\n");
        return 1;
    }
    GameLibrary lib(dataDir);
    if (!lib.load(err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    GameInstall g;
    if (!lib.importRom(id, g, err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    if (!lib.save(err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    std::printf("installed %s (%s)\n  id: %s\n  dir: %s\n  verdict: %s\n",
                g.displayName.c_str(), g.edition.c_str(), g.id.c_str(),
                g.installDir.c_str(), romVerdictName(id.verdict));
    return 0;
}

int cmdList(const std::string& dataDir) {
    GameLibrary lib(dataDir);
    std::string err;
    if (!lib.load(err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    if (lib.games().empty()) { std::printf("library empty\n"); return 0; }
    for (const auto& g : lib.games())
        std::printf("%-24s %-11s profile=%-14s mode=%-10s %s\n", g.id.c_str(),
                    g.edition.c_str(), g.activeProfile.c_str(), g.runtimeMode.c_str(),
                    g.displayName.c_str());
    return 0;
}

int cmdMods(const std::string& dataDir) {
    auto pkgs = loadPackages(dataDir);
    if (pkgs.empty()) { std::printf("no mod packages installed\n"); return 0; }
    for (const auto& p : pkgs) {
        std::printf("%s %s (%s) — %s\n", p.id.c_str(), p.modVersion.c_str(),
                    p.kind.c_str(), p.name.c_str());
        std::printf("  source: %s\n  artifacts: %zu\n", p.sourceRepo.c_str(),
                    p.artifacts.size());
    }
    return 0;
}

// List the cartridge filesystem, or extract it (optionally unpacking NARCs).
int cmdExtract(const std::string& rom, const std::string& outDir, bool listOnly,
               bool unpackNarcs) {
    std::ifstream f(rom, std::ios::binary | std::ios::ate);
    if (!f) { std::fprintf(stderr, "error: cannot open %s\n", rom.c_str()); return 1; }
    std::streamsize sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> img((size_t)sz);
    if (!f.read(reinterpret_cast<char*>(img.data()), sz)) {
        std::fprintf(stderr, "error: read failed\n");
        return 1;
    }
    NitroFsResult fs = parseNitroFs(img);
    if (!fs.ok) { std::fprintf(stderr, "error: %s\n", fs.error.c_str()); return 1; }

    uint64_t total = 0;
    int narcs = 0;
    for (const auto& file : fs.files) total += file.size;
    if (listOnly) {
        for (const auto& file : fs.files)
            std::printf("%5u %9u %s\n", file.id, file.size, file.path.c_str());
        std::printf("%zu files, %llu bytes\n", fs.files.size(), (unsigned long long)total);
        return 0;
    }

    int written = 0, subfiles = 0;
    for (const auto& file : fs.files) {
        std::vector<uint8_t> data;
        if (!readNitroFile(img, file, data)) continue;
        fs::path dst = fs::path(outDir) / file.path;
        fs::create_directories(dst.parent_path());
        std::ofstream o(dst, std::ios::binary | std::ios::trunc);
        o.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
        ++written;
        if (unpackNarcs && data.size() >= 4 && std::memcmp(data.data(), "NARC", 4) == 0) {
            NarcResult nr = parseNarc(data);
            if (nr.ok) {
                ++narcs;
                fs::path ndir = dst;
                ndir += ".d";
                fs::create_directories(ndir);
                for (size_t i = 0; i < nr.entries.size(); ++i) {
                    std::vector<uint8_t> sub;
                    if (!readNarcEntry(data, nr, i, sub)) continue;
                    std::ofstream so(ndir / (std::to_string(i) + ".bin"),
                                     std::ios::binary | std::ios::trunc);
                    so.write(reinterpret_cast<const char*>(sub.data()),
                             (std::streamsize)sub.size());
                    ++subfiles;
                }
            }
        }
    }
    std::printf("extracted %d files (%llu bytes)", written, (unsigned long long)total);
    if (unpackNarcs) std::printf(", unpacked %d NARCs (%d subfiles)", narcs, subfiles);
    std::printf(" -> %s\n", outDir.c_str());
    return 0;
}

int cmdProfile(const std::string& dataDir, const std::string& installId,
               const std::string& profileId) {
    GameLibrary lib(dataDir);
    std::string err;
    if (!lib.load(err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    GameInstall* g = lib.find(installId);
    if (!g) { std::fprintf(stderr, "error: no install '%s'\n", installId.c_str()); return 1; }
    const ModProfile* prof = nullptr;
    auto profiles = hgssBuiltinProfiles();
    for (const auto& p : profiles)
        if (p.id == profileId) { prof = &p; break; }
    if (!prof) {
        std::fprintf(stderr, "error: unknown profile '%s' (", profileId.c_str());
        for (auto& p : profiles) std::fprintf(stderr, "%s ", p.id.c_str());
        std::fprintf(stderr, ")\n");
        return 1;
    }
    auto pkgs = loadPackages(dataDir);
    if (!buildProfile(*g, *prof, pkgs, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    if (!lib.save(err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    std::printf("profile '%s' active for %s\n  play ROM: %s\n", prof->id.c_str(),
                g->id.c_str(), g->playRomPath.c_str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string dataDir = "local_data/prism";
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--data" && i + 1 < argc) dataDir = argv[++i];
        else args.push_back(a);
    }
    if (args.empty()) {
        std::printf("usage: prism <identify|import|list|mods|profile|apply> ...\n");
        return 2;
    }
    const std::string& cmd = args[0];
    if (cmd == "identify" && args.size() >= 2) return cmdIdentify(args[1]);
    if (cmd == "import" && args.size() >= 2) return cmdImport(args[1], dataDir);
    if (cmd == "list") return cmdList(dataDir);
    if (cmd == "mods") return cmdMods(dataDir);
    if (cmd == "profile" && args.size() >= 3) return cmdProfile(dataDir, args[1], args[2]);
    if (cmd == "extract" && args.size() >= 2) {
        bool list = false, narcs = false;
        std::string out = "local_data/extract";
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "--list") list = true;
            else if (args[i] == "--narcs") narcs = true;
            else if (args[i] == "--out" && i + 1 < args.size()) out = args[++i];
        }
        return cmdExtract(args[1], out, list, narcs);
    }
    if (cmd == "apply" && args.size() >= 4) {
        std::string err;
        if (!vcdiffApplyFile(args[1], args[2], args[3], err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        std::printf("wrote %s\n", args[3].c_str());
        return 0;
    }
    std::fprintf(stderr, "error: bad arguments\n");
    return 2;
}
