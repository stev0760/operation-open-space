// The clipped line seg12:0x7256 (src/world/line.h) and the seg5 long helpers it computes with
// (work/analysis/m4/s7-tether.md section 7, s7-tether-verify.md claims 55, 56 and Omission N).
// Every expected value is worked by hand from the listing seg12:0x7256-0x75c9 (M4 s7 TETHER).
#include <climits>
#include <cstdint>
#include <optional>

#include "doctest.h"
#include "world/line.h"
#include "world/sprite.h"

using namespace is::world;

namespace {
LineEnds ends(int x0, int y0, int x1, int y1) {
    return LineEnds{static_cast<std::int16_t>(x0), static_cast<std::int16_t>(y0),
                    static_cast<std::int16_t>(x1), static_cast<std::int16_t>(y1)};
}
}  // namespace

TEST_CASE("seg5 long helpers: LDIV truncates toward zero and wraps INT_MIN / -1") {
    CHECK(rtl_ldiv(-7, 2) == -3);
    CHECK(rtl_ldiv(7, -2) == -3);
    CHECK(rtl_ldiv(-7, -2) == 3);
    CHECK(rtl_ldiv(INT_MIN, -1) == INT_MIN);  // magnitudes divided unsigned [BYTES seg5:0x116-0x1a7]
    CHECK(rtl_lshl(-1, 7) == -128);
    CHECK(rtl_lshl(0x01000000, 7) == INT_MIN);  // bits past 32 lost
    CHECK(rtl_lsar(-129, 7) == -2);             // `sar` floors
    CHECK(rtl_lmul(0x10000, 0x10000) == 0);     // the low 32 bits
    CHECK(neg32(INT_MIN) == INT_MIN);
}

TEST_CASE("seg12:0x7256: a line inside [0, W] x [0, H] is drawn as given") {
    CHECK(clip_line(640, 400, 10, 20, 30, 40) == ends(10, 20, 30, 40));
    CHECK(clip_line(640, 400, 0, 0, 640, 400) == ends(0, 0, 640, 400));  // inclusive
    CHECK(clip_line(640, 400, 5, 5, 5, 5) == ends(5, 5, 5, 5));          // no nudge inside
}

TEST_CASE("seg12:0x7256: both ends beyond one edge draw nothing") {
    CHECK_FALSE(clip_line(640, 400, 10, -5, 30, -1).has_value());
    CHECK_FALSE(clip_line(640, 400, 10, 401, 30, 900).has_value());
    CHECK_FALSE(clip_line(640, 400, -10, 5, -1, 30).has_value());
    CHECK_FALSE(clip_line(640, 400, 641, 5, 700, 30).has_value());
}

TEST_CASE("seg12:0x7256: the y1 nudge runs before the edge tests") {
    // y0 = y1 = -1: y1 becomes 0, so not both above. m = 128 / 20 = 6, b = -1 - (60 sar 7) = -1;
    // the top crossing -((-128) / 6) = 21 replaces P0; m' = 2560 / 1, then y1 = y0 again
    // (y0 - y1 + 1 = 0); P1 keeps (30, -1).
    CHECK(clip_line(640, 400, 10, -1, 30, -1) == ends(21, 0, 30, -1));
}

TEST_CASE("seg12:0x7256: a right-edge crossing replaces the outside end") {
    // m = 1280 / 100 = 12, m' = 12800 / 10 = 1280, b' = 50 - (64000 sar 7) = -450,
    // yi = (550 << 7) / 1280 = 55.
    CHECK(clip_line(100, 100, 50, 50, 150, 60) == ends(50, 50, 100, 55));
}

TEST_CASE("seg12:0x7256: rejected crossings leave the ends unclipped") {
    // y = 2x + 150 misses the box: the bottom crossing -25 and the left 150 are both rejected.
    CHECK(clip_line(100, 100, -50, 50, 50, 250) == ends(-50, 50, 50, 250));
}

TEST_CASE("seg12:0x7256: x1 = x0 + 1 is set back to x0 after the slope") {
    // m = 7680, x1 = 5, b = -10 - 300 = -310, the top crossing -((-39680) / 7680) = 5; m' = 0
    // with both x inside, so P1 = (5, 50).
    CHECK(clip_line(100, 100, 5, -10, 6, 50) == ends(5, 0, 5, 50));
}

TEST_CASE("seg12:0x7256: a zero slope with an end beyond the matching edge draws nothing") {
    // An end above the top with m = (1 << 7) / 1000 = 0 (seg12:0x7379).
    CHECK_FALSE(clip_line(2000, 100, 10, -1, 1010, 0).has_value());
    // An end left of the side with m' = 0 after x1 = x0 (seg12:0x74b7).
    CHECK_FALSE(clip_line(100, 100, -1, 10, 0, 50).has_value());
}

TEST_CASE("seg12:0x7256 on the Blitter: MoveTo then LineTo, nothing when clipped away") {
    RecordingBlitter b;
    draw_clipped_line(b, kDcWindow, 100, 100, 50, 50, 150, 60);
    REQUIRE(b.calls.size() == 2);
    CHECK(b.calls[0] == move_to_call(kDcWindow, 50, 50));
    CHECK(b.calls[1] == line_to_call(kDcWindow, 100, 55));
    b.calls.clear();
    draw_clipped_line(b, kDcWindow, 100, 100, -10, 5, -1, 30);
    CHECK(b.calls.empty());
}

#ifdef _WIN32
#include "gfx/gfx.h"

TEST_CASE("the GDI blitter's pens and lines: LineTo leaves out the last pixel") {
    is::gfx::Gfx gfx(nullptr);
    gfx.set_client_size(64, 48, 1);
    REQUIRE(gfx.logical_width() > 0);
    GdiBlitter b(gfx);
    b.pat_blt(kDcWindow, 0, 0, 64, 48, kBlackness);
    const PenId old = b.select_pen(kDcWindow, kPenWhite);
    CHECK(old == kPenBlack);  // a new DC holds BLACK_PEN
    draw_clipped_line(b, kDcWindow, 63, 47, 2, 5, 10, 5);
    gfx.surface().flush();
    CHECK(gfx.surface().at(2, 5) == 255);
    CHECK(gfx.surface().at(9, 5) == 255);
    CHECK(gfx.surface().at(10, 5) == 0);
    CHECK(gfx.surface().at(5, 6) == 0);
    // A created pen: selected, put back, deleted
    const PenId p = b.create_pen(kPsSolid, 1, 0x00ffffffu);
    REQUIRE(p != kPenNone);
    CHECK(b.select_pen(kDcWindow, p) == kPenWhite);
    b.move_to(kDcWindow, 3, 20);
    b.line_to(kDcWindow, 3, 25);
    CHECK(b.select_pen(kDcWindow, kPenBlack) == p);
    b.delete_pen(p);
    gfx.surface().flush();
    CHECK(gfx.surface().at(3, 20) == 255);
    CHECK(gfx.surface().at(3, 24) == 255);
    CHECK(gfx.surface().at(3, 25) == 0);
}
#endif
