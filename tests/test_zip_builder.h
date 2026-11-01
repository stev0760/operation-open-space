// Builds small ZIP archives in memory for the tests of src/formats/zip and the install step's
// zip sets: stored entries, and deflate entries written with the fixed codes (RFC 1951 3.2.6)
// by a literal-and-repeat encoder of this file's own.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "formats/zip.h"

namespace is_test {

class BitWriter {
public:
    void bits(std::uint32_t v, int n) {
        for (int i = 0; i < n; ++i) put((v >> i) & 1);
    }
    // A Huffman code, most significant bit first (RFC 1951 3.1.1).
    void code(std::uint32_t c, int n) {
        for (int i = n - 1; i >= 0; --i) put((c >> i) & 1);
    }
    std::vector<std::uint8_t> done() {
        if (nbit_) out_.push_back(cur_);
        nbit_ = 0;
        cur_ = 0;
        return out_;
    }

private:
    void put(std::uint32_t b) {
        cur_ = static_cast<std::uint8_t>(cur_ | (b << nbit_));
        if (++nbit_ == 8) {
            out_.push_back(cur_);
            cur_ = 0;
            nbit_ = 0;
        }
    }
    std::vector<std::uint8_t> out_;
    std::uint8_t cur_ = 0;
    int nbit_ = 0;
};

inline void fixed_literal(BitWriter& w, int v) {
    if (v < 144) w.code(0x30 + v, 8);
    else if (v < 256) w.code(0x190 + (v - 144), 9);
    else if (v < 280) w.code(v - 256, 7);
    else w.code(0xC0 + (v - 280), 8);
}

// One fixed-code block: a byte that repeats the byte 3 back over a run of at least 9 is sent
// as a (length 9, distance 3) match; everything else as literals.
inline std::vector<std::uint8_t> deflate_fixed(const std::vector<std::uint8_t>& in) {
    BitWriter w;
    w.bits(1, 1);  // BFINAL
    w.bits(1, 2);  // BTYPE 01
    std::size_t i = 0;
    while (i < in.size()) {
        bool match = i >= 3 && i + 9 <= in.size();
        for (std::size_t k = 0; match && k < 9; ++k) match = in[i + k] == in[i + k - 3];
        if (match) {
            fixed_literal(w, 263);  // length 9: base 9, no extra bits
            w.code(2, 5);           // distance 3: code 2, no extra bits
            i += 9;
        } else {
            fixed_literal(w, in[i++]);
        }
    }
    fixed_literal(w, 256);
    return w.done();
}

struct ZipItem {
    std::string name;
    std::vector<std::uint8_t> data;
    bool deflate = false;
};

inline void le16(std::vector<std::uint8_t>& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
inline void le32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    le16(b, v & 0xFFFF);
    le16(b, v >> 16);
}

inline std::vector<std::uint8_t> build_zip(const std::vector<ZipItem>& items) {
    std::vector<std::uint8_t> z, cd;
    for (const ZipItem& it : items) {
        const std::vector<std::uint8_t> packed = it.deflate ? deflate_fixed(it.data) : it.data;
        const std::uint32_t crc = is::fmt::crc32(it.data.data(), it.data.size());
        const std::uint32_t off = static_cast<std::uint32_t>(z.size());
        const std::uint16_t method = it.deflate ? 8 : 0;
        le32(z, 0x04034b50);
        le16(z, 20);
        le16(z, 0);
        le16(z, method);
        le16(z, 0x6000);  // 12:00
        le16(z, 0x2862);  // 2000-03-02
        le32(z, crc);
        le32(z, static_cast<std::uint32_t>(packed.size()));
        le32(z, static_cast<std::uint32_t>(it.data.size()));
        le16(z, static_cast<std::uint32_t>(it.name.size()));
        le16(z, 0);
        z.insert(z.end(), it.name.begin(), it.name.end());
        z.insert(z.end(), packed.begin(), packed.end());

        le32(cd, 0x02014b50);
        le16(cd, 20);
        le16(cd, 20);
        le16(cd, 0);
        le16(cd, method);
        le16(cd, 0x6000);
        le16(cd, 0x2862);
        le32(cd, crc);
        le32(cd, static_cast<std::uint32_t>(packed.size()));
        le32(cd, static_cast<std::uint32_t>(it.data.size()));
        le16(cd, static_cast<std::uint32_t>(it.name.size()));
        le16(cd, 0);
        le16(cd, 0);
        le16(cd, 0);
        le16(cd, 0);
        le32(cd, 0);
        le32(cd, off);
        cd.insert(cd.end(), it.name.begin(), it.name.end());
    }
    const std::uint32_t cd_off = static_cast<std::uint32_t>(z.size());
    z.insert(z.end(), cd.begin(), cd.end());
    le32(z, 0x06054b50);
    le16(z, 0);
    le16(z, 0);
    le16(z, static_cast<std::uint32_t>(items.size()));
    le16(z, static_cast<std::uint32_t>(items.size()));
    le32(z, static_cast<std::uint32_t>(cd.size()));
    le32(z, cd_off);
    le16(z, 0);
    return z;
}

}  // namespace is_test
