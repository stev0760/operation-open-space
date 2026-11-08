// The effect table's routines (effects.h). Each step cites the original's addresses; the
// arithmetic is 16-bit where the original's is (ADR 0013 decision 7).
#include "world/effects.h"

#include "world/missile.h"

#include "world/collide.h"
#include "world/death.h"
#include "world/shots.h"

namespace is::world {

namespace {

constexpr std::size_t ix(int k) { return static_cast<std::size_t>(k); }

Point16 screen_of(const EffectCtx& c, std::int16_t x, std::int16_t y) {
    return c.to_screen ? c.to_screen(x, y) : Point16{};
}

bool record_ok(const Pools& p, int pool, int rec) {
    return pool >= 0 && pool < kPoolCount && rec >= 0 && rec < p.records(pool);
}

DcId slot_dc(const EffectCtx& c, int slot) {
    return c.images && slot >= 0 && slot < kImageSlots ? (*c.images)[ix(slot)] : kDcNone;
}

// seg8:0x1290(N, g) in declaration order (Pascal, `retf 8`): g = 3 when g < 3, then the low
// word of (N / g + g) sar 1, 32-bit (core.md "The intercept", C005) [BYTES seg8:0x129d
// seg8:0x12a4 seg8:0x12b2 seg8:0x12b6 seg8:0x12ba] (M4 s4 fixer: the parameters in the
// original's order).
std::int16_t mass_scale(std::int32_t n, std::int32_t g) {
    const std::int32_t m = g > 3 ? g : 3;
    return lo16(sar32(n / m + m, 1));
}

}  // namespace

EffectState& effect_state() {
    static EffectState s;
    return s;
}

void release_piece_sources(EffectState& st) {
    for (int e = 0; e < kPieceEntries; ++e) {  // [BYTES seg10:0x2cb seg10:0x2fd-0x301]
        if (st.piece_src[ix(e)] == kDcNone) continue;  // [BYTES seg10:0x2d5-0x2da]
        if (st.piece_src_owned[ix(e)] && effect_links().release) effect_links().release(st.piece_src[ix(e)]);  // [BYTES seg10:0x2e6 seg10:0x2ef]
        st.piece_src[ix(e)] = kDcNone;  // [BYTES seg10:0x2f4]
        st.piece_src_owned[ix(e)] = false;
    }
}

EffectLinks& effect_links() {
    static EffectLinks l;
    return l;
}

// [BYTES seg10:0x4c0b-0x4d16] (verified claim 31)
const std::array<std::uint32_t, kEffectBrushes> kEffectBrushColours = {
    0x808080u, 0xa0a0a0u, 0xc0c0c0u, 0xe0e0e0u, 0xffffffu, 0xc0c0c0u, 0xa0dfa0u,
    0x60e060u, 0x40ff40u, 0x00ff00u, 0x800000u, 0x8a2800u, 0xa05000u, 0xc4a000u,
    0xffc400u, 0x0000a0u, 0x0000c4u, 0x0020dfu, 0x0060ffu, 0x0080ffu};

// seg10:0x5102 [BYTES seg10:0x5110-0x51f6]
void build_effect_images(Blitter& out, ImageSlotTable& images, std::int16_t enhance) {
    if (images[ix(kSlotExplosion)] != kDcNone && images[ix(kSlotSnowball)] != kDcNone) return;
    // i = 0: slot 10, #170, mode 2 with `Enhance` else 0, arg 1 [BYTES seg10:0x51de-0x51f6
    // seg10:0x5128 seg10:0x512d]; i = 1: slot 11, #175 (0, -1) [BYTES seg10:0x513a-0x5149].
    if (images[ix(kSlotExplosion)] == kDcNone)  // [BYTES seg10:0x5163]
        images[ix(kSlotExplosion)] = out.load_graphic(kGraphicExplosion, enhance != 0 ? 2 : 0, 1);
    if (images[ix(kSlotSnowball)] == kDcNone)
        images[ix(kSlotSnowball)] = out.load_graphic(kGraphicSnowball, 0, -1);
}

bool in_play_area(std::int16_t x, std::int16_t y, std::int16_t W, std::int16_t H) {
    return pt_in_rect(rect16(-50, -50, W + 50, H + 50), x, y);  // [BYTES seg8:0x7ba8]
}

int find_free_effect(const EffectTable& t) {
    for (int j = 0; j < kEffectCount; ++j)
        if (t[ix(j)].frame == kEffectFree) return j;  // cmp -1 [BYTES seg16:0x9e2 seg12:0x2a5f]
    return -1;
}

// seg8:0x3fbe [BYTES seg8:0x3fbe-0x45db]
void effect_frame(EffectCtx& c, int k) {
    if (k < 0 || k >= kEffectCount) return;  // [DESIGN] bounds
    World& w = c.w;
    EffectEntry& e = w.effects[ix(k)];
    const std::int16_t size = w.size;
    bool frame_step = true;
    // A free or overrun entry: straight to the end test [BYTES seg8:0x3fd1-0x3fe2].
    if (static_cast<std::int16_t>(e.frame) < 0 || e.end < static_cast<std::int16_t>(e.frame)) {
        frame_step = false;
    } else {
        // 1. The wait of types 1 and < 0 [BYTES seg8:0x3fe5-0x403c].
        if (e.type == 1 || e.type < 0) {
            if (e.start.get() > c.in.now) return;  // no frame [BYTES seg8:0x3ffe-0x400c]
            if (e.frame == 1) {
                c.st.dg_5144 = c.st.sides.point[ix(k)].x;  // DGROUP:0x5224 -> 0x5144
                c.st.dg_5146 = c.st.sides.point[ix(k)].y;
                c.host.sound(0x45, 0, 0, 3);  // seg10:0x754a(0x45, 0, 0, 3) [BYTES seg8:0x4037]
            }
        }
        if (e.type > 5) {
            // seg8:0x4256-0x452a [BYTES seg8:0x4043 seg8:0x404a]; entries >= 4 never hold such a
            // type (s5-effects.md 1.2): the port skips the branch there [DESIGN].
            if (k < kPieceEntries) piece_frame(c, k);
        } else {
            // 2. The explosion bitmap [BYTES seg8:0x404d-0x4067].
            const std::int16_t cnt = static_cast<std::int16_t>(e.frame);
            const std::int16_t f = sub16(cnt, 1);
            const std::int16_t side = sar16(shl16(add16(mul16(f, 6), 10), size & 0x1f), 1);
            const EffectPoint cen = c.st.sides.centre[ix(k)];
            if (sub16(e.end, 1) > cnt) {
                // Not the last frame [BYTES seg8:0x41b8-0x422b].
                if (e.type > -1 && in_play_area(cen.x, cen.y, w.W, w.H) && c.out) {
                    const std::int16_t half = sar16(side, 1);
                    // min(f, 7) as `cmp bx, 7; jle`; f < 0 cannot come from a creator (they
                    // write 1): the port draws nothing then [DESIGN].
                    if (f >= 0) {
                        const std::int16_t sx =
                            sar16(shl16(kExplosionX[ix(f > 7 ? 7 : f)], size & 0x1f), 1);
                        c.out->bit_blt(kDcWindow, sub16(cen.x, half), sub16(cen.y, half), side, side,
                                       slot_dc(c, kSlotExplosion), sx, 0, kSrcCopy);
                    }
                }
                // The blast at +0x00 % 6 = 0, type != 1 [BYTES seg8:0x4230-0x4250].
                if (imod16(cnt, 6) == 0 && e.type != 1) blast(c, k);
            } else if (e.type != kEffectTypeFF06) {
                // The last frame [BYTES seg8:0x407b-0x4187].
                const std::int16_t q = sar16(shl16(0x1a, size & 0x1f), 1);
                const std::int16_t s52 = sar16(shl16(0x34, size & 0x1f), 1);
                if (c.out) c.out->pat_blt(kDcWindow, sub16(cen.x, q), sub16(cen.y, q), s52, s52, kBlackness);
                // seg7:0x1208 when the player is in play, DGROUP:0x951a = 0 and its byte
                // DGROUP:0x717c (+0xec) != 0 (verified claim 11, extra) [BYTES seg8:0x40b9-0x40e4].
                const int ps = w.roles.ships;
                if (record_ok(w.pools, ps, w.player) && w.player >= 0 && w.player < kShipCount &&
                    w.pools.rec(ps, w.player).state == 0 && c.in.autopilot == 0 &&
                    w.ships[ix(w.player)].bec != 0)
                    c.host.player_sprite();
                // 0x6a for a ship source in play, type != 1 [BYTES seg8:0x40e9-0x4125].
                if (e.pool == w.roles.ships && record_ok(w.pools, e.pool, e.rec) &&
                    w.pools.rec(e.pool, e.rec).state == 0 && e.type != 1)
                    c.host.sound(0x6a, e.pool, e.rec, 1);
                // The source's square, type != 1, any pool [BYTES seg8:0x4128-0x4187].
                if (e.type != 1 && record_ok(w.pools, e.pool, e.rec)) {
                    const ObjectRecord& src = w.pools.rec(e.pool, e.rec);
                    const Point16 p = screen_of(c, src.x, src.y);
                    if (c.out) c.out->pat_blt(kDcWindow, p.x, p.y, src.size, src.size, kBlackness);
                }
            }
        }
    }
    if (frame_step) e.frame = static_cast<std::uint16_t>(e.frame + 1);  // [BYTES seg8:0x4190]
    // The end test [BYTES seg8:0x4194-0x41b5].
    if (!(e.end <= static_cast<std::int16_t>(e.frame))) return;
    e.frame = kEffectFree;  // [BYTES seg8:0x41aa]
    // The rescan from 44 down [BYTES seg8:0x453b-0x4560].
    bool found = false;
    for (int j = kEffectCount - 1; j >= 0; --j) {
        if (w.effects[ix(j)].frame != kEffectFree) {
            w.w9434 = static_cast<std::int16_t>(j);
            found = true;
            break;
        }
    }
    if (!found) w.w9434 = 0;
    // DGROUP:0x9430 - 1, floor 0 (`dec; jns`) [BYTES seg8:0x4566-0x456e].
    std::int16_t n = sub16(w.w9430, 1);
    if (n < 0) n = 0;
    w.w9430 = n;
    if (w.w9130 != 0)  // [BYTES seg8:0x4571-0x4586]
        c.host.seg18_5350(c.st.sides.centre[ix(k)].x, c.st.sides.centre[ix(k)].y);
    // The type-6 source [BYTES seg8:0x458e-0x45cd] (verified claim 15): SelectObject(5e98[k],
    // 5ea0[k]) and DeleteDC for pool 1, then DGROUP:0x5e98[k] = 0 for any pool.
    if (k < kPieceEntries && e.type == 6 && c.st.piece_src[ix(k)] != kDcNone) {
        if (e.pool == 1 && c.st.piece_src_owned[ix(k)] && c.out) c.out->delete_image(c.st.piece_src[ix(k)]);
        c.st.piece_src[ix(k)] = kDcNone;
        c.st.piece_src_owned[ix(k)] = false;
    }
}

// ---- the piece branch (M4 s5 EFFECTS)

int piece_index(int e, int i, int j) {
    if (e < 0 || e >= kPieceEntries || i < 0 || i >= kPieceGrid || j < 0 || j >= kPieceGrid) return -1;
    return (e * 5 + i) * 5 + j;  // `imul e, 5; add i; imul 5; add j` [BYTES seg8:0x430e-0x431b]
}

// [BYTES seg7:0x3460-0x3671] (the same bodies at seg8:0x96e1-0x98f2)
PieceAccel piece_accel(int i0, int j0, int c) {
    const std::int16_t i = static_cast<std::int16_t>(i0), j = static_cast<std::int16_t>(j0);
    const std::int16_t im2 = sub16(i, 2), jm2 = sub16(j, 2);
    const std::int16_t twoi = sub16(2, i), twoj = sub16(2, j);
    PieceAccel a;
    switch (c) {
    case 0:  // [BYTES seg7:0x3460-0x348f]
        a.ax = sub16(shl16(add16(i, 5), 1), mul16(j, 7));
        a.ay = add16(shl16(sub16(-1, j), 2), mul16(i, 6));
        break;
    case 1:  // [BYTES seg7:0x3492-0x34d2]
        a.ax = add16(mul16(twoj, 7), shl16(mul16(twoi, im2), 1));
        a.ay = add16(shl16(mul16(twoj, jm2), 2), mul16(im2, 6));
        break;
    case 2:  // [BYTES seg7:0x34d5-0x3515]
        a.ax = add16(mul16(twoj, 7), shl16(mul16(twoi, im2), 1));
        a.ay = add16(shl16(mul16(jm2, jm2), 2), mul16(twoi, 6));
        break;
    case 3:  // [BYTES seg7:0x3518-0x3564]
        a.ax = add16(shl16(im2, 1), mul16(mul16(twoj, sub16(5, i)), 7));
        a.ay = add16(mul16(mul16(im2, sub16(j, 5)), 6), shl16(jm2, 2));
        break;
    case 4:  // [BYTES seg7:0x3566-0x35a1 seg7:0x3636-0x3638]
        a.ax = mul16(twoj, im2);
        a.ay = add16(mul16(mul16(jm2, im2), 7), shl16(mul16(twoj, twoi), 1));
        break;
    case 5:  // [BYTES seg7:0x35a4-0x35e2]
        a.ax = mul16(sub16(add16(mul16(mul16(twoi, twoi), im2), j), 2), 6);
        a.ay = add16(shl16(mul16(twoj, jm2), 1), mul16(im2, 5));
        break;
    case 6:  // [BYTES seg7:0x35e7-0x3638]
        a.ax = mul16(add16(mul16(mul16(im2, im2), twoi), mul16(mul16(jm2, jm2), jm2)), 6);
        a.ay = add16(shl16(mul16(twoj, jm2), 1), mul16(mul16(mul16(im2, im2), i), 7));
        break;
    case 7:  // [BYTES seg7:0x363e-0x366d]
        a.ax = sub16(sub16(mul16(j, 7), mul16(i, 3)), 8);
        a.ay = mul16(mul16(mul16(im2, im2), im2), 6);
        break;
    default: break;  // `ja` past 7: no write [BYTES seg7:0x3673-0x3676]
    }
    return a;
}

// seg7:0x342e [BYTES seg7:0x343d-0x3759]
void pieces_setup_break(EffectCtx& c, int e, int pool, int rec) {
    if (e < 0 || e >= kPieceEntries) return;  // [DESIGN]
    const bool ok = record_ok(c.w.pools, pool, rec);  // [DESIGN] bounds: a missing record reads 0
    const ObjectRecord zero{};
    const ObjectRecord& o = ok ? c.w.pools.rec(pool, rec) : zero;
    const std::int16_t s = idiv16(o.size, 5);  // [BYTES seg7:0x344b-0x3455]
    for (int i = 0; i < kPieceGrid; ++i) {      // [bp-4] [BYTES seg7:0x3740-0x3747]
        for (int j = 0; j < kPieceGrid; ++j) {  // [bp-6] [BYTES seg7:0x3690-0x3697]
            EffectPiece& p = c.st.pieces[ix(piece_index(e, i, j))];
            const std::int16_t i16 = static_cast<std::int16_t>(i), j16 = static_cast<std::int16_t>(j);
            p.wx = add16(shl16(mul16(s, j16), 3), o.x);  // [BYTES seg7:0x369b-0x36c8]
            p.wy = add16(shl16(mul16(s, i16), 3), o.y);  // [BYTES seg7:0x36cc-0x36d9]
            p.vx = add16(idiv16(o.vx, 10), shl16(sub16(j16, 2), 3));  // [BYTES seg7:0x36dd-0x36f1]
            p.vy = add16(idiv16(o.vy, 10), mul16(sub16(2, i16), 7));  // [BYTES seg7:0x36f5-0x3707]
            const PieceAccel a0 = piece_accel(i, j, 0);  // [BYTES seg7:0x370b-0x372c]
            p.ax = a0.ax;
            p.ay = a0.ay;
            const int r = imod16(static_cast<std::int16_t>(c.rng.rand()), 8);  // seg5:0xdc [BYTES seg7:0x3730-0x373b]
            if (r >= 0 && r <= 7) {  // `cmp 7; ja` [BYTES seg7:0x3673]
                const PieceAccel a = piece_accel(i, j, r);
                p.ax = a.ax;
                p.ay = a.ay;
            }
        }
    }
}

// seg8:0x9694 [BYTES seg8:0x96a3-0x99cc]
void pieces_setup_burst(EffectCtx& c, int e, int pool, int rec) {
    if (e < 0 || e >= kPieceEntries) return;  // [DESIGN]
    std::int16_t vx = 0, vy = 0;  // [bp-0xa], [bp-8]
    if (pool == kPieceShotPool) {  // [BYTES seg8:0x96a3-0x96b9]
        const ShotState& sh = c.shots ? *c.shots : shot_state();
        vx = sh.word(rec, shot::kVx);  // DGROUP:0x6108 + 0x94·rec
        vy = sh.word(rec, shot::kVy);  // DGROUP:0x610a
    } else if (record_ok(c.w.pools, pool, rec)) {  // [BYTES seg8:0x96bb-0x96d6]
        vx = c.w.pools.rec(pool, rec).vx;
        vy = c.w.pools.rec(pool, rec).vy;
    }
    const EffectPoint pt = c.st.sides.point[ix(e)];  // DGROUP:0x5224 + 4e
    for (int i = 0; i < kPieceGrid; ++i) {
        for (int j = 0; j < kPieceGrid; ++j) {
            EffectPiece& p = c.st.pieces[ix(piece_index(e, i, j))];
            const std::int16_t i16 = static_cast<std::int16_t>(i), j16 = static_cast<std::int16_t>(j);
            p.wx = add16(add16(shl16(j16, 4), pt.x), 8);  // [BYTES seg8:0x991c-0x993f]
            p.wy = add16(add16(shl16(i16, 4), pt.y), 8);  // [BYTES seg8:0x9943-0x9950]
            p.vx = add16(shl16(sub16(j16, 2), 3), vx);    // [BYTES seg8:0x9954-0x995f]
            p.vy = add16(mul16(sub16(2, i16), 7), vy);    // [BYTES seg8:0x9963-0x996f]
            const PieceAccel a0 = piece_accel(i, j, 0);   // [BYTES seg8:0x9973-0x9994]
            p.ax = a0.ax;
            p.ay = a0.ay;
            // (rand() % 32) % 8; the % 32 value goes to [bp+8], never read again [BYTES
            // seg8:0x9998-0x99ae].
            const std::int16_t r32 = imod16(static_cast<std::int16_t>(c.rng.rand()), 0x20);
            const int r = imod16(r32, 8);
            if (r >= 0 && r <= 7) {  // [BYTES seg8:0x98f4]
                const PieceAccel a = piece_accel(i, j, r);
                p.ax = a.ax;
                p.ay = a.ay;
            }
        }
    }
}

// seg8:0x7d2 [BYTES seg8:0x7f0-0x82f]
void piece_screen(EffectCtx& c, int e, int i, int j) {
    const int n = piece_index(e, i, j);
    if (n < 0) return;  // [DESIGN]
    EffectPiece& p = c.st.pieces[ix(n)];
    const Point16 pt = screen_of(c, p.wx, p.wy);  // seg8:0x29ea(dword +0x18) [BYTES seg8:0x7fc]
    p.sx = pt.x;          // [BYTES seg8:0x808 seg8:0x80f]
    p.sy = pt.y;
    p.old_x = p.prev_x;   // [BYTES seg8:0x813-0x81f]
    p.old_y = p.prev_y;
    p.prev_x = p.sx;      // [BYTES seg8:0x823-0x82f]
    p.prev_y = p.sy;
}

namespace {

// The motion of one coordinate [BYTES seg8:0x439f-0x4406 seg8:0x440a-0x4471]: cx = lo16(end·end);
// lo16(lo16(lo16(a·f)·f)·9), `cwd; idiv cx`; + v added to w; w > limit (signed `jle`) -> 0, then
// w < 0 (`jge`) -> limit.
std::int16_t piece_move(std::int16_t w, std::int16_t a, std::int16_t v, std::int16_t f,
                        std::int16_t end, std::int16_t limit) {
    const std::int16_t cx = mul16(end, end);
    const std::int16_t t = mul16(mul16(mul16(a, f), f), 9);
    const std::int16_t d = cx != 0 ? idiv16(t, cx) : std::int16_t{0};  // [DESIGN] the divide trap
    w = add16(w, add16(d, v));
    if (w > limit) w = 0;
    if (w < 0) w = limit;
    return w;
}

}  // namespace

// seg8:0x4256-0x4538
void piece_frame(EffectCtx& c, int k) {
    if (k < 0 || k >= kPieceEntries) return;  // [DESIGN]
    World& w = c.w;
    EffectEntry& e = w.effects[ix(k)];
    const std::int16_t f = static_cast<std::int16_t>(e.frame);
    if (f == 1) {  // [BYTES seg8:0x425a-0x425f]
        if (e.type == 6)
            pieces_setup_break(c, k, e.pool, e.rec);  // [BYTES seg8:0x4261-0x4278]
        else
            pieces_setup_burst(c, k, e.pool, e.rec);  // [BYTES seg8:0x427a-0x4289]
        // The source's square for a pool not above DGROUP:0x9324 (signed `jg`): never the gun's 0x1e
        // [BYTES seg8:0x4291-0x42c7].
        if (!(e.pool > w.roles.ships) && record_ok(w.pools, e.pool, e.rec) && c.out) {
            const ObjectRecord& o = w.pools.rec(e.pool, e.rec);
            c.out->pat_blt(kDcWindow, o.scr_x, o.scr_y, o.size, o.size, kBlackness);
        }
    }
    if (f == 5 && e.type == 6) blast(c, k);  // seg8:0x5ab0 [BYTES seg8:0x42cc-0x42e2]
    const std::int16_t limit_x = shl16(static_cast<std::int16_t>(kWorldCells), 3);  // DGROUP:0x5a0 << 3 [BYTES seg8:0x43d7-0x43da]
    const std::int16_t limit_y = shl16(static_cast<std::int16_t>(kWorldCells), 3);  // DGROUP:0x5a2 << 3 [BYTES seg8:0x4442-0x4445]
    std::int16_t gap = 0;  // [bp-0x16], read only for type 6
    for (std::int16_t i = 0; i < kPieceGrid;) {  // [BYTES seg8:0x42e5 seg8:0x452a]
        for (std::int16_t j = 0; j < kPieceGrid;) {  // [BYTES seg8:0x4533 seg8:0x4480]
            piece_screen(c, k, i, j);  // [BYTES seg8:0x4492]
            EffectPiece& p = c.st.pieces[ix(piece_index(k, i, j))];
            const std::int16_t side = e.side;  // +0x0c
            if (e.type == 6) {
                // The object's +0x24 read again on every piece [BYTES seg8:0x449b-0x44be].
                gap = record_ok(w.pools, e.pool, e.rec) ? w.pools.rec(e.pool, e.rec).gap : std::int16_t{0};
                if (c.out)  // [BYTES seg8:0x44cf-0x4517]: source x takes i, y takes j
                    c.out->bit_blt(kDcWindow, p.sx, p.sy, side, side, c.st.piece_src[ix(k)],
                                   add16(mul16(side, i), gap), add16(mul16(side, j), gap), kSrcCopy);
            } else if (c.out) {
                // SelectObject(DGROUP:0x13dc, DGROUP:0x576c[+0x0e + DGROUP:0xb6a]), not restored;
                // an index outside the table selects nothing [DESIGN] [BYTES seg8:0x42ed-0x4305].
                const int b = add16(e.brush, c.st.dg_b6a);
                if (b >= 0 && b < kEffectBrushes) c.out->select_brush(kDcWindow, c.st.brushes[ix(b)]);
                c.out->pat_blt(kDcWindow, p.sx, p.sy, side, side, kPatCopy);  // [BYTES seg8:0x430a-0x4333]
            }
            // The old point after the draw [BYTES seg8:0x4338-0x4366].
            if (c.out) c.out->pat_blt(kDcWindow, p.old_x, p.old_y, side, side, kBlackness);
            if (sub16(sub16(e.end, f), 1) == 0) {  // the last frame [BYTES seg8:0x436b-0x4374]
                if (c.out) c.out->pat_blt(kDcWindow, p.sx, p.sy, side, side, kBlackness);  // [BYTES seg8:0x4376-0x438e]
                if (c.in.autopilot == 0) c.host.player_sprite();  // seg7:0x1208 [BYTES seg8:0x4393-0x439a]
            }
            p.wx = piece_move(p.wx, p.ax, p.vx, f, e.end, limit_x);  // [BYTES seg8:0x439f-0x4406]
            p.wy = piece_move(p.wy, p.ay, p.vy, f, e.end, limit_y);  // [BYTES seg8:0x440a-0x4471]
            if (e.step <= 0) return;  // [DESIGN] no creator writes one
            j = add16(j, e.step);  // re-read [BYTES seg8:0x4479-0x447d]
        }
        if (e.step <= 0) return;  // [DESIGN]
        i = add16(i, e.step);  // [BYTES seg8:0x4523-0x4527]
    }
}

// seg8:0x82c6-0x82f4
void effect_colour_cycle(EffectState& st, std::uint32_t now) {
    if (!(now - 0x3cu > st.dg_579c)) return;  // `sub eax, 0x3c; cmp; jbe` [BYTES seg8:0x82c6-0x82d5]
    st.dg_579c = now;                         // [BYTES seg8:0x82db]
    st.dg_b6a = add16(st.dg_b6a, st.dg_b6c);  // [BYTES seg8:0x82e2]
    if (st.dg_b6a > 3 || st.dg_b6a < 1) st.dg_b6c = sub16(0, st.dg_b6c);  // [BYTES seg8:0x82e6-0x82f4]
}

// seg10:0x4c0b-0x4d16
void create_effect_brushes(Blitter& out, EffectState& st) {
    if (st.brushes_made) return;  // [DESIGN] once per program Blitter
    st.brushes_made = true;
    for (int n = 0; n < kEffectBrushes; ++n) st.brushes[ix(n)] = out.create_solid_brush(kEffectBrushColours[ix(n)]);
}

// seg8:0x5ab0 [BYTES seg8:0x5ab0-0x5de6] (core.md "Death and the explosion", The blast,
// C001-C007).
void blast(EffectCtx& c, int k) {
    if (k < 0 || k >= kEffectCount) return;
    World& w = c.w;
    const EffectEntry& e = w.effects[ix(k)];
    const std::int16_t sp = e.pool, sr = e.rec, type = e.type;  // [bp-6], [bp-8], [bp-4]
    const EffectPoint pt = c.st.sides.point[ix(k)];               // [bp-0x24], [bp-0x22]
    if (c.in.dg_1318 > 6) return;  // signed [BYTES seg8:0x5aec seg8:0x5af1 seg8:0x5af3] (C001)
    if (!record_ok(w.pools, sp, sr)) return;  // [DESIGN] bounds
    Pools& pools = w.pools;
    for (int p = w.roles.radar_first; p <= w.roles.ships && p < kPoolCount; ++p) {  // [BYTES seg8:0x5af6 seg8:0x5dd6]
        for (int r = 0; r < pools.in_use(p) && r < pools.records(p); ++r) {  // [BYTES seg8:0x5c9f-0x5cab]
            {
                const ObjectRecord& o = pools.rec(p, r);
                if (o.state != 0 || o.mass == 0) continue;  // [BYTES seg8:0x5cbf seg8:0x5cc6]
                if (p == w.roles.ships && r == w.w952e && o.on_screen == 0) continue;  // [BYTES seg8:0x5ccd-0x5ce1]
            }
            // The source only for type 0xff06 [BYTES seg8:0x5ce3-0x5cf8].
            if (type == kEffectTypeFF06 && p == sp && r == sr) continue;
            std::int16_t dx = 0, dy = 0;
            std::int16_t d;
            {
                const ObjectRecord& o = pools.rec(p, r);
                d = distance_cells(o.x, o.y, pt.x, pt.y, dx, dy);  // seg8:0x285a [BYTES seg8:0x5d20]
            }
            // (2v) sar Size for d, dx, dy [BYTES seg8:0x5d28 seg8:0x5d2c seg8:0x5d2e seg8:0x5d38
            // seg8:0x5d42] (C003); the seg18 doubling (DGROUP:0x9130 != 0, DGROUP:0x90f4 = 6)
            // [BYTES seg8:0x5d47 seg8:0x5d55] is M6's.
            d = sar16(shl16(d, 1), w.size & 0x1f);
            dx = sar16(shl16(dx, 1), w.size & 0x1f);
            dy = sar16(shl16(dy, 1), w.size & 0x1f);
            if (d < 100) d = 100;  // [BYTES seg8:0x5d5e-0x5d64]
            const std::int16_t b = pools.rec(sp, sr).blast_size;  // +0x48 [BYTES seg8:0x5d77]
            const std::int16_t bc = b > 7 ? std::int16_t{7} : b;
            if (!(mul16(add16(bc, 8), 15) > d)) continue;  // [BYTES seg8:0x5d85-0x5d90]
            std::int16_t amount = idiv16(mul16(b, 600), d);  // [BYTES seg8:0x5d9c-0x5da4]
            std::int16_t m = mass_scale(pools.rec(p, r).mass, 25);  // seg8:0x1290(mass, 25) [BYTES seg8:0x5dc0] (C005)
            if (m < 1) m = 1;                                       // [BYTES seg8:0x5dc5-0x5dcd]
            amount = idiv16(mul16(amount, 15), m);                  // [BYTES seg8:0x5b22-0x5b29]
            if (p == 2) amount = mul16(amount, 3);                  // [BYTES seg8:0x5b2c-0x5b38]
            {
                // Heat +2, +2 more when d < 150, at most 9 (signed byte) [BYTES seg8:0x5b49-0x5b77].
                ObjectRecord& o = pools.rec(p, r);
                o.flame = static_cast<std::uint8_t>(o.flame + 2);
                if (d < 150) o.flame = static_cast<std::uint8_t>(o.flame + 2);
                if (static_cast<std::int8_t>(o.flame) > 9) o.flame = 9;
            }
            if (c.dg_943a) *c.dg_943a = sp;  // [BYTES seg8:0x5b7c-0x5b85]
            if (c.dg_943c) *c.dg_943c = sr;
            // DGROUP:0xb0c, the dispatcher's word (M4 s4 fixer, C006, C084) [BYTES seg8:0x5b88
            // seg8:0x5b92 seg8:0x5b99].
            if (c.dg_b0c) *c.dg_b0c = 1;
            if (c.dg_b0c && e.frame != 0) *c.dg_b0c = 0;
            c.host.hit(p, r, sp, sr, sub16(0, amount));  // seg8:0x5df2 [BYTES seg8:0x5bb2]
            if (c.dg_b0c) *c.dg_b0c = 1;                 // [BYTES seg8:0x5bb8]
            // The push, with the record read again after the dispatcher [BYTES seg8:0x5bbe-0x5c98].
            if (!record_ok(pools, p, r) || !record_ok(pools, sp, sr)) continue;  // [DESIGN]
            ObjectRecord& o = pools.rec(p, r);
            const std::int16_t mm = idiv16(add16(o.mass, 100), 100);
            const std::int16_t d10 = idiv16(d, 10);
            if (mm == 0 || d10 == 0) continue;  // the original traps [DESIGN]
            const std::int16_t v = idiv16(idiv16(mul16(pools.rec(sp, sr).blast_size, 250), d10), mm);
            // seg5:0x314 (32-bit multiply) and seg5:0x110 (32-bit divide by d).
            const std::int32_t px = div32(static_cast<std::int32_t>(static_cast<std::uint32_t>(v) *
                                                                    static_cast<std::uint32_t>(dx)),
                                          d);
            const std::int32_t py = div32(static_cast<std::int32_t>(static_cast<std::uint32_t>(v) *
                                                                    static_cast<std::uint32_t>(dy)),
                                          d);
            o.vx = add16(o.vx, lo16(px));  // [BYTES seg8:0x5c87]
            o.vy = add16(o.vy, lo16(py));  // [BYTES seg8:0x5c98]
        }
    }
}

// seg16:0x8e4 [BYTES seg16:0x8e4-0xb4c]
void ship_destroyed(EffectCtx& c, int pool, int ship) {
    (void)pool;  // the routine reads only the ship [BYTES seg16:0x8f3]
    World& w = c.w;
    if (ship < 0 || ship >= kShipCount || !record_ok(w.pools, w.roles.ships, ship)) return;  // [DESIGN]
    ShipRecord& s = w.ships[ix(ship)];
    if (s.radar_hide == 1 && w.w9130 == 0) return;  // byte +0x147 [BYTES seg16:0x8f8-0x904]
    const std::uint32_t now = c.in.now;
    for (int k = 0; k < 8; ++k) {  // [BYTES seg16:0x908-0x9f7]
        const int j = find_free_effect(w.effects);
        if (j < 0) continue;  // no free slot: entry k is skipped [BYTES seg16:0x9d8-0x9dc]
        EffectEntry& e = w.effects[ix(j)];
        e.type = k == 0 ? 5 : 1;  // [BYTES seg16:0x9f1 seg16:0x914]
        e.frame = 1;              // [BYTES seg16:0x91e]
        e.start.set(now + static_cast<std::uint32_t>(static_cast<std::int32_t>(mul16(static_cast<std::int16_t>(k), 20))));  // [BYTES seg16:0x924-0x935]
        e.end = 10;                     // [BYTES seg16:0x939]
        e.pool = w.roles.ships;         // DGROUP:0x9324 [BYTES seg16:0x942]
        e.rec = static_cast<std::int16_t>(ship);  // [BYTES seg16:0x949]
        std::int16_t ox = 16, oy = 16;  // dword 0x100010 for k = 0 [BYTES seg16:0x953]
        if (k != 0) {  // x first [BYTES seg16:0x95d-0x97c]
            ox = sub16(imod16(static_cast<std::int16_t>(c.rng.rand()), 60), 12);
            oy = sub16(imod16(static_cast<std::int16_t>(c.rng.rand()), 60), 12);
        }
        const ObjectRecord& o = w.pools.rec(w.roles.ships, ship);
        c.st.sides.centre[ix(j)] = EffectPoint{add16(o.scr_x, ox), add16(o.scr_y, oy)};  // [BYTES seg16:0x99c seg16:0x9a7]
        c.st.sides.point[ix(j)] = EffectPoint{add16(o.x, 0x80), add16(o.y, 0x80)};       // [BYTES seg16:0x9b2 seg16:0x9bd]
        w.w9430 = add16(w.w9430, 1);  // [BYTES seg16:0x9c1]
    }
    s.be2 = 1;  // `tumbling` [BYTES seg16:0x9ff]
    w.pools.rec(w.roles.ships, ship).angle = 1;  // object +0x26 [BYTES seg16:0xa19]
    s.lockout.set(now + 6000u);                  // [BYTES seg16:0xa21-0xa2d]
    burn_ship_sheet(c, ship);                    // seg13:0x4710 [BYTES seg16:0xa37]
    s.bec = 0;                                   // `alive` [BYTES seg16:0xa3d]
    // The duel [BYTES seg16:0xa42-0xa9b]: DGROUP:0x668a = -1 when the player dies or no other
    // live ship in play remains; pool 14 by its literal pointer DGROUP:0x1e30.
    if (w.kind == 7) {
        bool flag = true;
        for (int k = 0; k <= w.last_ship && k < kShipCount; ++k) {
            if (k == w.player || w.ships[ix(k)].bec == 0) continue;
            if (record_ok(w.pools, kPoolShips, k) && w.pools.rec(kPoolShips, k).state == 0) flag = false;
        }
        if (ship == w.player) flag = true;
        if (flag) w.gate[0].w12 = -1;  // DGROUP:0x668a (update.cpp ship_entry)
    }
    c.host.sound(0x45, w.roles.ships, ship, 1);  // [BYTES seg16:0xaac]
    if (ship == w.player) {  // [BYTES seg16:0xab4-0xb2b]
        if (w.player_game != 0) {
            w.w1e84 = now + 8000u;  // [BYTES seg16:0xacd]
        } else {
            w.w1e84 = 0;                // [BYTES seg16:0xad3]
            w.w1e80 = now + 8000u;      // [BYTES seg16:0xae6]
            c.st.dg_95a8 = 1;           // [BYTES seg16:0xaea]
        }
        // The rating: (4 r) / 5 in a player game, r / 2 in the demo, signed 32-bit idiv
        // [BYTES seg16:0xaf0-0xb19].
        const std::int32_t r = static_cast<std::int32_t>(w.rating);
        const std::int32_t nr = w.player_game != 0
                                    ? static_cast<std::int32_t>(static_cast<std::uint32_t>(r) << 2) / 5
                                    : r / 2;
        w.rating = static_cast<std::uint32_t>(nr);
        if (w.player_game == 0 && c.in.autopilot == 0) c.host.seg11_aece();  // [BYTES seg16:0xb2b]
    }
    if (w.w9130 == 0) drop_pod(c, ship, 1);  // seg13:0x498e(ship, 1) [BYTES seg16:0xb3c]
}

std::int16_t fragment_count(std::int16_t b, std::int16_t detail, std::int16_t in_use) {
    std::int16_t n = 2;
    if (b == 1) {
        n = 1;  // [BYTES seg12:0x2816-0x2822]
    } else {
        switch (detail) {  // [BYTES seg12:0x2824-0x28c7]
        case 0: n = idiv16(mul16(b, 30), 100); break;
        case 1: n = idiv16(mul16(b, 60), 100); break;
        case 2: n = b; break;
        case 3: n = idiv16(mul16(b, 130), 100); break;
        default: n = 2; break;  // the original's [bp-8] is unset [DESIGN ADR 0016 decision 1]
        }
        if (n < 2) n = 2;  // [BYTES seg12:0x2852-0x285d]
    }
    if (in_use > 10) {  // [BYTES seg12:0x2860-0x287f]
        n = idiv16(mul16(sub16(35, in_use), n), 25);
        if (n < 1) n = 1;
    }
    return n;
}

// seg12:0x274a [BYTES seg12:0x274a-0x2b1d]
void object_destroyed(EffectCtx& c, int pool, int rec) {
    World& w = c.w;
    Pools& pools = w.pools;
    if (pool < 0 || pool >= kPoolCount) return;  // [DESIGN]
    if (!pools.destroy_index_ok(pool, static_cast<std::int16_t>(rec), w.roles.ships, w.last_ship)) return;
    if (!record_ok(pools, pool, rec)) return;  // [DESIGN] storage bound
    const std::uint32_t now = c.in.now;
    if (pool == 4) w.w698c = now + 10000u;  // DGROUP:0x698c, World::w698c [BYTES seg12:0x2787-0x279d]
    // A pool-8 record of kind 1 (DGROUP:0x3d6c + 0x34·rec) keeps its square and its state
    // [BYTES seg12:0x27a1-0x27b0 seg12:0x2a78-0x2a87].
    const bool kind1 = pool == kPoolGatePosts && rec < kPool8Max && w.pool8[ix(rec)].at(0x1c) == 1;
    if (!kind1) {
        const ObjectRecord& o = pools.rec(pool, rec);
        const Point16 p = screen_of(c, o.x, o.y);  // seg8:0x29ea [BYTES seg12:0x27ce]
        if (in_play_area(p.x, p.y, w.W, w.H) && c.out)  // [BYTES seg12:0x27da-0x2803]
            c.out->pat_blt(kDcWindow, o.scr_x, o.scr_y, o.size, o.size, kBlackness);
    }
    const std::int16_t n = fragment_count(pools.rec(pool, rec).blast_size, w.detail, w.w9430);
    for (std::int16_t j = 0; n > j; j = add16(j, 1)) {  // [BYTES seg12:0x2a40-0x2a49]
        const int s = find_free_effect(w.effects);
        if (s < 0) continue;  // [BYTES seg12:0x2a55-0x2a59]
        EffectEntry& e = w.effects[ix(s)];
        e.type = j == 0 ? 4 : 1;  // [BYTES seg12:0x2a6f seg12:0x28ce]
        e.frame = 1;              // [BYTES seg12:0x28d8]
        if (s > w.w9434) w.w9434 = static_cast<std::int16_t>(s);  // [BYTES seg12:0x28de-0x28e9]
        // The start: now + rand() % 110 + 30 j, 32-bit [BYTES seg12:0x28ec-0x2917].
        const std::int16_t r = imod16(static_cast<std::int16_t>(c.rng.rand()), 110);
        e.start.set(now + static_cast<std::uint32_t>(static_cast<std::int32_t>(r) +
                                                     static_cast<std::int32_t>(mul16(j, 30))));
        e.end = 10;  // [BYTES seg12:0x291b]
        // Past Detail / 2 + 2 a fragment may be short and late [BYTES seg12:0x2921-0x2953].
        if (add16(sar16(w.detail, 1), 2) < j) {
            if (imod16(static_cast<std::int16_t>(c.rng.rand()), 10) <= 6) {
                e.end = sub16(e.end, add16(imod16(static_cast<std::int16_t>(c.rng.rand()), 4), 4));
                e.start.set(e.start.get() + 60u);
            }
        }
        e.pool = static_cast<std::int16_t>(pool);  // [BYTES seg12:0x295f]
        e.rec = static_cast<std::int16_t>(rec);    // [BYTES seg12:0x2966]
        const ObjectRecord& o = pools.rec(pool, rec);
        EffectPoint wp{o.x, o.y};  // fragment 0 [BYTES seg12:0x297c-0x298e]
        if (j != 0) {
            // x then y: (rand() % (2 (n + 40)) - n - 40)·8 around x + extent / 2 [BYTES
            // seg12:0x29a0-0x2a1b] (verified claim 24).
            const std::int16_t span = shl16(add16(n, 0x28), 1);
            const std::int16_t half = idiv16(o.extent, 2);
            std::int16_t off = shl16(sub16(sub16(imod16(static_cast<std::int16_t>(c.rng.rand()), span), n), 0x28), 3);
            wp.x = add16(add16(half, off), o.x);
            off = shl16(sub16(sub16(imod16(static_cast<std::int16_t>(c.rng.rand()), span), n), 0x28), 3);
            wp.y = add16(add16(off, half), o.y);
        }
        c.st.sides.point[ix(s)] = wp;
        const Point16 sc = screen_of(c, wp.x, wp.y);  // seg8:0x29ea [BYTES seg12:0x2a28-0x2a38]
        c.st.sides.centre[ix(s)] = EffectPoint{sc.x, sc.y};
        w.w9430 = add16(w.w9430, 1);  // [BYTES seg12:0x2a3c]
    }
    if (!kind1) {
        pools.rec(pool, rec).state = 1;  // [BYTES seg12:0x2a97]
        if (pool == w.roles.ships && rec == w.player) {  // [BYTES seg12:0x2a9d-0x2ae2]
            if (w.player_game != 0) {
                w.w1e84 = now + 1500u;  // [BYTES seg12:0x2ac1]
            } else {
                w.w1e84 = 0;
                w.w1e80 = now + 1500u;  // [BYTES seg12:0x2adf]
            }
        }
    }
    if (pool == 6) {  // [BYTES seg12:0x2ae6-0x2afd] (s2-missile.md claim 79)
        missile_destroyed(w.pools, rec, c.weapons ? *c.weapons : missile_state().words);
        c.host.pool6_destroyed(rec);
    }
    c.host.sound(0x45, pool, rec, 1);            // [BYTES seg12:0x2b0d]
}

// ---- seg7:0x2e02 (M4 s3 FUEL)

void pickup_destroyed(CollideCtx& x, int pool, int rec) {
    World& w = x.w;
    if (!record_ok(w.pools, pool, rec)) return;  // [DESIGN] bounds
    ObjectRecord& o = w.pools.rec(pool, rec);
    if (o.state == 0) {  // [BYTES seg7:0x2e1f-0x2e24]
        o.state = 1;     // [BYTES seg7:0x2e28]
        x.h.erase(o.scr_x, o.scr_y, o.size, o.size);  // PatBlt BLACKNESS [BYTES seg7:0x2e2e-0x2e4f]
        int e = 0;
        for (; e < 4; ++e) {  // free, or of type <= 5 [BYTES seg7:0x2ffd-0x3013]
            const EffectEntry& E = w.effects[ix(e)];
            if (E.frame == kEffectFree || !(E.type > 5)) break;
        }
        if (e < 4) {
            EffectState& st = effect_state();
            // DGROUP:0x5e98 + 2e: the old source released for any pool, then 0 [BYTES
            // seg7:0x3015-0x3039] (verified claim 30); the port releases only its own [DESIGN].
            if (st.piece_src[ix(e)] != kDcNone) {
                if (st.piece_src_owned[ix(e)] && effect_links().release) effect_links().release(st.piece_src[ix(e)]);
                st.piece_src[ix(e)] = kDcNone;
                st.piece_src_owned[ix(e)] = false;
            }
            // The new source by pool [BYTES seg7:0x303f-0x306e seg7:0x2e5c-0x2ec2]; latched, as
            // the fragment's record may be reused before the break-up ends.
            if (effect_links().break_source) {
                const BreakSource src = effect_links().break_source(pool, rec);
                st.piece_src[ix(e)] = src.dc;
                st.piece_src_owned[ix(e)] = src.owned;
            }
            int id = 0;
            if (pool == 1 || pool == 8) {
                id = 0x1a;  // FLANGE [BYTES seg7:0x303f-0x3076 seg7:0x2e5c-0x2e79]
            } else if (pool == 6) {
                const std::int16_t W = w.pools.missile(rec).weapon;  // DGROUP:0x66e4 + 0x1c·rec
                id = x.h.weapon_kind(W) == 6 ? 0x1a : 0x1f;          // DGROUP:0x2f78 [BYTES seg7:0x2e9e-0x2ea5]
            } else if (pool == 4) {
                id = 0x1f;  // SHATTER [BYTES seg7:0x2ea8-0x2ec2]
            }
            if (id != 0) x.h.sound(id, pool, rec, 1);  // seg10:0x754a(id, pool, rec, 1)
            EffectEntry& E = w.effects[ix(e)];
            // An entry in use moves to the first free entry of 4 ... 44 (`jle` on -1: signed)
            // [BYTES seg7:0x2eca-0x2f5e].
            if (static_cast<std::int16_t>(E.frame) > -1) {
                for (int j = 4; j < kEffectCount; ++j) {
                    EffectEntry& F = w.effects[ix(j)];
                    if (F.frame != kEffectFree) continue;
                    F.frame = E.frame;
                    F.end = E.end;
                    F.pool = E.pool;
                    F.rec = E.rec;
                    F.start = E.start;
                    F.type = E.type;
                    F.side = E.side;
                    F.brush = E.brush;  // +0x04, the step, is not copied
                    st.sides.centre[ix(j)] = st.sides.centre[ix(e)];
                    st.sides.point[ix(j)] = st.sides.point[ix(e)];
                    break;
                }
            }
            E.frame = 1;  // dword 0x00140001 [BYTES seg7:0x2f62-0x2f66]
            E.end = 20;
            w.w9430 = add16(w.w9430, 1);  // [BYTES seg7:0x2f6f]
            E.type = 6;                   // [BYTES seg7:0x2f73]
            E.side = idiv16(o.size, 5);   // [BYTES seg7:0x2f92-0x2f9b]
            E.step = e > w.detail ? 2 : 1;  // [BYTES seg7:0x2f9f-0x2fb3]
            E.pool = static_cast<std::int16_t>(pool);
            E.rec = static_cast<std::int16_t>(rec);
            st.sides.centre[ix(e)] = EffectPoint{o.scr_x, o.scr_y};  // DGROUP:0x5170 [BYTES seg7:0x2fe6-0x2fea]
            st.sides.point[ix(e)] = EffectPoint{o.x, o.y};           // DGROUP:0x5224 [BYTES seg7:0x2fee-0x2ff3]
        }
    }
    // The icon's column row, even when `state` was already 1 [BYTES seg7:0x3079-0x308e]
    if (pool == 1) {
        const std::int16_t icon =
            rec >= 0 && rec < static_cast<int>(w.fragment_icon.size()) ? w.fragment_icon[ix(rec)] : std::int16_t{-1};
        x.h.column_row(0, icon);
    }
}

}  // namespace is::world
