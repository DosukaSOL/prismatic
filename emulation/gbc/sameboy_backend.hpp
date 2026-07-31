// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — Game Boy / Game Boy Color backend over the SameBoy core.
//
// Wraps SameBoy (MIT) behind PRISMATIC's EmulatorAdapter. Beyond the faithful
// 160x144 framebuffer, it reconstructs a *structured* frame from the real
// hardware state (BG tilemap + tile graphics + CGB palettes + OAM sprites) read
// straight out of VRAM. That structured tilemap is what feeds the tile-extruded
// voxel 2.5D — the honest analog of the gen1recomp/DramaticShape diorama, built
// only from the game's own tiles.
#pragma once
#include <memory>
#include <string>
#include "prismatic/adapter.hpp"

namespace prismatic {

// romPath: the user's own .gb/.gbc dump (never shipped). bootRomPath: SameBoy's
// open-source CGB boot ROM (built from third_party/SameBoy/BootROMs). If the
// boot ROM is missing the core still runs, just without the boot animation.
std::unique_ptr<EmulatorAdapter> makeGbcBackend(const std::string& romPath,
                                                const std::string& bootRomPath);

// Load from memory (used on Android where the ROM lives in app storage). romData
// is copied; the caller keeps ownership.
std::unique_ptr<EmulatorAdapter> makeGbcBackendFromMemory(
    const uint8_t* romData, size_t romSize,
    const uint8_t* bootRom, size_t bootRomSize);

}  // namespace prismatic
