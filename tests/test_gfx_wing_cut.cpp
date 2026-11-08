// ISPACE.DLL ordinal 15, the wing cut (src/gfx/wing_cut.h; work/analysis/m4/s5-sheet.md sections
// 2.2-2.8, verified claims 11-21 of s5-sheet-verify.md): the tables, the windows and the
// orientation, the exact vectors of section 2.8, the symmetry of the two sides, and golden crops:
// core-0001's 35-f1-20 (heading 2, one wing) and 36-f1-30 (heading 3, none) against the port's
// TRANSFORMER sheet, core-0006's SKY TALON frames of the second wing loss and the repair
// (sky-wing2-c0027 ... c0031, v1-back-cell, v2-back-cell). The goldens read IS_ORIGINAL_DIR and
// work/oracle beside it and skip without them. The Wine GDI raster of the mask is
// tests/test_gfx_wing_cut_win32.cpp's.
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/palette.h"
#include "gfx/ship_sheet.h"
#include "gfx/wing_cut.h"
#include "png_decode.h"
#include "world/player_sheet.h"

using namespace is::gfx;

namespace {

constexpr int kS = 33, kG = 5, kD = 5;  // TRANSFORMER at Size 1: cell 33, gap 5, D 5

// Columns (or rows) of an s x s keep map that are kept in every row (column).
bool keeps_columns(const std::vector<std::uint8_t>& k, int lo, int hi) {
    for (int y = 0; y < kS; ++y)
        for (int x = 0; x < kS; ++x)
            if ((k[static_cast<std::size_t>(y) * kS + x] != 0) != (x >= lo && x <= hi)) return false;
    return true;
}
bool keeps_rows(const std::vector<std::uint8_t>& k, int lo, int hi) {
    for (int y = 0; y < kS; ++y)
        for (int x = 0; x < kS; ++x)
            if ((k[static_cast<std::size_t>(y) * kS + x] != 0) != (y >= lo && y <= hi)) return false;
    return true;
}

}  // namespace

TEST_CASE("wing cut: ISPACE.DLL DGROUP:0x10 and DGROUP:0x1a, the windows of ordinal 15") {
    CHECK(kWingT1 == std::array<std::int16_t, 5>{-128, -118, -91, -50, 0});
    CHECK(kWingT2 == std::array<std::int16_t, 5>{0, 50, 91, 118, 128});
    // s = 33, D = 5: o = 29, A = (-5, -5, -4, -2, 0), B = (0, 1, 3, 4, 5) (s5-sheet.md 2.3).
    const int sx[5] = {34, 124, 213, 301, 389}, sy[5] = {29, 30, 32, 33, 34};
    for (int k = 0; k < 5; ++k) {
        const WingWindow w = wing_window(kS, kD, k);
        CHECK(w.sx == sx[k]);
        CHECK(w.sy == sy[k]);
        CHECK(w.w == kS);
        CHECK(w.h == kS);
    }
    // D = 10: A = (-10, -10, -8, -4, 0), B = (0, 3, 7, 9, 10).
    const int a10[5] = {-10, -10, -8, -4, 0}, b10[5] = {0, 3, 7, 9, 10};
    for (int k = 0; k < 5; ++k) {
        const WingWindow w = wing_window(kS, 10, k);
        CHECK(w.sx == 90 * k - a10[k] + 29);
        CHECK(w.sy == b10[k] + 29);
    }
    // A big cell clips the window: s = 80, D = 10 at cell 0: o = 5, w = min(-10 - 5 + 89, 80).
    CHECK(wing_window(80, 10, 0).w == 74);
    CHECK(wing_window(80, 10, 0).h == 80);
    // The switch at seg5:0x12ab.
    const int k[16] = {0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1};
    for (int f = 0; f < 16; ++f) CHECK(wing_cell(f) == k[f]);
    CHECK(wing_cell(16) == -1);
}

TEST_CASE("wing cut: the mask DGROUP:0x184, PatBlts exact, polygon runs as Wine's GDI draws them") {
    const WingMask& m = wing_mask();
    REQUIRE(m.bit.size() == static_cast<std::size_t>(kWingMaskW) * kWingMaskH);
    for (int y = 0; y < 90; ++y) {
        CHECK_FALSE(m.at(44, y));
        CHECK(m.at(45, y));
        CHECK(m.at(90, y));  // the PatBlt's last column is cell 1's column 0
        CHECK_FALSE(m.at(91, y));
        CHECK(m.at(179, y));
        CHECK(m.at(269, y));
        CHECK(m.at(359, y) == (y >= 21));
        CHECK(m.at(360, y) == (y >= 45));
        CHECK(m.at(449, y) == (y >= 45));
    }
    // Polygon 1 (cell 2) is the triangle below the diagonal; cell 1 starts at 157 on row 0 and 112
    // on row 89; cell 3's top edge runs from (359, 21) to (270, 68).
    CHECK(m.at(180, 89));
    CHECK_FALSE(m.at(180, 88));
    CHECK(m.at(157, 0));
    CHECK_FALSE(m.at(156, 0));
    CHECK(m.at(112, 89));
    CHECK_FALSE(m.at(111, 89));
    CHECK(m.at(270, 68));
    CHECK_FALSE(m.at(270, 67));
    CHECK(m.at(343, 29));
    CHECK_FALSE(m.at(342, 29));
}

TEST_CASE("wing cut: the exact vectors of s5-sheet.md 2.8 (s 33, g 5, D 5)") {
    // Heading 0: cut 1 keeps columns 11 ... 32, cut 0 keeps 0 ... 21, both 11 ... 21.
    CHECK(keeps_columns(wing_keep(kS, kD, 0, true), 11, 32));
    CHECK(keeps_columns(wing_keep(kS, kD, 0, false), 0, 21));
    {
        const auto a = wing_keep(kS, kD, 0, true), b = wing_keep(kS, kD, 0, false);
        int both = 0;
        for (std::size_t i = 0; i < a.size(); ++i) both += a[i] != 0 && b[i] != 0;
        CHECK(both == 363);
    }
    // Heading 8: the mirror image.
    CHECK(keeps_columns(wing_keep(kS, kD, 8, true), 0, 21));
    CHECK(keeps_columns(wing_keep(kS, kD, 8, false), 11, 32));
    // Headings 4 and 12: rows.
    CHECK(keeps_rows(wing_keep(kS, kD, 4, true), 11, 32));
    CHECK(keeps_rows(wing_keep(kS, kD, 4, false), 0, 21));
    CHECK(keeps_rows(wing_keep(kS, kD, 12, true), 0, 21));
    CHECK(keeps_rows(wing_keep(kS, kD, 12, false), 11, 32));
    // The debris of heading 0: cut 1 keeps columns 0 ... 10, cut 0 columns 22 ... 32.
    std::vector<std::uint8_t> sheet(static_cast<std::size_t>(16 * kS + 17 * kG) * (kS + 2 * kG), 9);
    const int W = 16 * kS + 17 * kG, H = kS + 2 * kG;
    const auto d1 = wing_debris_cell(sheet, W, H, kS, kG, kD, 0, true);
    const auto d0 = wing_debris_cell(sheet, W, H, kS, kG, kD, 0, false);
    CHECK(keeps_columns(d1, 0, 10));
    CHECK(keeps_columns(d0, 22, 32));
    // The sheet pass: row 0 of all 16 headings cut, rows below and column 16 untouched.
    const int rows = 2;
    std::vector<std::uint8_t> two(static_cast<std::size_t>(W + kS + kG) * ((kS + kG) * rows + kG), 9);
    const int W2 = W + kS + kG, H2 = (kS + kG) * rows + kG;
    wing_cut_sheet(two, W2, H2, kS, kG, kD, true);
    for (int f = 0; f < 16; ++f) {
        const auto k = wing_keep(kS, kD, f, true);
        for (int y = 0; y < kS; ++y)
            for (int x = 0; x < kS; ++x) {
                const std::uint8_t v = two[static_cast<std::size_t>(kG + y) * W2 + (kS + kG) * f + kG + x];
                CHECK((v != 0) == (k[static_cast<std::size_t>(y) * kS + x] != 0));
            }
    }
    int zeros_elsewhere = 0;
    for (int y = kS + 2 * kG; y < H2; ++y)
        for (int x = 0; x < W2; ++x) zeros_elsewhere += two[static_cast<std::size_t>(y) * W2 + x] == 0;
    for (int y = 0; y < H2; ++y)
        for (int x = 16 * (kS + kG) + kG; x < W2; ++x) zeros_elsewhere += two[static_cast<std::size_t>(y) * W2 + x] == 0;
    CHECK(zeros_elsewhere == 0);
}

TEST_CASE("wing cut: cut 0 is cut 1 turned 180 degrees, for every heading and band") {
    for (const int D : {3, 4, 5, 6, 10}) {
        for (int f = 0; f < 16; ++f) {
            const auto a = wing_keep(kS, D, f, true), b = wing_keep(kS, D, f, false);
            bool same = true;
            for (int y = 0; y < kS; ++y)
                for (int x = 0; x < kS; ++x)
                    same = same && a[static_cast<std::size_t>(y) * kS + x] ==
                                       b[static_cast<std::size_t>(kS - 1 - y) * kS + (kS - 1 - x)];
            CHECK_MESSAGE(same, "D " << D << " heading " << f);
            // The debris keeps what the sheet erases, inside the window.
            std::vector<std::uint8_t> sheet(static_cast<std::size_t>(16 * kS + 17 * kG) * (kS + 2 * kG), 9);
            const auto deb = wing_debris_cell(sheet, 16 * kS + 17 * kG, kS + 2 * kG, kS, kG, D, f, true);
            for (std::size_t i = 0; i < a.size(); ++i) CHECK((deb[i] != 0) == (a[i] == 0));
        }
    }
}

// ---- golden: the oracle's wing frames against the port's sheets

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

// work/oracle/<run> beside IS_ORIGINAL_DIR (work/extracted/game).
std::filesystem::path oracle_dir(const char* run) {
    const auto o = original_dir();
    if (o.empty()) return {};
    std::filesystem::path base = o;
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / run;
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

// A ship's sheet as seg14:0x11a8 renders it on the wave palette (seg10:0x6e76(8, 0)), its
// indices (kBlackness as 0) and the palette.
struct PortSheet {
    ShipSheet sh;
    std::vector<std::uint8_t> idx;
    PaletteArray pal{};
    bool ok = false;
    std::uint32_t rgb(int x, int y) const {
        const std::uint8_t v = idx[static_cast<std::size_t>(y) * sh.width + x];
        return v == 0 ? 0u : pal[v].rgb().rgb();
    }
};

PortSheet port_sheet(const std::filesystem::path& dir, const char* ship, int mode) {
    PortSheet p;
    is::assets::AssetStore store(dir);
    const is::assets::Ships ships = store.ships(dir);
    const is::fmt::ShipRecord* r = ships.find(ship);
    if (!r) return p;
    const is::fmt::BodyRecord* b = ships.body(*r);
    if (!b) return p;
    const is::fmt::Dib body = store.sprite(b->bitmap_id);
    PaletteManager pm;
    const is::fmt::Palette5500 p5 = store.palette5500();
    Colours5500 c5{};
    for (std::size_t i = 0; i < c5.size(); ++i) c5[i] = {p5.colours[i].r, p5.colours[i].g, p5.colours[i].b};
    pm.set_colours_5500(c5);
    const PalHandle pal = pm.create(0);
    pm.load_scheme(pal, 8, 0);
    SheetArgs a;
    a.scheme_a = r->byte_07;
    a.scheme_b = r->byte_08;
    a.mode = mode;
    p.sh = ship_sheet_for(body, b->bitmap_id, pm, pal, a);
    if (!p.sh.ok()) return p;
    p.idx.resize(p.sh.px.size());
    for (std::size_t k = 0; k < p.sh.px.size(); ++k)
        p.idx[k] = p.sh.px[k] < 0 ? std::uint8_t{0} : static_cast<std::uint8_t>(p.sh.px[k] & 0xff);
    p.pal = *pm.working(pal);
    p.ok = true;
    return p;
}

struct CellMatch {
    int heading = -1, diff = 1 << 30, outside = 0, lit = 0;
};

// The 33 x 33 cell of `crop` at (ox, oy) against row 0's cell of every heading of `s`; `outside`
// counts the crop's non-black pixels the cut erased at the best heading.
CellMatch match_cell(const PortSheet& s, const pngtest::Image& crop, int ox, int oy, int D, int cuts) {
    CellMatch m;
    for (int d = 0; d < 16; ++d) {
        int diff = 0, lit = 0;
        for (int y = 0; y < kS; ++y)
            for (int x = 0; x < kS; ++x) {
                const std::uint32_t a = s.rgb(s.sh.cell_x(d) + x, s.sh.cell_y(0) + y);
                const std::uint32_t b = crop.at(ox + x, oy + y);
                lit += a != 0;
                diff += a != b;
            }
        if (diff < m.diff) m = {d, diff, 0, lit};
    }
    if (m.heading < 0) return m;
    std::vector<std::uint8_t> keep(static_cast<std::size_t>(kS) * kS, 1);
    if (cuts & 1) {
        const auto k = wing_keep(kS, D, m.heading, true);
        for (std::size_t i = 0; i < keep.size(); ++i) keep[i] = keep[i] && k[i];
    }
    if (cuts & 2) {
        const auto k = wing_keep(kS, D, m.heading, false);
        for (std::size_t i = 0; i < keep.size(); ++i) keep[i] = keep[i] && k[i];
    }
    for (int y = 0; y < kS; ++y)
        for (int x = 0; x < kS; ++x)
            if (crop.at(ox + x, oy + y) != 0 && keep[static_cast<std::size_t>(y) * kS + x] == 0) ++m.outside;
    return m;
}

pngtest::Image crop_of(const std::filesystem::path& png, int x0, int y0, int w, int h) {
    const pngtest::Image full = pngtest::read(png.string());
    pngtest::Image c;
    if (full.w < x0 + w || full.h < y0 + h) return c;
    c.w = w;
    c.h = h;
    c.rgb.resize(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) c.rgb[static_cast<std::size_t>(y) * w + x] = full.at(x0 + x, y0 + y);
    return c;
}

}  // namespace

TEST_CASE("wing cut golden: core-0001's torn TRANSFORMER against the port's cut sheet") {
    const auto dir = original_dir();
    const auto od = oracle_dir("core-0001");
    if (dir.empty() || od.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR or work/oracle/core-0001 not available");
        return;
    }
    PortSheet s = port_sheet(dir, "TRANSFORMER", 1);  // mode 1: the shield body
    REQUIRE(s.ok);
    REQUIRE(s.sh.cell == kS);
    REQUIRE(s.sh.gap == kG);
    // 35-f1-20: wing +0xac torn (cut 1), heading 2 [ORACLE core-0001] (verified claim 21).
    {
        const pngtest::Image crop = crop_of(od / "hit" / "35-f1-20.png", 512, 341, kS, kS);
        REQUIRE(crop.w == kS);
        PortSheet c = s;
        wing_cut_sheet(c.idx, c.sh.width, c.sh.height, kS, kG, kD, true);
        const CellMatch m = match_cell(c, crop, 0, 0, kD, 1);
        MESSAGE("35-f1-20: heading " << m.heading << ", " << m.diff << " pixels differ, " << m.outside
                                     << " lit outside the kept region");
        CHECK(m.heading == 2);
        CHECK(m.outside == 0);
        CHECK(m.diff == 0);
    }
    // 36-f1-30: both wings torn, heading 3 [ORACLE core-0001].
    {
        const pngtest::Image crop = crop_of(od / "hit" / "36-f1-30.png", 512, 341, kS, kS);
        REQUIRE(crop.w == kS);
        PortSheet c = s;
        wing_cut_sheet(c.idx, c.sh.width, c.sh.height, kS, kG, kD, true);
        wing_cut_sheet(c.idx, c.sh.width, c.sh.height, kS, kG, kD, false);
        const CellMatch m = match_cell(c, crop, 0, 0, kD, 3);
        MESSAGE("36-f1-30: heading " << m.heading << ", " << m.diff << " pixels differ, " << m.outside
                                     << " lit outside the kept region");
        CHECK(m.heading == 3);
        CHECK(m.outside == 0);
        CHECK(m.diff == 0);
    }
}

TEST_CASE("wing cut golden: core-0006's SKY TALON at its second wing loss and after the repair") {
    const auto dir = original_dir();
    const auto od = oracle_dir("core-0006");
    if (dir.empty() || od.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR or work/oracle/core-0006 not available");
        return;
    }
    PortSheet s = port_sheet(dir, "SKY TALON", 0);  // a flame ship without the shield: mode 0
    REQUIRE(s.ok);
    REQUIRE(s.sh.cell == kS);
    const int D = 4;  // TALON (BOD 5) +0x1a = 9 at Size 1 [BYTES MASTER.BOD]
    // The flame paste's 9 x 9 box per heading (world/player_sheet.h flame_pastes; TALON's BOD
    // +0x19 = 24): the flail held Up, so the oracle's cells carry the flame there [ORACLE
    // core-0006]; the sheet pass does not touch it.
    const is::world::FlameAnchors fa = is::world::flame_anchors(kS, is::world::flame_radius_word(24, kS), 1);
    const auto boxes = is::world::flame_pastes(9, fa, 0, 1, 0, 0, kS, kG, 1);
    REQUIRE(boxes.size() == 16);
    // The cells: 128 x 128 crops from (464, 293), the ship's cell at (48, 48) [ORACLE core-0006].
    PortSheet one = s, none = s;
    wing_cut_sheet(one.idx, one.sh.width, one.sh.height, kS, kG, D, true);  // wing +0xac first
    wing_cut_sheet(none.idx, none.sh.width, none.sh.height, kS, kG, D, true);
    wing_cut_sheet(none.idx, none.sh.width, none.sh.height, kS, kG, D, false);
    // c0029 holds the torn wing's piece over the cell's top left (oracle-lit pixels the port has
    // black), so only the port's lit pixels are compared there. c0030 is left out: the piece's
    // masked erase (seg8:0x8a6, SRCAND from the strip's mask) blacks part of the ship's band.
    struct Case {
        const char* name;
        const PortSheet* sheet;
        int cuts;
        int heading;
        bool piece;
    };
    const Case cases[] = {{"sky-wing2-c0027.png", &one, 1, 12, false}, {"sky-wing2-c0029.png", &none, 3, 5, true},
                          {"sky-wing2-c0031.png", &none, 3, 5, false}, {"v1-back-cell.png", &s, 0, 4, false},
                          {"v2-back-cell.png", &s, 0, 11, false}};
    for (const Case& c : cases) {
        const pngtest::Image crop = crop_of(od / "picture" / c.name, 0, 0, 128, 128);
        REQUIRE(crop.w == 128);
        const CellMatch m = match_cell(*c.sheet, crop, 48, 48, D, c.cuts);
        CHECK(m.heading == c.heading);
        if (m.heading < 0) continue;
        const is::world::FlamePaste& b = boxes[static_cast<std::size_t>(m.heading)];
        const int bx = b.dx - c.sheet->sh.cell_x(m.heading), by = b.dy - c.sheet->sh.cell_y(0);
        int diff = 0, lit_diff = 0;
        std::string g;
        for (int y = 0; y < kS; ++y) {
            for (int x = 0; x < kS; ++x) {
                const std::uint32_t a = c.sheet->rgb(c.sheet->sh.cell_x(m.heading) + x, c.sheet->sh.cell_y(0) + y);
                const std::uint32_t o = crop.at(48 + x, 48 + y);
                const bool in_box = x >= bx && x < bx + 9 && y >= by && y < by + 9;
                g += in_box ? (a == o ? ',' : '#') : (a == o ? (a ? 'o' : '.') : (a == 0 ? '+' : (o == 0 ? '-' : 'x')));
                if (in_box || a == o) continue;
                ++diff;
                lit_diff += a != 0;
            }
            g += '\n';
        }
        MESSAGE(std::string(c.name) << ": heading " << m.heading << ", flame box at (" << bx << ", " << by << "), " << diff
                       << " pixels differ outside it (" << lit_diff << " lit in the port), " << m.outside
                       << " oracle pixels lit outside the kept region");
        if (std::getenv("IS_WING_DUMP")) MESSAGE(g);
        CHECK(lit_diff == 0);
        if (!c.piece) {
            CHECK(diff == 0);
            CHECK(m.outside == 0);
        }
    }
}
