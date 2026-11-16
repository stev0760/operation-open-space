// The ships' attitudes (relations.h). Every site cites the listing it ports.
#include "world/relations.h"

#include <cstdlib>

#include "world/capture.h"
#include "world/collide.h"
#include "world/pilot.h"
#include "world/roster_build.h"

namespace is::world {

namespace {

// seg9:0x88ba, abs of a 16-bit word [BYTES seg13:0x275d seg13:0x2c9c].
std::int16_t abs16w(std::int16_t v) { return v < 0 ? sub16(0, v) : v; }

// A ship record by index; outside the eight a scratch record (collide.h ship_at, logged once).
ShipRecord& sh(World& w, int i) { return ship_at(w, i); }
const ShipRecord& sh(const World& w, int i) { return ship_at(const_cast<World&>(w), i); }

// A(a, b), ship a's word +0x62 + 2b; b outside 0 ... 7 reads and writes a scratch word
// [DESIGN ADR 0016 decision 2].
std::int16_t g_scratch_attitude = 0;
std::int16_t attitude(const World& w, int a, int b) {
    if (b < 0 || b >= kShipCount) {
        static bool once = false;
        guard_todo(once, "an attitude word past the eight ships reads 0", "DGROUP:0x70f2");
        return 0;
    }
    return ship_w(sh(w, a), pw::kAttitude0 + 2 * b);
}
void set_attitude(World& w, int a, int b, std::int16_t v) {
    if (b < 0 || b >= kShipCount) {
        static bool once = false;
        guard_todo(once, "an attitude word past the eight ships is not kept", "DGROUP:0x70f2");
        g_scratch_attitude = v;
        return;
    }
    set_ship_w(sh(w, a), pw::kAttitude0 + 2 * b, v);
}
void add_attitude(World& w, int a, int b, std::int16_t d) { set_attitude(w, a, b, add16(attitude(w, a, b), d)); }
void clamp_attitude(World& w, int a, int b) {
    // `cmp 0xc8; jle` then `cmp 0xff38; jge` [BYTES seg13:0x2c18-0x2c38 seg13:0x2d18-0x2d68]
    if (attitude(w, a, b) > 200) set_attitude(w, a, b, 200);
    if (attitude(w, a, b) < -200) set_attitude(w, a, b, -200);
}

// The goal test "ship s's (+0x32, +0x34, +0x38) = (pool, rec, 1)".
bool goal_is(const ShipRecord& s, int pool, int rec) { return s.w32 == pool && s.w34 == rec && s.w38 == 1; }

// The word table DGROUP:0x131e (initialised data, no writer found): 0, 2, 4, 2, 10, 5, 3, 2, 4, 7
// for k = 0 ... 9 [BYTES DGROUP:0x131e-0x1331]. Another k reads 0 [DESIGN ADR 0016 decision 2].
std::int16_t note_weight(int k) {
    static constexpr std::int16_t kD[10] = {0, 2, 4, 2, 10, 5, 3, 2, 4, 7};
    if (k < 0 || k > 9) {
        static bool once = false;
        guard_todo(once, "seg13:0x2e60's weight past the ten words reads 0", "DGROUP:0x131e");
        return 0;
    }
    return kD[k];
}

}  // namespace

// ---- the design team

std::int16_t enforcer_shp_word(int k) {
    if (k < 0 || k > 3) {
        static bool once = false;
        guard_todo(once, "DGROUP:0x4ed8 read past its four words gives 0", "seg4:0x77f7");
        return 0;
    }
    return roster_words().enforcer_shp[static_cast<std::size_t>(k)];
}

std::int16_t design_team_of(const World& w, int design) {
    return design >= 0 && design < kShipCount ? w.design_ratings[static_cast<std::size_t>(design)].team : std::int16_t{0};
}

std::int16_t ship_team(const World& w, int i) { return design_team_of(w, sh(w, i).design_index); }

std::int16_t& team_word(int x, int y) {
    if (x < 0 || x > 7 || y < 0 || y > 7) {
        static bool once = false;
        static std::int16_t scratch = 0;
        guard_todo(once, "a team-table word outside the 8 x 8 is not kept", "DGROUP:0x3824");
        scratch = 0;
        return scratch;
    }
    return roster_words().team_table[static_cast<std::size_t>(8 * x + y)];
}

ContactWords& contact_words() {
    static ContactWords c;
    return c;
}

AttitudeOut& attitude_links() {
    static AttitudeOut o;
    return o;
}

// ---- seg13:0x2688

std::int16_t relation(const World& w, int a, int b) {
    const ShipRecord& A = sh(w, a);
    // v = A(a, b) + T(team a, team b) [BYTES seg13:0x2696-0x26cf]
    std::int16_t v = add16(attitude(w, a, b), team_word(ship_team(w, a), ship_team(w, b)));
    if (v > 0)  // `or dx, dx; jle` [BYTES seg13:0x26d2-0x26e5]
        v = add16(v, mul16(sub16(3, ship_sb(A, pw::kAllies)), 20));
    else  // [BYTES seg13:0x26ea-0x26fa]
        v = add16(v, mul16(sub16(ship_sb(A, pw::kClass), 3), 20));
    if (w.handover.demons_gate != 0) return -300;  // DGROUP:0x92e8 [BYTES seg13:0x26fd-0x2707]
    return v;
}

// ---- seg4:0x5d9c

std::int16_t strength_ratio(const World& w, int a, int b) {
    // [BYTES seg4:0x5dab-0x5dc4]: signed bounds on both
    if (a < 0 || a > w.last_ship || b < 0 || b > w.last_ship) return 100;  // [BYTES seg4:0x5e81]
    const ShipRecord& A = sh(w, a);
    const ShipRecord& B = sh(w, b);
    // 32-bit `imul` products with EDX dropped by the next `cdq`, `idiv` toward zero [BYTES
    // seg4:0x5dc7-0x5e56].
    const auto strength = [](std::int32_t e, std::int32_t k) -> std::int32_t {
        const std::int32_t den = e / 2 + 50;
        if (den == 0) {
            static bool once = false;
            guard_todo(once, "seg4:0x5d9c's zero divisor gives 0", "seg4:0x5e29");
            return 0;
        }
        const std::int32_t p = static_cast<std::int32_t>(static_cast<std::uint32_t>(e) * static_cast<std::uint32_t>(k));
        return p / den;
    };
    const std::int32_t sa = strength(A.w26, A.kill_value);
    std::int32_t sb = strength(B.w26, B.kill_value);
    if (sb == 0) sb = 10;  // [BYTES seg4:0x5e5a-0x5e5f]
    // seg5:0x110(100·Sa, Sb), the low word [BYTES seg4:0x5e67-0x5e7a]
    const std::int32_t n = static_cast<std::int32_t>(static_cast<std::uint32_t>(sa) * 100u);
    return lo16(n / sb);
}

// ---- seg13:0x2e60

void grudge_note(World& w, int x, int y, int k) {
    if (k == -1) k = 9;  // [BYTES seg13:0x2e6e-0x2e74]
    const std::int16_t d = note_weight(k);
    // seg13:0x4eec(x, 5·D[k]) and seg13:0x4eec(y, 5·D[k]), or 12·D[k] for y the player
    // [BYTES seg13:0x2e79-0x2ec3]
    const std::int16_t v = mul16(d, y == w.player ? 12 : 5);
    bump_w82(sh(w, x), v);
    bump_w82(sh(w, y), v);
    if (k == 9) return;  // [BYTES seg13:0x2ec6-0x2eca]
    if (k == 2 || k == 3) {  // [BYTES seg13:0x2ecc-0x2f08]
        if (ship_team(w, x) == ship_team(w, y)) return;
        if (relation(w, x, y) > 70) return;
    }
    // byte x +0xf6 + 9y + k += lo8(D[k]) [BYTES seg13:0x2f0a-0x2f21]
    const int off = 0xf6 + 9 * y + k;
    if (y < 0 || y >= kShipCount || off >= static_cast<int>(kShipRecordSize)) {
        static bool once = false;
        guard_todo(once, "a hurt byte past the ship record is not kept", "seg13:0x2f21");
        return;
    }
    ShipRecord& X = sh(w, x);
    set_ship_b(X, off, static_cast<std::uint8_t>(ship_b(X, off) + static_cast<std::uint8_t>(d & 0xff)));
}

// ---- seg13:0x2714

void attitude_change(World& w, const AttitudeOut& out, int a, int b, int dt_in, int ds_in, int prop, int kind) {
    if (w.kind == 7) return;  // DGROUP:0x9320 [BYTES seg13:0x2722-0x2727]
    if (kind == 2) prop = 0;  // [BYTES seg13:0x272b-0x2731]
    std::int16_t dt = lo16(dt_in), ds = lo16(ds_in);
    const int ships = w.roles.ships;  // DGROUP:0x9324
    const std::int16_t ta = ship_team(w, a), tb = ship_team(w, b);  // [bp-6], [bp-8]
    const auto outside = [&](std::int16_t lim) {
        const std::int16_t p = team_word(ta, tb), q = team_word(tb, ta);
        return p < -lim || p > lim || q < -lim || q > lim;
    };
    // The team pair's scaling [BYTES seg13:0x275a-0x2830]
    if (abs16w(dt) > 1 && outside(300)) dt = idiv16(dt, 2);
    if (abs16w(dt) > 1 && outside(450)) dt = idiv16(dt, 3);
    if (outside(800)) dt = 0;
    if (a == b) return;  // [BYTES seg13:0x2847-0x284d]
    ShipRecord& A = sh(w, a);
    std::int8_t c;
    if (ds < 0) {  // [BYTES seg13:0x2851-0x2876]
        if (A.w38 == 4 && A.w34 == b) {
            ds = shl16(ds, 1);
            dt = shl16(dt, 1);
        }
        c = ship_sb(A, pw::kClass);  // +0x5e
    } else {
        c = ship_sb(sh(w, b), pw::kAllies);  // +0x5d [BYTES seg13:0x287c-0x2881]
    }
    // ds += lo16(lo16((3 - c)·ds)·10) / 100 [BYTES seg13:0x2885-0x2897]
    ds = add16(ds, idiv16(mul16(mul16(sub16(3, c), ds), 10), 100));
    // b commanded against a: +0x52 in 2 ... 4 with (+0x54, +0x56) = (DGROUP:0x9324, a)
    const auto commanded = [&]() {
        const ShipRecord& B = sh(w, b);
        return B.w54 == ships && B.w56 == a && B.w52 >= 2 && B.w52 <= 4;
    };
    // The team pair [BYTES seg13:0x289a-0x295c]; seg13:0x2da4 (empty) after each change.
    if (tb != ta) {
        if ((dt < 0 && commanded()) || mul16(team_word(ta, tb), dt) > 0) {
            team_word(ta, tb) = add16(team_word(ta, tb), idiv16(dt, 2));
            team_word(tb, ta) = add16(team_word(tb, ta), idiv16(dt, 6));
        } else {
            team_word(ta, tb) = add16(team_word(ta, tb), dt);
            team_word(tb, ta) = add16(team_word(tb, ta), idiv16(dt, 3));
        }
    }
    // The ship pair [BYTES seg13:0x295f-0x2ab8]; seg13:0x2db8 (empty) after each change.
    if (ds < 0 && commanded()) {
        add_attitude(w, a, b, idiv16(ds, 2));
        grudge_note(w, a, b, kind);
        add_attitude(w, b, a, idiv16(ds, 8));  // `and dx, 7; add; sar 3`: toward zero
    } else if (mul16(relation(w, a, b), ds) > 0) {  // the low word of the product [BYTES seg13:0x29eb-0x29f0]
        add_attitude(w, a, b, idiv16(add16(ds, ds), 3));
        grudge_note(w, a, b, kind);
        add_attitude(w, b, a, idiv16(ds, 4));
    } else {
        add_attitude(w, a, b, ds);
        grudge_note(w, a, b, kind);
        add_attitude(w, b, a, idiv16(ds, 3));
    }
    // The spreading loop [BYTES seg13:0x2abb-0x2d0b]
    if (prop != 0) {
        for (int k = 0; k <= w.last_ship; ++k) {
            if (k == a || k == b) continue;
            if (k >= w.pools.records(ships) || w.pools.rec(ships, k).state != 0) continue;  // DGROUP:0x1df8 + 4·DGROUP:0x9324
            if (sh(w, k).bec == 0) continue;
            const std::int16_t tk = ship_team(w, k);  // [bp-0xa]
            if (abs16w(team_word(tk, tb)) >= 350) continue;  // [BYTES seg13:0x2c98-0x2ca5]
            if (relation(w, k, a) < -100) {  // k dislikes a [BYTES seg13:0x2ca7-0x2cb7]
                std::int16_t n = 11;  // [bp-0xe]
                const ShipRecord& B = sh(w, b);
                if (goal_is(A, ships, k) && B.w38 == 1 && B.w32 == ships && B.w34 == a) {  // [BYTES seg13:0x2cbb-0x2cf2]
                    n = 3;
                    if (out.message) out.message(b, ships, k, 0x51);  // [BYTES seg13:0x2cf7-0x2d03]
                }
                if (tk != tb)  // [BYTES seg13:0x2ad0-0x2aea]
                    team_word(tk, tb) = sub16(team_word(tk, tb), idiv16(mul16(dt, 3), n));
                add_attitude(w, k, b, sub16(0, idiv16(mul16(ds, 3), n)));  // [BYTES seg13:0x2afe-0x2b10]
                if ((goal_is(A, ships, k) || goal_is(sh(w, k), ships, a)) && ds < 0)  // [BYTES seg13:0x2b26-0x2b66]
                    grudge_note(w, k, b, 6);
            } else if (relation(w, k, a) > 100) {  // k likes a [BYTES seg13:0x2b73-0x2b83]
                if (tk != tb) team_word(tk, tb) = add16(team_word(tk, tb), idiv16(dt, 4));  // [BYTES seg13:0x2b88-0x2ba6]
                add_attitude(w, k, b, idiv16(ds, 4));  // [BYTES seg13:0x2bb8-0x2bce]
                if ((tk == ta || relation(w, k, a) > 180) && ds < 0) grudge_note(w, k, b, 3);  // [BYTES seg13:0x2be2-0x2c0b]
            }
            clamp_attitude(w, k, b);  // [BYTES seg13:0x2c0e-0x2c38]
        }
    }
    clamp_attitude(w, a, b);  // [BYTES seg13:0x2d0e-0x2d38]
    clamp_attitude(w, b, a);  // [BYTES seg13:0x2d3e-0x2d68]
    // The info display shows a: repaint [BYTES seg13:0x2d6e-0x2d96]
    if (b == w.player && sh(w, w.player).target_rec == a && out.info_mode && out.info_mode() == 2 && out.info_paint)
        out.info_paint();
}

void attitude_change(World& w, int a, int b, int dt, int ds, int prop, int kind) {
    attitude_change(w, attitude_links(), a, b, dt, ds, prop, kind);
}

// ---- seg4:0x7d36

void fight_record(World& w, ContactWords& cw, int a, int b, std::int16_t d) {
    ShipRecord& A = sh(w, a);
    ShipRecord& B = sh(w, b);
    // The literal 14, not DGROUP:0x9324 (pitfalls.md) [BYTES seg4:0x7d44-0x7d6c]
    if ((A.w32 == 14 && A.w34 == b) || (B.w32 == 14 && B.w34 == a)) {
        const std::int16_t k = d > 30 ? 2 : 1;  // [BYTES seg4:0x7d6e-0x7d79]
        if (b >= 0 && b < kShipCount && a >= 0 && a < kShipCount) {
            std::int16_t v = add16(ship_sb(A, pw::kFight0 + b), k);  // [BYTES seg4:0x7d7e-0x7d96]
            if (v > 6) v = 6;
            set_ship_b(A, pw::kFight0 + b, static_cast<std::uint8_t>(v & 0xff));
            v = sub16(ship_sb(B, pw::kFight0 + a), k);  // [BYTES seg4:0x7d9a-0x7db1]
            if (v < -6) v = -6;
            set_ship_b(B, pw::kFight0 + a, static_cast<std::uint8_t>(v & 0xff));
        }
        if (a == w.player) {  // [BYTES seg4:0x7db5-0x7dc9]
            cw.w9680 = add16(cw.w9680, 1);
            cw.d8e00 += static_cast<std::uint32_t>(static_cast<std::int32_t>(d));
        } else if (b == w.player) {  // [BYTES seg4:0x7dcb-0x7ddc]
            cw.w9682 = add16(cw.w9682, 1);
            cw.d8e04 += static_cast<std::uint32_t>(static_cast<std::int32_t>(d));
        }
    }
    // The player's hit stamps on b [BYTES seg4:0x7de1-0x7e1e]
    if (d > 0 && a == w.player) {
        for (int k = 3; k >= 0; --k) set_ship_w(B, 0x46 + 2 * k, ship_w(B, 0x44 + 2 * k));
        set_ship_w(B, 0x44, lo16(static_cast<std::int32_t>(w.frames / 10u)));  // seg5:0x2b4(frame#, 10)
    }
}

}  // namespace is::world
