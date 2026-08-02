// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — Nitro (NDS) filesystem and NARC archive extraction.
//
// Clean-room readers for the cartridge filesystem (FNT/FAT, GBATEK
// "NitroROM") and the NARC container format used for nearly all HGSS game
// data. This is the local-extraction stage of the game platform: the user's
// own ROM is the only data source, extraction output stays in the private
// installation and is never redistributed.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace prismatic {

struct NitroFile {
    uint16_t id = 0;
    std::string path;      // "a/0/2/8", "poketool/..." etc.
    uint32_t offset = 0;   // absolute ROM offset
    uint32_t size = 0;
};

struct NitroFsResult {
    bool ok = false;
    std::string error;
    std::vector<NitroFile> files;
};

// Parse the ROM's filesystem tables (header FNT/FAT at 0x40..0x4C). The ROM
// buffer must be the full image; only tables are touched, nothing is copied.
NitroFsResult parseNitroFs(const std::vector<uint8_t>& rom);

// Find a file by exact path ("" -> nullptr).
const NitroFile* findNitroFile(const NitroFsResult& fs, const std::string& path);

// Copy one file's bytes out of the ROM image (bounds-checked).
bool readNitroFile(const std::vector<uint8_t>& rom, const NitroFile& f,
                   std::vector<uint8_t>& out);

// ---- NARC ------------------------------------------------------------------

struct NarcResult {
    bool ok = false;
    std::string error;
    // Subfile byte ranges into the NARC image (start, size), index-ordered.
    std::vector<std::pair<uint32_t, uint32_t>> entries;
};

// Parse a NARC container ("NARC" + BTAF/BTNF/GMIF sections).
NarcResult parseNarc(const std::vector<uint8_t>& narc);

// Copy subfile `index` out of the NARC image.
bool readNarcEntry(const std::vector<uint8_t>& narc, const NarcResult& parsed,
                   size_t index, std::vector<uint8_t>& out);

}  // namespace prismatic
