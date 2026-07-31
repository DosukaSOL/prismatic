// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — Game Boy Color ROM smoke harness (host).
//
// Boots a REAL GB/GBC ROM through the SameBoy-backed EmulatorAdapter, runs
// frames, then writes two PNGs:
//   gbc_native.png      — SameBoy's ground-truth 160x144 framebuffer.
//   gbc_structured.png  — a flat reconstruction rebuilt ONLY from the structured
//                         frame (BG tilemap + tiles + CGB palettes + OAM) that
//                         PRISMATIC extracts from VRAM. If this matches native,
//                         the structured capture that feeds the voxel 2.5D is
//                         correct.
//
// A ROM is required and is NEVER committed. Supply it via argv/--rom or the
// PRISMATIC_GBC_ROM env var. With no ROM the harness prints usage and exits 0,
// so automated builds stay green without copyrighted content.
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "prismatic/adapter.hpp"
#include "prismatic/png.hpp"
#include "prismatic/voxel_diorama.hpp"
#include "sameboy_backend.hpp"

namespace fs = std::filesystem;
using namespace prismatic;

// Rebuild a flat 160x144 image from the structured frame alone. This is the
// validation oracle for the VRAM extraction (and the same data the voxel
// renderer consumes).
static Image flattenStructured(const StructuredFrame& f) {
    Image img(f.screenWidth, f.screenHeight, f.backdrop);
    if (!f.backgrounds.empty()) {
        const BackgroundLayer& bg = f.backgrounds[0];
        const int mapPx = bg.widthTiles * 8;   // 256
        const int mapPy = bg.heightTiles * 8;  // 256
        if (bg.enabled && !bg.tileset.empty() && !bg.map.empty()) {
            for (int y = 0; y < f.screenHeight; ++y) {
                for (int x = 0; x < f.screenWidth; ++x) {
                    int wx = ((x + bg.scrollX) % mapPx + mapPx) % mapPx;
                    int wy = ((y + bg.scrollY) % mapPy + mapPy) % mapPy;
                    const TileRef& c = bg.cell(wx >> 3, wy >> 3);
                    if (c.tileIndex >= bg.tileset.size()) continue;
                    int px = wx & 7, py = wy & 7;
                    if (c.flipH) px = 7 - px;
                    if (c.flipV) py = 7 - py;
                    uint8_t idx = bg.tileset[c.tileIndex].at(px, py);
                    if (c.paletteBank < bg.palettes.size() &&
                        idx < bg.palettes[c.paletteBank].size())
                        img.at(x, y) = bg.palettes[c.paletteBank][idx];
                }
            }
        }
    }
    // Sprites over BG (simple: all on top, color 0 transparent).
    for (const Sprite& sp : f.sprites) {
        for (int oy = 0; oy < sp.height; ++oy) {
            for (int ox = 0; ox < sp.width; ++ox) {
                int srcx = sp.flipH ? (sp.width - 1 - ox) : ox;
                int srcy = sp.flipV ? (sp.height - 1 - oy) : oy;
                int tileRow = srcy / 8;
                if ((size_t)tileRow >= sp.tiles.size()) continue;
                uint8_t idx = sp.tiles[tileRow].at(srcx & 7, srcy & 7);
                if (idx == 0) continue;
                int dx = sp.x + ox, dy = sp.y + oy;
                if (img.inBounds(dx, dy) && idx < sp.palette.size())
                    img.at(dx, dy) = sp.palette[idx];
            }
        }
    }
    return img;
}

static std::string firstExisting(std::initializer_list<std::string> paths) {
    for (const auto& p : paths)
        if (!p.empty() && fs::exists(p)) return p;
    return {};
}

int main(int argc, char** argv) {
    std::string romPath;
    std::string bootPath;
    int frames = 600;
    int film = 0;       // dump every N frames when > 0
    bool autoInput = false;  // mash through the intro toward the overworld
    bool voxel = false;      // also render the tile-extruded voxel diorama
    std::string loadStatePath;
    std::string saveStatePath;
    std::string walkSeq;     // e.g. "L30,D24,A6" — scripted movement
    fs::path outDir = "local_data/gbc_output";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--rom" && i + 1 < argc) romPath = argv[++i];
        else if (a == "--boot" && i + 1 < argc) bootPath = argv[++i];
        else if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (a == "--film" && i + 1 < argc) film = std::atoi(argv[++i]);
        else if (a == "--auto") autoInput = true;
        else if (a == "--voxel") voxel = true;
        else if (a == "--loadstate" && i + 1 < argc) loadStatePath = argv[++i];
        else if (a == "--savestate" && i + 1 < argc) saveStatePath = argv[++i];
        else if (a == "--walk" && i + 1 < argc) walkSeq = argv[++i];
        else if (a == "--out" && i + 1 < argc) outDir = argv[++i];
        else if (romPath.empty() && !a.empty() && a[0] != '-') romPath = a;
    }
    if (romPath.empty())
        if (const char* env = std::getenv("PRISMATIC_GBC_ROM")) romPath = env;

    if (romPath.empty()) {
        std::printf(
            "PRISMATIC GBC harness — no ROM supplied.\n"
            "Usage: prismatic_gbc_harness --rom /path/to/game.gbc "
            "[--boot cgb_boot.bin] [--frames 600] [--out DIR]\n"
            "(ROMs are never committed; this exits 0 so CI stays green.)\n");
        return 0;
    }

    if (bootPath.empty())
        if (const char* env = std::getenv("PRISMATIC_GBC_BOOT")) bootPath = env;
    if (bootPath.empty()) {
        bootPath = firstExisting({
            "third_party/SameBoy/build/bin/BootROMs/cgb_boot_fast.bin",
            "third_party/SameBoy/build/bin/BootROMs/cgb_boot.bin",
        });
    }

    std::printf("PRISMATIC GBC harness\n  rom : %s\n  boot: %s\n  frames: %d\n",
                romPath.c_str(), bootPath.empty() ? "(none)" : bootPath.c_str(), frames);

    auto be = makeGbcBackend(romPath, bootPath);
    if (!be) {
        std::fprintf(stderr, "ERROR: failed to create GBC backend / load ROM.\n");
        return 1;
    }

    GameIdentity id = be->identity();
    std::printf("  title: '%s'  code: '%s'  loaded: %s\n", id.title.c_str(),
                id.gameCode.c_str(), id.romLoaded ? "yes" : "no");

    std::error_code ec;
    fs::create_directories(outDir, ec);

    if (!loadStatePath.empty()) {
        bool ok = be->loadState(loadStatePath);
        std::printf("  loadstate %s: %s\n", loadStatePath.c_str(), ok ? "ok" : "FAIL");
    }

    std::vector<Image> filmShots;
    int gf = 0;
    auto capture = [&]() {
        if (film > 0 && (gf % film) == 0) {
            char name[64];
            std::snprintf(name, sizeof(name), "gbc_film_%05d.png", gf);
            writePng((outDir / name).string(), be->framebuffer(0));
            filmShots.push_back(be->framebuffer(0));
        }
    };
    auto press = [&](const InputState& in, int n) {
        for (int k = 0; k < n; ++k) { be->setInput(in); be->advanceFrame(); capture(); ++gf; }
    };

    for (int i = 0; i < frames; ++i) {
        InputState in{};
        if (autoInput) {
            // Blind masher: title -> NEW GAME -> Elm intro -> clock/name ->
            // overworld. Keep advancing dialogue with A the whole way; DOWN
            // nudges menu cursors (incl. the name-entry END); START only during
            // the early title phase (else it opens the overworld menu). Stop all
            // input for the last stretch so the player stands still, clean.
            int settle = frames - 200;
            if (i < settle) {
                if (i % 40 < 6) in.a = true;
                if (i % 220 < 4) in.down = true;
                if (i < 800 && i % 150 < 6) in.start = true;
            }
        }
        press(in, 1);
    }

    // Scripted movement (great after --loadstate), e.g. "L30,D24,A6": each token
    // is a button (U/D/L/R/A/B/S=start/E=select/W=wait) held for N frames.
    for (size_t p = 0; p < walkSeq.size();) {
        char btn = walkSeq[p++];
        int n = 0; bool hasN = false;
        while (p < walkSeq.size() && std::isdigit((unsigned char)walkSeq[p])) {
            n = n * 10 + (walkSeq[p] - '0'); ++p; hasN = true;
        }
        if (p < walkSeq.size() && walkSeq[p] == ',') ++p;
        if (!hasN) n = 16;
        InputState in{};
        switch (btn) {
            case 'U': case 'u': in.up = true; break;
            case 'D': case 'd': in.down = true; break;
            case 'L': case 'l': in.left = true; break;
            case 'R': case 'r': in.right = true; break;
            case 'A': case 'a': in.a = true; break;
            case 'B': case 'b': in.b = true; break;
            case 'S': case 's': in.start = true; break;
            case 'E': case 'e': in.select = true; break;
            default: break;  // 'W'/wait
        }
        press(in, n);
        press(InputState{}, 6);  // release between moves
    }

    if (!saveStatePath.empty()) {
        bool ok = be->saveState(saveStatePath);
        std::printf("  savestate %s: %s\n", saveStatePath.c_str(), ok ? "ok" : "FAIL");
    }

    // Contact sheet: tile every captured film frame into one image (5 columns).
    if (!filmShots.empty()) {
        const int cols = 5;
        const int rows = (int)(filmShots.size() + cols - 1) / cols;
        const int cw = 160, chh = 144, pad = 2;
        Image sheet(cols * cw + (cols + 1) * pad, rows * chh + (rows + 1) * pad,
                    Color{20, 20, 28, 255});
        for (size_t k = 0; k < filmShots.size(); ++k) {
            int cx = (int)(k % cols), cy = (int)(k / cols);
            int ox = pad + cx * (cw + pad), oy = pad + cy * (chh + pad);
            const Image& s = filmShots[k];
            for (int y = 0; y < chh && y < s.height; ++y)
                for (int x = 0; x < cw && x < s.width; ++x) sheet.set(ox + x, oy + y, s.at(x, y));
        }
        writePng((outDir / "gbc_contact.png").string(), sheet);
    }

    Image native = be->framebuffer(0);
    StructuredFrame sf = be->structuredFrame(0);
    Image structured = flattenStructured(sf);

    std::string nPath = (outDir / "gbc_native.png").string();
    std::string sPath = (outDir / "gbc_structured.png").string();
    bool okN = writePng(nPath, native);
    bool okS = writePng(sPath, structured);

    if (voxel) {
        Image diorama = renderVoxelDiorama(sf, VoxelOptions{});
        std::string vPath = (outDir / "gbc_voxel.png").string();
        bool okV = writePng(vPath, diorama);
        std::printf("  wrote %s (%s)\n", vPath.c_str(), okV ? "ok" : "FAIL");
    }

    std::printf("  frame %llu: %d bg layer(s), %zu sprite(s)\n",
                (unsigned long long)sf.frameIndex, (int)sf.backgrounds.size(),
                sf.sprites.size());
    std::printf("  wrote %s (%s)\n  wrote %s (%s)\n", nPath.c_str(), okN ? "ok" : "FAIL",
                sPath.c_str(), okS ? "ok" : "FAIL");
    return (okN && okS) ? 0 : 1;
}
