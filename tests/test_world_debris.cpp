// The wing debris (src/world/debris.h; work/analysis/m4/s1-death.md section 6, verified claims
// 42-47): the entry seg8:0x2cd8 starts, the mover seg13:0x447a (drift, drag, the flames at
// counter 1, the smoke counters, the end at 70), the draw seg8:0x8a6 (masked over a listed
// object, plain otherwise, the (-40 ... W + 40) bounds) and the smoke table seg7:0x5d6a.
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "world/debris.h"
#include "world/effects.h"
#include "world/flight.h"

using namespace is::world;

namespace {

struct Fixture {
    Pools pools;
    DebrisTable table{};
    SmokeTable smoke{};
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

TEST_CASE("debris: seg7:0x5d6a takes a slot by Detail") {
    SmokeTable t{};
    std::int16_t n = 0;
    is::shell::Rng rng;
    is::shell::Rng expect = rng;
    CHECK(smoke_start(t, n, 3, rng, 10, 20, 1, 5, 6) == 0);
    CHECK(rng.state() == expect.state());  // Detail 3 draws nothing
    CHECK(t[0].x == 10);
    CHECK(t[0].y == 20);
    CHECK(t[0].a == 5);
    CHECK(t[0].b == 6);
    CHECK(t[0].used == 0);
    CHECK(n == 1);
    // Detail 0: a draw below 20 takes nothing.
    int taken = 0;
    is::shell::Rng r2;
    is::shell::Rng e2 = r2;
    for (int i = 0; i < 18; ++i) {
        const bool want = e2.rand() % 100 >= 20;
        const int k = smoke_start(t, n, 0, r2, 0, 0, 1, 0, 0);
        CHECK((k >= 0) == want);
        taken += k >= 0;
    }
    CHECK(n == 1 + taken);
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
    CHECK(f.smoke[0].used == 0);
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
