// The launched weapons (missile.h). M4 stage 2, track MISSILE.
#include "world/missile.h"

#include <cstddef>

namespace is::world {

namespace ww = wofs;

MissileState& missile_state() {
    static MissileState s;
    return s;
}

namespace {

// The heading table's entry for f; a heading outside 0 ... 15 reads past DGROUP:0x1050 in the
// original, the port wraps it [DESIGN].
std::int16_t hx(int f) { return heading_dx(f & 15); }
std::int16_t hy(int f) { return heading_dy(f & 15); }

bool counted_pool(int pool) {
    // [BYTES seg7:0x208d-0x20cd] (verified claim 37's list: 1, 2, 7, 5, 11, 9, 10, 4, 6, 8, 0x13)
    switch (pool) {
    case 1: case 2: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 19:
        return true;
    default:
        return false;
    }
}


}  // namespace


// ---------------------------------------------------------------- seg7:0x2080

void recount_in_use(Pools& p, int pool) {
    if (!counted_pool(pool)) return;  // [BYTES seg7:0x208d-0x20cd]
    // k = DGROUP:0x92ec[pool], down to 1, testing record k - 1 [BYTES seg7:0x20d4-0x210c]
    for (int k = p.allocated(pool); k > 0; --k) {
        if (k - 1 >= p.records(pool)) continue;  // [DESIGN] storage bound
        const ObjectRecord& r = p.rec(pool, k - 1);
        if (r.state == 0 || r.race_status != 0) {
            p.set_in_use(pool, static_cast<std::int16_t>(k));
            return;
        }
    }
    p.set_in_use(pool, 0);  // [BYTES seg7:0x211b]
}

// ---------------------------------------------------------------- seg8:0x1cbe

void clamp_velocity(Pools& p, std::int16_t limit, std::int16_t pool_rec) {
    const int pool = pool_of(pool_rec), rec = rec_of(pool_rec);  // high byte, low byte, unsigned
    if (pool >= kPoolCount || rec >= p.records(pool)) return;    // [DESIGN] storage bound
    ObjectRecord& o = p.rec(pool, rec);
    const std::int16_t neg = sub16(0, limit);
    if (o.vx > limit) o.vx = limit;  // [BYTES seg8:0x1ccc-0x1cf1]
    if (o.vy > limit) o.vy = limit;  // [BYTES seg8:0x1d06-0x1d0c]
    if (o.vx < neg) o.vx = neg;      // [BYTES seg8:0x1d23-0x1d29]
    if (o.vy < neg) o.vy = neg;      // [BYTES seg8:0x1d40-0x1d46]
}

// ---------------------------------------------------------------- seg13:0xd6c

std::int16_t turn_toward(const TurnArgs& a, std::int16_t wave_kind, const ShipTable& ships,
                         const std::function<void(int ship)>& touched) {
    std::int16_t cur = a.cur;
    // A ship: byte +0xeb, or ship word +0x10 in 1 ... 6, keeps the heading [BYTES seg13:0xd7a-0xda3]
    if (a.ship > -1 && a.ship < kShipCount) {
        const ShipRecord& s = ships[static_cast<std::size_t>(a.ship)];
        if (s.beb != 0) return cur;
        if (s.type > 0 && s.type < 7) return cur;
    }
    // [BYTES seg13:0xda5-0xdb4]
    const std::int16_t my = sar16(a.my, 3), ty = sar16(a.ty, 3);
    const std::int16_t tx = sar16(a.tx, 3), mx = sar16(a.mx, 3);
    // The world wrap by DGROUP:0x5a0 / 0x5a2 outside a race wave [BYTES seg13:0xdb8-0xe2c]
    const std::int16_t half = sar16(kWorldCells, 1);
    std::int16_t dx = sub16(tx, mx);
    if (abs16(dx) > half && wave_kind != 6)
        dx = sub16(dx < 0 ? add16(tx, kWorldCells) : sub16(tx, kWorldCells), mx);
    std::int16_t dy = sub16(ty, my);
    if (abs16(dy) > half && wave_kind != 6)
        dy = sub16(dy < 0 ? add16(ty, kWorldCells) : sub16(ty, kWorldCells), my);
    std::int16_t h = 0;
    switch (a.mode) {
    case 2:
        // The race rules (seg8:0x19b0 then seg13:0xe54-0xef1) and seg8:0x2658 are not read
        // [OPEN M6] (s2-missile.md O1): the heading stays [BYTES seg13:0xe37 seg13:0xef4 seg13:0xefa].
        if (wave_kind == 6 || a.a0 != 0) return cur;
        h = direction16(dy, dx);  // [BYTES seg13:0xf27 seg13:0xf7f]
        break;
    case 4:  // [BYTES seg13:0xf2d-0xf48 seg13:0xf7c]
        h = direction16(sub16(0, add16(sar16(dy, 2), dx)), sub16(0, sub16(sar16(dx, 2), dy)));
        break;
    case 3:  // [BYTES seg13:0xf4a-0xf65 seg13:0xf7c]
        h = direction16(sub16(0, sub16(sar16(dy, 2), dx)), sub16(0, add16(sar16(dx, 2), dy)));
        break;
    case 5:
    case 0x1e:  // [BYTES seg13:0xf67-0xf7f]
        h = direction16(sub16(0, dy), sub16(0, dx));
        break;
    default:
        // An uninitialised local in the original; the port keeps the heading
        // [DESIGN ADR 0016 decision 1].
        return cur;
    }
    if (h < 0 || h > 15) return cur;                  // [BYTES seg13:0xf87-0xf93]
    if (a.mode == 0x1e || a.ship == -100) return h;  // [BYTES seg13:0xf96-0xfa5]
    if (h == cur) return h;                           // [BYTES seg13:0xfa7-0xfad]
    // seg13:0x6716(ship) [BYTES seg13:0xfaf-0xfb6]; for ship -1 it writes DGROUP:0x708c, which
    // the port skips [DESIGN ADR 0016 decision 3] (s2-missile.md 4.4).
    if (a.ship >= 0 && touched) touched(a.ship);
    if (h < cur) {  // [BYTES seg13:0xfb7-0xfdd]
        if (sub16(cur, h) < 9) {
            cur = sub16(cur, 1);
        } else {
            cur = add16(cur, 1);
            if (cur > 15) cur = 0;
        }
    } else {  // [BYTES seg13:0xfe0-0xffd]
        if (sub16(h, cur) < 9) {
            cur = add16(cur, 1);
        } else {
            cur = sub16(cur, 1);
            if (cur < 0) cur = 15;
        }
    }
    return cur;
}

// ---------------------------------------------------------------- seg7:0x2652

std::int16_t generic_on_damage(Pools& p, int pool, int rec, std::int16_t a, MissileHost& h) {
    if (pool == 15) return 0;  // [BYTES seg7:0x265e-0x2666]
    if (pool < 0 || pool >= kPoolCount || rec < 0 || rec >= p.records(pool)) return 0;  // [DESIGN]
    ObjectRecord& o = p.rec(pool, rec);
    if (o.damage > o.max_damage && pool != 9) return 0;  // [BYTES seg7:0x2678-0x2686]
    o.damage = add16(o.damage, a);                       // [BYTES seg7:0x2699]
    if (o.damage > o.max_damage) {                       // [BYTES seg7:0x26a3-0x26b8]
        h.on_destroy(pool, rec);
        return 1;
    }
    return 0;
}

void missile_destroyed(Pools& p, int m, const WeaponWords& words) {
    MissileSlot& e = p.missile(m);
    e.age = words.word(e.weapon, ww::kLifetime);  // [BYTES seg12:0x2af5 seg12:0x2af9]
    e.weapon = -1;                                // [BYTES seg12:0x2afd]
}

// ---------------------------------------------------------------- seg11:0xac86

void weapon_system(MissileCtx& c) {
    World& w = c.w;
    const WeaponWords& W = c.st.words;
    const int pl = c.g.player;
    ShipRecord& s = ship_at(w, pl);
    if (s.bea != 0 || s.be8 != 0) return;               // [BYTES seg11:0xac97 seg11:0xac9e]
    if (obj(w, c.g.ship_pool, pl).b2e != 0) return;     // [BYTES seg11:0xacb5]
    // w = (int8) byte +0xb8 + word +0x18 (cbw) [BYTES seg11:0xacc5 seg11:0xacc9 seg11:0xaccd]
    int wpn = sbyte_at(s, 0xb8 + s.weapon_slot);
    // Claim 7 as corrected [BYTES seg11:0xacd1 seg11:0xacd8 seg11:0xacdf]
    if (W.word(wpn, ww::kAggression) == 0 && s.w32 != 9) w.w9432 = add16(w.w9432, 3);
    c.h.capture_count(pl, 4);    // seg13:0x4eec(player, 4) [BYTES seg11:0xace4-0xacef]
    c.w.w8e28 = c.g.now;         // DGROUP:0x8e28, World::w8e28 [BYTES seg11:0xacf2 seg11:0xacfa]
    wpn = sbyte_at(s, 0xb8 + s.weapon_slot);  // re-read [BYTES seg11:0xacfe-0xad0d]
    if (W.word(wpn, ww::kKind) != 0) {        // [BYTES seg11:0xad10 seg11:0xad17-0xad24]
        launch(c, pl, c.g.ship_pool, wpn);
        return;
    }
    // The switch on w - 1, unsigned > 0x16 returns (w = 0 too) [BYTES seg11:0xadac-0xadba]
    const unsigned k = static_cast<std::uint16_t>(wpn - 1);
    if (k > 0x16) return;
    switch (k) {
    case 0: c.h.unported("seg11:0x8a42", wpn, 0); break;     // RING OF DEATH (M4-later)
    case 1: c.h.unported("seg11:0x947a", wpn, 2); break;     // GRAPPLING HOOK (M4-later)
    case 2: c.h.unported("seg11:0x9fdc", wpn, 0); break;     // LIGHTNING (M4-later)
    case 3: c.h.unported("seg13:0x1318", wpn, 4); break;     // (4, 0x1e, player, 14) (M4-later)
    case 4: c.h.unported("seg13:0x1318", wpn, 5); break;     // (5, 0x1e, player, 14) (M4-later)
    case 12: c.h.unported("seg11:0x9078", wpn, 0); break;    // WILDCARD (M4-later)
    case 14: c.h.unported("seg11:0x947a", wpn, 0xf); break;  // HANDCUFFS (M4-later)
    case 22: c.h.unported("seg11:0x9daa", wpn, 0); break;    // UNCERTAINTY (M6)
    default: break;
    }
}

// ---------------------------------------------------------------- seg11:0x7c12

namespace {

// The template of a grown record [BYTES seg11:0x7c86-0x7d37] (verified claim 12).
void grown_template(Pools& p, int m, int w) {
    ObjectRecord& r = p.rec(kPoolMissiles, m);
    r.state = 1;
    r.mass = 20;
    r.hit_sound = 100;
    r.radar_color.set(0x00007f00);
    r.max_damage = -1;
    r.blast_size = 5;
    r.radar_draw.set(kRadarSmall);
    r.on_damage.set(kDamageGeneric);
    r.b53 = w == 16 ? 1 : 0;
    r.b54 = 0;
    r.owner_pool = -1;
    r.src_y = r.gap;  // 0 in the zeroed record
    r.b2e = 0;
    p.missile(m).weapon = -1;
}

// seg11:0x86d4-0x8807, the front launch [BYTES seg11:0x86d4-0x8807] (claims 26-28).
void front_launch(MissileCtx& c, ObjectRecord& M, const ObjectRecord& L, int s, int pool) {
    const std::int16_t E = L.extent, e = M.extent;
    const int f = L.frame;
    const std::int16_t lo = idiv16(sub16(E, e), 2);    // cwd; sub ax, dx; sar ax, 1
    const std::int16_t mid = idiv16(add16(E, e), 2);
    // imul, then dx is overwritten by 15: the low word, cwd, idiv [BYTES seg11:0x8726-0x8739]
    M.x = add16(M.x, add16(lo, idiv16(mul16(mid, hx(f)), 15)));  // [BYTES seg11:0x8740]
    M.y = add16(M.y, add16(lo, idiv16(mul16(mid, hy(f)), 15)));  // [BYTES seg11:0x87a2]
    M.vx = shl16(hx(f), 2);  // [BYTES seg11:0x87b7-0x87c5]
    M.vy = shl16(hy(f), 2);  // [BYTES seg11:0x87d8-0x87df]
    if (pool == c.g.ship_pool)  // dword +0xa0 = frame# + 36: the gun waits 41 frames [BYTES seg11:0x87e3-0x8803]
        set_ship_dword(ship_at(c.w, s), 0xa0, c.g.game_frames + 0x24u);
}

}  // namespace

void launch(MissileCtx& c, int s, int pool, int w) {
    World& wd = c.w;
    Pools& p = wd.pools;
    const WeaponWords& W = c.st.words;
    auto Ww = [&](int off) { return W.word(w, off); };
    const bool ship_launcher = pool == c.g.ship_pool;
    if (p.in_use(kPoolMissiles) >= kMissileCap) return;  // [BYTES seg11:0x7c2b seg11:0x7c30]
    int m = -1;
    bool launched = false, ok = false;
    int slot = 0;  // [bp-0xa]
    for (;;) {
        m += 1;  // [BYTES seg11:0x7c34]
        if (m >= kMissileSlots) break;  // [DESIGN] the table bound; the cap keeps m < 10
        if (m >= p.allocated(kPoolMissiles)) {  // [BYTES seg11:0x7c3a seg11:0x7c3d]
            p.grow_to(kPoolMissiles, static_cast<std::int16_t>(m));  // GlobalReAlloc, zeroed [BYTES seg11:0x7c45-0x7c7a]
            grown_template(p, m, w);
            p.set_in_use(kPoolMissiles, static_cast<std::int16_t>(m + 1));  // [BYTES seg11:0x7d41]
        }
        {
            // The free test [BYTES seg11:0x7d44-0x7d8b] (claim 13)
            ObjectRecord& M = p.rec(kPoolMissiles, m);
            MissileSlot& E = p.missile(m);
            bool keep = false;
            if (M.state == 0 &&
                (W.word(E.weapon, ww::kLifetime) > E.age || W.word(E.weapon, ww::kEnd) == 6) &&
                E.age != 0)
                keep = true;
            if (!keep && M.race_status == 0) E.weapon = -1;
        }
        // The ammunition [BYTES seg11:0x7d91-0x7e01] (claim 14, display claim 22 as corrected)
        ok = false;
        if (ship_launcher) {
            ShipRecord& S = ship_at(wd, s);
            const int n = static_cast<std::int8_t>(S.slots_cb());
            for (int l = 1; l < n; ++l) {
                if (sbyte_at(S, 0xb8 + l) != w) continue;
                if (sbyte_at(S, 0xae + l) > 0 && Ww(ww::kFuel) == 0) {
                    ok = true;
                } else if (Ww(ww::kFuel) > 0 && (pool != 14 || Ww(ww::kFuel) < S.fuel)) {
                    ok = true;  // the literal 14 [BYTES seg11:0x7ddf]
                }
                slot = l;
                break;
            }
        } else {
            ok = true;  // [BYTES seg11:0x7d9c seg11:0x7e01]
        }
        if (p.missile(m).weapon != -1 || !ok) {  // [BYTES seg11:0x7e0a seg11:0x7e13]
            if (!launched && ok) continue;      // NEXT [BYTES seg11:0x8a05-0x8a0f]
            break;
        }
        launched = true;  // [BYTES seg11:0x7e1b]
        // The refusal: a missile of this launcher at 0 < age < 15 [BYTES seg11:0x7e20-0x7e5d]
        for (int k = 0; k < p.in_use(kPoolMissiles) && k < kMissileSlots; ++k) {
            const MissileSlot& e = p.missile(k);
            if (e.age > 0 && e.age < 15 && e.launcher_pool == pool && e.launcher_rec == s &&
                e.weapon != -1) {
                return;  // to seg11:0x8a35: no seg7:0x2080 [BYTES seg11:0x8a35]
            }
        }
        // The spend [BYTES seg11:0x7e60-0x7e96] (claim 17)
        if (ship_launcher) {
            ShipRecord& S = ship_at(wd, s);
            if (Ww(ww::kFuel) == 0) {
                set_byte_at(S, 0xae + slot, sbyte_at(S, 0xae + slot) - 1);  // [BYTES seg11:0x7e7b]
            } else {
                const std::int16_t f = sub16(S.fuel, Ww(ww::kFuel));
                S.fuel = f < 0 ? std::int16_t{0} : f;  // jns [BYTES seg11:0x7e8e-0x7e96]
            }
        }
        c.h.paint_weapons();  // every launcher [BYTES seg11:0x7e9a-0x7ea6] (claim 18)
        ObjectRecord& M = p.rec(kPoolMissiles, m);
        MissileSlot& E = p.missile(m);
        // [BYTES seg11:0x7eb1-0x7ec4] (claim 19)
        E.age = 0;
        E.w18 = 0;
        E.weapon = static_cast<std::int16_t>(w);
        E.anim = 0;
        E.mode = 2;
        // [BYTES seg11:0x7ed2-0x7ef5] (claim 20)
        M.scr_x = M.scr_y = M.old_scr_x = M.old_scr_y = -1000;
        // The target [BYTES seg11:0x7efb-0x7f5d] (claims 21, 22)
        if (Ww(ww::kKind) == 2 && ship_launcher) {
            ShipRecord& S = ship_at(wd, s);
            if (s != c.g.player || c.g.demo == 0)
                c.h.choose_target(s, static_cast<std::uint16_t>(S.w32) |
                                         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(S.w34)) << 16));
            E.target_pool = S.target_pool;
            E.target_rec = S.target_rec;
            E.lock = 0;
        } else {
            E.target_pool = -1;
            E.lock = 0;
        }
        M.state = 0;  // [BYTES seg11:0x7f69]
        M.b2e = 0;    // [BYTES seg11:0x7f73]
        // The cell [BYTES seg11:0x7f78-0x7fe7] (claim 23)
        const std::int16_t cell = Ww(ww::kCell);
        const int sz = c.g.size;
        M.size = sar16(shl16(cell, sz), 1);
        std::int16_t gap = 0;
        if ((cell & 1) != 0) {
            M.size = sar16(shl16(add16(cell, 1), sz), 1);
            gap = sar16(shl16(4, sz), 1);
        } else {
            gap = sar16(shl16(6, sz), 1);
        }
        M.gap = gap;
        M.src_y = M.gap;
        // [BYTES seg11:0x7ff5-0x8031] (claim 24)
        M.extent = shl16(cell, 3);
        M.blast_size = Ww(ww::kBlast);
        M.b50 = 0;
        M.mass = Ww(ww::kMass);
        M.max_damage = Ww(ww::kMaxDamage);
        M.damage = 0;
        const ObjectRecord& L = obj(wd, pool, s);
        M.frame = L.frame;  // [BYTES seg11:0x804b-0x8080]
        M.x = L.x;
        M.y = L.y;
        if (Ww(ww::kGraphic) == -1) c.h.unported("seg11:0x80c5", w, m);  // weapon 22 (M6)
        // [BYTES seg11:0x80e1-0x8113]
        M.on_destroy.set(Ww(ww::kEnd) == 4 || Ww(ww::kEnd) == 6 ? kDestroyPickup : kDestroyGeneric);
        if (w == 16) {  // [BYTES seg11:0x8119-0x8181]
            M.b53 = 1;
            c.h.unported("seg11:0x8119", w, m);  // the team-7 ships chase it (M4-later)
        } else {
            M.b53 = 0;
        }
        // The placement [BYTES seg11:0x818a seg11:0x86bf-0x8815] (claim 25)
        if (Ww(ww::kKind) == 5)
            c.h.unported("seg11:0x8193", w, m);  // the kind-5 aim (M4-later)
        else if (Ww(ww::kLaunch) == 1 || !ship_launcher)
            front_launch(c, M, L, s, pool);
        else if (Ww(ww::kLaunch) == 2)
            c.h.unported("seg11:0x881d", w, m);  // the rear launch (M4-later)
        // [BYTES seg11:0x897d-0x89d2] (claim 29)
        E.age = 0;
        if (Ww(ww::kKind) != 5) {
            M.vx = add16(M.vx, L.vx);
            M.vy = add16(M.vy, L.vy);
        }
        E.launcher_pool = static_cast<std::int16_t>(pool);
        E.launcher_rec = static_cast<std::int16_t>(s);
        // The sounds [BYTES seg11:0x89d6-0x8a02] (claim 30)
        if (ship_launcher && s == c.g.player) c.h.sound_reset();  // seg10:0x744c(0x69)
        c.h.sound_7944(Ww(ww::kSound), kPoolMissiles, m, 1);
        break;  // launched: NEXT falls out [BYTES seg11:0x8a05]
    }
    recount_in_use(p, kPoolMissiles);  // seg7:0x2080(6, -1) [BYTES seg11:0x8a13-0x8a17]
    if (p.rec(kPoolMissiles, 0).state == 0 && p.missile(0).weapon == -1)  // [BYTES seg11:0x8a1f-0x8a31]
        p.bad_index_count = add16(p.bad_index_count, 1);
}

// ---------------------------------------------------------------- seg12:0x586

void retarget(MissileCtx& c, std::int16_t range, int m) {
    World& w = c.w;
    Pools& p = w.pools;
    const WeaponWords& W = c.st.words;
    MissileSlot& E = p.missile(m);
    const int id = E.weapon;  // [BYTES seg12:0x595-0x59d]
    std::int16_t best = kRetargetBest;
    int bp_pool = -1, bp_rec = 0;
    if (c.g.demons_gate != 0 && id == 0x16) return;  // [BYTES seg12:0x5aa-0x5b6]
    if (W.word(id, ww::kKind) == 2) {                // [BYTES seg12:0x5bc-0x5ef]
        if (obj(w, E.target_pool, E.target_rec).state == 0) E.lock = 1;
        return;
    }
    const ObjectRecord& M = obj(w, kPoolMissiles, m);
    const int ships = c.g.ship_pool;
    for (int pl = 0; pl <= ships && pl < kPoolCount; ++pl) {  // [BYTES seg12:0x74e-0x777]
        if (W.word(id, ww::kW14) == 1 && pl == c.g.pool5) continue;
        if (pl == 2) continue;
        for (int r = 0; r < p.in_use(pl) && r < p.records(pl); ++r) {  // [BYTES seg12:0x635-0x643]
            if (pl == kPoolMissiles && r == m) continue;                 // [BYTES seg12:0x646-0x64f]
            const ObjectRecord& C = p.rec(pl, r);
            if (C.state != 0 || C.mass <= 30) continue;                  // [BYTES seg12:0x660-0x66c]
            if (W.word(id, ww::kKind) != 3 && C.flame == 0) continue;    // [BYTES seg12:0x672-0x67e]
            std::int16_t dx = 0, dy = 0;
            std::int16_t d = distance_cells(C.x, C.y, M.x, M.y, dx, dy);  // [BYTES seg12:0x680-0x6b5]
            if (pl == E.launcher_pool && r == E.launcher_rec) d = shl16(d, 2);  // [BYTES seg12:0x6bb-0x6ce]
            // Both ships of one team outside Demon's Gate: the low word of 7 d [BYTES seg12:0x6d2-0x712]
            if (E.launcher_pool == ships && pl == ships && c.g.demons_gate == 0 &&
                c.h.team(w, ship_at(w, r).design_index) ==
                    c.h.team(w, ship_at(w, E.launcher_rec).design_index))
                d = mul16(7, d);
            if (W.word(id, ww::kKind) == 4) {  // [BYTES seg12:0x715-0x74b]
                d = add16(d, static_cast<std::int16_t>(static_cast<std::int8_t>(0xe7) *
                                                        static_cast<std::int8_t>(C.flame)));
                if (pl == c.g.pool5) d = idiv16(d, 2);
            } else if (pl == ships) {  // add ax, ax; cwd; idiv 3 [BYTES seg12:0x5fa-0x60d]
                d = idiv16(add16(d, d), 3);
            }
            if (d < range && d < best) {  // signed [BYTES seg12:0x610-0x62f]
                best = d;
                bp_pool = pl;
                bp_rec = r;
            }
        }
    }
    // [BYTES seg12:0x77a-0x7b1] (claim 56)
    if (bp_pool == -1) return;
    if (bp_pool == E.launcher_pool && bp_rec == E.launcher_rec && E.age <= 60) return;
    E.lock = 1;
    E.target_pool = static_cast<std::int16_t>(bp_pool);
    E.target_rec = static_cast<std::int16_t>(bp_rec);
}

// ---------------------------------------------------------------- seg12:0x7c0

void steer(MissileCtx& c, int m) {
    World& w = c.w;
    Pools& p = w.pools;
    const WeaponWords& W = c.st.words;
    MissileSlot& E = p.missile(m);
    const int id = E.weapon;
    std::int16_t jit = 0;  // [bp-0x2e]
    ObjectRecord& M = obj(w, kPoolMissiles, m);
    // The missile's centre [BYTES seg12:0x7eb-0x802]
    const std::int16_t mx = add16(sar16(M.extent, 1), M.x);
    const std::int16_t my = add16(sar16(M.extent, 1), M.y);
    // The target's centre: `size` (pixels) / 2 on the world point [BYTES seg12:0x805-0x86f]
    bool alt = false;
    std::int16_t tx = 0, ty = 0;
    if (E.mode == 2) {
        const ObjectRecord& T = obj(w, E.target_pool, E.target_rec);
        tx = add16(sar16(T.size, 1), T.x);
        ty = add16(sar16(T.size, 1), T.y);
    } else {
        const ObjectRecord& T = obj(w, E.alt_pool, E.alt_rec);
        tx = add16(sar16(T.size, 1), T.x);
        ty = add16(sar16(T.size, 1), T.y);
        alt = true;
    }
    // A cloaked ship target jitters [BYTES seg12:0x874-0x8a8]
    if (E.target_pool == c.g.ship_pool && !alt && ship_at(w, E.target_rec).be8 != 0 &&
        W.word(id, ww::kCloak) == 0)
        jit = 1;
    // Two rand() on every call, x then y [BYTES seg12:0x8ad-0x8dc] (claim 58)
    const std::int16_t r1 = static_cast<std::int16_t>(c.rng.rand());
    tx = add16(tx, shl16(mul16(sub16(imod16(r1, 300), 150), jit), 3));
    const std::int16_t r2 = static_cast<std::int16_t>(c.rng.rand());
    ty = add16(ty, shl16(mul16(sub16(imod16(r2, 300), 150), jit), 3));
    if (jit != 0) E.lock = 0;  // [BYTES seg12:0x8df-0x8e9]
    // The turn [BYTES seg12:0x8ef-0x95c] (claim 59)
    std::int16_t e = 0;
    const std::function<void(int)> touched = [&c](int ship) { c.h.ship_touched(ship); };
    if (W.word(id, ww::kAnim) == 2) {
        TurnArgs a;
        a.a0 = 0, a.a1 = -1, a.a2 = -1, a.ship = -1, a.mode = E.mode, a.cur = M.frame;
        a.tx = tx, a.ty = ty, a.mx = mx, a.my = my;
        M.frame = turn_toward(a, c.g.wave_kind, w.ships, touched);
        e = M.frame;
    } else {
        TurnArgs a;
        a.a0 = 0, a.a1 = -100, a.a2 = -100, a.ship = -100, a.mode = E.mode, a.cur = 0;
        a.tx = tx, a.ty = ty, a.mx = mx, a.my = my;
        e = turn_toward(a, c.g.wave_kind, w.ships, touched);
    }
    // The proximity fuse [BYTES seg12:0x95f-0x9b8] (claim 60): no return afterwards
    if (W.word(id, ww::kFuse) != 0 && jit == 0) {
        std::int16_t dx = 0, dy = 0;
        const std::int16_t d = distance_cells(tx, ty, mx, my, dx, dy);
        if (W.word(id, ww::kFuse) > d) {
            M.blast_size = shl16(M.blast_size, 1);
            c.h.destroy(kPoolMissiles, m);       // seg12:0x274a(6, m)
            recount_in_use(p, kPoolMissiles);    // seg7:0x2080(6, m)
        }
    }
    // The thrust [BYTES seg12:0x9bb-0xa02] (claim 61)
    std::int16_t ax = mul16(hx(e), W.word(id, ww::kThrust));
    std::int16_t ay = mul16(hy(e), W.word(id, ww::kThrust));
    if (alt) {
        ax = add16(ax, ax);
        ay = add16(ay, ay);
    }
    ObjectRecord& M2 = obj(w, kPoolMissiles, m);
    M2.vx = add16(M2.vx, ax);
    M2.vy = add16(M2.vy, ay);
    clamp_velocity(p, W.word(id, ww::kClamp), pool_rec(kPoolMissiles, m));  // [BYTES seg12:0xa06-0xa16]
}

// ---------------------------------------------------------------- seg12:0x0

void move_pool6(MissileCtx& c, UpdateCtx& u) {
    World& w = c.w;
    Pools& p = w.pools;
    const WeaponWords& W = c.st.words;
    const int ships = c.g.ship_pool;
    // DGROUP:0x9412 re-read every pass [BYTES seg12:0x519-0x522]
    for (int m = 0; m < p.in_use(kPoolMissiles) && m < p.records(kPoolMissiles) && m < kMissileSlots; ++m) {
        MissileSlot& E = p.missile(m);
        const int id = E.weapon;  // [bp-8]
        auto Wd = [&](int off) { return W.word(id, off); };
        const std::int16_t pr = pool_rec(kPoolMissiles, m);
        {
            // The run condition [BYTES seg12:0x524-0x556] (claim 38)
            ObjectRecord& M = p.rec(kPoolMissiles, m);
            const bool run = (Wd(ww::kLifetime) > E.age || Wd(ww::kEnd) == 6) && id != -1 &&
                             M.state == 0;
            if (!run) {
                if (M.race_status != 0) c.h.unported("seg12:0x129e", kPoolMissiles, m);  // [BYTES seg12:0x4fe-0x516]
                continue;
            }
            // 1. The animation by W+0x06 [BYTES seg12:0x558-0x579] (claim 39)
            switch (Wd(ww::kAnim)) {
            case 1:  // [BYTES seg12:0x17-0x78]
                E.anim = add16(E.anim, 1);
                if (E.anim > Wd(ww::kSpin)) {
                    E.anim = 0;
                    if (Wd(ww::kSpin) == -1) M.frame = add16(M.frame, 2);
                    M.frame = add16(M.frame, 1);
                    if (M.frame > 15) {
                        M.frame = sub16(M.frame, 16);
                        if (Wd(ww::kSound) == 2) c.h.sound(2, kPoolMissiles, m, 1);
                    }
                }
                break;
            case 4:  // [BYTES seg12:0x2b1-0x2e6]
                M.frame = E.age < 50 ? idiv16(mul16(E.age, Wd(ww::kFrames)), 50)
                                     : sub16(Wd(ww::kFrames), 1);
                break;
            case 5:  // [BYTES seg12:0x2e8-0x2f6]
                M.frame = 0;
                break;
            default:  // 2, 3 and the others: nothing [BYTES seg12:0x569]
                break;
            }
        }
        // 2. [BYTES seg12:0x7b-0x94]
        if (Wd(ww::kW38) != 0 && (E.age & 3) == 0) c.h.unported("seg4:0x5514", kPoolMissiles, m);
        // 3. The retarget [BYTES seg12:0x99-0xe5] (claim 40)
        if (imod16(E.age, 13) == 0 && E.age > 8 && Wd(ww::kKind) != 1 && Wd(ww::kKind) != 5 &&
            (Wd(ww::kRetarget) != 0 || E.target_pool == -1 || E.age == 13))
            retarget(c, shl16(E.age, 3), m);
        // 4. The unlocked half-life [BYTES seg12:0xe8-0x10b] (claim 41)
        if (E.lock == 0 && idiv16(Wd(ww::kLifetime), 2) == E.age) E.age = sub16(Wd(ww::kLifetime), 5);
        // 5. The off-screen expiry [BYTES seg12:0x10f-0x13c] (claim 42)
        if (Wd(ww::kOffscreen) == 0 && p.rec(kPoolMissiles, m).on_screen == 0 && E.age > 20)
            E.age = Wd(ww::kLifetime);
        // 6. The steer [BYTES seg12:0x140-0x164] (claim 43)
        if (E.lock != 0 && (Wd(ww::kKind) != 1 || E.age > E.steer_delay)) steer(c, m);
        ObjectRecord& M = p.rec(kPoolMissiles, m);
        // 7. The drag [BYTES seg12:0x167-0x1cc]
        if (Wd(ww::kDrag) != 0) {
            const int n = Wd(ww::kDrag) & 0x1f;  // byte W+0x40 as cl, masked as a 386 does
            M.vx = sub16(M.vx, static_cast<std::int16_t>(sar32(M.vx, n)));
            M.vy = sub16(M.vy, static_cast<std::int16_t>(sar32(M.vy, n)));
            if (M.vx > -10 && M.vx < 10) M.vx = 0;
            if (M.vy > -10 && M.vy < 10) M.vy = 0;
        }
        // 8. [BYTES seg12:0x1d2-0x1fa]
        if (Wd(ww::kEnd) == 5 && sub16(Wd(ww::kLifetime), 8) < E.age) u.host.puff(kPoolMissiles, m, 1);
        // 9. [BYTES seg12:0x1fd-0x233]
        if (Wd(ww::kW14) != 0) {
            if (Wd(ww::kW14) == 3) {
                int b = static_cast<std::int8_t>(static_cast<std::uint8_t>(M.b50 + 4));
                if (b > 0x4b) b = 0x4b;
                M.b50 = static_cast<std::uint8_t>(b);
            }
            c.h.unported("seg12:0x64aa", id, m);
        }
        // 10. seg8:0x1b54(1, 0x600 | m) [BYTES seg12:0x236-0x240] (claim 44)
        integrate(u.flight, 1, pr);
        // 11. The cell [BYTES seg12:0x245-0x265]
        ObjectRecord& M3 = p.rec(kPoolMissiles, m);
        M3.src_x = add16(mul16(add16(M3.size, M3.gap), M3.frame), M3.gap);
        // 12. The draw, claim 45 as corrected [BYTES seg12:0x26f-0x313]
        if (M3.state == 0) {
            if (Wd(ww::kGraphic) == -1) {
                const Point16 s = c.h.to_screen(M3.x, M3.y);  // seg8:0x29ea [BYTES seg12:0x284-0x2ab]
                M3.scr_x = s.x;
                M3.scr_y = s.y;
            } else {
                u.host.draw_sprite(pr, 3, Wd(ww::kSlot), 0);  // seg8:0xb76(0x600|m, 3, DGROUP:0x39e8[W-4], 0)
            }
        }
        // 13. The collision [BYTES seg12:0x318-0x424] (claims 46-48)
        if (p.rec(kPoolMissiles, m).on_screen != 0 && E.age > 3) {
            u.f.dg_943a = kPoolMissiles;  // [BYTES seg12:0x332]
            u.f.dg_943c = static_cast<std::int16_t>(m);  // [BYTES seg12:0x33b]
            CollideGlobals& cg = u.host.collide_globals();
            CollideCtx x{w,      c.g,      cg,         u.host.collide_host(),
                         u.view, u.host.visible_list(), u.flight.rng, u.f.dg_943a,
                         u.f.dg_943c, u.f.dg_9454};
            const std::uint16_t hit = find_pair(x, pr);  // seg8:0x50aa [BYTES seg12:0x345]
            if (hit != 0xffffu) {
                ObjectRecord& K = p.rec(kPoolMissiles, m);
                // seg8:0x546c with `extent`, not extent + 2 [BYTES seg12:0x353-0x376]
                const std::int16_t a = respond(x, K.mass, K.extent, &K.vy, &K.vx, hit);
                const int hp = hit >> 8, hr = hit & 0xff;
                ObjectRecord& K2 = p.rec(kPoolMissiles, m);
                if (K2.b53 != 0 && hp == ships && ship_at(w, hr).bea == 0) {
                    c.h.touch(hr, pr);  // seg13:0x16ac [BYTES seg12:0x382-0x3ab]
                } else if (Wd(ww::kEnd) == 6 && hp == ships &&
                           W.word(sbyte_at(ship_at(w, hr), 0xb8 + ship_at(w, hr).weapon_slot), ww::kEnd) == 6) {
                    // [BYTES seg12:0x3ad-0x40b] (M4-later: the boomerang caught)
                    ShipRecord& S = ship_at(w, hr);
                    K2.b53 = 1;
                    c.h.touch(hr, pr);
                    set_byte_at(S, 0xae + S.weapon_slot, sbyte_at(S, 0xae + S.weapon_slot) + 1);
                } else {
                    dispatch(x, kPoolMissiles, m, hp, hr, a);  // seg8:0x5df2 [BYTES seg12:0x40d-0x424]
                }
            }
        }
        // 14. The age and the end [BYTES seg12:0x427-0x4f6] (claim 49)
        if (p.rec(kPoolMissiles, m).state != 0) continue;
        E.age = add16(E.age, 1);
        if (E.age < Wd(ww::kLifetime)) continue;
        switch (Wd(ww::kEnd)) {
        case 1:
            c.h.destroy(kPoolMissiles, m);     // seg12:0x274a(6, m)
            recount_in_use(p, kPoolMissiles);  // seg7:0x2080(6, m)
            break;
        case 4:
            c.h.unported("seg7:0x2e02", kPoolMissiles, m);
            recount_in_use(p, kPoolMissiles);
            break;
        case 5:
            c.h.unported("seg12:0x6a60", kPoolMissiles, m);
            break;
        case 6:  // the boomerang turns home [BYTES seg12:0x490-0x4c7]
            E.target_pool = E.launcher_pool;
            E.target_rec = E.launcher_rec;
            E.lock = 1;
            if (obj(w, E.launcher_pool, E.launcher_rec).state != 0) {
                c.h.unported("seg7:0x2e02", kPoolMissiles, m);
                recount_in_use(p, kPoolMissiles);
            }
            break;
        default:
            break;
        }
        if (Wd(ww::kEnd) != 6) p.rec(kPoolMissiles, m).state = 1;  // [BYTES seg12:0x4e3-0x4f6]
    }
}

// ---------------------------------------------------------------- the program's links

MissileLinks& missile_links() {
    static MissileLinks l;
    return l;
}

namespace {

class LinkedHost final : public MissileHost {
public:
    void sound(int id, int pool, int rec, int mode) override {
        if (missile_links().sound) missile_links().sound(id, pool, rec, mode);
    }
    void capture_count(int ship, int v) override {
        if (missile_links().capture_count) missile_links().capture_count(ship, v);
    }
    void paint_weapons() override {
        if (missile_links().paint_weapons) missile_links().paint_weapons();
    }
    void choose_target(int ship, std::uint32_t arg) override {
        if (missile_links().choose_target) missile_links().choose_target(ship, arg);
    }
    void destroy(int pool, int rec) override {
        if (missile_links().destroy) missile_links().destroy(pool, rec);
    }
    void on_destroy(int pool, int rec) override {
        if (missile_links().on_destroy) missile_links().on_destroy(pool, rec);
    }
    Point16 to_screen(std::int16_t x, std::int16_t y) override {
        if (missile_links().to_screen) return missile_links().to_screen(x, y);
        return Point16{x, y};
    }
    void touch(int ship, std::int16_t pr) override {
        if (missile_links().touch) missile_links().touch(ship, pr);
    }
    void ship_touched(int ship) override {
        if (missile_links().ship_touched) missile_links().ship_touched(ship);
    }
    void unported(const char* address, int a, int b) override {
        if (missile_links().unported) missile_links().unported(address, a, b);
    }
};

}  // namespace

MissileHost& linked_missile_host() {
    static LinkedHost h;
    return h;
}

void fire_weapon_system(World& w, WaveGlobals& g, shell::Rng& rng) {
    MissileCtx c{w, g, missile_state(), linked_missile_host(), rng};
    weapon_system(c);
}

void move_missiles(World& w, UpdateCtx& u) {
    MissileCtx c{w, u.flight.g, missile_state(), linked_missile_host(), u.flight.rng};
    move_pool6(c, u);
}

}  // namespace is::world
