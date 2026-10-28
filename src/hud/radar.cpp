// The radar: seg11:0x0 (its part), seg11:0x1ab6/0x1b44, seg11:0x3396, seg11:0x33cc,
// seg11:0x367c, the callbacks seg11:0x37a0/0x3914/0x39d8/0x3bae, and the zoom paths
// seg11:0x7215/0x7268/0x7798 (docs/re/world.md "Radar", "Scrollbar"). Widths as the original's
// (ADR 0013 decision 7): the mapping's products are 32-bit `imul` and the division seg5:0x110;
// the wrap deltas, the box sides and the refresh gate are 16-bit.
#include "hud/radar.h"

namespace is::hud {

using world::add16;
using world::div32;
using world::idiv16;
using world::kBlackness;
using world::lo16;
using world::mul16;
using world::sar16;
using world::sar32;
using world::shl16;
using world::sub16;

std::uint32_t radar_brush_colour(int i, bool deep) {
    // [BYTES seg11:0xf1-0x17b]: the values pushed for CreateSolidBrush, with the two
    // DGROUP:0x90f0 choices at seg11:0xf1 and seg11:0x131.
    switch (i) {
    case 0: return deep ? 0x02006000u : 0x02004000u;  // view frame [BYTES seg11:0xf8 seg11:0xff seg11:0x109]
    case 1: return 0x0040C0FFu;                       // [BYTES seg11:0x163 seg11:0x165]
    case 2: return deep ? 0x02FF8000u : 0x02FF3F00u;  // [BYTES seg11:0x138 seg11:0x13f seg11:0x149]
    case 3: return 0;                                 // METAL pattern brush [BYTES seg11:0xe9]
    case 4: return 0x00C0C0C0u;                       // [BYTES seg11:0x155]
    case 5: return 0x020040FFu;                       // [BYTES seg11:0x170]
    case 6: return 0x00FFA080u;                       // [BYTES seg11:0x123]
    case 7: return 0x02FFC800u;                       // [BYTES seg11:0x115]
    default: return 0;
    }
}

void Radar::create(world::Blitter& out, bool deep, BrushId metal) {
    if (created()) return;  // DGROUP:0x5042 != 0 [BYTES seg11:0xd]
    // 100 * DGROUP:0x5a0, 16 x 16 -> 32 `imul` [BYTES seg11:0x1b seg11:0x22 seg11:0x29 seg11:0x30].
    div_x = 100 * static_cast<std::int32_t>(world::kWorldCells);
    div_y = 100 * static_cast<std::int32_t>(world::kWorldCells);
    // The brush table in the original's creation order [BYTES seg11:0xe9-0x17b]: 3 (pattern),
    // 0, 7, 6, 2, 4, 1, 5.
    brushes[kRadarBrushMetal] = metal;
    for (const int i : {0, 7, 6, 2, 4, 1, 5})
        brushes[static_cast<std::size_t>(i)] = out.create_solid_brush(radar_brush_colour(i, deep));
    // CreateCompatibleBitmap(DGROUP:0x13dc, 86, 71) in DC DGROUP:0x93ce, and RADSHIP
    // (DGROUP:0x5046) in DC DGROUP:0x93cc [BYTES seg11:0x43 seg11:0xc6 seg11:0x182 seg11:0x19b].
    dc = out.create_image(kRadarW, kRadarH);
    ship_dc = out.load_bitmap("RADSHIP");
}

std::int32_t Radar::wrap_delta(std::int32_t v, std::int16_t centre_units,
                               std::int16_t world_cells) {
    // [BYTES seg11:0x1aff seg11:0x1b03 seg11:0x1b0b seg11:0x1b12-0x1b37]
    const std::int16_t a = lo16(sar32(v, 3));
    const std::int16_t c = sar16(centre_units, 3);
    std::int16_t r;
    if (a > c) {
        const std::int16_t t = add16(sub16(c, a), world_cells);  // the way round
        const std::int16_t u = sub16(a, c);
        r = t < u ? sub16(0, t) : u;
    } else {
        const std::int16_t t = add16(sub16(a, c), world_cells);
        const std::int16_t u = sub16(c, a);
        r = t < u ? t : sub16(0, u);
    }
    return r;  // `cwd`
}

std::int32_t Radar::wrap_x(const RadarInputs& in, std::int32_t v) const {
    // The centre: DGROUP:0x5a0 << 2 in a race, the player's x while DGROUP:0x951a = 0, else
    // DGROUP:0x6b50 [BYTES seg11:0x1ac4 seg11:0x1acb seg11:0x1adc seg11:0x1ae3 seg11:0x1af9].
    std::int16_t c;
    if (in.race != 0)
        c = shl16(world::kWorldCells, 2);
    else if (in.dg_951a == 0)
        c = in.pools.rec(in.ship_pool, in.player).x;
    else
        c = in.alt_centre_x;
    return wrap_delta(v, c, world::kWorldCells);
}

std::int32_t Radar::wrap_y(const RadarInputs& in, std::int32_t v) const {
    // [BYTES seg11:0x1b52 seg11:0x1b59 seg11:0x1b64 seg11:0x1b6b seg11:0x1b81]
    std::int16_t c;
    if (in.race != 0)
        c = shl16(world::kWorldCells, 2);
    else if (in.dg_951a == 0)
        c = in.pools.rec(in.ship_pool, in.player).y;
    else
        c = in.alt_centre_y;
    return wrap_delta(v, c, world::kWorldCells);
}

namespace {

// lo32(d * zoom * k) / div, `imul` twice then seg5:0x110 [BYTES seg11:0x347b-0x3489].
std::int16_t scale(std::int32_t d, std::int32_t zoom, std::int32_t k, std::int32_t div) {
    const std::uint32_t p = static_cast<std::uint32_t>(d) * static_cast<std::uint32_t>(zoom) *
                            static_cast<std::uint32_t>(k);
    const std::int32_t sp = p < 0x80000000u ? static_cast<std::int32_t>(p)
                                            : -static_cast<std::int32_t>(~p) - 1;
    return div == 0 ? std::int16_t{0} : lo16(div32(sp, div));
}

}  // namespace

world::Point16 Radar::map(const RadarInputs& in, std::int32_t x, std::int32_t y) const {
    // [BYTES seg11:0x3923 seg11:0x3954 seg11:0x3959 seg11:0x395e seg11:0x3963 seg11:0x3999]
    world::Point16 p;
    p.x = add16(scale(wrap_x(in, x), zoom, kRadarW, div_x), cx);
    p.y = add16(scale(wrap_y(in, y), zoom, kRadarH, div_y), cy);
    return p;
}

bool Radar::refresh(const RadarInputs& in, world::Blitter& out) {
    // [BYTES seg11:0x33a0 seg11:0x33a3 seg11:0x33a7 seg11:0x33aa seg11:0x33ad seg11:0x33b1
    // seg11:0x33b5 seg11:0x33b9 seg11:0x33bb seg11:0x33c2]
    const std::int16_t every =
        mul16(sar16(shl16(sub16(8, in.detail), 4), 6), refresh_div);
    counter = add16(counter, 1);
    if (every > counter) return false;
    counter = 0;
    repaint(in, out);
    return true;
}

void Radar::repaint(const RadarInputs& in, world::Blitter& out) {
    if (dc == world::kDcNone || !in.hud_gate) return;  // [BYTES seg11:0x33d9 seg11:0x33e1]
    // DGROUP:0x93d6 = min(26, lo16(DGROUP:0x9008 * 1000) / (zoom + 500)) [BYTES seg11:0x33eb
    // seg11:0x33ef seg11:0x33f7 seg11:0x33fd seg11:0x3400 seg11:0x3408 seg11:0x3410].
    refresh_div = lo16(div32(mul16(rate_word, 1000), zoom + 500));
    if (refresh_div > kRefreshCap) refresh_div = kRefreshCap;

    if (in.race != 0) {
        race_repaint(in, out);  // [BYTES seg11:0x3430 seg11:0x3438]
    } else {
        // Clear, and the view frame DGROUP:0x9158 times [BYTES seg11:0x343e seg11:0x3452
        // seg11:0x345f-0x3574] [OPEN why it repeats].
        out.pat_blt(dc, 0, 0, kRadarW, kRadarH, kBlackness);
        for (int k = 0; k < in.view_rects; ++k) {
            const std::int32_t far_x = static_cast<std::int32_t>(in.view_x) +
                                       shl16(in.play_w, in.shift);  // `add`/`adc` [BYTES seg11:0x34e0-0x34f0]
            const std::int32_t far_y = static_cast<std::int32_t>(in.view_y) +
                                       shl16(in.play_h, in.shift);
            const world::Point16 a = map(in, in.view_x, in.view_y);
            const world::Point16 b = map(in, far_x, far_y);
            out.frame_rect(dc, world::Rect16{a.x, a.y, b.x, b.y}, brush(0));
        }
        // The clip (1, 1, 85, 69) [BYTES seg11:0x357e seg11:0x35b2].
        clip = world::Rect16{
            sub16(1, sub16(kRadarW / 2, cx)), sub16(1, sub16(kRadarH / 2, cy)),
            sub16(add16(kRadarW / 2, cx), 1), sub16(add16(kRadarH / 2, cy), 1)};
        // The objects of pools DGROUP:0x9326 ... DGROUP:0x9324 in play with mass > 0
        // [BYTES seg11:0x35c7 seg11:0x35da seg11:0x35ef seg11:0x35f6 seg11:0x35fb seg11:0x3605
        // seg11:0x3624].
        const BrushId old = out.select_brush(dc, brush(0));
        for (int p = in.radar_first; p <= in.ship_pool; ++p) {
            if (p < 0 || p >= world::kPoolCount) continue;
            for (int r = 0; r < in.pools.in_use(p) && r < in.pools.records(p); ++r) {
                const world::ObjectRecord& o = in.pools.rec(p, r);
                if (o.state == 0 && o.mass > 0) draw_record(in, out, p, r);
            }
        }
        out.select_brush(dc, old);
    }
    // To (W/2 - 43, H' + 7) [BYTES seg11:0x3629-0x3655].
    out.bit_blt(in.window_dc, sub16(idiv16(in.play_w, 2), kRadarW / 2), add16(in.hud_top, 7),
                kRadarW, kRadarH, dc, 0, 0, world::kSrcCopy);
}

void Radar::race_repaint(const RadarInputs& in, world::Blitter& out) {
    // seg11:0x367c [BYTES seg11:0x367c seg11:0x36c3 seg11:0x36ed seg11:0x370a seg11:0x375f
    // seg11:0x3777 seg11:0x377f]
    clip = world::Rect16{
        sub16(1, sub16(kRadarW / 2, cx)), sub16(1, sub16(kRadarH / 2, cy)),
        sub16(add16(kRadarW / 2, cx), 1), sub16(add16(kRadarH / 2, cy), 1)};
    zoom = kZoomMin;
    const BrushId old = out.select_brush(dc, brush(0));
    // Restore each ship's last 5 x 5 dot from DGROUP:0x93d0.
    for (int k = 0; k <= in.last_ship && k < world::kShipCount; ++k) {
        const int px = sub16(dot_x[static_cast<std::size_t>(k)], 2);
        const int py = sub16(dot_y[static_cast<std::size_t>(k)], 2);
        out.bit_blt(dc, px, py, 5, 5, dots_dc, px, py, world::kSrcCopy);
    }
    // Pool 14's records in play, whatever their mass.
    for (int p = in.radar_first; p <= in.ship_pool; ++p) {
        if (p != world::kPoolShips) continue;
        for (int r = 0; r < in.pools.in_use(p) && r < in.pools.records(p); ++r)
            if (in.pools.rec(p, r).state == 0) draw_record(in, out, p, r);
    }
    out.select_brush(dc, old);
}

void Radar::draw_record(const RadarInputs& in, world::Blitter& out, int pool, int rec) {
    const world::ObjectRecord& o = in.pools.rec(pool, rec);
    const std::uint32_t id = o.radar_draw.get();
    // seg11:0x37a0 skips a ship whose byte +0x147 is set before mapping [BYTES seg11:0x37af
    // seg11:0x37ba].
    if (id == kRadarShip && pool == world::kPoolShips && rec < world::kShipCount &&
        in.ships[static_cast<std::size_t>(rec)].radar_hide != 0)
        return;
    if (id != kRadarShip && id != kRadarSmall && id != kRadarDot && id != kRadarBig)
        return;  // [OPEN] a callback the port does not know
    const world::Point16 p = map(in, o.x, o.y);
    const bool in_clip = world::pt_in_rect(clip, p.x, p.y);  // PtInRect(DGROUP:0x5048)
    const std::int16_t zoom16 = static_cast<std::int16_t>(zoom);
    if (id == kRadarSmall) {
        // [BYTES seg11:0x39b6 seg11:0x39c3 seg11:0x39c8 seg11:0x39d5]
        if (in_clip) out.set_pixel(dc, p.x, p.y, o.radar_color.get());
    } else if (id == kRadarDot) {
        // (zoom - 200)/150 + 2 by (zoom - 200)/200 + 2, 16-bit; the brush stays selected
        // [BYTES seg11:0x3a81 seg11:0x3a8b seg11:0x3a99 seg11:0x3aa5 seg11:0x3ab0 seg11:0x3aba].
        if (in_clip) {
            out.select_brush(dc, brush(o.radar_brush));
            out.pat_blt(dc, p.x, p.y, add16(idiv16(sub16(zoom16, 200), 150), 2),
                        add16(idiv16(sub16(zoom16, 200), 200), 2), world::kPatCopy);
        }
    } else if (id == kRadarBig) {
        // A pixel while the dword zoom < 201, else a square [BYTES seg11:0x3c50 seg11:0x3c62
        // seg11:0x3c7d seg11:0x3c8f seg11:0x3cad seg11:0x3cb2].
        if (in_clip) {
            if (zoom < 201) {
                out.set_pixel(dc, p.x, p.y, o.radar_color.get());
            } else {
                out.select_brush(dc, brush(o.radar_brush));
                const std::int16_t side = add16(idiv16(sub16(zoom16, 200), 150), 2);
                out.pat_blt(dc, p.x, p.y, side, side, world::kPatCopy);
            }
        }
    } else {
        // seg11:0x37a0: WHITE_BRUSH for the player, else brush +0x46; MERGECOPY of a 5 x 5
        // RADSHIP cell at heading * 5 (pool 7: 4 x 4 at 80) to (x - 2, y - 2)
        // [BYTES seg11:0x3866 seg11:0x3884 seg11:0x3899 seg11:0x38c0 seg11:0x38df seg11:0x38e3
        // seg11:0x38e9].
        if (in_clip) {
            const bool own = pool == world::kPoolShips && rec == in.player;
            out.select_brush(dc, own ? world::kBrushWhite : brush(o.radar_brush));
            const int side = pool == 7 ? 4 : 5;
            const int sx = pool == 7 ? 80 : mul16(o.frame, 5);
            out.bit_blt(dc, sub16(p.x, 2), sub16(p.y, 2), side, side, ship_dc, sx, 0,
                        world::kMergeCopy);
        }
        // In a race the mapped point is kept per record, inside the clip or not
        // [BYTES seg11:0x38ee seg11:0x3901 seg11:0x3905]; the table holds one entry per ship
        // [OPEN past record 7].
        if (in.race != 0 && rec >= 0 && rec < world::kShipCount) {
            dot_x[static_cast<std::size_t>(rec)] = p.x;
            dot_y[static_cast<std::size_t>(rec)] = p.y;
        }
    }
}

void Radar::zoom_key(bool plus, const RadarInputs& in, world::Blitter& out) {
    // [BYTES seg11:0x7215 seg11:0x7219 seg11:0x7226 seg11:0x722e seg11:0x7233 seg11:0x7268
    // seg11:0x726c seg11:0x7279 seg11:0x7281]
    if (plus) {
        zoom += 8;
        if (zoom > kZoomMax) zoom = kZoomMax;
    } else {
        zoom -= 8;
        if (zoom < kZoomMin) zoom = kZoomMin;
    }
    zoom_changed(zoom, in, out);
}

void Radar::zoom_changed(std::int32_t new_zoom, const RadarInputs& in, world::Blitter& out) {
    zoom = new_zoom;
    // SetScrollPos first, then the repaint [BYTES seg11:0x723a seg11:0x7242 seg11:0x7247].
    if (set_scroll_pos) set_scroll_pos(scroll_pos_of(zoom));
    repaint(in, out);
}

bool Radar::scroll(int code, int pos, const RadarInputs& in, world::Blitter& out) {
    const std::int32_t before = zoom;  // [BYTES seg11:0x77a5 seg11:0x77ad]
    switch (code) {
    case 0: zoom += 20; break;   // SB_LINEUP [BYTES seg11:0x77be]
    case 1: zoom -= 20; break;   // SB_LINEDOWN [BYTES seg11:0x77b6]
    case 4:                      // SB_THUMBPOSITION
    case 5:                      // SB_THUMBTRACK: 600 - (unsigned) pos [BYTES seg11:0x77dc-0x77e9]
        zoom = 600 - static_cast<std::int32_t>(static_cast<std::uint16_t>(pos));
        break;
    case 6: zoom = kZoomMax; break;  // SB_TOP [BYTES seg11:0x77c6]
    case 7: zoom = kZoomMin; break;  // SB_BOTTOM [BYTES seg11:0x77d4]
    default: break;                  // SB_PAGEUP, SB_PAGEDOWN and the rest
    }
    if (zoom > kZoomMax) zoom = kZoomMax;  // [BYTES seg11:0x780c seg11:0x781f]
    else if (zoom < kZoomMin) zoom = kZoomMin;
    if (zoom == before) return false;  // [BYTES seg11:0x7837]
    repaint(in, out);                  // [BYTES seg11:0x783d]
    if (set_scroll_pos) set_scroll_pos(scroll_pos_of(zoom));  // [BYTES seg11:0x7852]
    return true;
}

}  // namespace is::hud
