// ISPACE.DLL ordinal 17, the quick renderer, and its colour map (src/gfx/ship_quick.h,
// src/gfx/palette.h quick_colour_map; work/analysis/m4/s5-sheet.md 4.3-4.4, verified claims
// 23-34 of s5-sheet-verify.md, 31 CORRECTED): the odd-h shifts, the quarter turn, the gap fill,
// the blit into column 16, seg5:0x64f8's scan, and golden: core-0005's shield pictures (f1 for
// steps 1-3, f2 for 4-6, the ball for 7) against the port's quick cells and sheet. The golden
// reads IS_ORIGINAL_DIR and work/oracle/core-0005 beside it and skips without them.
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/palette.h"
#include "gfx/ship_quick.h"
#include "gfx/ship_sheet.h"
#include "png_decode.h"
#include "world/player_sheet.h"

using namespace is::gfx;

namespace {

// A 4-bpp strip of `frames` h x h frames, bottom-up as stored; pixel (x, t) top-down = v(x, t).
template <typename F>
is::fmt::Dib strip4(int h, int frames, const F& v) {
    is::fmt::Dib d;
    d.width = h * frames;
    d.height = h;
    d.bpp = 4;
    d.bottom_up = true;
    d.palette.assign(16, 0);
    d.bits.assign(d.stride() * static_cast<std::size_t>(h), 0xff);
    for (int t = 0; t < h; ++t) {
        const int m = h - 1 - t;
        for (int x = 0; x < d.width; ++x) {
            std::uint8_t& b = d.bits[static_cast<std::size_t>(m) * d.stride() + x / 2];
            const int n = v(x, t) & 15;
            b = (x & 1) ? static_cast<std::uint8_t>((b & 0xf0) | n) : static_cast<std::uint8_t>((b & 0x0f) | (n << 4));
        }
    }
    return d;
}

QuickColourMap plus_one() {
    QuickColourMap m{};
    for (int v = 0; v < 15; ++v) m[static_cast<std::size_t>(v)] = static_cast<std::uint8_t>(v + 1);
    return m;
}

}  // namespace

TEST_CASE("quick: heading 0 shifts frame 1 by one column and frame 2 by two for an odd h") {
    // Frame f's pixel (x, t) is nibble (x + 3 t + f) % 15; frame 0 is transparent (15).
    const int h = 33;
    const auto v = [h](int x, int t) {
        const int f = x / h, p = x % h;
        return f == 0 ? 15 : (p + 3 * t + f) % 15;
    };
    const QuickCells q = render_quick_cells(strip4(h, 4, v), plus_one(), 99, 0);
    REQUIRE(q.ok());
    CHECK(q.h == 33);
    CHECK(q.gap == 5);  // 0x63 -> 5 for an odd h
    CHECK(q.side == 43);
    const auto c1 = q.cell(0), c2 = q.cell(1);
    for (int t = 0; t < h; ++t) {
        CHECK(c1[static_cast<std::size_t>(t) * h + 0] == 0);  // nibble 32, frame 0's last: transparent
        for (int x = 1; x < h; ++x) CHECK(c1[static_cast<std::size_t>(t) * h + x] == v(h + x - 1, t) + 1);
        // Frame 2 two columns right; columns 0 and 1 take frame 1's pixels 31 and 32.
        CHECK(c2[static_cast<std::size_t>(t) * h + 0] == v(h + 31, t) + 1);
        CHECK(c2[static_cast<std::size_t>(t) * h + 1] == v(h + 32, t) + 1);
        for (int x = 2; x < h; ++x) CHECK(c2[static_cast<std::size_t>(t) * h + x] == v(2 * h + x - 2, t) + 1);
    }
    // The margins stay 0 (GlobalAlloc 0x42).
    for (int y = 0; y < q.side; ++y)
        for (int x = 0; x < q.side; ++x)
            if (x < q.gap || y < q.gap || x >= q.gap + h || y >= q.gap + h) CHECK(q.at(0, x, y) == 0);
}

TEST_CASE("quick: an even h has no shift, and heading 4 is a clockwise quarter turn (odd h)") {
    const int h = 8;
    const auto v = [h](int x, int t) {
        const int f = x / h, p = x % h;
        return f == 0 ? 15 : (p + 2 * t + 5 * f) % 15;
    };
    const is::fmt::Dib body = strip4(h, 4, v);
    const QuickCells q0 = render_quick_cells(body, plus_one(), 99, 0);
    REQUIRE(q0.ok());
    CHECK(q0.gap == 6);
    const auto a1 = q0.cell(0), a2 = q0.cell(1);
    for (int t = 0; t < h; ++t)
        for (int x = 0; x < h; ++x) {
            CHECK(a1[static_cast<std::size_t>(t) * h + x] == v(h + x, t) + 1);
            CHECK(a2[static_cast<std::size_t>(t) * h + x] == v(2 * h + x, t) + 1);
        }
    // Heading 4: memory (x, y) -> (y + gap, 2c - x + gap). For an odd h (2c = h - 1) that is a
    // clockwise quarter turn of the cell: row x, column h - 1 - t; frame 1 keeps its one-column
    // shift, so row 0 stays empty. For an even h (2c = h) the turn lands one row up and column x
    // = 0 falls outside: the original's arithmetic [BYTES ISPACE.DLL seg6:0x5981-0x5a71].
    const int ho = 9;
    const auto vo = [ho](int x, int t) {
        const int f = x / ho, p = x % ho;
        return f == 0 ? 15 : (p + 2 * t + 5 * f) % 15;
    };
    const QuickCells q4 = render_quick_cells(strip4(ho, 4, vo), plus_one(), 99, 4);
    REQUIRE(q4.ok());
    const auto b1 = q4.cell(0);
    for (int t = 0; t < ho; ++t) {
        CHECK(b1[static_cast<std::size_t>(0) * ho + (ho - 1 - t)] == 0);
        for (int x = 1; x < ho; ++x) CHECK(b1[static_cast<std::size_t>(x) * ho + (ho - 1 - t)] == vo(ho + x - 1, t) + 1);
    }
    const QuickCells q4e = render_quick_cells(body, plus_one(), 99, 4);
    const auto e1 = q4e.cell(0);
    for (int t = 0; t < h; ++t)
        for (int x = 1; x < h; ++x) CHECK(e1[static_cast<std::size_t>(x - 1) * h + (h - 1 - t)] == v(h + x, t) + 1);
}

TEST_CASE("quick: the gap fill at headings 2, 6, 10, 14 below the cell's top gap rows") {
    const int h = 32;
    const auto v = [](int x, int t) { return ((x + t) & 1) ? 15 : 3; };  // a checkerboard: holes before the fill
    const is::fmt::Dib body = strip4(h, 3, v);
    const auto holes = [h](const QuickCells& q, int from_row) {
        int n = 0;
        const auto c = q.cell(0);
        for (int t = from_row; t < h; ++t)
            for (int x = 1; x + 1 < h; ++x) {
                const std::size_t p = static_cast<std::size_t>(t) * h + x;
                n += c[p] == 0 && c[p - 1] != 0 && c[p + 1] != 0;
            }
        return n;
    };
    for (int d = 0; d < 16; ++d) {
        const QuickCells q = render_quick_cells(body, plus_one(), 99, d);
        REQUIRE(q.ok());
        if ((d + 2) % 4 == 0) CHECK_MESSAGE(holes(q, q.gap) == 0, "heading " << d);
    }
    // Without the fill the checkerboard keeps its holes (headings 1, 3: no fill) [BYTES ISPACE.DLL
    // seg6:0x5b07]; heading 2 keeps some only in the cell's top `gap` rows, which the fill's memory
    // rows 0 ... h - 1 do not reach.
    const QuickCells q1 = render_quick_cells(body, plus_one(), 99, 1), q2 = render_quick_cells(body, plus_one(), 99, 2);
    MESSAGE("holes: heading 1 " << holes(q1, 0) << ", heading 2 " << holes(q2, 0) << " (top rows), " << holes(q2, q2.gap));
    CHECK(holes(q1, 0) > 0);
    CHECK(holes(q2, q2.gap) == 0);
    CHECK(holes(q2, 0) > 0);
}

TEST_CASE("quick: the copy into column 16 of the sheet DC") {
    // Size 1: BitBlt to (16 h + 17 gap, (h + gap) it + gap) from (gap, gap), h x h.
    const QuickBlit a = quick_blit(33, 5, 0, 1), b = quick_blit(33, 5, 1, 1);
    CHECK_FALSE(a.stretch);
    CHECK(a.x == 613);
    CHECK(a.y == 5);
    CHECK(b.y == 43);
    CHECK(a.w == 33);
    CHECK(a.sx == 5);
    // Size 2: StretchBlt to the values << 2 sar 1, side 66.
    const QuickBlit c = quick_blit(33, 5, 1, 2);
    CHECK(c.stretch);
    CHECK(c.x == 1226);
    CHECK(c.y == 86);
    CHECK(c.w == 66);
    CHECK(c.sw == 33);
}

TEST_CASE("quick: seg5:0x64f8 scans the schemes' ramps and the colour map of ordinal 17") {
    PaletteManager pm;
    const PalHandle pal = pm.create(0);
    pm.load_scheme(pal, 8, 0);
    // Scheme 2's ramp (seg5:0x61b2): then the scan finds an entry within 14 of a ramp colour.
    pm.allocate_ramp(pal, 2);
    const int mode = pm.mode(pal);
    const int rec = pm.ramp_of(mode, 2);
    REQUIRE(rec >= 0);
    const PaletteManager::Ramp r = pm.ramp_record(mode, rec);
    REQUIRE(r.last >= r.first);
    const Rgb c = (*pm.working(pal))[static_cast<std::size_t>(r.first + 3)].rgb();
    int last = 10;
    const int e = pm.nearest_scheme_entry(pal, c, 2, -1, last);
    CHECK(e >= r.first);
    CHECK(e <= r.last);
    CHECK((*pm.working(pal))[static_cast<std::size_t>(e)].rgb() == c);
    // No scheme to scan: the last entry comes back.
    last = 77;
    CHECK(pm.nearest_scheme_entry(pal, c, -1, -1, last) == 77);
    // The map: with b1 = b2 = 12 each colour finds its own class ramp (seg5:0x66ea).
    std::vector<std::uint32_t> table(16, 0);
    for (int k = 0; k < 15; ++k) table[static_cast<std::size_t>(k)] = c.rgb();
    int dg = 10;
    const QuickColourMap m = quick_colour_map(pm, pal, table, 12, 12, 0, 0, dg);
    for (int k = 0; k < 15; ++k) {
        const int id = colour_ramp_id(c);
        const int rr = pm.ramp_of(mode, id);
        REQUIRE(rr >= 0);
        CHECK(m[static_cast<std::size_t>(k)] >= pm.ramp_record(mode, rr).first);
        CHECK(m[static_cast<std::size_t>(k)] <= pm.ramp_record(mode, rr).last);
    }
    // b1 != 12: entries 0 ... 9 scan the ids ordinal 17 set (here scheme 2's ramp).
    const QuickColourMap n = quick_colour_map(pm, pal, table, 3, 12, 2, -1, dg);
    for (int k = 0; k < 10; ++k) CHECK(n[static_cast<std::size_t>(k)] == e);
}

// ---- golden: core-0005's shield on the TRANSFORMER

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

std::filesystem::path oracle_dir(const char* run) {
    const auto o = original_dir();
    if (o.empty()) return {};
    std::filesystem::path base = o;
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / run;
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

}  // namespace

TEST_CASE("quick golden: core-0005's shield pictures against the port's quick cells and sheet") {
    const auto dir = original_dir();
    const auto od = oracle_dir("core-0005");
    if (dir.empty() || od.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR or work/oracle/core-0005 not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::assets::Ships ships = store.ships(dir);
    const is::fmt::ShipRecord* r = ships.find("TRANSFORMER");
    REQUIRE(r);
    const is::fmt::BodyRecord* b = ships.body(*r);
    REQUIRE(b);
    const is::fmt::Dib body = store.sprite(b->bitmap_id);
    // The flame-off paste's 9 x 9 box: the coast's Up released before, and every shield step
    // calls seg11:0xb09c(i, +0x02 = 0), whose off patch (a mirror for headings 5 ... 15) lands
    // in the heading cell and in column 16 [BYTES seg13:0x384-0x3df seg11:0xb105-0xb302]
    // (world/player_sheet.h flame_pastes; TRANSFORMER's BOD +0x19 = 23). Pixels inside it are
    // the flame's, not the renderers'.
    const is::world::FlameAnchors fa = is::world::flame_anchors(33, is::world::flame_radius_word(b->byte_19, 33), 1);
    const auto boxes = is::world::flame_pastes(9, fa, 0, 0, 0, 0, 33, 5, 1);
    REQUIRE(boxes.size() == 16);
    PaletteManager pm;
    const is::fmt::Palette5500 p5 = store.palette5500();
    Colours5500 c5{};
    for (std::size_t i = 0; i < c5.size(); ++i) c5[i] = {p5.colours[i].r, p5.colours[i].g, p5.colours[i].b};
    pm.set_colours_5500(c5);
    const PalHandle pal = pm.create(0);
    pm.load_scheme(pal, 8, 0);
    // The sheet first, as the placement builds it (mode 1: the shield body) [BYTES seg14:0x129c-0x12d0].
    SheetArgs a;
    a.scheme_a = r->byte_07;
    a.scheme_b = r->byte_08;
    a.mode = 1;
    const ShipSheet sh = ship_sheet_for(body, b->bitmap_id, pm, pal, a);
    REQUIRE(sh.ok());
    const int h = sh.cell;
    const auto sheet_rgb_at = [&](int x, int y) {
        const std::int16_t v = sh.at(x, y);
        return v <= 0 ? 0u : (*pm.working(pal))[static_cast<std::size_t>(v)].rgb().rgb();
    };
    const auto crop = [&od](const char* name) {
        const pngtest::Image full = pngtest::read((od / "picture" / name).string());
        return full;  // 64 x 64 from (496, 325): the ship's cell at (16, 16)
    };
    const pngtest::Image normal = crop("shield-coast-0-normal.png"), f1 = crop("shield-coast-1-f1.png"),
                         f2 = crop("shield-coast-2-f2.png"), ball = crop("shield-coast-3-ball.png");
    REQUIRE(normal.w == 64);
    // The heading: the row-0 cell that matches the normal picture.
    int heading = -1, best = 1 << 30;
    for (int d = 0; d < 16; ++d) {
        int diff = 0;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < h; ++x) diff += sheet_rgb_at(sh.cell_x(d) + x, sh.cell_y(0) + y) != normal.at(16 + x, 16 + y);
        if (diff < best) best = diff, heading = d;
    }
    REQUIRE(heading >= 0);
    const int bx = boxes[static_cast<std::size_t>(heading)].dx - sh.cell_x(heading);
    const int by = boxes[static_cast<std::size_t>(heading)].dy - sh.cell_y(0);
    const auto in_box = [bx, by](int x, int y) { return x >= bx && x < bx + 9 && y >= by && y < by + 9; };
    const auto diff_out = [&](const pngtest::Image& o, const auto& port) {
        int n = 0;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < h; ++x)
                if (!in_box(x, y) && port(x, y) != o.at(16 + x, 16 + y)) ++n;
        return n;
    };
    const int normal_out = diff_out(normal, [&](int x, int y) { return sheet_rgb_at(sh.cell_x(heading) + x, sh.cell_y(0) + y); });
    MESSAGE("the normal picture: heading " << heading << ", " << best << " pixels differ, " << normal_out
                                           << " outside the flame box at (" << bx << ", " << by << ")");
    CHECK(heading == 6);
    CHECK(normal_out == 0);
    // The ball (step 7): row 1 at the heading (body frame F - 1) [ORACLE core-0005].
    const int ball_out = diff_out(ball, [&](int x, int y) { return sheet_rgb_at(sh.cell_x(heading) + x, sh.cell_y(1) + y); });
    MESSAGE("the ball: " << ball_out << " pixels differ from row 1 outside the flame box");
    CHECK(ball_out == 0);
    // f1 and f2: ordinal 17's cells at the heading, through seg10:0x7328's arguments.
    const QuickCells q = quick_cells_for(body, b->bitmap_id, pm, pal, r->byte_07, r->byte_08, heading);
    REQUIRE(q.ok());
    for (int it = 0; it < 2; ++it) {
        const pngtest::Image& o = it == 0 ? f1 : f2;
        const auto cell = q.cell(it);
        int diff = 0;
        std::string g;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < h; ++x) {
                const std::uint8_t v = cell[static_cast<std::size_t>(y) * h + x];
                const std::uint32_t pc = v == 0 ? 0u : (*pm.working(pal))[v].rgb().rgb();
                const std::uint32_t oc = o.at(16 + x, 16 + y);
                if (!in_box(x, y)) diff += pc != oc;
                g += in_box(x, y) ? (pc == oc ? ',' : '#') : pc == oc ? (pc ? 'o' : '.') : (pc == 0 ? '+' : (oc == 0 ? '-' : 'x'));
            }
            g += '\n';
        }
        MESSAGE("f" << it + 1 << ": " << diff << " pixels differ from ordinal 17's cell " << it << " outside the flame box");
        if (std::getenv("IS_QUICK_DUMP")) MESSAGE(g);
        CHECK(diff == 0);
    }
}
