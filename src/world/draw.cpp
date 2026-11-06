// The wave frame's drawing: seg7:0x1208, seg7:0x3a42 and its frame loop, the rock cell, the
// ship bitmap table (docs/re/world.md "Drawing"). 16-bit arithmetic as the original's
// (ADR 0013 decision 7).
#include "world/draw.h"

namespace is::world {

PlayerCell player_cell(std::int16_t type, std::int16_t heading, std::int16_t size,
                       std::int16_t gap, std::uint8_t ship_be9) {
    PlayerCell cell;
    // Column [BYTES seg7:0x1315 seg7:0x131b seg7:0x1338 seg7:0x133d seg7:0x1346 seg7:0x1349 seg7:0x1358].
    if (type_cells_by_heading(type))
        cell.src_x = add16(mul16(heading, add16(gap, size)), gap);
    else
        cell.src_x = add16(mul16(size, 16), mul16(gap, 17));
    // Row [BYTES seg7:0x135c seg7:0x1365 seg7:0x1371 seg7:0x1384].
    if (type < 4) {
        if (ship_be9 == 0)
            cell.keep_y = true;
        else
            cell.src_y = gap;
    } else {
        cell.src_y = add16(mul16(gap, 2), size);
    }
    return cell;
}

bool player_sprite_drawn(const DrawContext& c) {
    const DrawGlobals& g = c.g;
    const int player = g.sprite.player;
    if (g.state != 1) return false;                                // [BYTES seg7:0x1216]
    if (c.pools.rec(kPoolShips, player).state != 0) return false;  // DGROUP:0x1e30 [BYTES seg7:0x1226]
    if (g.sprite.dg_951a != 0 || g.dg_9130 != 0) return false;  // [BYTES seg7:0x122d seg7:0x1234]
    // [BYTES seg7:0x123e seg7:0x1245 seg7:0x124f seg7:0x1257]
    if (g.sprite.dg_9e49 > 2 && g.sprite.dg_9e49 < 7 && player == g.sprite.dg_952e) return false;
    if (g.dg_3a38 == 0) return false;
    // The effect skip: ship byte +0xec = 0 and an entry 0..DGROUP:0x9434 with +6 = 14,
    // +8 = the player and 0 < +0 < 9, signed words [BYTES seg7:0x1266 seg7:0x127a
    // seg7:0x1283 seg7:0x128d seg7:0x1293 seg7:0x129a].
    if (c.ships[static_cast<std::size_t>(player)].bec == 0) {
        for (int k = 0; k <= g.last_effect && k < kEffectCount; ++k) {
            const EffectEntry& e = c.effects[static_cast<std::size_t>(k)];
            const std::int16_t frame = static_cast<std::int16_t>(e.frame);  // +0x00, the frame counter
            if (e.pool == 0xe && e.rec == player && frame > 0 && frame < 9) return false;
        }
    }
    return true;
}

void draw_player_sprite(DrawContext& c) {
    if (!player_sprite_drawn(c)) return;
    DrawGlobals& g = c.g;
    const int player = g.sprite.player;
    ObjectRecord& o = c.pools.rec(kPoolShips, player);
    const ShipRecord& ship = c.ships[static_cast<std::size_t>(player)];
    const std::int16_t size = o.size, gap = o.gap;
    // SetRect(DGROUP:0x1e5c, x + 2, y + 2, x + size - 2, y + size - 2) [BYTES seg7:0x12e4].
    g.player_rect = Rect16{add16(o.scr_x, 2), add16(o.scr_y, 2), sub16(add16(size, o.scr_x), 2),
                           sub16(add16(o.scr_y, size), 2)};
    const PlayerCell cell = player_cell(ship.type, o.frame, size, gap, ship.be9);
    o.src_x = cell.src_x;
    if (!cell.keep_y) o.src_y = cell.src_y;
    // seg8:0xb76((char)player | 0x0e00, 1, DGROUP:0x39e8[slot + 0x28], 0) [BYTES seg7:0x1388
    // seg7:0x1397 seg7:0x139d]: the pool is 14 whatever DGROUP:0x9324 holds.
    const std::int16_t pr = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(player))) |
        0x0e00u);
    const int slot = ship_image_slot(player, ship);
    const DcId src =
        slot >= 0 && slot < kImageSlots ? c.images[static_cast<std::size_t>(slot)] : kDcNone;
    draw_sprite(c.pools, pr, 1, src, 0, g.sprite, c.to_screen, c.out);
    g.dg_58a = 0;  // [BYTES seg7:0x13a2]
}

void rock_cell(ObjectRecord& rock) {
    // [BYTES seg7:0x3d51 seg7:0x3d5d seg7:0x3d5f seg7:0x3d75 seg7:0x3d79 seg7:0x3d81
    // seg7:0x3d83 seg7:0x3d87]
    rock.frame = idiv16(rock.angle, 100);
    rock.src_x = add16(mul16(add16(rock.size, rock.gap), rock.frame), rock.gap);
}

void dissolve_icon(DrawContext& c, int i) {
    DrawGlobals& g = c.g;
    // Size and gap of pool-0 record 0 [BYTES seg7:0x3a55 seg7:0x3a5c].
    const std::int16_t size = c.pools.rec(kPoolIcons, 0).size;
    const std::int16_t gap = c.pools.rec(kPoolIcons, 0).gap;
    ObjectRecord& o = c.pools.rec(kPoolIcons, i);
    // Byte +0x51 -= DGROUP:0x8ffe, floor 0 as a signed byte [BYTES seg7:0x3a63 seg7:0x3a6a
    // seg7:0x3a72 seg7:0x3a79].
    o.race_status = static_cast<std::uint8_t>(o.race_status - g.dissolve_step);
    if (static_cast<std::int8_t>(o.race_status) < 0) o.race_status = 0;
    if (o.on_screen == 0 && !(c.hooks && c.hooks->seg13_59e8(0, i))) return;  // [BYTES seg7:0x3a86 seg7:0x3a92]

    // The cell grows inside the strip: StretchBlt COLORONCOLOR from 2 px around it
    // [BYTES seg7:0x3aa0-0x3ae3].
    const std::int16_t sx = add16(mul16(add16(gap, size), static_cast<std::int16_t>(i)), gap);
    c.out.set_stretch_mode(g.strip_dc, kColorOnColor);
    c.out.stretch_blt(g.strip_dc, sx, gap, size, size, g.strip_dc, sub16(sx, 2), sub16(gap, 2),
                      add16(size, 4), add16(size, 4), kSrcCopy);
    // Then onto the icon's screen square [BYTES seg7:0x3ae8-0x3b14].
    c.out.bit_blt(g.sprite.window_dc, o.scr_x, o.scr_y, size, size, g.strip_dc, sx, gap,
                  kSrcCopy);
    // The player's ship drawn over it on odd steps when it is the one +0x48 names and
    // Detail > 1 [BYTES seg7:0x3b19-0x3b38]. What +0x48 holds for an icon is [OPEN].
    if (o.blast_size == g.sprite.player && g.detail > 1 && (o.race_status & 1) != 0)
        draw_player_sprite(c);
    // The last step blacks the screen square and the strip cell [BYTES seg7:0x3b49-0x3b9d].
    if (static_cast<std::int8_t>(o.race_status) < 1) {
        c.out.pat_blt(g.sprite.window_dc, o.scr_x, o.scr_y, size, size, kBlackness);
        c.out.pat_blt(g.strip_dc, sx, gap, size, size, kBlackness);
        if (c.hooks) {
            c.hooks->seg7_65aa(0, i);
            c.hooks->seg7_2080(0, -1);
        }
    }
}

void dissolve_pass(DrawContext& c) {
    // [BYTES seg7:0xbf2 seg7:0xc01 seg7:0xc06 seg7:0xc0b seg7:0xc14 seg7:0xc1a]
    for (int i = 0; i < c.pools.in_use(kPoolIcons) && i < c.pools.records(kPoolIcons); ++i)
        if (c.pools.rec(kPoolIcons, i).race_status != 0) dissolve_icon(c, i);
}

std::int16_t star_count_for(std::int16_t detail, std::int16_t bright, std::int16_t current) {
    std::int16_t n = current;
    switch (detail) {  // [BYTES seg6:0xe36 seg6:0xe3b seg6:0xe49 seg6:0xe57 seg6:0xe65]
    case 0: n = 4; break;
    case 1: n = 7; break;
    case 2: n = 13; break;
    case 3: n = 18; break;
    default: break;
    }
    if (bright != 0) n = sub16(n, 1);  // [BYTES seg6:0xe88 seg6:0xe94]
    return n;
}

namespace {

// rand() % d as `cwd; idiv`; 0 for d = 0, where the original faults [DESIGN].
std::int16_t rmod16(shell::Rng& rng, std::int16_t d) {
    const std::int16_t r = static_cast<std::int16_t>(rng.rand());
    return d == 0 ? std::int16_t{0} : imod16(r, d);
}

// The colour words DGROUP:0x2f04 (low) and 0x2f06 (high) of a star.
std::uint32_t star_colour(std::uint16_t lo, std::uint16_t hi) {
    return static_cast<std::uint32_t>(lo) | (static_cast<std::uint32_t>(hi) << 16);
}

// One wrap's re-draw: the other coordinate % span, the colour words and the size
// [BYTES seg8:0x16cf seg8:0x16d5 seg8:0x16e9 seg8:0x16fa seg8:0x170f seg8:0x171f].
void star_respawn(Star& s, std::int16_t& other, std::int16_t span, shell::Rng& rng) {
    other = rmod16(rng, span);
    const std::int16_t g = rmod16(rng, 255);
    const std::int16_t b = rmod16(rng, 255);
    // DGROUP:0x2f04 = (g << 8) | 0xff; DGROUP:0x2f06 = b & 0xff, no 0x02 here.
    s.colour = star_colour(static_cast<std::uint16_t>(((g & 0xff) << 8) | 0xff),
                           static_cast<std::uint16_t>(b & 0xff));
    s.speed = static_cast<std::uint8_t>(rmod16(rng, 3) + 1);
}

}  // namespace

void init_stars(World& w, shell::Rng& rng) {
    if (w.star_count > kStarMax) w.star_count = kStarMax;  // the table's 18 [DESIGN bound]
    if (w.stars.size() < static_cast<std::size_t>(kStarMax)) w.stars.resize(kStarMax);
    for (int i = 0; i < w.star_count; ++i) {
        Star& s = w.stars[static_cast<std::size_t>(i)];
        s.x = rmod16(rng, w.W);   // [BYTES seg8:0x1924 seg8:0x192a]
        s.y = rmod16(rng, w.H);   // [BYTES seg8:0x193a seg8:0x1940]
        const std::int16_t b = rmod16(rng, 255);  // DGROUP:0x2f06 = 0x0200 | b [BYTES seg8:0x1953]
        const std::int16_t g = rmod16(rng, 255);  // DGROUP:0x2f04 = (g << 8) | 0xff [BYTES seg8:0x1966]
        s.colour = star_colour(static_cast<std::uint16_t>(((g & 0xff) << 8) | 0xff),
                               static_cast<std::uint16_t>(0x0200 | (b & 0xff)));
        s.speed = static_cast<std::uint8_t>(rmod16(rng, 3) + 1);  // [BYTES seg8:0x197e]
        s.phase = static_cast<std::uint8_t>(rmod16(rng, 40));     // [BYTES seg8:0x1990]
        s.on = 1;                                                 // [BYTES seg8:0x1994]
    }
}

int star_brush(std::uint8_t phase) {
    const int v = static_cast<std::int8_t>(phase) / 5;
    return v < 4 ? v + 10 : 18 - v;
}

void draw_stars(World& w, const StarInputs& in, const WaveGraphics& wg, Blitter& out,
                shell::Rng& rng, int arg, const BrightStars& bright) {
    // The toggle [BYTES seg8:0x1599 seg8:0x15a4 seg8:0x15b0 seg8:0x15bb].
    w.w06ae = add16(w.w06ae, 1);
    if (w.w06ae > 1) w.w06ae = 0;
    if (arg != 0) w.w06ae = 0;
    if (w.w06ae != 0) return;
    if (w.bright != 0) {  // [BYTES seg8:0x15bf seg8:0x15ca]
        if (bright) bright(arg);
        return;
    }
    const int max = in.size == 0 ? 2 : 3;  // [BYTES seg8:0x15d0 seg8:0x15de]
    if (w.pools.records(in.ship_pool) > in.player) {
        ObjectRecord& p = w.pools.rec(in.ship_pool, in.player);
        // [BYTES seg8:0x15f5 seg8:0x15fc seg8:0x1603 seg8:0x161a seg8:0x1630]
        if (p.state == 1 || (p.b54 != 0 && in.demons_gate != 0)) {
            p.vx = 0;
            p.vy = 0;
        }
    }
    const Rect16& r = w.play_rect;  // DGROUP:0x1d30
    if (w.stars.size() < static_cast<std::size_t>(kStarMax)) w.stars.resize(kStarMax);
    const int count = w.star_count < kStarMax ? w.star_count : kStarMax;
    for (int i = 0; i < count; ++i) {
        Star& s = w.stars[static_cast<std::size_t>(i)];
        const int side = static_cast<std::int8_t>(s.speed) > max ? max : static_cast<std::int8_t>(s.speed);
        if (s.on != 0) out.pat_blt(in.window_dc, s.x, s.y, side, side, kBlackness);  // [BYTES seg8:0x186f seg8:0x189e]
        if (arg == 0) {
            // The shift counts are bytes: Size + 1 and 6 - size [BYTES seg8:0x18ab seg8:0x18bc
            // seg8:0x18c4].
            const int a = (in.size + 1) & 0x1f;
            const int b = (6 - static_cast<std::int8_t>(s.speed)) & 0x1f;
            auto move = [a, b](std::int16_t v) { return sar16(shl16(v, a & 15), b & 15); };
            std::int16_t vx = in.auto_vx, vy = in.auto_vy;  // [BYTES seg8:0x18df seg8:0x18ef]
            if (in.autopilot == 0 && w.pools.records(in.ship_pool) > in.player) {
                const ObjectRecord& p = w.pools.rec(in.ship_pool, in.player);
                vx = p.vx, vy = p.vy;  // [BYTES seg8:0x163e seg8:0x1664]
            }
            s.x = sub16(s.x, move(vx));
            s.y = sub16(s.y, move(vy));
            if (in.shake != 0) {  // [BYTES seg8:0x167f]
                s.x = add16(s.x, move(in.shake_x));
                s.y = add16(s.y, move(in.shake_y));
            }
            // The wraps: off the left to the right edge and back, then the same for y; the
            // other coordinate is redrawn without the rectangle's origin [BYTES seg8:0x16a9
            // seg8:0x16b2 seg8:0x1749 seg8:0x1799].
            if (s.x < r.l || s.x > r.r) {
                s.x = s.x < r.l ? r.r : r.l;
                star_respawn(s, s.y, sub16(r.b, r.t), rng);
            }
            if (s.y < r.t || s.y > r.b) {
                s.y = s.y < r.t ? r.b : r.t;
                star_respawn(s, s.x, sub16(r.r, r.l), rng);
            }
        }
        if (w.star_wanted == w.star_count) {  // [BYTES seg8:0x179d]
            if (!pt_in_rect(w.player_cell, s.x, s.y)) {  // DGROUP:0x1e5c [BYTES seg8:0x17a0]
                const int n = star_brush(s.phase);
                const int side2 = static_cast<std::int8_t>(s.speed) > max ? max : static_cast<std::int8_t>(s.speed);
                // [BYTES seg8:0x17fe seg8:0x1827 seg8:0x183b]
                const BrushId old = out.select_brush(in.window_dc, wg.star[static_cast<std::size_t>(n)]);
                out.pat_blt(in.window_dc, s.x, s.y, side2, side2, kPatCopy);
                out.select_brush(in.window_dc, old);
                // [BYTES seg8:0x1848 seg8:0x1850]
                s.phase = static_cast<std::int8_t>(s.phase) < 40 ? static_cast<std::uint8_t>(s.phase + 1) : 0;
                s.on = 1;
            } else {
                s.on = 0;  // [BYTES seg8:0x185a]
            }
        }
    }
    if (w.star_wanted != w.star_count) w.star_count = w.star_wanted;  // [BYTES seg8:0x18f6 seg8:0x18ff]
}

void draw_lines(World& w, std::int16_t ship_pool, DcId window_dc, LinkHooks* hooks) {
    Pools& p = w.pools;
    auto rec_ok = [&p](int pool, int rec) {
        return pool >= 0 && pool < kPoolCount && rec >= 0 && rec < p.records(pool);
    };
    // The ship test of an end: byte +0xec = 0 or +0x147 != 0 [BYTES seg12:0x37a6-0x37f2].
    auto ship_gone = [&w, ship_pool](int pool, int rec) {
        if (pool != ship_pool || rec < 0 || rec >= kShipCount) return false;
        const ShipRecord& s = w.ships[static_cast<std::size_t>(rec)];
        return s.bec == 0 || sbyte_at(s, 0x147) != 0;
    };
    const int n = p.allocated(10);  // DGROUP:0x9300
    for (int k = 0; k < n && k < kLinkSlots; ++k) {
        LinkSlot& l = w.links[static_cast<std::size_t>(k)];
        if (l.pool_a == -1) continue;  // [BYTES seg12:0x3792]
        bool broken = ship_gone(l.pool_a, l.rec_a) || ship_gone(l.pool_b, l.rec_b);
        // An end out of the port's pools counts as gone [DESIGN: the original reads on].
        const bool a_ok = rec_ok(l.pool_a, l.rec_a), b_ok = rec_ok(l.pool_b, l.rec_b);
        if (!a_ok || p.rec(l.pool_a, l.rec_a).state != 0) broken = true;
        if (!b_ok || p.rec(l.pool_b, l.rec_b).state != 0) broken = true;
        if (!broken) continue;
        // [BYTES seg12:0x3832-0x390c]
        if (k < p.records(10)) p.rec(10, k).state = 1;
        if (a_ok) p.rec(l.pool_a, l.rec_a).owner_pool = -1;  // +0x32
        if (b_ok) p.rec(l.pool_b, l.rec_b).w36 = -1;         // +0x36
        l.pool_a = -1;
        l.pool_b = -1;
        l.w08 = 0;
        // BLACK_PEN, seg12:0x7256(hdc, 0x65e8, 0x65ea, 0x65e0, 0x65e2), then 0x65e0/2 <- 0x65dc/e
        // and 0x65e8/a <- 0x65e4/6 [BYTES seg12:0x38c0 seg12:0x38d5 seg12:0x38f9].
        if (hooks) hooks->line(window_dc, l.line[6], l.line[7], l.line[2], l.line[3]);
        l.line[2] = l.line[0];
        l.line[3] = l.line[1];
        l.line[6] = l.line[4];
        l.line[7] = l.line[5];
        if (hooks) hooks->seg7_2080(10, -1);
    }
}

// ---- seg13:0x3d40 (M4 s3 AMBFLIGHT; core.md "The ship's part", seg13:0x3d40 steps 1-5, C044-C047)

namespace {

std::int16_t min16(std::int16_t a, std::int16_t b) { return a < b ? a : b; }

// One axis of the overlap: the part of a square at offset o (O's origin minus S's) that lies
// under the other. `under` (part a, seg13:0x3f7c-0x400b): O's cell into S's buffer, source
// offset in O and destination offset in S; else (part c, seg13:0x4219-0x42ae) S's pixels over
// O's buffer, source offset in S and destination offset in O.
struct Span {
    std::int16_t src = 0, dst = 0, len = 0;
};
Span overlap_a(std::int16_t o, std::int16_t Bo, std::int16_t Ss) {
    if (o < 0) return Span{sub16(0, o), 0, min16(add16(Bo, o), Ss)};
    return Span{0, o, min16(sub16(Ss, o), Bo)};
}
Span overlap_c(std::int16_t o, std::int16_t Bo, std::int16_t Ss) {
    if (o > 0) return Span{o, 0, min16(Bo, sub16(Ss, o))};
    return Span{0, sub16(0, o), min16(add16(Bo, o), Ss)};
}

}  // namespace

void pickup_composite(DrawContext& c, std::int16_t den, std::int16_t num, int o_rec, int o_pool,
                      int s_rec, int s_pool) {
    Pools& p = c.pools;
    const auto ok = [&p](int pool, int rec) {
        return pool >= 0 && pool < kPoolCount && rec >= 0 && rec < p.records(pool);
    };
    // The seg18 mode's outer pool 15 (seg13:0x3b88) is M6: the original still refreshes and
    // draws the ship's buffer for it (steps 3 and 5), the port draws nothing [BYTES seg13:0x3e4f
    // seg13:0x3e57 seg13:0x444e].
    if (o_pool == 15 || !ok(o_pool, o_rec) || !ok(s_pool, s_rec)) return;
    ObjectRecord& S = p.rec(s_pool, s_rec);
    ObjectRecord& O = p.rec(o_pool, o_rec);
    const DcId s3a = static_cast<DcId>(S.w3a), s3c = S.alt_dc;
    const DcId o3a = static_cast<DcId>(O.w3a), o3c = O.alt_dc;
    if (s3c == kDcNone || o3c == kDcNone) return;  // [DESIGN] no buffer to draw into
    Blitter& out = c.out;
    if (den == 0) den = 1;  // [BYTES seg13:0x3d4f]
    // seg8:0x29ea of both positions; O's offset from S on the screen [BYTES seg13:0x3d5a-0x3dbf]
    const Point16 ps = c.to_screen(S.x, S.y);
    const Point16 po = c.to_screen(O.x, O.y);
    const std::int16_t ox = sub16(po.x, ps.x), oy = sub16(po.y, ps.y);
    const std::int16_t Ss = S.size, g = S.gap;
    // lo16(Ss·num) / den: `imul word`, `cwd`, `idiv` (C044) [BYTES seg13:0x3de2-0x3de9]
    const std::int16_t s2 = (den == -1 && mul16(Ss, num) == INT16_MIN) ? std::int16_t{0} : idiv16(mul16(Ss, num), den);
    const std::int16_t off = add16(g, idiv16(sub16(Ss, s2), 2));  // `cwd; sub; sar` [BYTES seg13:0x3df0-0x3df7]
    const std::int16_t A = O.size, ag = O.gap;                    // [BYTES seg13:0x3dfc-0x3e27]
    const std::int16_t Bo = add16(mul16(ag, 2), A), Bs = add16(mul16(g, 2), Ss);
    const std::int16_t cellS = add16(mul16(S.frame, add16(g, Ss)), g);
    const std::int16_t cellO = add16(mul16(O.frame, add16(ag, A)), ag);
    const std::int16_t lo = sub16(-2, A), hi = add16(Ss, 2);
    const bool overlap = lo <= ox && lo <= oy && ox <= hi && oy <= hi;  // signed (C045) [BYTES seg13:0x3e2c-0x3e49]
    if (!overlap) {  // [BYTES seg13:0x3e53-0x3f79]
        out.pat_blt(o3c, 0, 0, Bo, Bo, kBlackness);
        out.bit_blt(o3c, ag, ag, A, A, o3a, cellO, ag, kSrcCopy);
        if (s2 == Ss) {
            out.bit_blt(s3c, g, g, Ss, Ss, s3a, cellS, g, kSrcCopy);
        } else {
            out.set_stretch_mode(s3c, kColorOnColor);
            out.pat_blt(s3c, 0, 0, Bs, Bs, kBlackness);
            out.stretch_blt(s3c, off, off, s2, s2, s3a, cellS, g, Ss, Ss, kSrcCopy);
        }
    } else {  // [BYTES seg13:0x3f7c-0x43c8]
        // a) O's cell under S's square, into S's buffer [BYTES seg13:0x3f7c-0x408e]
        const Span ax = overlap_a(ox, Bo, Ss), ay = overlap_a(oy, Bo, Ss);
        out.pat_blt(s3c, 0, 0, Bs, Bs, kBlackness);
        out.bit_blt(s3c, add16(g, ax.dst), add16(g, ay.dst), ax.len, ay.len, o3a, add16(cellO, ax.src),
                    add16(ag, ay.src), kSrcCopy);
        // b) the (scaled) ship cell M and its mask K (seg10:0x6ec6: 1 where M is black); the
        // palette selection of DGROUP:0x920e is the Blitter's (ADR 0007) [BYTES seg13:0x4093-0x4214]
        const DcId M = out.create_image(Bs, Bs);
        out.pat_blt(M, 0, 0, Bs, Bs, kBlackness);
        if (s2 != Ss) {
            out.set_stretch_mode(M, kColorOnColor);
            out.stretch_blt(M, off, off, s2, s2, s3a, cellS, g, Ss, Ss, kSrcCopy);
        } else {
            out.bit_blt(M, g, g, Ss, Ss, s3a, cellS, g, kSrcCopy);
        }
        const DcId K = out.create_mask(M);
        out.bit_blt(s3c, g, g, Ss, Ss, K, g, g, kSrcAnd);    // (C046) [BYTES seg13:0x41e4]
        out.bit_blt(s3c, g, g, Ss, Ss, M, g, g, kSrcPaint);  // [BYTES seg13:0x420e]
        // c) S over O's buffer where they overlap [BYTES seg13:0x4219-0x437e]
        const Span cx = overlap_c(ox, Bo, Ss), cy = overlap_c(oy, Bo, Ss);
        out.pat_blt(o3c, 0, 0, Bo, Bo, kBlackness);
        out.bit_blt(o3c, ag, ag, A, A, o3a, cellO, ag, kSrcCopy);
        out.bit_blt(o3c, add16(ag, cx.dst), add16(ag, cy.dst), cx.len, cy.len, K, add16(g, cx.src),
                    add16(g, cy.src), kSrcAnd);    // [BYTES seg13:0x4340]
        out.bit_blt(o3c, add16(ag, cx.dst), add16(ag, cy.dst), cx.len, cy.len, M, add16(g, cx.src),
                    add16(g, cy.src), kSrcPaint);  // [BYTES seg13:0x4378]
        out.delete_image(M);  // [BYTES seg13:0x4383-0x43c8]
        if (K != kDcNone) out.delete_image(K);
    }
    // Both drawn through seg8:0xb76, each src_x rewritten first and kept (C047) [BYTES seg13:0x43cd-0x446a]
    S.on_screen = 1;
    S.src_x = cellS;
    draw_sprite(p, pool_rec(s_pool, static_cast<std::int8_t>(s_rec) & 0xff), 1, s3a, 0, c.g.sprite, c.to_screen, out);
    S.on_screen = 0;
    O.src_x = cellO;
    draw_sprite(p, pool_rec(o_pool, o_rec), 1, o3a, 0, c.g.sprite, c.to_screen, out);
}

DcId load_ship_body(Blitter& out, const fmt::BodyRecord& body) {
    if (body.bitmap_id == fmt::kBodyBitmapFromFile) return kDcNone;  // [OPEN]
    return out.load_graphic(body.bitmap_id, 0, -1);                   // [DESIGN ADR 0013] [OPEN]
}

void set_ship_image(ImageSlotTable& images, int ship, const ShipRecord& record, DcId dc) {
    const int slot = ship_image_slot(ship, record);
    if (slot >= 0 && slot < kImageSlots) images[static_cast<std::size_t>(slot)] = dc;
}

}  // namespace is::world
