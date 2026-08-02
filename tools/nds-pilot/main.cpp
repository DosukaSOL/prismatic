// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — interactive NDS pilot (development tool).
//
// Drives a real ROM through the melonDS adapter with a tiny command script,
// so game states can be reached reproducibly, snapshotted (savestates), probed
// (RAM / camera / RTC / layer masks) and rendered. This is the workhorse for
// building game profiles: reach a scene once, save a state, then iterate on
// the shader engine against that exact frame forever.
//
// Script commands (one per line; '#' comments):
//   run N            advance N frames
//   press BTNS N     hold buttons for N frames (A B X Y L R START SELECT
//                    UP DOWN LEFT RIGHT), e.g. "press A 2"
//   touch X Y N      touch bottom screen at (X,Y) for N frames
//   save PATH        write savestate
//   load PATH        read savestate
//   shot PREFIX      write PREFIX_top.png / PREFIX_bottom.png (native)
//   scene PREFIX     shot + depth/layer-mask visualisations + camera dump
//   probe ADDR LEN   hex dump of emulated RAM
//   watch ADDR LEN   print hex dump every subsequent 'run'
//   info             print identity, RTC time, camera validity
//   quit
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "prismatic/png.hpp"
#include "prismatic/scene_stream.hpp"
#include "prismatic/game_profile.hpp"
#include "prismatic/game_lighting.hpp"
#include "nds_adapter.hpp"

using namespace prismatic;

namespace {

InputState parseButtons(const std::string& list) {
    InputState in;
    std::istringstream ss(list);
    std::string b;
    while (ss >> b) {
        for (auto& c : b) c = (char)std::toupper((unsigned char)c);
        if (b == "A") in.a = true;
        else if (b == "B") in.b = true;
        else if (b == "X") in.x = true;
        else if (b == "Y") in.y = true;
        else if (b == "L") in.l = true;
        else if (b == "R") in.r = true;
        else if (b == "START") in.start = true;
        else if (b == "SELECT") in.select = true;
        else if (b == "UP") in.up = true;
        else if (b == "DOWN") in.down = true;
        else if (b == "LEFT") in.left = true;
        else if (b == "RIGHT") in.right = true;
    }
    return in;
}

void hexDump(EmulatorAdapter& emu, uint32_t addr, uint32_t len) {
    std::vector<uint8_t> buf(len);
    if (!emu.peek(addr, buf.data(), len)) {
        std::printf("  peek failed @%08X\n", addr);
        return;
    }
    for (uint32_t i = 0; i < len; i += 16) {
        std::printf("  %08X:", addr + i);
        for (uint32_t j = i; j < i + 16 && j < len; ++j) std::printf(" %02X", buf[j]);
        std::printf("\n");
    }
}

// Grayscale visualisation of the depth buffer; magenta where no depth.
Image depthVis(const SceneStream& s) {
    Image img(s.width, s.height);
    for (int y = 0; y < s.height; ++y)
        for (int x = 0; x < s.width; ++x) {
            if (s.depth) {
                float d = s.depth->at(x, y);
                uint8_t g = (uint8_t)(255.0f * (1.0f - d));
                img.at(x, y) = Color{g, g, g, 255};
            } else {
                img.at(x, y) = Color{255, 0, 255, 255};
            }
        }
    return img;
}

// False-color layer provenance: 3D=blue, sprite=red, BG0-3=greens, backdrop=dark.
Image maskVis(const SceneStream& s) {
    Image img(s.width, s.height);
    for (int y = 0; y < s.height; ++y)
        for (int x = 0; x < s.width; ++x) {
            Color c{20, 20, 20, 255};
            if (s.layerMask) {
                uint8_t m = s.layerMask[(size_t)y * s.width + x];
                if (maskIs3D(m)) c = Color{60, 90, 255, 255};
                else if (maskIsSprite(m)) c = Color{255, 70, 70, 255};
                else if (m & LM_BG0) c = Color{40, 200, 40, 255};
                else if (m & LM_BG1) c = Color{40, 160, 90, 255};
                else if (m & LM_BG2) c = Color{140, 200, 60, 255};
                else if (m & LM_BG3) c = Color{200, 220, 90, 255};
                else if (m & LM_Backdrop) c = Color{50, 50, 60, 255};
            }
            img.at(x, y) = c;
        }
    return img;
}

void printMat(const char* name, const float* m) {
    std::printf("  %s:\n", name);
    for (int r = 0; r < 4; ++r)
        std::printf("    %8.3f %8.3f %8.3f %8.3f\n", m[r*4+0], m[r*4+1], m[r*4+2], m[r*4+3]);
}

}  // namespace

int main(int argc, char** argv) {
    std::string romPath, scriptPath, dataDir = "local_data/pilot_data";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--rom" && i + 1 < argc) romPath = argv[++i];
        else if (a == "--script" && i + 1 < argc) scriptPath = argv[++i];
        else if (a == "--data" && i + 1 < argc) dataDir = argv[++i];
    }
    if (romPath.empty()) {
        std::printf("usage: prismatic_nds_pilot --rom GAME.nds [--script cmds.txt] [--data DIR]\n"
                    "Reads commands from the script file then stdin. ROMs stay local.\n");
        return 0;
    }

    std::string err;
    auto emu = makeNdsAdapter(romPath, dataDir, &err);
    if (!emu) { std::fprintf(stderr, "load failed: %s\n", err.c_str()); return 1; }
    auto id = emu->identity();
    std::printf("pilot: '%s' (%s) sha %s...\n", id.title.c_str(), id.gameCode.c_str(),
                id.romSha256.substr(0, 12).c_str());

    struct Watch { uint32_t addr; uint32_t len; };
    std::vector<Watch> watches;

    // Command stream: script file first (if given), then stdin.
    std::ifstream scriptFile;
    if (!scriptPath.empty()) scriptFile.open(scriptPath);
    auto nextLine = [&](std::string& line) -> bool {
        if (scriptFile.is_open() && std::getline(scriptFile, line)) return true;
        return (bool)std::getline(std::cin, line);
    };

    std::string line;
    while (nextLine(line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string cmd; ss >> cmd;

        if (cmd == "quit") break;
        else if (cmd == "run") {
            int n = 1; ss >> n;
            InputState idle;
            emu->setInput(idle);
            for (int i = 0; i < n; ++i) emu->advanceFrame();
            for (auto& w : watches) { std::printf("watch @%08X:\n", w.addr); hexDump(*emu, w.addr, w.len); }
        }
        else if (cmd == "press") {
            std::string btns; int n = 2;
            // buttons are all tokens except a trailing integer
            std::vector<std::string> toks; std::string t;
            while (ss >> t) toks.push_back(t);
            if (!toks.empty()) { n = std::atoi(toks.back().c_str()); if (n <= 0) n = 2; else toks.pop_back(); }
            for (auto& b : toks) btns += b + " ";
            InputState in = parseButtons(btns);
            emu->setInput(in);
            for (int i = 0; i < n; ++i) emu->advanceFrame();
            InputState idle; emu->setInput(idle);
        }
        else if (cmd == "touch") {
            int x = 128, y = 96, n = 2; ss >> x >> y >> n;
            InputState in; in.touchActive = true; in.touchX = x; in.touchY = y;
            emu->setInput(in);
            for (int i = 0; i < n; ++i) emu->advanceFrame();
            InputState idle; emu->setInput(idle);
        }
        else if (cmd == "save" || cmd == "load") {
            std::string p; ss >> p;
            bool ok = (cmd == "save") ? emu->saveState(p) : emu->loadState(p);
            std::printf("%s %s: %s\n", cmd.c_str(), p.c_str(), ok ? "ok" : "FAILED");
        }
        else if (cmd == "shot" || cmd == "scene") {
            std::string p; ss >> p;
            writePng(p + "_top.png", emu->framebuffer(0));
            writePng(p + "_bottom.png", emu->framebuffer(1));
            if (cmd == "scene") {
                for (int s = 0; s < 2; ++s) {
                    SceneStream str = emu->sceneStream(s);
                    const char* nm = s == 0 ? "top" : "bottom";
                    writePng(p + "_" + nm + "_depth.png", depthVis(str));
                    writePng(p + "_" + nm + "_mask.png", maskVis(str));
                }
                SceneStream st = emu->sceneStream(0);
                std::printf("frame %llu  camera %s  rtc %04d-%02d-%02d %02d:%02d:%02d\n",
                            (unsigned long long)st.frameIndex,
                            st.camera.valid ? "VALID" : "invalid",
                            st.time.year, st.time.month, st.time.day,
                            st.time.hour, st.time.minute, st.time.second);
                if (st.camera.valid) {
                    printMat("proj", st.camera.proj);
                    printMat("viewStack0", st.camera.viewStack0);
                }
            }
            std::printf("wrote %s_*\n", p.c_str());
        }
        else if (cmd == "probe") {
            std::string a; uint32_t len = 64; ss >> a >> len;
            hexDump(*emu, (uint32_t)std::strtoul(a.c_str(), nullptr, 16), len);
        }
        else if (cmd == "poke") {   // poke ADDR HEXBYTES (e.g. poke 022A2044 00680100)
            std::string a, hex; ss >> a >> hex;
            std::vector<uint8_t> bytes;
            for (size_t i = 0; i + 1 < hex.size(); i += 2)
                bytes.push_back((uint8_t)std::strtoul(hex.substr(i, 2).c_str(), nullptr, 16));
            uint32_t addr = (uint32_t)std::strtoul(a.c_str(), nullptr, 16);
            bool ok = !bytes.empty() && emu->poke(addr, bytes.data(), (uint32_t)bytes.size());
            std::printf("poke %08X %zu bytes: %s\n", addr, bytes.size(), ok ? "ok" : "FAILED");
        }
        else if (cmd == "cam") {    // cam PITCH YAW FOVSCALE HEIGHT DOLLY  (all 0/1 = off)
            PresentationCamera pc;
            ss >> pc.pitchDeg >> pc.yawDeg >> pc.fovScale >> pc.heightOffset >> pc.dolly;
            pc.enabled = pc.pitchDeg != 0 || pc.yawDeg != 0 || pc.fovScale != 1.0f ||
                         pc.heightOffset != 0 || pc.dolly != 0;
            emu->setPresentationCamera(pc);
            std::printf("cam %s pitch=%.1f yaw=%.1f fov=%.2f h=%.0f dolly=%.0f\n",
                        pc.enabled ? "ON" : "off", pc.pitchDeg, pc.yawDeg, pc.fovScale,
                        pc.heightOffset, pc.dolly);
        }
        else if (cmd == "dump") {   // full 4MB main RAM to file (offline diffing)
            std::string p; ss >> p;
            std::vector<uint8_t> ram(0x400000);
            if (emu->peek(0x02000000u, ram.data(), (uint32_t)ram.size())) {
                std::ofstream o(p, std::ios::binary | std::ios::trunc);
                o.write(reinterpret_cast<const char*>(ram.data()), (std::streamsize)ram.size());
                std::printf("dumped 4MB RAM -> %s\n", p.c_str());
            } else std::printf("dump failed\n");
        }
        else if (cmd == "pick") {   // unproject screen pixel -> world position
            int x = 128, y = 96; ss >> x >> y;
            SceneStream st = emu->sceneStream(0);
            Vec3 w;
            if (unprojectPixel(st, x, y, w))
                std::printf("pick (%d,%d) depth=%.6f -> world %.2f %.2f %.2f\n", x, y,
                            st.depthAbs ? st.depthAbs->at(x, y) : -1.0f, w.x, w.y, w.z);
            else
                std::printf("pick (%d,%d): no depth/camera here\n", x, y);
        }
        else if (cmd == "lit") {    // render top screen through a game profile
            std::string profPath, outPrefix; ss >> profPath >> outPrefix;
            GameProfile prof; std::string perr;
            if (!loadGameProfileFile(profPath, prof, perr)) {
                std::printf("profile error: %s\n", perr.c_str());
                continue;
            }
            GameState gs;
            if (const StateProbe* mp = prof.findProbe("mapId")) {
                uint32_t v = 0;
                if (emu->peek(mp->addr, &v, (uint32_t)mp->bytes)) gs.mapId = (int)v;
            }
            SceneStream st = emu->sceneStream(0);
            if (st.time.valid) { gs.hour = st.time.hour; gs.timeName = prof.timeName(st.time.hour); }
            GameLightingParams glp;
            GameLitFrame lf = renderGameLit(emu->framebuffer(0), st, prof, gs, glp);
            writePng(outPrefix + "_native.png", emu->framebuffer(0));
            writePng(outPrefix + "_lit.png", lf.color);
            std::printf("lit: map=%d hour=%d time='%s' lights=%zu litPx=%d uiPx=%d -> %s_lit.png\n",
                        gs.mapId, gs.hour, gs.timeName.c_str(), lf.activeLights.size(),
                        lf.litPixels, lf.uiPixels, outPrefix.c_str());
        }
        else if (cmd == "watch") {
            std::string a; uint32_t len = 16; ss >> a >> len;
            watches.push_back({(uint32_t)std::strtoul(a.c_str(), nullptr, 16), len});
        }
        else if (cmd == "info") {
            SceneStream st = emu->sceneStream(0);
            const float* v = st.camera.viewStack0;
            std::printf("frame %llu camera=%d depthTop=%d depthBot=%d rtc %02d:%02d:%02d viewT %.1f %.1f %.1f\n",
                        (unsigned long long)st.frameIndex, st.camera.valid ? 1 : 0,
                        emu->depthBuffer(0) != nullptr, emu->depthBuffer(1) != nullptr,
                        st.time.hour, st.time.minute, st.time.second,
                        v[12], v[13], v[14]);
        }
        else std::printf("? unknown command: %s\n", cmd.c_str());
    }
    emu->flushSave();
    return 0;
}
