// The tick calls that are the computer pilots' (src/world/pilot_tick.h). Each body was read from
// the listing (work/world/m5s3/think-report.md "Readings no verifier has checked" for what
// s3-tick-verify.md does not cover).
#include "world/pilot_tick.h"

#include "world/world.h"

namespace is::world {

namespace {

struct Once {
    bool last = false, ship = false;
} g_once;

bool ship_ok(int j) { return j >= 0 && j < kShipCount; }

// DGROUP:0x9316 as a loop bound over the ship table: past 7 the original reads other DGROUP
// blocks; the port stops at 7 [DESIGN ADR 0016 decision 2].
int last_ship(PilotCtx& c) {
    if (c.g.last_ship < kShipCount) return c.g.last_ship;
    guard_todo(g_once.last, "DGROUP:0x9316 past the ship table", "seg4:0x7f63");
    return kShipCount - 1;
}

ShipRecord& ship_or_zero(PilotCtx& c, int j, const char* address) {
    if (ship_ok(j)) return c.s(j);
    static ShipRecord zero;
    zero = ShipRecord{};
    guard_todo(g_once.ship, "a ship index outside the ship table", address);
    return zero;
}

// seg8:0x285a between two objects' positions (the distance only; the order of the points does
// not change it) [BYTES seg8:0x285a].
std::int16_t cells(const ObjectRecord& a, const ObjectRecord& b) {
    std::int16_t dx = 0, dy = 0;
    return distance_cells(b.x, b.y, a.x, a.y, dx, dy);
}

}  // namespace

// --- seg4:0x6700, the rivals for s's capture -------------------------------------------------
void rivals(PilotCtx& c, int s) {
    if (!ship_ok(s)) return;
    WaveGlobals& g = c.g;
    Pilot S = c.p(s);
    const std::int16_t gp = S.est_pool(), gr = S.est_rec();  // [bp-0xc], [bp-0xe] [BYTES seg4:0x670f-0x671f]
    if (c.obj(gp, gr).state != 0) return;                     // [BYTES seg4:0x6722-0x6733]
    bool found = false;                                       // [bp-0x10] [BYTES seg4:0x6737-0x673c]
    const int last = last_ship(c);
    for (int j = 0; j <= last; ++j) {  // [BYTES seg4:0x69d4-0x69dd]
        ShipRecord& Js = c.s(j);
        Pilot J(Js);
        const ObjectRecord& Oj = c.obj(kPoolShips, j);  // DGROUP:0x1e30, the literal pool 14
        // The claim [BYTES seg4:0x69e1-0x6a65 seg4:0x6742-0x6763]
        if (Oj.state == 0 && Js.bec != 0 && J.order() != 0x0e && J.goal_pool() == gp && g.wave_kind != 6 &&
            j != s && J.goal_rec() == gr && c.obj(gp, gr).b53 != 0 &&
            !(J.claim_pool() == gp && J.claim_rec() == gr)) {
            // seg3:0x6ec(j, 14, s, code): 0x66, 0xbf for a team-7 j [BYTES seg4:0x6a55-0x6a6c]
            c.host.message(j, kPoolShips, s, c.team(j) == 7 ? 0xbf : 0x66);
            J.claim_pool(static_cast<std::int8_t>(gp & 0xff));  // [BYTES seg4:0x6754-0x6763]
            J.claim_rec(static_cast<std::int8_t>(gr & 0xff));
        }
        // The rival [BYTES seg4:0x6767-0x69cf]
        const std::int16_t tj = c.team(j);  // [bp-6]
        if (J.order() == 0x0e || found) continue;
        if (tj != 2 && tj != 5) continue;
        if (Oj.state != 0 || Js.bec == 0 || j == s) continue;
        if (j == g.player && g.demo != 0) continue;            // [BYTES seg4:0x67ba-0x67c7]
        if (J.order() == 2 && tj != 2) continue;               // [BYTES seg4:0x67cb-0x67db]
        if (!(c.host.relation(j, s) < 160)) {                  // seg13:0x2688(j, s) [BYTES seg4:0x67df-0x67f0]
            const int off = pw::kGrudge0 + s;                  // byte +0xc2 + s, signed `jle`
            if (!(ship_sb(Js, off) > 0)) continue;
        }
        const std::int16_t ts = c.team(s);
        const bool after_g = J.goal_pool() == gp && J.goal_rec() == gr;
        if (ts != tj && !after_g && !(tj == 2 && ts == 0)) {  // [BYTES seg4:0x6803-0x6848]
            J.goal_pool(gp);  // [BYTES seg4:0x684c-0x6863]
            J.est_pool(gp);
            J.goal_rec(gr);
            J.est_rec(gr);
            // seg8:0x285a(s's position, j's position) < 1900 [BYTES seg4:0x6867-0x6893]
            if (cells(c.obj(kPoolShips, s), Oj) < 0x76c) {
                J.est_act(2);  // `mov dword [+0x36], 0x20002` [BYTES seg4:0x6895]
                J.goal_act(2);
                c.host.message(s, kPoolShips, j, 0x17);  // seg3:0x6ec(s, 14, j, 0x17) [BYTES seg4:0x689e-0x68ad]
            } else {
                J.goal_act(3);  // [BYTES seg4:0x68b2-0x68be]
                J.est_act(3);
            }
            J.goal_app(2);  // [BYTES seg4:0x68c2-0x68f7]
            J.est_app(2);
            J.order(0x0e);
            J.order_pool(0x0e);
            J.order_rec(static_cast<std::int16_t>(s));
            J.replan_at(g.now + 500u);
            if (j == g.player) c.host.steal_text(gp, gr, s);  // [BYTES seg4:0x68fb-0x69af]
            found = true;                                      // [BYTES seg4:0x69b3]
        }
        if (J.goal_pool() == gp && J.goal_rec() == gr) found = true;  // [BYTES seg4:0x69b8-0x69cf]
    }
}

// --- seg13:0x3302, the capture-claim reset ---------------------------------------------------
void claim_reset(PilotCtx& c) {
    WaveGlobals& g = c.g;
    if (g.wave_kind == 6) return;  // [BYTES seg13:0x3310-0x3315]
    const int last = last_ship(c);
    for (int i = 0; i <= last; ++i) {  // [BYTES seg13:0x33c1-0x33ca]
        Pilot I = c.p(i);
        // A claim kept by a ship still after its capture goal survives [BYTES seg13:0x3321-0x3341]
        if (I.goal_act() == 2 && I.claim_pool() == I.goal_pool() && I.claim_rec() == I.goal_rec()) continue;
        const Pilot Pl(ship_or_zero(c, g.player, "seg13:0x3343"));
        if (Pl.goal_act() != 1 && I.claim_pool() == Pl.goal_pool() && I.claim_rec() == Pl.goal_rec() &&
            i != g.player) {  // [BYTES seg13:0x3343-0x3371]
            // seg13:0x2688(i, player) > 0, or the player's teammate [BYTES seg13:0x3373-0x339c]
            if (c.host.relation(i, g.player) > 0 || c.team(g.player) == c.team(i))
                c.host.message(g.player, kPoolShips, i, 0xb8);  // [BYTES seg13:0x339e-0x33af]
        }
        I.claim_rec(-1);   // 0xff [BYTES seg13:0x33b2-0x33bd]
        I.claim_pool(-1);
    }
}

// --- seg4:0x7e2a, the call-ins ---------------------------------------------------------------
void callins(PilotCtx& c) {
    WaveGlobals& g = c.g;
    World& w = c.w;
    PilotGlobals& pg = c.pg;
    const std::uint32_t now = g.now;
    if (g.dg_913a != 0 || g.dg_913c != 0) return;  // [BYTES seg4:0x7e38-0x7e44]
    if (g.game_frames - pg.callin_frame < 400u) return;  // 32-bit unsigned `jae` [BYTES seg4:0x7e46-0x7e55]
    if (g.game_frames < 500u) {                          // [BYTES seg4:0x7e5a-0x7e79]
        pg.due_1d12 = 0;
        pg.knight_due = 0;
        pg.pirate_due = 0;
        return;
    }
    if (g.demons_gate != 0 || g.wave_kind == 6 || g.wave_kind == 7) return;  // [BYTES seg4:0x7e7c-0x7e8f]
    if (add16(w.w992, 20) > w.budget) return;                                // [BYTES seg4:0x7e91-0x7e9b]
    const int player = g.player;
    const ShipRecord& P = ship_or_zero(c, player, "seg4:0x7ea0");
    const std::int16_t tp = c.team(player);
    const int last = last_ship(c);
    // Detail/2 (`cdq; sub ax, dx; sar ax, 1`, toward zero) [BYTES seg4:0x7ed1-0x7ed7]
    const std::int16_t half = idiv16(g.detail, 2);

    // ---- the Pirate [BYTES seg4:0x7ea0-0x803b]
    if (c.host.team_word(3, tp) < 150 && (pg.pirate_due < now || pg.pirate_due == 0) &&
        add16(half, 3) > g.last_ship && P.resources() > 398) {
        std::int16_t n = 2, q = 0;  // [bp-0x10], [bp-0xe]
        for (int j = 0; j <= last; ++j) {  // [BYTES seg4:0x7f05-0x7f69]
            const ShipRecord& Js = c.s(j);
            const Pilot J(c.s(j));
            if (c.team(j) == 3) {
                n = sub16(n, 1);
                if (ship_sb(Js, pw::kCalledFor) == player) n = 0;
            }
            if (J.goal_act() == 1 && J.goal_pool() == 14 && c.obj(kPoolShips, j).on_screen != 0) {
                q = add16(q, 1);
                if (J.goal_rec() == player) q = add16(q, 1);
            }
        }
        if (n > 0) {  // [BYTES seg4:0x7f6b-0x7f6f]
            // (resources - 220)/32 toward zero (`cdq; and dx, 0x1f; add; sar 5`) + T(3, tp)/-100 +
            // DGROUP:0x9334/3 - q - DGROUP:0x9316 [BYTES seg4:0x7f73-0x7fb9]
            std::int16_t cc = idiv16(sub16(P.resources(), 0xdc), 32);
            cc = add16(cc, idiv16(c.host.team_word(3, tp), -100));
            cc = add16(cc, idiv16(w.score_level, 3));
            cc = sub16(cc, q);
            cc = sub16(cc, g.last_ship);
            if (P.w26 < 40) cc = add16(cc, 1);  // [BYTES seg4:0x7fbc-0x7fc4]
            const std::int16_t r = static_cast<std::int16_t>(c.rng.rand() % 720);  // [BYTES seg4:0x7fc7-0x7fd0]
            cc = idiv16(mul16(n, cc), 2);  // `imul; cdq; sub ax, dx; sar ax, 1` [BYTES seg4:0x7fd2-0x7fdf]
            if (cc > r && w.score_level > 0) {  // [BYTES seg4:0x7fe2-0x7feb]
                if (pg.pirate_due != 0 && pg.pirate_due < now) {  // [BYTES seg4:0x7fed-0x7ffe]
                    c.host.call_in(player, 3, 0x69);  // seg13:0xadbe(player, 3, 0x69) [BYTES seg4:0x8000-0x800d]
                    pg.pirate_due = 0;                // seg4:0x7e70 [BYTES seg4:0x8010]
                    return;
                }
                if (pg.pirate_due == 0) {  // "You're Loaded With Stuff. Watch Out for Thieves!" [BYTES seg4:0x8013-0x803b]
                    c.host.message(player, 7, 0, 0x6b);
                    pg.pirate_due = now + 7000u;
                    return;
                }
            }
        }
    }

    // ---- the Knight [BYTES seg4:0x803e-0x8286]
    if (c.host.team_word(4, tp) > -50 && (pg.knight_due < now || pg.knight_due == 0) &&
        add16(half, 3) > g.last_ship && P.w26 < 60) {
        std::int16_t n = 2, f = 0, h = 0;  // [bp-0x10], [bp-8], [bp-6]
        for (int j = 0; j <= last; ++j) {  // [BYTES seg4:0x80a0-0x817b]
            if (j == player || c.s(j).bec == 0) continue;
            const ShipRecord& Js = c.s(j);
            const Pilot J(c.s(j));
            const std::int16_t tj = c.team(j);
            // The helpers of the player's kind: team 0 for a team-2 player, team 3 for team 3,
            // team 4 otherwise [BYTES seg4:0x815d-0x817b seg4:0x80a0-0x80d3]
            const bool kind = tp == 2 ? tj == 0 : tp == 3 ? tj == 3 : tj == 4;
            if (kind) {
                n = sub16(n, 1);
                if (ship_sb(Js, pw::kCalledFor) == player) n = 0;
            }
            if (tp == tj) f = add16(f, 1);  // [BYTES seg4:0x80e8-0x8109]
            if (J.goal_act() == 1 && J.goal_pool() == 14 && J.goal_rec() == player) {  // [BYTES seg4:0x810c-0x813b]
                h = add16(h, 1);
                if (c.host.strength(player, j) < 40) h = add16(h, 1);  // seg4:0x5d9c(player, j)
            }
        }
        for (int k = 0; k < c.pools.in_use(2); ++k) {  // the defenders guarding the player [BYTES seg4:0x8185-0x81bb]
            if (c.obj(2, k).state != 0) continue;
            if (c.host.defender_word(k, 0x0a) == 14 && c.host.defender_word(k, 0x0c) == player &&
                c.host.defender_word(k, 0) == 10)
                h = add16(h, 1);
        }
        if (n > 0) {  // [BYTES seg4:0x81bd-0x81c1]
            // (T(4, tp)/120 + 2(2h - f) - Engine/15 + DGROUP:0x9334/2 - DGROUP:0x9316 + 5)·n/2
            // [BYTES seg4:0x81c5-0x8216]
            std::int16_t cc = idiv16(c.host.team_word(4, tp), 120);
            cc = add16(cc, shl16(sub16(shl16(h, 1), f), 1));
            cc = sub16(cc, idiv16(P.w26, 15));
            cc = add16(idiv16(w.score_level, 2), cc);
            cc = sub16(cc, g.last_ship);
            cc = add16(cc, 5);
            cc = idiv16(mul16(cc, n), 2);
            if (h != 0) {  // [BYTES seg4:0x8219-0x821d]
                const std::int16_t r = static_cast<std::int16_t>(c.rng.rand() % 300);  // [BYTES seg4:0x821f-0x8228]
                if (r < cc) {
                    if (pg.knight_due != 0 && pg.knight_due < now) {  // [BYTES seg4:0x822f-0x825b]
                        c.host.call_in(player, 4, 0x6a);
                        pg.knight_due = 0;
                        return;
                    }
                    if (pg.knight_due == 0) {  // "Hang In There, You Could Get Help Anytime Now." [BYTES seg4:0x825e-0x8286]
                        c.host.message(player, 7, 0, 0x6c);
                        pg.knight_due = now + 7000u;
                        return;
                    }
                }
            }
        }
    }

    // ---- the Patrolman [BYTES seg4:0x8289-0x833a]
    if (!(add16(idiv16(g.detail, 2), 4) > g.last_ship)) return;
    if (!(w.w964a > 6) || !(pg.crimes_wave > 4) || !(g.detail > 0)) return;
    bool no_enforcer = true;  // [bp-0x10]
    for (int j = 0; j <= last; ++j)
        if (c.team(j) == 7) no_enforcer = false;
    const std::int16_t cc = add16(add16(idiv16(w.w964a, 2), idiv16(c.host.team_word(7, tp), -140)),
                                  idiv16(w.score_level, 2));  // [BYTES seg4:0x82e0-0x8314]
    if (!no_enforcer) return;
    const std::int16_t r = static_cast<std::int16_t>(c.rng.rand() % 300);  // [BYTES seg4:0x831d-0x8326]
    if (r < cc) c.host.call_in(player, 0x16, 0x6d);  // "Abide By The Law, The Patrolman Is Entering."
}

// --- seg4:0x83ae, the hunters' guess at a cloaked ship ----------------------------------------
void cloak_guess(PilotCtx& c) {
    const int last = last_ship(c);
    const std::int16_t wx = shl16(static_cast<std::int16_t>(kWorldCells), 3);  // 8·DGROUP:0x5a0
    const std::int16_t wy = shl16(static_cast<std::int16_t>(kWorldCells), 3);  // 8·DGROUP:0x5a2
    for (int t = 0; t <= last; ++t) {  // [BYTES seg4:0x84af-0x84cb]
        ShipRecord& Ts = c.s(t);
        if (Ts.bec == 0 || Ts.be8 == 0) continue;
        Pilot T(Ts);
        for (int j = 0; j <= last; ++j) {  // [BYTES seg4:0x8407-0x8433]
            if (j == t) continue;
            const ShipRecord& Js = c.s(j);
            const Pilot J(c.s(j));
            if (Js.bec == 0 || J.goal_pool() != 14 || J.goal_rec() != t) continue;
            const ObjectRecord& O = c.obj(kPoolShips, t);
            std::int16_t dx = 0, dy = 0;
            // seg8:0x285a(c's position, the guess) [BYTES seg4:0x8435-0x845b]
            const std::int16_t d = distance_cells(T.guess_x(), T.guess_y(), O.x, O.y, dx, dy);
            if (!(d > 0x15e)) continue;  // [BYTES seg4:0x845e-0x8461]
            // rand() % 4000 + x - 2000, 16-bit [BYTES seg4:0x8463-0x847a]
            std::int16_t x = sub16(add16(static_cast<std::int16_t>(c.rng.rand() % 4000), O.x), 0x7d0);
            T.guess_x(x);
            std::int16_t y = sub16(add16(static_cast<std::int16_t>(c.rng.rand() % 4000), O.y), 0x7d0);  // [BYTES seg4:0x847e-0x8495]
            T.guess_y(y);
            // One wrap each: x < 0 adds, else x > 8·DGROUP:0x5a0 subtracts; y likewise
            // [BYTES seg4:0x8499-0x84ac seg4:0x83c5-0x8403]
            if (x < 0)
                x = add16(x, wx);
            else if (x > wx)
                x = sub16(x, wx);
            T.guess_x(x);
            if (y < 0)
                y = add16(y, wy);
            else if (y > wy)
                y = sub16(y, wy);
            T.guess_y(y);
        }
    }
}

// --- seg13:0x675e, the watch windows' refresh ------------------------------------------------
void watch_refresh(PilotCtx& c) {
    WaveGlobals& g = c.g;
    const std::uint32_t now = g.now;
    const int last = last_ship(c);
    for (int i = 0; i <= last; ++i) {  // [BYTES seg13:0x67e7-0x67f0]
        Pilot I = c.p(i);
        // INI ShipAnim 0: only once +0x156 + 10000 < now (32-bit `jae`) [BYTES seg13:0x6773-0x678f]
        if (c.host.ship_anim() == 0 && !(I.due156() + 10000u < now)) continue;
        // +0x156 < now (high word, then low) and not the player, or the object out of play
        // (pool DGROUP:0x9324) [BYTES seg13:0x6791-0x67c7]
        const bool due = I.due156() < now && i != g.player;
        if (!due && c.obj(g.ship_pool, i).state == 0) continue;
        I.due156(now + 5000u);  // `add 0x1388; adc dx, 0` [BYTES seg13:0x67c9-0x67db]
        c.host.watch_ship(i);   // seg13:0x6890(i) [BYTES seg13:0x67df-0x67e3]
    }
    c.host.watch_tail();  // [BYTES seg13:0x67f2-0x6821]
    // The All Ships notice [BYTES seg13:0x6824-0x6881]
    if (c.host.smart_window() || g.last_ship == 0 || c.host.smwnd() == 0) return;
    int n = 0;  // DGROUP:0x95fa
    for (int i = 0; i <= last; ++i)
        if (i != g.player && c.host.visible(g.ship_pool, i) != 0) ++n;
    c.host.all_ships_notice(n);
}

}  // namespace is::world
