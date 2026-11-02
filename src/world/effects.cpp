// The effect table's routines (effects.h). Each step cites the original's addresses; the
// arithmetic is 16-bit where the original's is (ADR 0013 decision 7).
#include "world/effects.h"

#include "world/missile.h"

#include "world/death.h"

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

// seg8:0x1290(a, b) = (b / max(a, 3) + max(a, 3)) >> 1, 32-bit (s1-death.md claim 20, not
// verified) [BYTES seg8:0x1290] [OPEN].
std::int16_t mass_scale(std::int16_t a, std::int16_t b) {
    const std::int32_t m = a > 3 ? a : 3;
    return lo16(sar32(static_cast<std::int32_t>(b) / m + m, 1));
}

}  // namespace

EffectState& effect_state() {
    static EffectState s;
    return s;
}

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
            c.host.pieces(k);  // seg8:0x4256-0x452a [OPEN]
        } else {
            // 2. The explosion bitmap [BYTES seg8:0x404d-0x4067].
            const std::int16_t cnt = static_cast<std::int16_t>(e.frame);
            const std::int16_t f = sub16(cnt, 1);
            const std::int16_t side = sar16(shl16(add16(mul16(f, 6), 10), size & 15), 1);
            const EffectPoint cen = c.st.sides.centre[ix(k)];
            if (sub16(e.end, 1) > cnt) {
                // Not the last frame [BYTES seg8:0x41b8-0x422b].
                if (e.type > -1 && in_play_area(cen.x, cen.y, w.W, w.H) && c.out) {
                    const std::int16_t half = sar16(side, 1);
                    // min(f, 7) as `cmp bx, 7; jle`; f < 0 cannot come from a creator (they
                    // write 1): the port draws nothing then [DESIGN].
                    if (f >= 0) {
                        const std::int16_t sx =
                            sar16(shl16(kExplosionX[ix(f > 7 ? 7 : f)], size & 15), 1);
                        c.out->bit_blt(kDcWindow, sub16(cen.x, half), sub16(cen.y, half), side, side,
                                       slot_dc(c, kSlotExplosion), sx, 0, kSrcCopy);
                    }
                }
                // The blast at +0x00 % 6 = 0, type != 1 [BYTES seg8:0x4230-0x4250].
                if (imod16(cnt, 6) == 0 && e.type != 1) blast(c, k);
            } else if (e.type != kEffectTypeFF06) {
                // The last frame [BYTES seg8:0x407b-0x4187].
                const std::int16_t q = sar16(shl16(0x1a, size & 15), 1);
                const std::int16_t s52 = sar16(shl16(0x34, size & 15), 1);
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
    if (k < 4 && e.type == 6) c.host.piece_free(k);  // [BYTES seg8:0x458e-0x45cd] [OPEN]
}

// seg8:0x5ab0 [BYTES seg8:0x5ab0-0x5dee]. Read from the listing by the analyst (s1-death.md
// section 2) and by this port; no independent verifier has checked it [OPEN].
void blast(EffectCtx& c, int k) {
    if (k < 0 || k >= kEffectCount) return;
    World& w = c.w;
    const EffectEntry& e = w.effects[ix(k)];
    const std::int16_t sp = e.pool, sr = e.rec, type = e.type;  // [bp-6], [bp-8], [bp-4]
    const EffectPoint pt = c.st.sides.point[ix(k)];               // [bp-0x24], [bp-0x22]
    if (c.in.dg_1318 > 6) return;  // [BYTES seg8:0x5aec seg8:0x5af1] [OPEN]
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
            // (2v) >> Size for d, dx, dy [BYTES seg8:0x5d28-0x5d44]; the seg18 doubling with
            // DGROUP:0x90f4 = 6 is not ported [OPEN M6].
            d = sar16(shl16(d, 1), w.size & 15);
            dx = sar16(shl16(dx, 1), w.size & 15);
            dy = sar16(shl16(dy, 1), w.size & 15);
            if (d < 100) d = 100;  // [BYTES seg8:0x5d5e-0x5d64]
            const std::int16_t b = pools.rec(sp, sr).blast_size;  // +0x48 [BYTES seg8:0x5d77]
            const std::int16_t bc = b > 7 ? std::int16_t{7} : b;
            if (!(mul16(add16(bc, 8), 15) > d)) continue;  // [BYTES seg8:0x5d85-0x5d90]
            std::int16_t amount = idiv16(mul16(b, 600), d);  // [BYTES seg8:0x5d9c-0x5da4]
            std::int16_t m = mass_scale(25, pools.rec(p, r).mass);  // [BYTES seg8:0x5dc0]
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
            c.st.dg_b0c = 1;  // [BYTES seg8:0x5b88-0x5b99]
            if (e.frame != 0) c.st.dg_b0c = 0;
            c.host.hit(p, r, sp, sr, sub16(0, amount));  // seg8:0x5df2 [BYTES seg8:0x5bb2]
            c.st.dg_b0c = 1;                             // [BYTES seg8:0x5bb8]
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
        default: n = 2; break;  // the original's [bp-8] is unset [DESIGN]
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

}  // namespace is::world
