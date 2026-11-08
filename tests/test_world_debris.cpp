// The wing debris (src/world/debris.h; work/analysis/m4/s1-death.md section 6, verified claims
// 42-47): the entry seg8:0x2cd8 starts, the mover seg13:0x447a (drift, drag, the flames at
// counter 1, the smoke counters, the end at 70), the draw seg8:0x8a6 (masked over a listed
// object, plain otherwise, the (-40 ... W + 40) bounds). M4 s5 SHEET: the square over the torn
// wing, the strip's five blits and seg8:0x2cd8's picture part (work/analysis/m4/s5-sheet.md 1.3,
// 1.4, verified claims 6-10). seg7:0x5d6a's tests are smoke.h's (tests/test_world_smoke.cpp).
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "gfx/wing_cut.h"
#include "world/debris.h"
#include "world/design_words.h"
#include "world/effects.h"
#include "world/flight.h"
#include "world/player_sheet.h"
#include "world/world.h"

using namespace is::world;

namespace {

struct Fixture {
    Pools pools;
    DebrisTable table{};
    SmokeRecords smoke{};
    std::int16_t d9456 = 0;
    is::shell::Rng rng;
    RecordingBlitter out;
    VisibilityGlobals vis;
    int players = 0;
    DebrisCtx c{table, smoke, d9456, pools, rng, 1, 0, 0, 0, 3, nullptr, nullptr, nullptr, nullptr};
    Fixture() {
        c.W = 1024;
        c.H = 683;
        c.detail = 3;
        c.out = &out;
        c.visible = &vis;
        c.to_screen = [](std::int16_t x, std::int16_t y) {
            return Point16{static_cast<std::int16_t>(x >> 3), static_cast<std::int16_t>(y >> 3)};
        };
        c.player_sprite = [this] { ++players; };
    }
};

}  // namespace

TEST_CASE("debris: seg8:0x2cd8 starts an entry at counter 1, end 70") {
    DebrisTable t{};
    ObjectRecord so{};
    so.x = 800;
    so.y = 1600;
    so.vx = 10;
    so.vy = -3;
    so.extent = 264;
    so.frame = 0;  // (0 + 4) % 16 = 4: (16, 0)
    CHECK(debris_start(t, so) == 0);
    CHECK(t[0].counter == 1);
    CHECK(t[0].end == 70);
    CHECK(t[0].extent == 264);
    CHECK(t[0].x == 800);
    CHECK(t[0].sx == -1000);
    CHECK(t[0].vx == 10 + 8);
    CHECK(t[0].vy == -3);
    so.frame = 9;  // 13: (-15, -6) / 2 = (-7, -3), truncating
    CHECK(debris_start(t, so) == 1);
    CHECK(t[1].vx == 10 - 7);
    CHECK(t[1].vy == -3 - 3);
    CHECK(debris_start(t, so) == 2);
    CHECK(debris_start(t, so) == -1);
}

TEST_CASE("debris: seg13:0x447a, the flames at 1, the smoke counters and the end at 70") {
    Fixture f;
    ObjectRecord so{};
    so.x = 800;
    so.y = 1600;
    so.vx = 32;
    so.vy = -32;
    debris_start(f.table, so);
    f.table[0].dc = 0x60;
    f.table[0].mask = 0x61;
    // Counter 1: the screen position only, then the flames into the strip.
    move_debris(f.c);
    CHECK(f.table[0].sx == 100 + 0);  // stored before the move
    CHECK(f.table[0].x == 800 + 32 + 8);
    CHECK(f.table[0].vx == 40 - 2);  // 40 - (40 >> 4)
    REQUIRE(f.out.images.size() == 2);
    CHECK(f.out.images[0].graphic == kGraphicFlames);
    const DcId g = f.out.images[0].id, gm = f.out.images[1].id;
    REQUIRE(f.out.calls.size() == 3);
    CHECK(f.out.calls[0] == bit_blt_call(0x60, 0, 0, 160, 32, gm, 0, 0, kSrcAnd));
    CHECK(f.out.calls[1] == bit_blt_call(0x60, 0, 0, 160, 32, g, 0, 0, kSrcPaint));
    CHECK(f.out.calls[2] == bit_blt_call(0x60, 0, 0, 160, 32, 0x61, 0, 0, kDsna));
    CHECK(f.players == 1);
    CHECK(f.table[0].counter == 2);
    // On to 70: smoke at 50, 56, 61, 63, 65, 66 (Detail 3 always takes a slot).
    for (int i = 2; i < 70; ++i) move_debris(f.c);
    CHECK(f.d9456 == 6);
    CHECK(f.table[0].counter == 0);
    CHECK(f.out.deleted.size() >= 4);
    CHECK(f.table[0].dc == kDcNone);
    // The smoke positions are the debris position + 0x80 at that frame.
    CHECK(f.smoke[0].counter == 0);
}

TEST_CASE("debris: seg8:0x8a6 draws plain, or masked over a listed object, inside the bounds") {
    Fixture f;
    DebrisEntry& e = f.table[0];
    e.counter = 7;  // frame (7 / 2) % 5 = 3
    e.x = 800;
    e.y = 800;
    e.sx = 90;
    e.sy = 90;
    e.dc = 0x60;
    e.mask = 0x61;
    draw_debris(f.c, 0);
    REQUIRE(f.out.calls.size() == 2);
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 90, 90, 32, 32, kBlackness));
    CHECK(f.out.calls[1] == bit_blt_call(kDcWindow, 100, 100, 32, 32, 0x60, 96, 0, kSrcCopy));
    // A listed object under the old square: the mask path.
    ObjectRecord& o = f.pools.rec(3, 1);
    o.scr_x = 110;
    o.scr_y = 110;
    o.size = 22;
    f.vis.count = 1;
    f.vis.pool[0] = 3;
    f.vis.rec[0] = 1;
    f.out.calls.clear();
    draw_debris(f.c, 0);
    REQUIRE(f.out.calls.size() == 3);
    CHECK(f.out.calls[0] == bit_blt_call(kDcWindow, 100, 100, 32, 32, 0x61, 0, 0, kSrcAnd));
    CHECK(f.out.calls[1] == bit_blt_call(kDcWindow, 100, 100, 32, 32, 0x61, 0, 0, kSrcAnd));
    CHECK(f.out.calls[2] == bit_blt_call(kDcWindow, 100, 100, 32, 32, 0x60, 96, 0, kSrcPaint));
    // Outside (-40 ... W + 40): nothing.
    f.vis.count = 0;
    e.x = 8 * 1064;
    e.sx = -40;
    f.out.calls.clear();
    draw_debris(f.c, 0);
    CHECK(f.out.calls.empty());
    CHECK(e.sx == 1064);
    // Counter 1 stores the screen position only.
    e.counter = 1;
    e.x = 80;
    draw_debris(f.c, 0);
    CHECK(f.out.calls.empty());
    CHECK(e.sx == 10);
    CHECK(debris_cell(1, 0) == 32);
    CHECK(debris_cell(2, 0) == 64);
    CHECK(debris_cell(1, 1) == 32);  // (64 + 32) / 3
}

TEST_CASE("debris: seg8:0x2f90-0x3096 puts the square over the torn wing (s5-sheet.md 1.4)") {
    // TRANSFORMER at Size 1: E 264, D 5, R = 8 trunc(5 / 2) + trunc((264 - 40) / 4) = 16 + 56.
    CHECK(debris_reach(5, 264) == 72);
    CHECK(debris_reach(4, 264) == 16 + 58);  // the SKY TALON's D at Size 1
    CHECK(debris_reach(-3, 10) == -8 + 8);  // trunc toward zero both ways: -1 * 8 + trunc(34 / 4)
    DebrisTable t{};
    ObjectRecord so{};
    so.x = 1000;
    so.y = 2000;
    so.extent = 264;
    so.frame = 0;
    REQUIRE(debris_start(t, so) == 0);
    // Heading 0, cut 1 (wing +0xac, the port side): m = 12, T128 (-128, 0).
    DebrisWindow a = debris_place(t[0], 0, 5, true);
    CHECK(a.R == 72);
    CHECK(a.px == 60);
    CHECK(a.py == 132);
    CHECK(a.x0 == 0);
    CHECK(a.x1 == 188);
    CHECK(a.y0 == 4);
    CHECK(a.y1 == 260);
    CHECK(a.ox == 68);
    CHECK(a.oy == 0);
    CHECK(t[0].x == 1000 - 68);
    CHECK(t[0].y == 2000 + 4);
    const auto pa = debris_strip_plan(a, 3);
    for (int j = 0; j < 5; ++j) {
        CHECK(pa[static_cast<std::size_t>(j)].x == 32 * j + 8);
        CHECK(pa[static_cast<std::size_t>(j)].y == 0);
        CHECK(pa[static_cast<std::size_t>(j)].w == 23);
        CHECK(pa[static_cast<std::size_t>(j)].h == 32);
        CHECK(pa[static_cast<std::size_t>(j)].sx == 0);
        CHECK(pa[static_cast<std::size_t>(j)].sy == 0);
    }
    // Heading 0, cut 0 (wing +0xad, the starboard side): m = 4, T128 (128, 0).
    REQUIRE(debris_start(t, so) == 1);
    DebrisWindow b = debris_place(t[1], 0, 5, false);
    CHECK(b.px == 204);
    CHECK(b.x0 == 76);
    CHECK(b.x1 == 264);
    CHECK(b.ox == 0);
    CHECK(b.oy == 0);
    CHECK(t[1].x == 1000 + 76);
    CHECK(t[1].y == 2000 + 4);
    const auto pb = debris_strip_plan(b, 3);
    CHECK(pb[2].x == 64);
    CHECK(pb[2].w == 23);
    CHECK(pb[2].sx == 9);
    // DGROUP:0x66e is the 128-scaled heading table [BYTES DGROUP:0x66e].
    CHECK(kDebrisT128[2 * 3] == 110);
    CHECK(kDebrisT128[2 * 3 + 1] == -48);
    CHECK(kDebrisT128[2 * 9] == -48);
    CHECK(kDebrisT128[2 * 9 + 1] == 110);
}

namespace {

// A World with ship 0 of design 0 (D = 5) and a 613 x 76 sheet copy, every pixel 7.
struct WingRig {
    World w;
    DebrisTable t{};
    RecordingBlitter out;
    ImageSlotTable images{};
    WingRig() {
        reset_design_words();
        clear_sheet_copies();
        w.size = 1;
        w.shift = 3;
        w.pools.set_in_use(kPoolShips, 1);
        ObjectRecord& o = w.pools.rec(kPoolShips, 0);
        o.x = 1000;
        o.y = 2000;
        o.extent = 264;
        o.size = 33;
        o.gap = 5;
        o.frame = 0;
        DesignWordSet d;
        d.w3c = 5;
        set_design_words(0, d);
        SheetPixels px;
        px.w = 16 * 33 + 17 * 5;
        px.h = 2 * 38 + 5;
        px.index.assign(static_cast<std::size_t>(px.w) * px.h, 7);
        px.source_height = 33;
        set_sheet_copy(0, px);
        images[kShipImageSlotBase] = 0x50;
    }
};

}  // namespace

TEST_CASE("debris: seg8:0x2cd8's picture part, the debris path and the cut alone") {
    WingRig r;
    CHECK(wing_picture(r.w, r.t, &r.out, &r.images, 0, 1, true));
    CHECK(r.t[0].counter == 1);
    CHECK(r.t[0].x == 1000 - 68);  // over the port wing
    // The cut on the copy: heading 0 keeps columns 11 ... 32 of row 0's cell (s5-sheet.md 2.8).
    const SheetPixels* c = sheet_copy(0);
    REQUIRE(c);
    CHECK(c->index[static_cast<std::size_t>(5 + 3) * c->w + 5 + 10] == 0);
    CHECK(c->index[static_cast<std::size_t>(5 + 3) * c->w + 5 + 11] == 7);
    CHECK(c->index[static_cast<std::size_t>(38 + 5 + 3) * c->w + 5 + 0] == 7);  // row 1 keeps its wings
    // The images: the sheet's band through a temporary image, the black strip 160 x 32, the
    // wing cell 33 x 33 (columns 0 ... 10 of heading 0 kept), its five blits and the mask.
    bool band = false, strip = false, mask = false;
    int blits = 0;
    DcId strip_id = kDcNone, wing_id = kDcNone;
    for (const auto& im : r.out.images) {
        if (im.w == 160 && im.h == 32 && im.pixels.empty()) strip_id = im.id, strip = true;
        if (im.w == 33 && im.h == 33) {
            wing_id = im.id;
            CHECK(im.pixels[static_cast<std::size_t>(10) * 33 + 10] == 7);
            CHECK(im.pixels[static_cast<std::size_t>(10) * 33 + 11] == 0);
        }
        if (im.name == "mask") mask = true;
    }
    for (const BlitCall& b : r.out.calls) {
        if (b.dst == 0x50 && b.op == BlitCall::kBitBlt) {
            band = true;
            CHECK(b.x == 0);
            CHECK(b.y == 5);
            CHECK(b.w == 16 * 38);
            CHECK(b.h == 33);
        }
        if (b.dst == strip_id && b.src == wing_id && b.rop == kSrcCopy) {
            CHECK(b.x == 32 * blits + 8);
            CHECK(b.w == 23);
            ++blits;
        }
    }
    CHECK(band);
    CHECK(strip);
    CHECK(mask);
    CHECK(blits == 5);
    CHECK(r.t[0].dc == strip_id);
    CHECK(r.t[0].mask != kDcNone);
    // No debris (DGROUP:0x92e4 != 0, or no free entry): the cut alone, no entry.
    WingRig q;
    CHECK_FALSE(wing_picture(q.w, q.t, &q.out, &q.images, 0, 0, false));
    CHECK(q.t[0].counter == 0);
    const SheetPixels* qc = sheet_copy(0);
    REQUIRE(qc);
    CHECK(qc->index[static_cast<std::size_t>(5 + 3) * qc->w + 5 + 21] == 7);  // cut 0 keeps 0 ... 21
    CHECK(qc->index[static_cast<std::size_t>(5 + 3) * qc->w + 5 + 22] == 0);
    for (auto& e : q.t) e.counter = 3;  // a full table
    CHECK_FALSE(wing_picture(q.w, q.t, &q.out, &q.images, 0, 1, true));
    reset_design_words();
    clear_sheet_copies();
}
