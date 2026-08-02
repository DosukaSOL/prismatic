// SPDX-License-Identifier: GPL-3.0-or-later
// Clean-room VCDIFF decoder per RFC 3284. See vcdiff.hpp.
#include "prismatic/vcdiff.hpp"

#include <cstring>
#include <fstream>

namespace prismatic {
namespace {

// ---- byte reader with hard bounds ------------------------------------------
struct Reader {
    const uint8_t* p = nullptr;
    size_t n = 0;
    size_t off = 0;
    bool fail = false;

    uint8_t u8() {
        if (off >= n) { fail = true; return 0; }
        return p[off++];
    }
    // RFC 3284 base-128 varint, MSB-first with continuation bit.
    uint64_t varint() {
        uint64_t v = 0;
        for (int i = 0; i < 10; ++i) {
            uint8_t b = u8();
            if (fail) return 0;
            v = (v << 7) | (uint64_t)(b & 0x7F);
            if (!(b & 0x80)) return v;
        }
        fail = true;  // over-long
        return 0;
    }
    bool bytes(void* dst, size_t len) {
        if (off + len > n) { fail = true; return false; }
        std::memcpy(dst, p + off, len);
        off += len;
        return true;
    }
    Reader slice(size_t len) {
        Reader r;
        if (off + len > n) { fail = true; return r; }
        r.p = p + off; r.n = len;
        off += len;
        return r;
    }
};

// ---- RFC 3284 section 5.4: default instruction code table ------------------
enum { VCD_NOOP = 0, VCD_ADD = 1, VCD_RUN = 2, VCD_COPY = 3 };

struct Inst { uint8_t type; uint8_t size; uint8_t mode; };
struct CodeEntry { Inst i1, i2; };

const CodeEntry* defaultCodeTable() {
    static CodeEntry t[256];
    static bool built = false;
    if (built) return t;
    int idx = 0;
    auto put = [&](Inst a, Inst b) { t[idx++] = {a, b}; };
    // entry 0: RUN 0
    put({VCD_RUN, 0, 0}, {VCD_NOOP, 0, 0});
    // entries 1..18: ADD size 0,1..17
    for (int s = 0; s <= 17; ++s) put({VCD_ADD, (uint8_t)s, 0}, {VCD_NOOP, 0, 0});
    // entries 19..162: COPY mode 0..8, size 0,4..18
    for (int m = 0; m <= 8; ++m) {
        put({VCD_COPY, 0, (uint8_t)m}, {VCD_NOOP, 0, 0});
        for (int s = 4; s <= 18; ++s) put({VCD_COPY, (uint8_t)s, (uint8_t)m}, {VCD_NOOP, 0, 0});
    }
    // entries 163..234: ADD 1..4 + COPY mode 0..5 size 4..6
    for (int m = 0; m <= 5; ++m)
        for (int as = 1; as <= 4; ++as)
            for (int cs = 4; cs <= 6; ++cs)
                put({VCD_ADD, (uint8_t)as, 0}, {VCD_COPY, (uint8_t)cs, (uint8_t)m});
    // entries 235..246: ADD 1..4 + COPY mode 6..8 size 4
    for (int m = 6; m <= 8; ++m)
        for (int as = 1; as <= 4; ++as)
            put({VCD_ADD, (uint8_t)as, 0}, {VCD_COPY, 4, (uint8_t)m});
    // entries 247..255: COPY mode 0..8 size 4 + ADD 1
    for (int m = 0; m <= 8; ++m)
        put({VCD_COPY, 4, (uint8_t)m}, {VCD_ADD, 1, 0});
    built = true;
    return t;
}

// ---- RFC 3284 section 5.1: address cache -----------------------------------
struct AddrCache {
    static constexpr int kNear = 4, kSame = 3;
    uint64_t near_[kNear] = {};
    uint64_t same_[kSame * 256] = {};
    int nextNear = 0;

    void update(uint64_t addr) {
        near_[nextNear] = addr;
        nextNear = (nextNear + 1) % kNear;
        same_[addr % (kSame * 256)] = addr;
    }
    // mode semantics per RFC 3284 5.3; addrSection supplies encoded operands.
    uint64_t decode(Reader& addrs, uint64_t here, uint8_t mode, bool& ok) {
        ok = true;
        uint64_t a = 0;
        if (mode == 0) a = addrs.varint();                       // SELF
        else if (mode == 1) a = here - addrs.varint();           // HERE
        else if (mode < 2 + kNear) a = near_[mode - 2] + addrs.varint();
        else {                                                    // SAME
            uint8_t b = addrs.u8();
            a = same_[(uint64_t)(mode - (2 + kNear)) * 256 + b];
        }
        if (addrs.fail) { ok = false; return 0; }
        update(a);
        return a;
    }
};

uint32_t adler32(const uint8_t* d, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; ++i) {
        a = (a + d[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

}  // namespace

VcdiffResult vcdiffDecode(const std::vector<uint8_t>& source,
                          const std::vector<uint8_t>& patch) {
    VcdiffResult res;
    Reader r{patch.data(), patch.size()};

    // Header: 0xD6 'V' 0xC4 'C' 0xC4 'D' 0x00, hdr_indicator.
    uint8_t m0 = r.u8(), m1 = r.u8(), m2 = r.u8(), v = r.u8();
    if (r.fail || m0 != 0xD6 || m1 != 0xC3 || m2 != 0xC4 || v != 0x00) {
        res.error = "not a VCDIFF stream (bad magic)";
        return res;
    }
    uint8_t hdrInd = r.u8();
    if (hdrInd & 0x01) { res.error = "VCD_DECOMPRESS not supported"; return res; }
    if (hdrInd & 0x02) { res.error = "VCD_CODETABLE (custom table) not supported"; return res; }
    if (hdrInd & 0x04) {  // VCD_APPHEADER (xdelta3 extension): skip
        uint64_t apLen = r.varint();
        r.slice((size_t)apLen);
    }
    if (r.fail) { res.error = "truncated header"; return res; }

    const CodeEntry* table = defaultCodeTable();
    constexpr uint64_t kMaxOut = 1ull << 31;  // 2 GiB output guard

    while (r.off < r.n) {
        // ---- window header ----
        uint8_t winInd = r.u8();
        if (r.fail) break;
        uint64_t srcLen = 0, srcPos = 0;
        bool fromSource = (winInd & 0x01) != 0;  // VCD_SOURCE
        bool fromTarget = (winInd & 0x02) != 0;  // VCD_TARGET
        if (fromSource || fromTarget) {
            srcLen = r.varint();
            srcPos = r.varint();
        }
        uint64_t deltaLen = r.varint(); (void)deltaLen;
        uint64_t targetLen = r.varint();
        uint8_t deltaInd = r.u8();
        if (deltaInd & 0x07) { res.error = "compressed delta sections not supported"; return res; }
        uint64_t dataLen = r.varint();
        uint64_t instLen = r.varint();
        uint64_t addrLen = r.varint();
        uint32_t checksum = 0;
        bool hasChecksum = (winInd & 0x04) != 0;  // VCD_ADLER32 (xdelta3)
        if (hasChecksum) {
            uint8_t cs[4];
            if (!r.bytes(cs, 4)) { res.error = "truncated checksum"; return res; }
            checksum = ((uint32_t)cs[0] << 24) | ((uint32_t)cs[1] << 16) |
                       ((uint32_t)cs[2] << 8) | cs[3];
        }
        Reader data = r.slice((size_t)dataLen);
        Reader inst = r.slice((size_t)instLen);
        Reader addr = r.slice((size_t)addrLen);
        if (r.fail) { res.error = "truncated window sections"; return res; }

        // Copy-source segment for this window.
        const uint8_t* segP = nullptr;
        uint64_t segN = 0;
        uint64_t targetBase = res.output.size();
        if (fromSource) {
            if (srcPos + srcLen > source.size()) { res.error = "window source out of range"; return res; }
            segP = source.data() + srcPos;
            segN = srcLen;
        } else if (fromTarget) {
            if (srcPos + srcLen > res.output.size()) { res.error = "window target out of range"; return res; }
            segN = srcLen;  // segP resolved via res.output below (may reallocate)
        }

        if (res.output.size() + targetLen > kMaxOut) { res.error = "output too large"; return res; }
        res.output.reserve(res.output.size() + (size_t)targetLen);
        AddrCache cache;
        uint64_t produced = 0;

        auto execute = [&](const Inst& in) -> bool {
            if (in.type == VCD_NOOP) return true;
            uint64_t size = in.size ? in.size : inst.varint();
            if (inst.fail || size == 0) return in.type == VCD_NOOP;
            if (produced + size > targetLen) return false;
            if (in.type == VCD_ADD) {
                size_t start = data.off;
                if (start + size > data.n) return false;
                res.output.insert(res.output.end(), data.p + start, data.p + start + size);
                data.off += (size_t)size;
            } else if (in.type == VCD_RUN) {
                uint8_t b = data.u8();
                if (data.fail) return false;
                res.output.insert(res.output.end(), (size_t)size, b);
            } else {  // VCD_COPY
                bool ok = false;
                uint64_t a = cache.decode(addr, segN + produced, in.mode, ok);
                if (!ok) return false;
                for (uint64_t k = 0; k < size; ++k) {
                    uint64_t pos = a + k;
                    uint8_t byte;
                    if (pos < segN) {
                        byte = fromSource ? segP[pos]
                                          : res.output[(size_t)(srcPos + pos)];
                    } else {  // overlapping copy inside the growing target
                        uint64_t t = pos - segN;
                        if (targetBase + t >= res.output.size()) return false;
                        byte = res.output[(size_t)(targetBase + t)];
                    }
                    res.output.push_back(byte);
                }
            }
            produced += size;
            return true;
        };

        while (inst.off < inst.n) {
            uint8_t code = inst.u8();
            if (inst.fail) { res.error = "truncated instructions"; return res; }
            const CodeEntry& e = table[code];
            if (!execute(e.i1) || !execute(e.i2)) {
                res.error = "malformed instruction stream";
                return res;
            }
        }
        if (produced != targetLen) { res.error = "window length mismatch"; return res; }
        if (hasChecksum) {
            uint32_t got = adler32(res.output.data() + targetBase, (size_t)targetLen);
            if (got != checksum) { res.error = "window checksum mismatch"; return res; }
        }
    }

    res.ok = true;
    return res;
}

bool vcdiffApplyFile(const std::string& sourcePath, const std::string& patchPath,
                     const std::string& outPath, std::string& error) {
    auto readAll = [&](const std::string& p, std::vector<uint8_t>& out) {
        std::ifstream f(p, std::ios::binary | std::ios::ate);
        if (!f) { error = "cannot open " + p; return false; }
        std::streamsize sz = f.tellg();
        if (sz < 0) { error = "cannot stat " + p; return false; }
        f.seekg(0);
        out.resize((size_t)sz);
        if (sz > 0 && !f.read(reinterpret_cast<char*>(out.data()), sz)) {
            error = "cannot read " + p;
            return false;
        }
        return true;
    };
    std::vector<uint8_t> src, pat;
    if (!readAll(sourcePath, src) || !readAll(patchPath, pat)) return false;
    VcdiffResult r = vcdiffDecode(src, pat);
    if (!r.ok) { error = r.error; return false; }
    std::ofstream o(outPath, std::ios::binary | std::ios::trunc);
    if (!o) { error = "cannot write " + outPath; return false; }
    o.write(reinterpret_cast<const char*>(r.output.data()), (std::streamsize)r.output.size());
    if (!o) { error = "write failed " + outPath; return false; }
    return true;
}

}  // namespace prismatic
