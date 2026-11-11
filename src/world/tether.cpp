// The tethers, GRAPPLING HOOK and HANDCUFFS (tether.h). M4 s7 TETHER.
#include "world/tether.h"

#include <cstddef>

#include "world/ambulance.h"
#include "world/collide.h"
#include "world/line.h"
#include "world/missile.h"
#include "world/shots.h"
#include "world/update.h"
#include "world/view.h"
#include "world/visibility.h"

namespace is::world {

namespace {

constexpr std::int32_t sext(std::int16_t v) { return v; }

// DGROUP:0x10d0 + 4f: the non-ship muzzle offsets (x, y), 16 pairs (Omission D) [BYTES
// DGROUP:0x10d0].
constexpr std::array<std::int16_t, 32> kHookMuzzle = {
    -14, -23, -7, -24, -3, -21, -3, -18, 0,   -14, -2,  -8,  -4,  -6,  -6,  -3,
    -13, 0,   -16, 0,  -21, -4, -22, -10, -22, -13, -23, -18, -24, -22, -18, -23};

bool rec_ok(const Pools& p, int pool, int rec) {
    return pool >= 0 && pool < kPoolCount && rec >= 0 && rec < p.records(pool);
}

// The slot k of the link table, or a scratch slot with a one-time log for a k the table does not
// hold (the original reads DGROUP:0x65cc + 0x20k past its three slots) [DESIGN ADR 0016 decision 2].
LinkSlot g_scratch_link{};
LinkSlot& link_at(World& w, int k) {
    if (k < 0 || k >= kLinkSlots) {
        static bool once = false;
        guard_todo(once, "a link slot outside the table reads a scratch slot", "DGROUP:0x65cc");
        g_scratch_link = LinkSlot{};
        return g_scratch_link;
    }
    return w.links[static_cast<std::size_t>(k)];
}

// The heading table DGROUP:0x1050 + 4f (flight.h kHeadingTable), 0 for an f outside 0 ... 15
// with a one-time log (the original reads past the table) [DESIGN ADR 0016 decision 2].
std::int16_t heading_word(int f, int xy) {
    if (f < 0 || f > 15) {
        static bool once = false;
        guard_todo(once, "a heading outside 0 ... 15 reads 0 from the heading table", "DGROUP:0x1050");
        return 0;
    }
    return xy == 0 ? heading_dx(f) : heading_dy(f);
}
std::int16_t hook_muzzle_word(int f, int xy) {
    if (f < 0 || f > 15) {
        static bool once = false;
        guard_todo(once, "a heading outside 0 ... 15 reads 0 from the muzzle table", "DGROUP:0x10d0");
        return 0;
    }
    return kHookMuzzle[static_cast<std::size_t>(2 * f + xy)];
}

// The window DC's lines: seg12:0x7256 between SelectObject(DGROUP:0x13dc, pen) and the old pen
// back.
void pen_line(TetherCtx& c, PenId pen, std::int16_t x0, std::int16_t y0, std::int16_t x1,
              std::int16_t y1) {
    Blitter* out = c.h.blitter();
    if (!out) return;
    const PenId old = out->select_pen(kDcWindow, pen);
    draw_clipped_line(*out, kDcWindow, c.play_w, c.play_h, x0, y0, x1, y1);
    out->select_pen(kDcWindow, old);
}
// The slot's points by offset: line[k] is +0x10 + 2k.
std::int16_t& a_x(LinkSlot& l) { return l.line[0]; }       // +0x10 end A's point of this pass
std::int16_t& a_y(LinkSlot& l) { return l.line[1]; }       // +0x12
std::int16_t& a_old_x(LinkSlot& l) { return l.line[2]; }   // +0x14 end A's point of the last line
std::int16_t& a_old_y(LinkSlot& l) { return l.line[3]; }   // +0x16
std::int16_t& b_x(LinkSlot& l) { return l.line[4]; }       // +0x18 end B's point of this pass
std::int16_t& b_y(LinkSlot& l) { return l.line[5]; }       // +0x1a
std::int16_t& b_old_x(LinkSlot& l) { return l.line[6]; }   // +0x1c end B's point of the last line
std::int16_t& b_old_y(LinkSlot& l) { return l.line[7]; }   // +0x1e
// The BLACK_PEN erase of the last line (+0x1c, +0x1e) -> (+0x14, +0x16).
void erase_last(TetherCtx& c, LinkSlot& l) {
    pen_line(c, kPenBlack, b_old_x(l), b_old_y(l), a_old_x(l), a_old_y(l));
}
// The roll: +0x14/+0x16 <- +0x10/+0x12, +0x1c/+0x1e <- +0x18/+0x1a.
void roll(LinkSlot& l) {
    a_old_x(l) = a_x(l);
    a_old_y(l) = a_y(l);
    b_old_x(l) = b_x(l);
    b_old_y(l) = b_y(l);
}
// PatBlt(DGROUP:0x13dc, scr_x, scr_y, size, size, BLACKNESS) of a hook record, the ROP pushed
// as high word 0, low word 0x42 [BYTES seg11:0x9d6e seg11:0x9d70 seg12:0x3554 seg12:0x3556].
void black_square(TetherCtx& c, const ObjectRecord& h) {
    if (Blitter* out = c.h.blitter()) out->pat_blt(kDcWindow, h.scr_x, h.scr_y, h.size, h.size, kBlackness);
}
ShipRecord& ship_of(TetherCtx& c, int i) { return ship_at(c.w, i); }

// The muzzle offset of a fire from O (P, R) on the hook record H whose frame is f
// [BYTES seg11:0x9648-0x96a0 seg11:0x98a1-0x991f seg11:0x9afb-0x9bd3] (claims 16, 17, Omission D).
void muzzle(TetherCtx& c, int P, int R, const ObjectRecord& O, ObjectRecord& H) {
    if (P == c.g.ship_pool) {
        // DGROUP:0x7b60 + 4·(64n + f), n = ship word +0x4e: (lo16(m·4) / 3) << 3, all 16-bit
        // (`shl ax, 2`, `cwd; idiv 3`, `shl ax, 3`) [BYTES seg11:0x966a-0x96a0 seg11:0x9b1c-0x9b53]
        const int n = ship_of(c, R).design_index;
        H.x = add16(H.x, shl16(idiv16(shl16(c.h.muzzle(n, H.frame, 0), 2), 3), 3));
        H.y = add16(H.y, shl16(idiv16(shl16(c.h.muzzle(n, H.frame, 1), 2), 3), 3));
        return;
    }
    // e = O.extent / 2 (`cwd; sub; sar 1`); lo16(e·DGROUP:0x1050[4f]) / 16 (`cwd; and dx, 0xf;
    // add; sar 4`) + (DGROUP:0x10d0[4f] << 3) [BYTES seg11:0x98af-0x98e8 seg11:0x98f3-0x991f]
    const std::int16_t e = idiv16(O.extent, 2);
    H.x = add16(H.x, add16(idiv16(mul16(e, heading_word(H.frame, 0)), 16),
                           shl16(hook_muzzle_word(H.frame, 0), 3)));
    H.y = add16(H.y, add16(idiv16(mul16(e, heading_word(H.frame, 1)), 16),
                           shl16(hook_muzzle_word(H.frame, 1), 3)));
}

// seg11:0x9c30-0x9d9c, the release (section 2.4, claims 22-24, Omission F).
void release(TetherCtx& c, int P, int R) {
    World& w = c.w;
    WaveGlobals& g = c.g;
    ObjectRecord& O = obj(w, P, R);
    // A held ship: lock-out now + 800 (`add`/`adc`), object +0x26 = 1, `tumbling` 1
    // [BYTES seg11:0x9c43-0x9c8c]
    if (O.w36 == g.ship_pool) {
        ShipRecord& held = ship_of(c, O.w38);
        held.lockout.set(g.now + 800u);
        obj(w, g.ship_pool, O.w38).angle = 1;
        held.be2 = 1;
    }
    obj(w, O.w36, O.w38).owner_pool = -1;  // no velocity store: a swung object flies on [BYTES seg11:0x9cb7]
    O.w36 = -1;                            // [BYTES seg11:0x9cc9]
    const int s = own_slot(O);             // signed byte +0x31 [BYTES seg11:0x9cd5]
    LinkSlot& l = link_at(w, s);
    l.pool_a = -1;                         // [BYTES seg11:0x9cdf]
    l.pool_b = -1;                         // [BYTES seg11:0x9cef]
    ObjectRecord& hk = obj(w, kPoolHooks, s);
    hk.state = 1;                          // [BYTES seg11:0x9d01]
    erase_last(c, l);                      // no roll [BYTES seg11:0x9d07-0x9d46]
    black_square(c, hk);                   // [BYTES seg11:0x9d4b-0x9d72]
    if (P == g.ship_pool && R == g.player && g.autopilot == 0) c.h.player_sprite();  // [BYTES seg11:0x9d77-0x9d8e]
    recount_in_use(w.pools, kPoolHooks);   // seg7:0x2080(10, -1) [BYTES seg11:0x9d93]
}

// seg11:0x96a7-0x989e, the search (claims 10-12, Omission C): (tp, tr) with tp = -1 for none.
void search(TetherCtx& c, int P, int R, std::int16_t& tp, std::int16_t& tr) {
    World& w = c.w;
    Pools& p = w.pools;
    const ObjectRecord& O = obj(w, P, R);
    tp = -1;                       // [BYTES seg11:0x96a7]
    std::int16_t best = 0;         // [BYTES seg11:0x96ac]
    const std::int16_t radius = 300;  // [BYTES seg11:0x96b1]
    for (int q = w.roles.radar_first; q <= c.g.ship_pool; ++q) {  // DGROUP:0x9326 ... DGROUP:0x9324 (`jg`)
        if (q == 5 || q == 0xc || q == 2) continue;                // [BYTES seg11:0x9887-0x9897]
        if (q < 0 || q >= kPoolCount) continue;
        for (int j = 0; j < p.in_use(q) && j < p.records(q); ++j) {  // DGROUP:0x9406 + 2q (`jle`)
            const ObjectRecord& C = p.rec(q, j);
            if (C.state != 0) continue;                  // [BYTES seg11:0x9771]
            if (q == P && j == R) continue;              // [BYTES seg11:0x9778-0x9783]
            if (C.b54 != 0 && q != 4) continue;          // [BYTES seg11:0x9792-0x979d]
            std::int16_t dx = 0, dy = 0;
            const std::int16_t d = distance_cells(C.x, C.y, O.x, O.y, dx, dy);  // seg8:0x285a [BYTES seg11:0x97db]
            if (!(d < radius)) continue;                 // `jl` [BYTES seg11:0x97e6]
            std::int16_t score = add16(C.mass, 400);     // [BYTES seg11:0x97f2-0x9801]
            if (score > 1200) score = 1200;
            const std::int16_t h = direction16(dy, dx);  // seg8:0x19b0(num dy, den dx) [BYTES seg11:0x9808]
            if (O.frame >= sub16(h, 1) && O.frame <= add16(h, 1)) score = shl16(score, 1);  // [BYTES seg11:0x9810-0x9828]
            if (P == 0xe) {  // the literal 14 [BYTES seg11:0x982b]
                const ShipRecord& S = ship_of(c, R);
                if (S.w32 == q && S.w34 == j) {             // the goal [BYTES seg11:0x9833-0x984d]
                    score = shl16(score, 1);
                    if (C.b53 != 0) score = mul16(score, 3);  // [BYTES seg11:0x9868-0x9877]
                } else if (q == 0xe && S.w32 == 0xe) {      // [BYTES seg11:0x96c5-0x96e0]
                    score = idiv16(score, 8);
                } else if (C.b53 == 1 && S.w38 != 1) {      // [BYTES seg11:0x96f9-0x970c]
                    score = shl16(score, 1);
                }
            }
            if (q == 6) score = mul16(score, 6);  // [BYTES seg11:0x970f-0x971b]
            // lo16(((2·300 - d)·score) / 600): the 16 x 16 `imul` kept in 32 bits, seg5:0x110
            // [BYTES seg11:0x971e-0x973a]
            score = lo16(rtl_ldiv(sext(sub16(add16(radius, radius), d)) * sext(score),
                                  add32(sext(radius), sext(radius))));
            if (score > best) {  // `jle` skips [BYTES seg11:0x973d-0x974e]
                tp = static_cast<std::int16_t>(q);
                tr = static_cast<std::int16_t>(j);
                best = score;
            }
        }
    }
}

// The ends of a fire, both paths [BYTES seg11:0x9a16-0x9a63 seg11:0x9bd7-0x9c2d] (claim 21).
void link_fire(TetherCtx& c, int P, int R, int k, ObjectRecord& O, ObjectRecord& H, LinkSlot& l) {
    H.owner_pool = static_cast<std::int16_t>(P);
    H.owner_rec = static_cast<std::int16_t>(R);
    l.w08 = 0;
    l.pool_a = kPoolHooks;
    l.rec_a = static_cast<std::int16_t>(k);
    l.pool_b = static_cast<std::int16_t>(P);
    l.rec_b = static_cast<std::int16_t>(R);
    O.w36 = kPoolHooks;
    O.w38 = static_cast<std::int16_t>(k);
    recount_in_use(c.w.pools, kPoolHooks);  // seg7:0x2080(10, -1) [BYTES seg11:0x9d93]
}

// seg11:0x9530-0x9c2d, the fire (sections 2.2, 2.3).
void fire(TetherCtx& c, int P, int R, int w) {
    World& wd = c.w;
    Pools& p = wd.pools;
    WaveGlobals& g = c.g;
    // The first slot k < DGROUP:0x9300 with record `state` != 0 and +0x00 = -1 [BYTES
    // seg11:0x9530-0x956d] (claim 7)
    int k = -1;
    for (int i = 0; i < p.allocated(kPoolHooks); ++i) {
        if (i >= p.records(kPoolHooks) || i >= kLinkSlots) {
            static bool once = false;
            guard_todo(once, "a pool-10 slot past the port's link table is skipped", "DGROUP:0x9300");
            break;
        }
        if (p.rec(kPoolHooks, i).state == 0) continue;
        if (wd.links[static_cast<std::size_t>(i)].pool_a != -1) continue;
        k = i;
        break;
    }
    if (k == -1) return;  // [BYTES seg11:0x9569 seg11:0x956d]
    LinkSlot& l = wd.links[static_cast<std::size_t>(k)];
    ObjectRecord& H = p.rec(kPoolHooks, k);
    ObjectRecord& O = obj(wd, P, R);
    l.weapon = static_cast<std::int16_t>(w);  // [BYTES seg11:0x957a]
    H.scr_y = -1000;                          // [BYTES seg11:0x9586]
    H.scr_x = H.scr_y;                        // [BYTES seg11:0x9598]
    c.h.sound(0x1d, P, R, 1);                 // SLING [BYTES seg11:0x959b-0x95a8]
    // The target [bp-0x10], [bp-0x12]
    std::int16_t tp = -1, tr = 0;
    bool arrest = false;
    if (P == g.ship_pool) {
        const ShipRecord& S = ship_of(c, R);
        // A team-7 (ENFORCERS) ship with HANDCUFFS takes its arrest target, ship words
        // +0x54/+0x56 [BYTES seg11:0x95ab-0x95d9] (claim 9; ruling R4). The store [bp-0x16] = 200
        // is never read [BYTES seg11:0x95dc].
        if (c.h.team(wd, S.design_index) == 7 && w == kWeaponCuffs) {
            tp = S.w54;
            tr = S.w56;
            arrest = true;
        }
    }
    if (!arrest) search(c, P, R, tp, tr);
    set_own_slot(O, k);  // byte +0x31 = k [BYTES seg11:0x95e1-0x95f2] (claim 14)
    if (tp == -1 || g.demons_gate != 0) {
        // The straight shot [BYTES seg11:0x9a6a-0x9af6] (claim 15)
        H.state = 0;
        H.b2e = 0;
        H.vx = add16(mul16(heading_word(O.frame, 0), 7), O.vx);  // [BYTES seg11:0x9a9a-0x9aa7]
        H.vy = add16(mul16(heading_word(O.frame, 1), 7), O.vy);  // [BYTES seg11:0x9aba-0x9ac7]
        H.frame = O.frame;                                       // [BYTES seg11:0x9adf]
        H.x = O.x;                                               // dword [BYTES seg11:0x9af6]
        H.y = O.y;
        muzzle(c, P, R, O, H);  // f = O.frame, just copied [BYTES seg11:0x9afb-0x9bd3]
        link_fire(c, P, R, k, O, H, l);
        return;
    }
    // The aimed shot [BYTES seg11:0x9606-0x9a67]
    H.x = O.x;  // [BYTES seg11:0x960e-0x9626]
    H.y = O.y;
    H.vx = O.vx;  // dword [BYTES seg11:0x9630-0x9639]
    H.vy = O.vy;
    muzzle(c, P, R, O, H);  // f = record k's own, stale frame (claim 16)
    // seg8:0x348e(10, k, tp, tr, S): the target's velocity plus the terms (verified claim 19 as
    // corrected); the x distance 0 stores nothing (ruling R1, missile.h aim_at)
    // [BYTES seg11:0x9923-0x994a]
    const std::int16_t speed = tp == g.ship_pool ? 160 : 110;
    c.h.aim(wd, kPoolHooks, k, tp, tr, speed, pixel_shift(g.size));
    // The fallback when the hook still moves with O (Omission E) [BYTES seg11:0x994d-0x99d1]
    if (H.vx == O.vx && H.vy == O.vy) {
        H.vx = add16(mul16(heading_word(O.frame, 0), 7), O.vx);
        H.vy = add16(mul16(heading_word(O.frame, 1), 7), O.vy);
    }
    H.frame = direction16(H.vy, H.vx);  // seg8:0x19b0(num vy, den vx) [BYTES seg11:0x99d5-0x99fb] (claim 20)
    H.state = 0;                        // [BYTES seg11:0x9a03]
    H.b2e = 0;                          // [BYTES seg11:0x9a0d]
    link_fire(c, P, R, k, O, H, l);
}

}  // namespace

// ---------------------------------------------------------------- the state and the pens

TetherState& tether_state() {
    static TetherState s;
    return s;
}

void make_rope_pens(Blitter& out, TetherState& st) {
    for (PenId& pen : st.pens) {  // DeleteObject of the old ones [BYTES seg10:0x4d87-0x4db0]
        if (pen != kPenNone) out.delete_pen(pen);
        pen = kPenNone;
    }
    for (std::size_t n = 0; n < st.pens.size(); ++n)  // [BYTES seg10:0x4db2-0x4df3]
        st.pens[n] = out.create_pen(kPsSolid, 1, kRopePenColours[n]);
}

bool load_hook_sheet(Blitter& out, ImageSlotTable& images, const Pools& pools, int size_level,
                     const SheetRenderer& render) {
    // DGROUP:0x9300 != 0 and DGROUP:0x39ee = 0 [BYTES seg10:0x5624-0x5631]
    if (pools.allocated(kPoolHooks) == 0) return false;
    std::uint16_t& slot = images[static_cast<std::size_t>(kHookImageSlot)];
    if (slot != 0) return false;
    // seg10:0x6ee6(3, 180, far DGROUP:0x76e "", 0, 0x63, 3, 0, 1, 0, 12, 12, 0, 0) (claim 64)
    // [BYTES seg10:0x5632-0x5650]
    SheetRequest req;
    req.slot = kHookImageSlot;
    req.graphic_id = 180;
    req.file.clear();
    req.w1 = 0;
    req.gap_word = kSheetGapAuto;  // 0x63
    req.recolour_a = 3;
    req.recolour_b = 0;
    req.w5 = 1;
    req.scale = sheet_scale_flag(size_level, 0, 0);
    req.colour_a = 12;
    req.colour_b = 12;
    req.mode = 0;
    req.sel = 0;
    req.size_level = size_level;
    SheetPixels px;
    if (render && render(req, px) && px.w > 0 && px.h > 0)
        slot = out.create_image_from(px.w, px.h, px.index);  // [BYTES seg10:0x6f7f seg10:0x6f89]
    return slot != 0;
}

// ---------------------------------------------------------------- the host's defaults

std::int16_t TetherHost::weapon_word(int w, int off) { return world::weapon_word(w, off); }
std::int16_t TetherHost::team(const World& w, int design) { return design_from_world(w, design).team; }
std::int16_t TetherHost::muzzle(int n, int f, int xy) {
    // DGROUP:0x7b60 + 4·(64n + f): an f past 63 reads the next ship's table, as the flat index
    // does; outside the eight tables the port reads 0 (shots.h) [DESIGN ADR 0016 decision 2].
    const int flat = 64 * n + f;
    if (flat < 0 || flat >= kShipCount * 64) {
        static bool once = false;
        guard_todo(once, "a muzzle entry outside the tables reads 0", "DGROUP:0x7b60");
        return 0;
    }
    ShotState& s = shot_state();
    return xy == 0 ? s.mx(flat / 64, flat % 64) : s.my(flat / 64, flat % 64);
}
bool TetherHost::aim(World& w, int P, int R, int TP, int TR, std::int16_t S, int shift) {
    const AimToScreen ts = [this](std::int16_t x, std::int16_t y) { return to_screen(x, y); };
    return aim_at(w, ts, shift, P, R, TP, TR, S);
}

// ---------------------------------------------------------------- seg11:0x947a

void tether_use(TetherCtx& c, int P, int R, int w) {
    World& wd = c.w;
    WaveGlobals& g = c.g;
    if (!rec_ok(wd.pools, P, R)) {
        static bool once = false;
        guard_todo(once, "a tether user outside its pool does nothing", "seg11:0x947a");
        return;
    }
    ObjectRecord& O = wd.pools.rec(P, R);
    if (O.on_screen == 0) return;  // [BYTES seg11:0x9497 seg11:0x949c] (claim 2)
    int ok = 0;                    // [bp-0x34]
    if (P == g.ship_pool) {
        // Slots 1 ... signed byte +0xcb - 1: weapon 2 or 15 [BYTES seg11:0x94ad-0x94d9] (claim 3)
        const ShipRecord& S = ship_of(c, R);
        const int n = sbyte_at(S, 0xcb);
        for (int l = 1; l < n; ++l) {
            const std::uint8_t b = static_cast<std::uint8_t>(sbyte_at(S, 0xb8 + l));
            if (b == kWeaponHook || b == kWeaponCuffs) {
                ok = 1;
                break;
            }
        }
        // The cost whatever ok is: W(w)+0x2e > `fuel` refuses, else it is paid
        // [BYTES seg11:0x94e6-0x950d] (claims 4, 5)
        ShipRecord& Sw = ship_of(c, R);
        const std::int16_t cost = c.h.weapon_word(w, 0x2e);
        if (cost > Sw.fuel)
            ok = 0;
        else
            Sw.fuel = sub16(Sw.fuel, cost);
    } else {
        ok = 1;  // [BYTES seg11:0x94ab seg11:0x94d9]
    }
    if (ok == 0) return;  // [BYTES seg11:0x9511 seg11:0x9515]
    if (O.w36 != -1) {    // [BYTES seg11:0x9527 seg11:0x952c] (claim 6)
        release(c, P, R);
        return;
    }
    fire(c, P, R, w);
}

// ---------------------------------------------------------------- seg12:0x3498

int hook_meets(TetherCtx& c, int Tp, int Tr, int k) {
    World& w = c.w;
    LinkSlot& l = link_at(w, k);
    // Its own holder or another hook [BYTES seg12:0x34a7-0x34c7] (claim 31)
    if ((l.pool_b == Tp && l.rec_b == Tr) || Tp == kPoolHooks) return 0;
    erase_last(c, l);  // [BYTES seg12:0x34ca-0x34fb]
    roll(l);           // [BYTES seg12:0x3501-0x351d]
    ObjectRecord& hk = obj(w, kPoolHooks, k);
    hk.state = 1;         // [BYTES seg12:0x3535]
    black_square(c, hk);  // [BYTES seg12:0x353b-0x3558] (claim 32)
    // A ship holder's hook meets a ship: only its goal (ship words +0x32/+0x34), both literal 14
    // [BYTES seg12:0x355d-0x3580] (claim 33)
    if (l.pool_b == 0xe && Tp == 0xe) {
        const ShipRecord& S = ship_of(c, l.rec_b);
        if (!(S.w32 == 0xe && S.w34 == Tr)) Tp = -1;
    }
    // A missile's age / 4 (`cwd; idiv 4`) [BYTES seg12:0x3585-0x3599] (claim 34)
    if (Tp == 6) {
        MissileSlot& m = w.pools.missile(Tr);
        m.age = idiv16(m.age, 4);
    }
    bool miss = Tp == -1;  // [BYTES seg12:0x359d-0x35a3]
    if (!miss) {
        const ObjectRecord& T = obj(w, Tp, Tr);
        if (T.b54 != 0 && Tp != 4) miss = true;  // [BYTES seg12:0x35b4-0x35c1]
    }
    if (miss) {
        // [BYTES seg12:0x3728-0x375d] (claim 35)
        obj(w, l.pool_b, l.rec_b).w36 = -1;
        l.pool_a = -1;
        l.pool_b = -1;
        recount_in_use(w.pools, kPoolHooks);
        l.w08 = 0;  // [BYTES seg12:0x3769]
        return 1;
    }
    c.h.sound(6, kPoolHooks, k, 1);  // CLUNK [BYTES seg12:0x35c4-0x35cd]
    ObjectRecord& T = obj(w, Tp, Tr);
    T.b54 = 0;  // [BYTES seg12:0x35e3]
    if (T.owner_pool != -1) {
        // The steal: T's old tether s' freed [BYTES seg12:0x35f8-0x36a4] (claim 37)
        const int s2 = held_slot(T);
        obj(w, kPoolHooks, s2).state = 1;
        T.owner_pool = -1;
        LinkSlot& l2 = link_at(w, s2);
        obj(w, l2.pool_b, l2.rec_b).w36 = -1;
        l2.pool_a = -1;
        l2.pool_b = -1;
        l2.w08 = 0;
        erase_last(c, l2);
        roll(l2);
    }
    // The attach [BYTES seg12:0x36a9-0x3722] (claim 36)
    T.owner_pool = l.pool_b;
    T.owner_rec = l.rec_b;
    set_held_slot(T, k);  // byte +0x30 = k [BYTES seg12:0x36ec]
    ObjectRecord& holder = obj(w, l.pool_b, l.rec_b);
    holder.w36 = static_cast<std::int16_t>(Tp);
    holder.w38 = static_cast<std::int16_t>(Tr);
    l.pool_a = static_cast<std::int16_t>(Tp);
    l.rec_a = static_cast<std::int16_t>(Tr);
    l.w08 = 0;  // no seg7:0x2080 [BYTES seg12:0x3760-0x3769]
    return 1;
}

// ---------------------------------------------------------------- seg8:0x1e32

void tether_follow(TetherCtx& c, int pool, int rec) {
    World& w = c.w;
    TetherState& st = c.st;
    // The pen phase [BYTES seg8:0x1e41-0x1e4c] (claim 52)
    st.pen_phase = add16(st.pen_phase, 1);
    if (st.pen_phase > 3) st.pen_phase = 0;
    if (!rec_ok(w.pools, pool, rec)) return;
    ObjectRecord& T = w.pools.rec(pool, rec);
    const int s = held_slot(T);  // [BYTES seg8:0x1e60]
    if (s < 0 || s >= kLinkSlots) {
        static bool once = false;
        guard_todo(once, "a held object whose slot lies outside the link table is not followed",
                   "seg8:0x1e32");
        return;
    }
    LinkSlot& l = w.links[static_cast<std::size_t>(s)];
    const bool hook = l.weapon == kWeaponHook;  // [bp-0x2e] [BYTES seg8:0x1e6d-0x1e7b]
    T.x = sub16(T.x, T.vx);  // [BYTES seg8:0x1e90-0x1ea2] (claim 39)
    T.y = sub16(T.y, T.vy);
    const int hp = T.owner_pool, hr = T.owner_rec;  // [bp-0x14], [bp-0x16]
    ObjectRecord& H = obj(w, hp, hr);
    // [bp-0x30] = 0 and [bp-0x2c] = 300, each written once (verified claim 49 as corrected): the
    // mass / 3 steps, the r = -50 store, the strike seg11:0xa234 and the clamp seg8:0x1cbe(T,
    // 0x50) never run [BYTES seg8:0x1eba seg8:0x1ec2 seg8:0x222e-0x22ad seg8:0x2396-0x23ae].
    const std::int16_t rope = 300;
    // The holder's centre [BYTES seg8:0x1ed8-0x1ef5] (claim 40)
    const std::int16_t hcx = add16(idiv16(H.extent, 2), H.x);
    const std::int16_t hcy = add16(idiv16(H.extent, 2), H.y);
    // The raw screen points [BYTES seg8:0x1f08-0x1f4f]
    const Point16 tp0 = c.h.to_screen(T.x, T.y);
    a_x(l) = tp0.x;
    a_y(l) = tp0.y;
    const Point16 hp0 = c.h.to_screen(H.x, H.y);
    b_x(l) = hp0.x;
    b_y(l) = hp0.y;
    std::int16_t o1 = 0, o2 = 0;  // [bp-0x28], [bp-0x2a]: hc - T in cells
    std::int16_t d = distance_cells(hcx, hcy, T.x, T.y, o1, o2);  // [BYTES seg8:0x1f54-0x1f72]
    if (d == 0) d = 1;                                            // [BYTES seg8:0x1f79]
    // The holding cost, the literal pool 14 [BYTES seg8:0x1f7e-0x1fd5] (claim 41)
    if (hp == 0xe) {
        std::int16_t q = add16(idiv16(T.mass, -200), 8);
        if (q < 3) q = 3;
        if (c.g.game_frames % static_cast<std::uint32_t>(static_cast<std::int32_t>(q)) == 0) {  // `div ecx`
            ShipRecord& S = ship_of(c, hr);
            S.fuel = sub16(S.fuel, 3);
            if (S.fuel <= 0) d = 1000;
        }
    }
    // The break [BYTES seg8:0x1fda-0x2019] (claim 42, Omission J)
    if (add16(mul16(hook ? 1 : 0, 100), 350) < d || H.state != 0 || pool == 7) {
        T.owner_pool = -1;                    // [BYTES seg8:0x202b]
        obj(w, l.pool_b, l.rec_b).w36 = -1;   // [BYTES seg8:0x2052]
        l.pool_a = -1;                        // [BYTES seg8:0x2056]
        l.pool_b = -1;                        // [BYTES seg8:0x205a]
        if (Blitter* out = c.h.blitter()) {   // both lines under one BLACK_PEN [BYTES seg8:0x205e-0x20a5]
            const PenId old = out->select_pen(kDcWindow, kPenBlack);
            draw_clipped_line(*out, kDcWindow, c.play_w, c.play_h, b_old_x(l), b_old_y(l), a_old_x(l), a_old_y(l));
            draw_clipped_line(*out, kDcWindow, c.play_w, c.play_h, b_x(l), b_y(l), a_x(l), a_y(l));
            out->select_pen(kDcWindow, old);
        }
        recount_in_use(w.pools, kPoolHooks);  // seg7:0x2080(10, -1) [BYTES seg8:0x20aa]
        c.h.command(0x69);                    // seg10:0x744c(0x69) [BYTES seg8:0x20b6]
        c.h.sound(0x14, pool, rec, 1);        // STATIC at T [BYTES seg8:0x20be-0x20c8] (claim 43)
    }
    if (d > 40) {  // [BYTES seg8:0x20d0]
        if (hook) {
            if (T.b53 == 1) {
                // The pull of a capturable object [BYTES seg8:0x23c4-0x23e1] (claim 44)
                T.vx = add16(T.vx, sar16(o1, 2));
                T.vy = add16(T.vy, sar16(o2, 2));
            } else {
                // The swing: the velocity set to (+-180·o2 / d + 8·o1 / d, -+180·o1 / d + 8·o2 / d),
                // the upper signs for +0x0e != 0; each 180·o a 16 x 16 `imul` through seg5:0x110,
                // each o << 3 a 16-bit `shl` and `idiv` [BYTES seg8:0x23e8-0x24c8] (claim 45)
                const std::int16_t k1 = l.swing != 0 ? 180 : -180;
                T.vx = add16(lo16(rtl_ldiv(sext(k1) * sext(o2), sext(d))), idiv16(shl16(o1, 3), d));
                T.vy = add16(lo16(rtl_ldiv(sext(static_cast<std::int16_t>(-k1)) * sext(o1), sext(d))),
                             idiv16(shl16(o2, 3), d));
            }
        } else if (rope < d) {  // `jge` skips [BYTES seg8:0x20e0-0x20e6] (claim 46)
            // HANDCUFFS [BYTES seg8:0x20ea-0x2393] (claims 47, 48, Omission K)
            const std::int16_t dvx = sub16(T.vx, H.vx), dvy = sub16(T.vy, H.vy);
            const std::int32_t n2 = sext(add16(mul16(dvx, dvx), mul16(dvy, dvy)));
            const std::int32_t g1 = sext(add16(abs16(dvx), abs16(dvy)));
            std::int16_t v = lo16(newton_root(n2, g1));  // seg8:0x1290 [BYTES seg8:0x2157]
            const std::int16_t ex = sub16(b_x(l), a_x(l)), ey = sub16(b_y(l), a_y(l));  // [BYTES seg8:0x2163-0x2176]
            const std::int32_t dot = add32(sext(ex) * sext(dvx), sext(ey) * sext(dvy));  // [BYTES seg8:0x2179-0x2198]
            if (v == 0) v = 1;  // [BYTES seg8:0x219b-0x21a1]
            const std::int16_t c1 = lo16(rtl_ldiv(rtl_lmul(dot, -100), rtl_lmul(sext(v), sext(d))));  // [BYTES seg8:0x21a6-0x21d9]
            const std::int16_t r = lo16(rtl_ldiv(rtl_lmul(sext(c1), sext(v)), 100));  // [BYTES seg8:0x21dc-0x21ed]
            std::int16_t x = sub16(d, rope);  // min(d - 300, 60)·50 [BYTES seg8:0x21f0-0x2201]
            if (x > 60) x = 60;
            x = mul16(x, 50);
            const std::int16_t mt = T.mass, mh = H.mass;  // [BYTES seg8:0x2204-0x222b]
            std::int16_t at = 0, ah = 0;
            if (dot < 0) {  // the high word's sign [BYTES seg8:0x22b0-0x22b4]
                // aT = lo16((100·r) / ((100·mT) / mH + 100)), aH = r - aT [BYTES seg8:0x22b6-0x22ea].
                // mH = 0 (the 32-bit `idiv`) or a zero divisor of seg5:0x110 trap in the original;
                // the port leaves aT = aH = 0 and logs once [DESIGN ADR 0016 decision 4] (ruling R1's rule).
                const std::int32_t den = mh == 0 ? 0 : add32(rtl_ldiv(sext(mt) * 100, sext(mh)), 100);
                if (den == 0) {
                    static bool once = false;
                    guard_todo(once, "the cuffs' impulse divides by zero: no impulse", "seg8:0x22ca");
                } else {
                    at = lo16(rtl_ldiv(sext(r) * 100, den));
                    ah = sub16(0, sub16(at, r));
                }
            }
            // aT += X / mT, then T's velocity; aH += X / mH, then H's (16-bit `idiv`; mass 0 traps
            // in the original: the port skips that add, ruling R1's rule) [BYTES seg8:0x22f7-0x2386]
            if (mt != 0)
                at = add16(at, idiv16(x, mt));
            else {
                static bool once = false;
                guard_todo(once, "the cuffs divide by a zero mass: no spring term", "seg8:0x22fb");
            }
            T.vx = add16(T.vx, idiv16(mul16(add16(idiv16(st.impulse_t, 3), at), ex), d));
            T.vy = add16(T.vy, idiv16(mul16(add16(idiv16(st.impulse_t, 3), at), ey), d));
            if (mh != 0)
                ah = add16(ah, idiv16(x, mh));
            else {
                static bool once = false;
                guard_todo(once, "the cuffs divide by a zero mass: no spring term", "seg8:0x2346");
            }
            H.vx = sub16(H.vx, idiv16(mul16(add16(idiv16(st.impulse_h, 3), ah), ex), d));
            H.vy = sub16(H.vy, idiv16(mul16(add16(idiv16(st.impulse_h, 3), ah), ey), d));
            st.impulse_t = at;  // [BYTES seg8:0x238d]
            st.impulse_h = ah;  // [BYTES seg8:0x2393]
        }
    }
    T.x = add16(T.x, T.vx);  // [BYTES seg8:0x24cc-0x24ee]
    T.y = add16(T.y, T.vy);
    c.h.player_sprite();     // seg7:0x1208, no DGROUP:0x951a test [BYTES seg8:0x24f6] (claim 54)
    erase_last(c, l);        // [BYTES seg8:0x24fb-0x2535]
    if (T.owner_pool != -1) {
        // The new line: T's point + size / 2 + (v sar s), the holder's + size / 2, WHITE_PEN
        // under `Bright`, else pen DGROUP:0x3b44 + 2·DGROUP:0x915a [BYTES seg8:0x2547-0x262e]
        // (claim 51)
        // byte DGROUP:0x9150 = 4 - `Size` (`sar dx, cl`; a 286 masks cl to 5 bits, and 16 or
        // more leaves the sign) [BYTES seg8:0x2574-0x2578]
        const int sh = pixel_shift(c.g.size) & 0x1f;
        auto sar_cl = [sh](std::int16_t v) { return sh >= 16 ? sar16(v, 15) : sar16(v, sh); };
        const Point16 tp1 = c.h.to_screen(T.x, T.y);
        a_x(l) = add16(tp1.x, add16(sar_cl(T.vx), idiv16(T.size, 2)));
        a_y(l) = add16(tp1.y, add16(sar_cl(T.vy), idiv16(T.size, 2)));
        ObjectRecord& H2 = obj(w, hp, hr);
        const Point16 hp1 = c.h.to_screen(H2.x, H2.y);
        b_x(l) = add16(hp1.x, idiv16(H2.size, 2));
        b_y(l) = add16(hp1.y, idiv16(H2.size, 2));
        const int phase = st.pen_phase >= 0 && st.pen_phase < 4 ? st.pen_phase : 0;
        const PenId pen = w.bright != 0 ? kPenWhite : st.pens[static_cast<std::size_t>(phase)];
        pen_line(c, pen, b_x(l), b_y(l), a_x(l), a_y(l));
    }
    roll(l);  // broken or not [BYTES seg8:0x2633-0x2648]
}

// ---------------------------------------------------------------- seg12:0x3148

namespace {

// The frame's calls on the UpdateHost: the sound engine, the window Blitter, seg7:0x1208, the
// view's seg8:0x29ea and the weapon words.
class FrameTetherHost final : public TetherHost {
public:
    explicit FrameTetherHost(UpdateCtx& c) : c_(c) {}
    void sound(int id, int pool, int rec, int mode) override { c_.host.sound(id, pool, rec, mode); }
    void command(int id) override { c_.host.play(id); }
    Point16 to_screen(std::int16_t x, std::int16_t y) override {
        const WaveGlobals& g = c_.flight.g;
        const ScreenPoint p = world_to_screen(c_.view, x, y, g.wave_kind, g.dg_9eb5, g.dg_9130);
        return Point16{p.x, p.y};
    }
    Blitter* blitter() override { return c_.host.blitter(); }
    void player_sprite() override { c_.host.player_sprite(); }
    std::int16_t weapon_word(int w, int off) override { return c_.host.weapon_word(w, off); }

private:
    UpdateCtx& c_;
};

}  // namespace

void move_hooks(UpdateCtx& c, World& w) {
    MoverScope scope(c.host, c);
    FrameTetherHost fh(c);
    TetherCtx t{w, c.flight.g, fh, c.view.play_w, c.view.play_h, tether_state()};
    Pools& p = w.pools;
    WaveGlobals& g = c.flight.g;
    // k < DGROUP:0x941a, read on every pass (a miss recounts it) [BYTES seg12:0x3379-0x337f]
    for (int k = 0; k < p.in_use(kPoolHooks); ++k) {
        if (k >= p.records(kPoolHooks) || k >= kLinkSlots) {
            static bool once = false;
            guard_todo(once, "a pool-10 record past the port's link table is not moved", "seg12:0x3148");
            break;
        }
        ObjectRecord& H = p.rec(kPoolHooks, k);
        if (H.state != 0) continue;  // [BYTES seg12:0x3392]
        LinkSlot& l = w.links[static_cast<std::size_t>(k)];
        H.w1c = 0;  // `collided` [BYTES seg12:0x3399]
        // The heading of the last pass's line, holder to hook (Omission I: stale at the first
        // pass) [BYTES seg12:0x33a5-0x33c4] (claim 25)
        H.frame = direction16(sub16(a_y(l), b_y(l)), sub16(a_x(l), b_x(l)));
        const std::int16_t pr = pool_rec(kPoolHooks, k);
        integrate(c.flight, 1, pr);  // seg8:0x1b54(0x0a00 | k, 1) [BYTES seg12:0x33c8-0x33d5]
        H.src_x = add16(mul16(add16(H.size, H.gap), H.frame), H.gap);  // [BYTES seg12:0x33e6-0x33f6]
        c.host.draw_sprite(pr, 3, kHookImageSlot, 0);  // seg8:0xb76(pr, 3, DGROUP:0x39ee, 0) [BYTES seg12:0x33fa-0x3407] (claim 26)
        // + 50 when far from the holder: (H << 3) / 2 < d [BYTES seg12:0x340c-0x345c] (claim 29)
        const ObjectRecord& B = obj(w, l.pool_b, l.rec_b);
        std::int16_t dx = 0, dy = 0;
        const std::int16_t d = distance_cells(B.x, B.y, H.x, H.y, dx, dy);
        if (idiv16(shl16(c.view.play_h, 3), 2) < d) l.w08 = add16(l.w08, 50);
        // The points: scr_x/scr_y on screen, else seg8:0x29ea, + size / 2 [BYTES
        // seg12:0x3461-0x348b seg12:0x315f-0x3220] (claim 28)
        const Point16 hpt = H.on_screen != 0 ? Point16{H.scr_x, H.scr_y} : fh.to_screen(H.x, H.y);
        const Point16 bpt = B.on_screen == 0 ? fh.to_screen(B.x, B.y) : Point16{B.scr_x, B.scr_y};
        a_x(l) = add16(idiv16(H.size, 2), hpt.x);
        a_y(l) = add16(idiv16(H.size, 2), hpt.y);
        b_x(l) = add16(idiv16(B.size, 2), bpt.x);
        b_y(l) = add16(idiv16(B.size, 2), bpt.y);
        erase_last(t, l);  // [BYTES seg12:0x3224-0x325c]
        // The pen: rand() % 128 twice, RGB(r2, 0, 128 + r1) (the blue byte `add al, cl`)
        // [BYTES seg12:0x3261-0x3289] (claim 27)
        const int r1 = c.flight.rng.rand() % 128;
        const int r2 = c.flight.rng.rand() % 128;
        const std::uint32_t colour = (static_cast<std::uint32_t>((r1 + 0x80) & 0xff) << 16) |
                                     static_cast<std::uint32_t>(r2 & 0xff);
        if (Blitter* out = c.host.blitter()) {
            const PenId pen = out->create_pen(kPsSolid, 1, colour);  // GDI.61 CreatePen
            const PenId old = out->select_pen(kDcWindow, pen);
            draw_clipped_line(*out, kDcWindow, c.view.play_w, c.view.play_h, b_x(l), b_y(l), a_x(l), a_y(l));
            out->select_pen(kDcWindow, old);
            out->delete_pen(pen);  // GDI.69 DeleteObject [BYTES seg12:0x32c8]
        }
        roll(l);                     // [BYTES seg12:0x32cd-0x32e9]
        l.w08 = add16(l.w08, 1);     // [BYTES seg12:0x32ed]
        // The contact: seg8:0x50aa straight to seg12:0x3498, no response, no dispatch
        // [BYTES seg12:0x32f1-0x3334] (claim 30)
        if (H.on_screen != 0 && H.w1c == 0) {
            c.f.dg_943a = kPoolHooks;
            c.f.dg_943c = static_cast<std::int16_t>(k);
            const std::uint16_t hit = c.host.collide(pr);
            if (hit != 0xffffu) hook_meets(t, hit >> 8, hit & 0xff, k);
        }
        if (l.w08 > 0x29) hook_meets(t, -1, -1, k);  // the miss [BYTES seg12:0x333d-0x334f]
        if (l.pool_b == g.ship_pool && l.rec_b == g.player && g.autopilot == 0)
            c.host.player_sprite();  // seg7:0x1208 [BYTES seg12:0x3352-0x3371]
    }
}

// ---------------------------------------------------------------- seg13:0x6e5-0xa57

namespace {

// The throw test of the held object G against C (seg13:0x7ce-0x909, seg13:0x911-0xa44): true
// when 120 < d < 500 cells and (t << 7) / |G.v| > 125, t = -((G.vy·dy + G.vx·dx) / d) (32-bit).
bool heads_at(const ObjectRecord& G, const ObjectRecord& C) {
    std::int16_t dx = 0, dy = 0;  // [bp-0x40], [bp-0x42]: G - C in cells
    const std::int32_t d = sext(distance_cells(G.x, G.y, C.x, C.y, dx, dy));  // `cdq` [BYTES seg13:0x82d-0x836]
    if (!(d > 120 && d < 500)) return false;  // [BYTES seg13:0x839-0x853]
    const std::int32_t vx = sext(G.vx), vy = sext(G.vy);
    const std::int32_t sp = sext(magnitude(vx, vy));  // seg8:0x35d0, `cdq` [BYTES seg13:0x891-0x89d]
    if (sp == 0) return false;                       // [BYTES seg13:0x8a0-0x8a5]
    // seg5:0x314 twice, the sum, seg5:0x110 by d, negated [BYTES seg13:0x8a7-0x8e6]
    const std::int32_t t = neg32(rtl_ldiv(add32(rtl_lmul(vy, sext(dy)), rtl_lmul(vx, sext(dx))), d));
    // (t << 7) / |v|, 32-bit `idiv` [BYTES seg13:0x8e9-0x8ff]
    const std::int32_t cs = rtl_ldiv(rtl_lshl(t, 7), sp);
    return cs > 125;  // [BYTES seg13:0x903-0x909]
}

}  // namespace

void tether_throw(TetherCtx& c, const VisibilityGlobals* vis, int i) {
    World& w = c.w;
    WaveGlobals& g = c.g;
    if (!(g.detail > 1)) return;  // [BYTES seg13:0x6e5-0x6ea]
    if (!rec_ok(w.pools, g.ship_pool, i)) return;
    const ObjectRecord& O = w.pools.rec(g.ship_pool, i);
    const ShipRecord& S = ship_of(c, i);
    if (O.on_screen == 0 || S.w38 != 1 || !(O.w36 > -1)) return;  // [BYTES seg13:0x6ff-0x717]
    if (i == g.player && g.play_mode == 0) return;                 // [BYTES seg13:0x71c-0x729]
    if (O.w36 == kPoolHooks) return;                                // [BYTES seg13:0x73c-0x741]
    if (link_at(w, own_slot(O)).weapon != kWeaponHook) return;      // [BYTES seg13:0x743-0x752]
    const ObjectRecord& G = obj(w, O.w36, O.w38);
    bool go = false;
    if (O.w36 == S.w32 && O.w38 == S.w34) {
        // G is the goal: the first visible heavy object it heads at [BYTES seg13:0x773-0x90e]
        const int n = vis ? vis->count : 0;
        for (int j = 0; j < n && j < kVisibleMax; ++j) {
            const int cp = static_cast<std::int8_t>(vis->pool[static_cast<std::size_t>(j)]);
            const int cr = static_cast<std::int8_t>(vis->rec[static_cast<std::size_t>(j)]);
            const ObjectRecord& C = obj(w, cp, cr);
            if (!(C.mass > 500)) continue;                // [BYTES seg13:0x7a9-0x7af]
            if (O.w36 == cp && O.w38 == cr) continue;     // [BYTES seg13:0x7c2-0x7cc]
            if (heads_at(G, C)) {
                go = true;
                break;
            }
        }
    } else {
        // Otherwise the goal object alone [BYTES seg13:0x911-0xa44]
        go = heads_at(G, obj(w, S.w32, S.w34));
    }
    if (go) tether_use(c, g.ship_pool, i, kWeaponHook);  // seg11:0x947a(DGROUP:0x9324, i, 2) [BYTES seg13:0xa46-0xa4f]
}

// ---------------------------------------------------------------- seg12:0x3778's hooks

void TetherLinkHooks::line(DcId dc, std::int16_t x0, std::int16_t y0, std::int16_t x1, std::int16_t y1) {
    if (!out) return;
    const PenId old = out->select_pen(dc, kPenBlack);  // [BYTES seg12:0x38a6-0x38b3]
    draw_clipped_line(*out, dc, play_w, play_h, x0, y0, x1, y1);
    out->select_pen(dc, old);
}
void TetherLinkHooks::seg7_2080(int pool, int rec) {
    (void)rec;
    if (pools) recount_in_use(*pools, pool);
}

// ---------------------------------------------------------------- the program's links

TetherLinks& tether_links() {
    static TetherLinks l;
    return l;
}

namespace {

class LinkedTetherHost final : public TetherHost {
public:
    void sound(int id, int pool, int rec, int mode) override {
        if (tether_links().sound) tether_links().sound(id, pool, rec, mode);
    }
    void command(int id) override {
        if (tether_links().command) tether_links().command(id);
    }
    Point16 to_screen(std::int16_t x, std::int16_t y) override {
        if (tether_links().to_screen) return tether_links().to_screen(x, y);
        return TetherHost::to_screen(x, y);
    }
    Blitter* blitter() override { return tether_links().blitter ? tether_links().blitter() : nullptr; }
    void player_sprite() override {
        if (tether_links().player_sprite) tether_links().player_sprite();
    }
    std::int16_t weapon_word(int w, int off) override {
        if (tether_links().weapon_word) return tether_links().weapon_word(w, off);
        return TetherHost::weapon_word(w, off);
    }
};

// The context on the program's links, or false with one log.
template <typename F>
bool with_program(const char* address, F f) {
    TetherLinks& l = tether_links();
    World* w = l.world ? l.world() : nullptr;
    WaveGlobals* g = l.globals ? l.globals() : nullptr;
    if (!w || !g) {
        static bool once = false;
        guard_todo(once, "a tether routine without the program's world does nothing", address);
        return false;
    }
    std::int16_t pw = 0, ph = 0;
    if (l.play_size) {
        const auto s = l.play_size();
        pw = s.first;
        ph = s.second;
    }
    TetherCtx c{*w, *g, linked_tether_host(), pw, ph, tether_state()};
    f(c);
    return true;
}

}  // namespace

TetherHost& linked_tether_host() {
    static LinkedTetherHost h;
    return h;
}

void tether_use_program(int P, int R, int w) {
    with_program("seg11:0x947a", [&](TetherCtx& c) { tether_use(c, P, R, w); });
}
int hook_meets_program(int Tp, int Tr, int k) {
    int r = 0;
    with_program("seg12:0x3498", [&](TetherCtx& c) { r = hook_meets(c, Tp, Tr, k); });
    return r;
}
void tether_follow_program(int pool, int rec) {
    with_program("seg8:0x1e32", [&](TetherCtx& c) { tether_follow(c, pool, rec); });
}
void tether_throw_program(int ship) {
    with_program("seg13:0x6e5", [&](TetherCtx& c) {
        const VisibilityGlobals* vis = tether_links().visible ? tether_links().visible() : last_visibility();
        tether_throw(c, vis, ship);
    });
}

LinkHooks* program_link_hooks() {
    static TetherLinkHooks h;
    TetherLinks& l = tether_links();
    World* w = l.world ? l.world() : nullptr;
    h.out = l.blitter ? l.blitter() : nullptr;
    h.pools = w ? &w->pools : nullptr;
    if (l.play_size) {
        const auto s = l.play_size();
        h.play_w = s.first;
        h.play_h = s.second;
    }
    return &h;
}

}  // namespace is::world
