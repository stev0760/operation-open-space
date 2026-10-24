#include "doctest.h"
#include "formats/bytes.h"

using namespace is::fmt;

TEST_CASE("Reader reads little-endian values and stops at the end") {
    const std::uint8_t b[] = {0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 'h', 'i', 0, 2, 'o', 'k'};
    Reader r(b, sizeof b);
    CHECK(r.u16() == 0x1234);
    CHECK(r.u32() == 0x12345678);
    CHECK(r.cstr() == "hi");
    CHECK(r.pstr() == "ok");
    CHECK(r.remaining() == 0);
    CHECK_THROWS_AS(r.u8(), FormatError);
    CHECK_THROWS_AS(r.seek(13), FormatError);
}

TEST_CASE("cp1252 maps the 0x80-0x9F range and passes undefined bytes through") {
    CHECK(cp1252_to_utf16("A\x80\x99\xE9") == u"A€™é");
    CHECK(cp1252_to_utf16("\x81\x8D") == u"\u0081\u008D");
}
