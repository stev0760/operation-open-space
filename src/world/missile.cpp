// The launched weapons (missile.h). M4 stage 2, track MISSILE.
#include "world/missile.h"

#include <cstddef>

#include "world/effects.h"
#include "world/pods.h"
#include "world/shots.h"
#include "world/view.h"

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

// ---------------------------------------------------------------- seg8:0x348e (M4 s7 LAUNCHED)

bool aim_at(World& w, const AimToScreen& to_screen, int shift, int P, int R, int TP, int TR,
            std::int16_t V) {
    // Both corners through seg8:0x29ea [BYTES seg8:0x34ab-0x34bf seg8:0x34db-0x34f3]
    const ObjectRecord& O0 = obj(w, P, R);
    const ObjectRecord& T = obj(w, TP, TR);
    const Point16 a = to_screen ? to_screen(O0.x, O0.y) : Point16{O0.x, O0.y};
    const Point16 t = to_screen ? to_screen(T.x, T.y) : Point16{T.x, T.y};
    // `shl word, cl` with cl = byte DGROUP:0x9150: a 286/386 masks the count to 5 bits, and a
    // count of 16 or more leaves 0 in the word [BYTES seg8:0x34f7-0x3504]
    const int n = shift & 0x1f;
    auto shl = [n](std::int16_t v) { return n >= 16 ? std::int16_t{0} : shl16(v, n); };
    const std::int16_t ax = shl(a.x), ay = shl(a.y);
    // Only the target point gets + extent / 2 (`cwd; sub ax, dx; sar ax, 1`) [BYTES seg8:0x3507-0x3523]
    const std::int16_t tx = add16(shl(t.x), idiv16(T.extent, 2));
    const std::int16_t ty = add16(shl(t.y), idiv16(T.extent, 2));
    const std::int16_t dx = sub16(tx, ax), dy = sub16(ty, ay);  // [BYTES seg8:0x3526-0x3535]
    std::int16_t vx = T.vx, vy = T.vy;                          // [BYTES seg8:0x3538-0x3543]
    const std::int16_t d = magnitude(dx, dy);                   // seg8:0x35d0(dx, dy), >= 1 [BYTES seg8:0x3563]
    if (dx == 0) {
        // `idiv dword [bp-0x28]` with sext(dx) = 0 traps in the original: no store, the record's
        // velocity stays [DESIGN ADR 0016 decision 4] (ruling R1) [BYTES seg8:0x358e]
        static bool once = false;
        guard_todo(once, "the aim divides by a zero x distance: no velocity stored", "seg8:0x348e");
        return false;
    }
    // vx: 32-bit `imul ecx` of sext V and sext d, `cdq`, `idiv dword` by sext dx, the low word
    // added [BYTES seg8:0x3585-0x3592]
    const std::int32_t q = (static_cast<std::int32_t>(V) * static_cast<std::int32_t>(d)) / static_cast<std::int32_t>(dx);
    vx = add16(vx, lo16(q));
    // vy: seg5:0x110(sext dy · sext V, sext d) [BYTES seg8:0x3595-0x35a8]
    vy = add16(vy, lo16(div32(static_cast<std::int32_t>(dy) * static_cast<std::int32_t>(V), d)));
    ObjectRecord& O = obj(w, P, R);
    O.vx = vx;  // [BYTES seg8:0x35b5]
    O.vy = vy;  // [BYTES seg8:0x35c3]
    return true;
}

bool aim_at(MissileCtx& c, int P, int R, int TP, int TR, std::int16_t V) {
    MissileHost& h = c.h;
    const AimToScreen ts = [&h](std::int16_t x, std::int16_t y) { return h.to_screen(x, y); };
    return aim_at(c.w, ts, pixel_shift(c.g.size), P, R, TP, TR, V);
}

// ---------------------------------------------------------------- seg13:0x1076 (M4 s7 LAUNCHED)

std::uint16_t threat(MissileCtx& c, const VisibilityGlobals* vis, int pool, int rec) {
    World& w = c.w;
    // The goal [BYTES seg13:0x108b-0x10d0]
    std::int16_t goal_pool = -1, goal_rec = -1, goal_mode = 0;
    if (pool == c.g.ship_pool) {
        const ShipRecord& S = ship_at(w, rec);
        goal_pool = S.w32;  // DGROUP:0x70c2 [BYTES seg13:0x109d]
        goal_rec = S.w34;   // [BYTES seg13:0x10a4]
        goal_mode = S.w38;  // [BYTES seg13:0x10ab]
    } else if (pool == kPoolMissiles) {
        const MissileSlot& E = w.pools.missile(rec);
        goal_pool = E.target_pool;  // [BYTES seg13:0x10c2]
        goal_rec = E.target_rec;    // [BYTES seg13:0x10c9]
        goal_mode = 2;              // [BYTES seg13:0x10d0]
    }
    // Any other pool leaves the three locals uninitialised in the original; the port takes no
    // goal (mode 0) [DESIGN ADR 0016 decision 1].
    const ObjectRecord& self = obj(w, pool, rec);
    int best_pool = -1, best_rec = 0;
    std::int16_t best = 20000;  // [BYTES seg13:0x10ef]
    const int listed = vis ? vis->count : 0;
    // i < DGROUP:0x9318 + DGROUP:0x9146, both re-read every pass [BYTES seg13:0x12ce-0x12d8]
    for (int i = 0; i < add16(c.g.gates, static_cast<std::int16_t>(listed)); ++i) {
        int cp = 0, cr = 0;
        if (i >= listed) {
            cp = 0xc;                                                     // [BYTES seg13:0x10fc]
            cr = add16(add16(sub16(static_cast<std::int16_t>(i), static_cast<std::int16_t>(listed)),
                             sub16(static_cast<std::int16_t>(i), static_cast<std::int16_t>(listed))), 1);  // [BYTES seg13:0x1101-0x110b]
        } else if (i < kVisibleMax) {
            cp = static_cast<std::int8_t>(vis->pool[static_cast<std::size_t>(i)]);  // cbw [BYTES seg13:0x12e7]
            cr = static_cast<std::int8_t>(vis->rec[static_cast<std::size_t>(i)]);   // [BYTES seg13:0x12ef]
        } else {
            continue;  // [DESIGN] past the list's storage
        }
        // The skips [BYTES seg13:0x1113-0x1157]
        if (cp == pool && cr == rec) continue;
        if (cp == 2 || cp == 9 || cp == 4) continue;
        if (goal_mode == 2 && cp == goal_pool && cr == goal_rec) continue;
        if (goal_mode == 2 && cp == 0) continue;
        const ObjectRecord& C = obj(w, cp, cr);
        // seg8:0x285a(cand, self): dx = cand - self, in cells [BYTES seg13:0x1164-0x11a5]
        std::int16_t dx = 0, dy = 0;
        const std::int16_t d = distance_cells(C.x, C.y, self.x, self.y, dx, dy);
        if (d >= 300 || d <= 0) continue;  // [BYTES seg13:0x11a8-0x11b1]
        // dv = self - cand, 16-bit, then `cwd` [BYTES seg13:0x11b5-0x11eb]
        const std::int32_t dvx = sub16(self.vx, C.vx), dvy = sub16(self.vy, C.vy);
        // seg8:0x35d0(dvx, dvy) at seg13:0x11f4 is computed into [bp-0x64] and never read: not
        // called (no state) [BYTES seg13:0x11f4-0x1200]
        // q = (dx·dvx + dy·dvy) / d, seg5:0x314 twice, `idiv ecx` [BYTES seg13:0x1203-0x1252]
        const std::int32_t sum = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(dy * dvy) + static_cast<std::uint32_t>(dx * dvx));
        const std::int32_t q = sum / static_cast<std::int32_t>(d);
        if (q <= 0) continue;  // [BYTES seg13:0x1259-0x125f]
        // score = d - lo16(seg5:0x110(3q, 2)), 32-bit `imul ecx` [BYTES seg13:0x1261-0x127e]
        const std::int16_t score = sub16(d, lo16(div32(static_cast<std::int32_t>(static_cast<std::uint32_t>(q) * 3u), 2)));
        if (score >= best) continue;  // [BYTES seg13:0x1281-0x1287]
        if (score >= 60) {            // [BYTES seg13:0x1289-0x12a3]
            if (cp != 8) continue;
            if (add16(c.g.gates, c.g.gates) <= cr) continue;
            if (score >= 120) continue;
        }
        // A race wave with byte DGROUP:0x9eb5 = 0 rejects pool 8 (M6) [BYTES seg13:0x12a5-0x12b7]
        if (c.g.wave_kind == 6 && c.g.dg_9eb5 == 0 && cp == 8) continue;
        best = score;  // [BYTES seg13:0x12b9-0x12c8]
        best_pool = cp;
        best_rec = cr;
    }
    if (best_pool == -1) return 0xffffu;  // [BYTES seg13:0x12f7-0x12fb seg13:0x130a]
    // cbw of the record's low byte, OR the pool byte << 8 [BYTES seg13:0x12fd-0x1306]
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(static_cast<std::int8_t>(best_rec)) |
                                      (static_cast<std::uint16_t>(best_pool & 0xff) << 8));
}

// ---------------------------------------------------------------- seg4:0x5514 (M4 s7 LAUNCHED)

void dodge(MissileCtx& c, const VisibilityGlobals* vis, int m) {
    World& w = c.w;
    MissileSlot& E = w.pools.missile(m);
    if (obj(w, kPoolMissiles, m).on_screen == 0) {  // [BYTES seg4:0x552f-0x5534]
        E.mode = 2;                                  // [BYTES seg4:0x563e-0x5642]
        return;
    }
    const std::uint16_t v = threat(c, vis, kPoolMissiles, m);  // seg13:0x1076(6, m) [BYTES seg4:0x553d]
    if (v == 0xffffu) {  // `cwd`, then ax and dx both 0xffff [BYTES seg4:0x5542-0x554d]
        E.mode = 2;
        return;
    }
    // Stored before the launcher test [BYTES seg4:0x5551-0x5561] (claim 21 as corrected)
    E.alt_pool = static_cast<std::int16_t>(v >> 8);
    E.alt_rec = static_cast<std::int16_t>(v & 0xff);
    if (E.alt_pool == E.launcher_pool && E.alt_rec == E.launcher_rec) {  // [BYTES seg4:0x5565-0x5573]
        E.mode = 2;
        return;
    }
    // Target minus missile, 16-bit, then `cwd` [BYTES seg4:0x5577-0x55fd]
    const ObjectRecord& T = obj(w, E.alt_pool, E.alt_rec);
    const ObjectRecord& M = obj(w, kPoolMissiles, m);
    const std::int32_t dx = sub16(T.x, M.x), dy = sub16(T.y, M.y);
    const std::int32_t dvx = sub16(T.vx, M.vx), dvy = sub16(T.vy, M.vy);
    // dx·dvy - dy·dvx through seg5:0x314, `sub`/`sbb` [BYTES seg4:0x55fe-0x561a]
    const std::int32_t cross = static_cast<std::int32_t>(static_cast<std::uint32_t>(dx * dvy) -
                                                         static_cast<std::uint32_t>(dy * dvx));
    E.mode = cross > 0 ? 4 : 3;  // [BYTES seg4:0x561c-0x5636]
}

// ---------------------------------------------------------------- seg12:0x64aa (M4 s7 LAUNCHED)

void missile_fire(MissileCtx& c, int m, int id, const std::function<void(int pool, int rec)>& gun) {
    World& w = c.w;
    const WeaponWords& W = c.st.words;
    const std::int16_t age = w.pools.missile(m).age;
    if ((age & 3) != 0 || age < 13) return;  // `test al, 3`; signed `jge` [BYTES seg12:0x64b9-0x64cc]
    bool fire = false;
    const std::int16_t rule = W.word(id, ww::kFireRule);  // W+0x16 [BYTES seg12:0x64d8]
    const ObjectRecord& M = obj(w, kPoolMissiles, m);
    // Within one sector of the heading from M to O, the signed range h - 1 ... h + 1 with no
    // wrap [BYTES seg12:0x65a8-0x65c6 seg12:0x665f-0x667a] (claim 28 as corrected)
    auto facing = [&M](const ObjectRecord& O) {
        const std::int16_t h = direction16(sub16(O.scr_y, M.scr_y), sub16(O.scr_x, M.scr_x));
        return !(M.frame > add16(h, 1)) && !(M.frame < sub16(h, 1));
    };
    if (rule == 1) {
        fire = true;  // [BYTES seg12:0x64f1]
    } else if (rule == 2) {
        // The one rand() of the routine, after the steer's two of the pass [BYTES seg12:0x64f4-0x6501]
        const std::int16_t r = static_cast<std::int16_t>(c.rng.rand());
        if (imod16(r, 3) == 0) fire = true;
    } else if (rule == 3) {
        if (M.on_screen != 0) {  // [BYTES seg12:0x651c]
            // Ships k = 0 ... DGROUP:0x9316 inclusive, `state` 0 and on screen [BYTES seg12:0x6526-0x6558]
            for (int k = 0; k <= c.g.last_ship; ++k) {
                const ObjectRecord& S = obj(w, c.g.ship_pool, k);
                if (S.state != 0 || S.on_screen == 0) continue;
                if (facing(S)) {
                    fire = true;
                    break;
                }
            }
        }
    } else if (rule == 4) {
        // The entry target in state 0 and on screen; no table weapon has rule 4 [BYTES seg12:0x65ce-0x667a]
        const MissileSlot& E = w.pools.missile(m);
        const ObjectRecord& T = obj(w, E.target_pool, E.target_rec);
        if (T.state == 0 && T.on_screen != 0 && facing(T)) fire = true;
    }
    if (!fire) return;  // [BYTES seg12:0x6681-0x6685]
    switch (W.word(id, ww::kW14)) {  // [BYTES seg12:0x668b-0x66f7]
    case 1:
        c.h.pool5_shot(kPoolMissiles, m, M.frame, 0xb, 5);  // a FIREBALL [BYTES seg12:0x6691-0x66a7]
        break;
    case 2:
        c.h.pool5_shot(kPoolMissiles, m, M.frame, 0xb, 4);  // a SNOWBALL [BYTES seg12:0x66ac-0x66c1]
        break;
    case 3:
        // Signed byte +0x50 > 0x19 [BYTES seg12:0x66d7-0x66dc]
        if (static_cast<std::int8_t>(obj(w, kPoolMissiles, m).b50) > 0x19)
            c.h.lightning(kPoolMissiles, m);  // seg11:0x9fdc(6, m), cdecl [BYTES seg12:0x66de-0x66e8]
        break;
    case 4:
        if (gun) gun(kPoolMissiles, m);  // seg13:0x1fc2(6, m) [BYTES seg12:0x66c3-0x66c8]
        break;
    default:
        break;
    }
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
    // The switch on w - 1, unsigned > 0x16 returns (w = 0 too) [BYTES seg11:0xadac-0xadba]. Every
    // case passes (DGROUP:0x9324, DGROUP:0x9646) [BYTES seg11:0xad38-0xada7] (M4 s7 LAUNCHED).
    const unsigned k = static_cast<std::uint16_t>(wpn - 1);
    if (k > 0x16) return;
    const int sp = c.g.ship_pool;
    switch (k) {
    case 0: c.h.ring_of_death(sp, pl); break;         // w 1 RING OF DEATH [BYTES seg11:0xad5a-0xad63]
    case 1: c.h.tether_use(sp, pl, 2); break;         // w 2 GRAPPLING HOOK [BYTES seg11:0xad6b-0xad76]
    case 2: c.h.lightning(sp, pl); break;             // w 3 LIGHTNING [BYTES seg11:0xad82-0xad8b]
    case 3: c.h.fuel_ball(sp, pl, 0x1e, 4); break;    // w 4 SNOWBALLS [BYTES seg11:0xad4c-0xad58]
    case 4: c.h.fuel_ball(sp, pl, 0x1e, 5); break;    // w 5 FIREBALLS [BYTES seg11:0xad38-0xad44]
    case 12: c.h.wildcard(sp, pl); break;             // w 13 WILDCARD DEFENSE [BYTES seg11:0xad9e-0xada7]
    case 14: c.h.tether_use(sp, pl, 0xf); break;      // w 15 HANDCUFFS [BYTES seg11:0xad7e-0xad80]
    case 22: c.h.unported("seg11:0x9daa", sp, pl); break;  // w 23, the shock (M6, ruling R5) [BYTES seg11:0xad90-0xad99]
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

// seg11:0x881d-0x8979, the rear launch of a ship (weapons 9, 10, 16) (verified claims 19, 20,
// 44): M (at L's corner) moves by (rx << 3) + lo16(11·hx[(f + 8) % 16]) / 4 - M.extent / 2, y
// alike, with (rx, ry) the rear muzzle entry 16 + f of the ship's design n; the velocity is
// -(heading[f] << 2), and the ship's velocity comes off the position (step 11 adds it to the
// velocity). No dword +0xa0 [BYTES seg11:0x881d-0x8979].
void rear_launch(MissileCtx& c, ObjectRecord& M, const ObjectRecord& L, int s) {
    const std::int16_t f = L.frame;                                  // [BYTES seg11:0x882b]
    const int n = ship_at(c.w, s).design_index;                      // ship +0x4e [BYTES seg11:0x8836]
    const std::int16_t fb = imod16(add16(f, 8), 16);                 // `cwd; idiv 16`, the remainder [BYTES seg11:0x8841-0x884e]
    const Point16 r = c.h.muzzle(n, 16 + f);                         // [BYTES seg11:0x8850-0x8856 seg11:0x88d7-0x88e3]
    const std::int16_t half_e = idiv16(M.extent, 2);                 // `cwd; sub ax, dx; sar ax, 1`
    // /4: `cwd; and dx, 3; add; sar 2` (toward zero) [BYTES seg11:0x885a-0x8896]
    M.x = add16(M.x, sub16(add16(shl16(r.x, 3), idiv16(mul16(hx(fb), 11), 4)), half_e));
    M.y = add16(M.y, sub16(add16(shl16(r.y, 3), idiv16(mul16(hy(fb), 11), 4)), half_e));  // [BYTES seg11:0x88e7-0x890e]
    M.vx = sub16(0, shl16(hx(f), 2));  // `shl ax, 2; neg ax` [BYTES seg11:0x891e-0x892e]
    M.vy = sub16(0, shl16(hy(f), 2));  // [BYTES seg11:0x893e-0x894b]
    // The ship record of pool DGROUP:0x9324 [BYTES seg11:0x894f-0x8979]
    const ObjectRecord& S = obj(c.w, c.g.ship_pool, s);
    M.x = sub16(M.x, S.vx);
    M.y = sub16(M.y, S.vy);
}

// seg11:0x8193-0x86bc, the kind-5 aim (weapons 17 PROTECTIVE PELLETS, 21 THROWING STARS)
// (verified claims 10-16, 13 as corrected; Omission 12): it replaces the front and rear
// launch. The missile search (a) has no k != m test and the entry launcher words are written
// only after it (claim 12), so a record whose stale launcher differs from L adopts itself at
// d = 1: the self-aim, kept.
void aim_kind5(MissileCtx& c, ObjectRecord& M, int s, int pool, int w, int m) {
    World& wd = c.w;
    Pools& p = wd.pools;
    const WeaponWords& W = c.st.words;
    const int ships = c.g.ship_pool;
    const ObjectRecord& L = obj(wd, pool, s);
    M.vx = L.vx;  // [BYTES seg11:0x81a3-0x81b1]
    M.vy = L.vy;  // [BYTES seg11:0x81bf-0x81c7]
    // + L.extent / 2 - M.extent / 2, each halved toward zero [BYTES seg11:0x81d7-0x8235]
    M.x = add16(M.x, sub16(idiv16(L.extent, 2), idiv16(M.extent, 2)));
    M.y = add16(M.y, sub16(idiv16(L.extent, 2), idiv16(M.extent, 2)));
    int tp = -1, tr = 0;  // [bp-0xc], [bp-0xe] [BYTES seg11:0x8239]
    // (a) another launcher's missile within 350 cells, the last one found [BYTES seg11:0x8240-0x82a5]
    if (W.word(w, ww::kAggression) != 0) {
        for (int k = 0; k < p.in_use(kPoolMissiles) && k < p.records(kPoolMissiles); ++k) {
            const MissileSlot& e = p.missile(k);
            if (e.launcher_pool == pool && e.launcher_rec == s) continue;  // [BYTES seg11:0x824e-0x8262]
            const ObjectRecord& K = p.rec(kPoolMissiles, k);
            std::int16_t dx = 0, dy = 0;
            const std::int16_t d = distance_cells(K.x, K.y, M.x, M.y, dx, dy);  // [BYTES seg11:0x8264-0x8289]
            if (d < 350) {  // signed `jge` [BYTES seg11:0x828c-0x8299]
                tp = kPoolMissiles;
                tr = k;
            }
        }
    }
    // (b) the best visible object [BYTES seg11:0x82a7-0x844a]
    if (tp == -1) {
        std::int16_t best = 20000;  // [BYTES seg11:0x82b4]
        const VisibilityGlobals* vis = c.h.visible();
        const int listed = vis ? vis->count : 0;
        for (int i = 0; i < listed && i < kVisibleMax; ++i) {
            const int P = static_cast<std::int8_t>(vis->pool[static_cast<std::size_t>(i)]);  // cbw [BYTES seg11:0x838b]
            const int R = static_cast<std::int8_t>(vis->rec[static_cast<std::size_t>(i)]);
            if (P == pool && R == s) continue;              // [BYTES seg11:0x8390-0x839d]
            const ObjectRecord& C = obj(wd, P, R);
            if (C.mass <= 1) continue;                      // signed `jle` [BYTES seg11:0x83bc-0x83c1]
            // seg8:0x285a(L, cand): dx = L - cand, cells [BYTES seg11:0x83c3-0x8400]
            std::int16_t dx = 0, dy = 0;
            std::int16_t d = distance_cells(L.x, L.y, C.x, C.y, dx, dy);
            if (P == ships) {
                d = idiv16(mul16(d, 3), 4);  // `imul ax, d, 3; cwd; and dx, 3; add; sar 2` [BYTES seg11:0x8410-0x841d]
                if (pool == ships) {
                    // The other-ship rules (R4: plain conditions on the records; seg13:0x2688
                    // is MissileHost::relation, M5) [BYTES seg11:0x8420-0x844a seg11:0x82c1-0x8312]
                    const ShipRecord& Ls = ship_at(wd, s);
                    if (Ls.w32 == ships && Ls.w34 == R) {
                        d = idiv16(d, 6);
                    } else if (c.h.relation(s, R) > 150 ||
                               c.h.team(wd, ship_at(wd, R).design_index) ==
                                   c.h.team(wd, ship_at(wd, s).design_index)) {
                        d = mul16(add16(d, 50), 5);
                    }
                }
            }
            // The heading from L to the candidate: seg8:0x19b0 of the negated distances
            // [BYTES seg11:0x8315-0x8326]
            const std::int16_t h = direction16(sub16(0, dy), sub16(0, dx));
            // The plain signed range, no wrap (claim 13 as corrected) [BYTES seg11:0x833e-0x8357]
            if (!(sub16(L.frame, 1) > h) && !(add16(L.frame, 1) < h)) d = idiv16(add16(d, d), 3);
            if (d < best) {  // signed [BYTES seg11:0x835a-0x8378]
                best = d;
                tp = P;
                tr = R;
            }
        }
    }
    // (c) the aim [BYTES seg11:0x844d-0x855a]
    if (tp != -1) {
        aim_at(c, kPoolMissiles, m, tp, tr, W.word(w, ww::kClamp));  // seg8:0x348e(6, m, tp, tr, W+0x0e)
        const ObjectRecord& L2 = obj(wd, pool, s);
        const std::int16_t h = direction16(sub16(M.vy, L2.vy), sub16(M.vx, L2.vx));  // [BYTES seg11:0x8470-0x84bc]
        // lo16(5·lo16((M.extent / 2 + L.extent / 8)·heading)) / 20, all toward zero
        // [BYTES seg11:0x84d1-0x8516 seg11:0x8520-0x855a]
        const std::int16_t r = add16(idiv16(M.extent, 2), idiv16(L2.extent, 8));
        M.x = add16(M.x, idiv16(mul16(mul16(r, hx(h)), 5), 20));
        M.y = add16(M.y, idiv16(mul16(mul16(r, hy(h)), 5), 20));
    }
    // (d) no aim took effect: straight ahead [BYTES seg11:0x855e-0x86b8]
    const ObjectRecord& L3 = obj(wd, pool, s);
    if (M.vx == L3.vx && M.vy == L3.vy) {
        M.vx = add16(shl16(hx(L3.frame), 3), L3.vx);  // [BYTES seg11:0x85af-0x85c7]
        M.vy = add16(shl16(hy(L3.frame), 3), L3.vy);  // [BYTES seg11:0x85cb-0x85e9]
        const std::int16_t h = direction16(sub16(M.vy, L3.vy), sub16(M.vx, L3.vx));  // [BYTES seg11:0x85f9-0x8628]
        const std::int16_t r = add16(idiv16(M.extent, 2), idiv16(L3.extent, 2));
        M.x = add16(M.x, idiv16(mul16(r, hx(h)), 20));  // [BYTES seg11:0x863d-0x867b]
        M.y = add16(M.y, idiv16(mul16(r, hy(h)), 20));  // [BYTES seg11:0x8685-0x86b8]
    }
}

}  // namespace

Point16 MissileHost::muzzle(int n, int k) {
    ShotState& s = shot_state();
    return Point16{s.mx(n, k), s.my(n, k)};
}

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
        // Graphic -1, weapon 22 ENLIGHTENMENT (M4 s7 LAUNCHED; verified claims 5, 6) [BYTES
        // seg11:0x8087-0x80da]
        if (Ww(ww::kGraphic) == -1) {
            // The Demon's Gate store, overwritten by step 11's E.age = 0 before any read
            // (claim 6), kept as the bytes have it: `cwd; and dx, 3; add; sar 2` [BYTES seg11:0x808e-0x80a6]
            if (c.g.demons_gate != 0) p.missile(m).age = idiv16(Ww(ww::kLifetime), 4);
            // The rand() is drawn here whatever seg6:0x84fc does (ruling R5) [BYTES seg11:0x80c5]
            const std::int16_t r = static_cast<std::int16_t>(c.rng.rand());
            // (r % 2 + 1)·10: `cwd; idiv 2; inc dx; imul ax, dx, 10` [BYTES seg11:0x80ca-0x80d1]
            const std::int16_t kind = mul16(add16(imod16(r, 2), 1), 10);
            // seg6:0x84fc(kind, 0 ×7, W+0x1e - 1, m) (M6) [BYTES seg11:0x80aa-0x80da]
            c.h.particle(kind, sub16(Ww(ww::kLifetime), 1), m);
        }
        // [BYTES seg11:0x80e1-0x8113]
        M.on_destroy.set(Ww(ww::kEnd) == 4 || Ww(ww::kEnd) == 6 ? kDestroyPickup : kDestroyGeneric);
        if (w == 16) {  // [BYTES seg11:0x8119-0x8181] (verified claim 9)
            M.b53 = 1;  // [BYTES seg11:0x8127]
            // Every ship k < DGROUP:0x9316 (signed: the last ship is left out, `state` not
            // tested) whose design word DGROUP:0x970a + 0xad·(ship +0x4e) is 7 chases the
            // doughnut: words +0x06 = 6, +0x08 = m, +0x3a = 2, +0x36 = 2, dword +0x94 = now +
            // 100. Their readers are the pilots' (M5); ported per ruling R4 [BYTES seg11:0x812c-0x8177]
            for (int k = 0; k < c.g.last_ship; ++k) {
                ShipRecord& S = ship_at(wd, k);
                if (c.h.team(wd, S.design_index) != 7) continue;  // [BYTES seg11:0x813e-0x814e]
                S.w06 = 6;                                          // [BYTES seg11:0x8150]
                S.w08 = static_cast<std::int16_t>(m);               // [BYTES seg11:0x8159]
                S.w3a = 2;                                          // [BYTES seg11:0x8160]
                S.w36 = 2;                                          // [BYTES seg11:0x8164]
                S.d94.set(c.g.now + 100u);                          // [BYTES seg11:0x8168-0x8172]
            }
        } else {
            M.b53 = 0;  // [BYTES seg11:0x8181]
        }
        // The placement [BYTES seg11:0x818a seg11:0x86bf-0x8818] (claim 25; verify Omission 6:
        // a ship launcher with W+0x04 outside 1, 2 gets no placement)
        if (Ww(ww::kKind) == 5)
            aim_kind5(c, M, s, pool, w, m);  // [BYTES seg11:0x8193-0x86bc]
        else if (Ww(ww::kLaunch) == 1 || !ship_launcher)
            front_launch(c, M, L, s, pool);
        else if (Ww(ww::kLaunch) == 2)
            rear_launch(c, M, L, s);  // [BYTES seg11:0x881d-0x8979]
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

namespace {

// seg12:0x6a60(pool, rec) on the frame's collision host, as collide.cpp run_on_destroy builds
// it (pods.h pod_destroyed) (M4 s7 LAUNCHED) [BYTES seg12:0x6a60-0x6b1c].
void end_pod_destroyed(CollideCtx& x, int pool, int rec) {
    PodDestroyOut d;
    d.erase = [&x](int px, int py, int pw, int ph) { x.h.erase(px, py, pw, ph); };
    d.puff = [&x](int pp, int pr, int arg) { x.h.puff(pp, pr, arg); };
    d.to_screen = [&x](std::int16_t wx, std::int16_t wy) {
        return world_to_screen(x.view, wx, wy, x.g.wave_kind, x.g.dg_9eb5, x.g.dg_9130);
    };
    pod_destroyed(x.w, pool, rec, d);
}

}  // namespace

void move_pool6(MissileCtx& c, UpdateCtx& u) {
    World& w = c.w;
    Pools& p = w.pools;
    const WeaponWords& W = c.st.words;
    const int ships = c.g.ship_pool;
    // The frame's collision context, built where a routine needs it (the collision step, the
    // end through seg7:0x2e02 or seg12:0x6a60, the death head's shot)
    auto with_x = [&](const auto& f) {
        CollideGlobals& cg = u.host.collide_globals();
        CollideCtx x{w,      c.g,      cg,         u.host.collide_host(),
                     u.view, u.host.visible_list(), u.flight.rng, u.f.dg_943a,
                     u.f.dg_943c, u.f.dg_9454};
        f(x);
    };
    // seg13:0x1fc2(pool, rec) on the frame's shot slots (shots.h object_shot) (M4 s7 LAUNCHED)
    const std::function<void(int, int)> gun = [&](int pool, int rec) {
        with_x([&](CollideCtx& x) {
            ShotCtx sc{x, u.host.shots(), u.host.shot_host(), effect_state()};
            object_shot(sc, pool, rec);
        });
    };
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
                // The dissolve of an eaten doughnut or a caught boomerang: seg12:0x129e(6, m),
                // cdecl, pods.h pod_dissolve through the update host (verified claim 4; M4 s7
                // LAUNCHED) [BYTES seg12:0x4fe-0x516]
                if (M.race_status != 0) u.host.pod_dissolve(kPoolMissiles, m);
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
        // 2. The dodge seg4:0x5514(m) (Pascal) every 4 passes for W+0x38 != 0 (6, 9) (verify
        // Omission 11; M4 s7 LAUNCHED) [BYTES seg12:0x7b-0x94]
        if (Wd(ww::kW38) != 0 && (E.age & 3) == 0) dodge(c, u.host.visible_list(), m);
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
            missile_fire(c, m, id, gun);  // seg12:0x64aa(m, id) (M4 s7 LAUNCHED) [BYTES seg12:0x1fd-0x233]
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
        if (p.rec(kPoolMissiles, m).on_screen != 0 && E.age > 3) with_x([&](CollideCtx& x) {
            u.f.dg_943a = kPoolMissiles;  // [BYTES seg12:0x332]
            u.f.dg_943c = static_cast<std::int16_t>(m);  // [BYTES seg12:0x33b]
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
                    // The boomerang caught by any ship whose selected weapon has W+0x24 = 6
                    // (the launcher is not tested): +0x53 = 1, seg13:0x16ac(s, 0x600 | m), the
                    // slot's count + 1 with no cap, and no dispatch (verified claim 42, verify
                    // Omission 4) [BYTES seg12:0x3ad-0x40b]
                    ShipRecord& S = ship_at(w, hr);
                    K2.b53 = 1;                                                        // [BYTES seg12:0x3eb]
                    c.h.touch(hr, pr);                                                 // [BYTES seg12:0x3fb]
                    set_byte_at(S, 0xae + S.weapon_slot, sbyte_at(S, 0xae + S.weapon_slot) + 1);  // [BYTES seg12:0x407]
                } else {
                    dispatch(x, kPoolMissiles, m, hp, hr, a);  // seg8:0x5df2 [BYTES seg12:0x40d-0x424]
                }
            }
        });
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
            // BEER BOTTLES, SPUD MISSILES: the direct far call seg7:0x2e02(6, m) (effects.h
            // pickup_destroyed: SHATTER 0x1f, the shards), then seg7:0x2080(6, m) (verified claim
            // 38; M4 s7 LAUNCHED) [BYTES seg12:0x464-0x46d seg12:0x4c9 seg12:0x4d6]
            with_x([&](CollideCtx& x) { pickup_destroyed(x, kPoolMissiles, m); });
            recount_in_use(p, kPoolMissiles);
            break;
        case 5:
            // seg12:0x6a60(6, m), cdecl, no seg7:0x2080: reached by WILDCARD's weapon 27 once
            // seg11:0x9078 has stored W(27)+0x24 = 5 (claim 2 as corrected; verify Omission 2;
            // M4 s7 LAUNCHED) [BYTES seg12:0x473-0x483]
            with_x([&](CollideCtx& x) { end_pod_destroyed(x, kPoolMissiles, m); });
            break;
        case 6:  // the boomerang turns home [BYTES seg12:0x485-0x4c7] (verified claim 39)
            E.target_pool = E.launcher_pool;
            E.target_rec = E.launcher_rec;
            E.lock = 1;
            if (obj(w, E.launcher_pool, E.launcher_rec).state != 0) {
                // A dead launcher: seg7:0x2e02(6, m) breaks it up (FLANGE 0x1a), seg7:0x2080(6, m)
                // (M4 s7 LAUNCHED) [BYTES seg12:0x4c9 seg12:0x4d6]
                with_x([&](CollideCtx& x) { pickup_destroyed(x, kPoolMissiles, m); });
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
    // M4 s7 LAUNCHED: a set link runs the other track's port, else the logging default.
    void ring_of_death(int pool, int rec) override {
        if (missile_links().ring_of_death) return missile_links().ring_of_death(pool, rec);
        MissileHost::ring_of_death(pool, rec);
    }
    void tether_use(int pool, int rec, int w) override {
        if (missile_links().tether_use) return missile_links().tether_use(pool, rec, w);
        MissileHost::tether_use(pool, rec, w);
    }
    void lightning(int pool, int rec) override {
        if (missile_links().lightning) return missile_links().lightning(pool, rec);
        MissileHost::lightning(pool, rec);
    }
    void fuel_ball(int pool, int rec, int life, int kind) override {
        if (missile_links().fuel_ball) return missile_links().fuel_ball(pool, rec, life, kind);
        MissileHost::fuel_ball(pool, rec, life, kind);
    }
    void wildcard(int pool, int rec) override {
        if (missile_links().wildcard) return missile_links().wildcard(pool, rec);
        MissileHost::wildcard(pool, rec);
    }
    void pool5_shot(int pool, int rec, int heading, int life, int kind) override {
        if (missile_links().pool5_shot) return missile_links().pool5_shot(pool, rec, heading, life, kind);
        MissileHost::pool5_shot(pool, rec, heading, life, kind);
    }
    void particle(int kind, int life, int m) override {
        if (missile_links().particle) return missile_links().particle(kind, life, m);
        MissileHost::particle(kind, life, m);
    }
    std::int16_t relation(int s, int rec) override {
        if (missile_links().relation) return missile_links().relation(s, rec);
        return MissileHost::relation(s, rec);
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
