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
class World;
class ShiftHost;

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
    std::int16_t cam_x = 0, cam_y = 0;      // DGROUP:0x6b50, 0x6b52, the pan point (DGROUP:0x951a)
    // DGROUP:0x2394, 0x2396: the pan's last step, (lo16(12 d) / dist) << 3 per axis, written only
    // on the frames that still pan [BYTES seg8:0x186-0x1a1] (the stars read them, draw.h).
    std::int16_t pan_dx = 0, pan_dy = 0;
};

// seg8:0x0 with no screen shake (DGROUP:0x238c = 0) [BYTES seg8:0x14-0x62 seg8:0x248
// seg8:0x3a7-0x757]: the origin from the player's (x, y) while DGROUP:0x951a = 0, or the pan of
// a perspective shift (pan_view) while it is set; then the far corner, the activity rectangles
// round the player or, while DGROUP:0x951a is still set after the pan, round the pan point
// [BYTES seg8:0x3c4-0x3ef], and the sign-extended copies. The shake (seg8:0x251-0x3a5, which
// draws rand() and plays sounds) is [OPEN]: `shake` reports that the shake path was due and not
// run. Without `world` and `shift` the DGROUP:0x951a case centres the pan point and does not
// move it (the frame's stand-in before the shift's binding) [DESIGN].
struct ViewInputs {
    std::int16_t player_x = 0, player_y = 0;
    std::int16_t autopilot = 0;     // DGROUP:0x951a
    std::int16_t shake = 0;         // DGROUP:0x238c
    std::int16_t wave_kind = 0;     // DGROUP:0x9320
    std::uint8_t dg_9eb5 = 0;       // DGROUP:0x9eb5
    // The pan's words and calls (shift.h): the program's World, its ShiftHost, the frame's copies
    // of DGROUP:0x9646 / 0x9134 / 0x95bc / 0x951a (nullable) and DGROUP:0x1e50.
    World* world = nullptr;
    ShiftHost* shift = nullptr;
    WaveGlobals* frame = nullptr;
    std::uint32_t now = 0;
};
void recentre(ViewState& v, const ViewInputs& in);

// seg8:0x65-0x247, the pan while DGROUP:0x951a != 0 [BYTES seg8:0x65-0x247] (ships.md "The pan"):
//  1. the player's object out of play (`state` != 0) or dead (+0xec = 0): seg11:0xaece again
//     (shift.h perspective_shift) [BYTES seg8:0x65-0x89];
//  2. d = seg8:0x285a(player (x, y), DGROUP:0x6b50 (x, y), &dx, &dy) in 1/8-unit cells
//     (world.h distance_cells), dx and dy toward the player [BYTES seg8:0x8e-0xc5];
//  3. d < 16: the origin centres the player, PatBlt BLACKNESS over its cell (object +0x00, +0x02,
//     `size`), seg7:0x11cc, PlayMode 2 -> 0 and 3 -> 1 with DGROUP:0x95bc = 1, 4 -> 1, then
//     PlayMode = DGROUP:0x9378 in every case, DGROUP:0x951a = 0, seg7:0x1208, DGROUP:0xa76 = 2 and
//     WM_PAINT to DGROUP:0xa7c [BYTES seg8:0xc8-0x170];
//  4. d >= 16: DGROUP:0x2394 = (lo16(dx 12) / d) << 3, DGROUP:0x2396 likewise (signed 16-bit
//     idiv), the pan point moves by them, wrapped into 0 ... 8 DGROUP:0x5a0 (x first by the sign
//     of its add, then y), the origin centres the pan point, and with the player's object not in
//     `state` 0 DGROUP:0x1e80 = now + 2000, for `state` 1 also DGROUP:0x1e84 = now + 1500 [BYTES
//     seg8:0x186-0x244].
// Returns DGROUP:0x951a after the pan, so the caller picks the activity rectangles' point.
std::int16_t pan_view(ViewState& v, World& w, ShiftHost& h, WaveGlobals* frame, std::uint32_t now);

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
