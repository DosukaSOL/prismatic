// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — native (ground-truth) compositor.
// Rasterizes a StructuredFrame exactly as tile hardware would, so it can serve
// both as a backend's framebuffer and as the fidelity reference the enhanced
// renderer is compared against.
//
// Priority convention (PRISMATIC-wide): priority 0 = backmost, larger = front.
// Within a priority level, backgrounds are drawn before sprites (sprites win
// ties), matching GBA/DS OBJ-over-BG behavior. Palette index 0 is transparent.
#pragma once
#include "prismatic/adapter.hpp"
#include "prismatic/types.hpp"

namespace prismatic {

// Composite a structured frame into an RGBA image (native look, no enhancement).
Image compositeNative(const StructuredFrame& frame);

// Maximum priority level scanned by the compositor.
constexpr int kMaxPriority = 8;

}  // namespace prismatic
