// The ship sheet (src/gfx/ship_sheet.h), ISPACE.DLL ordinal 2's renderer seg6:0x11c: the
// tables and helpers pinned to their addresses, the arithmetic on small synthetic bodies, and
// TRANSFORMER's sheet against the wave-0001 oracle screenshots (48 x 48 crops at (504, 333) of
// work/oracle/wave-0001 05-wave, 06-fly-up, 07-fly-left and 08-fly-right, read at test time).
// The golden reads IS_ORIGINAL_DIR and work/oracle/wave-0001 beside it, and skips without them.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/palette.h"
#include "gfx/ship_sheet.h"
#include "png_decode.h"

using namespace is::gfx;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

// work/oracle/wave-0001 beside IS_ORIGINAL_DIR (work/extracted/shipdisk), as test_hud_win32.cpp
// finds it.
std::filesystem::path oracle_dir() {
    const auto o = original_dir();
    if (o.empty()) return {};
    std::filesystem::path base = o;
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / "wave-0001";
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

// The 48 x 48 crop at (504, 333) of an oracle screenshot: the player's cell and its margin.
pngtest::Image ship_crop(const std::filesystem::path& png) {
    const pngtest::Image full = pngtest::read(png.string());
    pngtest::Image c;
    if (full.w < 552 || full.h < 381) return c;
    c.w = c.h = 48;
    c.rgb.resize(48u * 48u);
    for (int y = 0; y < 48; ++y)
        for (int x = 0; x < 48; ++x) c.rgb[static_cast<std::size_t>(y) * 48 + x] = full.at(504 + x, 333 + y);
    return c;
}

// A 4-bpp body, h x (frames h), from top-down nibble rows.
is::fmt::Dib body4(int w, int h, const std::vector<std::vector<int>>& rows) {
    is::fmt::Dib d;
    d.width = w;
    d.height = h;
    d.bpp = 4;
    d.bottom_up = true;
    d.palette.assign(16, 0);
    d.bits.assign(d.stride() * h, 0xff);
    for (int t = 0; t < h; ++t) {
        const int m = h - 1 - t;  // memory row
        for (int x = 0; x < w; ++x) {
            std::uint8_t& b = d.bits[static_cast<std::size_t>(m) * d.stride() + x / 2];
            const int v = rows[t][x] & 15;
            b = (x & 1) ? static_cast<std::uint8_t>((b & 0xf0) | v)
                        : static_cast<std::uint8_t>((b & 0x0f) | (v << 4));
        }
    }
    return d;
}

// A palette with a grey ramp at 20-29 (bright to dark) and entry 245 red.
PaletteArray test_palette() {
    PaletteArray p{};
    for (int i = 0; i < 10; ++i) {
        const auto v = static_cast<std::uint8_t>(250 - 25 * i);
        p[20 + i].set({v, v, v});
    }
    for (int i = 0; i < 5; ++i) {
        const auto v = static_cast<std::uint8_t>(200 - 40 * i);
        p[40 + i].set({0, v, 0});
    }
    p[245].set({255, 0, 0});
    return p;
}

SheetPalette test_sheet_palette(const PaletteArray& p) {
    SheetPalette sp;
    sp.working = &p;
    sp.ramp = [](int c) { return c == 2 ? RampRange{20, 29} : RampRange{40, 44}; };
    return sp;
}

}  // namespace

TEST_CASE("ship sheet: the rotation tables ISPACE.DLL DGROUP:0x11a and DGROUP:0x13a") {
    const std::int16_t s[16] = {0, 196, 362, 473, 512, 473, 362, 196,
                                0, -196, -362, -473, -512, -473, -362, -196};
    const std::int16_t c[16] = {-512, -473, -362, -196, 0, 196, 362, 473,
                                512, 473, 362, 196, 0, -196, -362, -473};
    for (int i = 0; i < 16; ++i) {
        CHECK(kSheetSin[i] == s[i]);
        CHECK(kSheetCos[i] == c[i]);
    }
}

TEST_CASE("ship sheet: seg5:0x6f58 maps scheme rows to ramp ids") {
    CHECK(scheme_ramp_id(0) == 2);   // TRANSFORMER SHP +0x07: the grey record 2
    CHECK(scheme_ramp_id(10) == 4);  // TRANSFORMER SHP +0x08: the green record 4
    CHECK(scheme_ramp_id(0x10) == 2);
    CHECK(scheme_ramp_id(4) == 1);
    CHECK(scheme_ramp_id(8) == 0);
    CHECK(scheme_ramp_id(9) == 0);
    CHECK(scheme_ramp_id(0x11) == 9);
    CHECK(scheme_ramp_id(12) == -1);
}

TEST_CASE("ship sheet: seg6:0x7f52 and the brightness of a colour") {
    // x = 2 is raised from 0; three steps toward sqrt(100): 2 + 96/4 = 26, 26 + (100-676)/52
    // = 26 - 11 = 15, 15 + (100-225)/30 = 15 - 4 = 11.
    CHECK(newton_root(0, 100) == 11);
    CHECK(newton_root(10, 100) == 10);
    // White, body colour: R' = 192, G' = 255, B' = 127 + 31 = 158; m = 201, q = (36864 +
    // 65025 + 24964)/3 = 42284; 201 + (42284-40401)/402 = 205, 205 + (42284-42025)/410 = 205,
    // 205.
    CHECK(colour_brightness({255, 255, 255}) == 205);
    // White, palette entry: B' = 127; m = 191, q = 118018/3 = 39339; 191 + (39339-36481)/382 =
    // 198, 198 + (39339-39204)/396 = 198, 198.
    CHECK(entry_brightness({255, 255, 255}) == 198);
    CHECK(entry_brightness({0, 0, 0}) == 1);  // 2 + (0 - 4)/4 = 1, three times
}

TEST_CASE("ship sheet: seg5:0x6b72 walks the ramp toward the brightness") {
    const PaletteArray p = test_palette();
    std::vector<int> l;
    for (int e = 20; e < 30; ++e) l.push_back(entry_brightness(p[e].rgb()));
    // Within 3 of the start: the start.
    CHECK(nearest_ramp_entry(p, l[4], 20, 29, 24) == 24);
    // Starting bright (entry 20) for a dark target walks up to the closest entry.
    CHECK(nearest_ramp_entry(p, l[7], 20, 29, 20) == 27);
    // Starting dark walks down.
    CHECK(nearest_ramp_entry(p, l[2], 20, 29, 29) == 22);
    // Past the end: the end.
    CHECK(nearest_ramp_entry(p, 400, 20, 29, 25) == 20);
    CHECK(nearest_ramp_entry(p, 0, 20, 29, 25) == 29);
}

TEST_CASE("ship sheet: measures and layout of seg6:0x11c (TRANSFORMER's 132 x 33, mode 1)") {
    std::vector<std::vector<int>> rows(33, std::vector<int>(132, 15));
    const auto b = body4(132, 33, rows);
    const PaletteArray p = test_palette();
    SheetArgs a;
    a.gap = 99;
    a.four = 1;
    a.scheme_a = 0;
    a.scheme_b = 10;
    a.mode = 1;
    const ShipSheet sh = render_ship_sheet(b, std::vector<std::uint32_t>(16, 0x808080), a,
                                           test_sheet_palette(p));
    CHECK(sh.cell == 33);
    CHECK(sh.gap == 5);  // odd height
    CHECK(sh.frames == 4);
    CHECK(sh.rows == 2);
    CHECK(sh.dirs == 4);
    // ((16h + 17g) + (h + g)) x ((h + g) F + g) = 651 x 157.
    CHECK(sh.width == 651);
    CHECK(sh.height == 157);
    CHECK(sh.cell_x(0) == 5);
    CHECK(sh.cell_x(15) == 5 + 38 * 15);
    CHECK(sh.cell_y(1) == 43);
    // The bands of rows 0 and 1 are the row DIBs (index 0); the rest is BLACKNESS.
    CHECK(sh.at(0, 0) == 0);
    CHECK(sh.at(612, 80) == 0);
    CHECK(sh.at(613, 80) == ShipSheet::kBlackness);  // the extra cell column E
    CHECK(sh.at(0, 81) == ShipSheet::kBlackness);    // rows 2 and 3 are not rendered
    // An even height takes a gap of 6.
    std::vector<std::vector<int>> r8(8, std::vector<int>(8, 15));
    const ShipSheet s8 = render_ship_sheet(body4(8, 8, r8), std::vector<std::uint32_t>(16, 0), a,
                                           test_sheet_palette(p));
    CHECK(s8.gap == 6);
    CHECK(s8.rows == 1);
}

TEST_CASE("ship sheet: a lone pixel keeps half its brightness and maps onto the ramp") {
    // 9 x 9, one layer-0 pixel (colour 0) at top-down (4, 2), the rest transparent: no layer
    // edge (transparent neighbours count no corners) and no spill into them, so its 2x2 block
    // is L/2 four times, the average L/2 [BYTES ISPACE.DLL seg6:0x2217 seg6:0x4caf].
    std::vector<std::vector<int>> rows(9, std::vector<int>(9, 15));
    rows[2][4] = 0;
    const PaletteArray p = test_palette();
    std::vector<std::uint32_t> colours(16, 0);
    colours[0] = 0xffffff;
    SheetArgs a;
    a.scheme_a = 0;
    a.scheme_b = 10;
    const ShipSheet sh = render_ship_sheet(body4(9, 9, rows), colours, a, test_sheet_palette(p));
    REQUIRE(sh.ok());
    const int L = colour_brightness({255, 255, 255}) + 5;  // 210
    const int avg = L >> 1;                                 // 105
    // The ramp 20-29: p = hi - (hi - lo)(avg - L(hi)) / (L(lo) - L(hi)), then seg5:0x6b72.
    const int tlo = entry_brightness(p[20].rgb()), thi = entry_brightness(p[29].rgb());
    REQUIRE(thi < 2 * avg);
    int pv = 29 - (9 * (avg - thi)) / (tlo - thi);
    pv = nearest_ramp_entry(p, avg, 20, 29, pv);
    const auto c0 = sheet_cell(sh, 0, 0);
    const int n = sh.cell_size();
    REQUIRE(n == 9);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            CAPTURE(x);
            CAPTURE(y);
            CHECK(c0[y * n + x] == ((x == 4 && y == 2) ? pv : 0));
        }
    // Headings 4, 8, 12 are the quarter turns of heading 0, clockwise on screen
    // [BYTES ISPACE.DLL seg6:0x4df9 seg6:0x4e97 seg6:0x4f09 seg6:0x4f8b].
    const auto c4 = sheet_cell(sh, 4, 0), c8 = sheet_cell(sh, 8, 0), c12 = sheet_cell(sh, 12, 0);
    CHECK(c4[4 * n + (n - 1 - 2)] == pv);           // (x, y) -> (n-1-y, x)
    CHECK(c8[(n - 1 - 2) * n + (n - 1 - 4)] == pv);  // -> (n-1-x, n-1-y)
    CHECK(c12[(n - 1 - 4) * n + 2] == pv);           // -> (y, n-1-x)
}

TEST_CASE("ship sheet: quarter turns are exact for every pixel") {
    std::vector<std::vector<int>> rows(9, std::vector<int>(9, 15));
    for (int y = 1; y < 8; ++y)
        for (int x = 2; x < 6 + (y & 1); ++x) rows[y][x] = (x + y) % 3 == 0 ? 11 : 1;
    const PaletteArray p = test_palette();
    std::vector<std::uint32_t> colours(16, 0);
    for (int c = 0; c < 15; ++c) colours[c] = 0x101010u * static_cast<std::uint32_t>(15 - c);
    SheetArgs a;
    a.scheme_a = 0;
    a.scheme_b = 10;
    const ShipSheet sh = render_ship_sheet(body4(9, 9, rows), colours, a, test_sheet_palette(p));
    const int n = sh.cell_size();
    for (int d = 0; d < 4; ++d) {
        const auto c0 = sheet_cell(sh, d, 0), c4 = sheet_cell(sh, d + 4, 0);
        const auto c8 = sheet_cell(sh, d + 8, 0), c12 = sheet_cell(sh, d + 12, 0);
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                const auto v = c0[y * n + x];
                CHECK(c4[x * n + (n - 1 - y)] == v);
                CHECK(c8[(n - 1 - y) * n + (n - 1 - x)] == v);
                CHECK(c12[(n - 1 - x) * n + y] == v);
            }
    }
}

namespace {

// The best placement of any cell of `sheet` inside `crop`: the fewest differing pixels over
// the cell, black where the cell is black.
struct Match {
    int heading = -1, row = -1, ox = 0, oy = 0, diff = 1 << 30, lit = 0;
};
Match best_match(const ShipSheet& sh, const std::vector<std::uint32_t>& rgb,
                 const pngtest::Image& crop) {
    Match m;
    const int n = sh.cell_size();
    for (int row = 0; row < sh.rows; ++row)
        for (int d = 0; d < 16; ++d)
            for (int oy = 0; oy + n <= crop.h; ++oy)
                for (int ox = 0; ox + n <= crop.w; ++ox) {
                    int diff = 0, lit = 0;
                    for (int y = 0; y < n && diff < m.diff; ++y)
                        for (int x = 0; x < n; ++x) {
                            const std::uint32_t a =
                                rgb[static_cast<std::size_t>(sh.cell_y(row) + y) * sh.width +
                                    sh.cell_x(d) + x];
                            const std::uint32_t b = crop.at(ox + x, oy + y);
                            if (a) ++lit;
                            if (a != b) ++diff;
                        }
                    if (diff < m.diff) m = {d, row, ox, oy, diff, lit};
                }
    return m;
}

}  // namespace

TEST_CASE("ship sheet: TRANSFORMER against the wave-0001 oracle (golden)") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const auto od = oracle_dir();
    if (od.empty()) {
        MESSAGE("skipped: work/oracle/wave-0001 not found");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::assets::Ships ships = store.ships(dir);
    const is::fmt::ShipRecord* r = ships.find("TRANSFORMER");
    REQUIRE(r);
    const is::fmt::BodyRecord* b = ships.body(*r);
    REQUIRE(b);
    const is::fmt::Dib body = store.sprite(b->bitmap_id);
    // The wave palette: kind 0, seg10:0x6e76(8, 0) [BYTES seg10:0x37].
    PaletteManager pm;
    const is::fmt::Palette5500 p5 = store.palette5500();
    Colours5500 c5{};
    for (std::size_t i = 0; i < c5.size(); ++i) c5[i] = {p5.colours[i].r, p5.colours[i].g, p5.colours[i].b};
    pm.set_colours_5500(c5);
    const PalHandle pal = pm.create(0);
    pm.load_scheme(pal, 8, 0);
    // seg14:0x11a8: w1 1, w2 99, w5 1, w6 0 (Size 1), b1 SHP +0x07, b2 SHP +0x08, mode 1
    // (ship byte +0xe9 set) [BYTES seg14:0x129c-0x12d0].
    SheetArgs a;
    a.gap = 99;
    a.four = 1;
    a.scale = 0;
    a.scheme_a = r->byte_07;
    a.scheme_b = r->byte_08;
    a.mode = 1;
    const ShipSheet sh = ship_sheet_for(body, b->bitmap_id, pm, pal, a);
    REQUIRE(sh.ok());
    CHECK(sh.width == 651);
    CHECK(sh.height == 157);
    const std::vector<std::uint32_t> rgb = sheet_rgb(sh, *pm.working(pal));
    // 05 (wave start) and 07 (after Left 0.3 s) show the coasting cells untouched: every pixel
    // of the 33 x 33 cell matches. 06 (thrust) and 08 (after Right) differ only inside the
    // 9 x 9 box where seg11:0xb09c pastes the flame-on and flame-off cells of the strip DC
    // DGROUP:0x3a2c into the sheet DC, which is not ordinal 2's work; tests/test_ship_flame.cpp
    // checks those cells [BYTES seg11:0xb09c] [ORACLE wave-0001].
    struct Expect {
        const char* name;
        int heading;
        bool exact;
    };
    const Expect cases[] = {{"05-wave.png", 6, true},
                            {"07-fly-left.png", 2, true},
                            {"08-fly-right.png", 6, false},
                            {"06-fly-up.png", 6, false}};
    const int n = sh.cell_size();
    {
        // Row 1 (body frame 3, shifted one column by the odd f h quirk) against row 0.
        int rows_differ = 0;
        for (int d = 0; d < 16; ++d) {
            const auto r0 = sheet_cell(sh, d, 0), r1 = sheet_cell(sh, d, 1);
            for (std::size_t i = 0; i < r0.size(); ++i) rows_differ += r0[i] != r1[i];
        }
        MESSAGE("rows 0 and 1 differ in " << rows_differ << " pixels over the 16 headings");
    }
    for (const Expect& e : cases) {
        const pngtest::Image crop = ship_crop(od / e.name);
        REQUIRE(crop.w == 48);
        const Match m = best_match(sh, rgb, crop);
        MESSAGE(std::string(e.name) << ": heading " << m.heading << " row " << m.row << " at ("
                                    << m.ox << ", " << m.oy << "), " << m.diff << " of " << n * n
                                    << " pixels differ, " << m.lit << " lit");
        CHECK(m.heading == e.heading);
        CHECK(m.row == 0);
        CHECK(m.ox == 8);
        CHECK(m.oy == 8);
        int outside = 0;
        std::string dump;
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                const std::uint32_t pa =
                    rgb[static_cast<std::size_t>(sh.cell_y(m.row) + y) * sh.width +
                        sh.cell_x(m.heading) + x];
                const std::uint32_t pb = crop.at(m.ox + x, m.oy + y);
                if (pa == pb) continue;
                if (x < 4 || x > 12 || y < 4 || y > 12) ++outside;  // heading 6's paste box (4, 4) + 9 x 9
                char buf[80];
                std::snprintf(buf, sizeof buf, "(%d,%d) port %06x oracle %06x\n",
                              static_cast<int>(x), static_cast<int>(y), pa, pb);
                dump += buf;
            }
        if (std::getenv("IS_SHEET_DUMP")) MESSAGE(dump);
        if (e.exact)
            CHECK(m.diff == 0);
        else
            CHECK(outside == 0);
    }
}
