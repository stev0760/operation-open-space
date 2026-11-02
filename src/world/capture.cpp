// Capture (capture.h).
#include "world/capture.h"

#include "world/bounce.h"
#include "world/damage.h"
#include "world/effects.h"

namespace is::world {

namespace {

// Ship words the touch writes that records.h keeps in reserved blocks.
constexpr int kCaptured = 0xd2;   // DGROUP:0x7162, the ship's captured count
constexpr int kDestroyed = 0xd0;  // DGROUP:0x7160, the ship's destroyed count

std::int16_t sbyte_of(const ShipRecord& s, int off) {
    return static_cast<std::int16_t>(sbyte_at(s, off));
}

// The 32-bit add of a sign-extended 16-bit value (`cwd`, then `add` / `adc` on the dword).
std::uint32_t add_sx(std::uint32_t d, std::int16_t v) {
    return d + static_cast<std::uint32_t>(static_cast<std::int32_t>(v));
}

// The pod record DGROUP:0x8560 + 0x34·rec (effects.h EffectState::pods, death.h fill_pod).
PodRecord& pod(int rec) {
    static PodRecord scratch;
    if (rec < 0 || rec >= kPodRecords) {
        scratch = PodRecord{};
        return scratch;
    }
    return effect_state().pods[static_cast<std::size_t>(rec)];
}

// The tank word DGROUP:0x96f0 + 0xad·n (DesignRating::tank), 0 for a design the port does not hold.
std::int16_t design_tank(const World& w, int n) {
    return n >= 0 && n < kShipCount ? w.design_ratings[static_cast<std::size_t>(n)].tank : std::int16_t{0};
}

// The missile entry DGROUP:0x66e0 + 0x1c·rec: Pools::missile (records.h), the one table the
// launcher and the pool-6 mover write.
const MissileSlot& missile(const World& w, int rec) { return w.pools.missile(rec); }

// 16-bit `cwd; idiv` with the port's guard on the two trapping cases [DESIGN].
std::int16_t div16(std::int16_t n, std::int16_t d) {
    if (d == 0 || (d == -1 && n == -32768)) return n;
    return idiv16(n, d);
}

// cwd; sub ax, dx; sar ax, 1: half toward zero [BYTES seg8:0x7d81-0x7d84].
std::int16_t half(std::int16_t v) { return idiv16(v, 2); }

// The virused capture (seg13:0x18e0-0x1ad6): resources, message 0x48, the player's PlayMode,
// then the goal of the nearest pool-3, -8 or -9 record. It returns without the tail.
void virused(CaptureCtx& c, int ship) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    ShipRecord& S = ship_at(w, ship);
    // resources -= 100, 0 when negative (`sub ax, 0x64; jns; xor ax, ax`) [BYTES seg13:0x18e5-0x18f0]
    std::int16_t r = sub16(S.resources(), 100);
    if (r < 0) r = 0;
    S.set_resources(r);
    c.h.message(ship, 7, 0, 0x48);  // seg3:0x6ec(ship, 7, 0, 0x48) [BYTES seg13:0x18fd]
    // The player's PlayMode while it is < 2 [BYTES seg13:0x1905-0x19bc].
    if (ship == g.player && g.play_mode < 2) {
        if (g.demo != 0 && g.play_mode == 0) {
            g.demo = 0;
            g.play_mode = 2;
        } else if (g.demo != 0 && g.play_mode != 0) {
            g.demo = 0;
            g.play_mode = 3;
        } else if (g.demo == 0 && g.play_mode != 0) {
            g.play_mode = 4;
        }
        // DGROUP:0x95bc and DGROUP:0x9134 are the World's too (flight_hooks.cpp sync_out copies
        // PlayMode back; the port writes both words here) [DESIGN].
        w.player_game = g.demo;
        w.play_mode = g.play_mode;
        if (g.play_mode > 2) {  // [BYTES seg13:0x1963-0x19bc]
            // LoadString 1700 into a 0x28-byte buffer, lstrcpy; LoadString 1701, lstrcat.
            auto load = [&](int id) {
                std::string s = c.h.lang(id);
                if (s.size() > 0x27) s.resize(0x27);
                return s;
            };
            w.w8630 = load(1700) + load(1701);  // [BYTES ENG_LANG.DLL STRING 107] ids 1700-1701
            w.w058c = 1;
            w.w05c0 = g.now + 1200u;
        }
    }
    set_ship_dword(S, 0x94, g.now + 15000u);  // [BYTES seg13:0x19c0-0x19d6]
    w.w8690 = g.now + 7500u;                  // [BYTES seg13:0x19da-0x19e4]
    // The nearest `state`-0 record of pools 3, 8 (side kind DGROUP:0x3d6c = 2 only) and 9 below
    // DGROUP:0x9324, by seg8:0x285a, from 20000 [BYTES seg13:0x19e8-0x1aa0].
    std::int16_t best_pool = -1, best_rec = 0, best = 20000;
    const ObjectRecord& so = obj(w, g.ship_pool, ship);
    for (int k = 0; k < g.ship_pool; ++k) {
        if (k != 3 && k != 8 && k != 9) continue;
        for (int i = 0; i < w.pools.in_use(k) && i < w.pools.records(k); ++i) {
            if (k == 8 && (i >= kPool8Max || w.pool8[static_cast<std::size_t>(i)].at(0x1c) != 2)) continue;
            const ObjectRecord& o = w.pools.rec(k, i);
            if (o.state != 0) continue;
            std::int16_t dx = 0, dy = 0;
            const std::int16_t d = distance_cells(o.x, o.y, so.x, so.y, dx, dy);  // seg8:0x285a [BYTES seg13:0x1a4d]
            if (d < best) {  // `cmp ax, [bp-0x16]; jge` [BYTES seg13:0x1a55]
                best = d;
                best_pool = static_cast<std::int16_t>(k);
                best_rec = static_cast<std::int16_t>(i);
            }
        }
    }
    if (best_pool == -1) return;  // [BYTES seg13:0x1aa0 seg13:0x1aa4]
    // The steering words of M5's pilot [BYTES seg13:0x1aa8-0x1ad2].
    S.w06 = best_pool;
    S.w32 = best_pool;
    S.w08 = best_rec;
    S.w34 = best_rec;
    S.w36 = S.w38 = S.w3a = S.w3c = 2;
}

// The good capture (seg13:0x1ad9-0x1c3e).
void good(CaptureCtx& c, int ship, int pool, int rec) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    ShipRecord& S = ship_at(w, ship);
    ObjectRecord& O = obj(w, pool, rec);
    // The value [BYTES seg13:0x1ae7-0x1b00]
    w.w9604 = capture_value(O.damage);
    if (w.w9604 < 0) {
        w.pools.bad_index_count = add16(w.pools.bad_index_count, 1);  // DGROUP:0x9498
        w.w9604 = 50;
    }
    S.set_resources(add16(S.resources(), w.w9604));                   // [BYTES seg13:0x1b0e]
    set_word_at(S, kCaptured, add16(word_at(S, kCaptured), 1));       // [BYTES seg13:0x1b12]
    // A teammate's capture (M5: computer ships): message 0x6f to the player and 1000 to the score
    // and the rating [BYTES seg13:0x1b16-0x1b53].
    if (ship != g.player) {
        const std::int16_t pt = c.h.team(ship_at(w, g.player).design_index);
        if (pt == c.h.team(S.design_index)) {
            c.h.message(g.player, g.ship_pool, ship, 0x6f);
            w.score += 1000u;   // `add dword [0x1e58], 0x3e8`
            w.rating += 1000u;  // `add dword [0x5078], 0x3e8`
        }
    }
    // The defenders (pool 2, DGROUP:0x1e00, below DGROUP:0x940a) [BYTES seg13:0x1b5c-0x1ba4];
    // the entries DGROUP:0x5304 / 0x5306 are CollideGlobals::defenders (M5's ships use them).
    w.w9602 = 0;
    for (int k = 0; k < w.pools.in_use(2) && k < w.pools.records(2); ++k) {
        if (w.pools.rec(2, k).state != 0) continue;
        if (k >= static_cast<int>(c.x.c.defenders.size())) break;  // [DESIGN] the port's table size
        DefenderEntry& d = c.x.c.defenders[static_cast<std::size_t>(k)];
        if (d.owner_pool != 0 || d.owner_rec != rec) continue;
        d.owner_pool = g.ship_pool;
        d.owner_rec = static_cast<std::int16_t>(ship);
        w.w9602 = add16(w.w9602, 1);
    }
    if (w.w9602 != 0) {  // [BYTES seg13:0x1ba6-0x1bb4]
        c.h.menu_refresh();  // seg2:0x170e
        c.h.play(0x69);      // seg10:0x744c(0x69): only the reset call (s2-capture.md 6)
    }
    // The sound seg10:0x754a(id, pool, rec, 1) [BYTES seg13:0x1bba-0x1c06]
    c.h.sound(capture_sound(w.w9602, O.b54, g.wave_kind), pool, rec, 1);
    c.h.message(ship, 7, 0, 0x55);  // [BYTES seg13:0x1c0e-0x1c17]
    // The player's score and rating: (DGROUP:0x9602 + 4)·1000 as a 16-bit 3-operand `imul`, then
    // `cwd` and add/adc on both dwords [BYTES seg13:0x1c1f-0x1c3e]. World::rating is DGROUP:0x5078;
    // src/world/rating.* is a different word, the ship rating +0xd8 (seg13:0x33d6), which the
    // capture does not touch (s2-capture.md 5).
    if (ship == g.player) {
        const std::int16_t award = mul16(add16(w.w9602, 4), 1000);
        w.score = add_sx(w.score, award);
        w.rating = add_sx(w.rating, award);
    }
}

// The pool-0 branch (seg13:0x1816-0x1c3e): false when it returned without the tail (virused).
bool capture_icon(CaptureCtx& c, int ship, std::int16_t pr) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    const int pool = pool_of(pr), rec = rec_of(pr);
    ObjectRecord& O = obj(w, pool, rec);
    ShipRecord& S = ship_at(w, ship);
    // Only an icon not dissolving, for a capturing design [BYTES seg13:0x1822-0x1836]
    if (O.race_status != 0) return true;
    if (c.x.h.design(w, S.design_index).captures == 0) return true;
    // 1. [BYTES seg13:0x1842-0x1854]: +0x48 is the capturing ship (draw.cpp's dissolve reads it).
    O.state = 1;
    O.race_status = 11;
    O.blast_size = static_cast<std::int16_t>(ship);
    rivals(c, pr, ship);  // 2. seg13:0x3008 [BYTES seg13:0x185d]
    // 3. A race (kind 6) [BYTES seg13:0x1863-0x18a5] (M6)
    if (g.wave_kind == 6 && c.h.seg4_8d3a() != ship) {
        c.h.seg13_b6f2(ship, 8);
        for (int k = 0; k < 2; ++k) c.h.seg13_2714(c.h.seg4_8d3a(), ship, -50, -80, 1, 2);
    }
    icon_counted(w, rec, 1, g.ship_pool);  // 4. seg7:0x5ae8(rec, 1) [BYTES seg13:0x18ac]
    c.h.dissolve(rec);                     // 5. seg7:0x3a42(rec) [BYTES seg13:0x18b7]
    bump_w82(S, 35);                       // 6. seg13:0x4eec(ship, 0x23) [BYTES seg13:0x18c3]
    if (O.b53 == 2) {                      // 7. virused [BYTES seg13:0x18d7]
        virused(c, ship);
        return false;
    }
    good(c, ship, pool, rec);  // 8.
    return true;
}

// The pod's weapons (seg13:0x1dea-0x1e8a): for j = 1 ... 9 a weapon u != -1 adds its count to
// the slot holding u (capped at 100), or takes a new slot through seg12:0x6d64.
void pod_weapons(CaptureCtx& c, int ship, int rec) {
    World& w = c.x.w;
    PodRecord& P = pod(rec);
    ShipRecord& S = ship_at(w, ship);
    for (int j = 1; j < 10; ++j) {
        std::int16_t& u = P.weapon[static_cast<std::size_t>(j)];
        if (u == -1) continue;  // [BYTES seg13:0x1e7e]
        for (int s = 1; static_cast<std::int8_t>(S.slots_cb()) > s && s < 10; ++s) {
            if (static_cast<std::int8_t>(S.weapon[static_cast<std::size_t>(s)]) != u) continue;  // [BYTES seg13:0x1e14]
            std::int16_t n = add16(static_cast<std::int8_t>(S.weapon_count[static_cast<std::size_t>(s)]), P.count[static_cast<std::size_t>(j)]);
            if (n > 100) n = 100;  // [BYTES seg13:0x1e23-0x1e28]
            S.weapon_count[static_cast<std::size_t>(s)] = static_cast<std::uint8_t>(n);
            u = -1;  // [BYTES seg13:0x1e2f]
            break;
        }
        if (u == -1) continue;  // [BYTES seg13:0x1e3e]
        c.h.seg12_6d64(ship, u);  // [BYTES seg13:0x1e4e]
        // byte +0xad + (byte +0xcb, re-read after the call) = the count's low byte [BYTES seg13:0x1e56-0x1e68]
        set_byte_at(S, 0xad + static_cast<std::int8_t>(S.slots_cb()), P.count[static_cast<std::size_t>(j)] & 0xff);
    }
}

// Pools 11 and 6 (seg13:0x1cec-0x1f95): false for the jumps to the tail before the writes.
void touch_pod(CaptureCtx& c, int ship, std::int16_t pr) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    const int pool = pool_of(pr), rec = rec_of(pr);
    ObjectRecord& O = obj(w, pool, rec);
    ShipRecord& S = ship_at(w, ship);
    if (S.bea != 0 || O.race_status != 0) return;  // [BYTES seg13:0x1cff seg13:0x1d1a]
    // A dead ship's own pod: straight to the tail (verified claim 31) [BYTES seg13:0x1d24-0x1d3e]
    if (pool == 11 && pod(rec).rec == ship && S.bec == 0) return;
    O.race_status = 10;  // [BYTES seg13:0x1d50-0x1d6c]
    O.state = 1;
    O.blast_size = static_cast<std::int16_t>(ship);
    if (pool == 11 && pod(rec).kind == 2) {  // [BYTES seg13:0x1d70-0x1dad]
        w.w1318 = 1;
        obj(w, kPoolShips, g.player).b54 = 0;  // DGROUP:0x1e30, pool 14 directly
        if (w.w94b2 != 0) w.w913e = 1;
        c.h.message(g.player, 7, 0, 0xb5);
    }
    if (pool == 11 && pod(rec).kind == 1) {  // [BYTES seg13:0x1db5-0x1de2] (M5)
        w.w9604 = pod(rec).resources;
        S.set_resources(add16(S.resources(), w.w9604));
        c.h.message(ship, 7, 0, 0x56);
        pod_weapons(c, ship, rec);
    }
    // Pool 6 with W(w)+0x42 != 0: all three writes behind that one test (verified claim 29;
    // core.md "Other touches" puts them outside it) [BYTES seg13:0x1e8d-0x1ee8].
    if (pool == 6 && c.h.weapon_word(missile(w, rec).weapon, 0x42) != 0) {
        obj(w, g.ship_pool, ship).angle = 1;  // object word +0x26
        S.be2 = 1;
        S.lockout.set(g.now + 1200u);  // dword +0x84
        const std::int16_t tank = design_tank(w, S.design_index);
        const std::int16_t f = add16(S.fuel, 20);
        S.fuel = tank <= f ? tank : f;  // `cmp ax, cx; jle` [BYTES seg13:0x1ee2]
    }
    // The sounds; the pool-6 ones pass DGROUP:0x9324 (verified claim 30, CORRECTED)
    // [BYTES seg13:0x1eec-0x1f32].
    if (pool == 11)
        c.h.sound(0x26, pool, rec, 1);
    else if (c.h.weapon_word(missile(w, rec).weapon, 0x24) == 6)
        c.h.sound(0x23, g.ship_pool, ship, 1);
    else
        c.h.sound(0x71, g.ship_pool, ship, 1);
    rivals(c, pr, ship);         // [BYTES seg13:0x1f41]
    bump_w82(S, 50);             // [BYTES seg13:0x1f4d]
    c.h.seg12_129e(pool, rec);   // [BYTES seg13:0x1f59] [OPEN]
    // [BYTES seg13:0x1f61-0x1f90] (M5)
    if (c.h.team(S.design_index) != 7 && pool == 11 && pod(rec).kind == 1 && g.demons_gate == 0)
        c.h.seg13_b6f2(ship, 2);
}

}  // namespace

// ---- seg8:0x7d70

void push_out(const ObjectRecord& ship, ObjectRecord& o) {
    const std::int16_t hs = half(ship.size), ho = half(o.size);
    // [BYTES seg8:0x7d7e-0x7db6]
    const std::int16_t dx = add16(sub16(sub16(ho, hs), ship.scr_x), o.scr_x);
    const std::int16_t dy = add16(sub16(sub16(ho, hs), ship.scr_y), o.scr_y);
    const std::int16_t R = add16(hs, ho);
    std::int16_t rvx = sub16(o.vx, ship.vx), rvy = sub16(o.vy, ship.vy);  // [BYTES seg8:0x7db9-0x7dd4]
    std::int16_t d = magnitude(dx, dy);  // seg8:0x35d0, both sign-extended [BYTES seg8:0x7de2]
    if (d == 0) d = 1;
    // The slow pair: the object's velocity rewritten, 15·dx a 16-bit `imul` before `cwd; idiv`
    // (verified claim 14) [BYTES seg8:0x7df6-0x7e59].
    if (abs16(rvx) < 10 && abs16(rvy) < 10) {
        o.vx = sub16(0, sub16(div16(mul16(dx, 15), d), ship.vx));
        o.vy = sub16(0, sub16(div16(mul16(dy, 15), d), ship.vy));
        rvx = sub16(o.vx, ship.vx);
        rvy = sub16(o.vy, ship.vy);
    }
    std::int16_t s = magnitude(rvx, rvy);  // [BYTES seg8:0x7e68]
    if (s == 0) s = 1;
    // seg5:0x110(seg5:0x314(rvx·dx + rvy·dy, 100), s·d), the low word compared signed: > 15
    // returns, the velocity rewrite already made (verified claim 15) [BYTES seg8:0x7e7c-0x7eb5].
    const std::int32_t dot = lmul32(rvx, dx) + lmul32(rvy, dy);
    const std::int32_t sd = lmul32(s, d);
    const std::int16_t q = lo16(sd != 0 ? ldiv32(lmul32(dot, 100), sd) : 0);
    if (q > 15) return;
    // [BYTES seg8:0x7eb8-0x7ed1]
    const std::int16_t ux = div16(mul16(rvx, 100), s);
    const std::int16_t uy = div16(mul16(rvy, 100), s);
    auto at_least_1 = [](std::int16_t v) {
        const std::int16_t a = abs16(v);
        return a < 1 ? std::int16_t{1} : a;  // `cmp ax, 1; jge` [BYTES seg8:0x7ee1]
    };
    // The y penetration [BYTES seg8:0x7ed4-0x7f5d] and the x one [BYTES seg8:0x7f60-0x7fed].
    const std::int16_t py =
        div16(mul16(add16(rvy > 0 ? dy : sub16(0, dy), R), 100), at_least_1(uy));
    const std::int16_t px =
        div16(mul16(add16(rvx > 0 ? dx : sub16(0, dx), R), 100), at_least_1(ux));
    // The smaller magnitude, the y one on a tie (verified claim 16) [BYTES seg8:0x7ff0-0x8010].
    const std::int16_t t = abs16(py) <= abs16(px) ? py : px;
    // `imul word`, then `cwd` cuts the product to 16 bits before `idiv` by -100 [BYTES seg8:0x801e-0x803c].
    o.scr_x = add16(o.scr_x, div16(mul16(t, ux), -100));
    o.scr_y = add16(o.scr_y, div16(mul16(t, uy), -100));
}

// ---- seg13:0x4eec

void bump_w82(ShipRecord& s, std::int16_t v) {
    if (v > 0) {
        // w += ldiv((i32)(1000 - w)·v, 1000), the product a full `imul word` [BYTES seg13:0x4efd-0x4f18]
        const std::int32_t p = static_cast<std::int32_t>(sub16(1000, s.w82)) * v;
        s.w82 = add16(s.w82, lo16(div32(p, 1000)));
        return;
    }
    // w = ldiv((i32)(w + v)·1000, (i32)(v + 1000)) [BYTES seg13:0x4f1e-0x4f41]; v = -1000 traps in
    // the original, the port keeps w [DESIGN].
    const std::int32_t den = add16(v, 1000);
    if (den == 0) return;
    s.w82 = lo16(div32(static_cast<std::int32_t>(add16(s.w82, v)) * 1000, den));
}

// ---- seg13:0x3008

void rivals(CaptureCtx& c, std::int16_t pr, int ship) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    const std::int16_t pool = static_cast<std::int16_t>(pool_of(pr));
    const std::int16_t rec = static_cast<std::int16_t>(rec_of(pr));
    ShipRecord& S = ship_at(w, ship);
    const int last = w.last_ship;
    auto alive_in_play = [&](int k) {
        return obj(w, g.ship_pool, k).state == 0 && ship_at(w, k).bec != 0;
    };
    // 1. The escort case [BYTES seg13:0x3025-0x312f]
    if ((S.w52 == 9 || S.w52 == 12) && S.w54 == pool && S.w56 == rec) {
        c.h.message(S.w58, g.ship_pool, ship, 0x20);
        S.w52 = 0;
        set_ship_dword(S, 0x94, g.now);
        for (int k = 0; k <= last && k < kShipCount; ++k) {
            if (k == ship) continue;
            const ShipRecord& K = ship_at(w, k);
            // The same word +0x06 against the pool and then the record (verified claim 17).
            if (K.w06 != pool || K.w06 != rec || K.w36 != 2 || !alive_in_play(k)) continue;
            c.h.message(ship, g.ship_pool, k, 0x49);
            c.h.seg13_2714(k, S.w58, -5, -20, 0, -1);
            c.h.message(S.w58, g.ship_pool, k, 0x4a);
            c.h.seg4_5e8e(k, ship, 90, 2);
        }
    } else {
        // 2. The ships whose goal it was [BYTES seg13:0x3132-0x3216] (M5: computer ships)
        for (int k = 0; k <= last && k < kShipCount; ++k) {
            if (k == ship) continue;
            const ShipRecord& K = ship_at(w, k);
            if (K.w32 != pool || K.w38 != 2 || K.w34 != rec || !alive_in_play(k)) continue;
            if (c.h.team(K.design_index) == 7 && pool == 6 && k == g.player) {
                c.h.seg13_b6f2(ship, 4);  // verified claim 19
                continue;
            }
            // bytes +0x148 / +0x149, sign-extended [BYTES seg13:0x313e-0x3150]
            if (sbyte_of(K, 0x148) != pool || sbyte_of(K, 0x149) != rec) continue;
            if (pool != 0 && pool != 11) continue;
            c.h.message(ship, g.ship_pool, k, 0x49);  // "Hey, I Wanted That %D!" (M5)
            // 50 for pool 4 is dead code here: the pool is 0 or 11 [BYTES seg13:0x3172]
            c.h.seg4_5e8e(k, ship, pool == 4 ? 50 : 90, 2);
        }
    }
    // 3. A computer ship's call for help (M5) [BYTES seg13:0x3219-0x32f5]; the player draws no rand().
    if (ship == g.player) return;
    if (!(S.resources() > 350)) return;
    if (!(add16(w.detail, 3) > w.last_ship)) return;
    const std::int16_t team = c.h.team(S.design_index);
    if (team == 3) return;
    if (!(add16(w.w992, 20) < w.budget)) return;
    std::int16_t n = div16(sub16(S.resources(), 200), 50);
    n = add16(n, div16(c.h.team_3854(team), -100));
    n = sub16(n, half(w.last_ship));
    n = add16(n, half(w.score_level));
    n = add16(n, 2);
    if (S.w26 < 40) n = add16(n, 1);  // [BYTES seg13:0x3294-0x329c]
    const std::int16_t r = imod16(static_cast<std::int16_t>(c.x.rng.rand()), 30);  // seg5:0xdc [BYTES seg13:0x329f]
    if (r >= n || w.score_level <= 0) return;
    for (int k = 0; k <= last && k < kShipCount; ++k) {
        const ShipRecord& K = ship_at(w, k);
        if (K.bec == 0 || c.h.team(K.design_index) == 0 || sbyte_of(K, 0xcd) == 0)
            c.h.seg13_adbe(ship, 3, 0x69);
    }
}

// ---- seg7:0x5a46, seg7:0x5ae8

void count_icons(World& w) {
    w.icons_b = 0;
    for (int i = 0; i < 8; ++i) {
        const bool live = i < w.pools.records(kPoolIcons) && w.pools.rec(kPoolIcons, i).state == 0 &&
                          w.pools.rec(kPoolIcons, i).b2e == 0 &&
                          w.pools.rec(kPoolIcons, i).race_status == 0;
        if (live) {
            w.icons_b = add16(w.icons_b, 1);  // [BYTES seg7:0x5a5f]
            continue;
        }
        // Every other record's icon number n >= 0 sets bit n % 8 of byte +0x1e2 + n/8 of the
        // current directory record [BYTES seg7:0x5a8d-0x5ad8].
        const std::int16_t n = w.icons[static_cast<std::size_t>(i)].number;
        if (!(n > -1)) continue;
        const int byte_ix = idiv16(n, 8);
        const int bit = imod16(n, 8);
        DirRecord& d = w.dir();
        const std::size_t off = 0x1e2 + static_cast<std::size_t>(byte_ix);
        if (off < kDirRecordSize) d.set_byte(off, d.byte(off) | (1 << bit));
    }
}

void icon_counted(World& w, int rec, int captured, std::int16_t ship_pool) {
    count_icons(w);  // [BYTES seg7:0x5af4]
    DirRecord& d = w.dir();
    if (captured != 0) {
        d.set_word(0x150, add16(d.word(0x150), 1));  // CAPTURED [BYTES seg7:0x5b07]
        w.w945c = add16(w.w945c, 1);                 // [BYTES seg7:0x5b0c]
    } else {
        d.set_word(0x14e, add16(d.word(0x14e), 1));  // DESTROYED [BYTES seg7:0x5b1c]
        if (rec >= 0 && rec < kIconRecords) {
            const IconRecord& ic = w.icons[static_cast<std::size_t>(rec)];
            // The killer: byte DGROUP:0x57b2 the pool, DGROUP:0x57b3 the ship [BYTES seg7:0x5b21-0x5b39]
            if (static_cast<std::int8_t>(ic.b0a) == ship_pool) {
                ShipRecord& k = ship_at(w, static_cast<std::int8_t>(ic.b0b));
                set_word_at(k, kDestroyed, add16(word_at(k, kDestroyed), 1));
            }
        }
        w.w945e = add16(w.w945e, 1);  // [BYTES seg7:0x5b3d]
    }
    // The directory record complete [BYTES seg7:0x5b41-0x5b5a]
    if (w.icons_b == 0 && d.byte(0x1d0) != 1) d.set_byte(0x1d0, 2);
}

// ---- seg13:0x16ac

void touch(CaptureCtx& c, int ship, std::int16_t pr) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    const int pool = pool_of(pr);  // [bp-8], the high byte [BYTES seg13:0x16bb]
    const int rec = rec_of(pr);    // [bp-0xa], the low byte [BYTES seg13:0x16c3]
    ObjectRecord& O = obj(w, pool, rec);
    ShipRecord& S = ship_at(w, ship);
    // The early exits, in order.
    if (O.b53 == 0) return;  // 1. `capturable` [BYTES seg13:0x16d7]
    if (ship == w.w952e && w.w9e49 >= 1 && w.w9e49 <= 6) return;  // 2. [BYTES seg13:0x16df-0x16f3]
    // 3. A dead ship takes one point through seg8:0x45e0 with the caller's DGROUP:0x943a / 0x943c
    // (s2-capture.md 4c), unless it touches its own pod [BYTES seg13:0x16fc-0x1726].
    if (S.bec == 0 && !(pool == 11 && pod(rec).rec == ship)) {
        ship_damage(c.x, g.ship_pool, ship, 1);
        return;
    }
    if (S.bea != 0 || S.be8 != 0) return;                                     // 4. [BYTES seg13:0x172e-0x173c]
    if (pool == 0 && static_cast<std::int8_t>(O.flame) < 0) return;           // 5. [BYTES seg13:0x174d]
    c.h.erase(O.scr_x, O.scr_y, O.size, O.size);  // PatBlt BLACKNESS [BYTES seg13:0x1756-0x1782]
    ship_touched(c.f, ship);                     // seg13:0x6716 [BYTES seg13:0x178b]
    push_out(obj(w, g.ship_pool, ship), O);      // seg8:0x7d70 [BYTES seg13:0x17f6]
    // The player's +0x156 = now - 1, whichever ship touched [BYTES seg13:0x17fb-0x1812]
    ship_at(w, g.player).due156.set(g.now - 1u);
    if (pool == 0 && !capture_icon(c, ship, pr)) return;  // the virused exit skips the tail
    // Pool 4, the fuel tank [BYTES seg13:0x1c42-0x1ce8]
    if (pool == 4 && S.bea == 0 && O.race_status == 0) {
        O.state = 1;
        O.blast_size = static_cast<std::int16_t>(ship);
        const std::int16_t tank = design_tank(w, S.design_index);
        const std::int16_t f = add16(S.fuel, 250);
        S.fuel = tank <= f ? tank : f;  // [BYTES seg13:0x1c8a-0x1c9e]
        bump_w82(S, 30);
        c.h.sound(0x71, g.ship_pool, ship, 1);  // [BYTES seg13:0x1cb7]
        O.race_status = 10;
        rivals(c, pr, ship);
        c.h.seg12_e5c(rec);  // [OPEN M4 stage 3]
        w.w698c = g.now + 10000u;  // [BYTES seg13:0x1cde-0x1ce8]
    }
    if (pool == 11 || pool == 6) touch_pod(c, ship, pr);
    // The tail [BYTES seg13:0x1f98-0x1fb2]
    if (ship == g.player && g.demo != 0 && g.play_mode != 0) S.w52 = 7;
}

}  // namespace is::world
