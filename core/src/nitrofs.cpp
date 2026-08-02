// SPDX-License-Identifier: GPL-3.0-or-later
// Clean-room NitroFS/NARC readers per GBATEK "NitroROM filesystem".
#include "prismatic/nitrofs.hpp"

#include <cstring>

namespace prismatic {
namespace {

uint16_t rd16(const std::vector<uint8_t>& b, size_t off) {
    if (off + 2 > b.size()) return 0;
    return (uint16_t)(b[off] | (b[off + 1] << 8));
}
uint32_t rd32(const std::vector<uint8_t>& b, size_t off) {
    if (off + 4 > b.size()) return 0;
    return (uint32_t)(b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24));
}

// Recursive FNT walk. dirId is 0xF000-based; guards bound recursion and every
// table read so malformed images cannot loop or read out of range.
void walkDir(const std::vector<uint8_t>& rom, uint32_t fntBase, uint32_t fatBase,
             uint32_t fatSize, uint16_t dirId, const std::string& prefix,
             int depth, NitroFsResult& out) {
    if (depth > 32) { out.error = "directory nesting too deep"; return; }
    uint32_t entryOff = fntBase + (uint32_t)(dirId & 0x0FFF) * 8;
    uint32_t subOff = fntBase + rd32(rom, entryOff);
    uint16_t fileId = rd16(rom, entryOff + 4);

    size_t p = subOff;
    while (true) {
        if (p >= rom.size()) { out.error = "FNT subtable out of range"; return; }
        uint8_t len = rom[p++];
        if (len == 0) break;                       // end of subtable
        bool isDir = (len & 0x80) != 0;
        uint8_t nameLen = len & 0x7F;
        if (nameLen == 0 || p + nameLen > rom.size()) { out.error = "bad FNT name"; return; }
        std::string name(reinterpret_cast<const char*>(&rom[p]), nameLen);
        p += nameLen;
        if (isDir) {
            uint16_t sub = rd16(rom, p);
            p += 2;
            walkDir(rom, fntBase, fatBase, fatSize, sub,
                    prefix.empty() ? name : prefix + "/" + name, depth + 1, out);
            if (!out.error.empty()) return;
        } else {
            uint32_t fatOff = fatBase + (uint32_t)fileId * 8;
            if (fatOff + 8 > fatBase + fatSize || fatOff + 8 > rom.size()) {
                out.error = "FAT entry out of range";
                return;
            }
            NitroFile f;
            f.id = fileId++;
            f.path = prefix.empty() ? name : prefix + "/" + name;
            f.offset = rd32(rom, fatOff);
            uint32_t end = rd32(rom, fatOff + 4);
            f.size = end > f.offset ? end - f.offset : 0;
            out.files.push_back(std::move(f));
        }
    }
}

}  // namespace

NitroFsResult parseNitroFs(const std::vector<uint8_t>& rom) {
    NitroFsResult res;
    if (rom.size() < 0x200) { res.error = "image too small"; return res; }
    uint32_t fntOff = rd32(rom, 0x40);
    uint32_t fntSize = rd32(rom, 0x44);
    uint32_t fatOff = rd32(rom, 0x48);
    uint32_t fatSize = rd32(rom, 0x4C);
    if (fntOff == 0 || fatOff == 0 || fntSize == 0 || fatSize == 0 ||
        (uint64_t)fntOff + fntSize > rom.size() || (uint64_t)fatOff + fatSize > rom.size()) {
        res.error = "no filesystem tables";
        return res;
    }
    walkDir(rom, fntOff, fatOff, fatSize, 0xF000, "", 0, res);
    if (!res.error.empty()) { res.files.clear(); return res; }
    res.ok = true;
    return res;
}

const NitroFile* findNitroFile(const NitroFsResult& fs, const std::string& path) {
    for (const auto& f : fs.files)
        if (f.path == path) return &f;
    return nullptr;
}

bool readNitroFile(const std::vector<uint8_t>& rom, const NitroFile& f,
                   std::vector<uint8_t>& out) {
    if ((uint64_t)f.offset + f.size > rom.size()) return false;
    out.assign(rom.begin() + f.offset, rom.begin() + f.offset + f.size);
    return true;
}

// ---- NARC -------------------------------------------------------------------

NarcResult parseNarc(const std::vector<uint8_t>& n) {
    NarcResult res;
    if (n.size() < 0x1C || std::memcmp(n.data(), "NARC", 4) != 0) {
        res.error = "not a NARC";
        return res;
    }
    // Generic chunk walk: BTAF (FAT), BTNF (names, unused here), GMIF (data).
    size_t off = 0x10;
    uint32_t fatCount = 0;
    size_t fatOff = 0, gmifData = 0;
    while (off + 8 <= n.size()) {
        char tag[5] = {0};
        std::memcpy(tag, &n[off], 4);
        uint32_t size = rd32(n, off + 4);
        if (size < 8 || off + size > n.size()) { res.error = "bad NARC chunk"; return res; }
        if (std::memcmp(tag, "BTAF", 4) == 0) {
            fatCount = rd32(n, off + 8);
            fatOff = off + 12;
        } else if (std::memcmp(tag, "GMIF", 4) == 0) {
            gmifData = off + 8;
        }
        off += size;
    }
    if (fatOff == 0 || gmifData == 0) { res.error = "missing BTAF/GMIF"; return res; }
    for (uint32_t i = 0; i < fatCount; ++i) {
        uint32_t s = rd32(n, fatOff + (size_t)i * 8);
        uint32_t e = rd32(n, fatOff + (size_t)i * 8 + 4);
        if (e < s || (uint64_t)gmifData + e > n.size()) { res.error = "bad BTAF entry"; return res; }
        res.entries.push_back({(uint32_t)(gmifData + s), e - s});
    }
    res.ok = true;
    return res;
}

bool readNarcEntry(const std::vector<uint8_t>& narc, const NarcResult& parsed,
                   size_t index, std::vector<uint8_t>& out) {
    if (index >= parsed.entries.size()) return false;
    auto [start, size] = parsed.entries[index];
    if ((uint64_t)start + size > narc.size()) return false;
    out.assign(narc.begin() + start, narc.begin() + start + size);
    return true;
}

}  // namespace prismatic
