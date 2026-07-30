// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — headless validation runner.
//
// Drives the synthetic backend deterministically, renders native + enhanced
// frames (and debug views) across presets / times-of-day / weather, writes PNGs
// and an HTML report. No GPU, no ROM. This is the primary on-machine proof that
// the capture -> reconstruct -> enhance -> render pipeline works end to end.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "prismatic/pipeline.hpp"
#include "prismatic/png.hpp"
#include "synthetic_backend.hpp"

namespace fs = std::filesystem;
using namespace prismatic;

namespace {

struct Shot {
    std::string id;
    std::string title;
    int frame;
    std::string preset;
    EnvironmentState env;
    int screen;         // 0 top, 1 bottom
    bool lantern;       // add a player lantern (night)
    bool touch;         // inject a touch on the bottom screen
    bool debug;         // also emit debug views
};

struct ReportRow {
    Shot shot;
    std::string nativePng, enhancedPng;
    std::string depthPng, normalPng, idPng, emissivePng, lightPng;
    double renderMs = 0;
};

void advanceTo(EmulatorAdapter& a, int frame, bool touch) {
    a.reset();
    for (int i = 0; i < frame; ++i) {
        InputState in;
        if (touch) { in.touchActive = true; in.touchX = 96 + (i % 64); in.touchY = 100; }
        a.setInput(in);
        a.advanceFrame();
    }
}

std::string save(const fs::path& dir, const std::string& name, const Image& img) {
    std::string fn = name + ".png";
    writePng((dir / fn).string(), img);
    return fn;
}

std::string esc(const std::string& s) { return s; }

void writeReport(const fs::path& dir, const std::vector<ReportRow>& rows,
                 const Capabilities& caps, const AdapterInfo& info, size_t cacheSize) {
    std::ofstream o((dir / "report.html").string());
    o << "<!doctype html><html><head><meta charset='utf-8'><title>PRISMATIC Validation Report</title>";
    o << "<style>body{font-family:system-ui,Segoe UI,Arial;background:#14161c;color:#e8eaf0;margin:24px}"
         "h1{color:#b39dff}h2{color:#9ec1ff;border-bottom:1px solid #333;padding-bottom:4px;margin-top:32px}"
         ".shot{background:#1c1f28;border:1px solid #2a2f3a;border-radius:8px;padding:12px;margin:12px 0}"
         ".imgs{display:flex;flex-wrap:wrap;gap:10px}.imgs figure{margin:0;text-align:center;font-size:12px;color:#9aa}"
         "img{image-rendering:pixelated;border:1px solid #333;background:#000;max-width:320px}"
         "code{color:#c8f0c8}table{border-collapse:collapse}td,th{border:1px solid #333;padding:3px 8px;font-size:13px}"
         "</style></head><body>";
    o << "<h1>PRISMATIC Validation Report</h1>";
    o << "<p>Backend: <code>" << esc(info.backendName) << "</code> · API v" << info.apiVersion
      << " · compatibility <code>" << compatibilityName(info.compatibility) << "</code>"
      << " · material cache entries: " << cacheSize << "</p>";

    o << "<h2>Capabilities</h2><table><tr>";
    struct C { const char* n; CapabilityBit b; };
    const C cb[] = {{"Framebuffer", Cap_Framebuffer}, {"Backgrounds", Cap_Backgrounds},
        {"Sprites", Cap_Sprites}, {"Palettes", Cap_Palettes}, {"Priorities", Cap_Priorities},
        {"Scroll", Cap_Scroll}, {"DualScreen", Cap_DualScreen}, {"Touch", Cap_Touch},
        {"Nds3D", Cap_Nds3D}, {"TileFlip", Cap_TileFlip}, {"PerTilePalette", Cap_PerTilePalette}};
    o << "<tr>"; for (auto& c : cb) o << "<th>" << c.n << "</th>"; o << "</tr><tr>";
    for (auto& c : cb) o << "<td style='text-align:center'>" << (caps.has(c.b) ? "✔" : "·") << "</td>";
    o << "</tr></table>";

    for (const auto& r : rows) {
        o << "<div class='shot'><h2>" << esc(r.shot.title) << "</h2>";
        o << "<p>frame <code>" << r.shot.frame << "</code> · preset <code>" << esc(r.shot.preset)
          << "</code> · time <code>" << r.shot.env.timeOfDay << "h</code> · weather <code>"
          << weatherName(r.shot.env.weather) << "</code> · tag <code>"
          << locationTagName(r.shot.env.tag) << "</code> · render <code>"
          << r.renderMs << " ms</code></p>";
        o << "<div class='imgs'>";
        o << "<figure><img src='" << r.nativePng << "'><figcaption>native</figcaption></figure>";
        o << "<figure><img src='" << r.enhancedPng << "'><figcaption>enhanced</figcaption></figure>";
        if (r.shot.debug) {
            o << "<figure><img src='" << r.depthPng << "'><figcaption>depth</figcaption></figure>";
            o << "<figure><img src='" << r.normalPng << "'><figcaption>normals</figcaption></figure>";
            o << "<figure><img src='" << r.idPng << "'><figcaption>object-id</figcaption></figure>";
            o << "<figure><img src='" << r.emissivePng << "'><figcaption>emissive</figcaption></figure>";
            o << "<figure><img src='" << r.lightPng << "'><figcaption>light-only</figcaption></figure>";
        }
        o << "</div></div>";
    }
    o << "<p style='color:#667'>All imagery derives from the first-party synthetic fixture. "
         "No copyrighted game assets are present.</p>";
    o << "</body></html>";
}

}  // namespace

int main(int argc, char** argv) {
    fs::path outDir = "local_data/validation_output";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--out" && i + 1 < argc) outDir = argv[++i];
    }
    std::error_code ec;
    fs::create_directories(outDir, ec);

    auto backend = makeSyntheticBackend();
    AdapterInfo info = backend->info();
    Capabilities caps = backend->capabilities();

    EnvironmentState day; day.timeOfDay = 12.0f;
    EnvironmentState morning; morning.timeOfDay = 8.0f;
    EnvironmentState dusk; dusk.timeOfDay = 18.0f;
    EnvironmentState night; night.timeOfDay = 22.0f;
    EnvironmentState rain; rain.timeOfDay = 12.0f; rain.weather = Weather::Rain;

    std::vector<Shot> shots = {
        {"hero", "Hero — HD-2.5D Balanced (top screen, noon)", 100, "HD-2.5D BALANCED", day, 0, false, false, true},
        {"orig", "Preset: ORIGINAL PLUS", 100, "ORIGINAL PLUS", day, 0, false, false, false},
        {"cine", "Preset: CINEMATIC HD-2D", 100, "CINEMATIC HD-2D", day, 0, false, false, false},
        {"pixel", "Preset: PIXEL PERFECT", 100, "PIXEL PERFECT", day, 0, false, false, false},
        {"tod_morning", "Time of day: morning (08h)", 100, "HD-2.5D BALANCED", morning, 0, false, false, false},
        {"tod_dusk", "Time of day: dusk (18h)", 100, "HD-2.5D BALANCED", dusk, 0, false, false, false},
        {"weather_rain", "Weather: rain (noon)", 100, "HD-2.5D BALANCED", rain, 0, false, false, false},
        {"night_glow", "NIGHT GLOW + player lantern (22h)", 260, "NIGHT GLOW", night, 0, true, false, true},
        {"bottom_ui", "Bottom screen UI + touch", 150, "FAITHFUL HD-2D", day, 1, false, true, false},
        {"motion", "Motion sample (frame 200, follower trailing)", 200, "HD-2.5D BALANCED", day, 0, false, false, false},
    };

    PrismaticPipeline pipe;
    std::vector<ReportRow> rows;

    for (const auto& shot : shots) {
        advanceTo(*backend, shot.frame, shot.touch);
        pipe.setPresetByName(shot.preset);
        pipe.setEnvironment(shot.env);
        pipe.clearLights();
        if (shot.lantern) {
            Light lantern;
            lantern.type = Light::Point;
            lantern.pos = {SyntheticDsBackend::kScreenW / 2.0f, SyntheticDsBackend::kScreenH / 2.0f};
            lantern.height = 0.5f;
            lantern.color = {1.0f, 0.85f, 0.55f};
            lantern.intensity = 2.2f;
            lantern.radius = 110.0f;
            lantern.priority = 10;
            pipe.addLight(lantern);
        }
        auto t0 = std::chrono::high_resolution_clock::now();
        RenderResult r = pipe.renderScreen(*backend, shot.screen);
        auto t1 = std::chrono::high_resolution_clock::now();

        ReportRow row;
        row.shot = shot;
        row.renderMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        row.nativePng = save(outDir, shot.id + "_native", r.nativeImage);
        row.enhancedPng = save(outDir, shot.id + "_enhanced", r.enhanced);
        if (shot.debug) {
            row.depthPng = save(outDir, shot.id + "_depth", r.depthView);
            row.normalPng = save(outDir, shot.id + "_normal", r.normalView);
            row.idPng = save(outDir, shot.id + "_id", r.objectIdView);
            row.emissivePng = save(outDir, shot.id + "_emissive", r.emissiveView);
            row.lightPng = save(outDir, shot.id + "_light", r.lightView);
        }
        rows.push_back(row);
        std::printf("[shot] %-14s frame=%-4d preset=%-16s %.2f ms\n",
                    shot.id.c_str(), shot.frame, shot.preset.c_str(), row.renderMs);
    }

    writeReport(outDir, rows, caps, info, pipe.cache().size());
    std::printf("Wrote %zu shots + report.html to %s\n", rows.size(), outDir.string().c_str());
    return 0;
}
