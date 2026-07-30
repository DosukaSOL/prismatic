// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — Nintendo DS backend adapter (melonDS core) implementation.
#include "nds_adapter.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <vector>

#include "prismatic/hash.hpp"
#include "melon_platform.hpp"

// melonDS core headers (third_party/melonDS/src on the include path).
#include "NDS.h"
#include "NDSCart.h"
#include "GPU.h"
#include "ARMJIT.h"
#include "Platform.h"

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

    bool init(const std::string& romPath, const std::string& dataDir, std::string* error);

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

    void readFramebuffers() {
        if (top_.pixels.empty()) top_ = Image(kDsW, kDsH);
        if (bottom_.pixels.empty()) bottom_ = Image(kDsW, kDsH);
        void* topp = nullptr;
        void* botp = nullptr;
        if (!nds_->GPU.GetFramebuffers(&topp, &botp)) return;  // GL renderer: no RAM buffers
        convert(reinterpret_cast<const uint32_t*>(topp), top_);
        convert(reinterpret_cast<const uint32_t*>(botp), bottom_);
    }

    static void convert(const uint32_t* src, Image& dst) {
        if (!src) return;
        for (int i = 0; i < kDsPixels; ++i) dst.pixels[(size_t)i] = unpackDsPixel(src[i]);
    }

    std::unique_ptr<melonDS::NDS> nds_;
    Image top_{kDsW, kDsH};
    Image bottom_{kDsW, kDsH};
    InputState input_;
    uint64_t frame_ = 0;
    GameIdentity id_;
    std::string savePath_;
    std::vector<uint8_t> saveBuf_;
};

bool NdsAdapter::init(const std::string& romPath, const std::string& dataDir, std::string* error) {
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
    // Ship the interpreter (JIT disabled) for the first release: it is immune to
    // Android W^X / executable-memory restrictions that the A64 JIT can hit, and
    // the Thor's Snapdragon 8 Gen 2 runs the DS interpreter at full speed. The
    // A64 JIT is still compiled in — delete this line to enable it.
    args.JIT = std::nullopt;
    nds_ = std::make_unique<melonDS::NDS>(std::move(args), nullptr);

    // Parse + insert the cart (the unique_ptr overload takes ownership).
    auto cart = melonDS::NDSCart::ParseROM(std::move(rom), (uint32_t)sz, nullptr, std::nullopt);
    if (!cart) return fail("not a valid NDS ROM (ParseROM failed): " + romPath);
    nds_->SetNDSCart(std::move(cart));

    // Load an existing battery save if present, then boot the game directly.
    savePath_ = romPath + ".sav";
    loadSaveIfPresent();

    nds_->Reset();
    if (nds_->NeedsDirectBoot()) nds_->SetupDirectBoot("prismatic.nds");
    nds_->Start();
    readFramebuffers();
    return true;
}

}  // namespace

std::unique_ptr<EmulatorAdapter> makeNdsAdapter(const std::string& romPath,
                                                const std::string& dataDir,
                                                std::string* error) {
    auto a = std::make_unique<NdsAdapter>();
    if (!a->init(romPath, dataDir, error)) return nullptr;
    return a;
}

}  // namespace prismatic
