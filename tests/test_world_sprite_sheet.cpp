// The player's sheet and its entry effect (src/world/player_sheet.h, src/world/entry_effect.h;
// work/analysis/m5/s1-sprite.md): the sheet measures of ISPACE.DLL seg6:0x11c, seg14:0x11a8's
// row mode, sel and scale flag, the build into slot 0x28 + i, ship byte +0x5b, the frame and
// row seg7:0x1208 picks in the sheet, and seg12:0x2556 / seg7:0x13b2 / seg7:0x158e against a
// recording Blitter. The golden builds TRANSFORMER's sheet from the original files and skips
// without IS_ORIGINAL_DIR.
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "formats/dib.h"
#include "formats/ships.h"
#include "gfx/palette.h"
#include "gfx/ship_sheet.h"
#include "gfx/surface.h"
#include "shell/rng.h"
#include "world/draw.h"
#include "world/entry_effect.h"
#include "world/flight.h"
#include "world/player_sheet.h"
#include "world/world.h"

using namespace is::world;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return std::filesystem::path(v);
}

}  // namespace

TEST_CASE("player sheet: gap word 99 picks 5 for an odd side, 6 for an even one (ISPACE.DLL seg6:0x2b1)") {
    CHECK(sheet_gap(kSheetGapAuto, 33) == 5);
    CHECK(sheet_gap(kSheetGapAuto, 32) == 6);
    CHECK(sheet_gap(4, 33) == 4);
}

TEST_CASE("player sheet: the bitmap measures of ISPACE.DLL seg6:0x3a83-0x3ae4") {
    // TRANSFORMER: 132 x 33, four frames, mode 1 (Shield) at Size 1: 651 x 157.
    const SheetGeometry g = sheet_geometry(132, 33, kSheetGapAuto, 1, 0);
    CHECK(g.size == 33);
    CHECK(g.gap == 5);
    CHECK(g.frames == 4);
    CHECK(g.rows == 2);       // mode 1 with more than one frame [BYTES ISPACE.DLL seg6:0x2d5]
    CHECK(g.extra == 38);     // (h + g) << s
    CHECK(g.w == 16 * 33 + 17 * 5 + 38);
    CHECK(g.w == 651);
    CHECK(g.h == 157);        // (33 + 5) 4 + 5
    // Mode 0: no extra column, every frame a row.
    const SheetGeometry g0 = sheet_geometry(132, 33, kSheetGapAuto, 0, 0);
    CHECK(g0.w == 613);
    CHECK(g0.rows == 4);
    CHECK(g0.extra == 0);
    // The scale flag doubles both sides, the extra column too.
    const SheetGeometry g1 = sheet_geometry(132, 33, kSheetGapAuto, 1, 1);
    CHECK(g1.w == (613 << 1) + (38 << 1));
    CHECK(g1.h == 157 << 1);
    // One frame in mode 1 renders one row.
    CHECK(sheet_geometry(32, 32, kSheetGapAuto, 1, 0).rows == 1);
}

TEST_CASE("player sheet: cell origins, column = heading, row 0 at y = gap (seg7:0x1338, seg9:0x6624)") {
    CHECK(sheet_cell(0, 0, 33, 5).x == 5);
    CHECK(sheet_cell(0, 0, 33, 5).y == 5);
    CHECK(sheet_cell(3, 1, 33, 5).x == 3 * 38 + 5);
    CHECK(sheet_cell(3, 1, 33, 5).y == 43);
    CHECK(sheet_cell(15, 0, 33, 5).x == 575);
    // seg7:0x1208's frame choice lands on the same cells: types 0 and 7 by heading, row 1 at
    // 2 gap + size for types above 3 [BYTES seg7:0x1338 seg7:0x133d seg7:0x1384].
    for (std::int16_t h = 0; h < 16; ++h) {
        const PlayerCell c0 = player_cell(0, h, 33, 5, 1);
        CHECK(c0.src_x == sheet_cell(h, 0, 33, 5).x);
        CHECK(c0.src_y == sheet_cell(h, 0, 33, 5).y);
        CHECK_FALSE(c0.keep_y);
        const PlayerCell c7 = player_cell(7, h, 33, 5, 0);
        CHECK(c7.src_x == sheet_cell(h, 0, 33, 5).x);
        CHECK(c7.src_y == sheet_cell(h, 1, 33, 5).y);
    }
    // Types 1 ... 6 take column 16, the extra column of mode 1.
    CHECK(player_cell(4, 9, 33, 5, 1).src_x == sheet_cell(16, 0, 33, 5).x);
    CHECK(player_cell(4, 9, 33, 5, 1).src_y == sheet_cell(16, 1, 33, 5).y);
    // A coasting type 0 without the Shield byte keeps +0x12 (= gap from the placement).
    CHECK(player_cell(0, 2, 33, 5, 0).keep_y);
}

TEST_CASE("player sheet: seg14:0x11a8's row mode, sel and seg10:0x6ee6's scale flag") {
    // [BYTES seg14:0x1245-0x1261]
    CHECK(sheet_mode(1, 0) == 0);  // a flame ship
    CHECK(sheet_mode(0, 0) == 2);  // body thrust
    CHECK(sheet_mode(1, 1) == 1);  // Shield
    CHECK(sheet_mode(0, 1) == 1);
    // [BYTES seg14:0x1265-0x1297]
    CHECK(sheet_sel(0, 1, 100, 1, 0) == 0);
    CHECK(sheet_sel(1, 1, 100, 1, 0) == 1);   // Quick 1, in the wave, not the player
    CHECK(sheet_sel(1, 1, 100, 0, 0) == 0);   // the player gets the full renderer
    CHECK(sheet_sel(1, 1, 0, 1, 0) == 0);     // frame 0: the placement
    CHECK(sheet_sel(1, 3, 100, 1, 0) == 0);
    CHECK(sheet_sel(2, 3, 0, 0, 0) == 1);
    CHECK(sheet_sel(2, 1, 100, 0, 0) == 1);
    // [BYTES seg10:0x6f21-0x6f41]
    CHECK(sheet_scale_flag(1, 0, 0) == 0);
    CHECK(sheet_scale_flag(2, 0, 0) == 1);
    CHECK(sheet_scale_flag(2, -1, 0) == 0);
    CHECK(sheet_scale_flag(2, 0, 1) == 0);
    CHECK(sheet_scale_flag(0, 2, 1) == 1);
}

TEST_CASE("player sheet: ship byte +0x5b from the sheet's height (seg14:0xc3e)") {
    CHECK(sheet_last_row(157, 33, 5) == 3);
    CHECK(sheet_last_row(5 + 38, 33, 5) == 0);
    CHECK(sheet_last_row(10, 0, 0) == 0);
}

namespace {

// A source strip of two 4 x 4 frames: frame f's pixel (x, y) = 16 f + 4 y + x + 1.
std::vector<std::uint8_t> strip2() {
    std::vector<std::uint8_t> s(8 * 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 8; ++x) s[static_cast<std::size_t>(y) * 8 + x] = static_cast<std::uint8_t>(16 * (x / 4) + 4 * y + x % 4 + 1);
    return s;
}

}  // namespace

TEST_CASE("player sheet: the stand-in sheet puts frame r unrotated in each heading of row r") {
    SheetRequest req;
    req.mode = 0;
    const SheetPixels px = unrotated_sheet(strip2(), 8, 4, req);
    const SheetGeometry g = sheet_geometry(8, 4, kSheetGapAuto, 0, 0);
    REQUIRE(px.w == g.w);
    REQUIRE(px.h == g.h);
    CHECK(px.source_height == 4);
    auto at = [&px](int x, int y) { return px.index[static_cast<std::size_t>(y) * px.w + x]; };
    for (int c = 0; c < 16; ++c) {
        const Point16 o0 = sheet_cell(c, 0, 4, 6), o1 = sheet_cell(c, 1, 4, 6);
        CHECK(at(o0.x, o0.y) == 1);
        CHECK(at(o0.x + 3, o0.y + 3) == 16);
        CHECK(at(o1.x, o1.y) == 17);
        CHECK(at(o0.x - 1, o0.y) == 0);  // the gap stays black
    }
    // Scale 1 doubles each pixel.
    req.scale = 1;
    const SheetPixels p2 = unrotated_sheet(strip2(), 8, 4, req);
    auto at2 = [&p2](int x, int y) { return p2.index[static_cast<std::size_t>(y) * p2.w + x]; };
    CHECK(at2(12, 12) == 1);
    CHECK(at2(13, 13) == 1);
    CHECK(at2(14, 12) == 2);
}

TEST_CASE("player sheet: seg14:0x11a8 renders into slot 0x28 + i, then skips while +0xac/+0xad hold") {
    RecordingBlitter out;
    ImageSlotTable images{};
    ShipRecord ship{};
    ship.roster_index = -1;
    ObjectRecord obj{};
    int renders = 0;
    SheetRenderer r = [&renders](const SheetRequest& q, SheetPixels& px) {
        ++renders;
        const SheetGeometry g = sheet_geometry(132, 33, q.gap_word, q.mode, q.scale);
        px.w = g.w;
        px.h = g.h;
        px.index.assign(static_cast<std::size_t>(g.w) * g.h, 7);
        px.source_height = 33;
        return true;
    };
    SheetRequest req;
    req.slot = kShipImageSlotBase + 2;
    req.mode = 1;
    const SheetBuild b = rebuild_ship_sheet(out, images, ship, obj, 2, req, r);
    CHECK(renders == 1);
    CHECK(b.rendered);
    CHECK(b.side == 33);
    CHECK(b.gap == 5);
    CHECK(b.bm_height == 157);
    CHECK(b.to_object);  // no roster record and the slot was empty [BYTES seg14:0x1322]
    REQUIRE(out.images.size() == 1);
    CHECK(out.images[0].w == 651);
    CHECK(out.images[0].h == 157);
    CHECK(out.images[0].pixels.size() == 651u * 157u);
    CHECK(images[kShipImageSlotBase + 2] == out.images[0].id);
    CHECK(obj.w3a == static_cast<std::int16_t>(out.images[0].id));  // [BYTES seg16:0x3645]
    CHECK(sbyte_at(ship, 0xac) == 1);
    CHECK(sbyte_at(ship, 0xad) == 1);
    // Built: seg14:0x11a8 on its own does not render again [BYTES seg14:0x120e-0x1220].
    build_ship_sheet(out, images, ship, 2, req, r);
    CHECK(renders == 1);
    // seg16:0x3468 clears +0xac: the old DC goes (seg10:0x552) and the sheet renders anew, now
    // into the roster bytes' branch's opposite: the slot was not empty.
    const DcId old = images[kShipImageSlotBase + 2];
    const SheetBuild b2 = rebuild_ship_sheet(out, images, ship, obj, 2, req, r);
    CHECK(renders == 2);
    CHECK_FALSE(b2.to_object);
    REQUIRE(out.deleted.size() == 1);
    CHECK(out.deleted[0] == old);
    CHECK(images[kShipImageSlotBase + 2] != old);
    // A roster record sends the values to the roster bytes.
    ShipRecord rs{};
    rs.roster_index = 3;
    ImageSlotTable empty{};
    CHECK_FALSE(build_ship_sheet(out, empty, rs, 0, req, r).to_object);
}

namespace {

// The player in pool 14 record 0 with TRANSFORMER's cell: size 33, gap 5, heading 0, on screen
// at (512, 341), the sheet DC 0x50 in +0x3a.
struct EntryFixture {
    Pools pools;
    ShipTable ships{};
    EntryTable table;
    RecordingBlitter out;
    is::shell::Rng rng;
    std::int16_t dg_9148 = 0;
    struct Hooks : EntryHooks {
        std::vector<int> sounds;
        int menus = 0;
        void sound(int id, int, int, int) override { sounds.push_back(id); }
        void menu_refresh() override { ++menus; }
    } hooks;
    EntryContext c{pools, ships, table, out, rng, dg_9148};

    EntryFixture() {
        clear_entries(table);
        rng.srand(1234);
        ObjectRecord& o = pools.rec(kPoolShips, 0);
        o.size = 33;
        o.gap = 5;
        o.frame = 0;
        o.scr_x = 512;
        o.scr_y = 341;
        o.on_screen = 1;
        o.w3a = 0x50;
        c.hooks = &hooks;
    }
    ObjectRecord& o() { return pools.rec(kPoolShips, 0); }
};

}  // namespace

TEST_CASE("entry effect: blocks of (2 << Size) >> 1 and n = 17 for TRANSFORMER (seg7:0x1417)") {
    CHECK(entry_block(0) == 1);
    CHECK(entry_block(1) == 2);
    CHECK(entry_block(2) == 4);
    CHECK(entry_blocks(33, 1) == 17);
    CHECK(entry_blocks(32, 1) == 16);
    CHECK(entry_blocks(33, 0) == 33);
    CHECK(entry_blocks(66, 2) == 17);
}

TEST_CASE("entry effect: seg12:0x2556 copies the heading's (size + 2 gap)² square into a new buffer") {
    EntryFixture f;
    f.o().frame = 3;
    entry_buffer(f.c, kPoolShips, 0);
    REQUIRE(f.out.images.size() == 1);
    CHECK(f.out.images[0].w == 43);
    CHECK(f.out.images[0].h == 43);
    const DcId buf = f.out.images[0].id;
    CHECK(f.o().alt_dc == buf);
    REQUIRE(f.out.calls.size() == 1);
    CHECK(f.out.calls[0] == bit_blt_call(buf, 0, 0, 43, 43, 0x50, 3 * 38, 0, kSrcCopy));  // [BYTES seg12:0x267d]
    // A second call keeps the buffer.
    entry_buffer(f.c, kPoolShips, 0);
    CHECK(f.out.images.size() == 1);
    // A type above 1 copies from row 1 (y = size + gap) [BYTES seg12:0x262e].
    EntryFixture g;
    g.ships[0].type = 4;
    entry_buffer(g.c, kPoolShips, 0);
    CHECK(g.out.calls[0].sy == 38);
    // seg12:0x26a4 frees it.
    entry_buffer_free(f.c, kPoolShips, 0);
    CHECK(f.o().alt_dc == 0);
    CHECK(f.o().w3e == 0);
    REQUIRE(f.out.deleted.size() == 1);
    CHECK(f.out.deleted[0] == buf);
}

TEST_CASE("entry effect: seg7:0x13b2(2, 14, 0) takes a slot, sets +0x2e, +0xeb and blacks the buffer") {
    EntryFixture f;
    entry_buffer(f.c, kPoolShips, 0);
    const DcId buf = f.o().alt_dc;
    f.out.calls.clear();
    entry_start(f.c, 2, kPoolShips, 0);
    CHECK(f.o().b2e == 2);
    CHECK(f.ships[0].beb == 1);
    CHECK(f.dg_9148 == 1);
    CHECK(f.table[0].pool == kPoolShips);
    CHECK(f.table[0].rec == 0);
    CHECK(f.table[0].n == 17);
    CHECK(f.table[0].total == 289);
    CHECK(f.table[0].count == 0);
    REQUIRE(f.hooks.sounds.size() == 1);
    CHECK(f.hooks.sounds[0] == 0x27);  // [BYTES seg7:0x13f2]
    REQUIRE(f.out.calls.size() == 1);
    CHECK(f.out.calls[0] == pat_blt_call(buf, 0, 0, 43, 43, kBlackness));  // [BYTES seg7:0x1548]
    // +0x2e > 0: a second start returns at once [BYTES seg7:0x13e6].
    entry_start(f.c, 2, kPoolShips, 0);
    CHECK(f.dg_9148 == 1);
    CHECK(f.hooks.sounds.size() == 1);
    CHECK(f.table[1].pool == -1);
}

TEST_CASE("entry effect: seg7:0x13b2 with four slots in use blacks the screen square") {
    EntryFixture f;
    for (EntrySlot& s : f.table) s.pool = 3;
    entry_start(f.c, 2, kPoolShips, 0);
    CHECK(f.o().b2e == 0);
    CHECK(f.dg_9148 == 0);
    REQUIRE(f.out.calls.size() == 1);
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 512, 341, 33, 33, kBlackness));  // [BYTES seg7:0x1580]
}

TEST_CASE("entry effect: seg7:0x158e fills 288 blocks in 29 frames, then blits the cell and ends") {
    EntryFixture f;
    entry_buffer(f.c, kPoolShips, 0);
    const DcId buf = f.o().alt_dc;
    entry_start(f.c, 2, kPoolShips, 0);
    f.out.calls.clear();
    // The same picks with a second generator: rand() % 289, retried on a block already set.
    is::shell::Rng ref;
    ref.srand(1234);
    std::vector<bool> used(289, false);
    std::vector<BlitCall> expect;
    int frames = 0;
    for (int done = 0; done < 289;) {
        ++frames;
        for (int pick = 0; pick < 10 && done < 289; ++pick) {
            int r;
            do r = ref.rand() % 289;
            while (used[static_cast<std::size_t>(r)]);
            used[static_cast<std::size_t>(r)] = true;
            ++done;
            if (done == 289) break;
            const int x = (r % 17) << 1, y = (r / 17) << 1;
            expect.push_back(bit_blt_call(buf, 5 + x, 5 + y, 2, 2, 0x50, 5 + x, 5 + y, kSrcCopy));
        }
    }
    CHECK(frames == 29);
    // The end: the buffer onto the screen square from ((size + gap) heading + gap, gap)
    // [BYTES seg7:0x1a69].
    expect.push_back(bit_blt_call(kDcWindow, 512, 341, 33, 33, buf, 5, 5, kSrcCopy));
    int steps = 0;
    while (f.dg_9148 > 0 && steps < 100) {
        entry_step(f.c);
        ++steps;
        if (steps < 29) {
            CHECK(f.o().b2e == 2);
            CHECK(f.ships[0].beb == 1);
        }
    }
    CHECK(steps == 29);
    CHECK(f.out.calls == expect);
    CHECK(f.o().b2e == 0);
    CHECK(f.ships[0].beb == 0);
    CHECK(f.o().alt_dc == 0);
    CHECK(f.table[0].pool == -1);
    CHECK(f.dg_9148 == 0);
    CHECK(f.hooks.menus == 1);
    REQUIRE(f.hooks.sounds.size() == 2);
    CHECK(f.hooks.sounds[1] == 0x65);  // the player [BYTES seg7:0x1ab2]
    // The generator ran as far as the reference.
    CHECK(f.rng.rand() == ref.rand());
}

TEST_CASE("entry effect: at heading 3 the blocks read the heading's cell and the end blit reads past the buffer") {
    // The original's sources as they stand [BYTES seg7:0x1c62 seg7:0x1a18-0x1a69]: the blocks
    // come from the sheet's heading cell, the end blit takes the buffer at the sheet's x, which
    // lies inside the (size + 2 gap)-square buffer only at heading 0 (review M4). What GDI then
    // copies from outside the bitmap is the device's; the start heading is [OPEN].
    EntryFixture f;
    f.o().frame = 3;
    entry_buffer(f.c, kPoolShips, 0);
    const DcId buf = f.o().alt_dc;
    entry_start(f.c, 2, kPoolShips, 0);
    f.out.calls.clear();
    while (f.dg_9148 > 0) entry_step(f.c);
    REQUIRE(f.out.calls.size() == 289);
    const BlitCall& first = f.out.calls.front();
    CHECK(first.dst == buf);
    CHECK(first.sx == 3 * 38 + 5 + (first.x - 5));
    const BlitCall& last = f.out.calls.back();
    CHECK(last == bit_blt_call(kDcWindow, 512, 341, 33, 33, buf, 3 * 38 + 5, 5, kSrcCopy));
    CHECK(last.sx + 33 > 43);  // outside the 43 x 43 buffer
}

TEST_CASE("entry effect: no 0x65 for a ship that is not the player, none while word +0x2e = 2") {
    EntryFixture f;
    f.c.player = 1;
    entry_buffer(f.c, kPoolShips, 0);
    entry_start(f.c, 2, kPoolShips, 0);
    while (f.dg_9148 > 0) entry_step(f.c);
    CHECK(f.hooks.sounds.size() == 1);
    EntryFixture g;
    g.ships[0].w2e = 2;
    entry_buffer(g.c, kPoolShips, 0);
    entry_start(g.c, 2, kPoolShips, 0);
    while (g.dg_9148 > 0) entry_step(g.c);
    CHECK(g.hooks.sounds.size() == 1);
}

TEST_CASE("player sheet: TRANSFORMER's sheet from the original files (golden)") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::assets::Ships ships = store.ships(dir);
    const is::fmt::ShipRecord* shp = ships.find("TRANSFORMER");
    REQUIRE(shp);
    const is::fmt::BodyRecord* body = ships.body(*shp);
    REQUIRE(body);
    CHECK(body->bitmap_id == 60);
    CHECK(body->flags == 0x06);
    const is::fmt::Dib dib = store.sprite(body->bitmap_id);
    // The ship record's bytes as the set-up derives them: Body Thrust clear, so +0xe4 = 1;
    // Shield set, so +0xe9 = 1 and row mode 1 [BYTES seg14:0x1fab seg14:0x21f2].
    const std::uint8_t has_flame = (body->flags & 0x01) != 0 ? 0 : 1;
    const std::uint8_t be9 = (body->flags & 0x02) != 0 ? 1 : 0;
    SheetRequest req;
    req.graphic_id = body->bitmap_id;
    req.colour_a = shp->byte_07;
    req.colour_b = shp->byte_08;
    req.mode = sheet_mode(has_flame, be9);
    CHECK(req.mode == 1);
    req.sel = sheet_sel(0, 3, 0, 0, 0);
    req.scale = sheet_scale_flag(1, 0, req.sel);
    is::gfx::PaletteManager pm;
    const std::vector<std::uint8_t> src = is::gfx::dib_indices(dib, pm.device());
    RecordingBlitter out;
    ImageSlotTable images{};
    ShipRecord ship{};
    ship.roster_index = -1;
    ship.has_flame = has_flame;
    ObjectRecord obj{};
    SheetRenderer r = [&](const SheetRequest& q, SheetPixels& px) {
        px = unrotated_sheet(src, dib.width, dib.height, q);
        return px.w > 0;
    };
    const SheetBuild b = rebuild_ship_sheet(out, images, ship, obj, 0, req, r);
    REQUIRE(b.rendered);
    CHECK(b.side == 33);
    CHECK(b.gap == 5);
    REQUIRE(out.images.size() == 1);
    CHECK(out.images[0].w == 651);
    CHECK(out.images[0].h == 157);
    CHECK(images[kShipImageSlotBase] == out.images[0].id);
    // The object words of the ship set-up at Size 1 and the frame count +0x5b.
    obj.size = 33;
    obj.gap = 5;
    CHECK(sheet_last_row(b.bm_height, obj.size, obj.gap) == 3);
    // Heading 0's cell is frame 0, and the margins are black.
    const auto& px = out.images[0].pixels;
    int coloured = 0;
    for (int y = 0; y < 33; ++y)
        for (int x = 0; x < 33; ++x) {
            const std::uint8_t v = px[static_cast<std::size_t>(5 + y) * 651 + 5 + x];
            CHECK(v == src[static_cast<std::size_t>(y) * dib.width + x]);
            if (v) ++coloured;
        }
    CHECK(coloured > 100);
    for (int x = 0; x < 651; ++x) CHECK(px[static_cast<std::size_t>(x)] == 0);
}

namespace {

// The wave palette of the ship-sheet golden: kind 0, seg10:0x6e76(8, 0) [BYTES seg10:0x37].
is::gfx::PalHandle wave_palette(is::assets::AssetStore& store, is::gfx::PaletteManager& pm) {
    const is::fmt::Palette5500 p5 = store.palette5500();
    is::gfx::Colours5500 c5{};
    for (std::size_t i = 0; i < c5.size(); ++i) c5[i] = {p5.colours[i].r, p5.colours[i].g, p5.colours[i].b};
    pm.set_colours_5500(c5);
    const is::gfx::PalHandle pal = pm.create(0);
    pm.load_scheme(pal, 8, 0);
    return pal;
}

// The player's request as src/hud/wave_win32.cpp build_player_sheet makes it, Size 1.
SheetRequest player_request(const is::fmt::ShipRecord& shp, const is::fmt::BodyRecord& body,
                            ShipRecord& ship) {
    const is::world::FlightDesign d = flight_design(shp.thruster, shp.armor, body.strength_base,
                                                    body.byte_1c, body.byte_1d, body.flags, shp.flags);
    ObjectRecord tmp{};
    apply_flight_design(ship, tmp, d, false, 0, 0);
    SheetRequest req;
    req.graphic_id = body.bitmap_id;
    req.colour_a = shp.byte_07;
    req.colour_b = shp.byte_08;
    req.mode = sheet_mode(ship.has_flame, ship.be9);
    req.sel = sheet_sel(0, 3, 0, 0, 0);
    req.scale = sheet_scale_flag(1, 0, req.sel);
    return req;
}

}  // namespace

TEST_CASE("player sheet: the render glue through gfx::ship_sheet_for, TRANSFORMER (golden)") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::assets::Ships ships = store.ships(dir);
    const is::fmt::ShipRecord* shp = ships.find("TRANSFORMER");
    REQUIRE(shp);
    const is::fmt::BodyRecord* body = ships.body(*shp);
    REQUIRE(body);
    const is::fmt::Dib dib = store.sprite(body->bitmap_id);
    ShipRecord ship{};
    ship.roster_index = -1;
    const SheetRequest req = player_request(*shp, *body, ship);
    CHECK(ship.be9 == 1);  // BOD 0x06: Shield [BYTES seg14:0x21f2]
    CHECK(ship.be7 == 0);
    CHECK(req.mode == 1);
    const is::gfx::SheetArgs a = sheet_args(req);
    CHECK(a.gap == 99);
    CHECK(a.four == 1);
    CHECK(a.scale == 0);
    CHECK(a.scheme_a == shp->byte_07);
    CHECK(a.scheme_b == shp->byte_08);
    CHECK(a.mode == 1);
    is::gfx::PaletteManager pm;
    const is::gfx::PalHandle pal = wave_palette(store, pm);
    const is::gfx::ShipSheet sh = is::gfx::ship_sheet_for(dib, body->bitmap_id, pm, pal, a);
    REQUIRE(sh.ok());
    SheetRenderer r = [&](const SheetRequest&, SheetPixels& px) { return sheet_pixels_from(sh, px); };
    RecordingBlitter out;
    ImageSlotTable images{};
    ObjectRecord obj{};
    const SheetBuild b = rebuild_ship_sheet(out, images, ship, obj, 0, req, r);
    REQUIRE(b.rendered);
    CHECK(b.side == 33);
    CHECK(b.gap == 5);
    REQUIRE(out.images.size() == 1);
    CHECK(out.images[0].w == 651);
    CHECK(out.images[0].h == 157);
    obj.size = 33;
    obj.gap = 5;
    CHECK(sheet_last_row(b.bm_height, obj.size, obj.gap) == 3);
    // Cell (6, 0), the oracle's wave-start heading, as the renderer left it; BLACKNESS is 0.
    const auto& px = out.images[0].pixels;
    const std::vector<std::int16_t> cell = is::gfx::sheet_cell(sh, 6, 0);
    int lit = 0;
    for (int y = 0; y < 33; ++y)
        for (int x = 0; x < 33; ++x) {
            const std::int16_t v = cell[static_cast<std::size_t>(y) * 33 + x];
            const std::uint8_t want = v < 0 ? 0 : static_cast<std::uint8_t>(v);
            CHECK(px[static_cast<std::size_t>(sh.cell_y(0) + y) * 651 + sh.cell_x(6) + x] == want);
            if (want) ++lit;
        }
    CHECK(lit == 441);  // the lit pixels of the 05-wave crop (test_ship_sheet.cpp)
}

TEST_CASE("ship sheet: seg5:0x66ea classifies colours into ramp ids") {
    using is::gfx::colour_ramp_id;
    using is::gfx::Rgb;
    CHECK(colour_ramp_id(Rgb{128, 128, 128}) == 2);  // grey [BYTES ISPACE.DLL seg5:0x6765]
    CHECK(colour_ramp_id(Rgb{0, 0, 0}) == 2);        // raised to 1, 1, 1
    CHECK(colour_ramp_id(Rgb{255, 0, 0}) == 1);      // r/g, r/b > 10 [BYTES ISPACE.DLL seg5:0x68a5]
    CHECK(colour_ramp_id(Rgb{0, 0, 255}) == 0);      // b > g [BYTES ISPACE.DLL seg5:0x6b39]
    CHECK(colour_ramp_id(Rgb{0, 255, 0}) == 4);      // [BYTES ISPACE.DLL seg5:0x6b5f]
    CHECK(colour_ramp_id(Rgb{0, 255, 255}) == 10);   // [BYTES ISPACE.DLL seg5:0x6b64]
    CHECK(colour_ramp_id(Rgb{255, 255, 0}) == 1);    // r > 210, r - b > 30, g * 100 / r > 85 [BYTES ISPACE.DLL seg5:0x6912]
}

TEST_CASE("player sheet: a scheme-12 ship (EAGLE, the default MainShip) renders lit cells (golden)") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::assets::Ships ships = store.ships(dir);
    const is::fmt::ShipRecord* shp = ships.find("EAGLE");
    REQUIRE(shp);
    CHECK(shp->byte_07 == 12);
    const is::fmt::BodyRecord* body = ships.body(*shp);
    REQUIRE(body);
    const is::fmt::Dib dib = store.sprite(body->bitmap_id);
    ShipRecord ship{};
    ship.roster_index = -1;
    const SheetRequest req = player_request(*shp, *body, ship);
    is::gfx::PaletteManager pm;
    const is::gfx::PalHandle pal = wave_palette(store, pm);
    const is::gfx::ShipSheet sh = is::gfx::ship_sheet_for(dib, body->bitmap_id, pm, pal, sheet_args(req));
    REQUIRE(sh.ok());
    // Without seg5:0x66ea every colour fell to palette entry 0 (review H2); now each heading
    // cell of row 0 carries lit, non-black pixels.
    const auto& work = *pm.working(pal);
    for (int d = 0; d < 16; ++d) {
        const auto cell = is::gfx::sheet_cell(sh, d, 0);
        int coloured = 0;
        for (std::int16_t v : cell)
            if (v > 0 && !work[static_cast<std::size_t>(v)].rgb().black()) ++coloured;
        CHECK(coloured > 50);
    }
}
