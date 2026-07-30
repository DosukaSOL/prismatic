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
#include "prismatic/emulator_present.hpp"
#include "prismatic/png.hpp"
#include "nds_adapter.hpp"

namespace fs = std::filesystem;
using namespace prismatic;

int main(int argc, char** argv) {
    std::string romPath;
    std::string preset = "HD-2.5D BALANCED";
    int frames = 300;
    fs::path outDir = "local_data/nds_output";
    bool pattern = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--rom" && i + 1 < argc) romPath = argv[++i];
        else if (a == "--preset" && i + 1 < argc) preset = argv[++i];
        else if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (a == "--out" && i + 1 < argc) outDir = argv[++i];
        else if (a == "--pattern") pattern = true;
        else if (romPath.empty() && !a.empty() && a[0] != '-') romPath = a;
    }

    // Shader/2.5D preview on a synthetic colour frame (no ROM needed).
    if (pattern) {
        std::error_code pec;
        fs::create_directories(outDir, pec);
        Image img(256, 192);
        for (int y = 0; y < 192; ++y)
            for (int x = 0; x < 256; ++x) {
                bool ck = ((x >> 4) ^ (y >> 4)) & 1;
                uint8_t r = (uint8_t)(x);                    // horizontal red ramp
                uint8_t g = (uint8_t)(y * 4 / 3);            // vertical green ramp
                uint8_t b = (uint8_t)(ck ? 220 : 60);        // checker blue
                if (y > 150) { r = 250; g = 240; b = 90; }   // bright band (bloom test)
                img.at(x, y) = Color{r, g, b, 255};
            }
        auto w = [&](const std::string& t, const Image& im) {
            writePng((outDir / (std::string("pat_") + t + ".png")).string(), im);
        };
        w("native", img);
        for (int st = 0; st < 5; ++st) {
            PresentationOptions o; o.enableShader = true; o.shaderStyle = st; o.timeOfDay = 13.0f;
            w("shader" + std::to_string(st), renderEmulatorScreen(img, o));
        }
        PresentationOptions o; o.enable25D = true; w("25d", renderEmulatorScreen(img, o));
        o.enableShader = true; o.shaderStyle = 2; w("both", renderEmulatorScreen(img, o));
        std::printf("Wrote pattern previews to %s\n", outDir.string().c_str());
        return 0;
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

    // Independent presentation layers, exactly as on-device: faithful base,
    // geometric 2.5D, shader overlay, and both together. Written per screen so
    // the four combinations can be eyeballed.
    fs::create_directories(outDir, ec);
    const char* names[2] = {"top", "bottom"};
    int shaderStyle = 0;
    if (preset.find("LCD") != std::string::npos) shaderStyle = 1;
    else if (preset.find("Warm") != std::string::npos) shaderStyle = 2;
    else if (preset.find("Night") != std::string::npos) shaderStyle = 3;
    else if (preset.find("Vivid") != std::string::npos) shaderStyle = 4;

    auto write = [&](const std::string& tag, const Image& img) {
        writePng((outDir / (std::string("nds_") + tag + ".png")).string(), img);
    };
    for (int s = 0; s < adapter->screenCount() && s < 2; ++s) {
        const Image fb = adapter->framebuffer(s);
        write(std::string(names[s]) + "_native", fb);

        PresentationOptions o;
        o.timeOfDay = 20.0f; o.shaderStyle = shaderStyle;
        auto e0 = std::chrono::high_resolution_clock::now();
        o.enable25D = true;  o.enableShader = false; write(std::string(names[s]) + "_25d",    renderEmulatorScreen(fb, o));
        o.enable25D = false; o.enableShader = true;  write(std::string(names[s]) + "_shader", renderEmulatorScreen(fb, o));
        o.enable25D = true;  o.enableShader = true;  Image both = renderEmulatorScreen(fb, o);
        auto e1 = std::chrono::high_resolution_clock::now();
        write(std::string(names[s]) + "_both", both);
        std::printf("[%s] 2.5D+shader style %d, %dx%d, ~%.2f ms\n", names[s], shaderStyle,
                    both.width, both.height,
                    std::chrono::duration<double, std::milli>(e1 - e0).count() / 3.0);
    }
    std::printf("Wrote native / 25d / shader / both PNGs to %s\n", outDir.string().c_str());
    return 0;
}
