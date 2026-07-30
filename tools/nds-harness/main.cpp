// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — Nintendo DS ROM smoke harness (host).
//
// Boots a REAL DS ROM through the melonDS-backed EmulatorAdapter, runs frames,
// then writes the native + PRISMATIC-enhanced top/bottom screens as PNGs. This
// is the on-machine proof of the full real chain:
//   ROM -> melonDS core -> framebuffer -> screen-space enhancement -> PNG.
//
// A ROM is required and is NEVER committed to the repo. Supply it via argv or
// the PRISMATIC_NDS_ROM environment variable. With no ROM the harness prints
// usage and exits 0, so automated builds stay green without copyrighted content.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "prismatic/pipeline.hpp"
#include "prismatic/png.hpp"
#include "nds_adapter.hpp"

namespace fs = std::filesystem;
using namespace prismatic;

int main(int argc, char** argv) {
    std::string romPath;
    std::string preset = "HD-2.5D BALANCED";
    int frames = 300;
    fs::path outDir = "local_data/nds_output";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--rom" && i + 1 < argc) romPath = argv[++i];
        else if (a == "--preset" && i + 1 < argc) preset = argv[++i];
        else if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (a == "--out" && i + 1 < argc) outDir = argv[++i];
        else if (romPath.empty() && !a.empty() && a[0] != '-') romPath = a;
    }
    if (romPath.empty()) {
        if (const char* env = std::getenv("PRISMATIC_NDS_ROM")) romPath = env;
    }
    if (romPath.empty()) {
        std::printf(
            "PRISMATIC NDS harness — no ROM supplied.\n"
            "Usage: prismatic_nds_harness --rom /path/to/game.nds "
            "[--preset \"HD-2.5D BALANCED\"] [--frames 300] [--out DIR]\n"
            "  or set PRISMATIC_NDS_ROM. ROMs are never bundled with PRISMATIC.\n");
        return 0;
    }

    fs::path dataDir = outDir / "melon_data";
    std::error_code ec;
    fs::create_directories(dataDir, ec);

    std::string err;
    auto adapter = makeNdsAdapter(romPath, dataDir.string(), &err);
    if (!adapter) {
        std::fprintf(stderr, "Failed to load ROM: %s\n", err.c_str());
        return 1;
    }

    const GameIdentity id = adapter->identity();
    const AdapterInfo info = adapter->info();
    std::printf("Loaded '%s' (code %s) via %s — sha256 %s...\n",
                id.title.c_str(), id.gameCode.c_str(), info.backendName.c_str(),
                id.romSha256.substr(0, 12).c_str());

    // Warm up: run frames so the game boots past its intro logos. Tap START/A in
    // the second half to advance through title screens.
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < frames; ++i) {
        InputState in;
        if (i > frames / 2 && (i % 32) < 4) { in.start = true; in.a = true; }
        adapter->setInput(in);
        adapter->advanceFrame();
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    const double emuMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("Ran %d frames in %.1f ms (%.2f ms/frame).\n",
                frames, emuMs, frames ? emuMs / frames : 0.0);

    PrismaticPipeline pipe;
    pipe.setPresetByName(preset);

    fs::create_directories(outDir, ec);
    const char* names[2] = {"top", "bottom"};
    for (int s = 0; s < adapter->screenCount() && s < 2; ++s) {
        const Image native = adapter->framebuffer(s);
        writePng((outDir / (std::string("nds_") + names[s] + "_native.png")).string(), native);

        auto e0 = std::chrono::high_resolution_clock::now();
        RenderResult r = pipe.renderScreen(*adapter, s);
        auto e1 = std::chrono::high_resolution_clock::now();

        const std::string outPng =
            (outDir / (std::string("nds_") + names[s] + "_enhanced.png")).string();
        writePng(outPng, r.enhanced);
        std::printf("[%s] enhanced %dx%d in %.2f ms -> %s\n", names[s],
                    r.enhanced.width, r.enhanced.height,
                    std::chrono::duration<double, std::milli>(e1 - e0).count(),
                    outPng.c_str());
    }
    std::printf("Wrote native + enhanced PNGs to %s\n", outDir.string().c_str());
    return 0;
}
