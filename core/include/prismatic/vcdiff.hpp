// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — VCDIFF (RFC 3284) delta decoder.
//
// Applies xdelta3-style binary patches: clean source ROM + .xdelta -> private
// patched installation. Clean-room implementation from RFC 3284 (no xdelta3
// code). Supports the standard code table, near/same address caches, and
// adler32 window checksums (xdelta3 default output). Secondary compression
// (VCD_DECOMPRESS) is intentionally unsupported and reported as an error —
// xdelta3 does not emit it by default.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace prismatic {

struct VcdiffResult {
    bool ok = false;
    std::string error;
    std::vector<uint8_t> output;
};

// Decode `patch` against `source`. On failure `error` names the offending
// structure (safe against truncated/malformed input; never reads out of range).
VcdiffResult vcdiffDecode(const std::vector<uint8_t>& source,
                          const std::vector<uint8_t>& patch);

// Convenience: file-based apply with byte budget guards.
bool vcdiffApplyFile(const std::string& sourcePath, const std::string& patchPath,
                     const std::string& outPath, std::string& error);

}  // namespace prismatic
