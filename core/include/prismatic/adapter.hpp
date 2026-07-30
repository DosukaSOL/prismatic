// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — Emulator Adapter API.
//
// This is the single seam between PRISMATIC and any emulator backend. A backend
// exposes each emulated frame both as a ground-truth framebuffer AND as a
// *structured* description (backgrounds, tilemaps, palettes, sprites/OAM,
// priorities, scroll, windows, blending, optional NDS 3D layer). The renderer
// consumes the structured frame to reconstruct an enhanced scene while the
// framebuffer remains the fidelity reference.
//
// The API is explicitly versioned so backends and the host can detect mismatch.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include "prismatic/types.hpp"

namespace prismatic {

// Bump when the structured-frame contract changes in a breaking way.
constexpr int kAdapterApiVersion = 1;

// ---- Systems & compatibility ---------------------------------------------
enum class System { Unknown, GB, GBC, GBA, NDS, Synthetic };

inline const char* systemName(System s) {
    switch (s) {
        case System::GB: return "GB";
        case System::GBC: return "GBC";
        case System::GBA: return "GBA";
        case System::NDS: return "NDS";
        case System::Synthetic: return "Synthetic";
        default: return "Unknown";
    }
}

// How complete the structured-capture is for a given backend/title.
enum class CompatibilityLevel {
    Level0_Unsupported = 0,  // will not run / no capture
    Level1_Framebuffer = 1,  // only the composited image is available
    Level2_Structured  = 2,  // backgrounds/sprites/palettes exposed, some gaps
    Level3_Full        = 3,  // full structured capture incl. priorities/windows
};

inline const char* compatibilityName(CompatibilityLevel l) {
    switch (l) {
        case CompatibilityLevel::Level1_Framebuffer: return "Level1_Framebuffer";
        case CompatibilityLevel::Level2_Structured: return "Level2_Structured";
        case CompatibilityLevel::Level3_Full: return "Level3_Full";
        default: return "Level0_Unsupported";
    }
}

// ---- Capability flags -----------------------------------------------------
enum CapabilityBit : uint32_t {
    Cap_Framebuffer      = 1u << 0,
    Cap_Backgrounds      = 1u << 1,
    Cap_Sprites          = 1u << 2,
    Cap_Palettes         = 1u << 3,
    Cap_Priorities       = 1u << 4,
    Cap_Scroll           = 1u << 5,
    Cap_Windows          = 1u << 6,
    Cap_Blending         = 1u << 7,
    Cap_DualScreen       = 1u << 8,
    Cap_Touch            = 1u << 9,
    Cap_Nds3D            = 1u << 10,
    Cap_TileFlip         = 1u << 11,
    Cap_PerTilePalette   = 1u << 12,
};

struct Capabilities {
    uint32_t bits = 0;
    bool has(CapabilityBit c) const { return (bits & c) != 0; }
    void add(CapabilityBit c) { bits |= c; }
};

// ---- Structured frame data ------------------------------------------------
constexpr int kTileSize = 8;

// 8x8 tile of palette indices.
struct Tile {
    std::array<uint8_t, kTileSize * kTileSize> px{};
    uint8_t at(int x, int y) const { return px[(size_t)y * kTileSize + x]; }
    void set(int x, int y, uint8_t v) { px[(size_t)y * kTileSize + x] = v; }
};

using Palette = std::vector<Color>;  // index 0 conventionally transparent for sprites

// One cell of a tilemap.
struct TileRef {
    uint16_t tileIndex = 0;
    uint8_t paletteBank = 0;  // for per-tile 16-color palettes (GBC/GBA/DS)
    bool flipH = false;
    bool flipV = false;
    uint8_t priority = 0;     // 0 = back .. higher = front
};

// A scrollable background layer.
struct BackgroundLayer {
    int index = 0;
    bool enabled = true;
    int widthTiles = 0;
    int heightTiles = 0;
    int scrollX = 0;          // pixels
    int scrollY = 0;
    uint8_t priority = 0;
    bool affine = false;
    std::vector<TileRef> map;   // widthTiles*heightTiles
    std::vector<Tile> tileset;  // shared tile graphics
    std::vector<Palette> palettes;  // one or more 16/256-color palettes

    const TileRef& cell(int tx, int ty) const { return map[(size_t)ty * widthTiles + tx]; }
};

// A hardware sprite (OAM entry), already resolved to graphics.
struct Sprite {
    int id = 0;             // stable per-frame identity hint from the backend
    int x = 0, y = 0;       // top-left in screen space
    int width = 8, height = 8;
    uint8_t priority = 0;
    bool enabled = true;
    bool flipH = false;
    bool flipV = false;
    std::vector<Tile> tiles;  // (width/8)*(height/8), row-major
    Palette palette;
    int tilesWide() const { return width / kTileSize; }
    int tilesHigh() const { return height / kTileSize; }
};

// Window/masking region (GBA/DS style).
struct WindowRegion {
    bool enabled = false;
    int left = 0, top = 0, right = 0, bottom = 0;
    uint32_t layerMask = 0xFFFFFFFF;  // which layers are visible inside
};

// Alpha/brightness blending config.
struct BlendConfig {
    enum Mode { None, Alpha, BrightnessInc, BrightnessDec } mode = None;
    float evA = 1.0f;  // top weight
    float evB = 0.0f;  // bottom weight
    float evy = 0.0f;  // brightness coefficient
};

// Optional captured NDS 3D layer (we never re-implement the 3D GPU; the backend
// hands us its rendered 3D image + optional depth so we can composite/enhance).
struct Nds3DLayer {
    bool present = false;
    Image color;         // RGBA render of the 3D layer
    FloatBuffer depth;   // optional; empty if not provided
};

// Everything needed to reconstruct one screen for one frame.
struct StructuredFrame {
    int screenWidth = 0;
    int screenHeight = 0;
    Color backdrop{0, 0, 0, 255};
    std::vector<BackgroundLayer> backgrounds;
    std::vector<Sprite> sprites;
    std::array<WindowRegion, 2> windows{};
    BlendConfig blend;
    Nds3DLayer nds3d;
    uint64_t frameIndex = 0;
};

// ---- Screen routing (single vs dual display) ------------------------------
enum class ScreenId { Single = 0, Top = 0, Bottom = 1 };

struct ScreenRouting {
    int screenCount = 1;
    // Suggested placement when two physical displays are available.
    bool topOnPrimary = true;    // top screen -> primary display
    bool allowManualSwap = true;
};

// ---- Game identity --------------------------------------------------------
struct GameIdentity {
    System system = System::Unknown;
    std::string title;        // internal title if known (never a ROM dump)
    std::string gameCode;     // e.g. cartridge code, if provided by backend
    std::string romSha256;    // hash of the user's ROM, if one was loaded
    bool romLoaded = false;
};

// ---- Input ----------------------------------------------------------------
struct InputState {
    bool up = false, down = false, left = false, right = false;
    bool a = false, b = false, x = false, y = false;
    bool l = false, r = false, start = false, select = false;
    // Touch (DS bottom screen). active=false when not touching.
    bool touchActive = false;
    int touchX = 0, touchY = 0;
};

struct AdapterInfo {
    std::string backendName;
    int apiVersion = kAdapterApiVersion;
    System system = System::Unknown;
    CompatibilityLevel compatibility = CompatibilityLevel::Level0_Unsupported;
};

// ---- The adapter interface ------------------------------------------------
//
// A backend is both *drivable* (reset/setInput/advanceFrame) and *capturable*
// (framebuffer/structuredFrame). Real cores (mGBA/melonDS) implement this by
// wrapping their internal state; the synthetic backend implements it directly
// for deterministic testing.
class EmulatorAdapter {
public:
    virtual ~EmulatorAdapter() = default;

    virtual AdapterInfo info() const = 0;
    virtual GameIdentity identity() const = 0;
    virtual Capabilities capabilities() const = 0;
    virtual ScreenRouting screenRouting() const = 0;
    virtual int screenCount() const = 0;

    // Drive.
    virtual void reset() = 0;
    virtual void setInput(const InputState& in) = 0;
    virtual void advanceFrame() = 0;
    virtual uint64_t frameIndex() const = 0;

    // Capture. screen index: 0 = single/top, 1 = bottom.
    virtual Image framebuffer(int screen) const = 0;
    virtual StructuredFrame structuredFrame(int screen) const = 0;

    // Pull up to maxFrames stereo audio sample-frames (interleaved L,R) produced
    // since the last call. Returns the number of frames written. Backends
    // without audio return 0. Safe to call from a dedicated audio thread.
    virtual int readAudio(int16_t* /*out*/, int /*maxFrames*/) { return 0; }
};

}  // namespace prismatic
