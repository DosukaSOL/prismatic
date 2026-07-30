// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — dependency-free PNG writer (8-bit RGBA).
// Uses zlib "stored" (uncompressed) DEFLATE blocks so no external zlib is
// required. Output is a valid PNG readable by any decoder.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "prismatic/types.hpp"

namespace prismatic {

namespace pngdetail {

inline uint32_t crc32_of(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        init = true;
    }
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

inline uint32_t adler32_of(const uint8_t* data, size_t len) {
    uint32_t a = 1, b = 0;
    const uint32_t MOD = 65521;
    for (size_t i = 0; i < len; ++i) { a = (a + data[i]) % MOD; b = (b + a) % MOD; }
    return (b << 16) | a;
}

inline void put32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v >> 24)); out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));  out.push_back((uint8_t)(v));
}

inline void writeChunk(std::vector<uint8_t>& out, const char type[4], const std::vector<uint8_t>& data) {
    put32(out, (uint32_t)data.size());
    size_t crcStart = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uint32_t crc = crc32_of(out.data() + crcStart, out.size() - crcStart) ^ 0xFFFFFFFFu;
    put32(out, crc);
}

// Wrap raw bytes in a zlib stream using stored DEFLATE blocks.
inline std::vector<uint8_t> zlibStore(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> z;
    z.push_back(0x78);  // CMF
    z.push_back(0x01);  // FLG (no dict, fastest)
    size_t pos = 0;
    while (pos < raw.size() || raw.empty()) {
        size_t remaining = raw.size() - pos;
        uint16_t len = (uint16_t)(remaining > 65535 ? 65535 : remaining);
        bool last = (pos + len >= raw.size());
        z.push_back(last ? 1 : 0);  // BFINAL, BTYPE=00 (stored)
        z.push_back((uint8_t)(len & 0xFF));
        z.push_back((uint8_t)(len >> 8));
        uint16_t nlen = (uint16_t)~len;
        z.push_back((uint8_t)(nlen & 0xFF));
        z.push_back((uint8_t)(nlen >> 8));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + len);
        pos += len;
        if (raw.empty()) break;
    }
    put32(z, 0);  // placeholder replaced below (big-endian adler)
    z.pop_back(); z.pop_back(); z.pop_back(); z.pop_back();
    uint32_t adler = adler32_of(raw.data(), raw.size());
    z.push_back((uint8_t)(adler >> 24)); z.push_back((uint8_t)(adler >> 16));
    z.push_back((uint8_t)(adler >> 8));  z.push_back((uint8_t)(adler));
    return z;
}

}  // namespace pngdetail

// Serialize an image to PNG bytes.
inline std::vector<uint8_t> encodePng(const Image& img) {
    using namespace pngdetail;
    std::vector<uint8_t> raw;
    raw.reserve((size_t)(img.width * 4 + 1) * img.height);
    for (int y = 0; y < img.height; ++y) {
        raw.push_back(0);  // filter type: none
        for (int x = 0; x < img.width; ++x) {
            const Color& c = img.at(x, y);
            raw.push_back(c.r); raw.push_back(c.g); raw.push_back(c.b); raw.push_back(c.a);
        }
    }
    std::vector<uint8_t> out;
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.insert(out.end(), sig, sig + 8);

    std::vector<uint8_t> ihdr;
    put32(ihdr, (uint32_t)img.width);
    put32(ihdr, (uint32_t)img.height);
    ihdr.push_back(8);   // bit depth
    ihdr.push_back(6);   // color type RGBA
    ihdr.push_back(0);   // compression
    ihdr.push_back(0);   // filter
    ihdr.push_back(0);   // interlace
    writeChunk(out, "IHDR", ihdr);

    writeChunk(out, "IDAT", zlibStore(raw));
    writeChunk(out, "IEND", {});
    return out;
}

// Write an image to a PNG file. Returns false on I/O failure.
inline bool writePng(const std::string& path, const Image& img) {
    std::vector<uint8_t> bytes = encodePng(img);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t written = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return written == bytes.size();
}

}  // namespace prismatic
