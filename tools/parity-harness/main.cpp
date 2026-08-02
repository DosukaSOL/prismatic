// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — differential parity harness.
//
// Runs the same deterministic input script against two configurations of a
// game (ROM A vs ROM B, or JIT vs interpreter) and compares state at fixed
// checkpoints: framebuffer hashes for both screens plus a list of RAM probes
// (map ID etc). Reports the first divergent checkpoint with per-item detail.
//
// This is the validation oracle for (a) proving mod builds do not alter game
// state outside their intent, (b) JIT correctness, and (c) — as the native
// HGSS runtime grows — native-vs-emulator parity.
//
//   prismatic_parity --rom-a A.nds --rom-b B.nds --script s.txt
//                    [--interval 60] [--probe ADDR:LEN ...] [--jit-a 0|1] [--jit-b 0|1]
//
// Script commands: run N | press BTNS N | touch X Y N   (pilot-compatible).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "prismatic/hash.hpp"
#include "nds_adapter.hpp"

using namespace prismatic;

namespace {

struct Probe { uint32_t addr; uint32_t len; };

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

// One scripted step: hold `input` for `frames`.
struct Step { InputState input; int frames; };

std::vector<Step> loadScript(const std::string& path, std::string& err) {
    std::vector<Step> steps;
    std::ifstream f(path);
    if (!f) { err = "cannot open script " + path; return steps; }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string cmd; ss >> cmd;
        if (cmd == "run") {
            int n = 1; ss >> n;
            steps.push_back({InputState{}, n});
        } else if (cmd == "press") {
            std::vector<std::string> toks; std::string t;
            while (ss >> t) toks.push_back(t);
            int n = 2;
            if (!toks.empty()) { n = std::atoi(toks.back().c_str()); if (n > 0) toks.pop_back(); else n = 2; }
            std::string btns;
            for (auto& b : toks) btns += b + " ";
            steps.push_back({parseButtons(btns), n});
        } else if (cmd == "touch") {
            int x = 128, y = 96, n = 2; ss >> x >> y >> n;
            InputState in; in.touchActive = true; in.touchX = x; in.touchY = y;
            steps.push_back({in, n});
        }
    }
    return steps;
}

std::string frameHash(const Image& img) {
    if (img.pixels.empty()) return "empty";
    return Sha256::hashBytes(reinterpret_cast<const uint8_t*>(img.pixels.data()),
                             img.pixels.size() * sizeof(Color)).substr(0, 16);
}

struct Checkpoint {
    uint64_t frame = 0;
    std::string topHash, bottomHash;
    std::vector<std::vector<uint8_t>> probeData;
};

Checkpoint capture(EmulatorAdapter& emu, const std::vector<Probe>& probes) {
    Checkpoint c;
    c.frame = emu.frameIndex();
    c.topHash = frameHash(emu.framebuffer(0));
    c.bottomHash = frameHash(emu.framebuffer(1));
    for (const auto& p : probes) {
        std::vector<uint8_t> buf(p.len, 0);
        emu.peek(p.addr, buf.data(), p.len);
        c.probeData.push_back(std::move(buf));
    }
    return c;
}

}  // namespace

int main(int argc, char** argv) {
    std::string romA, romB, scriptPath, dataDir = "local_data/parity";
    int interval = 60;
    bool jitA = true, jitB = true;
    std::vector<Probe> probes;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--rom-a" && i + 1 < argc) romA = argv[++i];
        else if (a == "--rom-b" && i + 1 < argc) romB = argv[++i];
        else if (a == "--script" && i + 1 < argc) scriptPath = argv[++i];
        else if (a == "--interval" && i + 1 < argc) interval = std::atoi(argv[++i]);
        else if (a == "--data" && i + 1 < argc) dataDir = argv[++i];
        else if (a == "--jit-a" && i + 1 < argc) jitA = std::atoi(argv[++i]) != 0;
        else if (a == "--jit-b" && i + 1 < argc) jitB = std::atoi(argv[++i]) != 0;
        else if (a == "--probe" && i + 1 < argc) {
            std::string spec = argv[++i];
            size_t colon = spec.find(':');
            Probe p;
            p.addr = (uint32_t)std::strtoul(spec.substr(0, colon).c_str(), nullptr, 16);
            p.len = colon == std::string::npos ? 4 : (uint32_t)std::atoi(spec.substr(colon + 1).c_str());
            probes.push_back(p);
        }
    }
    if (romA.empty() || romB.empty() || scriptPath.empty()) {
        std::printf("usage: prismatic_parity --rom-a A.nds --rom-b B.nds --script s.txt\n"
                    "       [--interval 60] [--probe ADDR:LEN ...] [--jit-a 0|1] [--jit-b 0|1]\n");
        return 2;
    }

    std::string err;
    auto steps = loadScript(scriptPath, err);
    if (steps.empty()) { std::fprintf(stderr, "error: %s\n", err.empty() ? "empty script" : err.c_str()); return 1; }

    // Separate data dirs so battery saves cannot cross-contaminate the runs.
    auto emuA = makeNdsAdapter(romA, dataDir + "/a", &err, jitA);
    if (!emuA) { std::fprintf(stderr, "error: rom-a: %s\n", err.c_str()); return 1; }
    auto emuB = makeNdsAdapter(romB, dataDir + "/b", &err, jitB);
    if (!emuB) { std::fprintf(stderr, "error: rom-b: %s\n", err.c_str()); return 1; }

    std::printf("parity: A=%s (jit=%d)  B=%s (jit=%d)  interval=%d probes=%zu\n",
                romA.c_str(), (int)jitA, romB.c_str(), (int)jitB, interval, probes.size());

    int checkpoints = 0, mismatches = 0;
    int sinceCheck = 0;
    uint64_t firstDiv = 0;
    for (const auto& step : steps) {
        emuA->setInput(step.input);
        emuB->setInput(step.input);
        for (int i = 0; i < step.frames; ++i) {
            emuA->advanceFrame();
            emuB->advanceFrame();
            if (++sinceCheck >= interval) {
                sinceCheck = 0;
                Checkpoint ca = capture(*emuA, probes);
                Checkpoint cb = capture(*emuB, probes);
                ++checkpoints;
                bool diff = false;
                std::string detail;
                if (ca.topHash != cb.topHash) { diff = true; detail += " top-frame"; }
                if (ca.bottomHash != cb.bottomHash) { diff = true; detail += " bottom-frame"; }
                for (size_t p = 0; p < probes.size(); ++p)
                    if (ca.probeData[p] != cb.probeData[p]) {
                        diff = true;
                        char buf[32];
                        std::snprintf(buf, sizeof(buf), " probe@%08X", probes[p].addr);
                        detail += buf;
                    }
                if (diff) {
                    ++mismatches;
                    if (firstDiv == 0) firstDiv = ca.frame;
                    std::printf("DIVERGENCE @frame %llu:%s\n",
                                (unsigned long long)ca.frame, detail.c_str());
                }
            }
        }
    }
    std::printf("parity result: %d checkpoints, %d divergent%s\n", checkpoints, mismatches,
                mismatches ? "" : " — MATCH");
    if (firstDiv) std::printf("first divergence at frame %llu\n", (unsigned long long)firstDiv);
    return mismatches == 0 ? 0 : 1;
}
