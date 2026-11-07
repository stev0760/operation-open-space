// Icon groups and images: synthetic cases, then golden tests against the original modules
// and the .ico files `isre res` writes. The golden tests skip when IS_ORIGINAL_DIR or
// IS_ASSETS_DIR is unset or missing.
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "doctest.h"
#include "formats/icon.h"
#include "ne_builder.h"

using namespace is::fmt;
namespace fs = std::filesystem;

namespace {
void p16(std::vector<std::uint8_t>& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void p32(std::vector<std::uint8_t>& b, unsigned v) {
    p16(b, v & 0xFFFF);
    p16(b, v >> 16);
}
std::uint32_t r32(const std::vector<std::uint8_t>& b, std::size_t at) {
    return b[at] | b[at + 1] << 8 | b[at + 2] << 16 | std::uint32_t{b[at + 3]} << 24;
}

// 8x2, 4 bpp. Top row: indices 1 2 1 2 0 0 0 0, AND 0 0 0 0 1 1 1 1.
// Bottom row: index 2 everywhere, AND set only at x = 7 (a non-black pixel under the mask).
// 40 + 64 + 8 + 8 = 120 bytes.
std::vector<std::uint8_t> icon_8x2() {
    std::vector<std::uint8_t> b;
    p32(b, 40); p32(b, 8); p32(b, 4); p16(b, 1); p16(b, 4); p32(b, 0);
    p32(b, 0); p32(b, 0); p32(b, 0); p32(b, 0); p32(b, 0);
    for (int i = 0; i < 16; ++i) {
        const unsigned rgb = i == 1 ? 0xFF0000 : i == 2 ? 0x00FF00 : 0;
        b.push_back(rgb & 0xFF); b.push_back((rgb >> 8) & 0xFF); b.push_back(rgb >> 16); b.push_back(0);
    }
    const std::uint8_t xor_rows[] = {0x22, 0x22, 0x22, 0x22, 0x12, 0x12, 0x00, 0x00};  // bottom row first
    const std::uint8_t and_rows[] = {0x01, 0, 0, 0, 0x0F, 0, 0, 0};
    b.insert(b.end(), xor_rows, xor_rows + 8);
    b.insert(b.end(), and_rows, and_rows + 8);
    return b;
}

// 32x1, 1 bpp: 40 + 8 + 4 + 4 = 56 bytes.
std::vector<std::uint8_t> icon_32x1() {
    std::vector<std::uint8_t> b;
    p32(b, 40); p32(b, 32); p32(b, 2); p16(b, 1); p16(b, 1); p32(b, 0);
    p32(b, 0); p32(b, 0); p32(b, 0); p32(b, 2); p32(b, 0);
    p32(b, 0x000000); p32(b, 0xFFFFFF);
    p32(b, 0xFFFFFFFF);
    p32(b, 0x0000FFFF);
    return b;
}

std::vector<std::uint8_t> group(const std::vector<IconDirEntry>& es) {
    std::vector<std::uint8_t> b;
    p16(b, 0); p16(b, 1); p16(b, static_cast<unsigned>(es.size()));
    for (const auto& e : es) {
        b.push_back(e.width); b.push_back(e.height); b.push_back(e.colors); b.push_back(e.reserved);
        p16(b, e.planes); p16(b, e.bpp); p32(b, e.bytes); p16(b, e.id);
    }
    return b;
}

IconDirEntry entry(std::uint8_t w, std::uint8_t h, std::uint8_t colors, std::uint16_t bpp,
                   std::uint32_t bytes, std::uint16_t id) {
    IconDirEntry e;
    e.width = w; e.height = h; e.colors = colors; e.planes = 1; e.bpp = bpp; e.bytes = bytes; e.id = id;
    return e;
}
}  // namespace

TEST_CASE("an icon group lists its entries") {
    auto g = group({entry(8, 2, 16, 4, 120, 1), entry(32, 1, 2, 1, 56, 7)});
    g.push_back(0xEE);  // alignment padding
    auto es = parse_group_icon(g.data(), g.size());
    REQUIRE(es.size() == 2);
    CHECK(es[0].width == 8);
    CHECK(es[0].height == 2);
    CHECK(es[0].colors == 16);
    CHECK(es[0].planes == 1);
    CHECK(es[0].bpp == 4);
    CHECK(es[0].bytes == 120);
    CHECK(es[0].id == 1);
    CHECK(es[1].id == 7);
}

TEST_CASE("an icon image: XOR image, AND mask and exact size") {
    auto b = icon_8x2();
    b.insert(b.end(), 8, 0xEE);  // alignment padding
    Icon ic = parse_icon(b.data(), b.size());
    CHECK(ic.size == 120);
    CHECK(ic.image.width == 8);
    CHECK(ic.image.height == 2);
    CHECK(ic.image.bpp == 4);
    CHECK(ic.image.palette.size() == 16);
    CHECK(ic.image.index_at(0, 0) == 1);
    CHECK(ic.image.index_at(1, 0) == 2);
    CHECK(ic.image.index_at(0, 1) == 2);
    CHECK_FALSE(ic.masked(3, 0));
    CHECK(ic.masked(4, 0));
    CHECK_FALSE(ic.masked(6, 1));
    CHECK(ic.masked(7, 1));

    auto px = icon_to_bgra(ic);
    REQUIRE(px.size() == 16);
    CHECK(px[0] == 0xFFFF0000);
    CHECK(px[1] == 0xFF00FF00);
    CHECK(px[4] == 0x00000000);   // transparent
    CHECK(px[8] == 0xFF00FF00);
    CHECK(px[15] == 0x0000FF00);  // under the mask, XOR colour kept with alpha 0
}

TEST_CASE("a .ico file from a group: ICONDIR, entries with offsets, images cut to size") {
    const auto a = icon_8x2(), c = icon_32x1();
    // the group's stored sizes are copied neither way: the .ico takes the exact sizes
    NeModule m(build_ne("TEST", {{RT16_ICON, 1, "", a},
                                 {RT16_ICON, 7, "", c},
                                 {RT16_GROUP_ICON, 0, "GAME",
                                  group({entry(8, 2, 16, 4, 999, 1), entry(32, 1, 2, 1, 56, 7)})}}));
    const NeResource* g = m.find(RT16_GROUP_ICON, "GAME");
    REQUIRE(g);
    auto ico = build_ico(m, *g);
    REQUIRE(ico.size() == 6 + 32 + 120 + 56);
    CHECK(ico[0] == 0);
    CHECK(ico[2] == 1);
    CHECK(ico[4] == 2);
    // entry 0
    CHECK(ico[6] == 8);
    CHECK(ico[7] == 2);
    CHECK(ico[8] == 16);
    CHECK(ico[9] == 0);
    CHECK(ico[10] == 1);
    CHECK(ico[12] == 4);
    CHECK(r32(ico, 14) == 120);
    CHECK(r32(ico, 18) == 38);
    // entry 1
    CHECK(ico[22] == 32);
    CHECK(ico[24] == 2);
    CHECK(ico[28] == 1);
    CHECK(r32(ico, 30) == 56);
    CHECK(r32(ico, 34) == 158);
    CHECK(std::equal(a.begin(), a.end(), ico.begin() + 38));
    CHECK(std::equal(c.begin(), c.end(), ico.begin() + 158));
}

TEST_CASE("bad icon groups and images are rejected") {
    auto g = group({entry(8, 2, 16, 4, 120, 1)});
    auto bad = g;
    bad[0] = 1;  // reserved
    CHECK_THROWS_AS(parse_group_icon(bad.data(), bad.size()), FormatError);
    bad = g;
    bad[2] = 2;  // a cursor directory
    CHECK_THROWS_AS(parse_group_icon(bad.data(), bad.size()), FormatError);
    bad = group({});
    CHECK_THROWS_AS(parse_group_icon(bad.data(), bad.size()), FormatError);
    bad = g;
    bad.resize(bad.size() - 1);
    CHECK_THROWS_AS(parse_group_icon(bad.data(), bad.size()), FormatError);

    auto ic = icon_8x2();
    ic.resize(ic.size() - 1);  // AND mask truncated
    CHECK_THROWS_AS(parse_icon(ic.data(), ic.size()), FormatError);
    ic = icon_8x2();
    ic[8] = 3;  // odd header height
    CHECK_THROWS_AS(parse_icon(ic.data(), ic.size()), FormatError);

    NeModule m(build_ne("TEST", {{RT16_GROUP_ICON, 5, "", g}}));
    CHECK_THROWS_AS(build_ico(m, *m.find(RT16_GROUP_ICON, 5)), FormatError);
}

namespace {
// A file-format DIB: BITMAPFILEHEADER, BITMAPINFOHEADER with `bi_size`, 8x8 at 1 bpp,
// 2 palette entries, 8 rows of 4 bytes. 14 + 40 + 8 + 32 = 94 bytes.
std::vector<std::uint8_t> bm_8x8(std::uint32_t bi_size, std::uint32_t height = 8) {
    std::vector<std::uint8_t> b = {'B', 'M'};
    p32(b, 94); p32(b, 0); p32(b, 14 + 40 + 8);
    p32(b, bi_size); p32(b, 8); p32(b, height); p16(b, 1); p16(b, 1); p32(b, 0);
    p32(b, 0); p32(b, 0); p32(b, 0); p32(b, 2); p32(b, 0);
    p32(b, 0x000000); p32(b, 0xFFFFFF);
    b.insert(b.end(), 32, 0x55);
    return b;
}
}  // namespace

TEST_CASE("DIB headers with out-of-range heights or header sizes are rejected") {
    // the image parser icons share with parse_dib (formats/dib.cpp)
    auto bm = bm_8x8(40);
    Dib d = parse_dib(bm.data(), bm.size());
    CHECK(d.width == 8);
    CHECK(d.height == 8);
    CHECK(d.palette.size() == 2);
    CHECK(d.palette[1] == 0xFFFFFF);

    // biHeight INT32_MIN has no positive counterpart
    bm = bm_8x8(40, 0x80000000u);
    CHECK_THROWS_WITH_AS(parse_dib(bm.data(), bm.size()), "bad DIB size", FormatError);
    auto ic = icon_8x2();
    ic[8] = 0; ic[9] = 0; ic[10] = 0; ic[11] = 0x80;
    CHECK_THROWS_WITH_AS(parse_icon(ic.data(), ic.size()), "bad DIB size", FormatError);

    // biSize 0xFFFFFFF2 after the 14-byte file header: 14 + biSize wraps a 32-bit size_t
    // to 0, which would read the palette from the "BM" bytes
    bm = bm_8x8(0xFFFFFFF2u);
    CHECK_THROWS_WITH_AS(parse_dib(bm.data(), bm.size()), "DIB header past end", FormatError);
    bm = bm_8x8(95 - 14);  // one byte past the end
    CHECK_THROWS_WITH_AS(parse_dib(bm.data(), bm.size()), "DIB header past end", FormatError);
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
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// The distribution mixes cases (ISPACE.dll, eng_fx.dll); look names up ignoring case.
fs::path find_ci(const fs::path& dir, const std::string& name) {
    if (dir.empty() || !fs::is_directory(dir)) return {};
    for (const auto& e : fs::directory_iterator(dir))
        if (upper(e.path().filename().string()) == upper(name)) return e.path();
    return {};
}

std::string group_key(const NeResource& r) { return r.name.empty() ? std::to_string(r.id) : r.name; }

// Every group of `file` decodes, matches its entries and builds isre's .ico. Returns the
// number of groups checked.
std::size_t check_groups(const fs::path& dir, const std::string& file, const fs::path& assets,
                         const std::string& asset_dir) {
    const fs::path f = find_ci(dir, file);
    INFO("module " << file);
    REQUIRE(!f.empty());
    NeModule m = NeModule::load(f);
    const auto groups = m.all(RT16_GROUP_ICON);
    for (const NeResource* g : groups) {
        INFO(file << " GROUP_ICON " << group_key(*g));
        const auto es = parse_group_icon(m.data(*g), m.size(*g));
        CHECK(!es.empty());
        for (const IconDirEntry& e : es) {
            const NeResource* r = m.find(RT16_ICON, e.id);
            REQUIRE(r);
            const Icon ic = parse_icon(m.data(*r), m.size(*r));
            CHECK(ic.image.width == e.width);
            CHECK(ic.image.height == e.height);
            CHECK(ic.image.bpp == e.bpp);
            CHECK(ic.size == e.bytes);
            CHECK(ic.size <= m.size(*r));
            // every masked pixel is black in the XOR image: none inverts the screen
            const auto px = icon_to_bgra(ic);
            CHECK(std::all_of(px.begin(), px.end(), [](std::uint32_t c) { return c >> 24 == 0xFF || c == 0; }));
        }
        const auto ico = build_ico(m, *g);
        if (!assets.empty()) {
            const auto ref = read_file(assets / asset_dir / "GROUP_ICON" / (group_key(*g) + ".ico"));
            CHECK(ico == ref);
        }
    }
    return groups.size();
}
}  // namespace

TEST_CASE("INSPACE.EXE: the GAME icon group, 32x32 and 16x16 at 4 bpp") {
    const fs::path dir = env_dir("IS_ORIGINAL_DIR");
    const fs::path assets = env_dir("IS_ASSETS_DIR");
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    NeModule m = NeModule::load(find_ci(dir, "INSPACE.EXE"));
    CHECK(m.all(RT16_ICON).size() == 2);
    REQUIRE(m.all(RT16_GROUP_ICON).size() == 1);
    const NeResource* g = m.find(RT16_GROUP_ICON, "GAME");
    REQUIRE(g);
    const auto es = parse_group_icon(m.data(*g), m.size(*g));
    REQUIRE(es.size() == 2);
    const int dims[2] = {32, 16};
    const std::uint32_t sizes[2] = {744, 296};  // 40 + 64 + XOR + AND
    const std::size_t clear[2] = {514, 116};    // pixels whose AND bit is set
    for (int i = 0; i < 2; ++i) {
        INFO("entry " << i);
        CHECK(es[i].width == dims[i]);
        CHECK(es[i].height == dims[i]);
        CHECK(es[i].colors == 16);
        CHECK(es[i].planes == 1);
        CHECK(es[i].bpp == 4);
        CHECK(es[i].bytes == sizes[i]);
        CHECK(es[i].id == i + 1);
        const NeResource* r = m.find(RT16_ICON, es[i].id);
        REQUIRE(r);
        const Icon ic = parse_icon(m.data(*r), m.size(*r));
        CHECK(ic.image.width == dims[i]);
        CHECK(ic.image.height == dims[i]);
        CHECK(ic.image.palette.size() == 16);
        CHECK(ic.size == sizes[i]);
        const auto px = icon_to_bgra(ic);
        CHECK(static_cast<std::size_t>(std::count(px.begin(), px.end(), 0u)) == clear[i]);
    }
    const auto ico = build_ico(m, *g);
    CHECK(ico.size() == 6 + 32 + 744 + 296);
    if (assets.empty()) {
        MESSAGE("skipped: IS_ASSETS_DIR not available for the .ico comparison");
        return;
    }
    CHECK(ico == read_file(assets / "inspace.exe" / "GROUP_ICON" / "GAME.ico"));
}

TEST_CASE("game: every GROUP_ICON decodes and builds the .ico isre wrote") {
    const fs::path dir = env_dir("IS_ORIGINAL_DIR");
    const fs::path assets = env_dir("IS_ASSETS_DIR");
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    if (assets.empty()) MESSAGE("skipped: IS_ASSETS_DIR not available for the .ico comparison");
    // the NE modules of the distribution (docs/re/resources.md#inventory)
    const char* modules[] = {"ENG_LANG.DLL", "IGRAPHIC.DLL", "ISPACE.DLL", "INSPACE.EXE",
                             "SND_FX.DLL",   "ENG_FX.DLL",   "FACTORY.EXE", "INSPACE.SCR",
                             "INRUN.AD",     "INSTALL.EXE",  "CTL3D.DLL",   "WAVEMIX.DLL"};
    std::size_t groups = 0;
    for (const char* f : modules) groups += check_groups(dir, f, assets, lower(f));
    CHECK(groups == 8);  // ISPACE.DLL 3, five other modules 1 each

    // THEWORKS, when it sits next to the game directory as in work/extracted
    const fs::path base = dir.has_filename() ? dir : dir.parent_path();  // drop a trailing separator
    const fs::path works = base.parent_path() / "theworks";
    if (find_ci(works, "FACTORY.EXE").empty()) {
        MESSAGE("THEWORKS modules not found next to IS_ORIGINAL_DIR; their icons are not checked");
        return;
    }
    std::size_t more = 0;
    for (const char* f : {"FACTORY.EXE", "SETUP.EXE"}) more += check_groups(works, f, assets, "theworks-" + lower(f));
    CHECK(more == 2);
}
