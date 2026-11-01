// src/formats/zip: the inflater on stored, fixed-code and dynamic-code streams, the archive
// reader on zips built in memory, the CRC and unsafe-name checks, and (when original/ is
// present) every entry of original/archives/Shipdisk.zip against original/shipdisk.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "doctest.h"
#include "formats/bytes.h"
#include "formats/zip.h"
#include "test_zip_builder.h"

namespace fs = std::filesystem;
using is::fmt::FormatError;

namespace {

std::vector<std::uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }

// The text whose raw deflate stream below Python's zlib (level 9, wbits -15) wrote as one
// dynamic-code block (BTYPE 2).
std::vector<std::uint8_t> sample_text() {
    std::string t;
    char line[96];
    for (int i = 0; i < 40; ++i) {
        std::snprintf(line, sizeof line, "[Group%d]\r\nINSPACE.EXE=1\r\nSHIP%02d.SHP=%d\r\n", i % 5,
                      i, i * 7);
        t += line;
    }
    return bytes(t);
}

const std::vector<std::uint8_t> kDynamic = {
    0x85, 0xd4, 0xbd, 0x6a, 0xc3, 0x40, 0x10, 0xc4, 0xf1, 0x5e, 0xa0, 0x57, 0x31, 0x37, 0xb3, 0xf7,
    0xa1, 0x2b, 0x5c, 0x84, 0x20, 0x62, 0x37, 0x41, 0xa0, 0xc6, 0x60, 0xfc, 0x0c, 0x09, 0x81, 0xbc,
    0x7f, 0xc2, 0xed, 0xaa, 0x14, 0xd3, 0xff, 0x9b, 0xe1, 0x77, 0xb7, 0xcf, 0x8f, 0x9f, 0xaf, 0xdf,
    0xef, 0xf4, 0x9a, 0xa7, 0xfb, 0xe7, 0xbe, 0xbd, 0xbd, 0xaf, 0x97, 0xf5, 0xb1, 0x5e, 0x31, 0x4f,
    0xfb, 0xed, 0xbe, 0xa5, 0x74, 0xd9, 0x6f, 0xdb, 0x35, 0xcd, 0xd3, 0x73, 0x64, 0x38, 0xc9, 0x30,
    0xb2, 0x76, 0x64, 0x3c, 0xc9, 0x38, 0x32, 0xe4, 0xa3, 0xb3, 0x93, 0xce, 0x46, 0x47, 0x1c, 0x5d,
    0x3e, 0xe9, 0xb2, 0x77, 0xcb, 0xd1, 0x9d, 0xad, 0x28, 0xa3, 0xb3, 0xa2, 0x66, 0xd4, 0xd1, 0x65,
    0xaa, 0x1d, 0xcd, 0xbb, 0xae, 0x76, 0x2c, 0xa3, 0x2b, 0x55, 0xed, 0xe8, 0xa3, 0xab, 0x26, 0x76,
    0xc0, 0x35, 0x9a, 0xe2, 0x40, 0x70, 0x28, 0x0f, 0xb8, 0xc7, 0xa2, 0x3c, 0xe0, 0x1e, 0x5d, 0x79,
    0xc0, 0x3d, 0xba, 0xf2, 0x80, 0x7b, 0x20, 0x29, 0x10, 0x38, 0x08, 0xa0, 0x44, 0xd0, 0x22, 0x54,
    0x24, 0x70, 0x12, 0x50, 0x99, 0xc0, 0x4d, 0x60, 0x0a, 0x85, 0x29, 0x1e, 0xb5, 0x52, 0x21, 0x22,
    0x54, 0x2c, 0x8c, 0x6f, 0x52, 0x94, 0x0b, 0xdd, 0x05, 0x55, 0xc1, 0x30, 0x47, 0xa8, 0x64, 0x18,
    0x32, 0x4d, 0xc9, 0x30, 0x64, 0x16, 0x25, 0xc3, 0x90, 0x59, 0x94, 0x0c, 0x43, 0xa6, 0x2b, 0x19,
    0xba, 0x0c, 0x93, 0x92, 0xb1, 0x14, 0x67, 0x44, 0xc9, 0x18, 0x22, 0x54, 0x32, 0xe6, 0x32, 0xa4,
    0x92, 0xb1, 0xb8, 0x60, 0xa6, 0x64, 0x2c, 0x4e, 0x98, 0x29, 0x19, 0x73, 0x19, 0x66, 0x25, 0x63,
    0x2e, 0xc3, 0xa2, 0x64, 0xac, 0x45, 0xa8, 0x64, 0xcc, 0x65, 0x58, 0x95, 0x8c, 0x85, 0x4c, 0xfb,
    0x97, 0xf9, 0x03};

}  // namespace

TEST_CASE("zip: CRC-32 check value") {
    const std::string s = "123456789";
    CHECK(is::fmt::crc32(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()) == 0xCBF43926u);
    const std::vector<std::uint8_t> t = sample_text();
    CHECK(t.size() == 1623);
    CHECK(is::fmt::crc32(t.data(), t.size()) == 0xddf67ae4u);
}

TEST_CASE("zip: inflate stored, fixed and dynamic blocks") {
    // A stored block: BFINAL 1, BTYPE 00, aligned LEN/NLEN, the bytes.
    const std::vector<std::uint8_t> stored = {0x01, 0x03, 0x00, 0xFC, 0xFF, 'a', 'b', 'c'};
    CHECK(is::fmt::inflate(stored.data(), stored.size()) == bytes("abc"));

    const std::vector<std::uint8_t> text = bytes("ABCABCABCABCABCABCxyz-ABCABCABCABC.");
    const std::vector<std::uint8_t> fixed = is_test::deflate_fixed(text);
    CHECK(fixed.size() < text.size());  // the matches were used
    CHECK(is::fmt::inflate(fixed.data(), fixed.size()) == text);

    CHECK(is::fmt::inflate(kDynamic.data(), kDynamic.size()) == sample_text());

    // Truncated and reserved-type streams throw.
    CHECK_THROWS_AS(is::fmt::inflate(kDynamic.data(), kDynamic.size() / 2), FormatError);
    const std::vector<std::uint8_t> reserved = {0x07};
    CHECK_THROWS_AS(is::fmt::inflate(reserved.data(), reserved.size()), FormatError);
}

TEST_CASE("zip: an archive built in memory reads back") {
    const std::vector<std::uint8_t> ini = bytes("[Vers]\r\nV=12908\r\n");
    std::vector<std::uint8_t> big;
    for (int i = 0; i < 3000; ++i) big.push_back(static_cast<std::uint8_t>("ABC"[i % 3] + (i / 999)));
    const std::vector<std::uint8_t> z = is_test::build_zip({
        {"INSTALL.INI", ini, false},
        {"SUB/", {}, false},
        {"SUB/big.bin", big, true},
        {"empty.dat", {}, false},
        {"empty2.dat", {}, true},
    });
    const is::fmt::ZipArchive a = is::fmt::ZipArchive::parse(z);
    REQUIRE(a.entries().size() == 5);
    CHECK(a.entries()[0].name == "INSTALL.INI");
    CHECK(a.entries()[0].method == 0);
    CHECK(a.extract(a.entries()[0]) == ini);
    CHECK(a.entries()[1].is_dir());
    CHECK(a.entries()[2].method == 8);
    CHECK(a.entries()[2].csize < big.size());
    CHECK(a.extract(a.entries()[2]) == big);
    CHECK(a.extract(a.entries()[3]).empty());
    CHECK(a.extract(a.entries()[4]).empty());

    // A trailing archive comment is skipped when the end record is looked for.
    std::vector<std::uint8_t> zc = z;
    zc[zc.size() - 2] = 4;
    for (char c : std::string("note")) zc.push_back(static_cast<std::uint8_t>(c));
    CHECK(is::fmt::ZipArchive::parse(zc).entries().size() == 5);

    // A flipped data byte fails the CRC.
    std::vector<std::uint8_t> bad = z;
    bad[30 + 11] ^= 0x20;  // the first entry's first data byte (header 30 + name 11)
    const is::fmt::ZipArchive b = is::fmt::ZipArchive::parse(bad);
    CHECK_THROWS_AS(b.extract(b.entries()[0]), FormatError);

    CHECK_THROWS_AS(is::fmt::ZipArchive::parse(bytes("not a zip at all, not a zip at all")), FormatError);
}

TEST_CASE("zip: entry names as relative paths") {
    using is::fmt::zip_entry_path;
    CHECK(zip_entry_path("INSTALL.INI", false) == fs::path("INSTALL.INI"));
    CHECK(zip_entry_path("a/b/c.txt", false) == fs::path("a") / "b" / "c.txt");
    CHECK(zip_entry_path("a\\b.txt", false) == fs::path("a") / "b.txt");
    CHECK(zip_entry_path("./a//b", false) == fs::path("a") / "b");
    CHECK(zip_entry_path("../evil", false).empty());
    CHECK(zip_entry_path("a/../../evil", false).empty());
    CHECK(zip_entry_path("/etc/passwd", false).empty());
    CHECK(zip_entry_path("\\x", false).empty());
    CHECK(zip_entry_path("C:/x", false).empty());
    CHECK(zip_entry_path("", false).empty());
    // NUL, control and Win32-reserved characters, components Win32 would strip, device names.
    CHECK(zip_entry_path(std::string("a\0b", 3), false).empty());
    CHECK(zip_entry_path("a\tb", false).empty());
    CHECK(zip_entry_path("a\x7f", false).empty());
    for (const char* bad : {"a<b", "a>b", "a\"b", "a|b", "a?b", "a*b", "INSTALL.INI.", "dir./x", "dir /x",
                            "...", ". .", "CON", "con", "Con.txt", "a/NUL", "AUX.", "PRN.DAT", "COM1", "lpt9.txt",
                            "COM1 .txt"})
        CHECK_MESSAGE(zip_entry_path(bad, false).empty(), bad);
    CHECK(zip_entry_path("CONFIG.SYS", false) == fs::path("CONFIG.SYS"));
    CHECK(zip_entry_path("COM0", false) == fs::path("COM0"));
    CHECK(zip_entry_path("LPT10", false) == fs::path("LPT10"));
    CHECK(zip_entry_path("READ ME.TXT", false) == fs::path("READ ME.TXT"));
    CHECK(zip_entry_path(".hidden", false) == fs::path(".hidden"));
}

TEST_CASE("zip: the output bound, ranges and header agreement") {
    std::vector<std::uint8_t> big(3000, 'A');
    const std::vector<std::uint8_t> packed = is_test::deflate_fixed(big);
    CHECK(is::fmt::inflate(packed.data(), packed.size(), 0, 3000) == big);
    CHECK_THROWS_AS(is::fmt::inflate(packed.data(), packed.size(), 0, 2999), FormatError);
    const std::vector<std::uint8_t> stored = {0x01, 0x03, 0x00, 0xFC, 0xFF, 'a', 'b', 'c'};
    CHECK_THROWS_AS(is::fmt::inflate(stored.data(), stored.size(), 0, 2), FormatError);

    const std::vector<std::uint8_t> z = is_test::build_zip({{"A.BIN", big, true}});
    const std::size_t cd = z.size() - 22 - (46 + 5);
    REQUIRE(z[cd] == 0x50);
    REQUIRE(z[cd + 3] == 2);
    // A bomb: the central directory states 10 bytes; the stream stops at the bound.
    std::vector<std::uint8_t> bomb = z;
    bomb[cd + 24] = 10, bomb[cd + 25] = 0;
    const is::fmt::ZipArchive b = is::fmt::ZipArchive::parse(bomb);
    CHECK_THROWS_AS(b.extract(b.entries()[0]), FormatError);
    // A local offset out of range is refused at the parse.
    std::vector<std::uint8_t> far = z;
    far[cd + 42] = 0xff, far[cd + 43] = 0xff;
    CHECK_THROWS_AS(is::fmt::ZipArchive::parse(far), FormatError);
    // A central directory that runs past the end record.
    std::vector<std::uint8_t> trunc = z;
    trunc[z.size() - 22 + 12] = 0xff;  // cd_size
    CHECK_THROWS_AS(is::fmt::ZipArchive::parse(trunc), FormatError);
    // The local header's method must be the central directory's.
    std::vector<std::uint8_t> m = z;
    m[8] = 0;
    const is::fmt::ZipArchive mm = is::fmt::ZipArchive::parse(m);
    CHECK_THROWS_AS(mm.extract(mm.entries()[0]), FormatError);
    // A stored entry whose sizes differ.
    std::vector<std::uint8_t> s = is_test::build_zip({{"S.TXT", {'a', 'b'}, false}});
    const std::size_t scd = s.size() - 22 - (46 + 5);
    s[scd + 24] = 1;
    const is::fmt::ZipArchive ss = is::fmt::ZipArchive::parse(s);
    CHECK_THROWS_AS(ss.extract(ss.entries()[0]), FormatError);
}

TEST_CASE("zip: golden, original/archives/Shipdisk.zip against original/shipdisk") {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v) return;
    const fs::path root = fs::path(v).parent_path().parent_path().parent_path();
    const fs::path zip = root / "original" / "archives" / "Shipdisk.zip";
    const fs::path dir = root / "original" / "shipdisk";
    std::error_code ec;
    if (!fs::is_regular_file(zip, ec) || !fs::is_directory(dir, ec)) return;
    const is::fmt::ZipArchive a = is::fmt::ZipArchive::load(zip);
    CHECK(a.entries().size() == 25);
    int compared = 0;
    for (const is::fmt::ZipEntry& e : a.entries()) {
        const std::vector<std::uint8_t> got = a.extract(e);  // throws on a CRC mismatch
        CHECK(got.size() == e.usize);
        for (const auto& f : fs::directory_iterator(dir)) {
            if (is::fmt::fold_name(f.path().filename().string()) != is::fmt::fold_name(e.name)) continue;
            CHECK_MESSAGE(got == is::fmt::read_file(f.path()), e.name);
            ++compared;
        }
    }
    CHECK(compared > 0);
    MESSAGE("Shipdisk.zip entries matched in original/shipdisk: " << compared);
}
