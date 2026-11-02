// seg8:0xb76, the wave's sprite routine (docs/re/world.md "Sprites"), and RecordingBlitter.
// Every coordinate is a 16-bit word as in the original (ADR 0013 decision 7): differences,
// sums and the absolute value seg9:0x88ba wrap in 16 bits.
#include "world/sprite.h"

#include <algorithm>

namespace is::world {

namespace {

// seg9:0x88ba: |v| of a 16-bit word (`neg`, so |-32768| = -32768) [BYTES seg9:0x88c4
// seg9:0x88d2].
std::int16_t abs16(std::int16_t v) { return v < 0 ? sub16(0, v) : v; }

// The alternate source of seg8:0xb76: the DC word +0x3c, with the cell at (+0x24, +0x24).
void take_alternate(ObjectRecord& o, DcId& src) {
    src = o.alt_dc;
    o.src_x = o.gap;
    o.src_y = o.gap;
}

bool special_mode(const SpriteGlobals& g) {
    // DGROUP:0x9e49 = 3, or 6 with DGROUP:0x9e47 = 15 [BYTES seg8:0xcfd seg8:0xd04 seg8:0xd0b
    // seg8:0x112c seg8:0x1133 seg8:0x113a].
    return g.dg_9e49 == 3 || (g.dg_9e49 == 6 && g.dg_9e47 == 0xf);
}

}  // namespace

bool erase_and_merge(Blitter& out, DcId dst, DcId src, std::int16_t x, std::int16_t y,
                     std::int16_t old_x, std::int16_t old_y, std::int16_t size,
                     std::int16_t gap, std::int16_t src_x, std::int16_t src_y,
                     std::uint32_t rop) {
    // The move, lowered by 2 more when negative under MERGECOPY [BYTES seg8:0xdfd seg8:0xe02
    // seg8:0xe0d seg8:0xe14 seg8:0xe24 seg8:0xe2c].
    std::int16_t dx = sub16(x, old_x);
    std::int16_t dy = sub16(y, old_y);
    if (rop == kMergeCopy) {
        if (dx < 0) dx = sub16(dx, 2);
        if (dy < 0) dy = sub16(dy, 2);
    }
    const std::int16_t adx = abs16(dx), ady = abs16(dy);

    // Within the gap: one larger blit that covers the old cell too, no erase [BYTES seg8:0xe47
    // seg8:0xe61 seg8:0x104e].
    if (adx <= gap && ady <= gap) {
        const std::int16_t ox = dx < 0 ? std::int16_t{0} : dx;
        const std::int16_t oy = dy < 0 ? std::int16_t{0} : dy;
        out.bit_blt(dst, sub16(x, ox), sub16(y, oy), add16(adx, size), add16(ady, size), src,
                    sub16(src_x, ox), sub16(src_y, oy), rop);
        return true;
    }

    // Within the size: black the L-shaped strips the new cell leaves of the old one, a
    // vertical strip beside the cell and a horizontal one above or below it [BYTES seg8:0xe80
    // seg8:0xe9e seg8:0xea7 seg8:0xeb5 seg8:0xf96 seg8:0x1015].
    if (adx < size && ady < size) {
        if (dx < 0) {
            if (dy < 0) {
                out.pat_blt(dst, add16(size, x), y, sub16(0, dx), sub16(size, dy), kBlackness);
                out.pat_blt(dst, x, add16(y, size), size, sub16(0, dy), kBlackness);
            } else {
                out.pat_blt(dst, add16(size, x), sub16(y, dy), sub16(0, dx), add16(size, dy),
                            kBlackness);
                out.pat_blt(dst, x, sub16(y, dy), size, dy, kBlackness);
            }
        } else {
            if (dy < 0) {
                out.pat_blt(dst, sub16(x, dx), y, dx, sub16(size, dy), kBlackness);
                out.pat_blt(dst, x, add16(y, size), size, sub16(0, dy), kBlackness);
            } else {
                out.pat_blt(dst, sub16(x, dx), sub16(y, dy), dx, add16(size, dy), kBlackness);
                out.pat_blt(dst, x, sub16(y, dy), size, dy, kBlackness);
            }
        }
        return false;
    }

    // Beyond: black the whole old cell [BYTES seg8:0xff1 seg8:0x1015].
    out.pat_blt(dst, old_x, old_y, size, size, kBlackness);
    return false;
}

int draw_sprite(Pools& pools, std::int16_t pool_rec, std::int16_t kind, DcId src_dc,
                std::int16_t masked, SpriteGlobals& g, const ToScreen& to_screen,
                Blitter& out) {
    const int pool = pool_of(pool_rec);  // byte [bp+0xd]
    const int rec = rec_of(pool_rec);    // byte [bp+0xc] [BYTES seg8:0xb8a seg8:0xb92]
    ObjectRecord& o = pools.rec(pool, rec);
    bool draw = true;  // [bp-0x16]

    // 1. Early exit [BYTES seg8:0xba8 seg8:0xbaf seg8:0xbb4 seg8:0xbba seg8:0xbc0 seg8:0x123b].
    if (o.on_screen == 0 && pool != 7 && kind != 4 && kind != 5 && o.owner_pool == -1) return 0;

    const bool is_player = pool == g.ship_pool && rec == g.player;

    // The pinned player: no new position and no erase, a SRCCOPY blit at the stored position
    // whatever `masked` says [BYTES seg8:0xbc9 seg8:0xbe0 seg8:0x10c9-0x1236].
    if (is_player && g.dg_951a == 0 && g.dg_238e == 0) {
        o.w3a = static_cast<std::int16_t>(src_dc);  // +0x3a keeps the DC passed in [BYTES seg8:0x10c9]
        if (o.b2e != 0 && o.b2e != 0xfe) {
            take_alternate(o, src_dc);
            if (o.b2e == 0xff) draw = false;  // [BYTES seg8:0x1117 seg8:0x111e]
        }
        if (g.dg_952e == g.player && special_mode(g)) take_alternate(o, src_dc);
        if (g.dg_913a == 2 || g.dg_913c == 2) take_alternate(o, src_dc);  // [BYTES seg8:0x116e]
        if (!draw) return 1;
        out.bit_blt(g.window_dc, o.scr_x, o.scr_y, o.size, o.size, src_dc, o.src_x, o.src_y,
                    kSrcCopy);  // [BYTES seg8:0x11ea]
        if (o.b2e != 0xfe) return 1;
        // The mask: ROP 0x00A000C9 with brush DGROUP:0x5ea8 over the cell [BYTES seg8:0x11f1
        // seg8:0x1200 seg8:0x121f seg8:0x1225 seg8:0x1231].
        const BrushId old = out.select_brush(g.window_dc, g.player_mask_brush);
        out.pat_blt(g.window_dc, o.scr_x, o.scr_y, o.size, o.size, kDpa);
        out.select_brush(g.window_dc, old);
        return 1;
    }

    // 2. Position.
    if (is_player && g.dg_951a == 0) {
        // DGROUP:0x238e != 0: pin to (W/2, H/2) unless the screen shakes; the old position is
        // not saved [BYTES seg8:0xc4b seg8:0xc52 seg8:0xc58 seg8:0xc6f seg8:0xc75 seg8:0xc7f].
        if (g.dg_238c == 0) {
            o.scr_x = idiv16(g.play_w, 2);
            o.scr_y = idiv16(g.play_h, 2);
            g.dg_238e = 0;
        }
    } else {
        // [BYTES seg8:0xc12 seg8:0xc17 seg8:0xc25 seg8:0xc3b seg8:0xc45]
        o.old_scr_x = o.scr_x;
        o.old_scr_y = o.scr_y;
        const Point16 p = to_screen(o.x, o.y);
        o.scr_x = p.x;
        o.scr_y = p.y;
    }
    if (kind == 4) return 1;                      // [BYTES seg8:0xc85]
    if (kind == 5 && o.on_screen == 0) return 1;  // [BYTES seg8:0xc8d seg8:0xca1]

    // 3. Source: +0x3a keeps the DC passed in; +0x2e != 0 takes the alternate source, and
    // +0x2e >= 0x80 (signed byte < 0) draws nothing for every kind but 7 [BYTES seg8:0xcaa
    // seg8:0xcbb seg8:0xcc5 seg8:0xccc seg8:0xcf1 seg8:0xcf6 seg8:0xcf8].
    o.w3a = static_cast<std::int16_t>(src_dc);
    if (o.b2e != 0) {
        take_alternate(o, src_dc);
        if (o.b2e >= 0x80) draw = false;
    }
    if (special_mode(g) && ((pool == g.ship_pool && rec == g.dg_952e) || pool == 7))
        take_alternate(o, src_dc);  // [BYTES seg8:0xcfd-0xd51]

    const std::uint32_t rop = masked != 0 ? kMergeCopy : kSrcCopy;  // [BYTES seg8:0xd55 seg8:0xd5b seg8:0xd65]

    // Kind 6: SRCPAINT over the screen, no erase [BYTES seg8:0xd6d seg8:0xd73 seg8:0xda7].
    if (kind == 6 && draw) {
        out.bit_blt(g.window_dc, o.scr_x, o.scr_y, o.size, o.size, src_dc, o.src_x, o.src_y,
                    kSrcPaint);
        return 1;
    }
    // Kind 7: a 3 x 3 WHITENESS dot [BYTES seg8:0xdb5 seg8:0xdd6 seg8:0xddf].
    if (kind == 7) {
        out.pat_blt(g.window_dc, o.scr_x, o.scr_y, 3, 3, kWhiteness);
        return 1;
    }
    if (!draw) return 1;  // [BYTES seg8:0xde7]

    // 4. Erase, 5. blit [BYTES seg8:0xdfd-0x1015 seg8:0x1015-0x10c6 seg8:0xdad].
    if (!erase_and_merge(out, g.window_dc, src_dc, o.scr_x, o.scr_y, o.old_scr_x, o.old_scr_y,
                         o.size, o.gap, o.src_x, o.src_y, rop))
        out.bit_blt(g.window_dc, o.scr_x, o.scr_y, o.size, o.size, src_dc, o.src_x, o.src_y,
                    rop);
    return 1;
}

// ---- RecordingBlitter

void RecordingBlitter::bit_blt(DcId dst, int x, int y, int w, int h, DcId src, int sx, int sy,
                               std::uint32_t rop) {
    calls.push_back(bit_blt_call(dst, x, y, w, h, src, sx, sy, rop));
}

void RecordingBlitter::pat_blt(DcId dst, int x, int y, int w, int h, std::uint32_t rop) {
    calls.push_back(pat_blt_call(dst, x, y, w, h, rop));
}

void RecordingBlitter::stretch_blt(DcId dst, int x, int y, int w, int h, DcId src, int sx,
                                   int sy, int sw, int sh, std::uint32_t rop) {
    BlitCall c = bit_blt_call(dst, x, y, w, h, src, sx, sy, rop);
    c.op = BlitCall::kStretchBlt, c.sw = sw, c.sh = sh;
    calls.push_back(c);
}

void RecordingBlitter::set_stretch_mode(DcId dc, int mode) {
    BlitCall c;
    c.op = BlitCall::kStretchMode, c.dst = dc, c.rop = static_cast<std::uint32_t>(mode);
    calls.push_back(c);
}

BrushId RecordingBlitter::select_brush(DcId dc, BrushId brush) {
    BlitCall c;
    c.op = BlitCall::kSelectBrush, c.dst = dc, c.brush = brush;
    calls.push_back(c);
    for (auto& s : selected_) {
        if (s.first == dc) {
            const BrushId old = s.second;
            s.second = brush;
            return old;
        }
    }
    selected_.emplace_back(dc, brush);
    return kBrushWhite;  // a new DC's brush is WHITE_BRUSH [REF Win16 SDK CreateCompatibleDC]
}

void RecordingBlitter::set_pixel(DcId dc, int x, int y, std::uint32_t colorref) {
    BlitCall c;
    c.op = BlitCall::kSetPixel, c.dst = dc, c.x = x, c.y = y, c.rop = colorref;
    calls.push_back(c);
}

void RecordingBlitter::frame_rect(DcId dc, const Rect16& r, BrushId brush) {
    BlitCall c;
    c.op = BlitCall::kFrameRect, c.dst = dc, c.x = r.l, c.y = r.t, c.w = r.r, c.h = r.b,
    c.brush = brush;
    calls.push_back(c);
}

DcId RecordingBlitter::create_image(int w, int h) {
    images.push_back(Image{next_dc_, w, h, {}, 0});
    return next_dc_++;
}

DcId RecordingBlitter::create_image_from(int w, int h, const std::vector<std::uint8_t>& index) {
    images.push_back(Image{next_dc_, w, h, {}, 0, index});
    return next_dc_++;
}

void RecordingBlitter::delete_image(DcId id) { deleted.push_back(id); }

DcId RecordingBlitter::create_mask(DcId src) {
    images.push_back(Image{next_dc_, 0, 0, "mask", src});
    return next_dc_++;
}

DcId RecordingBlitter::load_bitmap(const std::string& name) {
    images.push_back(Image{next_dc_, 0, 0, name, 0});
    return next_dc_++;
}

DcId RecordingBlitter::load_graphic(int id, int /*mode*/, int /*arg*/) {
    images.push_back(Image{next_dc_, 0, 0, {}, id});
    return next_dc_++;
}

BrushId RecordingBlitter::create_solid_brush(std::uint32_t colorref) {
    brushes.push_back(Brush{next_brush_, colorref, {}});
    return next_brush_++;
}

BrushId RecordingBlitter::create_pattern_brush(const std::uint8_t (&rows)[8]) {
    Brush b{next_brush_, 0, {}};
    for (int i = 0; i < 8; ++i) b.rows[static_cast<std::size_t>(i)] = rows[i];
    brushes.push_back(b);
    return next_brush_++;
}

// ---- the window's Blitter

namespace {
Blitter* g_wave_blitter = nullptr;
}  // namespace

Blitter* wave_blitter() { return g_wave_blitter; }
void set_wave_blitter(Blitter* b) { g_wave_blitter = b; }

}  // namespace is::world
