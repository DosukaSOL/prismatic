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
// `dataDir` is a writable directory for saves / generated firmware. No external
// BIOS is required (melonDS FreeBIOS + generated firmware are used). Returns
// nullptr and fills `error` on failure. The .sav file is `<romPath>.sav`.
std::unique_ptr<EmulatorAdapter> makeNdsAdapter(const std::string& romPath,
                                                const std::string& dataDir,
                                                std::string* error = nullptr);

}  // namespace prismatic
