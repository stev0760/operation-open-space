// The stars (src/world/draw.h star_count_for, init_stars, draw_stars, star_brush;
// work/analysis/m3/s4-draw.md 2): the count by Detail, the six rand() per star of seg8:0x190e
// in their order, the every-second-call body of seg8:0x158a, the move against the player's
// velocity, the wraps in DGROUP:0x1d30, the twinkle brushes and the player's cell. The oracle
// shows 1, 4 and 9 pixel dots in the five colours of DGROUP:0x3b60 ... 0x3b68 [ORACLE
// wave-0001 16-capture-before.png].
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/draw.h"
#include "world/rock_images.h"

using namespace is::world;

namespace {

struct StarRig {
    World w;
    StarInputs in;
    WaveGraphics wg;
    RecordingBlitter out;
    is::shell::Rng rng;

    StarRig() {
        w.W = 1024;
        w.H = 683;
        w.play_rect = rect16(68, 78, 956, 663);  // seg2:0xca4 at 1024 x 768 with Border
        w.star_count = w.star_wanted = 1;
        w.stars.resize(kStarMax);
        for (std::size_t n = 10; n < wg.star.size(); ++n) wg.star[n] = static_cast<BrushId>(0x400 + n);
        in.size = 1;
        ObjectRecord& p = w.pools.rec(kPoolShips, 0);
        p.state = 0;
    }
    Star& star() { return w.stars[0]; }
    // The body runs on the second call (DGROUP:0x6ae 0 -> 1 -> 0).
    void pass(int arg = 0) {
        draw_stars(w, in, wg, out, rng, arg);
        if (arg == 0) draw_stars(w, in, wg, out, rng, arg);
    }
};

BlitCall select_call(DcId dc, BrushId b) {
    BlitCall c;
    c.op = BlitCall::kSelectBrush, c.dst = dc, c.brush = b;
    return c;
}

}  // namespace

TEST_CASE("stars: the count by Detail and Bright (seg6:0xe36)") {
    CHECK(star_count_for(0, 0, 9) == 4);
    CHECK(star_count_for(1, 0, 9) == 7);
    CHECK(star_count_for(2, 0, 9) == 13);
    CHECK(star_count_for(3, 0, 9) == 18);
    CHECK(star_count_for(5, 0, 9) == 9);   // another Detail keeps the count
    CHECK(star_count_for(3, 1, 9) == 17);  // Bright takes one off [BYTES seg6:0xe88]
}

TEST_CASE("stars: seg8:0x190e draws six rand() per star in its order [BYTES seg8:0x190e]") {
    World w;
    w.W = 1024;
    w.H = 683;
    w.star_count = 18;
    is::shell::Rng rng, ref;
    rng.srand(37816);
    ref.srand(37816);
    init_stars(w, rng);
    REQUIRE(w.stars.size() >= 18u);
    for (int i = 0; i < 18; ++i) {
        const Star& s = w.stars[static_cast<std::size_t>(i)];
        const int x = ref.rand() % 1024, y = ref.rand() % 683;
        const int b = ref.rand() % 255, g = ref.rand() % 255;
        const int size = ref.rand() % 3 + 1, phase = ref.rand() % 40;
        CHECK(s.x == x);
        CHECK(s.y == y);
        // DGROUP:0x2f04 = (g << 8) | 0xff, DGROUP:0x2f06 = 0x0200 | b.
        CHECK(s.colour == ((0x0200u | static_cast<std::uint32_t>(b)) << 16 |
                           static_cast<std::uint32_t>(g) << 8 | 0xffu));
        CHECK(s.speed == size);
        CHECK(s.phase == phase);
        CHECK(s.on == 1);
    }
    CHECK(rng.state() == ref.state());  // 108 draws at Detail 3
    // No stars, no draws.
    World e;
    e.W = 1024, e.H = 683;
    is::shell::Rng r2;
    r2.srand(5);
    const std::uint32_t st = r2.state();
    init_stars(e, r2);
    CHECK(r2.state() == st);
}

TEST_CASE("stars: the twinkle brush index (seg8:0x17b7)") {
    CHECK(star_brush(0) == 10);
    CHECK(star_brush(4) == 10);
    CHECK(star_brush(5) == 11);
    CHECK(star_brush(15) == 13);
    CHECK(star_brush(19) == 13);
    CHECK(star_brush(20) == 14);
    CHECK(star_brush(25) == 13);
    CHECK(star_brush(39) == 11);
    CHECK(star_brush(40) == 10);
}

TEST_CASE("stars: one pass erases, moves and draws (seg8:0x158a)") {
    StarRig r;
    Star& s = r.star();
    s.x = 500, s.y = 300, s.speed = 1, s.phase = 7, s.on = 1;
    r.w.pools.rec(kPoolShips, 0).vx = 16;   // (16 << 2) >> 5 = 2
    r.w.pools.rec(kPoolShips, 0).vy = -16;  // -2
    // The first call only toggles DGROUP:0x6ae.
    draw_stars(r.w, r.in, r.wg, r.out, r.rng, 0);
    CHECK(r.out.calls.empty());
    draw_stars(r.w, r.in, r.wg, r.out, r.rng, 0);
    REQUIRE(r.out.calls.size() == 4);
    CHECK(r.out.calls[0] == pat_blt_call(kDcWindow, 500, 300, 1, 1, kBlackness));
    CHECK(r.out.calls[1] == select_call(kDcWindow, 0x40b));  // phase 7: n = 11
    CHECK(r.out.calls[2] == pat_blt_call(kDcWindow, 498, 302, 1, 1, kPatCopy));
    CHECK(r.out.calls[3] == select_call(kDcWindow, kBrushWhite));
    CHECK(s.phase == 8);
    CHECK(s.on == 1);
    // Size 3 moves (16 << 2) >> 3 = 8 and draws 3 x 3; Size 0 caps the side at 2.
    StarRig b;
    b.star().x = 500, b.star().y = 300, b.star().speed = 3, b.star().on = 0;
    b.w.pools.rec(kPoolShips, 0).vx = 16;
    b.pass();
    REQUIRE(b.out.calls.size() == 3);
    CHECK(b.out.calls[1] == pat_blt_call(kDcWindow, 492, 300, 3, 3, kPatCopy));
    StarRig z;
    z.in.size = 0;
    z.star().x = 500, z.star().y = 300, z.star().speed = 3;
    z.w.pools.rec(kPoolShips, 0).vx = 16;  // (16 << 1) >> 3 = 4
    z.pass();
    REQUIRE(z.out.calls.size() == 3);
    CHECK(z.out.calls[1] == pat_blt_call(kDcWindow, 496, 300, 2, 2, kPatCopy));
    // The phase wraps after 40.
    StarRig p;
    p.star().x = 500, p.star().y = 300, p.star().speed = 1, p.star().phase = 40;
    p.pass();
    CHECK(p.star().phase == 0);
    CHECK(p.out.calls[0] == select_call(kDcWindow, 0x40a));  // not drawn before: no erase
}

TEST_CASE("stars: the wraps redraw the other coordinate without the origin [BYTES seg8:0x16b2]") {
    StarRig r;
    Star& s = r.star();
    s.x = 69, s.y = 300, s.speed = 2, s.on = 0;
    r.w.pools.rec(kPoolShips, 0).vx = 64;  // (64 << 2) >> 4 = 16: x 53 < 68
    r.rng.srand(99);
    is::shell::Rng ref;
    ref.srand(99);
    r.pass();
    CHECK(s.x == 956);  // to the right edge
    const int y = ref.rand() % (663 - 78);
    const int g = ref.rand() % 255, b = ref.rand() % 255;
    const int size = ref.rand() % 3 + 1;
    CHECK(s.y == y);
    CHECK(s.colour == (static_cast<std::uint32_t>(b) << 16 | static_cast<std::uint32_t>(g) << 8 | 0xffu));
    CHECK(s.speed == size);
    // A y below the top (y % 585 can be < 78) wraps again on y: here it did when y < 78.
    if (y >= 78) CHECK(r.rng.state() == ref.state());
}

TEST_CASE("stars: the player's cell, the player's velocity and the counts") {
    StarRig r;
    r.star().x = 500, r.star().y = 300, r.star().speed = 1, r.star().on = 1;
    r.w.player_cell = rect16(490, 290, 520, 320);  // DGROUP:0x1e5c
    r.pass();
    REQUIRE(r.out.calls.size() == 1);  // the erase only
    CHECK(r.star().on == 0);
    // The player's state 1 zeroes its velocity [BYTES seg8:0x15f5-0x1630].
    StarRig v;
    v.w.pools.rec(kPoolShips, 0).state = 1;
    v.w.pools.rec(kPoolShips, 0).vx = 40;
    v.w.pools.rec(kPoolShips, 0).vy = 40;
    v.pass();
    CHECK(v.w.pools.rec(kPoolShips, 0).vx == 0);
    CHECK(v.w.pools.rec(kPoolShips, 0).vy == 0);
    // A changed Detail: no star drawn on the pass that takes the new count [BYTES seg8:0x18f6].
    StarRig c;
    c.star().x = 500, c.star().y = 300, c.star().speed = 1, c.star().on = 0;
    c.w.star_wanted = 4;
    c.pass();
    CHECK(c.out.calls.empty());
    CHECK(c.w.star_count == 4);
    // A non-zero argument forces the body and does not move.
    StarRig a;
    a.star().x = 500, a.star().y = 300, a.star().speed = 1;
    a.w.pools.rec(kPoolShips, 0).vx = 64;
    a.pass(1);
    CHECK(a.star().x == 500);
    CHECK(a.out.calls.size() == 3);
    // Bright calls seg8:0x1320 instead [OPEN].
    StarRig br;
    br.w.bright = 1;
    int bright = 0;
    draw_stars(br.w, br.in, br.wg, br.out, br.rng, 1, [&bright](int) { ++bright; });
    CHECK(bright == 1);
    CHECK(br.out.calls.empty());
}
