#include "doctest.h"
#include "formats/bytes.h"
#include "formats/dib.h"

using namespace is::fmt;

namespace {
void p16(std::vector<std::uint8_t>& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void p32(std::vector<std::uint8_t>& b, unsigned v) {
    p16(b, v & 0xFFFF);
    p16(b, v >> 16);
}
// 3x2, 4 bpp, bottom-up, two palette entries used.
std::vector<std::uint8_t> packed_dib() {
    std::vector<std::uint8_t> b;
    p32(b, 40); p32(b, 3); p32(b, 2); p16(b, 1); p16(b, 4); p32(b, 0);
    p32(b, 0); p32(b, 0); p32(b, 0); p32(b, 16); p32(b, 0);
    for (int i = 0; i < 16; ++i) {
        const unsigned rgb = i == 1 ? 0xFF0000 : i == 15 ? 0x000000 : 0x0000FF;
        b.push_back(rgb & 0xFF); b.push_back((rgb >> 8) & 0xFF); b.push_back(rgb >> 16); b.push_back(0);
    }
    // bottom row first: indices 15 1 15 / top row: 1 15 1
    const std::uint8_t rows[] = {0xF1, 0xF0, 0, 0, 0x1F, 0x10, 0, 0};
    b.insert(b.end(), rows, rows + 8);
    return b;
}
}  // namespace

TEST_CASE("packed 4 bpp DIB: palette, bottom-up rows and transparency") {
    auto b = packed_dib();
    Dib d = parse_dib(b.data(), b.size());
    CHECK(d.width == 3);
    CHECK(d.height == 2);
    CHECK(d.palette.size() == 16);
    CHECK(d.index_at(0, 0) == 1);
    CHECK(d.index_at(1, 0) == 15);
    CHECK(d.index_at(0, 1) == 15);
    auto px = to_bgra(d, 15, 0x112233);
    CHECK(px[0] == 0xFFFF0000);
    CHECK(px[1] == 0x00112233);
    CHECK(px[3] == 0x00112233);
}

TEST_CASE("file-format DIB with BITMAPFILEHEADER") {
    auto body = packed_dib();
    std::vector<std::uint8_t> f = {'B', 'M'};
    p32(f, static_cast<unsigned>(14 + body.size()));
    p32(f, 0);
    p32(f, 14 + 40 + 64);
    f.insert(f.end(), body.begin(), body.end());
    Dib d = parse_dib(f.data(), f.size());
    CHECK(d.index_at(2, 0) == 1);
    CHECK(d.rgb_at(0, 0) == 0xFF0000);
}

TEST_CASE("bad DIBs are rejected") {
    auto b = packed_dib();
    b.resize(b.size() - 3);
    CHECK_THROWS_AS(parse_dib(b.data(), b.size()), FormatError);
    auto c = packed_dib();
    c[16] = 1;  // BI_RLE8
    CHECK_THROWS_AS(parse_dib(c.data(), c.size()), FormatError);
}
