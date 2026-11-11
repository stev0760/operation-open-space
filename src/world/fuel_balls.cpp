// The pool-5 balls (fuel_balls.h). Ported from the listings (isre asm, INSPACE.EXE seg7, seg12,
// seg13) with work/analysis/m4/s7-fuel.md and its verification s7-fuel-verify.md. M4 stage 7,
// track FUEL.
#include "world/fuel_balls.h"

#include <cstddef>

#include "world/effects.h"
#include "world/icon_effects.h"
#include "world/missile.h"
#include "world/rock_images.h"
#include "world/sprite.h"
#include "world/view.h"

namespace is::world {

namespace {

constexpr std::size_t ix(int k) { return static_cast<std::size_t>(k); }

// The heading table's entry for f; a heading outside 0 ... 15 reads past DGROUP:0x1050 in the
// original, the port wraps it, as missile.cpp does [DESIGN].
std::int16_t hx(int f) { return heading_dx(f & 15); }
std::int16_t hy(int f) { return heading_dy(f & 15); }

// `shl r, cl` with cl the byte DGROUP:0x9156 or DGROUP:0x9150: the count is taken mod 32.
int shift_of(std::int16_t v) { return v & 0x1f; }

std::function<CollideGlobals*()>& tables_fn() {
    static std::function<CollideGlobals*()> f;
    return f;
}

bool g_once_sound = false;

}  // namespace

void FuelHost::sound(int id, int pool, int rec, int mode) {
    (void)id, (void)pool, (void)rec, (void)mode;
    guard_todo(g_once_sound, "a pool-5 ball's sound without a host", "seg10:0x754a");
}

// ---------------------------------------------------------------- the tables

Pool5Entry& pool5_entry(CollideGlobals& c, int r) {
    if (r >= 0 && r < kPool5Table && r < static_cast<int>(c.pool5.size())) return c.pool5[ix(r)];
    static Pool5Entry scratch;
    static bool once = false;
    guard_todo(once, "a pool-5 table index outside its 11 entries", "DGROUP:0x5a4c");  // [DESIGN ADR 0016 decision 2]
    return scratch = Pool5Entry{};
}

void pool5_grow(Pools& p, CollideGlobals& c, int pool, int r) {
    // DGROUP:0x92ec + 2·pool = r + 1, GlobalUnlock, GlobalReAlloc(max(r + 1, 1)·0x70, 0x42),
    // GlobalLock [BYTES seg13:0x13c1-0x13fc seg7:0x575b-0x57af seg12:0x2e27-0x2e7b] (O3)
    p.grow_to(pool, lo16(r));
    ObjectRecord& o = p.rec(pool, r);
    o.mass = kMassFireball;              // [BYTES seg13:0x1408 seg7:0x57c2 seg12:0x2e8e]
    o.hit_sound = 0x64;                  // [BYTES seg13:0x1412 seg7:0x57d7]
    o.radar_color.set(0x000000ffu);      // [BYTES seg13:0x141c seg7:0x57e8 seg12:0x2eb4 seg12:0x2eba]
    o.radar_draw.set(kRadarSmall);       // far seg11:0x3914 [BYTES seg13:0x1429 seg7:0x57fc]
    o.on_damage.set(kDamagePool5);       // far seg7:0x570a [BYTES seg13:0x1439 seg7:0x5813]
    o.b53 = 0;                           // [BYTES seg13:0x1449 seg7:0x582a]
    o.b54 = 0;                           // [BYTES seg13:0x1452 seg7:0x583a]
    o.owner_pool = -1;                   // [BYTES seg13:0x145b seg7:0x584a]
    o.src_y = o.gap;                     // [BYTES seg13:0x146b-0x146f seg7:0x585d-0x5861]
    o.b2e = 0;                           // [BYTES seg13:0x1479 seg7:0x5874]
    pool5_entry(c, r).age = -1;          // [BYTES seg13:0x1482 seg7:0x587d seg12:0x2f4c]
    p.set_in_use(pool, lo16(r + 1));     // [BYTES seg13:0x1488 seg7:0x5889 seg12:0x2f58]
}

int pool5_free_record(Pools& p, CollideGlobals& c, int pool) {
    for (int r = 0;; ++r) {
        if (r >= kPool5Table) {
            // The original goes on into DGROUP:0x5b28 and DGROUP:0x6038 (s7-fuel.md Port plan 1,
            // Open items); the port makes no ball [DESIGN ADR 0016 decisions 2 and 3] (ruling R2).
            static bool once = false;
            guard_todo(once, "a pool-5 ball past the 11 table entries (no ball made)", "DGROUP:0x5a4c");
            return -1;
        }
        if (r >= p.allocated(pool)) pool5_grow(p, c, pool, r);  // [BYTES seg13:0x13b4-0x13bd]
        if (r >= p.records(pool)) return -1;                    // [DESIGN] storage bound
        if (pool5_entry(c, r).age == -1) return r;              // [BYTES seg13:0x148c-0x1495]
    }
}

std::int16_t launch_cap(std::int16_t detail) {
    const std::int16_t d2 = add16(detail, 2);  // inc ax; inc ax [BYTES seg13:0x139d-0x139e]
    return add16(d2, d2);                      // add ax, ax [BYTES seg13:0x139f]
}

std::int16_t spawn_cap(std::int16_t detail, std::int16_t score_level) {
    const std::int16_t d2 = add16(add16(detail, 2), add16(detail, 2));  // [BYTES seg12:0x2dd3-0x2dd8]
    const std::int16_t l = add16(score_level > 10 ? std::int16_t{10} : score_level, 10);  // [BYTES seg12:0x2dda-0x2de6]
    const std::int16_t cap = idiv16(mul16(d2, l), 20);  // imul cx; cwd; idiv 20 [BYTES seg12:0x2de9-0x2def]
    return cap > 10 ? std::int16_t{10} : cap;           // [BYTES seg12:0x2df1-0x2df6]
}

BallFrame ball_frame(std::int16_t age) {
    BallFrame b;
    if (age < 0x1c) {  // [BYTES seg7:0x474f-0x4771]
        b.f = idiv16(age, 5);
        b.side = add16(mul16(b.f, 6), 10);
        b.off = mul16(b.f, 3);
    } else if (age < 0x1e) {  // dword [bp-0xe] = 0x340007, [bp-0x10] = 0x15 [BYTES seg7:0x40de-0x40f2]
        b.f = 7;
        b.side = 0x34;
        b.off = 0x15;
    }  // else 0, 0, 0 [BYTES seg7:0x40f4-0x40fc]
    return b;
}

// ---------------------------------------------------------------- seg13:0x1318

bool launch_fuel_ball(FuelCtx& c, int pool, int rec, std::int16_t life, std::int16_t kind) {
    World& w = c.w;
    Pools& p = w.pools;
    bool go = false;  // [bp-0xc] [BYTES seg13:0x1327]
    if (pool == c.g.ship_pool) {  // [BYTES seg13:0x132c-0x1332]
        ShipRecord& S = ship_at(w, rec);
        const int n = static_cast<std::int8_t>(S.slots_cb());  // (int8) +0xcb, cbw [BYTES seg13:0x1343-0x134b]
        for (int l = 1; l < n; ++l) {
            if (sbyte_at(S, 0xb8 + l) != kind) continue;  // the first slot holding kind [BYTES seg13:0x1354-0x135c]
            if (S.fuel > kBallFuelFloor) {                // signed `jle` [BYTES seg13:0x135e-0x1363]
                const std::int16_t f = sub16(S.fuel, c.h.weapon_word(kind, 0x2e));  // [BYTES seg13:0x1365-0x136c]
                S.fuel = f < 0 ? std::int16_t{0} : f;     // `jns` [BYTES seg13:0x1370-0x1374]
                go = true;                                // [BYTES seg13:0x1378]
            }
            break;
        }
    } else {
        go = true;  // [BYTES seg13:0x1332 seg13:0x1378]
    }
    if (!go) return false;  // no ball, no sound, no fuel [BYTES seg13:0x137d-0x1381]
    // The launcher on screen, then the cap, both after the fuel [BYTES seg13:0x1385-0x13a7] (O1)
    if (obj(w, pool, rec).on_screen == 0) return false;
    if (!(launch_cap(w.detail) > p.in_use(kPool5))) return false;  // signed `jg`, the literal pool 5
    const int r = pool5_free_record(p, c.c, kPool5);
    if (r < 0) return false;
    Pool5Entry& E = pool5_entry(c.c, r);
    // The take [BYTES seg13:0x1499-0x14fc] (verified claim 6)
    E.age = 0;
    E.w10 = 0;
    E.hit_pool = static_cast<std::int16_t>(pool);
    E.hit_rec = static_cast<std::int16_t>(rec);
    {
        ObjectRecord& B = p.rec(kPool5, r);
        if (kind == kBallSnow)
            B.mass = kMassSnowball;
        else if (kind == kBallFire)
            B.mass = kMassFireball;  // any other kind keeps the record's mass (O3)
        B.state = 0;
        E.split = 0;
        B.b2e = 0;
    }
    // Five stores to the same two words: the counter [bp-6] is not in the index [BYTES
    // seg13:0x1501-0x151c] (verified claim 7); Tx/Ty[1 ... 5] and Ts[] keep the record's last
    // ball's words (O4).
    for (int k = 0; k < 5; ++k) {
        E.tx[0] = kTrailStart;
        E.ty[0] = kTrailStart;
    }
    E.shooter_pool = static_cast<std::int16_t>(pool);  // [BYTES seg13:0x151e-0x152c]
    E.shooter_rec = static_cast<std::int16_t>(rec);
    const ObjectRecord& L = obj(w, pool, rec);
    ObjectRecord& B = p.rec(kPool5, r);
    const int f = L.frame;  // the launcher's heading +0x28
    B.vx = mul16(hx(f), 6);  // `imul di, [di + 0x1050], 6` [BYTES seg13:0x153a-0x1558]
    B.vy = mul16(hy(f), 6);  // [BYTES seg13:0x155c-0x1574]
    // The corner: lo16(heading · (E/32 + 6)) + E/2 + the launcher's x/y - 32, E/2 and E/32 toward
    // zero [BYTES seg13:0x1580-0x161f] (verified claim 9)
    const std::int16_t e2 = idiv16(L.extent, 2);
    const std::int16_t q = add16(idiv16(L.extent, 32), 6);
    B.x = sub16(add16(add16(mul16(hx(f), q), e2), L.x), 0x20);
    B.y = sub16(add16(add16(mul16(hy(f), q), e2), L.y), 0x20);
    E.life = life;  // [BYTES seg13:0x1623-0x1629]
    B.vx = add16(B.vx, L.vx);  // [BYTES seg13:0x162d-0x163a]
    B.vy = add16(B.vy, L.vy);  // [BYTES seg13:0x163e-0x164b]
    // The sounds: the reset for the player's ball, then 0x1b or 0x19 at the literal pool 5
    // [BYTES seg13:0x164f-0x1689] (verified claim 11; O15, O16)
    if (pool == c.g.ship_pool && rec == c.g.player) c.h.sound_reset();
    c.h.sound(life > 0x18 ? kSoundFireball : kSoundFire1, kPool5, r, 1);
    recount_in_use(p, kPool5);  // seg7:0x2080(5, -1) [BYTES seg13:0x1693-0x169c]
    return true;
}

// ---------------------------------------------------------------- seg12:0x2db6

bool spawn_fuel_ball(FuelCtx& c, int pool, int rec, std::int16_t heading, std::int16_t life,
                     std::int16_t kind) {
    World& w = c.w;
    Pools& p = w.pools;
    const int P5 = c.g.pool5;  // DGROUP:0x9328
    if (life > 0x18) life = 0x18;  // [BYTES seg12:0x2dc5-0x2dd0]
    if (!(spawn_cap(w.detail, w.score_level) > p.in_use(P5))) return false;  // [BYTES seg12:0x2df9-0x2e05]
    const int r = pool5_free_record(p, c.c, P5);
    if (r < 0) return false;
    Pool5Entry& E = pool5_entry(c.c, r);
    // The take [BYTES seg12:0x2f6a-0x2fd3]
    E.age = 0;
    E.w10 = 0;
    {
        ObjectRecord& B = p.rec(P5, r);
        B.state = 0;
        E.split = 0;
        B.b2e = 0;
        if (kind == kBallFire)
            B.mass = kMassFireball;
        else if (kind == kBallSnow)
            B.mass = kMassSnowball;
    }
    for (int k = 0; k < 5; ++k) {  // (r·0x12 + k)·2: the counter in the index [BYTES seg12:0x2fd9-0x2ff9]
        E.tx[ix(k)] = kTrailStart;
        E.ty[ix(k)] = kTrailStart;
    }
    const ObjectRecord& L = obj(w, pool, rec);
    ObjectRecord& B = p.rec(P5, r);
    // The corner with E/32 + 4 steps of the heading argument [BYTES seg12:0x2ffb-0x30a5]
    const std::int16_t e2 = idiv16(L.extent, 2);
    const std::int16_t q = add16(idiv16(L.extent, 32), 4);
    B.x = sub16(add16(add16(e2, mul16(q, hx(heading))), L.x), 0x20);
    B.y = sub16(add16(add16(mul16(q, hy(heading)), e2), L.y), 0x20);
    E.shooter_pool = static_cast<std::int16_t>(pool);  // [BYTES seg12:0x30a9-0x30c5]
    E.shooter_rec = static_cast<std::int16_t>(rec);
    E.hit_pool = static_cast<std::int16_t>(pool);
    E.hit_rec = static_cast<std::int16_t>(rec);
    B.vx = mul16(hx(heading), 6);  // no launcher velocity [BYTES seg12:0x30c9-0x30d9]
    B.vy = mul16(hy(heading), 6);  // [BYTES seg12:0x30dd-0x30ef]
    // life > 25 after the cap of 24: always 0x19 (O13) [BYTES seg12:0x30f3-0x3116]
    c.h.sound(life > 0x19 ? kSoundFireball : kSoundFire1, P5, r, 1);
    E.life = life;              // [BYTES seg12:0x3119-0x3120]
    recount_in_use(p, P5);      // seg7:0x2080(DGROUP:0x9328, -1) [BYTES seg12:0x312f-0x313a]
    return true;
}

// ---------------------------------------------------------------- seg7:0x40c4

namespace {

// The heating copies after the dispatch [BYTES seg7:0x443a-0x463d] (s7-fuel.md 2.8; verified
// claims 23-26, O9). bm is the ball's `mass`, read again at each test as the bytes do.
void heating(UpdateCtx& c, World& w, int r, int hp, int hr) {
    WaveGlobals& g = c.flight.g;
    Pools& p = w.pools;
    const int P5 = g.pool5;
    auto ball_mass = [&]() { return p.rec(P5, r).mass; };
    auto heat_rules = [&](ObjectRecord& K) {
        if (ball_mass() == kMassFireball) {
            int v = static_cast<std::int8_t>(K.flame) + 1;  // cbw; inc; cmp 9; jle
            if (v > 9) v = 9;
            K.flame = static_cast<std::uint8_t>(v);
            K.d6a.set(g.now + 0x1388u);  // now + 5000 [BYTES seg7:0x4482-0x448e seg7:0x4534-0x4548]
        } else if (ball_mass() == kMassSnowball) {
            const int h = static_cast<std::int8_t>(K.flame);
            if (h > 0) K.flame = static_cast<std::uint8_t>(-(5 * h));  // 8-bit imul, neg al [BYTES seg7:0x44c4-0x44d3 seg7:0x457e-0x458d]
        }
    };
    // 1. The rock pool DGROUP:0x934a (a word compare); unreachable in a wave, where the pair
    // finder returns no pool below 5 (verified claim 23) [BYTES seg7:0x443a-0x44d3].
    if (hp == g.rock_pool) heat_rules(obj(w, hp, hr));
    // 2. A pool-8 object of `mass` 0xdac, the gate posts: the live copy in a wave (verified
    // claims 24, 25) [BYTES seg7:0x44d7-0x458d].
    if (hp == 8 && obj(w, hp, hr).mass == 0xdac) heat_rules(obj(w, hp, hr));
    // 3. Any object of `mass` 0xa0: the icon words of icon i = the hit record (verified claim
    // 26, O9; core.md:418 [OPEN]) [BYTES seg7:0x4591-0x463d].
    if (obj(w, hp, hr).mass != 0xa0) return;
    const int i = hr;
    if (i < 0 || i >= kIconRecords) {
        // The icon words of a record past the eight lie in the smoke table: the read gives 0 and
        // the writes are lost, and the ignite seg7:0x477e, whose image slot 14 + i would pass
        // the eight flame slots, is not made [DESIGN ADR 0016 decision 2].
        static bool once = false;
        guard_todo(once, "the mass-160 branch's icon index outside the eight icons", "seg7:0x45c9");
        return;
    }
    IconRecord& ic = w.icons[ix(i)];
    if (ball_mass() == kMassFireball) {
        if (ic.w02 != 0) return;              // DGROUP:0x57aa + 0x23·i [BYTES seg7:0x45c9-0x45d1]
        ic.w02 = 2;                           // [BYTES seg7:0x45d3]
        set_icon_burn_until(w, i, g.now + 0xfa0u);  // DGROUP:0x57a4 + 0x23·i [BYTES seg7:0x45d9-0x45e5]
        icon_ignite(c, w, i);                 // seg7:0x477e(i) [BYTES seg7:0x45ea-0x45ef]
    } else if (ball_mass() == kMassSnowball) {
        if (ic.w02 <= 0) return;              // signed `jle` [BYTES seg7:0x4609-0x4616]
        ic.w00 = 0;                           // the dword DGROUP:0x57a8 + 0x23·i [BYTES seg7:0x4618-0x461b]
        ic.w02 = 0;
        for (int k = 0; k < 4; ++k) c.host.puff(hp, hr, 1);  // seg7:0x5b68(hit pool, hit rec, 1) [BYTES seg7:0x4623-0x463d]
    }
}

}  // namespace

void move_pool5(UpdateCtx& c, World& w) {
    MoverScope scope(c.host, c);
    WaveGlobals& g = c.flight.g;
    Pools& p = w.pools;
    UpdateHost& h = c.host;
    CollideGlobals& cg = h.collide_globals();
    const int P5 = g.pool5;
    const int S = shift_of(w.size);  // DGROUP:0x9156
    const WaveGraphics& wg = wave_graphics();
    int slot = -1;  // [bp-0x12]: kept from the previous record (O6)
    // r < DGROUP:0x9406 + 2·P5, re-read every iteration [BYTES seg7:0x4736-0x4743]
    for (int r = 0; r < p.in_use(P5); ++r) {
        if (r >= p.records(P5)) break;  // [DESIGN] storage bound
        if (r >= kPool5Table) {
            static bool once = false;
            guard_todo(once, "the pool-5 mover past the 11 table entries", "seg7:0x4745");  // [DESIGN ADR 0016 decisions 2 and 3] (ruling R2)
            break;
        }
        Pool5Entry& E = cg.pool5[ix(r)];
        if (E.age <= -1) continue;  // [BYTES seg7:0x4745-0x474d]
        const BallFrame fr = ball_frame(E.age);
        const std::int16_t side = sar16(shl16(fr.side, S), 1);  // [BYTES seg7:0x40ff-0x410a]
        // 2.3 the trail shift, k = 5 ... 1 [BYTES seg7:0x410d-0x413a]
        for (int k = kPool5Trail - 1; k > 0; --k) {
            E.tx[ix(k)] = E.tx[ix(k - 1)];
            E.ty[ix(k)] = E.ty[ix(k - 1)];
            E.ts[ix(k)] = E.ts[ix(k - 1)];
        }
        const std::int16_t o8 = shl16(fr.off, 3);  // [BYTES seg7:0x413c-0x413f]
        {
            ObjectRecord& B = p.rec(P5, r);
            B.x = sub16(B.x, o8);  // [BYTES seg7:0x4151]
            B.y = sub16(B.y, o8);  // [BYTES seg7:0x4164]
            // seg8:0x29ea(x, y), the dword copied into +0x00 [BYTES seg7:0x4168-0x4193]
            const ScreenPoint sp = world_to_screen(c.view, B.x, B.y, g.wave_kind, g.dg_9eb5, g.dg_9130);
            B.scr_x = sp.x;
            B.scr_y = sp.y;
            E.tx[0] = B.scr_x;  // [BYTES seg7:0x41ab]
            E.ty[0] = B.scr_y;  // [BYTES seg7:0x41b3]
            E.ts[0] = side;     // [BYTES seg7:0x41ba]
            B.size = side;      // [BYTES seg7:0x41be]
            B.extent = shl16(side, shift_of(w.shift));  // << DGROUP:0x9150 [BYTES seg7:0x41cd-0x41d3]
            B.src_x = sar16(shl16(kExplosionX[ix(fr.f)], S), 1);  // DGROUP:0xb70[f] [BYTES seg7:0x41d7-0x41f3]
            B.src_y = 0;  // [BYTES seg7:0x4202]
            if (B.mass == kMassFireball)
                slot = kSlotExplosion;  // [BYTES seg7:0x4213-0x421a]
            else if (B.mass == kMassSnowball)
                slot = kSlotSnowball;   // [BYTES seg7:0x4230-0x4238]
        }
        // seg8:0xb76(P5·256 + r, 6, DGROUP:0x39e8[slot], 0) [BYTES seg7:0x423d-0x4257]
        if (slot >= 0) {
            h.draw_sprite(pool_rec(P5, r), 6, slot, 0);
        } else {
            static bool once = false;
            guard_todo(once, "a pool-5 ball of another mass before any slot (not drawn)", "seg7:0x423d");  // [DESIGN ADR 0016 decision 1]
        }
        {
            ObjectRecord& B = p.rec(P5, r);
            B.x = add16(B.x, o8);  // [BYTES seg7:0x4271]
            B.y = add16(B.y, o8);  // [BYTES seg7:0x4284]
        }
        // 2.5 the trail's erase and fade on DGROUP:0x13dc [BYTES seg7:0x4288-0x4368]
        Blitter* out = h.blitter();
        if (out)
            out->pat_blt(kDcWindow, sub16(E.tx[5], 1), sub16(E.ty[5], 1), add16(E.ts[5], 2), add16(E.ts[5], 2),
                         kBlackness);
        {
            // The player's redraw: the raw element-5 square against the box from (W/2, H/2) (claim
            // 20 as corrected) [BYTES seg7:0x42af-0x430b]
            const std::int16_t hw = idiv16(w.W, 2), hh = idiv16(w.H, 2);
            const std::int16_t b = sar16(shl16(0x28, S), 1);
            if (sub16(hw, E.ts[5]) < E.tx[5] && sub16(hh, E.ts[5]) < E.ty[5] && add16(hw, b) > E.tx[5] &&
                add16(b, hh) > E.ty[5])
                ship_at(w, g.player).due156.set(g.now - 1u);  // sub 1; sbb 0 (DGROUP:0x71e6 + 0x15a·player)
        }
        if (out) {
            for (int j = 0; j < 2; ++j) {  // brushes DGROUP:0x5ea8 + 2j on elements 4 - j
                const BrushId old = out->select_brush(kDcWindow, j == 0 ? wg.mask : wg.checker);
                const int k = 4 - j;
                out->pat_blt(kDcWindow, sub16(E.tx[ix(k)], 1), sub16(E.ty[ix(k)], 1), add16(E.ts[ix(k)], 2),
                             add16(E.ts[ix(k)], 2), kDpa);
                out->select_brush(kDcWindow, old);
            }
        }
        // 2.6 the move [BYTES seg7:0x436a-0x439e]
        {
            const ObjectRecord& B = p.rec(P5, r);
            E.svx = B.vx;
            E.svy = B.vy;
        }
        integrate(c.flight, 1, pool_rec(P5, r));  // seg8:0x1b54(P5·256 + r, 1)
        // 2.7 the collision [BYTES seg7:0x43a3-0x4437]
        if (p.rec(P5, r).on_screen != 0 || g.wave_kind == 6) {
            c.f.dg_943c = static_cast<std::int16_t>(r);  // [BYTES seg7:0x43c1]
            c.f.dg_943a = static_cast<std::int16_t>(P5);  // [BYTES seg7:0x43c9]
            const std::uint16_t hit = h.collide(pool_rec(P5, r));  // seg8:0x50aa [BYTES seg7:0x43d4]
            if (hit != 0xffffu) {
                CollideCtx x = frame_collide_ctx(c, w);
                std::int16_t a = 0;
                {
                    // The pointers are written before the response's own dispatch, which may
                    // grow pool 5 through a split [BYTES seg8:0x5a27-0x5a36].
                    ObjectRecord& B = p.rec(P5, r);
                    a = respond(x, B.mass, add16(B.extent, 2), &B.vy, &B.vx, hit);  // [BYTES seg7:0x43e1-0x4414]
                }
                const int hp = hit >> 8, hr = hit & 0xff;
                dispatch(x, P5, r, hp, hr, a);  // cdecl, the return dropped [BYTES seg7:0x4419-0x4437]
                heating(c, w, r, hp, hr);
            }
        }
        // 2.9 the age and the end [BYTES seg7:0x463f-0x4730]
        E.age = add16(E.age, 1);
        if (add16(E.life, 4) >= E.age) continue;  // signed `jge`
        {
            ObjectRecord& B = p.rec(P5, r);
            out = h.blitter();
            if (in_play_area(B.scr_x, B.scr_y, w.W, w.H) && out)  // seg8:0x7ba8 [BYTES seg7:0x4665-0x4699]
                out->pat_blt(kDcWindow, sub16(B.scr_x, 1), sub16(B.scr_y, 1), kBallEndSquare, kBallEndSquare,
                             kBlackness);
            for (int k = 0; k < kPool5Trail && out; ++k)  // [BYTES seg7:0x469e-0x46e3]
                out->pat_blt(kDcWindow, sub16(E.tx[ix(k)], 1), sub16(E.ty[ix(k)], 1), add16(B.size, 2),
                             add16(B.size, 2), kBlackness);
            E.age = -1;   // [BYTES seg7:0x46e5-0x46e9]
            B.state = 1;  // [BYTES seg7:0x46fe]
        }
        if (g.dg_9130 != 0) {  // seg18:0x5350(dword +0) [BYTES seg7:0x4704-0x4721] (O7)
            static bool once = false;
            guard_todo(once, "the seg18 mode's end of a pool-5 ball", "seg18:0x5350");  // [OPEN M6]
        }
        recount_in_use(p, P5);  // seg7:0x2080(P5, r) [BYTES seg7:0x4724-0x472b]
    }
}

// ---------------------------------------------------------------- seg7:0x570a

std::int16_t pool5_on_damage(CollideCtx& x, int pool, int rec, std::int16_t a) {
    (void)a;  // [bp+0xa] is never read (verified claim 29)
    World& w = x.w;
    Pools& p = w.pools;
    if (pool < 0 || pool >= kPoolCount || rec < 0 || rec >= p.records(pool)) return 0;  // [DESIGN]
    Pool5Entry& E = pool5_entry(x.c, rec);
    if (E.split != 0 || p.rec(pool, rec).mass != kMassSnowball) {  // [BYTES seg7:0x5719-0x573a]
        ObjectRecord& B = p.rec(pool, rec);
        B.vx = E.svx;  // [BYTES seg7:0x5a18-0x5a36] (verified claim 30)
        B.vy = E.svy;
        return 0;
    }
    const int P5 = x.g.pool5;  // DGROUP:0x9328
    const std::int16_t svx = E.svx, svy = E.svy;
    // The free search on DGROUP:0x9328, no cap [BYTES seg7:0x573d-0x5896 seg7:0x59a4-0x59a8]
    const int n = pool5_free_record(p, x.c, P5);
    if (n >= 0) {
        Pool5Entry& N = pool5_entry(x.c, n);
        N.age = 0;   // [BYTES seg7:0x589a-0x589c]
        N.w10 = 0;   // [BYTES seg7:0x58a0]
        ObjectRecord& C = obj(w, pool, n);  // the [bp+6] pool's records [BYTES seg7:0x58a6-0x58b4]
        C.state = 0;
        N.split = 1;   // [BYTES seg7:0x58c0]
        C.b2e = 0;     // [BYTES seg7:0x58cc]
        C.mass = kMassSnowball;  // [BYTES seg7:0x58d6]
        for (int k = 0; k < 5; ++k) {  // the counter in the index [BYTES seg7:0x58df-0x58fa] (verified claim 33)
            N.tx[ix(k)] = kTrailStart;
            N.ty[ix(k)] = kTrailStart;
        }
        const ObjectRecord& R = p.rec(pool, rec);
        C.x = R.x;  // [BYTES seg7:0x590c-0x5916]
        C.y = R.y;  // [BYTES seg7:0x5924-0x592a]
        N.shooter_pool = E.shooter_pool;  // [BYTES seg7:0x592e-0x5952]
        N.shooter_rec = E.shooter_rec;
        N.hit_pool = E.hit_pool;
        N.hit_rec = E.hit_rec;
        C.vx = add16(idiv16(svy, 3), svx);   // [BYTES seg7:0x5956-0x597f]
        C.vy = add16(idiv16(svx, -3), svy);  // [BYTES seg7:0x5983-0x5998]
        N.life = 0x10;                       // [BYTES seg7:0x599c]
    }
    recount_in_use(p, P5);  // seg7:0x2080(DGROUP:0x9328, -1) [BYTES seg7:0x59ac-0x59b7]
    ObjectRecord& R = p.rec(pool, rec);
    R.vx = add16(idiv16(svy, -3), svx);  // [BYTES seg7:0x59ba-0x59e7] (verified claim 32)
    R.vy = add16(idiv16(svx, 3), svy);   // [BYTES seg7:0x59eb-0x5a03]
    E.split = 1;                         // [BYTES seg7:0x5a0a]
    E.life = 0x10;                       // [BYTES seg7:0x5a10]
    return 0;                            // [BYTES seg7:0x5a3a]
}

// ---------------------------------------------------------------- seg12:0x2288, seg9:0x6624

void pool5_gate(CollideGlobals& c, int rec) {
    Pool5Entry& E = pool5_entry(c, rec);
    E.age = sub16(E.life, 7);  // [BYTES seg12:0x22cf-0x22d6]
}

void pool5_wave_start(Pools& p, int pool5, CollideGlobals* c) {
    for (int r = 0; r < p.allocated(pool5); ++r) {  // [BYTES seg9:0x6782-0x67af]
        if (c) pool5_entry(*c, r).age = -1;
        if (r < p.records(pool5)) p.rec(pool5, r).state = 1;
    }
}

void set_pool5_tables(std::function<CollideGlobals*()> f) { tables_fn() = std::move(f); }

CollideGlobals* pool5_tables() {
    const auto& f = tables_fn();
    return f ? f() : nullptr;
}

}  // namespace is::world
