// The view (view.h).
#include "world/view.h"

#include "world/flight.h"

namespace is::world {

namespace {

// `cwd; sub ax, dx; sar ax, 1`: a signed halving toward zero.
std::int16_t half(std::int16_t v) { return idiv16(v, 2); }

}  // namespace

// seg8:0x0 [BYTES seg8:0x14-0x62 seg8:0x3a7-0x757]
void recentre(ViewState& v, const ViewInputs& in) {
    const std::int16_t ww = shl16(v.play_w, v.shift);  // [bp-0x1c]
    const std::int16_t hw = shl16(v.play_h, v.shift);  // [bp-0x1a]
    std::int16_t px = in.player_x, py = in.player_y;
    if (in.autopilot == 0) {
        v.x0 = sub16(px, half(ww));  // -(Ww/2 - x)
        v.y0 = sub16(py, half(hw));
    } else {
        // The 0x951a camera seg8:0x65-0x247 (target pan, seg11:0xaece, PlayMode switch) [OPEN]:
        // the port keeps the camera point as the origin's centre.
        px = v.cam_x;
        py = v.cam_y;
        v.x0 = sub16(px, half(ww));
        v.y0 = sub16(py, half(hw));
    }
    // seg8:0x248: with DGROUP:0x238c the shake path seg8:0x251-0x3a5 runs [OPEN]; without it
    // the shake words are cleared [BYTES seg8:0x3a7].
    if (in.shake == 0) {
        v.shake_x = 0;
        v.shake_y = 0;
    }
    v.x1 = add16(ww, v.x0);  // [BYTES seg8:0x3b0-0x3c1]
    v.y1 = add16(hw, v.y0);
    activity_rects(v, px, py, in.wave_kind, in.dg_9eb5);
    v.x0l = v.x0;  // movsx [BYTES seg8:0x747-0x757]
    v.y0l = v.y0;
}

// [BYTES seg8:0x3f2-0x741]
void activity_rects(ViewState& v, std::int16_t px, std::int16_t py, std::int16_t wave_kind,
                    std::uint8_t dg_9eb5) {
    const std::int16_t ww = shl16(v.play_w, v.shift);
    const std::int16_t hw = shl16(v.play_h, v.shift);
    const std::int16_t wx = kWorldUnits;  // DGROUP:0x5a0 << 3, [bp-0x18]
    const std::int16_t wy = kWorldUnits;  // DGROUP:0x5a2 << 3, [bp-0x16]
    const std::int16_t m = 600;           // [bp-0xc]
    auto& r = v.active;
    const std::int16_t hx = half(ww), hy = half(hw);
    const std::int16_t mm = sub16(0, m);
    if (wave_kind == 6 && dg_9eb5 != 1) {  // [BYTES seg8:0x409-0x438]
        r[0] = {sub16(v.x0, m), sub16(v.y0, m), add16(v.x1, m), add16(v.y1, m)};
        v.active_count = 1;
        return;
    }
    const bool near_top = add16(hy, m) > py;                       // Hw/2 + m > y
    const bool near_bottom = sub16(sub16(wy, hy), m) < py;         // Wy - Hw/2 - m < y
    if (add16(hx, m) > px) {  // near the left edge [BYTES seg8:0x43b-0x55a]
        const std::int16_t r0r = add16(m, v.x1);
        const std::int16_t r1l = add16(sub16(sub16(wx, hx), m), px);
        const std::int16_t r1r = add16(wx, m);
        r[0].left = mm;
        r[0].right = r0r;
        r[1].left = r1l;
        r[1].right = r1r;
        if (near_top) {  // [BYTES seg8:0x491-0x4c9 seg8:0x51c]
            r[2].left = r1l, r[2].right = r1r;
            r[3].left = mm, r[3].right = r0r;
            r[0].top = r[1].top = mm;
            r[2].top = r[3].top = add16(sub16(sub16(wy, hy), m), py);
            r[0].bottom = r[1].bottom = add16(m, v.y1);
            r[2].bottom = r[3].bottom = add16(wy, m);
            v.active_count = 4;
        } else if (near_bottom) {  // [BYTES seg8:0x4e2-0x51c]
            r[2].left = r1l, r[2].right = r1r;
            r[3].left = mm, r[3].right = r0r;
            r[0].top = r[1].top = mm;
            r[2].top = r[3].top = sub16(v.y0, m);
            r[0].bottom = r[1].bottom = add16(add16(sub16(hy, wy), py), m);
            r[2].bottom = r[3].bottom = add16(wy, m);
            v.active_count = 4;
        } else {  // [BYTES seg8:0x52b-0x551]
            r[0].top = r[1].top = sub16(sub16(py, hy), m);
            r[0].bottom = r[1].bottom = add16(add16(hy, py), m);
            v.active_count = 2;
        }
        return;
    }
    if (sub16(sub16(wx, hx), m) < px) {  // near the right edge [BYTES seg8:0x55a-0x64c]
        const std::int16_t r0l = sub16(v.x0, m);
        const std::int16_t r0r = add16(wx, m);
        const std::int16_t r1r = add16(add16(sub16(hx, wx), px), m);
        r[0].left = r0l;
        r[0].right = r0r;
        r[1].left = mm;
        r[1].right = r1r;
        if (near_top) {  // [BYTES seg8:0x5b2-0x5ca seg8:0x4a9-0x528]
            r[2].left = mm, r[2].right = r1r;
            r[3].left = r0l, r[3].right = r0r;
            r[0].top = r[1].top = mm;
            r[2].top = r[3].top = add16(sub16(sub16(wy, hy), m), py);
            r[0].bottom = r[1].bottom = add16(m, v.y1);
            r[2].bottom = r[3].bottom = add16(wy, m);
            v.active_count = 4;
        } else if (near_bottom) {  // [BYTES seg8:0x5e4-0x62a]
            r[2].left = mm, r[2].right = r1r;
            r[3].left = r0l, r[3].right = r0r;
            r[0].top = r[1].top = sub16(v.y0, m);
            r[2].top = r[3].top = mm;
            r[0].bottom = r[1].bottom = add16(wy, m);
            r[2].bottom = r[3].bottom = add16(add16(sub16(hy, wy), py), m);
            v.active_count = 4;
        } else {  // [BYTES seg8:0x633-0x64c]
            r[0].top = r[1].top = sub16(v.y0, m);
            r[0].bottom = r[1].bottom = add16(m, v.y1);
            v.active_count = 2;
        }
        return;
    }
    if (near_top) {  // [BYTES seg8:0x661-0x6b1]
        const std::int16_t l = sub16(sub16(px, hx), m);
        const std::int16_t rr = add16(add16(l, add16(m, m)), ww);
        r[0].left = r[1].left = l;
        r[0].right = r[1].right = rr;
        r[0].top = mm;
        r[1].top = add16(sub16(sub16(wy, hy), m), py);
        r[0].bottom = add16(m, v.y1);
        r[1].bottom = add16(wy, m);
        v.active_count = 2;
        return;
    }
    if (near_bottom) {  // [BYTES seg8:0x6cb-0x718]
        const std::int16_t l = sub16(sub16(px, hx), m);
        const std::int16_t rr = add16(add16(l, add16(m, m)), ww);
        r[0].left = r[1].left = l;
        r[0].right = r[1].right = rr;
        r[0].top = sub16(v.y0, m);
        r[1].top = mm;
        r[0].bottom = add16(wy, m);
        r[1].bottom = add16(add16(sub16(hy, wy), py), m);
        v.active_count = 2;
        return;
    }
    // [BYTES seg8:0x71b-0x741]
    r[0] = {sub16(v.x0, m), sub16(v.y0, m), add16(m, v.x1), add16(m, v.y1)};
    v.active_count = 1;
}

// seg8:0x29ea [BYTES seg8:0x29f9-0x2b4f]
ScreenPoint world_to_screen(const ViewState& v, std::int16_t x, std::int16_t y,
                            std::int16_t wave_kind, std::uint8_t dg_9eb5, std::int16_t dg_9130) {
    const int s = v.shift;
    const std::int32_t xl = x, yl = y;
    ScreenPoint p;
    p.x = lo16(sar32(static_cast<std::int32_t>(static_cast<std::uint32_t>(xl) -
                                               static_cast<std::uint32_t>(v.x0l)),
                     s));
    if (dg_9130 != 0) {  // seg8:0x2b51 [OPEN]
        p.y = lo16(sar32(static_cast<std::int32_t>(static_cast<std::uint32_t>(yl) -
                                                   static_cast<std::uint32_t>(v.y0l)),
                         s));
        return p;
    }
    const bool wrap_ok = !(wave_kind == 6 && dg_9eb5 != 1);
    const std::int16_t wcells = kWorldCells;
    // x: |x - (W << (s - 1)) - X0| > 0x5a0 * 4, all 16-bit [BYTES seg8:0x2a2f-0x2a53]
    const std::int16_t dx = sub16(sub16(x, shl16(v.play_w, s - 1)), lo16(v.x0l));
    if (abs16(dx) > shl16(wcells, 2) && wrap_ok) {
        const std::int32_t w = shl16(wcells, 3);
        const std::int32_t t = p.x < 0 ? w + xl : xl - w;  // [BYTES seg8:0x2a63-0x2a8f]
        p.x = lo16(sar32(static_cast<std::int32_t>(static_cast<std::uint32_t>(t) -
                                                   static_cast<std::uint32_t>(v.x0l)),
                         s));
    }
    p.y = lo16(sar32(static_cast<std::int32_t>(static_cast<std::uint32_t>(yl) -
                                               static_cast<std::uint32_t>(v.y0l)),
                     s));
    // y: the same test on a 32-bit difference [BYTES seg8:0x2abf-0x2afc]
    const std::int32_t hh = shl16(v.play_h, s - 1);
    const std::int32_t dy = static_cast<std::int32_t>(static_cast<std::uint32_t>(yl) -
                                                      static_cast<std::uint32_t>(hh) -
                                                      static_cast<std::uint32_t>(v.y0l));
    if (abs32(dy) > static_cast<std::int32_t>(shl16(wcells, 2)) && wrap_ok) {
        const std::int32_t w = shl16(wcells, 3);
        const std::int32_t t = p.y < 0 ? w + yl : yl - w;
        p.y = lo16(sar32(static_cast<std::int32_t>(static_cast<std::uint32_t>(t) -
                                                   static_cast<std::uint32_t>(v.y0l)),
                         s));
    }
    return p;
}

ScreenPoint pinned_player(const ViewState& v) { return {half(v.play_w), half(v.play_h)}; }

}  // namespace is::world
