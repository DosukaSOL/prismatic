// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — Nintendo DS backend adapter (melonDS core) implementation.
#include "nds_adapter.hpp"

#include <cstdint>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

#include "prismatic/hash.hpp"
#include "melon_platform.hpp"

// melonDS core headers (third_party/melonDS/src on the include path).
#include "NDS.h"
#include "NDSCart.h"
#include "GPU.h"
#include "GPU3D.h"
#include "SPU.h"
#include "RTC.h"
#include "ARMJIT.h"
#include "Platform.h"
#include "Savestate.h"

namespace prismatic {
namespace {

constexpr int kDsW = 256;
constexpr int kDsH = 192;
constexpr int kDsPixels = kDsW * kDsH;

// DS SetKeyMask bit layout (active-low: a set bit means *released*).
//   0=A 1=B 2=Select 3=Start 4=Right 5=Left 6=Up 7=Down 8=R 9=L 10=X 11=Y
uint32_t buildKeyMask(const InputState& in) {
    uint32_t m = 0xFFF;
    auto press = [&](int bit) { m &= ~(1u << bit); };
    if (in.a)      press(0);
    if (in.b)      press(1);
    if (in.select) press(2);
    if (in.start)  press(3);
    if (in.right)  press(4);
    if (in.left)   press(5);
    if (in.up)     press(6);
    if (in.down)   press(7);
    if (in.r)      press(8);
    if (in.l)      press(9);
    if (in.x)      press(10);
    if (in.y)      press(11);
    return m;
}

// melonDS software framebuffers are BGRA byte order (verified against the
// upstream Qt frontend, which uploads them as GL_BGRA/GL_UNSIGNED_BYTE — see
// melonDS Screen.cpp glTexSubImage3D). As a little-endian u32 that is
// 0xAARRGGBB. If a future core build ever swaps red/blue, flip ONLY this line.
inline Color unpackDsPixel(uint32_t p) {
    return Color{
        (uint8_t)((p >> 16) & 0xFF),  // R
        (uint8_t)((p >> 8) & 0xFF),   // G
        (uint8_t)(p & 0xFF),          // B
        255};
}

class NdsAdapter final : public EmulatorAdapter {
public:
    ~NdsAdapter() override {
        // Detach the global save sink so a dangling `this` is never called.
        melon::setNdsSaveSink(nullptr, nullptr);
    }

    bool init(const std::string& romPath, const std::string& dataDir, std::string* error,
              bool enableJit);

    AdapterInfo info() const override {
        AdapterInfo i;
        i.backendName = "melonDS";
        i.apiVersion = kAdapterApiVersion;
        i.system = System::NDS;
        // Real DS output is a composited framebuffer; PRISMATIC enhances it in
        // screen space (re-shading real pixels, never inventing art).
        i.compatibility = CompatibilityLevel::Level1_Framebuffer;
        return i;
    }

    GameIdentity identity() const override { return id_; }

    Capabilities capabilities() const override {
        Capabilities c;
        c.add(Cap_Framebuffer);
        c.add(Cap_DualScreen);
        c.add(Cap_Touch);
        return c;
    }

    ScreenRouting screenRouting() const override {
        ScreenRouting r;
        r.screenCount = 2;
        r.topOnPrimary = true;
        r.allowManualSwap = true;
        return r;
    }

    int screenCount() const override { return 2; }

    void reset() override {
        if (!nds_) return;
        nds_->Reset();
        if (nds_->NeedsDirectBoot()) nds_->SetupDirectBoot("prismatic.nds");
        nds_->Start();
        frame_ = 0;
        readFramebuffers();
    }

    void setInput(const InputState& in) override { input_ = in; }

    void advanceFrame() override {
        if (!nds_) return;
        nds_->SetKeyMask(buildKeyMask(input_));
        if (input_.touchActive) {
            nds_->TouchScreen((uint16_t)clampi(input_.touchX, 0, kDsW - 1),
                              (uint16_t)clampi(input_.touchY, 0, kDsH - 1));
        } else {
            nds_->ReleaseScreen();
        }
        nds_->RunFrame();
        readFramebuffers();
        ++frame_;
    }

    uint64_t frameIndex() const override { return frame_; }

    Image framebuffer(int screen) const override {
        return (screen == (int)ScreenId::Bottom) ? bottom_ : top_;
    }

    // Genuine per-pixel depth from the 3D engine, but only for the screen the 3D
    // is actually composited to this frame (engine A -> top iff ScreenSwap). Null
    // otherwise, so 2D-only screens never get warped by unrelated depth.
    const FloatBuffer* depthBuffer(int screen) const override {
        if (!hasDepth_ || screen != depthScreen_) return nullptr;
        return &depth_;
    }

    // Full-machine save states via melonDS's serializer.
    bool saveState(const std::string& path) override {
        if (!nds_) return false;
        melonDS::Savestate st;
        if (st.Error || !nds_->DoSavestate(&st) || st.Error) return false;
        std::ofstream o(path, std::ios::binary | std::ios::trunc);
        if (!o) return false;
        o.write(reinterpret_cast<const char*>(st.Buffer()), (std::streamsize)st.Length());
        return (bool)o;
    }

    bool loadState(const std::string& path) override {
        if (!nds_) return false;
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return false;
        std::streamsize sz = f.tellg();
        if (sz <= 0) return false;
        f.seekg(0);
        std::vector<uint8_t> buf((size_t)sz);
        if (!f.read(reinterpret_cast<char*>(buf.data()), sz)) return false;
        melonDS::Savestate st(buf.data(), (uint32_t)sz, false);
        if (st.Error || !nds_->DoSavestate(&st) || st.Error) return false;
        readFramebuffers();
        return true;
    }

    // Side-effect-free reads of emulated main RAM (0x02000000 mirror range).
    // Data-driven game profiles use this for map/state probes.
    bool peek(uint32_t addr, void* out, uint32_t len) const override {
        if (!nds_ || !out || len == 0) return false;
        if (addr < 0x02000000u) return false;
        uint32_t off = (addr - 0x02000000u) & nds_->MainRAMMask;
        if ((uint64_t)off + len > (uint64_t)nds_->MainRAMMask + 1) return false;
        std::memcpy(out, nds_->MainRAM + off, len);
        return true;
    }

    SceneStream sceneStream(int screen) const override {
        SceneStream s;
        s.width = kDsW;
        s.height = kDsH;
        s.frameIndex = frame_;
        s.depth = depthBuffer(screen);
        if (s.depth) s.depthAbs = &depthAbs_;
        // Layer provenance for the 2D engine composited to this screen.
        // GBATEK POWCNT1 bit15: 1 = engine A on the upper screen (ScreenSwap).
        if (nds_) {
            bool topIsA = (nds_->GPU.ScreenSwap != 0);
            int unit = (screen == (int)ScreenId::Top) == topIsA ? 0 : 1;
            if (layerMaskValid_[unit]) s.layerMask = layerMask_[unit].data();
        }
        s.camera = camera_;
        s.time = time_;
        return s;
    }

    // Pull decoded stereo audio from the SPU. Called on the audio thread; the
    // SPU serialises internally, so no adapter-wide lock is taken here.
    int readAudio(int16_t* out, int maxFrames) override {
        if (!nds_ || maxFrames <= 0) return 0;
        return nds_->SPU.ReadOutput(out, maxFrames);
    }

    // Level1 backends route through the framebuffer path; structuredFrame is
    // provided for completeness (a minimal, correctly-sized empty frame).
    StructuredFrame structuredFrame(int screen) const override {
        StructuredFrame f;
        f.screenWidth = kDsW;
        f.screenHeight = kDsH;
        f.frameIndex = frame_;
        const Image& src = (screen == (int)ScreenId::Bottom) ? bottom_ : top_;
        if (!src.pixels.empty()) f.backdrop = src.at(0, 0);
        return f;
    }

    // Called by the global save sink when melonDS flushes SRAM.
    void writeSave(const uint8_t* data, uint32_t len) {
        if (savePath_.empty() || !data || len == 0) return;
        std::ofstream o(savePath_, std::ios::binary | std::ios::trunc);
        if (o) o.write(reinterpret_cast<const char*>(data), (std::streamsize)len);
    }

    // Force the live cartridge SRAM to disk (used by "Save & Close"). melonDS
    // normally flushes through the sink after in-game writes; this guarantees
    // the newest bytes are persisted right now.
    void flushSave() override {
        if (!nds_) return;
        const uint8_t* data = nds_->GetNDSSave();
        uint32_t len = nds_->GetNDSSaveLength();
        writeSave(data, len);
    }

private:
    static void saveSinkThunk(const uint8_t* data, uint32_t len, void* ud) {
        if (ud) static_cast<NdsAdapter*>(ud)->writeSave(data, len);
    }

    void loadSaveIfPresent() {
        std::ifstream f(savePath_, std::ios::binary | std::ios::ate);
        if (!f) return;
        std::streamsize sz = f.tellg();
        if (sz <= 0) return;
        f.seekg(0);
        saveBuf_.resize((size_t)sz);
        if (f.read(reinterpret_cast<char*>(saveBuf_.data()), sz))
            nds_->SetNDSSave(saveBuf_.data(), (uint32_t)saveBuf_.size());
    }

    // <dataDir>/saves/<gamecode>_<sha8>.sav — stable across re-imports of the
    // same ROM. Falls back to <dataDir>/saves/rom.sav if identity is missing.
    std::string computeSavePath(const std::string& dataDir) const {
        namespace fs = std::filesystem;
        fs::path dir = fs::path(dataDir) / "saves";
        std::error_code ec;
        fs::create_directories(dir, ec);
        std::string code;
        for (char c : id_.gameCode)
            if (std::isalnum((unsigned char)c)) code += c;
        if (code.empty()) code = "rom";
        std::string sha8 = id_.romSha256.substr(0, 8);
        return (dir / (code + "_" + sha8 + ".sav")).string();
    }

    void readFramebuffers() {
        if (top_.pixels.empty()) top_ = Image(kDsW, kDsH);
        if (bottom_.pixels.empty()) bottom_ = Image(kDsW, kDsH);
        void* topp = nullptr;
        void* botp = nullptr;
        if (!nds_->GPU.GetFramebuffers(&topp, &botp)) return;  // GL renderer: no RAM buffers
        convert(reinterpret_cast<const uint32_t*>(topp), top_);
        convert(reinterpret_cast<const uint32_t*>(botp), bottom_);
        readDepth();
        readLayerMasks();
        readCamera();
        readTime();
    }

    // Copy the per-pixel layer-provenance bytes both 2D engines produced.
    void readLayerMasks() {
        auto& rend = nds_->GPU.GetRenderer();
        for (int unit = 0; unit < 2; ++unit) {
            layerMaskValid_[unit] = false;
            if (layerMask_[unit].size() != (size_t)kDsPixels)
                layerMask_[unit].resize((size_t)kDsPixels);
            bool any = false;
            for (int y = 0; y < kDsH; ++y) {
                uint8_t* line = rend.GetLayerMaskLine(unit, y);
                if (!line) return;
                std::memcpy(&layerMask_[unit][(size_t)y * kDsW], line, kDsW);
                any = true;
            }
            layerMaskValid_[unit] = any;
        }
    }

    // Capture the geometry engine's matrices as they stand at end of frame.
    // DS games reload the camera each frame, so the position-matrix stack
    // bottom is the game's view matrix in the overwhelmingly common case.
    void readCamera() {
        camera_.valid = false;
        auto& g3d = nds_->GPU.GPU3D;
        if (!g3d.RenderingEnabled) return;
        auto fx = [](int32_t v) { return (float)v / 4096.0f; };  // 20.12 fixed
        for (int i = 0; i < 16; ++i) {
            camera_.proj[i] = fx(g3d.ProjMatrix[i]);
            camera_.viewStack0[i] = fx(g3d.PosMatrixStack[0][i]);
            camera_.clip[i] = fx(g3d.ClipMatrix[i]);
        }
        camera_.valid = true;
    }

    void readTime() {
        nds_->RTC.GetDateTime(time_.year, time_.month, time_.day,
                              time_.hour, time_.minute, time_.second);
        time_.valid = true;
    }

    // Copy the 3D rasteriser's 24-bit depth into a normalised 0(near)..1(far)
    // buffer for the screen it belongs to. Detects "no real 3D this frame" by a
    // near-flat depth span and disables the effect so 2D frames pass through.
    void readDepth() {
        hasDepth_ = false;
        depthScreen_ = -1;
        auto& rend = nds_->GPU.GetRenderer();
        // Probe: gather raw min/max across the frame.
        uint32_t lo = 0xFFFFFFFFu, hi = 0u;
        bool any = false;
        for (int y = 0; y < kDsH; ++y) {
            uint32_t* dl = rend.GetDepth3DLine(y);
            if (!dl) return;  // renderer keeps no CPU depth (e.g. GL)
            for (int x = 0; x < kDsW; ++x) {
                uint32_t z = dl[x] & 0x00FFFFFFu;
                if (z < lo) lo = z;
                if (z > hi) hi = z;
            }
            any = true;
        }
        // A flat span means there is no meaningful 3D geometry this frame.
        constexpr uint32_t kMinSpan = 4096;
        if (!any || hi <= lo || (hi - lo) < kMinSpan) return;

        if (depth_.width != kDsW || depth_.height != kDsH) depth_ = FloatBuffer(kDsW, kDsH);
        if (depthAbs_.width != kDsW || depthAbs_.height != kDsH) depthAbs_ = FloatBuffer(kDsW, kDsH);
        const float inv = 1.0f / (float)(hi - lo);
        constexpr float kInv24 = 1.0f / 16777215.0f;
        for (int y = 0; y < kDsH; ++y) {
            uint32_t* dl = rend.GetDepth3DLine(y);
            float* out = &depth_.data[(size_t)y * kDsW];
            float* outAbs = &depthAbs_.data[(size_t)y * kDsW];
            for (int x = 0; x < kDsW; ++x) {
                uint32_t z = dl[x] & 0x00FFFFFFu;
                out[x] = (float)(z - lo) * inv;  // 0 near .. 1 far
                outAbs[x] = (float)z * kInv24;   // absolute rasteriser depth
            }
        }
        // Engine A carries the 3D; it maps to the top screen only when swapped.
        depthScreen_ = nds_->GPU.ScreenSwap ? (int)ScreenId::Top : (int)ScreenId::Bottom;
        hasDepth_ = true;
    }

    static void convert(const uint32_t* src, Image& dst) {
        if (!src) return;
        for (int i = 0; i < kDsPixels; ++i) dst.pixels[(size_t)i] = unpackDsPixel(src[i]);
    }

    std::unique_ptr<melonDS::NDS> nds_;
    Image top_{kDsW, kDsH};
    Image bottom_{kDsW, kDsH};
    FloatBuffer depth_;            // normalised 0(near)..1(far) for depthScreen_
    FloatBuffer depthAbs_;         // absolute 24-bit depth / 0xFFFFFF
    int depthScreen_ = -1;        // ScreenId of the screen the 3D is drawn to
    bool hasDepth_ = false;       // false => no usable 3D depth this frame
    std::vector<uint8_t> layerMask_[2];  // per 2D engine unit (A, B)
    bool layerMaskValid_[2] = {false, false};
    CameraCapture camera_;
    EmuDateTime time_;
    InputState input_;
    uint64_t frame_ = 0;
    GameIdentity id_;
    std::string savePath_;
    std::vector<uint8_t> saveBuf_;
};

bool NdsAdapter::init(const std::string& romPath, const std::string& dataDir, std::string* error,
                      bool enableJit) {
    auto fail = [&](const std::string& m) { if (error) *error = m; return false; };

    // Platform layer: where melonDS reads/writes (firmware, temp) and how saves
    // are delivered back to us.
    melon::setBaseDir(dataDir);
    melon::setNdsSaveSink(&NdsAdapter::saveSinkThunk, this);

    // Read the whole ROM into memory.
    std::ifstream romFile(romPath, std::ios::binary | std::ios::ate);
    if (!romFile) return fail("cannot open ROM: " + romPath);
    std::streamsize sz = romFile.tellg();
    if (sz <= 0) return fail("ROM is empty: " + romPath);
    romFile.seekg(0);
    auto rom = std::make_unique<uint8_t[]>((size_t)sz);
    if (!romFile.read(reinterpret_cast<char*>(rom.get()), sz))
        return fail("failed to read ROM: " + romPath);

    // Identity — from the ROM header, plus a hash of the user's ROM. We never
    // store or redistribute the ROM itself.
    id_.system = System::NDS;
    id_.romLoaded = true;
    id_.romSha256 = Sha256::hashBytes(rom.get(), (size_t)sz);
    if (sz >= 0x10) {
        char title[13] = {0};
        std::memcpy(title, rom.get(), 12);
        id_.title.assign(title);
        char code[5] = {0};
        std::memcpy(code, rom.get() + 0x0C, 4);
        id_.gameCode.assign(code);
    }

    // Build the DS with defaults: FreeBIOS + generated firmware (no external
    // BIOS needed), software renderer (null Renderer auto-creates SoftRenderer).
    melonDS::NDSArgs args;
    // JIT (A64 recompiler) is much faster on the Thor's Snapdragon 8 Gen 2 but
    // needs executable memory some Android W^X policies deny. Caller chooses;
    // std::nullopt selects the always-safe interpreter.
    if (!enableJit) args.JIT = std::nullopt;
    nds_ = std::make_unique<melonDS::NDS>(std::move(args), nullptr);

    // Parse + insert the cart (the unique_ptr overload takes ownership).
    auto cart = melonDS::NDSCart::ParseROM(std::move(rom), (uint32_t)sz, nullptr, std::nullopt);
    if (!cart) return fail("not a valid NDS ROM (ParseROM failed): " + romPath);
    nds_->SetNDSCart(std::move(cart));

    // Battery saves live in a stable, user-visible folder keyed by game code +
    // a short ROM hash, so re-opening the same game always finds its save and
    // continues where the player left off (auto-load below).
    savePath_ = computeSavePath(dataDir);
    loadSaveIfPresent();

    nds_->Reset();
    if (nds_->NeedsDirectBoot()) nds_->SetupDirectBoot("prismatic.nds");
    nds_->Start();
    // 48 kHz stereo out — a rate every Android device supports directly.
    nds_->SPU.SetOutputSampleRate(48000.0);
    readFramebuffers();
    return true;
}

}  // namespace

std::unique_ptr<EmulatorAdapter> makeNdsAdapter(const std::string& romPath,
                                                const std::string& dataDir,
                                                std::string* error,
                                                bool enableJit) {
    auto a = std::make_unique<NdsAdapter>();
    if (!a->init(romPath, dataDir, error, enableJit)) return nullptr;
    return a;
}

}  // namespace prismatic
