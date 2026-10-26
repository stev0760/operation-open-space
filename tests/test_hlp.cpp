// Unit tests of the WinHelp reader's helpers (src/formats/hlp); the golden tests against
// `isre hlp` are in test_hlp_markdown.cpp. Expected values come from re/isre/hlp.py.
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "formats/hlp.h"

using namespace is::fmt;

TEST_CASE("hlp: context hash as re/isre/hlp.py context_hash") {
    CHECK(hlp_context_hash("") == 1u);
    CHECK(hlp_context_hash("a") == 0x11u);
    CHECK(hlp_context_hash("General") == 0x9839fb1cu);
    CHECK(hlp_context_hash("general") == 0x9839fb1cu);  // case-blind
    CHECK(hlp_context_hash("idh_Contents") == 0x68310beeu);
    CHECK(hlp_context_hash("Credits") == 0x252add62u);
    CHECK(hlp_context_hash("Why_Register_Inner_Space") == 0x2b2a120eu);
}

TEST_CASE("hlp: LZ77 and RunLen") {
    const std::vector<std::uint8_t> lz = {0x04, 'a', 'b', 0x01, 0x10};
    const auto out = hlp_lz77_expand(lz.data(), lz.size());
    CHECK(std::string(out.begin(), out.end()) == "ababab");
    CHECK(hlp_lz77_expand(lz.data(), lz.size(), 3).size() == 3);
    const std::vector<std::uint8_t> bad = {0x01, 0x00, 0x00};  // reference before the start
    CHECK_THROWS_AS(hlp_lz77_expand(bad.data(), bad.size()), FormatError);

    const std::vector<std::uint8_t> rl = {0x83, 1, 2, 3, 4, 9};
    CHECK(hlp_runlen_expand(rl.data(), rl.size()) ==
          std::vector<std::uint8_t>{1, 2, 3, 9, 9, 9, 9});
}

TEST_CASE("hlp: a file that is not WinHelp throws") {
    CHECK_THROWS_AS(HlpFile(std::vector<std::uint8_t>(8, 0)), FormatError);
    std::vector<std::uint8_t> b(64, 0);
    b[0] = 0x3F, b[1] = 0x5F, b[2] = 0x03, b[3] = 0x00;  // magic, directory at 0
    b[4] = 0xF0;                                         // directory far outside the file
    CHECK_THROWS_AS(HlpFile(std::move(b)), FormatError);
}

TEST_CASE("hlp: picture to BMP") {
    HlpPicture p;
    p.ptype = 6;
    p.width = 2;
    p.height = 2;
    p.bit_count = 1;
    p.bits = {0x80, 0, 0, 0, 0x40, 0, 0, 0};
    auto bmp = p.to_bmp();
    REQUIRE(bmp.size() == 14 + 40 + 8 + 8);
    CHECK(bmp[0] == 'B');
    CHECK(bmp[10] == 62);  // pixel data offset: headers + the default 1 bpp palette
    CHECK(bmp[14 + 40 + 4] == 0xff);  // second palette entry white

    // A DDB is stored top-down with 16-bit rows; the BMP flips it and pads to 32 bits.
    p.ptype = 5;
    p.bits = {0x80, 0, 0x40, 0};
    bmp = p.to_bmp();
    REQUIRE(bmp.size() == 14 + 40 + 8 + 8);
    CHECK(bmp[62] == 0x40);  // bottom row first
    CHECK(bmp[66] == 0x80);

    p.ptype = 8;  // metafile
    CHECK(p.to_bmp().empty());
}
