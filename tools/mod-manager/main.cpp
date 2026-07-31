// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC mod manager — inspect and toggle graphics/data mods.
//
//   mod-manager [--dir <mods>] <command> [args]
//
//   list                 list every discovered mod and its enable state
//   info <id>            print one mod's full manifest
//   validate             report manifest / dependency problems (exit 1 if any)
//   order                print the enabled mods in dependency load order
//   enable  <id>         mark a mod enabled  (persists to <dir>/mods.state.json)
//   disable <id>         mark a mod disabled (persists to <dir>/mods.state.json)
//
// The registry is data-driven: this tool only reads manifests and writes an
// enable-state file. It never touches game assets.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "prismatic/mod_registry.hpp"

using namespace prismatic;

namespace {

const char* kStateName = "mods.state.json";

void usage() {
    std::printf(
        "usage: mod-manager [--dir <mods>] <command> [args]\n"
        "  list                 list discovered mods and enable state\n"
        "  info <id>            print a mod's manifest\n"
        "  validate             report manifest / dependency problems\n"
        "  order                print enabled mods in dependency order\n"
        "  enable  <id>         enable a mod (persisted)\n"
        "  disable <id>         disable a mod (persisted)\n");
}

std::string stateFile(const std::string& dir) {
    return dir + "/" + kStateName;
}

// Load every mod under `dir`, then apply persisted enable state. Prints scan
// errors to stderr but keeps going. Returns false only if the dir is unreadable.
bool loadRegistry(ModRegistry& reg, const std::string& dir) {
    std::vector<std::string> errs;
    int n = reg.scanDirectory(dir, errs);
    for (const auto& e : errs) std::fprintf(stderr, "warning: %s\n", e.c_str());
    if (n < 0) {
        std::fprintf(stderr, "error: cannot read mods directory '%s'\n", dir.c_str());
        return false;
    }
    reg.loadState(stateFile(dir));
    return true;
}

int cmdList(const std::string& dir) {
    ModRegistry reg;
    if (!loadRegistry(reg, dir)) return 1;
    const auto& mods = reg.mods();
    if (mods.empty()) {
        std::printf("no mods found in '%s'\n", dir.c_str());
        return 0;
    }
    std::printf("%-3s %-26s %-9s %-8s %s\n", "", "id", "kind", "version", "name");
    for (const auto& lm : mods) {
        const ModManifest& m = lm.manifest;
        std::printf("%-3s %-26s %-9s %-8s %s%s\n",
                    lm.enabled ? "[x]" : "[ ]",
                    m.id.c_str(),
                    modKindName(m.kind),
                    m.modVersion.c_str(),
                    m.name.c_str(),
                    m.builtin ? "  (built-in)" : "");
    }
    return 0;
}

int cmdInfo(const std::string& dir, const std::string& id) {
    ModRegistry reg;
    if (!loadRegistry(reg, dir)) return 1;
    const LoadedMod* lm = reg.find(id);
    if (!lm) {
        std::fprintf(stderr, "error: no mod with id '%s'\n", id.c_str());
        return 1;
    }
    std::printf("%s\n", serializeModManifest(lm->manifest).c_str());
    std::printf("enabled: %s\npath: %s\n",
                lm->enabled ? "true" : "false",
                lm->path.empty() ? "(none)" : lm->path.c_str());
    return 0;
}

int cmdValidate(const std::string& dir) {
    ModRegistry reg;
    if (!loadRegistry(reg, dir)) return 1;
    auto issues = reg.validate();
    if (issues.empty()) {
        std::printf("ok: %zu mod(s), no issues\n", reg.mods().size());
        return 0;
    }
    for (const auto& i : issues) std::printf("issue: %s\n", i.c_str());
    return 1;
}

int cmdOrder(const std::string& dir) {
    ModRegistry reg;
    if (!loadRegistry(reg, dir)) return 1;
    std::vector<const LoadedMod*> order;
    std::string err;
    if (!reg.resolveOrder(order, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    int i = 0;
    for (const LoadedMod* lm : order)
        std::printf("%d. %s\n", ++i, lm->manifest.id.c_str());
    if (order.empty()) std::printf("(no enabled mods)\n");
    return 0;
}

int cmdToggle(const std::string& dir, const std::string& id, bool on) {
    ModRegistry reg;
    if (!loadRegistry(reg, dir)) return 1;
    if (!reg.setEnabled(id, on)) {
        std::fprintf(stderr, "error: no mod with id '%s'\n", id.c_str());
        return 1;
    }
    if (!reg.saveState(stateFile(dir))) {
        std::fprintf(stderr, "error: cannot write %s\n", stateFile(dir).c_str());
        return 1;
    }
    std::printf("%s %s\n", on ? "enabled" : "disabled", id.c_str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string dir = "mods";
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--dir" && i + 1 < argc) { dir = argv[++i]; }
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else args.push_back(a);
    }
    if (args.empty()) { usage(); return 2; }

    const std::string& cmd = args[0];
    if (cmd == "list")     return cmdList(dir);
    if (cmd == "validate") return cmdValidate(dir);
    if (cmd == "order")    return cmdOrder(dir);
    if (cmd == "info") {
        if (args.size() < 2) { std::fprintf(stderr, "error: info needs <id>\n"); return 2; }
        return cmdInfo(dir, args[1]);
    }
    if (cmd == "enable" || cmd == "disable") {
        if (args.size() < 2) { std::fprintf(stderr, "error: %s needs <id>\n", cmd.c_str()); return 2; }
        return cmdToggle(dir, args[1], cmd == "enable");
    }
    std::fprintf(stderr, "error: unknown command '%s'\n", cmd.c_str());
    usage();
    return 2;
}
