// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — Game Boy Color backend over SameBoy (implementation).
#include "sameboy_backend.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "sameboy_shim.h"

namespace prismatic {
namespace {

// CGB palette RAM: 8 palettes * 4 colors * 2 bytes, little-endian RGB555.
inline Color decodeCgbColor(const uint8_t* pal, int palIdx, int colorIdx) {
    const uint8_t* p = pal + ((size_t)palIdx * 4 + colorIdx) * 2;
    uint16_t c = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    int r5 = c & 0x1F, g5 = (c >> 5) & 0x1F, b5 = (c >> 10) & 0x1F;
    auto up = [](int v5) { return (uint8_t)((v5 * 255 + 15) / 31); };
    return Color{up(r5), up(g5), up(b5), 255};
}

// 2bpp planar 8x8 tile -> palette indices 0..3.
inline void decodeTile(const uint8_t* src, Tile& t) {
    for (int y = 0; y < 8; ++y) {
        uint8_t lo = src[y * 2], hi = src[y * 2 + 1];
        for (int x = 0; x < 8; ++x) {
            int bit = 7 - x;
            uint8_t v = (uint8_t)(((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));
            t.set(x, y, v);
        }
    }
}

class GbcBackend final : public EmulatorAdapter {
public:
    GbcBackend() { sb_ = sb_new(); }
    ~GbcBackend() override {
        if (!savePath_.empty() && sb_) sb_save_battery(sb_, savePath_.c_str());
        if (sb_) sb_free(sb_);
    }

    bool ok() const { return sb_ != nullptr; }

    void loadBootRomPath(const std::string& path) {
        if (sb_ && !path.empty()) sb_load_boot_rom(sb_, path.c_str());
    }
    void loadBootRomMem(const uint8_t* data, size_t size) {
        if (sb_ && data && size) sb_load_boot_rom_mem(sb_, data, size);
    }
    void loadRomPath(const std::string& path) {
        if (!sb_) return;
        romLoaded_ = (sb_load_rom(sb_, path.c_str()) == 0);
        savePath_ = path + ".sav";
        sb_load_battery(sb_, savePath_.c_str());
    }
    void loadRomMem(const uint8_t* data, size_t size) {
        if (!sb_ || !data || !size) return;
        sb_load_rom_mem(sb_, data, size);
        romLoaded_ = true;
    }

    // ---- identity / capabilities -----------------------------------------
    AdapterInfo info() const override {
        AdapterInfo i;
        i.backendName = "SameBoy";
        i.apiVersion = kAdapterApiVersion;
        i.system = System::GBC;
        i.compatibility = CompatibilityLevel::Level2_Structured;
        return i;
    }

    GameIdentity identity() const override {
        GameIdentity id;
        id.system = System::GBC;
        id.romLoaded = romLoaded_;
        if (sb_) {
            char title[17] = {0};
            sb_rom_title(sb_, title);
            id.title = title;
            char code[5] = {0};
            for (int i = 0; i < 4; ++i) code[i] = (char)sb_read(sb_, (uint16_t)(0x013F + i));
            id.gameCode = code;
        }
        return id;
    }

    Capabilities capabilities() const override {
        Capabilities c;
        c.add(Cap_Framebuffer);
        c.add(Cap_Backgrounds);
        c.add(Cap_Sprites);
        c.add(Cap_Palettes);
        c.add(Cap_Priorities);
        c.add(Cap_Scroll);
        c.add(Cap_TileFlip);
        c.add(Cap_PerTilePalette);
        return c;
    }

    ScreenRouting screenRouting() const override { return ScreenRouting{}; }
    int screenCount() const override { return 1; }

    // ---- drive ------------------------------------------------------------
    void reset() override {
        if (sb_) sb_reset(sb_);
        frameIndex_ = 0;
    }

    void setInput(const InputState& in) override { input_ = in; }

    void advanceFrame() override {
        if (!sb_) return;
        sb_set_key(sb_, SB_KEY_UP, input_.up);
        sb_set_key(sb_, SB_KEY_DOWN, input_.down);
        sb_set_key(sb_, SB_KEY_LEFT, input_.left);
        sb_set_key(sb_, SB_KEY_RIGHT, input_.right);
        sb_set_key(sb_, SB_KEY_A, input_.a);
        sb_set_key(sb_, SB_KEY_B, input_.b);
        sb_set_key(sb_, SB_KEY_START, input_.start);
        sb_set_key(sb_, SB_KEY_SELECT, input_.select);
        sb_run_frame(sb_);
        ++frameIndex_;
    }

    uint64_t frameIndex() const override { return frameIndex_; }

    // ---- capture ----------------------------------------------------------
    Image framebuffer(int) const override {
        Image img(160, 144);
        if (!sb_) return img;
        const uint32_t* px = sb_pixels(sb_);
        for (int i = 0; i < 160 * 144; ++i) {
            uint32_t v = px[i];
            img.pixels[i] = Color{(uint8_t)((v >> 16) & 0xFF), (uint8_t)((v >> 8) & 0xFF),
                                  (uint8_t)(v & 0xFF), 255};
        }
        return img;
    }

    StructuredFrame structuredFrame(int) const override {
        StructuredFrame f;
        f.screenWidth = 160;
        f.screenHeight = 144;
        f.frameIndex = frameIndex_;
        if (!sb_) return f;

        size_t vsize = 0;
        const uint8_t* vram = sb_vram(sb_, &vsize);
        const uint8_t* bgpal = sb_bg_palettes(sb_, nullptr);
        const uint8_t* objpal = sb_obj_palettes(sb_, nullptr);
        const uint8_t* oam = sb_oam(sb_, nullptr);
        if (!vram || !bgpal || !objpal || !oam) return f;

        uint8_t lcdc = sb_read(sb_, 0xFF40);
        uint8_t scy = sb_read(sb_, 0xFF42);
        uint8_t scx = sb_read(sb_, 0xFF43);
        bool cgb = sb_is_cgb(sb_) != 0;

        f.backdrop = decodeCgbColor(bgpal, 0, 0);

        // ---- BG layer ----
        BackgroundLayer bg;
        bg.index = 0;
        bg.enabled = (lcdc & 0x01) != 0;
        bg.widthTiles = 32;
        bg.heightTiles = 32;
        bg.scrollX = scx;
        bg.scrollY = scy;

        bg.palettes.resize(8);
        for (int p = 0; p < 8; ++p) {
            bg.palettes[p].resize(4);
            for (int c = 0; c < 4; ++c) bg.palettes[p][c] = decodeCgbColor(bgpal, p, c);
        }

        // Two banks of 384 tiles each; map indices resolve into this table.
        bg.tileset.resize(768);
        for (int b = 0; b < 2; ++b)
            for (int i = 0; i < 384; ++i)
                decodeTile(vram + (size_t)b * 0x2000 + (size_t)i * 16, bg.tileset[(size_t)b * 384 + i]);

        int mapBase = (lcdc & 0x08) ? 0x1C00 : 0x1800;
        bool unsignedTiles = (lcdc & 0x10) != 0;
        bg.map.resize(32 * 32);
        for (int n = 0; n < 32 * 32; ++n) {
            uint8_t idx = vram[mapBase + n];
            uint8_t attr = cgb ? vram[0x2000 + mapBase + n] : 0;
            int resolved = unsignedTiles ? (int)idx : (256 + (int)(int8_t)idx);
            if ((attr >> 3) & 1) resolved += 384;
            TileRef& t = bg.map[n];
            t.tileIndex = (uint16_t)resolved;
            t.paletteBank = attr & 0x07;
            t.flipH = (attr & 0x20) != 0;
            t.flipV = (attr & 0x40) != 0;
            t.priority = (uint8_t)((attr >> 7) & 1);
        }
        f.backgrounds.push_back(std::move(bg));

        // ---- sprites (OAM) ----
        bool tall = (lcdc & 0x04) != 0;
        bool objEnable = (lcdc & 0x02) != 0;
        if (objEnable) {
            int th = tall ? 2 : 1;
            for (int s = 0; s < 40; ++s) {
                const uint8_t* e = oam + (size_t)s * 4;
                if (e[0] == 0 || e[0] >= 160) continue;  // off-screen vertically
                Sprite sp;
                sp.id = s;
                sp.x = (int)e[1] - 8;
                sp.y = (int)e[0] - 16;
                sp.width = 8;
                sp.height = tall ? 16 : 8;
                uint8_t attr = e[3];
                sp.flipH = (attr & 0x20) != 0;
                sp.flipV = (attr & 0x40) != 0;
                sp.priority = (uint8_t)((attr >> 7) & 1);
                int objBank = cgb ? ((attr >> 3) & 1) : 0;
                int palIdx = cgb ? (attr & 0x07) : ((attr >> 4) & 1);
                uint8_t baseTile = tall ? (uint8_t)(e[2] & 0xFE) : e[2];
                sp.tiles.resize(th);
                for (int r = 0; r < th; ++r)
                    decodeTile(vram + (size_t)objBank * 0x2000 + (size_t)(baseTile + r) * 16,
                               sp.tiles[r]);
                sp.palette.resize(4);
                for (int c = 0; c < 4; ++c) sp.palette[c] = decodeCgbColor(objpal, palIdx, c);
                sp.palette[0].a = 0;  // color 0 is transparent for sprites
                f.sprites.push_back(std::move(sp));
            }
        }
        return f;
    }

    void flushSave() override {
        if (sb_ && !savePath_.empty()) sb_save_battery(sb_, savePath_.c_str());
    }

    bool saveState(const std::string& path) override {
        return sb_ && sb_save_state(sb_, path.c_str()) == 0;
    }
    bool loadState(const std::string& path) override {
        return sb_ && sb_load_state(sb_, path.c_str()) == 0;
    }

private:
    SBGb* sb_ = nullptr;
    InputState input_{};
    uint64_t frameIndex_ = 0;
    bool romLoaded_ = false;
    std::string savePath_;
};

}  // namespace

std::unique_ptr<EmulatorAdapter> makeGbcBackend(const std::string& romPath,
                                                const std::string& bootRomPath) {
    auto be = std::make_unique<GbcBackend>();
    if (!be->ok()) return nullptr;
    be->loadBootRomPath(bootRomPath);
    be->loadRomPath(romPath);
    return be;
}

std::unique_ptr<EmulatorAdapter> makeGbcBackendFromMemory(const uint8_t* romData, size_t romSize,
                                                          const uint8_t* bootRom,
                                                          size_t bootRomSize) {
    auto be = std::make_unique<GbcBackend>();
    if (!be->ok()) return nullptr;
    be->loadBootRomMem(bootRom, bootRomSize);
    be->loadRomMem(romData, romSize);
    return be;
}

}  // namespace prismatic
