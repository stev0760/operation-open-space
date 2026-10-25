// The portable parts of the renderer: DIB conversion (ordinal 1's seg5:0x2080, LoadBitmap's
// monochrome bitmaps, ordinal 6's mask), the bitmap font's character map and layout
// (ordinal 33), the presenter's lookup, the scale factor (ADR 0006) and [Port] Scale.
#include <cstdlib>
#include <filesystem>
#include <set>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/font.h"
#include "gfx/gfx.h"
#include "gfx/present.h"
#include "gfx/surface.h"
#include "settings/settings.h"
#include "shell/geometry.h"

using namespace is::gfx;

namespace {

is::fmt::Dib make_dib(int w, int h, int bpp, std::vector<std::uint32_t> pal) {
    is::fmt::Dib d;
    d.width = w;
    d.height = h;
    d.bpp = bpp;
    d.palette = std::move(pal);
    d.bottom_up = false;
    d.bits.assign(d.stride() * static_cast<std::size_t>(h), 0);
    return d;
}

void put(is::fmt::Dib& d, int x, int y, int v) {
    std::uint8_t* row = d.bits.data() + d.stride() * static_cast<std::size_t>(y);
    switch (d.bpp) {
    case 1:
        if (v) row[x >> 3] |= static_cast<std::uint8_t>(0x80 >> (x & 7));
        else row[x >> 3] &= static_cast<std::uint8_t>(~(0x80 >> (x & 7)));
        break;
    case 4:
        row[x >> 1] = static_cast<std::uint8_t>(
            (x & 1) ? (row[x >> 1] & 0xf0) | v : (row[x >> 1] & 0x0f) | (v << 4));
        break;
    default: row[x] = static_cast<std::uint8_t>(v);
    }
}

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

}  // namespace

TEST_CASE("gfx render: 8-bit DIB pixels are indices, 4- and 1-bit ones the nearest entries") {
    DeviceColours dev = default_device_colours();
    dev[100] = Rgb::of(0x9f9f9f);
    dev[101] = Rgb::of(0x9f9f9f);  // a duplicate: the lower index wins
    auto d8 = make_dib(3, 2, 8, std::vector<std::uint32_t>(256, 0xffffff));
    put(d8, 0, 0, 7);
    put(d8, 2, 1, 200);
    auto i8 = dib_indices(d8, dev);
    CHECK(i8[0] == 7);
    CHECK(i8[5] == 200);
    CHECK(i8[1] == 0);

    auto d4 = make_dib(4, 1, 4, {0xa0a0a0, 0xffffff, 0x000000, 0xff0000});
    for (int x = 0; x < 4; ++x) put(d4, x, 0, x);
    auto i4 = dib_indices(d4, dev);
    CHECK(i4[0] == 100);  // a0a0a0 is nearer 9f9f9f than the static a0a0a4
    CHECK(i4[1] == 255);
    CHECK(i4[2] == 0);
    CHECK(i4[3] == 249);

    // bottom-up rows read top-down
    auto bu = make_dib(1, 2, 8, std::vector<std::uint32_t>(256, 0));
    bu.bottom_up = true;
    put(bu, 0, 0, 5);  // the stored first row is the bottom row
    auto ib = dib_indices(bu, dev);
    CHECK(ib[0] == 0);
    CHECK(ib[1] == 5);
}

TEST_CASE("gfx render: black-and-white bitmaps become monochrome, white = 1") {
    // SD_LOGO's layout: colour table [ffffff, 000000], the letters at index 0
    auto d = make_dib(10, 2, 1, {0xffffff, 0x000000});
    for (int x = 0; x < 10; ++x) put(d, x, 0, 1);
    put(d, 3, 0, 0);
    CHECK(is_black_and_white(d));
    const auto bits = dib_mono_bits(d);
    CHECK(bits.size() == 4);  // two rows of one word
    CHECK(bits[0] == 0x10);   // pixel 3 white
    CHECK(bits[1] == 0x00);
    CHECK(bits[2] == 0xff);   // row 1: all index 0, white
    CHECK(bits[3] == 0xc0);
    CHECK_FALSE(is_black_and_white(make_dib(2, 1, 1, {0xff0000, 0x000000})));
    CHECK_FALSE(is_black_and_white(make_dib(2, 1, 4, {0xffffff, 0x000000})));
    const Surface m = Surface::mono(10, 2, bits);
    CHECK(m.format() == Surface::Format::kMono1);
    CHECK(m.stride() == 2);
    CHECK(m.at(3, 0) == 1);
    CHECK(m.at(4, 0) == 0);
    CHECK(m.at(9, 1) == 1);
}

TEST_CASE("gfx render: ordinal 6's mask is 1 where the pixel is black") {
    Surface s = Surface::index8(5, 2);
    s.pixels()[1] = 7;
    s.pixels()[s.stride() + 4] = 255;
    const auto bits = mask_bits(s);
    REQUIRE(bits.size() == 4);
    CHECK(bits[0] == 0xb8);  // 1 0 1 1 1
    CHECK(bits[2] == 0xf0);  // 1 1 1 1 0
    // a mono source: its black (0) bits
    const Surface m = Surface::mono(3, 1, {0xa0, 0});
    const auto mb = mask_bits(m);
    CHECK(mb[0] == 0x40);
}

TEST_CASE("gfx render: an index surface is top-down with DIB rows") {
    Surface s = Surface::index8(5, 3);
    CHECK(s.width() == 5);
    CHECK(s.height() == 3);
    CHECK(s.stride() == 8);
    CHECK(s.at(4, 2) == 0);
    s.pixels()[2 * s.stride() + 4] = 9;
    CHECK(s.at(4, 2) == 9);
    Surface t = std::move(s);
    CHECK(t.at(4, 2) == 9);
    CHECK(s.empty());
    CHECK_THROWS(Surface::index8(0, 3));
}

TEST_CASE("gfx render: the fixed index table keeps the statics and has 256 distinct colours") {
    const DeviceColours& t = index_table();
    const DeviceColours stat = default_device_colours();
    for (int i = 0; i < 10; ++i) CHECK(t[i] == stat[i]);
    for (int i = 246; i < 256; ++i) CHECK(t[i] == stat[i]);
    CHECK(t[0] == Rgb{0, 0, 0});
    CHECK(t[255] == Rgb{255, 255, 255});
    std::set<std::uint32_t> distinct;
    for (const Rgb& c : t) distinct.insert(c.rgb());
    CHECK(distinct.size() == 256);
}

TEST_CASE("gfx render: ordinal 33 maps characters with case folding, blanks and skips") {
    CHECK(font_cell('A') == 0);
    CHECK(font_cell('a') == 0);
    CHECK(font_cell('Z') == 25);
    CHECK(font_cell('z') == 25);
    CHECK(font_cell('0') == 35);
    CHECK(font_cell('9') == 44);
    CHECK(font_cell('\\') == 26);
    CHECK(font_cell('/') == 26);
    CHECK(font_cell('(') == 27);
    CHECK(font_cell(')') == 28);
    CHECK(font_cell(':') == 29);
    CHECK(font_cell('!') == 30);
    CHECK(font_cell('?') == 31);
    CHECK(font_cell(',') == 32);
    CHECK(font_cell('.') == 33);
    CHECK(font_cell('-') == 34);
    CHECK(font_cell('%') == 45);
    CHECK(font_cell(' ') == kCellBlank);
    CHECK(font_cell('_') == kCellBlank);
    for (char c : std::string("\"'+#&;=@[]{|}~\x7f\x01\t"))
        CHECK(font_cell(c) == kCellSkip);
    for (int b = 0x80; b < 0x100; ++b) CHECK(font_cell(static_cast<char>(b)) == kCellSkip);
}

TEST_CASE("gfx render: ordinal 33 advances by the cell and aligns on the full length") {
    // "A'B C": the quote is skipped (no advance) but counts in lstrlen
    auto l = font_layout("A'B C", 0, 10, 18);
    REQUIRE(l.size() == 4);
    CHECK(l[0].x == 10);
    CHECK(l[0].cell == 0);
    CHECK(l[1].x == 28);
    CHECK(l[1].cell == 1);
    CHECK(l[2].x == 46);
    CHECK(l[2].cell == kCellBlank);
    CHECK(l[3].x == 64);
    // right: x - 5 * 12
    l = font_layout("A'B C", 2, 100, 12);
    CHECK(l[0].x == 40);
    // centred: x - (len * cw) / 2, truncating toward zero
    l = font_layout("ABC", 6, 0, 9);
    CHECK(l[0].x == -13);
    CHECK(font_start_x(6, 5, 3, 9) == 5 - 13);
    CHECK(font_start_x(4, 5, 3, 9) == 5);  // other aligns: x [DESIGN]
    FontCell c;
    REQUIRE(font_cell_size(0, &c));
    CHECK((c.w == 18 && c.h == 20));
    REQUIRE(font_cell_size(1, &c));
    CHECK((c.w == 12 && c.h == 18));
    REQUIRE(font_cell_size(2, &c));
    CHECK((c.w == 9 && c.h == 10));
    CHECK_FALSE(font_cell_size(3, &c));
    CHECK(bar_fill_width(200, 50) == 100);
    CHECK(bar_fill_width(30, 33) == 9);
    CHECK(bar_fill_width(30, -10) == -3);
}

TEST_CASE("gfx render: the presenter's lookup converts indices with the palette") {
    DeviceColours pal = default_device_colours();
    pal[42] = Rgb::of(0x123456);
    const std::uint8_t src[8] = {0, 42, 255, 9, 249, 0, 0, 0};
    std::uint32_t out[8] = {};
    fill_bgra(src, 4, 3, 2, pal, out, 4);
    CHECK(out[0] == 0x000000);
    CHECK(out[1] == 0x123456);
    CHECK(out[2] == 0xffffff);
    CHECK(out[4] == 0xff0000);
    CHECK(out[3] == 0);  // past the row's width: untouched
}

TEST_CASE("gfx render: the scale factor of ADR 0006") {
    using is::shell::scale_factor;
    CHECK(scale_factor(0, 1024, 768) == 1);
    CHECK(scale_factor(0, 800, 600) == 1);
    CHECK(scale_factor(0, 1920, 1200) == 2);
    CHECK(scale_factor(0, 1920, 1199) == 1);
    CHECK(scale_factor(0, 2560, 1440) == 2);
    CHECK(scale_factor(0, 3840, 2160) == 3);
    CHECK(scale_factor(0, 5120, 2880) == 4);
    CHECK(scale_factor(0, 600, 300) == 1);  // never below 1
    // fixed factors, lowered while the logical size is below 600 x 400
    CHECK(scale_factor(2, 1600, 900) == 2);
    CHECK(scale_factor(4, 1024, 768) == 1);
    CHECK(scale_factor(3, 3840, 2160) == 3);
    CHECK(scale_factor(4, 3840, 2160) == 4);
    CHECK(scale_factor(4, 2400, 1599) == 3);
    CHECK(scale_factor(2, 1199, 900) == 1);
    CHECK(scale_factor(9, 2560, 1440) == 2);  // others are automatic
    CHECK(is::shell::logical_length(2560, 2) == 1280);
    CHECK(is::shell::logical_length(1025, 2) == 512);
    // the logical cover of a physical rectangle (WM_ERASEBKGND's clip box)
    using is::shell::Rect;
    CHECK(is::shell::logical_cover(Rect{3, 4, 9, 10}, 2, 100, 100) == Rect{1, 2, 5, 5});
    CHECK(is::shell::logical_cover(Rect{0, 0, 1025, 769}, 2, 512, 384) == Rect{0, 0, 512, 384});
    CHECK(is::shell::logical_cover(Rect{-5, -1, 7, 7}, 1, 100, 100) == Rect{0, 0, 7, 7});
    CHECK(is::shell::logical_cover(Rect{300, 300, 310, 310}, 3, 50, 50) == Rect{50, 50, 50, 50});
}

TEST_CASE("gfx render: [Port] Scale defaults to 0; values outside 0-4 act as 0") {
    const auto dir = std::filesystem::temp_directory_path() / "is_gfx_port_scale";
    std::filesystem::create_directories(dir);
    const auto path = dir / "INSPACE.INI";
    std::filesystem::remove(path);
    is::settings::IniFile ini(path);
    is::settings::PortSettings p;
    p.load(ini);
    CHECK(p.scale == 0);
    ini.set_int(is::settings::kPortSection, "Scale", 3);
    p.load(ini);
    CHECK(p.scale == 3);
    // out of range: scale_factor treats the key as automatic (0)
    ini.set_int(is::settings::kPortSection, "Scale", 5);
    p.load(ini);
    CHECK(is::shell::scale_factor(p.scale, 2560, 1440) == is::shell::scale_factor(0, 2560, 1440));
    ini.set_int(is::settings::kPortSection, "Scale", -1);
    p.load(ini);
    CHECK(is::shell::scale_factor(p.scale, 1600, 900) == is::shell::scale_factor(0, 1600, 900));
    // the original's section is untouched by the port's key
    CHECK(ini.entries(is::settings::kGameSection).empty());
}

TEST_CASE("gfx render: the facade sizes the surface in logical pixels and keeps its overlap") {
    Gfx g(nullptr);
    g.set_client_size(1024, 768, 0);
    CHECK(g.factor() == 1);
    CHECK(g.logical_width() == 1024);
    CHECK(g.logical_height() == 768);
    g.surface().flush();
    g.surface().pixels()[5 * g.surface().stride() + 7] = 33;
    g.set_client_size(2560, 1440, 0);
    CHECK(g.factor() == 2);
    CHECK(g.logical_width() == 1280);
    CHECK(g.logical_height() == 720);
    CHECK(g.surface().at(7, 5) == 33);
    CHECK(g.surface().at(1100, 700) == 0);
    g.set_client_size(0, 0, 0);  // minimised: unchanged
    CHECK(g.logical_width() == 1280);
    // the palette wrappers and the colour tables follow the device palette
    g.palette_create();
    CHECK(g.palette() != 0);
    g.palette_scheme(-1000, 2);
    CHECK(g.palettes().device()[50] == Rgb{255, 255, 255});
    CHECK(g.colour_index(0x00FFFFFF) == 255);
    g.palette_delete();
    CHECK_FALSE(g.palettes().valid(g.palette()));
}

TEST_CASE("gfx render: golden: the intro's images load and map to the palette") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    is::assets::AssetStore store(dir);
    Gfx g(&store);
    g.set_client_size(640, 480, 0);
    g.palette_create();
    REQUIRE(g.alphabet() != nullptr);  // ISPACE.DLL loads ALPHABET in command 0
    CHECK(g.alphabet()->format() == Surface::Format::kMono1);
    CHECK(g.alphabet()->width() == 828);
    CHECK(g.alphabet()->height() == 20);
    g.palette_scheme(8, 2);
    ImagePtr logo = g.load_bitmap("SD_LOGO");
    CHECK(logo->format() == Surface::Format::kMono1);
    CHECK(logo->width() == 130);
    CHECK(logo->height() == 32);
    for (int id : {220, 221, 300, 340}) {
        CAPTURE(id);
        ImagePtr im = g.load_graphic(id, 3, -1);
        CHECK(im->format() == Surface::Format::kIndex8);
        CHECK(g.graphic_height() == im->height());
    }
    ImagePtr ring = g.load_graphic(220, 3, -1);
    CHECK(ring->width() == 40);
    // 220's colour 15 is black: index 0; its corners are the black background
    CHECK(ring->at(0, 0) == 0);
    ImagePtr m = g.mask(*ring);
    CHECK(m->at(0, 0) == 1);
    // the title's ordinal 12 load writes 185-229 and leaves 230-245 black
    g.palette_scheme(8, 1);
    g.palette_ramp(5);
    const is::fmt::Dib d300 = g.graphic_dib(300, 0, 1, 12, 0, true);
    CHECK(d300.width == 138);
    CHECK(g.palettes().device()[185] == Rgb::of(0xffe66e));
    CHECK(g.palettes().device()[229] == Rgb::of(0x565552));
    CHECK(g.palettes().device()[230].black());
}
