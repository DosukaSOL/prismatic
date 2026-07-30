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
        for (int st = 0; st < shaderPresetCount(); ++st) {
            PresentationOptions o; o.enableShader = true; o.shader = shaderPreset(st);
            w(std::string("shader_") + shaderPresetName(st), renderEmulatorScreen(img, o));
        }
        PresentationOptions o; o.enable25D = true; w("25d", renderEmulatorScreen(img, o));
        o.enableShader = true; o.shader = shaderPreset(0); w("both", renderEmulatorScreen(img, o));

        // Genuine depth-based 2.5D preview: a small diorama scene with a real
        // depth map (0 near .. 1 far) so parallax / DoF / depth-darkening show.
        Image scene(256, 192);
        FloatBuffer depth(256, 192);
        auto put = [&](int x, int y, Color c, float d) {
            if (x < 0 || y < 0 || x >= 256 || y >= 192) return;
            scene.at(x, y) = c; depth.at(x, y) = d;
        };
        for (int y = 0; y < 192; ++y)
            for (int x = 0; x < 256; ++x) {
                float gd = 0.15f + 0.75f * (1.0f - y / 191.0f);   // ground recedes upward
                bool ck = ((x >> 4) ^ (y >> 4)) & 1;
                put(x, y, Color{(uint8_t)(ck ? 70 : 54), (uint8_t)(ck ? 130 : 108),
                                (uint8_t)(ck ? 72 : 58), 255}, gd);
            }
        for (int y = 30; y < 120; ++y)                            // building (far-mid)
            for (int x = 180; x < 230; ++x) put(x, y, Color{150, 150, 160, 255}, 0.70f);
        for (int y = 24; y < 112; ++y)                            // tree trunk+canopy (mid)
            for (int x = 40; x < 74; ++x)
                put(x, y, Color{(uint8_t)(y < 70 ? 40 : 90), (uint8_t)(y < 70 ? 120 : 70),
                                (uint8_t)(y < 70 ? 50 : 40), 255}, 0.50f);
        for (int y = 96; y < 150; ++y)                            // character (near)
            for (int x = 110; x < 146; ++x) put(x, y, Color{210, 70, 60, 255}, 0.22f);
        for (int i = 0; i < 192; ++i) {                           // diagonal edge (AA test)
            int x = 20 + i; if (x < 256) { scene.at(x, i) = Color{250, 250, 250, 255}; }
        }
        w("scene_native", scene);
        { PresentationOptions d; d.enable25D = true;
          w("depth25d", renderEmulatorScreen(scene, &depth, d)); }
        { PresentationOptions d; d.enable25D = true; d.enableShader = true;
          d.shader = shaderPreset(1);  // Octopath
          w("depth25d_octopath", renderEmulatorScreen(scene, &depth, d)); }
        { PresentationOptions aa; aa.antialias = true;
          w("aa", renderEmulatorScreen(scene, nullptr, aa)); }
        {   // Full HGSS/Platinum profile: genuine depth 2.5D + Diorama grade + FXAA.
            int di = 0; for (int i = 0; i < shaderPresetCount(); ++i)
                if (std::string(shaderPresetName(i)) == "Diorama") { di = i; break; }
            PresentationOptions d; d.enable25D = true; d.enableShader = true;
            d.antialias = true; d.tilt = 0.6f; d.shader = shaderPreset(di);
            w("depth25d_diorama", renderEmulatorScreen(scene, &depth, d)); }

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
    int presetIdx = 0;
    for (int i = 0; i < shaderPresetCount(); ++i)
        if (preset.find(shaderPresetName(i)) != std::string::npos) { presetIdx = i; break; }

    auto write = [&](const std::string& tag, const Image& img) {
        writePng((outDir / (std::string("nds_") + tag + ".png")).string(), img);
    };
    for (int s = 0; s < adapter->screenCount() && s < 2; ++s) {
        const Image fb = adapter->framebuffer(s);
        write(std::string(names[s]) + "_native", fb);

        PresentationOptions o;
        o.shader = shaderPreset(presetIdx);
        auto e0 = std::chrono::high_resolution_clock::now();
        o.enable25D = true;  o.enableShader = false; write(std::string(names[s]) + "_25d",    renderEmulatorScreen(fb, o));
        o.enable25D = false; o.enableShader = true;  write(std::string(names[s]) + "_shader", renderEmulatorScreen(fb, o));
        o.enable25D = true;  o.enableShader = true;  Image both = renderEmulatorScreen(fb, o);
        auto e1 = std::chrono::high_resolution_clock::now();
        write(std::string(names[s]) + "_both", both);
        std::printf("[%s] 2.5D+shader '%s', %dx%d, ~%.2f ms\n", names[s], shaderPresetName(presetIdx),
                    both.width, both.height,
                    std::chrono::duration<double, std::milli>(e1 - e0).count() / 3.0);
    }
    std::printf("Wrote native / 25d / shader / both PNGs to %s\n", outDir.string().c_str());
    return 0;
}
