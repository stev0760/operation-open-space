// The attack reaction, the death bookkeeping, the assist score and the wing taunts
// (ship_react.h). Every site cites the listing it ports.
#include "world/ship_react.h"

#include "world/pilot.h"

namespace is::world {

namespace {

constexpr int kLiteralShips = 14;  // the literal pool 14 of seg4:0x5ee9, seg4:0x7669, seg4:0x771d (pitfalls.md)

ShipRecord& sh(World& w, int i) { return ship_at(w, i); }

// v's grudge byte +0xc2 + j (pilot.h pw::kGrudge0), signed; j outside 0 ... 7 reads 0 and is not
// written [DESIGN ADR 0016 decision 2].
bool rec_ok(int j) { return j >= 0 && j < kShipCount; }
std::int8_t grudge(const ShipRecord& v, int j) { return rec_ok(j) ? ship_sb(v, pw::kGrudge0 + j) : std::int8_t{0}; }
void set_grudge(ShipRecord& v, int j, std::uint8_t b) {
    if (rec_ok(j)) set_ship_b(v, pw::kGrudge0 + j, b);
}
// v's hurt byte +0xf6 + 9j + k (k = 1: +0xf7 + 9j, seg13:0x2e60's), signed.
std::int8_t hurt(const ShipRecord& v, int j, int k = 1) {
    const int off = 0xf6 + 9 * j + k;
    if (!rec_ok(j) || off >= static_cast<int>(kShipRecordSize)) return 0;
    return ship_sb(v, off);
}
// The pool-14 object `state` +0x18 through the literal pointer DGROUP:0x1e30 [BYTES seg4:0x639d-0x63a1].
bool in_play14(const World& w, int j) {
    return j >= 0 && j < w.pools.records(kPoolShips) && w.pools.rec(kPoolShips, j).state == 0;
}

}  // namespace

AttitudeOut attitude_out(CollideCtx& x) {
    AttitudeOut o;
    CollideHost* h = &x.h;
    o.message = [h](int r, int p, int rec, int code) { h->message(r, p, rec, code); };
    o.info_mode = [h]() { return h->info_mode(); };
    o.info_paint = [h]() { h->info_paint(); };
    return o;
}

// ---- seg4:0x5e8e

void attack_reaction(CollideCtx& x, int v, int a, int d_in, int kind) {
    World& w = x.w;
    WaveGlobals& g = x.g;
    ContactWords& cw = contact_words();
    const AttitudeOut out = attitude_out(x);
    std::int16_t d = lo16(d_in);
    if (d == 90) d = 50;  // [BYTES seg4:0x5e9c-0x5ea2]
    // [BYTES seg4:0x5ea7-0x5ecd]
    attitude_change(w, out, v, a, sub16(0, add16(idiv16(d, 4), 4)), sub16(-8, d), 0, -1);
    // The player's kind-1 hit on a ship it does not hunt [BYTES seg4:0x5ed0-0x5f39]
    if (kind == 1 && a == g.player) {
        const ShipRecord& P = sh(w, g.player);
        const ShipRecord& V = sh(w, v);
        if (!(P.w38 == 1 && P.w32 == kLiteralShips && P.w34 == v) && grudge(V, a) == 0) {
            if (d < 33) return;  // `jl` [BYTES seg4:0x5f08-0x5f0c]
            const std::int16_t h46 = ship_w(V, 0x46);
            if (h46 == 0) return;  // [BYTES seg4:0x5f10-0x5f15]
            // seg5:0x2b4(frame#, 10) − sext(+0x46), 32-bit: high word 0 and low word <= 80 go on
            // [BYTES seg4:0x5f17-0x5f39]
            const std::uint32_t q = w.frames / 10u - static_cast<std::uint32_t>(static_cast<std::int32_t>(h46));
            if ((q >> 16) != 0 || (q & 0xffffu) > 80u) return;
        }
    }
    if (w.handover.demons_gate != 0 || w.kind == 7) return;  // DGROUP:0x92e8, DGROUP:0x9320 [BYTES seg4:0x5f3b-0x5f47]
    if (kind == 1 && hurt(sh(w, v), a) < 9)  // [BYTES seg4:0x5f4c-0x5f78]
        attitude_change(w, out, v, a, -8, sub16(-10, d), 0, 1);
    // A fleeing target [BYTES seg4:0x5f7b-0x6031]
    if (sh(w, v).w38 == 4) {
        if (team_word(7, ship_team(w, v)) > -100 && team_word(7, ship_team(w, a)) < 100) {
            bool charged = false;
            if (d > 10) {  // `jle` [BYTES seg4:0x5fb4-0x5fb8]
                // The one rand() of seg4:0x5e8e (seg5:0xdc), % 4 by a 16-bit `idiv` [BYTES seg4:0x5fbc-0x5fca]
                const std::int16_t r = static_cast<std::int16_t>(x.rng.rand());
                if (imod16(r, 4) <= 1 && sh(w, v).w26 < 40) {  // [BYTES seg4:0x5fcc-0x5fd1]
                    x.h.seg13_b6f2(a, 3);  // crime 3 [BYTES seg4:0x5fd8]
                    charged = true;
                }
            }
            if (!charged) {
                // The first Enforcer j < DGROUP:0x9316 (`jge`: not the last record) [BYTES seg4:0x5fe2-0x6015]
                for (int j = 0; j < w.last_ship; ++j) {
                    if (ship_team(w, j) != 7) continue;
                    x.h.message(a, kLiteralShips, j, 0x5c);
                    break;
                }
            }
        }
        attitude_change(w, out, v, a, -13, sub16(0, d), 0, kind);  // [BYTES seg4:0x6018-0x602e]
        return;
    }
    // [BYTES seg4:0x6034-0x605a]
    std::int16_t cnt = 0;  // [bp-0x10]
    const std::int16_t tv = ship_team(w, v), ta = ship_team(w, a);  // [bp-8], [bp-0xa]
    {
        const ShipRecord& V = sh(w, v);
        if (V.w26 < 41 && kind != 1) return;  // [BYTES seg4:0x605d-0x6068]
        if (kind == 2) {                      // [BYTES seg4:0x606c-0x6095]
            if (ta == tv) return;
            if (V.w52 == 9) return;
            if (relation(w, v, a) > 100) return;
        }
        if (a == v) return;  // [BYTES seg4:0x6097-0x609f]
        // v under a's command [BYTES seg4:0x60a2-0x60c8]
        if (V.w52 >= 2 && V.w52 <= 4 && V.w58 == v && V.w54 == kLiteralShips && V.w56 == a) return;
    }
    // The attitude is unchanged since the test above, so this one never fires (s3-contact-verify
    // Omission 1) [BYTES seg4:0x60cc-0x60e1].
    if (kind == 2 && relation(w, v, a) > 130) return;
    if (tv == 5 || ta == tv) cnt = add16(cnt, 1);  // [BYTES seg4:0x60e5-0x60f3]
    if (tv == 4) cnt = add16(cnt, 1);              // [BYTES seg4:0x60f6-0x60fc]
    {
        ShipRecord& V = sh(w, v);
        if (grudge(V, a) >= 20) {  // signed `jl` [BYTES seg4:0x60ff-0x6113]
            set_grudge(V, a, 0);
            return;
        }
        // v already hunts a, unless v is an Enforcer [BYTES seg4:0x6116-0x6136]
        if (V.w32 == kLiteralShips && V.w34 == a && V.w38 == 1 && tv != 7) return;
        // The low-fuel plea: design +0x1e / 10 above v's fuel, the player the attacker
        // [BYTES seg4:0x613a-0x61cb]
        const int n = V.design_index;
        const std::int16_t tank = n >= 0 && n < kShipCount ? w.design_ratings[static_cast<std::size_t>(n)].tank : std::int16_t{0};
        if (idiv16(tank, 10) > V.fuel && a == g.player) {
            x.h.message(a, kLiteralShips, v, 0xbe);
            if (grudge(V, a) == 4) {
                d = shl16(d, 1);  // [BYTES seg4:0x617c]: nothing reads it again
                if (team_word(7, tv) > -100 && team_word(7, ta) < 100) x.h.seg13_b6f2(a, 3);  // [BYTES seg4:0x61ac]
            } else {
                x.h.play(0x3d);  // seg10:0x744c(0x3d) [BYTES seg4:0x61b6-0x61bd]
            }
            set_grudge(V, a, 4);  // [BYTES seg4:0x61be-0x61c6]
            return;
        }
    }
    // [BYTES seg4:0x61ce-0x6207]
    if (kind != 1 || hurt(sh(w, v), a) > 8)
        attitude_change(w, out, v, a, sub16(0, add16(idiv16(d, 2), 7)), sub16(-10, d), 1, kind);
    // The player's own ship in Action mode or a player game [BYTES seg4:0x620a-0x6220]
    if (v == g.player && (g.play_mode == 0 || g.demo != 0)) return;
    // seg4:0x5d9c(v, a) [BYTES seg4:0x6223-0x6256]
    const std::int16_t s = strength_ratio(w, v, a);
    if (ship_sb(sh(w, v), pw::kClass) > 1 && (s < 15 || s > 700) && tv != 7) return;
    if (s < 30 || s > 300) cnt = add16(cnt, 1);
    {
        ShipRecord& V = sh(w, v);
        // The first hurt by the player: seg10:0x754a(0x3d, 0, 0, 0) [BYTES seg4:0x6259-0x627b]
        if (grudge(V, a) == 0 && a == g.player) x.h.sound(0x3d, 0, 0, 0);
        set_grudge(V, a, static_cast<std::uint8_t>(grudge(V, a) + 1));  // `inc byte` [BYTES seg4:0x6286]
    }
    if (a == g.player && kind == 1) w.w9432 = add16(w.w9432, idiv16(d, 2));  // [BYTES seg4:0x628a-0x629e]
    const std::int16_t rel = relation(w, v, a);  // [bp-4] [BYTES seg4:0x62a2-0x62ae]
    // The weight [bp-0xc] [BYTES seg4:0x62b1-0x6337]
    std::int16_t wt = 0;
    {
        const ShipRecord& V = sh(w, v);
        const std::int16_t div = x.h.design(w, V.design_index).hit_div[0];  // design +0x10 DGROUP:0x96e2
        std::int16_t q = 0;
        if (div == 0) {
            // The original's `idiv` traps on a zero design +0x10 (SHP +2 below 4) [DESIGN ADR 0016
            // decision 4]: the port takes 0 for the quotient.
            static bool once = false;
            guard_todo(once, "seg4:0x5e8e's zero design +0x10 divisor gives 0", "seg4:0x62c5");
        } else {
            q = idiv16(d, div);
        }
        const std::int16_t left = sub16(V.w26, q);  // `sub; neg`: engine − d / div
        if (left < 65) wt = 1;
        if (left < 45) wt = add16(wt, 1);
        if (d > 24) wt = add16(wt, 1);
        if (d > 36) wt = add16(wt, 1);
        if (d == 90) wt = add16(wt, ship_sb(V, pw::kClass) == 3 ? 3 : 1);  // d is never 90 here (step 1)
        if (rel < -100) wt = add16(wt, 1);
        if (rel < -210) wt = add16(wt, 1);
    }
    int victim = v;  // [bp+6], which the FUZZY ONES' hand-over rewrites
    const auto team_two_handover = [&]() {
        // A FUZZY ONE hands the fight to the last live AVENGER in play with engine > 40
        // [BYTES seg4:0x6583-0x65f2 seg4:0x6663-0x66ce]
        if (ship_team(w, v) != 2) return;
        for (int j = 0; j <= w.last_ship; ++j) {
            if (ship_team(w, j) != 0 || j == g.player || sh(w, j).bec == 0 || !in_play14(w, j)) continue;
            if (sh(w, j).w26 <= 40 || j == a) continue;
            victim = j;
        }
    };
    bool turn = false;  // seg3:0xf4 and seg13:0x915c at seg4:0x66d0
    if (tv == 7 && ta != 7) {
        // The Enforcer branch [BYTES seg4:0x6356-0x64ea]
        ShipRecord& V = sh(w, v);
        if (ship_sb(V, pw::kClass) > 1) {
            if (grudge(V, a) < 2) {  // [BYTES seg4:0x6364-0x6405]
                bool none = true;    // [bp-0x14]: no Interceptor in play and alive
                for (int j = 0; j <= w.last_ship; ++j) {
                    const ShipRecord& J = sh(w, j);
                    if (ship_sb(J, pw::kClass) == 1 && ship_team(w, j) == 7 && in_play14(w, j) && J.bec != 0) none = false;
                }
                if (none) x.h.message(a, kLiteralShips, v, 0x3b);
                V.w52 = 0x15;  // the dword store 0x000e0015 [BYTES seg4:0x63de]
                V.w54 = kLiteralShips;
                V.w56 = static_cast<std::int16_t>(a);
                set_ship_b(V, pw::kOneShot, 1);
                set_ship_d(V, pw::kReplanAt, g.now);
                w.w964a = add16(w.w964a, 1);
                cw.w9648 = add16(cw.w9648, 3);
            } else {  // [BYTES seg4:0x6408-0x6481]
                if (x.h.seg13_adbe(a, 0x15, 0x3c) != 0)
                    cw.ba163 = 5;
                else
                    x.h.message(a, kLiteralShips, v, 0x43);
                if (a == g.player) {
                    // byte DGROUP:0xa152 = DGROUP:0xa14d + 5 against DGROUP:0xa15c = DGROUP:0xa157 + 5
                    if (sub16(static_cast<std::int8_t>(w.ba14d[5]), 1) < static_cast<std::int8_t>(w.ba157[5]))
                        w.ba14d[5] = static_cast<std::uint8_t>(w.ba14d[5] + 1);
                }
                w.w964a = add16(w.w964a, 3);
                cw.w9648 = add16(cw.w9648, 3);
                V.w52 = 0x14;
                for (int j = 0; j <= w.last_ship; ++j) set_grudge(V, j, 0);
            }
        } else if (!(V.w54 == kLiteralShips && (V.w56 == a || ship_sb(V, pw::kClass) != 1))) {
            // An Interceptor or Terminator [BYTES seg4:0x6484-0x64ea]
            if (grudge(V, a) > 2) {
                V.w52 = 0x3e;
                set_ship_b(V, pw::kOneShot, 2);
                set_ship_d(V, pw::kOrderUntil, g.now + 20000u);
                V.w54 = kLiteralShips;
                V.w56 = static_cast<std::int16_t>(a);
            } else {
                x.h.message(a, kLiteralShips, v, 0x41);
            }
        }
    } else if (rel > 0) {
        // [BYTES seg4:0x64ed-0x65f2]: the allies byte +0x5d (DGROUP:0x70ed) at seg4:0x64fa and
        // seg4:0x654e: lim = cnt - sbyte +0x5d + 4 (`cwde; sub ax, [bp-0x10]; neg; add 4`)
        const ShipRecord& V = sh(w, v);
        const std::int16_t lim = add16(sub16(cnt, ship_sb(V, pw::kAllies)), 4);
        const std::int8_t gv = grudge(V, a);
        if (lim >= gv) {
            if (d != 90) {
                x.h.message(a, kLiteralShips, v, gv > 1 ? 0x2a : 0x29);
                if (sub16(sub16(cnt, grudge(V, a)), ship_sb(V, pw::kAllies)) == -5 && a == g.player) {  // [BYTES seg4:0x6542-0x6555]
                    x.h.message(a, kLiteralShips, v, 0x2b);
                    x.h.sound(0x3e, 0, 0, 0);  // [BYTES seg4:0x6573-0x657b]
                }
            }
        } else {
            team_two_handover();
            turn = true;
        }
    } else {
        // [BYTES seg4:0x65f5-0x66ce]
        const ShipRecord& V = sh(w, v);
        const std::int16_t lim = add16(add16(ship_sb(V, pw::kClass), cnt), 1);
        const std::int8_t gv = grudge(V, a);
        if (lim >= gv) {
            if (d != 90) {
                x.h.message(a, kLiteralShips, v, gv > 1 ? 0x2e : 0x2d);
                if (add16(sub16(ship_sb(V, pw::kClass), grudge(V, a)), cnt) == -2 && a == g.player) {
                    x.h.message(a, kLiteralShips, v, 0x2f);
                    x.h.sound(0x3e, 0, 0, 0);
                }
            }
        } else {
            team_two_handover();
            turn = true;
        }
    }
    if (turn) {  // [BYTES seg4:0x66d0-0x66e8]
        x.h.seg3_f4(victim, kLiteralShips, a);
        x.h.seg13_915c(victim);
    }
    // The grudge step, on the ship [bp+6] holds now [BYTES seg4:0x66e9-0x66f4]
    ShipRecord& Vv = sh(w, victim);
    set_grudge(Vv, a, static_cast<std::uint8_t>(static_cast<std::uint8_t>(grudge(Vv, a)) + static_cast<std::uint8_t>(wt & 0xff)));
}

// ---- seg4:0x7636

void ship_killed(CollideCtx& x, int ship, int cp, int cr, int kill) {
    World& w = x.w;
    WaveGlobals& g = x.g;
    ContactWords& cw = contact_words();
    x.h.music_hold(g.now + 5000u);  // DGROUP:0x2384 [BYTES seg4:0x7649-0x7653]
    ShipRecord& S = sh(w, ship);
    const std::int16_t t = ship_team(w, ship);  // [bp-4] [BYTES seg4:0x7657-0x7666]
    // An Enforcer's kill [BYTES seg4:0x7669-0x768f]
    if (cp == kLiteralShips && ship_team(w, cr) == 7) {
        S.w06 = kLiteralShips;
        S.w08 = static_cast<std::int16_t>(cr);
        return;
    }
    // The goal ship that hurt it [BYTES seg4:0x7692-0x76b3]
    if (S.w32 == kLiteralShips && hurt(S, S.w34) > 3) {
        cp = kLiteralShips;
        cr = S.w34;
    }
    // The ship that hurt it most [BYTES seg4:0x76b6-0x771a]
    std::int16_t best = 7;
    int bestj = -1;
    for (int j = 0; j <= w.last_ship; ++j) {
        if (j == ship) continue;
        std::int16_t v = hurt(S, j);
        if (cp == kLiteralShips && j == cr) {
            v = add16(v, 5);
            if (v > 12) v = add16(v, 100);
        }
        if (best < v) {  // `cmp [bp-0xc], ax; jge`: the first wins a tie
            best = v;
            bestj = j;
        }
    }
    if (bestj != -1) {
        cp = kLiteralShips;
        cr = bestj;
    }
    if (cp != kLiteralShips) {  // [BYTES seg4:0x771d-0x7721 seg4:0x7977-0x79a2]
        if (ship != g.player) {
            S.w06 = static_cast<std::int16_t>(cp);
            S.w08 = static_cast<std::int16_t>(cr);
            x.h.message(g.player, kLiteralShips, ship, 0x62);
        }
        return;
    }
    S.w06 = kLiteralShips;  // [BYTES seg4:0x7725-0x7734]
    S.w08 = static_cast<std::int16_t>(cr);
    const bool team_ok = t >= 0 && t <= 7;
    if (!team_ok) {
        static bool once = false;
        guard_todo(once, "seg4:0x7636 for a team outside 0 ... 7 keeps no tally or slot", "seg4:0x774a");
    }
    if (cr == g.player && team_ok) kill_tally(cw, t) = add16(kill_tally(cw, t), 1);  // [BYTES seg4:0x7738-0x774a]
    if (t == 7) {  // [BYTES seg4:0x774e-0x779f]
        if (ship_sb(S, pw::kClass) > 0) {
            w.w9346 = sh(w, cr).roster_index;  // cr's +0xd6
            if (cr == g.player) {
                w.ba14d[0] = static_cast<std::uint8_t>(w.ba14d[0] + 1);
                cw.da147 = w.frames;   // dword DGROUP:0x1e68
                cw.wa14b = w.wave;     // DGROUP:0x91f8
            }
        } else {
            for (int j = 0; j <= 9; ++j) w.ba157[static_cast<std::size_t>(j)] = w.ba14d[static_cast<std::size_t>(j)];
        }
    }
    // The revenge slot [BYTES seg4:0x77a1-0x78c5]
    const bool vengeful = rec_ok(cr) ? (ship_sb(S, pw::kEe0 + cr) > 5 || S.w38 == 4) : S.w38 == 4;
    if (vengeful && !(t == 3 && ship_sb(S, pw::kCalledFor) == cr) && w.kind != 7 && team_ok) {
        // The dead ship's design word: roster word +0x00, or DGROUP:0x4ed8 + 2·class
        const auto design_word = [&]() -> std::int16_t {
            if (S.roster_index != -1) {
                if (const RosterRecord* r = roster_of(w, ship)) return r->shp;
                static bool once = false;
                guard_todo(once, "a roster index past the port's roster reads 0", "seg4:0x784d");
                return 0;
            }
            const int k = ship_sb(S, pw::kClass);
            const std::int16_t v = enforcer_shp_word(k);
            return v;
        };
        int j = 0;
        for (; j < 3; ++j) {
            const std::size_t ix = static_cast<std::size_t>(3 * t + j);
            if (w.w4eec[ix] != -1) continue;  // [BYTES seg4:0x7861-0x786f]
            w.w4eec[ix] = sh(w, cr).roster_index;  // [BYTES seg4:0x7871-0x787a]
            cw.w4fd0[ix] = w.wave;                 // [BYTES seg4:0x787e-0x7881]
            cw.w6b10[ix] = design_word();          // [BYTES seg4:0x7885-0x78a3 seg4:0x77e9-0x7804]
            break;
        }
        if (j == 3) {
            // No free slot: slot 0's DGROUP:0x4eec word, but the other two at index 3t + 3, the
            // next team's slot 0; for team 7, DGROUP:0x5000 and the kill tally DGROUP:0x6b40
            // [BYTES seg4:0x7815-0x7856 seg4:0x78aa-0x78c5] [DESIGN ADR 0016 decision 2].
            w.w4eec[static_cast<std::size_t>(3 * t)] = sh(w, cr).roster_index;
            const std::size_t ix = static_cast<std::size_t>(3 * t + 3);
            if (ix == 24) {
                static bool once = false;
                guard_todo(once, "seg4:0x7636's full team 7 writes DGROUP:0x5000 (kept in ContactWords)", "seg4:0x7832");
            }
            cw.w4fd0[ix] = w.wave;
            cw.w6b10[ix] = design_word();
        }
    }
    if (ship == g.player) return;  // [BYTES seg4:0x78c9-0x78cf]
    if (cr == ship) {              // [BYTES seg4:0x78d3-0x78df]
        x.h.message(g.player, kLiteralShips, ship, 0x89);
        return;
    }
    if (cr == g.player) {  // [BYTES seg4:0x78e2-0x7923]
        if (ship_team(w, g.player) == t && kill == 0)
            x.h.message(g.player, kLiteralShips, ship, 0x63);
        else if (kill == 0)
            x.h.message(g.player, kLiteralShips, ship, 0x5f);
        return;
    }
    // [BYTES seg4:0x7925-0x7975]: both branches rewrite +0x06 / +0x08 first
    S.w06 = kLiteralShips;
    S.w08 = static_cast<std::int16_t>(cr);
    if (kill == 0) x.h.message(g.player, kLiteralShips, ship, ship_team(w, g.player) == t ? 0x60 : 0x61);
}

// ---- seg8:0x7ce0

void assist_score(CollideCtx& x, int tp, int tr, std::int16_t a, std::int16_t res) {
    World& w = x.w;
    std::int16_t n = 0;  // [bp-6]
    for (int k = 0; k < 8; ++k) {  // [BYTES seg8:0x7cf7-0x7d51]
        const std::size_t i = static_cast<std::size_t>(k);
        if (static_cast<std::int8_t>(w.hit_pool[i]) != tp || static_cast<std::int8_t>(w.hit_rec[i]) != tr) continue;
        if (w.w937a[i] == 0) n = add16(n, 1);
        // seg5:0x2b4(frame#, 10) − sext(word), the high word 0 and the low word below 20, unsigned
        // [BYTES seg8:0x7d1c-0x7d46]
        const std::uint32_t q = w.frames / 10u - static_cast<std::uint32_t>(static_cast<std::int32_t>(w.w937a[i]));
        if ((q >> 16) == 0 && (q & 0xffffu) < 20u) n = add16(n, 3);
    }
    if (n > 6) kill_score(x, tp, tr, a, res);  // seg8:0x7be8 [BYTES seg8:0x7d53-0x7d66]
}

// ---- seg8:0x2d96-0x2e5b

void wing_taunts(CollideCtx& x, int ship) {
    World& w = x.w;
    const int ships = x.g.ship_pool;  // DGROUP:0x9324
    for (int j = 0; j <= w.last_ship; ++j) {
        const ShipRecord& J = sh(w, j);
        if (J.w32 != ships || J.w34 != ship || J.w38 != 1) continue;  // [BYTES seg8:0x2e1d-0x2e39]
        const ShipRecord& S = sh(w, ship);
        const std::uint8_t sac = ship_b(S, 0xac), sad = ship_b(S, 0xad);
        const std::uint8_t jac = ship_b(J, 0xac), jad = ship_b(J, 0xad);
        int code;
        if (jac == sac && jad == sad) {  // [BYTES seg8:0x2e3b-0x2e57]
            code = 0x1b;
        } else if (sac != 0 || sad != 0) {  // [BYTES seg8:0x2da6-0x2de7]
            if (jac == 0 && jad == 0)
                code = 0x1e;
            else
                code = strength_ratio(w, ship, j) > 0x5f ? 0x1c : 0x19;
        } else {  // [BYTES seg8:0x2de9-0x2dfe]
            code = strength_ratio(w, ship, j) > 0x5f ? 0x1d : 0x1a;
        }
        x.h.message(j, ships, ship, code);  // seg3:0x6ec(j, DGROUP:0x9324, ship, code) [BYTES seg8:0x2e00-0x2e0f]
    }
}

}  // namespace is::world
