// The renderer's GDI side under Win32 (Wine in CTest): a surface at a scaled logical size, the
// start-up palette sequence, a PatBlt, a bitmap-font string, images whose fixed colour table
// keeps indices through blits while the palette changes, and the presented 32-bit pixels. The logo strip and the real ALPHABET are
// golden parts (IS_ORIGINAL_DIR).
#ifdef _WIN32
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/gfx.h"
#include "gfx/palette_message.h"
#include "shell/game_state.h"

using namespace is::gfx;

namespace {

// A stand-in ALPHABET (828 x 20, colour table [ffffff, 000000], glyph pixels index 0): cell 0
// ('A') has its left half as glyph, cell 36 ('1') is all glyph, the rest background.
is::fmt::Dib fake_alphabet() {
    is::fmt::Dib d;
    d.width = 828;
    d.height = 20;
    d.bpp = 1;
    d.palette = {0xffffff, 0x000000};
    d.bottom_up = false;
    d.bits.assign(d.stride() * 20, 0xff);  // index 1 everywhere
    for (int y = 0; y < 20; ++y) {
        std::uint8_t* row = d.bits.data() + d.stride() * y;
        for (int x = 0; x < 828; ++x) {
            const int cell = x / 18, cx = x % 18;
            if ((cell == 0 && cx < 9) || cell == 36) row[x >> 3] &= ~(0x80 >> (x & 7));
        }
    }
    return d;
}

Colours5500 fake5500() {
    Colours5500 c{};
    for (int i = 0; i < 92; ++i)
        c[i] = {static_cast<std::uint8_t>(i + 1), static_cast<std::uint8_t>(200 - i),
                static_cast<std::uint8_t>(50 + i)};
    return c;
}

// A 32-bpp top-down DIB section with a DC, standing in for the window.
struct Target {
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ old = nullptr;
    std::uint32_t* bits = nullptr;
    int w = 0, h = 0;
    Target(int w_, int h_) : w(w_), h(h_) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        void* p = nullptr;
        bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &p, nullptr, 0);
        bits = static_cast<std::uint32_t*>(p);
        dc = CreateCompatibleDC(nullptr);
        old = SelectObject(dc, bmp);
        for (int i = 0; i < w * h; ++i) bits[i] = 0x00abcdef;
    }
    ~Target() {
        SelectObject(dc, old);
        DeleteDC(dc);
        DeleteObject(bmp);
    }
    std::uint32_t at(int x, int y) const {
        GdiFlush();
        return bits[y * w + x] & 0xffffff;
    }
};

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

}  // namespace

TEST_CASE("gfx win32: palette sequence, PatBlt and font on the surface, presented at factor 2") {
    Gfx g(nullptr);
    g.set_alphabet(fake_alphabet());
    g.set_colours_5500(fake5500());
    g.set_client_size(1203, 801, 2);  // logical 601 x 400, one spare physical column and row
    REQUIRE(g.factor() == 2);
    REQUIRE(g.logical_width() == 601);
    REQUIRE(g.logical_height() == 400);
    // start-up: seg6:0x190c, then the 0x9cf handler (shell.md "Palette", Events)
    g.palette_create();
    REQUIRE(g.alphabet() != nullptr);
    g.palette_scheme(-1000, 2);
    g.palette_delete();
    g.palette_create();
    g.palette_scheme(8, 2);
    g.blacken();

    HDC dc = g.dc();
    // a plain RGB brush lands on the static red, 249
    HBRUSH red = CreateSolidBrush(g.dc_colour(RGB(255, 0, 0)));
    HGDIOBJ old = SelectObject(dc, red);
    PatBlt(dc, 10, 20, 30, 5, PATCOPY);
    DeleteObject(SelectObject(dc, old));
    // a palette entry by PALETTEINDEX: scheme 8's first entry, 103
    HBRUSH p103 = CreateSolidBrush(g.dc_colour(0x01000067));
    old = SelectObject(dc, p103);
    PatBlt(dc, 100, 100, 4, 4, PATCOPY);
    DeleteObject(SelectObject(dc, old));
    // font 0, align 0, green: "A1" at (200, 50); then font 2 right-aligned
    g.font_text(0, 0, 0x0000FF00, 200, 50, "A1");
    g.font_text(2, 2, 0x0000FFFF, 300, 150, "1 ");
    g.surface().flush();

    const Surface& s = g.surface();
    CHECK(s.at(10, 20) == 249);
    CHECK(s.at(39, 24) == 249);
    CHECK(s.at(40, 24) == 0);
    CHECK(s.at(9, 20) == 0);
    CHECK(s.at(100, 100) == 103);
    // 'A': the left half of its 18 x 20 cell in the brush colour, the right half black
    CHECK(s.at(200, 50) == 250);
    CHECK(s.at(208, 69) == 250);
    CHECK(s.at(209, 50) == 0);
    CHECK(s.at(217, 69) == 0);
    // '1': its whole cell
    CHECK(s.at(218, 50) == 250);
    CHECK(s.at(235, 69) == 250);
    CHECK(s.at(236, 50) == 0);
    // font 2 (9 x 10), right-aligned on the full length 2: x = 300 - 18; the blank is black
    CHECK(s.at(282, 150) == 251);
    CHECK(s.at(290, 159) == 251);
    CHECK(s.at(291, 150) == 0);
    CHECK(s.at(281, 150) == 0);

    // present into a 1203 x 801 "client": 2 x 2 blocks, the spare column and row black
    Target t(1203, 801);
    REQUIRE(g.present_dc(t.dc, 1203, 801, true));
    CHECK(t.at(20, 40) == 0xff0000);
    CHECK(t.at(21, 41) == 0xff0000);
    CHECK(t.at(79, 49) == 0xff0000);
    CHECK(t.at(80, 49) == 0x000000);
    CHECK(t.at(200, 200) == kSchemes10[7][0].rgb());
    CHECK(t.at(400, 100) == 0x00ff00);
    CHECK(t.at(1202, 5) == 0x000000);
    CHECK(t.at(5, 800) == 0x000000);
    // nothing changed: no conversion and, without force, no blit
    CHECK_FALSE(g.present_dc(t.dc, 1203, 801, false));
    // a palette change alone recolours the presented pixels; the indices stay
    g.palette_save(0, 1);  // blanks 10-245
    REQUIRE(g.present_dc(t.dc, 1203, 801, false));
    CHECK(t.at(200, 200) == 0x000000);
    CHECK(t.at(20, 40) == 0xff0000);  // a static colour does not change
    g.palette_restore(0);
    g.present_dc(t.dc, 1203, 801, false);
    CHECK(t.at(200, 200) == kSchemes10[7][0].rgb());

    // ordinal 34 and 35 on the surface: style 0 is black with a light grey right edge
    g.frame(400, 300, 20, 10, 0);
    g.bar(450, 300, 100, 10, 50);
    g.surface().flush();
    CHECK(s.at(410, 305) == 0);
    CHECK(s.at(420, 305) == 7);    // c0c0c0
    CHECK(s.at(410, 310) == 7);
    CHECK(s.at(455, 305) == 250);  // green inside the filled half
    CHECK(s.at(450, 300) == 251);  // yellow top edge
    CHECK(s.at(520, 305) == 0);    // black, unfilled
    CHECK(s.at(448, 311) == 255);  // white frame line below
}

TEST_CASE("gfx win32: images keep indices through blits as the palette changes") {
    Gfx g(nullptr);
    g.set_colours_5500(fake5500());
    g.set_client_size(640, 480, 0);
    g.palette_create();
    g.palette_scheme(-1000, 2);  // 10-245 white: many duplicates
    ImagePtr im = g.create_image(8, 8);
    im->pixels()[0] = 150;
    im->pixels()[1] = 10;
    im->pixels()[2] = 255;
    // indices copy as they are between the image and the surface, duplicates or not
    BitBlt(g.dc(), 0, 0, 8, 8, im->hdc(), 0, 0, SRCCOPY);
    g.surface().flush();
    CHECK(g.surface().at(0, 0) == 150);
    CHECK(g.surface().at(1, 0) == 10);
    CHECK(g.surface().at(2, 0) == 255);
    // after a palette change too, which leaves every colour table as it was
    g.palette_scheme(8, 0);
    RGBQUAD tab[256];
    REQUIRE(GetDIBColorTable(im->hdc(), 0, 256, tab) == 256);
    for (int i = 0; i < 256; ++i) {
        CAPTURE(i);
        CHECK(Rgb{tab[i].rgbRed, tab[i].rgbGreen, tab[i].rgbBlue} == index_table()[i]);
    }
    BitBlt(g.dc(), 0, 1, 8, 1, im->hdc(), 0, 0, SRCCOPY);
    g.surface().flush();
    CHECK(g.surface().at(0, 1) == 150);
    CHECK(g.surface().at(1, 1) == 10);
    // a monochrome mask over the surface: SRCAND keeps where 1, clears where 0
    ImagePtr m = g.mask(*im);
    CHECK(m->at(0, 0) == 0);
    CHECK(m->at(3, 0) == 1);
    PatBlt(g.dc(), 0, 2, 8, 1, WHITENESS);
    BitBlt(g.dc(), 0, 2, 8, 1, m->hdc(), 0, 0, SRCAND);
    g.surface().flush();
    CHECK(g.surface().at(0, 2) == 0);
    CHECK(g.surface().at(3, 2) == 255);
    // a mirrored StretchBlt with negative extents (ordinal 2's turned cells)
    ImagePtr src = g.create_image(4, 4);
    src->pixels()[0] = 77;  // (0, 0)
    StretchBlt(g.dc(), 13, 13, -4, -4, src->hdc(), 0, 0, 4, 4, SRCCOPY);
    g.surface().flush();
    CHECK(g.surface().at(13, 13) == 77);
    CHECK(g.surface().at(10, 10) == 0);
}

TEST_CASE("gfx win32: golden: ALPHABET text and the logo strip") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    is::assets::AssetStore store(dir);
    Gfx g(&store);
    g.set_client_size(640, 480, 0);
    g.palette_create();
    g.palette_create();  // the second palette does not reload ALPHABET
    g.palette_delete();
    g.palette_create();
    g.palette_scheme(8, 2);
    REQUIRE(g.alphabet() != nullptr);
    g.blacken();
    // "I" in font 0: some glyph pixels in green, none outside the 18 x 20 cell
    g.font_text(0, 0, 0x0000FF00, 10, 10, "I");
    g.surface().flush();
    int green = 0, other = 0;
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 40; ++x) {
            const int v = g.surface().at(x, y);
            const bool inside = x >= 10 && x < 28 && y >= 10 && y < 30;
            if (v == 250) green += inside ? 1 : 1000;
            else if (v != 0) ++other;
        }
    CHECK(green > 20);
    CHECK(green < 1000);
    CHECK(other == 0);

    ImagePtr strip = g.logo_strip();
    REQUIRE(strip != nullptr);
    CHECK(strip->width() == 650);
    CHECK(strip->height() == 52);
    ImagePtr ring = g.load_graphic(220, 3, -1);
    ImagePtr cells = g.load_graphic(221, 3, -1);
    strip->flush();
    // cell 0 is the bare ring; the gaps are black
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 40; ++x) CHECK(strip->at(6 + x, 6 + y) == ring->at(x, y));
    for (int y = 0; y < 52; ++y) CHECK(strip->at(46 * 3 + 2, y) == 0);
    for (int x = 0; x < 650; ++x) CHECK(strip->at(x, 2) == 0);
    // cells 1-4 are 221's cells 0-3 over the ring, cells 9-6 the same cells turned 180 degrees,
    // cell 5 is 221's cell 4 and cells 10-13 its cells 5-8 (ispace.md, ordinal 2)
    int checked = 0;
    const auto over = [&](int strip_cell, int src_cell, bool turned) {
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 40; ++x) {
                const int v = cells->at(40 * src_cell + x, y);
                const int sx = 46 * strip_cell + 6 + (turned ? 39 - x : x);
                const int sy = 6 + (turned ? 39 - y : y);
                if (v == 0) continue;
                ++checked;
                CHECK(strip->at(sx, sy) == v);
            }
    };
    for (int k = 0; k < 4; ++k) {
        over(k + 1, k, false);
        over(9 - k, k, true);
    }
    over(5, 4, false);
    for (int k = 0; k < 4; ++k) over(k + 10, k + 5, false);
    CHECK(checked == 2 * (50 + 112 + 164 + 273) + 524 + 699 + 847 + 1014 + 1106);
}

// ADR 0007 decision 5: the emulated WM_QUERYNEWPALETTE (seg2:0x5cb with WM_PAINT seg2:0x7ac).
TEST_CASE("gfx win32: the emulated palette message erases to black, after the state-1 repaint") {
    Gfx g(nullptr);
    g.set_alphabet(fake_alphabet());
    g.set_colours_5500(fake5500());
    g.set_client_size(640, 480, 1);
    std::vector<std::string> order;
    const auto fill = [&g](int index) {
        HBRUSH b = CreateSolidBrush(g.dc_colour(0x01000000u | static_cast<unsigned>(index)));
        HGDIOBJ old = SelectObject(g.dc(), b);
        PatBlt(g.dc(), 0, 0, 640, 480, PATCOPY);
        DeleteObject(SelectObject(g.dc(), old));
        g.surface().flush();
    };
    // the paint routine draws entry 103 over everything; it records what it found first
    const auto paint = [&] {
        g.surface().flush();
        order.push_back("paint " + std::to_string(g.surface().at(5, 5)));
        fill(103);
    };
    const auto post = [&] { order.push_back("post"); };

    // no palette (DGROUP:0x920e = 0): nothing happens
    fill(249);
    CHECK_FALSE(emulate_query_new_palette(g, is::shell::kIntro, true, paint, post));
    CHECK(order.empty());
    CHECK(g.surface().at(5, 5) == 249);

    g.palette_create();
    g.palette_scheme(8, 2);
    // state 0 (the title): 0x978 is posted, the client erased to black
    fill(249);
    CHECK(emulate_query_new_palette(g, is::shell::kIntro, true, paint, post));
    g.surface().flush();
    CHECK(order == std::vector<std::string>{"post"});
    CHECK(g.surface().at(0, 0) == 0);
    CHECK(g.surface().at(639, 479) == 0);
    CHECK(g.surface().at(320, 240) == 0);

    // state 3 likewise
    order.clear();
    fill(249);
    CHECK(emulate_query_new_palette(g, is::shell::kBriefing, true, paint, post));
    g.surface().flush();
    CHECK(order == std::vector<std::string>{"post"});
    CHECK(g.surface().at(320, 240) == 0);

    // states 1 and 7: the paint routine runs first, on the old picture, then the erase
    for (int state : {is::shell::kWave, is::shell::kHelp}) {
        CAPTURE(state);
        order.clear();
        fill(249);
        CHECK(emulate_query_new_palette(g, state, true, paint, post));
        g.surface().flush();
        CHECK(order == std::vector<std::string>{"paint 249"});
        CHECK(g.surface().at(5, 5) == 0);
        CHECK(g.surface().at(639, 479) == 0);
    }

    // before the first WM_TIMER (DGROUP:0x1e = 0) WM_PAINT only erases
    order.clear();
    fill(249);
    CHECK(emulate_query_new_palette(g, is::shell::kWave, false, paint, post));
    g.surface().flush();
    CHECK(order.empty());
    CHECK(g.surface().at(5, 5) == 0);

    // presented: the window shows black (index 0 is the static black)
    Target t(640, 480);
    REQUIRE(g.present_dc(t.dc, 640, 480, true));
    CHECK(t.at(5, 5) == 0x000000);
    CHECK(t.at(639, 479) == 0x000000);
}
#endif
