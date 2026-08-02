// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — structured per-frame scene stream.
//
// The honest input contract for the game-aware shader engine. A backend fills
// what it can genuinely capture each frame; every field carries a validity
// flag so downstream stages never operate on guessed data. This is what
// separates PRISMATIC's lighting from framebuffer-only post-processing:
// depth, layer provenance, camera matrices and emulated-RTC time all come
// from the emulator core itself, not from analysing final pixels.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "prismatic/types.hpp"

namespace prismatic {

// Per-pixel layer provenance byte (DS compositor blending encoding, captured
// from the winning pixel of the 2D compositor):
enum LayerMaskBit : uint8_t {
    LM_BG0      = 0x01,
    LM_BG1      = 0x02,
    LM_BG2      = 0x04,
    LM_BG3      = 0x08,
    LM_Sprite   = 0x10,
    LM_Backdrop = 0x20,
    LM_3D       = 0x40,
    LM_SpriteSemi = 0x80,  // semi-transparent sprite modifier
};

inline bool maskIs3D(uint8_t m)     { return (m & LM_3D) != 0; }
inline bool maskIsSprite(uint8_t m) { return (m & (LM_Sprite | LM_SpriteSemi)) != 0; }

// 4x4 matrices captured from the geometry engine, row-major float.
struct CameraCapture {
    bool valid = false;
    float proj[16] = {};       // projection at end of frame
    float viewStack0[16] = {}; // position-matrix stack bottom (typical camera view)
    float clip[16] = {};       // last clip matrix (proj * view at capture time)
};

// Emulated wall-clock (RTC) as the game sees it.
struct EmuDateTime {
    bool valid = false;
    int year = 0, month = 0, day = 0;
    int hour = 0, minute = 0, second = 0;
};

// One screen's structured capture for one frame. Pointers reference
// adapter-owned storage valid until the next advanceFrame().
struct SceneStream {
    int width = 0, height = 0;
    uint64_t frameIndex = 0;

    // Layer provenance per pixel (width*height), or nullptr if not captured.
    const uint8_t* layerMask = nullptr;

    // Normalised depth 0(near)..1(far) for pixels where maskIs3D() holds, or
    // nullptr when this screen carried no real 3D geometry this frame.
    const FloatBuffer* depth = nullptr;

    // Absolute 24-bit rasteriser depth scaled to 0..1 (value/0xFFFFFF), same
    // validity as `depth`. Preserves the true non-linear mapping needed to
    // unproject pixels into world space via the captured camera matrices.
    const FloatBuffer* depthAbs = nullptr;

    CameraCapture camera;
    EmuDateTime time;
};

}  // namespace prismatic
