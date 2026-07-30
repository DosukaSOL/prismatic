// SPDX-License-Identifier: GPL-3.0-or-later
#include "test_util.hpp"
#include "prismatic/png.hpp"
#include <vector>
using namespace prismatic;

// Walk a PNG byte stream, verifying signature and per-chunk CRC. Proves the
// writer emits structurally valid PNG with correct checksums.
static bool validatePng(const std::vector<uint8_t>& b, int& w, int& h) {
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (b.size() < 8) return false;
    for (int i = 0; i < 8; ++i) if (b[i] != sig[i]) return false;
    size_t p = 8;
    bool sawIHDR = false, sawIEND = false;
    while (p + 12 <= b.size()) {
        uint32_t len = (b[p] << 24) | (b[p + 1] << 16) | (b[p + 2] << 8) | b[p + 3];
        const uint8_t* type = &b[p + 4];
        if (p + 12 + len > b.size()) return false;
        uint32_t crc = (b[p + 8 + len] << 24) | (b[p + 9 + len] << 16) | (b[p + 10 + len] << 8) | b[p + 11 + len];
        uint32_t calc = pngdetail::crc32_of(&b[p + 4], len + 4) ^ 0xFFFFFFFFu;
        if (crc != calc) return false;
        if (std::memcmp(type, "IHDR", 4) == 0) {
            sawIHDR = true;
            w = (b[p + 8] << 24) | (b[p + 9] << 16) | (b[p + 10] << 8) | b[p + 11];
            h = (b[p + 12] << 24) | (b[p + 13] << 16) | (b[p + 14] << 8) | b[p + 15];
        }
        if (std::memcmp(type, "IEND", 4) == 0) sawIEND = true;
        p += 12 + len;
    }
    return sawIHDR && sawIEND && p == b.size();
}

static void test_encode() {
    Image img(7, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 7; ++x)
            img.at(x, y) = Color{(uint8_t)(x * 30), (uint8_t)(y * 40), 128, 255};
    auto bytes = encodePng(img);
    int w = 0, h = 0;
    CHECK(validatePng(bytes, w, h));
    CHECK_EQ(w, 7);
    CHECK_EQ(h, 5);
}

static void test_large() {
    // Exceed a single 65535-byte stored block to exercise block splitting.
    Image img(200, 200);
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 200; ++x) img.at(x, y) = Color{(uint8_t)x, (uint8_t)y, 0, 255};
    auto bytes = encodePng(img);
    int w = 0, h = 0;
    CHECK(validatePng(bytes, w, h));
    CHECK_EQ(w, 200);
    CHECK_EQ(h, 200);
}

int main() {
    RUN(test_encode);
    RUN(test_large);
    return REPORT();
}
