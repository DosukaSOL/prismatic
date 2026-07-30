// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — configuration hooks for the melonDS Platform layer.
#pragma once
#include <cstdint>
#include <string>

namespace prismatic::melon {

// Called by the core when the NDS save memory changes; receives the full save
// buffer. The adapter registers a sink that persists it (e.g. to a .sav file).
using SaveSink = void (*)(const uint8_t* data, uint32_t len, void* userdata);

// Base directory used to resolve OpenLocalFile() relative paths (savestates,
// generated firmware, etc.). Set to the app's writable data dir on Android.
void setBaseDir(const std::string& dir);

// Register the NDS save writeback sink (nullptr to disable).
void setNdsSaveSink(SaveSink sink, void* userdata);

}  // namespace prismatic::melon
