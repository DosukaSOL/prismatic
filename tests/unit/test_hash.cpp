// SPDX-License-Identifier: GPL-3.0-or-later
#include "test_util.hpp"
#include "prismatic/hash.hpp"
using namespace prismatic;

static void test_known_vectors() {
    CHECK_EQ(Sha256::hashString(""),
             std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK_EQ(Sha256::hashString("abc"),
             std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK_EQ(Sha256::hashString("The quick brown fox jumps over the lazy dog"),
             std::string("d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"));
}

static void test_long_input() {
    // 1,000,000 'a' -> known digest.
    Sha256 h;
    std::string block(1000, 'a');
    for (int i = 0; i < 1000; ++i) h.update(block);
    CHECK_EQ(h.hexDigest(),
             std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

int main() {
    RUN(test_known_vectors);
    RUN(test_long_input);
    return REPORT();
}
