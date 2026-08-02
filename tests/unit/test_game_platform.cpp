// SPDX-License-Identifier: GPL-3.0-or-later
#include "test_util.hpp"
#include "prismatic/vcdiff.hpp"
#include "prismatic/game_library.hpp"
#include "prismatic/mod_packages.hpp"
#include "prismatic/nitrofs.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace prismatic;
namespace fs = std::filesystem;

namespace {
void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) {
    b[off] = (uint8_t)v; b[off + 1] = (uint8_t)(v >> 8);
}
void put32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[off + i] = (uint8_t)(v >> (i * 8));
}
}  // namespace

// Synthetic NitroFS: root { "dir" { "b.bin" }, "a.bin" } with 2 FAT entries.
static void test_nitrofs_synthetic() {
    std::vector<uint8_t> rom(0x400, 0);
    const uint32_t fnt = 0x200, fat = 0x300, dataA = 0x380, dataB = 0x390;
    put32(rom, 0x40, fnt); put32(rom, 0x44, 0x100);
    put32(rom, 0x48, fat); put32(rom, 0x4C, 16);
    // FNT main table: root (0xF000) at +0, dir 0xF001 at +8.
    put32(rom, fnt + 0, 0x10);      // root subtable offset
    put16(rom, fnt + 4, 0);         // root first file id
    put16(rom, fnt + 6, 1);         // dir count
    put32(rom, fnt + 8, 0x20);      // dir subtable offset
    put16(rom, fnt + 12, 1);        // dir first file id
    put16(rom, fnt + 14, 0xF000);   // parent
    // root subtable: file "a.bin", dir "dir" -> 0xF001, end.
    size_t p = fnt + 0x10;
    rom[p++] = 5; memcpy(&rom[p], "a.bin", 5); p += 5;
    rom[p++] = 0x83; memcpy(&rom[p], "dir", 3); p += 3;
    put16(rom, p, 0xF001); p += 2;
    rom[p++] = 0;
    // dir subtable: file "b.bin", end.
    p = fnt + 0x20;
    rom[p++] = 5; memcpy(&rom[p], "b.bin", 5); p += 5;
    rom[p++] = 0;
    // FAT: file0 = a.bin (8 bytes), file1 = b.bin (4 bytes).
    put32(rom, fat + 0, dataA); put32(rom, fat + 4, dataA + 8);
    put32(rom, fat + 8, dataB); put32(rom, fat + 12, dataB + 4);
    memcpy(&rom[dataA], "AAAAAAAA", 8);
    memcpy(&rom[dataB], "BBBB", 4);

    NitroFsResult r = parseNitroFs(rom);
    CHECK(r.ok);
    CHECK_EQ(r.files.size(), (size_t)2);
    const NitroFile* a = findNitroFile(r, "a.bin");
    const NitroFile* b = findNitroFile(r, "dir/b.bin");
    CHECK(a != nullptr);
    CHECK(b != nullptr);
    std::vector<uint8_t> data;
    CHECK(a && readNitroFile(rom, *a, data) && data.size() == 8 && data[0] == 'A');
    CHECK(b && readNitroFile(rom, *b, data) && data.size() == 4 && data[0] == 'B');
    // Malformed image: FNT points past the end.
    std::vector<uint8_t> bad(0x200, 0);
    put32(bad, 0x40, 0x10000); put32(bad, 0x44, 4);
    put32(bad, 0x48, 0x100);  put32(bad, 0x4C, 8);
    CHECK(!parseNitroFs(bad).ok);
}

static void test_narc_synthetic() {
    // NARC with 2 subfiles: "XY" and "Z".
    std::vector<uint8_t> n(0x40, 0);
    memcpy(&n[0], "NARC", 4);
    put32(n, 8, 0x40);              // total size
    // BTAF at 0x10: size 0x1C, count 2, entries (0,2) (2,3).
    memcpy(&n[0x10], "BTAF", 4); put32(n, 0x14, 0x1C);
    put32(n, 0x18, 2);
    put32(n, 0x1C, 0); put32(n, 0x20, 2);
    put32(n, 0x24, 2); put32(n, 0x28, 3);
    // GMIF at 0x2C: size 0x14, data at 0x34.
    memcpy(&n[0x2C], "GMIF", 4); put32(n, 0x30, 0x14);
    n[0x34] = 'X'; n[0x35] = 'Y'; n[0x36] = 'Z';

    NarcResult r = parseNarc(n);
    CHECK(r.ok);
    CHECK_EQ(r.entries.size(), (size_t)2);
    std::vector<uint8_t> sub;
    CHECK(readNarcEntry(n, r, 0, sub) && sub.size() == 2 && sub[0] == 'X');
    CHECK(readNarcEntry(n, r, 1, sub) && sub.size() == 1 && sub[0] == 'Z');
    CHECK(!readNarcEntry(n, r, 2, sub));
    std::vector<uint8_t> junk{'N', 'O', 'P', 'E'};
    CHECK(!parseNarc(junk).ok);
}

static void test_vcdiff_rejects_garbage() {
    std::vector<uint8_t> src{1, 2, 3};
    // bad magic
    VcdiffResult r = vcdiffDecode(src, {0, 1, 2, 3, 4, 5});
    CHECK(!r.ok);
    // valid magic, truncated header
    r = vcdiffDecode(src, {0xD6, 0xC3, 0xC4, 0x00});
    CHECK(!r.ok);
    // secondary compression declared -> explicit unsupported error
    r = vcdiffDecode(src, {0xD6, 0xC3, 0xC4, 0x00, 0x01, 0x02});
    CHECK(!r.ok);
    CHECK(r.error.find("VCD_DECOMPRESS") != std::string::npos);
    // empty patch stream (header only) decodes to empty output
    r = vcdiffDecode(src, {0xD6, 0xC3, 0xC4, 0x00, 0x00});
    CHECK(r.ok);
    CHECK_EQ(r.output.size(), (size_t)0);
}

// Real xdelta3 fixtures (generated with `xdelta3 -e -S none -A=`).
static void test_vcdiff_add_copy_run() {
    // Fixture 1: "world" -> "helloworld" (pure ADD window + adler32).
    std::vector<uint8_t> src1{'w', 'o', 'r', 'l', 'd'};
    std::vector<uint8_t> p1{
        0xD6, 0xC3, 0xC4, 0x00, 0x00, 0x04, 0x14, 0x0A, 0x00, 0x0A, 0x01, 0x00,
        0x17, 0x36, 0x04, 0x3D, 0x68, 0x65, 0x6C, 0x6C, 0x6F, 0x77, 0x6F, 0x72,
        0x6C, 0x64, 0x0B};
    VcdiffResult r = vcdiffDecode(src1, p1);
    CHECK(r.ok);
    CHECK(std::string(r.output.begin(), r.output.end()) == "helloworld");

    // Fixture 2: 2KB pattern with 8 bytes inserted mid-stream (COPY windows,
    // near/same address cache, adler32).
    std::vector<uint8_t> src2(2048);
    for (int i = 0; i < 2048; ++i) src2[(size_t)i] = (uint8_t)(i & 0xFF);
    std::vector<uint8_t> want = src2;
    const char* ins = "INSERTED";
    want.insert(want.begin() + 1000, ins, ins + 8);
    std::vector<uint8_t> p2{
        0xD6, 0xC3, 0xC4, 0x00, 0x00, 0x05, 0x90, 0x00, 0x00, 0x1C, 0x90, 0x08,
        0x00, 0x08, 0x07, 0x03, 0x37, 0xAE, 0xFE, 0x8C, 0x49, 0x4E, 0x53, 0x45,
        0x52, 0x54, 0x45, 0x44, 0x13, 0x87, 0x68, 0x09, 0x13, 0x88, 0x18, 0x00,
        0x87, 0x68};
    r = vcdiffDecode(src2, p2);
    CHECK(r.ok);
    CHECK(r.output == want);
}

static void test_rom_identity_synthetic() {
    // 512-byte fake NDS header: title + IPKE code + capacity byte.
    fs::path dir = fs::temp_directory_path() / "prism_test";
    fs::create_directories(dir);
    fs::path rom = dir / "fake.nds";
    std::vector<uint8_t> hdr(0x200, 0);
    std::memcpy(hdr.data(), "POKEMON HG\0\0", 12);
    std::memcpy(hdr.data() + 0x0C, "IPKE", 4);
    hdr[0x1E] = 0;
    hdr[0x14] = 10;  // capacity 128MB (file is tiny -> trimmed)
    std::ofstream(rom, std::ios::binary)
        .write(reinterpret_cast<char*>(hdr.data()), (std::streamsize)hdr.size());

    RomIdentity id;
    std::string err;
    CHECK(identifyRom(rom.string(), id, err));
    CHECK_EQ(id.gameCode, std::string("IPKE"));
    CHECK_EQ(id.title, std::string("POKEMON HG"));
    CHECK(id.trimmed);
    classifyRom(id, builtinHgssDatabase());
    // Hash differs from the verified dump -> Modified (clean hash IS known).
    CHECK(id.verdict == RomVerdict::Modified);
    CHECK_EQ(id.edition, std::string("heartgold"));
    fs::remove_all(dir);
}

static void test_install_json_roundtrip() {
    GameInstall g;
    g.id = "heartgold_abcd1234";
    g.family = "pokemon-hgss";
    g.edition = "heartgold";
    g.displayName = "Pokémon HeartGold";
    g.sourceSha256 = "abcd";
    g.activeProfile = "visual-plus";
    g.enabledMods = {"visual-plus-hgss@full"};
    g.playSeconds = 4242;
    GameInstall q;
    CHECK(gameInstallFromJson(gameInstallToJson(g), q));
    CHECK_EQ(q.id, g.id);
    CHECK_EQ(q.activeProfile, g.activeProfile);
    CHECK_EQ(q.enabledMods.size(), (size_t)1);
    CHECK_EQ(q.playSeconds, g.playSeconds);
}

static void test_prismod_roundtrip() {
    ModPackage m;
    m.id = "visual-plus-hgss";
    m.name = "Visual+";
    m.modVersion = "1.0.0";
    m.family = "pokemon-hgss";
    m.sourceRepo = "DosukaSOL/pokemon-hgss-visual-mod";
    ModArtifact a;
    a.edition = "heartgold";
    a.variant = "full";
    a.patchFile = "hg-full.xdelta";
    a.sourceRomSha256 = "aa";
    a.patchedRomSha256 = "bb";
    m.artifacts.push_back(a);
    ModPackage q;
    std::string err;
    CHECK(parseModPackage(serializeModPackage(m), q, err));
    CHECK_EQ(q.id, m.id);
    CHECK_EQ(q.artifacts.size(), (size_t)1);
    CHECK(findArtifact(q, "heartgold", "full") != nullptr);
    CHECK(findArtifact(q, "soulsilver", "full") == nullptr);

    CHECK(!parseModPackage("{ bad", q, err));
    CHECK(!parseModPackage("{\"version\":99,\"id\":\"x\"}", q, err));
}

static void test_builtin_profiles() {
    auto ps = hgssBuiltinProfiles();
    CHECK(ps.size() >= 4);
    CHECK_EQ(ps[0].id, std::string("vanilla"));
    CHECK(ps[0].modId.empty());
    bool haveVp = false;
    for (auto& p : ps)
        if (p.id == "visual-plus") { haveVp = true; CHECK_EQ(p.variant, std::string("full")); }
    CHECK(haveVp);
}

int main() {
    RUN(test_vcdiff_rejects_garbage);
    RUN(test_vcdiff_add_copy_run);
    RUN(test_rom_identity_synthetic);
    RUN(test_install_json_roundtrip);
    RUN(test_prismod_roundtrip);
    RUN(test_builtin_profiles);
    RUN(test_nitrofs_synthetic);
    RUN(test_narc_synthetic);
    return REPORT();
}
