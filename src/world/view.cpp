// The view (view.h).
#include "world/view.h"

#include "world/flight.h"
#include "world/shift.h"
#include "world/world.h"

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
    std::int16_t autopilot = in.autopilot;
    if (autopilot == 0) {
        v.x0 = sub16(px, half(ww));  // -(Ww/2 - x) [BYTES seg8:0x29-0x5f]
        v.y0 = sub16(py, half(hw));
    } else if (in.world && in.shift) {
        autopilot = pan_view(v, *in.world, *in.shift, in.frame, in.now);  // [BYTES seg8:0x65-0x247]
        // The activity point: DGROUP:0x9646's object while DGROUP:0x951a is 0 after the pan, the
        // pan point while it is set [BYTES seg8:0x3c4-0x3ef].
        if (autopilot == 0) {
            const World& w = *in.world;
            if (w.player >= 0 && w.pools.records(w.roles.ships) > w.player) {
                px = w.pools.rec(w.roles.ships, w.player).x;
                py = w.pools.rec(w.roles.ships, w.player).y;
            }
        }
    } else {
        // Without the shift's binding the pan point only centres the view [DESIGN].
        v.x0 = sub16(v.cam_x, half(ww));
        v.y0 = sub16(v.cam_y, half(hw));
    }
    if (autopilot != 0) {
        px = v.cam_x;
        py = v.cam_y;
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

std::int16_t pan_view(ViewState& v, World& w, ShiftHost& h, WaveGlobals* frame, std::uint32_t now) {
    const std::int16_t ww = shl16(v.play_w, v.shift);  // [bp-0x1c]
    const std::int16_t hw = shl16(v.play_h, v.shift);  // [bp-0x1a]
    const int ps = w.roles.ships;  // DGROUP:0x9324 through DGROUP:0x1df8
    const auto in_records = [&w, ps](int i) {
        return i >= 0 && i < kShipCount && w.pools.records(ps) > i;
    };
    if (!in_records(w.player)) {
        static bool once = false;
        guard_todo(once, "seg8:0x0's pan with DGROUP:0x9646 outside the ship records", "seg8:0x65");
        return w.w951a;
    }
    // 1. [BYTES seg8:0x65-0x89]
    if (w.pools.rec(ps, w.player).state != 0 || w.ship(w.player).bec == 0) perspective_shift(w, v, h, frame);
    if (!in_records(w.player)) return w.w951a;
    // 2. [BYTES seg8:0x8e-0xc5]: the player read again after seg11:0xaece.
    ObjectRecord& o = w.pools.rec(ps, w.player);
    std::int16_t dx = 0, dy = 0;  // [bp-4], [bp-6]
    const std::int16_t d = distance_cells(o.x, o.y, v.cam_x, v.cam_y, dx, dy);  // [bp-8]
    if (d < 16) {
        // 3. [BYTES seg8:0xcf-0x170]; [bp-0xa] is 0 on this path, so y0 = y - Hw / 2.
        v.x0 = sub16(o.x, half(ww));
        v.y0 = sub16(o.y, half(add16(shl16(0, v.shift), hw)));
        // GDI.29 PatBlt(DGROUP:0x13dc, +0x00, +0x02, +0x22, +0x22, BLACKNESS 0x00000042) [BYTES seg8:0x10c-0x123]
        h.black_cell(o.scr_x, o.scr_y, o.size, o.size);
        h.pin_player();                                   // seg7:0x11cc [BYTES seg8:0x128]
        switch (w.play_mode) {                            // [BYTES seg8:0x12d-0x184]
        case 2:
            w.play_mode = 0;
            w.player_game = 1;
            break;
        case 3:
            w.play_mode = 1;
            w.player_game = 1;
            break;
        case 4:
            w.play_mode = 1;
            break;
        default:
            break;
        }
        w.play_mode = shift_words().saved_mode;  // DGROUP:0x9378 [BYTES seg8:0x148-0x14b]
        w.w951a = 0;                             // [BYTES seg8:0x14e]
        if (frame) {
            frame->play_mode = w.play_mode;
            frame->demo = w.player_game;
            frame->autopilot = 0;
        }
        h.draw_player();  // seg7:0x1208 [BYTES seg8:0x154]
        h.info_mode(2);   // DGROUP:0xa76 = 2, PostMessage(DGROUP:0xa7c, WM_PAINT) [BYTES seg8:0x159-0x16b]
        return 0;
    }
    // 4. [BYTES seg8:0x186-0x20b]: `imul ax, [bp-4], 0xc; cdq; idiv [bp-8]; shl ax, 3`.
    v.pan_dx = shl16(idiv16(mul16(dx, 12), d), 3);
    v.pan_dy = shl16(idiv16(mul16(dy, 12), d), 3);
    const std::int16_t wx = shl16(kWorldCells, 3);  // DGROUP:0x5a0 << 3
    const std::int16_t wy = shl16(kWorldCells, 3);  // DGROUP:0x5a2 << 3
    v.cam_y = add16(v.cam_y, v.pan_dy);
    v.cam_x = add16(v.cam_x, v.pan_dx);
    if (v.cam_x < 0) v.cam_x = add16(wx, v.cam_x);  // `jns` on the add [BYTES seg8:0x1ac-0x1b8]
    if (wx < v.cam_x) v.cam_x = sub16(v.cam_x, wx);  // [BYTES seg8:0x1bb-0x1c7]
    if (v.cam_y < 0) v.cam_y = add16(v.cam_y, wy);  // [BYTES seg8:0x1cb-0x1d8]
    if (wy < v.cam_y) v.cam_y = sub16(v.cam_y, wy);  // [BYTES seg8:0x1dc-0x1e8]
    v.x0 = sub16(v.cam_x, half(ww));
    v.y0 = sub16(v.cam_y, half(hw));
    // [BYTES seg8:0x20e-0x244]
    if (o.state != 0) {
        w.w1e80 = now + 2000u;
        if (o.state == 1) w.w1e84 = now + 1500u;
    }
    return w.w951a;
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
