// Resource records loaded as data (docs/re/formats.md#resource-records-loaded-as-data):
// synthetic cases, then golden tests against ISPACE.DLL and IGRAPHIC.DLL and the raw .bin
// files `isre res` writes. The golden tests skip when IS_ORIGINAL_DIR or IS_ASSETS_DIR is
// unset or missing.
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

#include "doctest.h"
#include "formats/records.h"
#include "ne_builder.h"

using namespace is::fmt;
namespace fs = std::filesystem;

namespace {
void p16(std::vector<std::uint8_t>& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
}  // namespace

TEST_CASE("#5500 100: 92 RGB entries from the first 0x114 bytes") {
    std::vector<std::uint8_t> b;
    for (int i = 0; i < 96; ++i) {
        b.push_back(static_cast<std::uint8_t>(i));
        b.push_back(static_cast<std::uint8_t>(0x80 + i));
        b.push_back(static_cast<std::uint8_t>(0xFF - i));
    }
    Palette5500 p = parse_palette5500(b.data(), b.size());
    CHECK(p.colours.size() == 92);
    CHECK(Palette5500::kFirstIndex == 11);
    CHECK(p.colours[0].r == 0);
    CHECK(p.colours[0].g == 0x80);
    CHECK(p.colours[0].b == 0xFF);
    CHECK(p.colours[91].r == 91);
    CHECK(p.colours[91].b == 0xFF - 91);
    b.resize(0x113);
    CHECK_THROWS_AS(parse_palette5500(b.data(), b.size()), FormatError);
}

TEST_CASE("#5600 135: 64 signed words, row by row") {
    std::vector<std::uint8_t> b;
    for (int i = 0; i < 64; ++i) p16(b, static_cast<unsigned>(i % 2 ? -i : i) & 0xFFFF);
    Table5600 t = parse_table5600(b.data(), b.size());
    CHECK(t.rows[0][0] == 0);
    CHECK(t.rows[0][1] == -1);
    CHECK(t.rows[1][0] == 8);
    CHECK(t.rows[7][7] == -63);
    b.pop_back();
    CHECK_THROWS_AS(parse_table5600(b.data(), b.size()), FormatError);
}

TEST_CASE("global_size rounds up to a power of two of at most 64 KiB") {
    CHECK(global_size(0x6c) == 0x80);
    CHECK(global_size(0x6c, 16) == 0x70);
    CHECK(global_size(0x80) == 0x80);
    CHECK(global_size(0) == 0);
    CHECK(global_size(928, 64) == 960);
    CHECK(global_size(1, 0x10000) == 0x10000);
    CHECK_THROWS_AS(global_size(1, 0), FormatError);
    CHECK_THROWS_AS(global_size(1, 24), FormatError);
    CHECK_THROWS_AS(global_size(1, 0x20000), FormatError);
    CHECK_THROWS_AS(global_size(static_cast<std::size_t>(-1), 32), FormatError);
}

TEST_CASE("#6600: header, body from GlobalSize of the header block, 0x26-byte records") {
    std::vector<std::uint8_t> b(0x80 + 3 * 0x26 + 5, 0);
    b[0] = 0xFF;
    b[0x50] = 6;
    b[0x51] = 8;
    b[0x5c] = 0xF4; b[0x5d] = 0x01;                                  // 500
    b[0x60] = 0xFE; b[0x61] = 0xFF; b[0x62] = 0xFF; b[0x63] = 0xFF;  // -2
    b[0x70] = 0xAA;  // inside the body with 16-byte rounding only
    b[0x80] = 1;
    b[0x80 + 0x26] = 2;
    b[0x80 + 2 * 0x26] = 3;
    b[0x80 + 2 * 0x26 + 0x1f] = 0xEC;

    NeModule m(build_ne("INGRAPHIC", {{Scene6600::kType, 105, "", b}}));
    Scene6600 s = load_scene6600(m, 105);
    CHECK(s.header[0] == 0xFF);
    CHECK(s.byte_50 == 6);
    CHECK(s.byte_51 == 8);
    CHECK(s.dword_5c == 500);
    CHECK(s.dword_60 == -2);
    CHECK(s.body_offset == 0x80);
    // build_ne pads the resource to 16 bytes, as the NE table does: 256 bytes, so the
    // body is 128 bytes, already a multiple of 32
    CHECK(s.body_length == m.size(*m.find(Scene6600::kType, 105)) - 0x80);
    CHECK(s.body.size() == 128);
    REQUIRE(s.records.size() == s.body.size() / 0x26);
    CHECK(s.records.size() == 3);
    CHECK(s.records[0][0] == 1);
    CHECK(s.records[1][0] == 2);
    CHECK(s.records[2][0] == 3);
    CHECK(s.records[2][0x1f] == 0xEC);
    CHECK(s.body[0x6b] == 0xEC);

    Scene6600 s16 = parse_scene6600(b.data(), b.size(), 16);
    CHECK(s16.body_offset == 0x70);
    CHECK(s16.body_length == b.size() - 0x70);
    CHECK(s16.body.size() == 144);
    CHECK(s16.records.size() == 3);
    CHECK(s16.records[0][0] == 0xAA);

    // The body block is GlobalSize(n - body_offset) long and zero-filled, so its rounding
    // tail can complete a record: 2 records and 30 bytes copied, 128 bytes in the block,
    // 3 records whose last one ends in 8 zeros. The 0xEC at record byte 0x1f is cut off.
    const std::vector<std::uint8_t> t(b.begin(), b.begin() + 0x80 + 2 * 0x26 + 30);
    Scene6600 st = parse_scene6600(t.data(), t.size());
    CHECK(st.body_length == 2 * 0x26 + 30);
    REQUIRE(st.body.size() == 128);
    CHECK(std::all_of(st.body.begin() + 2 * 0x26 + 30, st.body.end(), [](std::uint8_t c) { return c == 0; }));
    REQUIRE(st.records.size() == 3);
    CHECK(st.records[1][0] == 2);
    CHECK(st.records[2][0] == 3);
    CHECK(st.records[2][0x1f] == 0);

    CHECK_THROWS_AS(parse_scene6600(b.data(), b.size(), 24), FormatError);
    CHECK_THROWS_AS(parse_scene6600(b.data(), b.size(), 0x20000), FormatError);
    CHECK_THROWS_AS(parse_scene6600(b.data(), 0x6c, 32), FormatError);
    CHECK_THROWS_AS(load_scene6600(m, 100), FormatError);
}

TEST_CASE("#5000 150: raw bytes and the length before the zero padding") {
    const std::uint8_t b[] = {1, 0, 3, 4, 0, 0, 0, 0};
    Bytes5000 x = parse_bytes5000(b, sizeof b);
    CHECK(x.bytes.size() == 8);
    CHECK(x.length == 4);
    NeModule m(build_ne("ISPACE", {{Table5600::kType, 135, "", std::vector<std::uint8_t>(128, 0)}}));
    CHECK_THROWS_AS(load_bytes5000(m), FormatError);
    CHECK_THROWS_AS(load_palette5500(m), FormatError);
    CHECK(load_table5600(m).rows[3][3] == 0);
}

// ---- golden

namespace {
fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v || !fs::is_directory(v)) return {};
    return fs::path(v);
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// The distribution mixes cases (ISPACE.dll); look names up ignoring case.
fs::path find_ci(const fs::path& dir, const std::string& name) {
    if (dir.empty() || !fs::is_directory(dir)) return {};
    for (const auto& e : fs::directory_iterator(dir))
        if (upper(e.path().filename().string()) == upper(name)) return e.path();
    return {};
}

std::vector<std::uint8_t> asset(const fs::path& assets, const char* rel) {
    return assets.empty() ? std::vector<std::uint8_t>() : read_file(assets / rel);
}
}  // namespace

TEST_CASE("ISPACE.DLL: #5500 100, #5600 135 and #5000 150") {
    const fs::path dir = env_dir("IS_ORIGINAL_DIR");
    const fs::path assets = env_dir("IS_ASSETS_DIR");
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    if (assets.empty()) MESSAGE("skipped: IS_ASSETS_DIR not available for the .bin comparison");
    const fs::path f = find_ci(dir, "ISPACE.DLL");
    REQUIRE(!f.empty());
    NeModule m = NeModule::load(f);
    CHECK(m.module_name() == "ISPACE");

    // #5500 100: 288 bytes; 92 entries used, the 12 bytes after them zero; the first
    // ramp runs from cyan (docs/re/resources.md#other-data-types)
    const NeResource* pr = m.find(Palette5500::kType, Palette5500::kId);
    REQUIRE(pr);
    CHECK(m.size(*pr) == 288);
    CHECK(std::all_of(m.data(*pr) + 0x114, m.data(*pr) + 288, [](std::uint8_t c) { return c == 0; }));
    const Palette5500 pal = load_palette5500(m);
    CHECK(pal.colours[0].r == 0x00);
    CHECK(pal.colours[0].g == 0xFF);
    CHECK(pal.colours[0].b == 0xFF);
    const auto pal_ref = asset(assets, "ispace.dll/5500/100.bin");
    if (!pal_ref.empty()) {
        REQUIRE(pal_ref.size() == 288);
        for (std::size_t k = 0; k < Palette5500::kCount; ++k) {
            CHECK(pal.colours[k].r == pal_ref[3 * k]);
            CHECK(pal.colours[k].g == pal_ref[3 * k + 1]);
            CHECK(pal.colours[k].b == pal_ref[3 * k + 2]);
        }
    }

    // #5600 135: 128 bytes, 64 signed words within +-250
    const NeResource* tr = m.find(Table5600::kType, Table5600::kId);
    REQUIRE(tr);
    CHECK(m.size(*tr) == 128);
    const Table5600 t = load_table5600(m);
    int peak = 0;
    for (const auto& row : t.rows)
        for (std::int16_t w : row) peak = std::max(peak, w < 0 ? -w : static_cast<int>(w));
    CHECK(peak == 250);
    const std::int16_t row0[8] = {250, 100, 200, -200, 150, -200, -200, 0};
    CHECK(std::equal(t.rows[0].begin(), t.rows[0].end(), row0));
    const auto t_ref = asset(assets, "ispace.dll/5600/135.bin");
    if (!t_ref.empty()) {
        REQUIRE(t_ref.size() == 128);
        for (std::size_t k = 0; k < 64; ++k)
            CHECK(t.rows[k / 8][k % 8] == static_cast<std::int16_t>(t_ref[2 * k] | t_ref[2 * k + 1] << 8));
    }

    // #5000 150: 88 distinct non-zero bytes, then zero padding
    const Bytes5000 x = load_bytes5000(m);
    CHECK(x.bytes.size() == 96);
    CHECK(x.length == 88);
    const std::set<std::uint8_t> distinct(x.bytes.begin(), x.bytes.begin() + 88);
    CHECK(distinct.size() == 88);
    CHECK(distinct.count(0) == 0);
    const auto x_ref = asset(assets, "ispace.dll/5000/150.bin");
    if (!x_ref.empty()) CHECK(x.bytes == x_ref);
}

TEST_CASE("IGRAPHIC.DLL: the five #6600 records") {
    const fs::path dir = env_dir("IS_ORIGINAL_DIR");
    const fs::path assets = env_dir("IS_ASSETS_DIR");
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    if (assets.empty()) MESSAGE("skipped: IS_ASSETS_DIR not available for the .bin comparison");
    const fs::path f = find_ci(dir, "IGRAPHIC.DLL");
    REQUIRE(!f.empty());
    NeModule m = NeModule::load(f);
    CHECK(m.all(Scene6600::kType).size() == 5);
    // sizes: docs/re/resources.md#other-data-types; record counts follow from them,
    // GlobalSize(size - 0x80) / 0x26 under 32- and 64-byte rounding
    const std::size_t sizes[5] = {2496, 3520, 2432, 1056, 1088};
    const std::size_t counts[5] = {62, 89, 60, 24, 25};
    const std::size_t counts64[5] = {62, 89, 60, 25, 25};
    // header dwords +0x60 (+0x50, +0x51 and +0x5c are the same in all five)
    const std::int32_t dword_60[5] = {217, 201, 263, 632, 881};
    for (int i = 0; i < 5; ++i) {
        const std::uint16_t id = Scene6600::kIds[i];
        INFO("#6600 " << id);
        const NeResource* r = m.find(Scene6600::kType, id);
        REQUIRE(r);
        CHECK(m.size(*r) == sizes[i]);
        const Scene6600 s = load_scene6600(m, id);
        const std::uint8_t sig[6] = {0xFF, 0, 0xFF, 0, 0xFF, 0};
        CHECK(std::equal(sig, sig + 6, s.header.begin()));
        CHECK(s.byte_50 == 6);
        CHECK(s.byte_51 == 7);
        CHECK(s.dword_5c == 500);
        CHECK(s.dword_60 == dword_60[i]);
        // bytes 0x6c-0x7f, between the header and the body, are zero
        CHECK(std::all_of(m.data(*r) + 0x6c, m.data(*r) + 0x80, [](std::uint8_t c) { return c == 0; }));
        CHECK(s.body_offset == 0x80);
        CHECK(s.body_length == sizes[i] - 0x80);
        CHECK(s.body.size() == s.body_length);  // whole multiples of 32: no zero tail
        CHECK(s.records.size() == counts[i]);
        const Scene6600 s64 = load_scene6600(m, id, 64);
        CHECK(s64.body_offset == 0x80);
        CHECK(s64.records.size() == counts64[i]);
        const auto ref = asset(assets, ("igraphic.dll/6600/" + std::to_string(id) + ".bin").c_str());
        if (!ref.empty()) {
            REQUIRE(ref.size() == sizes[i]);
            CHECK(std::equal(s.header.begin(), s.header.end(), ref.begin()));
            CHECK(std::equal(s.body.begin(), s.body.end(), ref.begin() + 0x80));
        }
    }
}
