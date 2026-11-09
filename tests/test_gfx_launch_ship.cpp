// owner: M4 stage 6, track PAD.
// The portable tables of src/gfx/launch_ship.h: seg15:0x20ca's size, seg15:0x2855's frame 0 by
// `Detail` and seg15:0x1a42's 16 jet frames (work/analysis/m4/s6-pad.md 2.2 and 2.6, the listing of
// seg15:0x1a42).
#include "doctest.h"
#include "gfx/launch_ship.h"

using is::gfx::jet_frame0;
using is::gfx::jet_table;
using is::gfx::jet_table_height;

TEST_CASE("launch ship: seg15:0x20ca's sz from the roster bytes +0x7a and +0x7b") {
    is::gfx::VisitShipArt v;
    v.r7a = 33, v.r7b = 6;
    CHECK(is::gfx::visit_ship_size(v) == 33);
    v.r7a = 30, v.r7b = 9;  // half(3) = 1
    CHECK(is::gfx::visit_ship_size(v) == 31);
    v.r7a = 30, v.r7b = 3;  // half(-3) = -1, toward zero
    CHECK(is::gfx::visit_ship_size(v) == 29);
}

TEST_CASE("launch ship: frame 0 of the jet by Detail [BYTES seg15:0x2855-0x28a7]") {
    int w = 0, h = 0;
    jet_frame0(31, 2, w, h);
    CHECK(w == 186);  // the SKY TALON's corridor frames differ in x 419-604: 186 px [ORACLE core-0003]
    CHECK(h == 74);   // trunc(24 31 / 10)
    jet_frame0(33, 0, w, h);
    CHECK(w == 132);
    CHECK(h == 52);
    jet_frame0(33, 1, w, h);
    CHECK(w == 165);
    CHECK(h == 66);
    jet_frame0(33, 3, w, h);
    CHECK(w == 231);
    CHECK(h == 92);
    jet_frame0(33, 4, w, h);
    CHECK(w == 0);
}

TEST_CASE("launch ship: seg15:0x1a42's jet table, frames 1-9 then 15 down to 10") {
    const auto t = jet_table(31, 2);  // w0 186, q 6, c 550, dc 39
    CHECK(t[0].w == 186);
    CHECK(t[0].y == 0);
    // frame 1: trunc(lo16(31 55) / 10) = 170, h = trunc(40 170 / 100) = 68, y = 74
    CHECK(t[1].w == 170);
    CHECK(t[1].h == 68);
    CHECK(t[1].y == 74);
    // frame 2: c = 511 -> 51: trunc(31 51 / 10) = 158
    CHECK(t[2].w == 158);
    CHECK(t[2].y == 74 + 68);
    // frame 15 follows frame 9: m 10, p 100: w = 31, h = 31
    CHECK(t[15].w == 31);
    CHECK(t[15].h == 31);
    CHECK(t[15].y == t[9].y + t[9].h);
    // frame 10 is the last stored: m 20, p 40: w = 62, h = 24
    CHECK(t[10].w == 62);
    CHECK(t[10].h == 24);
    CHECK(t[10].y == t[11].y + t[11].h);
    CHECK(jet_table_height(t) == t[10].y + t[10].h);
    // every frame narrower than frame 0, the bitmap's width
    for (const auto& f : t) CHECK(f.w <= t[0].w);
    // an unknown Detail: no table
    CHECK(jet_table(31, 7)[0].w == 0);
}
