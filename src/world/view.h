// The view (docs/re/world.md "View"): the origin seg8:0x0 that keeps the player at the play
// area's centre, the activity rectangles DGROUP:0x23e0, world to screen seg8:0x29ea and the
// pinned player seg7:0x11cc. All arithmetic keeps the original's widths (ADR 0013 decision 7).
// Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>

#include "world/records.h"

namespace is::world {

struct WaveGlobals;

// A RECT of 16-bit coordinates (left, top, right, bottom), as DGROUP:0x23e0 holds them.
struct ViewRect {
    std::int16_t left = 0, top = 0, right = 0, bottom = 0;
    bool operator==(const ViewRect& o) const {
        return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
    }
};

// The view's DGROUP words.
struct ViewState {
    std::int16_t play_w = 0;   // W, DGROUP:0x90ea
    std::int16_t play_h = 0;   // H, DGROUP:0x90ec
    std::int16_t shift = 3;    // s, DGROUP:0x9150 = 4 - Size
    std::int16_t x0 = 0, y0 = 0;    // the origin DGROUP:0x2400, 0x2402
    std::int16_t x1 = 0, y1 = 0;    // the far corner DGROUP:0x2404, 0x2406
    std::int32_t x0l = 0, y0l = 0;  // sign-extended copies DGROUP:0x240c, 0x2410
    std::array<ViewRect, 4> active{};  // the activity rectangles DGROUP:0x23e0
    std::int16_t active_count = 1;  // DGROUP:0x9158
    std::int16_t shake_x = 0, shake_y = 0;  // DGROUP:0x2390, 0x2392
    std::int16_t cam_x = 0, cam_y = 0;      // DGROUP:0x6b50, 0x6b52 (the 0x951a camera)
};

// The pass of seg8:0x0 that runs while DGROUP:0x951a = 0, with no screen shake
// (DGROUP:0x238c = 0) [BYTES seg8:0x14-0x62 seg8:0x248 seg8:0x3a7-0x757]: the origin from the
// player's (x, y), the far corner, the activity rectangles, the sign-extended copies.
// The 0x951a camera (seg8:0x65-0x247) and the shake (seg8:0x251-0x3a5, which draws rand() and
// plays sounds) are [OPEN]: `shake` reports that the shake path was due and not run.
struct ViewInputs {
    std::int16_t player_x = 0, player_y = 0;
    std::int16_t autopilot = 0;     // DGROUP:0x951a
    std::int16_t shake = 0;         // DGROUP:0x238c
    std::int16_t wave_kind = 0;     // DGROUP:0x9320
    std::uint8_t dg_9eb5 = 0;       // DGROUP:0x9eb5
};
void recentre(ViewState& v, const ViewInputs& in);

// The activity rectangles alone, from the origin, far corner and player (seg8:0x3f2-0x741), and
// their count; separated for tests.
void activity_rects(ViewState& v, std::int16_t px, std::int16_t py, std::int16_t wave_kind,
                    std::uint8_t dg_9eb5);

// seg8:0x29ea: a world point to the screen, (x - X0) >> s in 32 bits, with the torus wrap; the
// x test is on a 16-bit difference and a 16-bit abs, the y test on a 32-bit difference and a
// 32-bit abs [BYTES seg8:0x2a0e-0x2b4f]. No wrap in kind-6 waves before DGROUP:0x9eb5 = 1. The
// seg18 path (DGROUP:0x9130 != 0, seg8:0x2b51) is [OPEN]: the port returns the unwrapped point.
struct ScreenPoint {
    std::int16_t x = 0, y = 0;
};
ScreenPoint world_to_screen(const ViewState& v, std::int16_t x, std::int16_t y,
                            std::int16_t wave_kind, std::uint8_t dg_9eb5, std::int16_t dg_9130);

// seg7:0x11cc: the player's screen position, (W / 2, H / 2) [BYTES seg7:0x11e8 seg7:0x11fc].
ScreenPoint pinned_player(const ViewState& v);

}  // namespace is::world
