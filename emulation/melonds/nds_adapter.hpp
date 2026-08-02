// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — Nintendo DS backend adapter (melonDS core).
//
// Wraps a melonDS NDS instance behind PRISMATIC's EmulatorAdapter so the
// enhancement pipeline can drive a real DS game. Reports Level1_Framebuffer:
// PRISMATIC re-shades the emulator's real output pixels (screen-space HD-2.5D)
// and never invents or replaces game art.
#pragma once
#include <memory>
#include <string>
#include "prismatic/adapter.hpp"

namespace prismatic {

// Create a melonDS-backed adapter with `romPath` loaded and direct-booted.
// `dataDir` is a writable directory for battery saves + generated firmware; the
// adapter creates a `saves/` subfolder there and stores `<gamecode>_<hash>.sav`,
// which is auto-loaded next time the same game is opened. `savePathOverride`
// (optional) pins the battery save to an exact file instead — used by the game
// platform so every mod build of one install shares one save. No external BIOS
// is required (melonDS FreeBIOS + generated firmware). `enableJit` turns on the
// A64 recompiler (faster); pass false for the safe interpreter. Returns nullptr
// and fills `error` on failure.
std::unique_ptr<EmulatorAdapter> makeNdsAdapter(const std::string& romPath,
                                                const std::string& dataDir,
                                                std::string* error = nullptr,
                                                bool enableJit = true,
                                                const std::string& savePathOverride = "");

}  // namespace prismatic
