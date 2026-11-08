// The sheet side of M4 stage 5, track SHEET (src/world/design_words.h, src/world/player_sheet.h):
// the design words +0x2a, +0x3c, +0x68 ... +0x6e (work/analysis/m4/s5-sheet.md sections 1.1 and 6,
// s5-weapons.md 8 and 9.6, verified claims 4, 41, 42), the port's sheet copies, seg10:0x533a's
// one-weapon load with the animation-4 renderer, the shield's quick cells into column 16
// (seg10:0x7328), the restore re-cut seg14:0xc84-0xcce and the rebuild after a visit
// (seg16:0x35db). The golden reads IS_ORIGINAL_DIR: the body heights against the SHP tank byte.
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/ship_quick.h"
#include "gfx/wing_cut.h"
#include "world/collide.h"
#include "world/design_words.h"
#include "world/player_sheet.h"
#include "world/shots.h"
#include "world/world.h"

using namespace is::world;

TEST_CASE("sheet: seg14:0x2070-0x208b's D and seg14:0x1380-0x13e0's barrel words") {
    // trunc(((sext b << Size) sar 1) / 2)
    CHECK(design_band(10, 0) == 2);
    CHECK(design_band(10, 1) == 5);   // TRANSFORMER (BOD 33) at Size 1
    CHECK(design_band(10, 2) == 10);
    CHECK(design_band(9, 1) == 4);    // TALON (BOD 5)
    CHECK(design_band(-7, 1) == -3);  // toward zero
    DesignWordSet d;
    design_barrels(d, 1, 16, 33, 1);  // SKY TALON at side 33, Size 1 [BYTES MASTER.BOD]
    CHECK(d.w68 == -16);
    CHECK(d.w6a == 0);
    CHECK(d.w6c == 15);
    CHECK(d.w6e == 0);
    design_barrels(d, 4, 6, 33, 1);   // TRANSFORMER
    CHECK(d.w68 == -13);
    CHECK(d.w6a == -10);
    CHECK(d.w6c == 12);
    CHECK(d.w6e == -10);
    // The store and seg8:0x2cd8's recompute.
    reset_design_words();
    CHECK(design_words(0).w3c == 0);
    design_band_recompute(0, 1);  // no body bytes: nothing
    CHECK(design_words(0).w3c == 0);
    set_design_body(0, 10, 4, 6);
    design_band_recompute(0, 1);
    CHECK(design_words(0).w3c == 5);
    CHECK(design_words(9).w3c == 0);  // outside the table: zeros
    // design +0x2a is the store's, not the tank / 40 of the earlier stand-in.
    World w;
    w.design_ratings[0].tank = 40 * 34;
    DesignWordSet e = design_words(0);
    e.w2a = 35;
    set_design_words(0, e);
    CHECK(design_from_world(w, 0).w2a == 35);
    CHECK(design_from_world(w, 0).w3c == 5);
    reset_design_words();
}

namespace {

// A sheet renderer that returns a w x h picture of index 7 with source height 33.
SheetRenderer fixed_render(int w, int h) {
    return [w, h](const SheetRequest&, SheetPixels& px) {
        px.w = w;
        px.h = h;
        px.index.assign(static_cast<std::size_t>(w) * h, 7);
        px.source_height = 33;
        return true;
    };
}

}  // namespace

TEST_CASE("sheet: a rendered build keeps the copy and the request, and writes the barrel words") {
    reset_design_words();
    clear_sheet_copies();
    RecordingBlitter out;
    ImageSlotTable images{};
    ShipRecord ship{};
    ship.roster_index = 0;
    ship.design_index = 0;
    set_design_body(0, 10, 4, 6);
    SheetRequest req;
    req.slot = kShipImageSlotBase;
    req.graphic_id = 60;
    req.size_level = 1;
    const SheetBuild b = build_ship_sheet(out, images, ship, 0, req, fixed_render(651, 157));
    REQUIRE(b.rendered);
    REQUIRE(sheet_copy(0));
    CHECK(sheet_copy(0)->w == 651);
    REQUIRE(sheet_request(0));
    CHECK(sheet_request(0)->graphic_id == 60);
    CHECK(design_words(0).w68 == -13);
    CHECK(design_words(0).w6a == -10);
    // DGROUP:0x95a6 = 0 (the visit's seg16:0x3468): no design tail.
    reset_design_words();
    set_design_body(0, 1, 1, 16);
    req.shp_loaded = false;
    set_byte_at(ship, 0xac, 0);
    build_ship_sheet(out, images, ship, 0, req, fixed_render(651, 157));
    CHECK(design_words(0).w68 == 0);
    // The already-built path (slot set, both wing bytes set): no render, no design tail.
    req.shp_loaded = true;
    const SheetBuild c = build_ship_sheet(out, images, ship, 0, req, fixed_render(651, 157));
    CHECK_FALSE(c.rendered);
    CHECK(design_words(0).w68 == 0);
    reset_design_words();
    clear_sheet_copies();
}

TEST_CASE("sheet: seg10:0x533a loads one weapon, the animation-4 sheet through seg10:0x4412") {
    RecordingBlitter out;
    ImageSlotTable images{};
    Pools pools;
    WeaponWords words;
    // Weapon 10 (REAR-DEPLOYED MINES): W+0x06 = 4, graphic 150, n = 6 [BYTES seg10:0x27f8 seg10:0x27ad].
    words.set_word(10, 0x0a, 1);
    words.set_word(10, 0x06, 4);
    words.set_word(10, -2, 150);
    words.set_word(10, 0x26, 6);
    // Weapon 8 (RADAR-GUIDED): the ship renderer.
    words.set_word(8, 0x0a, 1);
    words.set_word(8, -2, 370);
    int calls = 0, k_seen = -1, pool_seen = -1, id_seen = -1, n_seen = -1;
    const Anim4Renderer anim4 = [&](int k, int pool, int id, int n) {
        ++calls;
        k_seen = k, pool_seen = pool, id_seen = id, n_seen = n;
        return DcId{0x77};
    };
    CHECK(load_weapon_sheet(out, images, pools, words, 10, 0, 1, fixed_render(23, 23), anim4));
    CHECK(calls == 1);
    CHECK(k_seen == 0x30);
    CHECK(pool_seen == 6);
    CHECK(id_seen == 150);
    CHECK(n_seen == 6);
    CHECK(images[0x30] == 0x77);
    CHECK(words.word(10, -4) == 0x30);
    // Without a renderer the slot stays empty.
    ImageSlotTable empty{};
    CHECK_FALSE(load_weapon_sheet(out, empty, pools, words, 10, 0, 1, fixed_render(23, 23)));
    CHECK(empty[0x30] == 0);
    // The ship renderer for the others, into the next empty slot.
    CHECK(load_weapon_sheet(out, images, pools, words, 8, 0, 1, fixed_render(23, 23), anim4));
    CHECK(images[0x31] != 0);
    CHECK(calls == 1);
    // build_weapon_sheets runs the same for each slot l < +0xcb.
    ShipRecord s{};
    set_byte_at(s, 0xcb, 2);
    set_byte_at(s, 0xb8, 8);
    set_byte_at(s, 0xb9, 10);
    ImageSlotTable fresh{};
    words.set_word(8, -4, 0);  // W(w)-4 from the loads above would make seg10:0x533a reuse slot 0x30
    words.set_word(10, -4, 0);
    CHECK(build_weapon_sheets(out, fresh, pools, words, s, 0, 1, fixed_render(23, 23), anim4) == 2);
    CHECK(k_seen == 0x31);
}

namespace {

// Ship 0 at heading 3 with a 651 x 157 sheet of index 7 in slot 0x28 (DC 0x50) and a kept request.
struct ShieldRig {
    World w;
    RecordingBlitter out;
    ImageSlotTable images{};
    ShieldRig() {
        reset_design_words();
        clear_sheet_copies();
        w.size = 1;
        w.pools.set_in_use(kPoolShips, 1);
        ObjectRecord& o = w.pools.rec(kPoolShips, 0);
        o.size = 33;
        o.gap = 5;
        o.frame = 3;
        SheetRequest req;
        req.slot = kShipImageSlotBase;
        req.graphic_id = 60;
        req.colour_a = 0;
        req.colour_b = 10;
        build_ship_sheet(out, images, w.ship(0), 0, req, fixed_render(651, 157));
        images[kShipImageSlotBase] = 0x50;
        out.calls.clear();
        out.images.clear();
    }
};

}  // namespace

TEST_CASE("sheet: seg10:0x7328 renders the quick cells into column 16, rows 0 and 1") {
    ShieldRig r;
    QuickRequest seen;
    const QuickRenderer quick = [&seen](const QuickRequest& q, is::gfx::QuickCells& c) {
        seen = q;
        c.h = 33;
        c.gap = 5;
        c.side = 43;
        for (int it = 0; it < 2; ++it) c.buf[static_cast<std::size_t>(it)].assign(43u * 43u, static_cast<std::uint8_t>(20 + it));
        return true;
    };
    shield_render(r.out, r.images, r.w, 0, quick);
    // The 11 words: the request's graphic and scheme bytes, the heading, 0x63, `Size`.
    CHECK(seen.graphic_id == 60);
    CHECK(seen.colour_b == 10);
    CHECK(seen.heading == 3);
    CHECK(seen.gap_word == 99);
    CHECK(seen.size_level == 1);
    // Two temporary pictures, each BitBlt SRCCOPY of (5, 5, 33, 33) to (613, 5) and (613, 43).
    int blits = 0;
    for (const BlitCall& b : r.out.calls) {
        if (b.dst != 0x50) continue;
        CHECK(b.op == BlitCall::kBitBlt);
        CHECK(b.x == 613);
        CHECK(b.y == (blits == 0 ? 5 : 43));
        CHECK(b.w == 33);
        CHECK(b.sx == 5);
        CHECK(b.rop == kSrcCopy);
        ++blits;
    }
    CHECK(blits == 2);
    // The copy has the cells in column 16.
    const SheetPixels* c = sheet_copy(0);
    REQUIRE(c);
    CHECK(c->index[static_cast<std::size_t>(5) * c->w + 613] == 20);
    CHECK(c->index[static_cast<std::size_t>(43 + 32) * c->w + 613 + 32] == 21);
    CHECK(c->index[static_cast<std::size_t>(5) * c->w + 612] == 7);
    // No renderer: nothing drawn.
    ShieldRig q;
    shield_render(q.out, q.images, q.w, 0, QuickRenderer{});
    CHECK(q.out.calls.empty());
    clear_sheet_copies();
}

TEST_CASE("sheet: seg14:0xc84-0xcce cuts the lost wings again in a restored game") {
    ShieldRig r;
    ShipRecord& s = r.w.ship(0);
    s.has_flame = 1;
    s.design_index = 0;
    r.w.design_ratings[0].special = 3;
    r.w.clock = 50000;
    DesignWordSet d;
    d.w3c = 5;
    set_design_words(0, d);
    set_design_body(0, 10, 4, 6);
    set_byte_at(s, 0xac, 0);
    set_byte_at(s, 0xad, 1);
    // Not restored: nothing.
    recut_lost_wings(r.w, 0);
    CHECK(sheet_copy(0)->index[static_cast<std::size_t>(5 + 4) * 651 + (38 * 3 + 5) + 0] == 7);
    r.w.restored = 1;
    recut_lost_wings(r.w, 0);
    CHECK(s.due156.get() == 50000u - 10000u);
    // Heading 3's row-0 cell is cut as ordinal 15 with cut 1 does it (wing +0xac); row 1 is not.
    const auto keep = is::gfx::wing_keep(33, 5, 3, true);
    const SheetPixels* c = sheet_copy(0);
    REQUIRE(c);
    int mismatch = 0;
    for (int y = 0; y < 33; ++y)
        for (int x = 0; x < 33; ++x) {
            const std::uint8_t v = c->index[static_cast<std::size_t>(5 + y) * c->w + 38 * 3 + 5 + x];
            mismatch += (v != 0) != (keep[static_cast<std::size_t>(y) * 33 + x] != 0);
        }
    CHECK(mismatch == 0);
    CHECK(c->index[static_cast<std::size_t>(43 + 4) * c->w + 38 * 3 + 5] == 7);
    // A ship that is not a wing design keeps its sheet.
    ShieldRig q;
    q.w.restored = 1;
    q.w.ship(0).has_flame = 1;
    q.w.design_ratings[0].special = -1;
    recut_lost_wings(q.w, 0);
    int zeros = 0;
    for (std::uint8_t v : sheet_copy(0)->index) zeros += v == 0;
    CHECK(zeros == 0);
    reset_design_words();
    clear_sheet_copies();
}

TEST_CASE("sheet golden: design +0x2a is the body's height, not the SHP tank byte (S1-17)") {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(v);
    const is::assets::Ships ships = store.ships(v);
    // TRANSFORMER and SKY TALON: tank byte 33, body 33 high (core-0001 and core-0006's +0x20 =
    // 19 = 33 - 14) [ORACLE core-0001] [ORACLE core-0006]; NEMESIS: tank 34, body SKULL 35 high.
    struct Case {
        const char* name;
        int tank, height;
    };
    for (const Case& c : {Case{"TRANSFORMER", 33, 33}, Case{"SKY TALON", 33, 33}, Case{"NEMESIS", 34, 35}}) {
        const is::fmt::ShipRecord* r = ships.find(c.name);
        REQUIRE(r);
        const is::fmt::BodyRecord* b = ships.body(*r);
        REQUIRE(b);
        CHECK(r->fuel_tank == c.tank);
        CHECK(store.sprite(b->bitmap_id).height == c.height);
    }
}

TEST_CASE("sheet: the program's seg14:0x11a8 for seg16:0x3468(i, 3) after a visit (S3-9, A-3)") {
    ShieldRig r;  // ship 0 with a kept request and slot 0x28 = DC 0x50
    r.w.roster.assign(1, RosterRecord{});
    ShipRecord& s = r.w.ship(0);
    s.roster_index = 0;
    SpriteLinks saved = sprite_links();
    Blitter* saved_blitter = wave_blitter();
    std::vector<std::string> todos;
    sprite_links().images = [&r]() -> ImageSlotTable* { return &r.images; };
    sprite_links().render = fixed_render(651, 157);
    sprite_links().todo = [&todos](const std::string& what, const char*) { todos.push_back(what); };
    set_wave_blitter(&r.out);
    CHECK(sheet_dc_program(0));
    CHECK(sheet_dc_word_program(0) == 0x50);
    CHECK_FALSE(sheet_dc_program(1));
    // The visit cleared +0xac (seg16:0x3621): the build renders into a new image of the slot, the
    // roster bytes +0x7a/+0x7b get the side and gap, and the wing bytes are 1 again.
    set_byte_at(s, 0xac, 0);
    set_byte_at(s, 0xad, 1);
    build_sheet_program(r.w, 0);
    CHECK(r.images[kShipImageSlotBase] != 0x50);
    CHECK(r.images[kShipImageSlotBase] != kDcNone);
    CHECK(r.w.roster[0].size == 33);
    CHECK(r.w.roster[0].gap == 5);
    CHECK(sbyte_at(s, 0xac) == 1);
    CHECK(sbyte_at(s, 0xad) == 1);
    CHECK(todos.empty());
    // Without the drawing side: a todo, nothing built.
    sprite_links().images = nullptr;
    build_sheet_program(r.w, 0);
    CHECK(todos.size() == 1);
    set_wave_blitter(saved_blitter);
    sprite_links() = saved;
    clear_sheet_copies();
    reset_design_words();
}

TEST_CASE("sheet: seg14:0x0's tail writes design +0x2a, +0x00 / +0x02 and the muzzles (M4 s5 fixer)") {
    // design_half_side: the raw side, +1 at Size 0 when odd, then cwd; sub ax, dx; sar ax, 1
    // [BYTES seg14:0xbb5-0xbc2 seg14:0xc42-0xc54].
    CHECK(design_half_side(33, 1) == 16);
    CHECK(design_half_side(33, 0) == 17);
    CHECK(design_half_side(34, 0) == 17);
    CHECK(design_half_side(35, 2) == 17);
    CHECK(design_half_side(-3, 1) == -1);  // toward zero
    ShieldRig r;
    ShotState shots;
    ShipRecord& s = r.w.ship(0);
    s.design_index = 0;
    s.roster_index = -1;
    // No roster record: +0x2a = the raw side seg14:0x11a8 left in the object [BYTES seg14:0xba9].
    placement_tail(r.w, 0, 33, shots);
    CHECK(design_words(0).w2a == 33);
    CHECK(design_words(0).w00 == 16);
    CHECK(design_words(0).w02 == 16);
    // seg10:0x1460 (r = +0x00 + 3 = 19, a from 450) and seg10:0x1620 (r = +0x02 = 16, a from 270)
    // at heading 0: the nose 19 px above the cell's centre, the tail 16 px below it; heading 4:
    // the tail 16 px left of it [BYTES seg10:0x147b seg10:0x163b].
    CHECK(shots.mx(0, 0) == 16);
    CHECK(shots.my(0, 0) == -3);
    CHECK(shots.mx(0, 16) == 16);
    CHECK(shots.my(0, 16) == 32);
    CHECK(shots.mx(0, 16 + 4) == 0);
    CHECK(shots.my(0, 16 + 4) == 16);
    // A roster record: +0x2a = sext(roster byte +0x7a) [BYTES seg14:0xd86-0xd94].
    r.w.roster.assign(1, RosterRecord{});
    r.w.roster[0].size = 35;
    r.w.roster[0].wing0 = 0;
    r.w.roster[0].wing1 = 1;
    s.roster_index = 0;
    s.has_flame = 1;
    set_byte_at(s, 0xac, 1);
    set_byte_at(s, 0xad, 1);
    placement_tail(r.w, 0, 33, shots);
    CHECK(design_words(0).w2a == 35);
    CHECK(design_words(0).w00 == 17);
    CHECK(sbyte_at(s, 0xac) == 1);  // not restored: the wing bytes stay
    // A restored game takes the wing bytes from roster +0x72 / +0x73 again.
    r.w.restored = 1;
    placement_tail(r.w, 0, 33, shots);
    CHECK(sbyte_at(s, 0xac) == 0);
    CHECK(sbyte_at(s, 0xad) == 1);
    reset_design_words();
    clear_sheet_copies();
}
