// Damage (damage.h). Every site cites the listing it ports.
#include "world/damage.h"

namespace is::world {

namespace {

std::uint32_t add32u(std::uint32_t a, std::int32_t b) { return a + static_cast<std::uint32_t>(b); }
std::int32_t lmul(std::int32_t a, std::int32_t b) {
    const std::uint32_t p = static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b);
    return p < 0x80000000u ? static_cast<std::int32_t>(p) : -static_cast<std::int32_t>(~p) - 1;
}
// seg5:0x110: signed 32-bit divide truncating toward zero.
std::int32_t ldiv(std::int32_t n, std::int32_t d) {
    return static_cast<std::int32_t>(static_cast<std::int64_t>(n) / static_cast<std::int64_t>(d));
}
// A 16-bit `idiv` with the port's guard: a zero divisor, which the original traps on, gives 0
// [DESIGN].
std::int16_t q16(std::int16_t n, std::int16_t d) { return d == 0 ? std::int16_t{0} : idiv16(n, d); }

// The roster byte +0x34 + k of ship s's roster record (0 without one in the World) [DESIGN].
std::int16_t roster_byte(const World& w, const ShipRecord& s, int k) {
    if (s.roster_index < 0 || s.roster_index >= static_cast<int>(w.roster.size())) return 0;
    return w.roster[static_cast<std::size_t>(s.roster_index)].b33[static_cast<std::size_t>(1 + k)];
}

std::int16_t& sys(ShipRecord& s, int k) { return s.roster_word(k); }

}  // namespace

// ---- seg8:0x45e0

std::int16_t ship_damage(CollideCtx& x, int pool, int ship, std::int16_t d) {
    (void)pool;  // the routine reads the ship pool DGROUP:0x9324, not [bp+6]
    World& w = x.w;
    WaveGlobals& g = x.g;
    CollideGlobals& c = x.c;
    const int ships = g.ship_pool;
    ShipRecord& S = ship_at(w, ship);
    // 1. No damage [BYTES seg8:0x45f4-0x4614]
    if (S.radar_hide == 1 && g.dg_9130 == 0) return 0;
    if (ship == g.player && c.w104c != 0) return 0;
    // 2. A dead ship explodes early [BYTES seg8:0x461b-0x467c]
    if (S.bec == 0) {
        if (!(x.dg_943a == ships && x.dg_943c == ship) && x.dg_943a != 11) {
            if (S.lockout.get() - 5500u < g.now) x.h.destroy(ships, ship);  // seg12:0x274a
        }
        if (g.dg_9130 != 0) {
            c.w9530 = 6;
            x.h.seg18_633c();
        }
        return 0;
    }
    const int n = S.design_index;  // [BYTES seg8:0x4683]
    const DamageDesign D = x.h.design(w, n);
    // 3. Scale [BYTES seg8:0x468a-0x4697]
    d = idiv16(mul16(d, 7), 8);
    if (ship == g.player) {
        // [BYTES seg8:0x46a5-0x46b7], then the rating loss [BYTES seg8:0x46ba-0x4774]
        d = idiv16(mul16(add16(w.w9000, 6), d), 8);
        std::int32_t r = static_cast<std::int32_t>(w.rating);
        const std::int32_t m = r > 35000 ? 35000 : r;
        if (w.player_game == 0) {
            r = static_cast<std::int32_t>(add32u(static_cast<std::uint32_t>(r), ldiv(lmul(m, d), -4000)));
            if (r > 200000) r = static_cast<std::int32_t>(add32u(static_cast<std::uint32_t>(r), ldiv(lmul(r, d), -6000)));
        } else {
            r = static_cast<std::int32_t>(add32u(static_cast<std::uint32_t>(r), ldiv(lmul(m, d), -9000)));
        }
        w.rating = static_cast<std::uint32_t>(r);
    } else if (w.handover.demons_gate != 0 && ship == 1) {
        c.w8e30 = g.now + 20000u;  // [BYTES seg8:0x4776-0x4793]
    }
    // 4. The attacker DGROUP:0x943a / 0x943c [BYTES seg8:0x4797-0x488c]
    std::int16_t avx, avy, ax, ay, akind;
    if (x.dg_943a == 0x1e) {
        const ShotSlotView sh = x.h.shot(x.dg_943c);
        avx = sh.vx, avy = sh.vy, ax = sh.x, ay = sh.y;
        akind = 2;
        if (sh.owner_pool == ships && sh.owner_rec != ship) x.h.seg4_5e8e(ships, sh.owner_rec, d, 1);
        if (sh.owner_pool == ships && sh.owner_rec == g.player && g.play_mode == 0 && ship != g.player) {
            c.hit_pool = static_cast<std::int16_t>(ships);
            c.hit_rec = static_cast<std::int16_t>(ship);
            c.hit_time = g.now;
        }
    } else {
        const ObjectRecord& X = obj(w, x.dg_943a, x.dg_943c);
        avx = X.vx, avy = X.vy, ax = X.x, ay = X.y;
        akind = X.size;  // [bp-0xc]: the attacker's +0x22, which the shot rules test for 2
        d = idiv16(mul16(d, 3), 4);
        if (g.wave_kind == 6 && x.dg_943a == 0xe) d = idiv16(d, 2);
    }
    ObjectRecord& O = obj(w, ships, ship);
    // 5. The last hit [BYTES seg8:0x48aa-0x48bd]
    set_ship_dword(S, 0x14a, static_cast<std::uint16_t>(O.x) | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(O.y)) << 16));
    set_ship_dword(S, 0x14e, g.now);
    // 6. The side [BYTES seg8:0x48c1-0x4948]
    const std::int16_t p10 = sub16(avx, mul16(O.vx, 10));
    const std::int16_t pe = sub16(mul16(O.vy, 10), avy);
    const std::int16_t hs = idiv16(O.size, 2), ha = idiv16(akind, 2);
    const std::int16_t p14 = add16(sub16(sub16(ha, hs), O.x), ax);
    const std::int16_t p12 = sub16(add16(ha, sub16(O.y, hs)), ay);
    const std::int32_t cross = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(p12) * p10) -
                                                         static_cast<std::uint32_t>(static_cast<std::int32_t>(p14) * pe));
    O.angle = cross > 0 ? std::int16_t{1} : std::int16_t{-1};
    // 7. The bounce sound [BYTES seg8:0x494e-0x4960]
    if (c.w0b0a != 0) x.h.sound(7, ships, ship, 1);
    // 8. The mode factor [BYTES seg8:0x4968-0x49b8]
    std::int16_t v;
    if (S.bea != 0) {
        v = idiv16(shl16(d, 2), 3);
        if (v < 1) v = 1;
    } else if (S.be8 != 0) {
        v = mul16(d, 3);
        if (v < 2) v = 2;
    } else {
        v = add16(d, d);
        if (v < 1) v = 1;
    }
    O.damage = v;
    // 9. The design's hit sound through seg10:0x7944 [BYTES seg8:0x49bc-0x49dc]. The ship set-up
    // writes SHP +0x1f into both the design word and object +0x42 [BYTES seg14:0x1e2d
    // seg14:0x20d3]; with no design word in the World the port reads the object's [DESIGN].
    if (c.w0b0a == 0) x.h.design_sound(D.hit_sound != 0 ? D.hit_sound : O.hit_sound, ships, ship, 1);
    // 10. The systems [BYTES seg8:0x49e4-0x4b1c]
    for (int k = 0; k < 4; ++k) {
        std::int16_t q;
        if (akind == 2) {
            q = q16(O.damage, D.shot_div[static_cast<std::size_t>(k)]);
            if (S.roster_index != -1)
                q = idiv16(mul16(q, sub16(10, roster_byte(w, S, k))), 10);
        } else {
            q = q16(O.damage, D.hit_div[static_cast<std::size_t>(k)]);
            if (S.roster_index != -1)
                q = idiv16(mul16(q, sub16(20, mul16(3, roster_byte(w, S, k)))), 20);
        }
        if (q < 1) q = 1;
        sys(S, k) = sub16(sys(S, k), q);
    }
    for (int k = 0; k < 4; ++k)
        if (sys(S, k) < 0) sys(S, k) = 0;
    // 11. The wings [BYTES seg8:0x4b1e-0x4c61]
    const bool wing_ship = D.lamp_row[3] == 3;
    const std::uint8_t wing_ac = static_cast<std::uint8_t>(sbyte_at(S, 0xac));
    const std::uint8_t wing_ad = static_cast<std::uint8_t>(sbyte_at(S, 0xad));
    if (S.w2c == 0 && wing_ship) {
        if (wing_ad != 0 || wing_ac != 0) {
            S.type = 0;
            S.bea = 0;
            if (sbyte_at(S, 0xad) != 0) tear_wing(x, ship, 1);
            if (sbyte_at(S, 0xac) != 0) tear_wing(x, ship, 0);
            S.w20 = sub16(D.w2a, 14);
            S.w22 = 7;
            if (S.w24 > 1) S.w24 = 1;
            set_byte_at(S, 0xe0, 1);
        }
    } else if (S.w2c < 50 && wing_ship && wing_ac != 0 && wing_ad != 0) {
        S.type = 0;
        S.bea = 0;
        if (O.angle == 1) {
            tear_wing(x, ship, 1);
            if (S.w24 == 2) {
                set_byte_at(S, 0xe0, 1);
                set_byte_at(S, 0xe1, 0);
            }
        } else {
            tear_wing(x, ship, 0);
            if (S.w24 == 2) {
                set_byte_at(S, 0xe1, 1);
                set_byte_at(S, 0xe0, 0);
            }
        }
        obj(w, ships, ship).angle = 0;
    }
    // 12. The thruster out [BYTES seg8:0x4c67-0x4c8f]
    if (S.w2a == 0 && D.lamp_row[2] == 2 && S.thrusting != 0) {
        S.thrusting = 0;
        x.h.flame_off(ship);  // seg13:0x6716
    }
    // 13. Death [BYTES seg8:0x4c95-0x4d41]
    if (S.w26 < 1 && !(x.dg_943a == ships && x.dg_943c == ship) && x.dg_943a != 11) {
        x.h.ship_destroyed(ships, ship);  // seg16:0x8e4
        for (DefenderEntry& e : c.defenders) {
            if (e.owner_pool != ships || e.owner_rec != ship) continue;
            e.w00 = 0xf;
            if (x.dg_943a == 0x1e) {
                const ShotSlotView sh = x.h.shot(x.dg_943c);
                e.killer_pool = sh.owner_pool;
                e.killer_rec = sh.owner_rec;
            } else if (x.dg_943a == 6) {
                const MissileSlot& m = x.w.pools.missile(x.dg_943c);  // DGROUP:0x66e0 (records.h)
                e.killer_pool = m.launcher_pool;
                e.killer_rec = m.launcher_rec;
            }
        }
        return 1;
    }
    // 14. Otherwise [BYTES seg8:0x4d43-0x4d52]
    c.w0b0a = 0;
    rate_ship(w, ship);  // seg13:0x33d6
    return 0;
}

// ---- seg8:0x2cd8

void tear_wing(CollideCtx& x, int ship, int side) {
    World& w = x.w;
    ShipRecord& S = ship_at(w, ship);
    S.due156.set(x.g.now - 10000u);  // [BYTES seg8:0x2cee-0x2d05]
    if (side == 0)
        set_byte_at(S, 0xac, 0);     // [BYTES seg8:0x2d0e]
    else
        set_byte_at(S, 0xad, 0);     // [BYTES seg8:0x2d1e]
    if (x.c.w95a6 == 0) x.h.wing_design(ship);  // seg14:0x3eda, seg14:0x4074 [BYTES seg8:0x2d23-0x2d91] [OPEN]
    if (w.restored == 0) {  // DGROUP:0x92e4 [BYTES seg8:0x2d96]
        x.h.wing_taunts(ship, side);  // [BYTES seg8:0x2da6-0x2e57] (M5) [OPEN]
        // The debris entry and its sound [BYTES seg8:0x33c2 seg8:0x33cd]
        if (x.h.wing_debris(ship, side)) x.h.sound(0x6a, x.g.ship_pool, ship, 1);
    }
}

// ---- seg7:0x5090

std::int16_t rock_damage(CollideCtx& x, int pool, int rec, std::int16_t d) {
    World& w = x.w;
    WaveGlobals& g = x.g;
    const CollideGlobals& c = x.c;
    // The guard on the last collision's B party (s1-hit-verify.md 25, CORRECTED): the word
    // DGROUP:0x9406 + 2·B pool, for a gun hit (B = (30, j)) the collision word eA DGROUP:0x9442
    // (s2-gun.md 6.1, claim 58) [BYTES seg7:0x509f-0x50b0]
    const std::int16_t in_use = in_use_word(x, c.b_pool);
    if (in_use < c.b_rec) return 0;
    const int rocks = g.rock_pool;
    if (pool != rocks || d <= 1) {  // [BYTES seg7:0x50b3-0x50c1 seg7:0x563d-0x5652]
        ObjectRecord& r = obj(w, pool, rec);
        r.damage = add16(r.damage, d);
        return 0;
    }
    d = idiv16(mul16(d, 3), 2);  // [BYTES seg7:0x50c5-0x50ce]
    const std::int16_t old_heat = static_cast<std::int8_t>(obj(w, pool, rec).flame);
    // The heat, unless B is a pool-5 object of mass 200 [BYTES seg7:0x50e7-0x5131]
    if (!(c.b_pool == g.pool5 && obj(w, c.b_pool, c.b_rec).mass == 0xc8)) {
        ObjectRecord& r = obj(w, pool, rec);
        std::int16_t hv = add16(idiv16(d, 4), static_cast<std::int8_t>(r.flame));
        if (hv > 9) hv = 9;
        r.flame = static_cast<std::uint8_t>(hv);
    }
    {
        ObjectRecord& r = obj(w, pool, rec);
        r.d6a.set(g.now + 3000u);           // [BYTES seg7:0x5135-0x5154]
        r.damage = add16(r.damage, d);      // [BYTES seg7:0x5163]
        if (r.damage <= r.max_damage || !(old_heat > 6 || g.dg_9130 != 0)) {  // [BYTES seg7:0x5171-0x5184]
            r.damage = add16(r.damage, d);  // [BYTES seg7:0x564e]
            return 0;
        }
        if (r.mass == 0x15e) {  // a small rock [BYTES seg7:0x5196-0x51b7]
            x.h.destroy(pool, rec);   // seg12:0x274a
            x.h.seg7_2080(pool, -1);  // seg7:0x2080
            return 1;
        }
        // A big rock splits [BYTES seg7:0x51bd-0x5219]
        obj(w, rocks, rec).state = 1;
        x.h.erase(r.scr_x, r.scr_y, r.size, r.size);
        x.h.sound(0x45, rocks, rec, 1);
    }
    const int sz = g.size & 0x1f;
    int n = 0;  // [bp-0xa]
    for (int k = 0; k < 3; ++k) {  // [BYTES seg7:0x5297-0x52b3]
        for (int j = 0; j < 0x7e; ++j) {
            if (w.pools.allocated(rocks) <= j) {
                // A new record like the template [BYTES seg7:0x52c6-0x542f]
                w.pools.grow_to(rocks, static_cast<std::int16_t>(j));
                ObjectRecord& t = obj(w, rocks, j);
                t.blast_size = g.dg_9130 != 0 ? 5 : 9;
                t.hit_sound = 8;
                t.radar_brush = 4;
                t.radar_color.set(0x00c0c0c0);
                t.on_damage.set(kDamageRock);
                t.b53 = 0;
                t.b54 = 0;
                t.owner_pool = -1;
                t.gap = sar16(shl16(5, sz), 1);
                t.src_y = t.gap;
                t.b2e = 0;
                t.state = 1;
                w.pools.set_in_use(rocks, static_cast<std::int16_t>(j + 1));
            }
            ObjectRecord& s = obj(w, rocks, j);
            if (s.state == 0) continue;  // [BYTES seg7:0x5444]
            // The small rock (s1-death.md 5) [BYTES seg7:0x544d-0x5611]
            ++n;
            s.mass = 0x15e;
            s.size = sar16(shl16(0x16, sz), 1);
            s.extent = 0xa8;
            s.b2e = 0;
            s.gap = sar16(shl16(4, sz), 1);
            s.src_y = s.gap;
            if (g.size == 0 && g.dg_9130 != 0) s.size = idiv16(add16(s.size, s.size), 3);
            if (g.size == 2 && w.w740 != 0) s.gap = 4, s.src_y = 4;
            if (g.size == 1 && (w.w740 != 0 || g.dg_9130 != 0)) s.gap = 2, s.src_y = 2;
            if (g.size == 0 && (w.w740 != 0 || g.dg_9130 != 0)) s.gap = 1, s.src_y = 1;
            s.radar_draw.set(kRadarSmall);
            s.max_damage = g.dg_9130 != 0 ? 100 : 300;
            s.damage = 0;
            s.flame = 9;
            // The offsets 8·(0, -10), 8·(15, 12), 8·(-15, 13) [BYTES seg7:0x5616-0x5632
            // seg7:0x521b-0x5280]
            static constexpr std::int16_t kOff[3][2] = {{0, -10}, {15, 12}, {-15, 13}};
            const int o = n <= 3 ? n - 1 : 2;
            const ObjectRecord& parent = obj(w, pool, rec);
            const std::int16_t px = parent.x, py = parent.y;
            ObjectRecord& s2 = obj(w, rocks, j);
            s2.x = add16(shl16(kOff[o][0], 3), px);
            s2.y = add16(shl16(kOff[o][1], 3), py);
            s2.state = 0;
            break;
        }
    }
    return 1;  // [BYTES seg7:0x51b7]
}

// ---- seg7:0x233c, seg7:0x26c4, seg7:0x2768 (M4 s2 GUN)

namespace {
IconRecord g_scratch_icon{};
IconRecord& icon_at(World& w, int rec) {
    if (rec < 0 || rec >= kIconRecords) return g_scratch_icon = IconRecord{};
    return w.icons[static_cast<std::size_t>(rec)];
}
}  // namespace

std::int16_t icon_damage(CollideCtx& x, int pool, int rec, std::int16_t d) {
    (void)pool;  // the routine reads pool 0 (DGROUP:0x1df8) [BYTES seg7:0x2360]
    World& w = x.w;
    const IconRecord& ic = icon_at(w, rec);
    ObjectRecord& O = obj(w, 0, rec);
    const auto heat = [&]() { return static_cast<std::int8_t>(O.flame); };
    // [BYTES seg7:0x234b-0x2372]
    if (ic.w00 == 0 && heat() != -100 && heat() != -2) {
        std::int16_t v = add16(mul16(d, 3), O.damage);  // [BYTES seg7:0x2375-0x2386]
        if (v > 0xd3) v = 0xd3;
        O.damage = v;
        w.dir().set_word(0x1d2 + 2 * static_cast<std::size_t>(rec), O.damage);  // [BYTES seg7:0x2390-0x23a5]
        std::int16_t gap = O.gap, side = O.size;  // [bp-6], [bp-4] [BYTES seg7:0x23b0-0x23bb]
        if (O.damage > 0 && heat() >= 0) {        // [BYTES seg7:0x23be-0x23ca]
            CrackPlan p;
            p.rec = rec;
            p.scaled = d != 0;  // seg10:0x6308(-1, -1, bitmap) [BYTES seg7:0x23f7]
            if (d == 0) gap = 6, side = 0x20;  // [BYTES seg7:0x2404]
            p.gap = gap;
            p.side = side;
            p.frame = idiv16(O.damage, 0x46);  // [BYTES seg7:0x2420-0x242a]
            p.dest_x = add16(mul16(add16(gap, side), static_cast<std::int16_t>(rec)), gap);  // [BYTES seg7:0x2433-0x243c]
            p.dest_y = gap;
            p.src_merge_x = mul16(add16(p.frame, 3), side);  // [BYTES seg7:0x244e-0x2455]
            p.src_and_x = mul16(p.frame, side);              // [BYTES seg7:0x2479-0x247c]
            x.h.icon_crack(p);
            if (d != 0 && ic.w02 == 0) x.h.sound(0x12, 0, rec, 1);  // [BYTES seg7:0x24a6-0x24c0]
        }
        // [BYTES seg7:0x24c8-0x24f8]
        if (d != 0 && (O.damage > 0xd0 || heat() < 0)) {
            icon_shatter(x, rec);
            return 1;
        }
    }
    x.h.column_row(0, rec);  // seg7:0x65aa(0, rec) [BYTES seg7:0x24fa-0x2506]
    return 0;
}

void icon_shatter(CollideCtx& x, int rec) {
    World& w = x.w;
    {
        ObjectRecord& O = obj(w, 0, rec);
        O.damage = -1;  // [BYTES seg7:0x26d8]
        w.dir().set_word(0x1d2 + 2 * static_cast<std::size_t>(rec), O.damage);  // [BYTES seg7:0x26e5-0x26f7]
    }
    x.h.sound(0x1f, 0, rec, 1);  // [BYTES seg7:0x26fc-0x2705]
    icon_fragments(x, rec);      // [BYTES seg7:0x2711]
    obj(w, 0, rec).state = 1;    // [BYTES seg7:0x2719]
    x.h.column_row(0, rec);      // seg7:0x65aa(0, rec) [BYTES seg7:0x2725]
    const ObjectRecord& O = obj(w, 0, rec);
    const std::int16_t side = sprite_side(0x20, x.g.size & 0x1f);  // [BYTES seg7:0x273c-0x2745]
    x.h.erase(O.scr_x, O.scr_y, side, side);  // PatBlt BLACKNESS [BYTES seg7:0x274d]
    x.h.icon_gone(rec, 0);                    // seg7:0x5ae8(rec, 0) [BYTES seg7:0x2758]
}

void icon_fragments(CollideCtx& x, int rec) {
    World& w = x.w;
    const int sz = x.g.size & 0x1f;
    const std::int16_t piece = sprite_side(0x14, sz);  // [bp-0xa] local_c
    const std::int16_t gap = sprite_side(6, sz);       // local_12 = local_e
    const std::int16_t cell = sprite_side(0x20, sz);   // local_10
    icon_at(w, rec).w19 = 1;  // DGROUP:0x57c1 + 0x23·rec [BYTES seg7:0x2799]
    std::array<int, 4> r4{};
    for (int k = 0; k < 4; ++k) {
        int r = 0;
        bool found = false;
        // The first pool-1 record of `state` != 0 is reused [BYTES seg7:0x27b1-0x2840]
        for (; r < w.pools.allocated(1); ++r) {
            ObjectRecord& f = obj(w, 1, r);
            if (f.state != 0) {
                f.state = 0;
                f.damage = 0;
                found = true;
                break;
            }
        }
        if (!found) {
            // Appended: the allocated count r + 1, GlobalReAlloc(GMEM 0x42), the fields of
            // s2-gun-verify.md 60 [BYTES seg7:0x2842-0x2938]
            w.pools.grow_to(1, static_cast<std::int16_t>(r));
            ObjectRecord& f = obj(w, 1, r);
            f.size = piece;
            f.extent = 0xa0;
            f.gap = gap;
            f.mass = 0x3c;
            f.hit_sound = 0x12;
            f.radar_color.set(0x00808000);
            f.max_damage = 0xe;
            f.state = 1;
            f.on_destroy.set(kDestroyPickup);   // seg7:0x2e02
            f.radar_draw.set(kRadarSmall);      // seg11:0x3914
            f.on_damage.set(kDamageGeneric);    // seg7:0x2652
            f.b53 = 0;
            f.damage = 0;
            f.state = 0;
            f.owner_pool = -1;
            f.src_y = f.gap;
            f.b2e = 0;
        }
        // The bitmap DGROUP:0x4e94 + 2r and the maps [BYTES seg7:0x29ce-0x2a20 seg7:0x2c84-0x2cb4]
        x.h.fragment_bitmap(r, add16(add16(gap, gap), piece));
        if (r >= 0 && r < static_cast<int>(w.fragment_icon.size()))
            w.fragment_icon[static_cast<std::size_t>(r)] = static_cast<std::int16_t>(rec);  // DGROUP:0x6038
        if (rec >= 0 && rec < kIconRecords)  // DGROUP:0x57c3 + 0x23·rec + k
            w.icons[static_cast<std::size_t>(rec)].fragment[static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(r);
        r4[static_cast<std::size_t>(k)] = r;
        if (w.pools.in_use(1) <= r) w.pools.set_in_use(1, static_cast<std::int16_t>(r + 1));  // [BYTES seg7:0x2cb6]
    }
    // The pictures [BYTES seg7:0x2a20-0x2c82]
    FragmentPlan p;
    p.icon = rec;
    p.rec = r4;
    p.side = add16(add16(gap, gap), piece);
    p.piece_at = gap;
    p.piece = piece;
    p.quad_at = add16(sprite_side(2, sz), gap);
    p.quad = sub16(piece, sprite_side(4, sz));
    const std::int16_t base = mul16(add16(gap, cell), static_cast<std::int16_t>(rec));
    const std::int16_t three = sprite_side(3, sz);
    p.src_x = {add16(add16(base, three), gap), add16(add16(base, idiv16(cell, 2)), gap),
               add16(add16(base, three), gap), add16(add16(base, gap), piece)};
    p.src_y = {add16(three, gap), add16(three, gap), add16(gap, piece), add16(gap, piece)};
    x.h.fragment_pictures(p);
    // Positions and velocities [BYTES seg7:0x2cc0-0x2df4]
    static constexpr std::int16_t kDx[4] = {0, 176, 0, 176}, kDy[4] = {0, 0, 176, 176};
    static constexpr std::int16_t kVx[4] = {-10, 20, -10, 10}, kVy[4] = {-10, -10, 20, 10};
    for (int k = 0; k < 4; ++k) {
        const ObjectRecord I = obj(w, 0, rec);
        ObjectRecord& f = obj(w, 1, r4[static_cast<std::size_t>(k)]);
        f.x = add16(I.x, kDx[k]);
        f.y = add16(I.y, kDy[k]);
        f.vx = add16(I.vx, kVx[k]);
        f.vy = add16(I.vy, kVy[k]);
    }
}

// ---- seg11:0x1d7c

void auto_repair(World& w, RepairState& r, CollideHost& h) {
    if (r.due >= w.frames) return;  // [BYTES seg11:0x1d8a-0x1d97]
    r.due = w.frames + 180u;        // [BYTES seg11:0x1d9b-0x1da4]
    for (int i = 0; i <= w.last_ship && i < kShipCount; ++i) {
        std::int16_t& cd = r.countdown[static_cast<std::size_t>(i)];
        if (cd <= 0) {  // the reload [BYTES seg11:0x1e75-0x1e97]
            std::int16_t v = add16(idiv16(obj(w, w.roles.ships, i).mass, -75), 9);
            if (v < 1) v = 1;
            cd = v;
            continue;
        }
        ShipRecord& S = w.ships[static_cast<std::size_t>(i)];
        const DamageDesign D = h.design(w, S.design_index);
        // [BYTES seg11:0x1eb2-0x1ee5]
        if (D.repair != 0 && (i == w.player || w.w9130 == 0) && S.fuel > 0x5a) {
            for (int k = 0; k < 4; ++k) {  // [BYTES seg11:0x1db0-0x1e41]
                if (D.lamp_row[static_cast<std::size_t>(k)] == -1) continue;
                std::int16_t& hv = sys(S, k);
                if (hv >= 0x52) continue;
                if (k == 3 && (hv == 0 || hv == 0x32)) continue;
                hv = add16(hv, (i == w.player && k != 3) ? 2 : 1);
                if (w.kind == 6) hv = add16(hv, 1);
                S.fuel = sub16(S.fuel, 2);
            }
        }
        // The gun's extra interval [BYTES seg11:0x1e44-0x1e66]
        std::int16_t t = shl16(sub16(0x4b, S.w28), 2);
        if (t < 0x1b) t = 0x1b;
        S.d8c.set(static_cast<std::uint32_t>(static_cast<std::int32_t>(idiv16(t, 0x1b))));
        cd = sub16(cd, 1);  // [BYTES seg11:0x1e6f]
    }
}

}  // namespace is::world
