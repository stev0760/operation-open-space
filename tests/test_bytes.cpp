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

TEST_CASE("UTF-16 to UTF-8 replaces unpaired surrogates and never throws") {
    CHECK(utf8_from_utf16(u"MASTER.SHP") == "MASTER.SHP");
    CHECK(utf8_from_utf16(u"tw-é €") == "tw-\xC3\xA9 \xE2\x82\xAC");
    CHECK(utf8_from_utf16(u"\U0001F600") == "\xF0\x9F\x98\x80");
    CHECK(utf8_from_utf16(std::u16string(1, char16_t(0xD800)) + u"a") == "\xEF\xBF\xBD" "a");
    CHECK(utf8_from_utf16(std::u16string(1, char16_t(0xDC00))) == "\xEF\xBF\xBD");
    CHECK(utf8_from_utf16(std::u16string{char16_t(0xDC00), char16_t(0xD800)}) == "\xEF\xBF\xBD\xEF\xBF\xBD");
}

TEST_CASE("path_utf8 gives a message path without throwing") {
    CHECK(path_utf8(std::filesystem::path(u"tw-é")) == "tw-\xC3\xA9");
#ifdef _WIN32
    // NTFS allows an unpaired surrogate in a name; libstdc++'s path::u8string throws for it
    const std::filesystem::path odd(std::wstring(L"A") + wchar_t(0xD800) + L".DLL");
    CHECK(path_utf8(odd) == "A\xEF\xBF\xBD.DLL");
    // read_file reports it as a FormatError, not as a filesystem_error from the message
    CHECK_THROWS_AS(read_file(std::filesystem::path(L"no-such-dir") / odd), FormatError);
#else
    // native bytes stay as they are, UTF-8 or not
    const std::filesystem::path odd(std::string("A\xFF.DLL"));
    CHECK(path_utf8(odd) == "A\xFF.DLL");
    CHECK_THROWS_AS(read_file(std::filesystem::path("no-such-dir") / odd), FormatError);
#endif
}

TEST_CASE("fold_name: one case fold for code points, cp1252 bytes and UTF-16") {
    CHECK(fold_name(std::string("eng_lang.Dll")) == "ENG_LANG.DLL");
    CHECK(fold_name(std::string("\xE9" "cran")) == "\xC9" "CRAN");
    CHECK(fold_name(std::string("\x9A\x9C\x9E\xFF")) == "\x8A\x8C\x8E\x9F");
    CHECK(fold_name(std::string("\xDF\xB5\xF7\xD7")) == "\xDF\xB5\xF7\xD7");  // no partner, no letter
    CHECK(fold_name(u"écranšł") == u"ÉCRANŠł");
    CHECK(fold_name(std::u16string(1, char16_t(0xD800))) == std::u16string(1, char16_t(0xD800)));
    // the byte rule is the code point rule over cp1252
    for (int b = 0; b < 0x100; ++b) {
        CAPTURE(b);
        const std::string one(1, static_cast<char>(b));
        const char16_t c = cp1252_to_utf16(one)[0];
        CHECK(cp1252_to_utf16(fold_name(one)) ==
              std::u16string(1, static_cast<char16_t>(fold_name(char32_t{c}))));
        CHECK(fold_name(cp1252_to_utf16(one)) == cp1252_to_utf16(fold_name(one)));
    }
}
