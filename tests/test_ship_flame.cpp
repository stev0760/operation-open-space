// The thrust flame (M5 s2 FLAME): ISPACE.DLL ordinal 18 (src/gfx/ship_flame.h), seg10:0x7032's
// mirrors, seg10:0x16ba's anchors, seg10:0x5d3c's strip and seg11:0xb09c's paste
// (src/world/player_sheet.h), on synthetic inputs, and TRANSFORMER's flame against the oracle:
// the 33 x 33 player cell at (512, 341) of work/oracle/wave-0001 05-wave (before), 06-fly-up
// (flame on, heading 6), 07-fly-left (heading 2 after the flame-off) and 08-fly-right (heading 6
// after the flame-off, with the mirrored patch's trace), and of wave-0001-run2 06-fly-up (flame
// on, heading 0) [ORACLE wave-0001]. The goldens read IS_ORIGINAL_DIR and work/oracle beside it,
// and skip without them.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/palette.h"
#include "gfx/ship_flame.h"
#include "gfx/ship_sheet.h"
#include "png_decode.h"
#include "world/player_sheet.h"
#include "world/sprite.h"
#ifdef _WIN32
#include "gfx/gfx.h"
#endif

using namespace is;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

// work/oracle/<run> beside IS_ORIGINAL_DIR (work/extracted/game), as test_ship_sheet.cpp
// finds it.
std::filesystem::path oracle_run(const char* run) {
    const auto o = original_dir();
    if (o.empty()) return {};
    std::filesystem::path base = o;
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / run;
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

// The logical index as the screen shows it: 0 black (the port's BLACKNESS index), else the
// working entry.
std::uint32_t rgb_of(const gfx::PaletteArray& pal, std::uint8_t v) {
    return v == 0 ? 0u : pal[v].rgb().rgb();
}

}  // namespace

TEST_CASE("flame: ordinal 18's tables and seg10:0x5c42's cell") {
    CHECK(gfx::kFlameT1[0] == 0);
    CHECK(gfx::kFlameT1[4] == 512);
    CHECK(gfx::kFlameT2[0] == -512);
    CHECK(gfx::kFlameT2[3] == -196);
    CHECK(gfx::kFlameOffX[1] == -1);
    CHECK(gfx::kFlameOffY[1] == 2);
    CHECK(gfx::flame_cell(0) == 4);
    CHECK(gfx::flame_cell(1) == 9);
    CHECK(gfx::flame_cell(2) == 18);
    CHECK(gfx::flame_cell(1, true) == 9);
    CHECK(gfx::flame_cell(2, true) == 15);
}

TEST_CASE("flame: seg5:0x64f8 keeps the first entry within 14, else the nearest") {
    gfx::PaletteArray p{};
    p[30].set({100, 0, 0});
    p[31].set({200, 0, 0});
    p[32].set({205, 0, 0});
    gfx::FlamePalette fp;
    fp.working = &p;
    fp.scan[0] = {30, 32};
    int last = 10;
    CHECK(gfx::nearest_flame_entry(fp, {204, 0, 0}, last) == 31);  // 4 < 14 at 31: stops
    CHECK(gfx::nearest_flame_entry(fp, {150, 0, 0}, last) == 30);  // 50 = 50: the first
    CHECK(gfx::nearest_flame_entry(fp, {255, 0, 0}, last) == 32);
    fp.mode2 = true;
    CHECK(gfx::nearest_flame_entry(fp, {255, 0, 0}, last) == 0);
}

TEST_CASE("flame: seg10:0x7032 mirrors 5 directions to 16 headings") {
    gfx::FlameBitmap b;
    b.cell = 2;
    b.dirs = 5;
    b.width = 10;
    b.height = 4;
    b.px.assign(40, 0);
    for (int k = 0; k < 5; ++k) b.px[static_cast<std::size_t>(2 * k)] = static_cast<std::uint8_t>(k + 1);  // top-left
    const gfx::FlameHeadings h = gfx::flame_headings(b, 2);
    REQUIRE(h.ok());
    CHECK(h.width == 32);
    CHECK(h.height == 4);
    CHECK(h.at(0, 0) == 1);
    CHECK(h.at(16, 1) == 1);   // heading 8: vertical flip of 0
    CHECK(h.at(25, 0) == 5);   // heading 12: horizontal flip of 4
    CHECK(h.at(14, 1) == 2);   // heading 7: vertical flip of 1
    CHECK(h.at(19, 1) == 2);   // heading 9: 180 degrees of 1
    CHECK(h.at(31, 0) == 2);   // heading 15: horizontal flip of 1
}

TEST_CASE("flame: the design words and seg10:0x16ba's anchors for TRANSFORMER at Size 1") {
    // MASTER.SHP TRANSFORMER +0x09 = 0x15, BOD 33 +0x19 = 23, raw side 33 [BYTES MASTER.SHP]
    // [BYTES MASTER.BOD].
    const world::FlameDesign d = world::flame_design(0x15, 0x00, 23);
    CHECK(d.type == 0);
    CHECK_FALSE(d.behind);
    CHECK(world::flame_design(0x14, 0x08, 0).type == 1);
    CHECK(world::flame_design(0x14, 0x08, 0).behind);
    CHECK(world::flame_design(0x21, 0, 0).type == 0x20);
    CHECK(world::flame_radius_word(23, 33) == 11);
    CHECK(world::flame_radius_word(-3, 33) == 3);
    const world::FlameAnchors a = world::flame_anchors(33, 11, 1);
    const int want[5][2] = {{12, 23}, {8, 23}, {4, 20}, {1, 15}, {1, 12}};
    for (int k = 0; k < 5; ++k) {
        CAPTURE(k);
        CHECK(a.d[static_cast<std::size_t>(k)].x == want[k][0]);
        CHECK(a.d[static_cast<std::size_t>(k)].y == want[k][1]);
    }
}

TEST_CASE("flame: seg11:0xb09c's paste boxes") {
    const world::FlameAnchors a = world::flame_anchors(33, 11, 1);
    // Type 0: every heading into row 0; (ox, oy) as s2-flame.md lists them.
    const int want[16][2] = {{12, 23}, {8, 23}, {4, 20}, {1, 15}, {1, 12}, {1, 9},  {4, 4},  {8, 1},
                             {12, 1},  {16, 1}, {20, 4}, {23, 9}, {23, 12}, {23, 15}, {20, 20}, {16, 23}};
    const auto v = world::flame_pastes(9, a, 0, 1, 0, 6, 33, 5, 1);
    REQUIRE(v.size() == 16);
    for (int h = 0; h < 16; ++h) {
        CAPTURE(h);
        CHECK(v[static_cast<std::size_t>(h)].dx == 38 * h + 5 + want[h][0]);
        CHECK(v[static_cast<std::size_t>(h)].dy == 5 + want[h][1]);
        CHECK(v[static_cast<std::size_t>(h)].sx == 9 * h);
        CHECK(v[static_cast<std::size_t>(h)].sy == 9);
    }
    // Ship 2, off: strip row 4. Type 7: row 1.
    const auto w = world::flame_pastes(9, a, 2, 0, 7, 0, 33, 5, 1);
    CHECK(w[0].sy == 36);
    CHECK(w[0].dy == 38 + 5 + 23);
    // Types 1 ... 6: only the heading, into column 16, twice.
    const auto c = world::flame_pastes(9, a, 0, 1, 3, 2, 33, 5, 1);
    REQUIRE(c.size() == 2);
    CHECK(c[0].dx == 16 * 33 + 17 * 5 + 4);
    CHECK(c[0].dy == 5 + 20);
    CHECK(c[1].dy == 10 + 33 + 20);
    CHECK(c[0].sx == 18);
    // A body-thrust ship (+0xe4 = 0): nothing.
    CHECK(world::flame_pastes(9, a, 0, 1, 0, 2, 33, 5, 0).empty());
}

TEST_CASE("flame: seg10:0x5d3c composites the flame over the patch, in front and behind") {
    // A 1-cell flame bitmap of f = 2: direction k has value 50 + k at (0, 0) and black elsewhere.
    gfx::FlameBitmap b;
    b.cell = 2;
    b.dirs = 5;
    b.width = 10;
    b.height = 4;
    b.px.assign(40, 0);
    for (int k = 0; k < 5; ++k) b.px[static_cast<std::size_t>(2 * k)] = static_cast<std::uint8_t>(50 + k);
    const gfx::FlameHeadings fh = gfx::flame_headings(b, 2);
    // A sheet of size 4, gap 1, filled with 7 (the ship) except 0 at its very first pixel row.
    world::SheetPixels sh;
    sh.w = 16 * 4 + 17;
    sh.h = 10;
    sh.index.assign(static_cast<std::size_t>(sh.w) * sh.h, 7);
    world::FlameAnchors a;
    for (auto& d : a.d) d = {1, 1};
    world::FlameStrip st;
    world::flame_strip_create(st, 2);
    CHECK(st.w == 32);
    world::FlameDesign front;
    world::build_flame_strip(st, fh, 0, sh, 4, 1, front, a, 1);
    CHECK(st.at(0, 0) == 7);   // row 0: the patch
    CHECK(st.at(0, 2) == 50);  // row 1: the flame where it is lit
    CHECK(st.at(1, 2) == 7);   // and the patch elsewhere
    CHECK(st.at(18, 2) == 7);  // heading 9 = 180 degrees of 1: the flame at (1, 1)
    CHECK(st.at(19, 3) == 51);
    // Behind: the flame only where ship 0's row 0 is black; the patch is lit everywhere here.
    world::FlameDesign behind;
    behind.behind = true;
    world::build_flame_strip(st, fh, 0, sh, 4, 1, behind, a, 1);
    CHECK(st.at(0, 2) == 7);
    // A body-thrust ship leaves its rows to the mirror alone.
    world::FlameStrip st2;
    world::flame_strip_create(st2, 2);
    world::build_flame_strip(st2, fh, 1, sh, 4, 1, front, a, 0);
    CHECK(st2.at(0, 4) == 0);
    CHECK(st2.at(0, 6) == 0);
    // The trim: an anchor of (-1, 3) in a size-4 cell blacks the left column and the bottom row.
    for (auto& d : a.d) d = {-1, 3};
    world::FlameStrip st3;
    world::flame_strip_create(st3, 2);
    world::build_flame_strip(st3, fh, 0, sh, 4, 1, front, a, 1);
    CHECK(st3.at(0, 2) == 0);  // left: max(0, 1) column
    CHECK(st3.at(1, 2) == 7);
    CHECK(st3.at(1, 3) == 0);  // bottom: from y + 1, height 1
}

TEST_CASE("flame: TRANSFORMER's flame against the wave-0001 oracle (golden)") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const auto od = oracle_run("wave-0001");
    if (od.empty()) {
        MESSAGE("skipped: work/oracle/wave-0001 not found");
        return;
    }
    assets::AssetStore store(dir);
    const assets::Ships ships = store.ships(dir);
    const fmt::ShipRecord* r = ships.find("TRANSFORMER");
    REQUIRE(r);
    const fmt::BodyRecord* bod = ships.body(*r);
    REQUIRE(bod);
    // The wave palette: kind 0, seg10:0x6e76(8, 0) [BYTES seg10:0x37].
    gfx::PaletteManager pm;
    const fmt::Palette5500 p5 = store.palette5500();
    gfx::Colours5500 c5{};
    for (std::size_t i = 0; i < c5.size(); ++i) c5[i] = {p5.colours[i].r, p5.colours[i].g, p5.colours[i].b};
    pm.set_colours_5500(c5);
    const gfx::PalHandle pal = pm.create(0);
    pm.load_scheme(pal, 8, 0);

    // The sheet as stage 1 renders it (mode 1, Size 1) [BYTES seg14:0x129c-0x12d0].
    gfx::SheetArgs sa;
    sa.gap = 99;
    sa.four = 1;
    sa.scale = 0;
    sa.scheme_a = r->byte_07;
    sa.scheme_b = r->byte_08;
    sa.mode = 1;
    const fmt::Dib body = store.sprite(bod->bitmap_id);
    const gfx::ShipSheet sh = gfx::ship_sheet_for(body, bod->bitmap_id, pm, pal, sa);
    REQUIRE(sh.ok());
    world::SheetPixels px;
    REQUIRE(world::sheet_pixels_from(sh, px));

    // Ordinal 18 and seg10:0x7032 at Size 1.
    const fmt::Dib fdib = store.sprite(gfx::kFlameGraphic);
    const gfx::FlameBitmap fb = gfx::flame_bitmap_for(fdib, pm, pal);
    REQUIRE(fb.ok());
    CHECK(fb.width == 45);
    CHECK(fb.height == 18);
    // Direction 0 and 2 of row 0, as s2-flame.md lists them (rows 1 and 2).
    const int dir0_row1[9] = {0, 0, 58, 51, 50, 51, 58, 0, 0};
    const int dir2_row1[9] = {0, 0, 61, 47, 35, 32, 35, 60, 0};
    for (int x = 0; x < 9; ++x) {
        CAPTURE(x);
        CHECK(fb.at(x, 1) == dir0_row1[x]);
        CHECK(fb.at(18 + x, 1) == dir2_row1[x]);
    }
    // Row 1: the same remapped onto ramp 0 ((v - 28) 16 / 34 + 11).
    CHECK(fb.at(2, 10) == (58 - 28) * 16 / 34 + 11);
    const int f = gfx::flame_cell(1);
    const gfx::FlameHeadings fh = gfx::flame_headings(fb, f);
    REQUIRE(fh.ok());

    // seg10:0x5d3c(0) after the sheet.
    const std::int16_t size = static_cast<std::int16_t>(sh.cell_size()), gap = static_cast<std::int16_t>(sh.gap);
    REQUIRE(size == 33);
    REQUIRE(gap == 5);
    const world::FlameDesign d = world::flame_design(r->flags, bod->flags, bod->byte_19);
    const world::FlameAnchors a =
        world::flame_anchors(33, world::flame_radius_word(bod->byte_19, 33), 1);
    world::FlameStrip st;
    world::flame_strip_create(st, f);
    world::build_flame_strip(st, fh, 0, px, size, gap, d, a, 1);
    // The game's path (src/hud/wave_win32.cpp build_player_sheet): ship_flame_after_sheet derives
    // the anchors' sides from SheetBuild::side and gives the same strip (review L10).
    {
        world::SheetBuild wb;
        wb.rendered = true;
        wb.side = 33;
        wb.pixels = px;
        world::ObjectRecord wo{};
        wo.size = size;
        wo.gap = gap;
        world::ShipRecord ws{};
        ws.has_flame = 1;
        world::RecordingBlitter wrec;
        world::ship_flame_after_sheet(wrec, fh, 0, wb, wo, ws, d, 1);
        const world::FlameStrip& gs = world::flame_strip();
        REQUIRE(gs.ok());
        CHECK(gs.f == st.f);
        for (int k = 0; k < 5; ++k) {
            CAPTURE(k);
            CHECK(gs.anchors[0].d[static_cast<std::size_t>(k)].x == a.d[static_cast<std::size_t>(k)].x);
            CHECK(gs.anchors[0].d[static_cast<std::size_t>(k)].y == a.d[static_cast<std::size_t>(k)].y);
        }
        CHECK(gs.px == st.px);
    }

    const gfx::PaletteArray& wp = *pm.working(pal);
    // The cell of heading h, row 0, against the screen at (cx, cy).
    const auto cell_diff = [&](const world::SheetPixels& s, int h, const pngtest::Image& img, int cx,
                               int cy, std::string* dump) {
        int diff = 0;
        for (int y = 0; y < 33; ++y)
            for (int x = 0; x < 33; ++x) {
                const std::uint32_t a1 =
                    rgb_of(wp, s.index[static_cast<std::size_t>(gap + y) * s.w + h * (size + gap) + gap + x]);
                const std::uint32_t b1 = img.at(cx + x, cy + y);
                if (a1 == b1) continue;
                ++diff;
                if (dump) {
                    char buf[64];
                    std::snprintf(buf, sizeof buf, "(%d,%d) port %06x oracle %06x\n", x, y, a1, b1);
                    *dump += buf;
                }
            }
        return diff;
    };
    const int cx = 512, cy = 341;
    std::string dump;
    {
        const pngtest::Image i05 = pngtest::read((od / "05-wave.png").string());
        CHECK(cell_diff(px, 6, i05, cx, cy, nullptr) == 0);
    }
    // Flame on at thrust start (seg11:0x641f): every heading pasted from strip row 1.
    world::SheetPixels on = px;
    world::apply_flame_pastes(on, st, world::flame_pastes(f, a, 0, 1, 0, 6, size, gap, 1));
    {
        const pngtest::Image i06 = pngtest::read((od / "06-fly-up.png").string());
        const int n = cell_diff(on, 6, i06, cx, cy, &dump);
        MESSAGE("06-fly-up, heading 6, flame on: " << n << " of 1089 cell pixels differ");
        if (n && std::getenv("IS_SHEET_DUMP")) MESSAGE(dump);
        CHECK(n == 0);
        // The flame is drawn: the cell differs from the pristine one.
        CHECK(cell_diff(px, 6, i06, cx, cy, nullptr) == 52);
    }
    // Flame off at release (seg11:0x6039): strip row 0 back; headings 0 ... 4 are restored and
    // 5 ... 15 keep the mirrored patches.
    world::SheetPixels off = on;
    world::apply_flame_pastes(off, st, world::flame_pastes(f, a, 0, 0, 0, 2, size, gap, 1));
    {
        const pngtest::Image i07 = pngtest::read((od / "07-fly-left.png").string());
        CHECK(cell_diff(off, 2, i07, cx, cy, nullptr) == 0);
        const pngtest::Image i08 = pngtest::read((od / "08-fly-right.png").string());
        dump.clear();
        const int n = cell_diff(off, 6, i08, cx, cy, &dump);
        MESSAGE("08-fly-right, heading 6, after the flame-off: " << n << " of 1089 cell pixels differ");
        if (n && std::getenv("IS_SHEET_DUMP")) MESSAGE(dump);
        CHECK(n == 0);
        CHECK(cell_diff(px, 6, i08, cx, cy, nullptr) == 22);  // the trace of the first flame-off
    }
    // wave-0001-run2 starts heading up (heading 0) and thrusts.
    const auto od2 = oracle_run("wave-0001-run2");
    if (!od2.empty() && std::filesystem::exists(od2 / "06-fly-up.png")) {
        const pngtest::Image r06 = pngtest::read((od2 / "06-fly-up.png").string());
        dump.clear();
        const int n = cell_diff(on, 0, r06, cx, cy, &dump);
        MESSAGE("run2 06-fly-up, heading 0, flame on: " << n << " of 1089 cell pixels differ");
        if (n && std::getenv("IS_SHEET_DUMP")) MESSAGE(dump);
        CHECK(n == 0);
    }
}

#ifdef _WIN32
// seg11:0xb09c's BitBlt SRCCOPY from the strip DC into the sheet DC copies the palette indices
// on the port's 8-bpp DIB sections, as the pixel path does [DESIGN ADR 0013].
TEST_CASE("flame: the GDI paste copies the strip's indices into the sheet") {
    gfx::Gfx g(nullptr);
    g.set_client_size(64, 48, 1);
    world::GdiBlitter b(g);
    world::SheetPixels sh;
    sh.w = 16 * 33 + 17 * 5;
    sh.h = 2 * 33 + 3 * 5;
    sh.index.assign(static_cast<std::size_t>(sh.w) * sh.h, 0);
    for (std::size_t k = 0; k < sh.index.size(); ++k) sh.index[k] = static_cast<std::uint8_t>(11 + k % 90);
    world::ImageSlotTable images{};
    images[world::kShipImageSlotBase] = b.create_image_from(sh.w, sh.h, sh.index);
    REQUIRE(images[world::kShipImageSlotBase] != world::kDcNone);
    world::FlameStrip st;
    world::flame_strip_create(st, 9);
    for (std::size_t k = 0; k < st.px.size(); ++k) st.px[k] = static_cast<std::uint8_t>(28 + k % 35);
    world::upload_flame_strip(b, st);
    REQUIRE(st.dc != world::kDcNone);
    const world::FlameAnchors a = world::flame_anchors(33, 11, 1);
    const auto pastes = world::flame_pastes(9, a, 0, 1, 0, 6, 33, 5, 1);
    world::paste_flame(b, images, st, 0, pastes);
    world::apply_flame_pastes(sh, st, pastes);
    const auto im = b.image(images[world::kShipImageSlotBase]);
    REQUIRE(im);
    im->flush();
    int diff = 0;
    for (int y = 0; y < sh.h; ++y)
        for (int x = 0; x < sh.w; ++x) diff += im->at(x, y) != sh.index[static_cast<std::size_t>(y) * sh.w + x];
    CHECK(diff == 0);
}
#endif

TEST_CASE("flame: the program's strip after the sheet and the flame_tail paste") {
    // A flame of f = 9 lit everywhere in direction 0 (value 40), and a sheet of TRANSFORMER's
    // measures.
    gfx::FlameBitmap fb;
    fb.cell = 9;
    fb.dirs = 5;
    fb.width = 45;
    fb.height = 18;
    fb.px.assign(45u * 18u, 40);
    const gfx::FlameHeadings fh = gfx::flame_headings(fb, 9);
    world::SheetBuild b;
    b.rendered = true;
    b.side = 33;
    b.pixels.w = 16 * 33 + 17 * 5 + 38;
    b.pixels.h = 2 * 33 + 3 * 5;
    b.pixels.index.assign(static_cast<std::size_t>(b.pixels.w) * b.pixels.h, 12);
    world::ObjectRecord o{};
    o.size = 33;
    o.gap = 5;
    o.frame = 6;
    world::ShipRecord s{};
    s.has_flame = 1;
    s.type = 0;
    world::RecordingBlitter rec;
    world::ship_flame_after_sheet(rec, fh, 0, b, o, s, world::flame_design(0x15, 0, 23), 1);
    const world::FlameStrip& st = world::flame_strip();
    REQUIRE(st.ok());
    CHECK(st.f == 9);
    CHECK(st.dc != world::kDcNone);
    CHECK(st.anchors[0].d[2].x == 4);
    CHECK(st.anchors[0].d[2].y == 20);
    CHECK(st.at(0, 0) == 12);  // row 0: the patch
    CHECK(st.at(0, 9) == 40);  // row 1: the flame over it
    world::ImageSlotTable images{};
    images[world::kShipImageSlotBase] = 0x321;
    rec.calls.clear();
    world::ship_flame_tail(rec, images, 0, 1, s, o);
    REQUIRE(rec.calls.size() == 16);
    CHECK(rec.calls[6].dst == 0x321);
    CHECK(rec.calls[6].src == st.dc);
    CHECK(rec.calls[6].rop == world::kSrcCopy);
    CHECK(rec.calls[6].x == 6 * 38 + 5 + 4);
    CHECK(rec.calls[6].y == 5 + 33 - 20 - 9);
    // A body-thrust ship pastes nothing.
    s.has_flame = 0;
    rec.calls.clear();
    world::ship_flame_tail(rec, images, 0, 1, s, o);
    CHECK(rec.calls.empty());
}
