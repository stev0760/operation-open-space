// What a computer pilot decides (src/world/pilot_think.h). The think seg4:0x78 follows
// s3-tick.md 2.2 as corrected by s3-tick-verify.md (claims 25-33 and the Omissions on seg4:0x78);
// the goal choice seg4:0x2f98 and the greed level seg4:0x84de are this track's readings of the
// listing (work/world/m5s3/think-report.md, "Readings no verifier has checked").
#include "world/pilot_think.h"

#include <array>

#include "world/pilot_orders.h"
#include "world/pilot_tick.h"
#include "world/world.h"

namespace is::world {

namespace {

// The one-time logs, one flag per site.
struct Once {
    bool obj = false, ship = false, grudge = false, last = false, divisor = false, pod = false;
    bool design = false, relation = false, strength = false, team = false, message = false;
    bool target = false, visible = false, executor = false, wsys = false, wuse = false;
    bool thrust = false, gun = false, callin = false, gate = false, ambulance = false;
    bool touched = false, wdisplay = false, revenge = false, reactions = false, gate_goal = false;
    bool report = false, systems = false, voice = false, s9ae8 = false, s9ce0 = false;
    bool s474c = false, strategy = false, demons = false, duel = false, manual = false;
    bool pod_word = false, tallies = false, greed_div = false;
};
Once g_once;

bool ship_ok(int j) { return j >= 0 && j < kShipCount; }

// A ship index from a record word: outside the table the reads see a zeroed scratch record
// [DESIGN ADR 0016 decision 2].
ShipRecord& ship_or_zero(PilotCtx& c, int j, const char* address) {
    if (ship_ok(j)) return c.s(j);
    static ShipRecord zero;
    zero = ShipRecord{};
    guard_todo(g_once.ship, "a ship index outside the ship table", address);
    return zero;
}

// The last ship index DGROUP:0x9316 as a loop bound over the ship table: past 7 the original's
// loops read other DGROUP blocks; the port stops at 7 [DESIGN ADR 0016 decision 2].
int last_ship(PilotCtx& c) {
    if (c.g.last_ship < kShipCount) return c.g.last_ship;
    guard_todo(g_once.last, "DGROUP:0x9316 past the ship table", "seg4:0x367b");
    return kShipCount - 1;
}

// The 32-bit arithmetic of the goal choice (ADR 0013 decision 7).
std::int32_t wrap32(std::int64_t v) {
    const std::uint32_t u = static_cast<std::uint32_t>(static_cast<std::uint64_t>(v));
    return u < 0x80000000u ? static_cast<std::int32_t>(u) : -static_cast<std::int32_t>(~u) - 1;
}
std::int32_t add32(std::int32_t a, std::int32_t b) { return wrap32(std::int64_t{a} + b); }
std::int32_t sub32(std::int32_t a, std::int32_t b) { return wrap32(std::int64_t{a} - b); }
// seg5:0x314 and `imul ecx`: the low 32 bits of the product [BYTES seg5:0x314-0x343].
std::int32_t mul32(std::int32_t a, std::int32_t b) { return wrap32(std::int64_t{a} * b); }
// seg5:0x110 and `cdq; idiv ecx`: truncating; the divisors here are constants other than 0, -1.
std::int32_t ldiv32(std::int32_t a, std::int32_t b) { return a / b; }
// seg5:0x8c0 (`shl ax; rcl dx`) and `shl eax, cl` [BYTES seg5:0x8c0-0x8ca].
std::int32_t shl32(std::int32_t a, int n) {
    return wrap32(static_cast<std::int64_t>(static_cast<std::uint32_t>(a) << n) & 0xffffffffLL);
}
std::int32_t sx(std::int16_t v) { return v; }

std::int32_t money(const ShipRecord& s) {  // sext(word +0xce) + dword +0xdc (`movsx; add eax`)
    return add32(sx(ship_w(s, pw::kResources)), static_cast<std::int32_t>(ship_d(s, pw::kCredit)));
}

void log_once(bool& once, const char* what, const char* address) { guard_todo(once, what, address); }

}  // namespace

// --- PilotHost defaults -------------------------------------------------------------------------
std::int16_t PilotHost::design_word(int, int) {
    log_once(g_once.design, "the design table (the roster track's)", "DGROUP:0x96d2");
    return 0;
}
std::int16_t PilotHost::relation(int, int) {
    log_once(g_once.relation, "the attitude read (the contact track's)", "seg13:0x2688");
    return 0;
}
std::int16_t PilotHost::strength(int, int) {
    log_once(g_once.strength, "the strength ratio (the contact track's)", "seg4:0x5d9c");
    return 0;
}
std::int16_t PilotHost::team_word(int, int) {
    log_once(g_once.team, "the team table (stage 4)", "DGROUP:0x3824");
    return 0;
}
void PilotHost::message(int, int, int, int) { log_once(g_once.message, "a pilot's message", "seg3:0x6ec"); }
void PilotHost::target(int, int, int) { log_once(g_once.target, "the target words", "seg3:0xf4"); }
std::int16_t PilotHost::visible(int, int) {
    log_once(g_once.visible, "the visibility test", "seg3:0x0");
    return 0;
}
void PilotHost::set_manual_target() { log_once(g_once.manual, "the target display's MANUAL flag", "DGROUP:0xa62"); }
std::int16_t PilotHost::weapon_system(int) {
    log_once(g_once.wsys, "the weapon system decision", "seg4:0x5654");
    return 0;
}
void PilotHost::weapon_use(int, int, int) { log_once(g_once.wuse, "the weapon use", "seg4:0x584c"); }
void PilotHost::thrust(int, int) { log_once(g_once.thrust, "a computer ship's thrust step", "seg11:0x5ab6"); }
void PilotHost::gun(int) { log_once(g_once.gun, "a computer ship's gun", "seg11:0x4e0a"); }
std::int16_t PilotHost::call_in(int, int, int) {
    log_once(g_once.callin, "a call-in (the entry track's)", "seg13:0xadbe");
    return 0;
}
void PilotHost::through_gate(int, int) { log_once(g_once.gate, "an exit through the gate", "seg12:0x2288"); }
std::int16_t PilotHost::ambulance_call(int) {
    log_once(g_once.ambulance, "a computer ship's ambulance call", "seg16:0xb4e");
    return 0;
}
void PilotHost::touched(int) { log_once(g_once.touched, "ship_touched", "seg13:0x6716"); }
void PilotHost::weapon_display() { log_once(g_once.wdisplay, "the player's weapon display", "seg11:0x2f8c"); }
std::int16_t PilotHost::pod_word(int, int) {
    log_once(g_once.pod_word, "the pod table (effects.h pods)", "DGROUP:0x8560");
    return 0;
}
void PilotHost::clear_tallies() {
    log_once(g_once.tallies, "the player's hit tallies DGROUP:0x9680/0x9682/0x8e00/0x8e04", "seg4:0x41ff");
}
void PilotHost::reactions(int) { log_once(g_once.reactions, "the reactions to other ships (stage 4)", "seg13:0x7ba2"); }
void PilotHost::gate_goal(int) { log_once(g_once.gate_goal, "the gate goal (stage 4)", "seg13:0x3678"); }
void PilotHost::command_report() { log_once(g_once.report, "the command report (stage 4)", "seg13:0xa65a"); }
void PilotHost::seg13_9ae8(int) { log_once(g_once.s9ae8, "seg13:0x9ae8 (stage 4)", "seg13:0x9ae8"); }
void PilotHost::seg13_9ce0(int) { log_once(g_once.s9ce0, "seg13:0x9ce0 (stage 4)", "seg13:0x9ce0"); }
void PilotHost::seg4_474c(int, int, int, int) { log_once(g_once.s474c, "seg4:0x474c (stage 4)", "seg4:0x474c"); }
void PilotHost::demons_gate(int) { log_once(g_once.demons, "the think's Demon's Gate block (M6)", "seg4:0x174"); }
void PilotHost::duel(int) { log_once(g_once.duel, "the think's duel blocks (M6)", "seg4:0x24f"); }

// --- The context ----------------------------------------------------------------------------------
ObjectRecord& PilotCtx::obj(int pool, int rec) {
    if (pool >= 0 && pool < kPoolCount && rec >= 0 && rec < pools.records(pool)) return pools.rec(pool, rec);
    static ObjectRecord zero;
    zero = ObjectRecord{};
    guard_todo(g_once.obj, "a pilot's object index outside the pools", "seg4:0x78");
    return zero;
}

std::int16_t PilotCtx::team(int i) {
    if (!ship_ok(i)) {
        guard_todo(g_once.ship, "a ship index outside the ship table", "DGROUP:0x970a");
        return host.design_word(0, 0x38);
    }
    return host.design_word(s(i).design_index, 0x38);
}

std::int16_t cells_between(const ObjectRecord& first, std::int16_t sx2, std::int16_t sy2,
                           std::int16_t& dx, std::int16_t& dy) {
    // dx = (second.x >> 3) - (first.x >> 3) [BYTES seg8:0x2872-0x2888]
    return distance_cells(sx2, sy2, first.x, first.y, dx, dy);
}

std::int16_t gate_kind(std::int16_t pool, std::int16_t rec) {
    if (pool != 12) return 0;            // [BYTES seg13:0x3729-0x3731]
    return (rec & 1) == 0 ? 1 : 2;       // `test byte [bp+8], 1` [BYTES seg13:0x3733-0x373e]
}

namespace {

// seg8:0x285a from object `first` to object `second` (pushed in that order).
std::int16_t cells(const ObjectRecord& first, const ObjectRecord& second) {
    std::int16_t dx = 0, dy = 0;
    return cells_between(first, second.x, second.y, dx, dy);
}

bool held_by_ambulance(PilotCtx& c, int i) {  // i = DGROUP:0x952e with 1 <= DGROUP:0x9e49 <= 6
    return i == c.g.dg_952e && c.g.dg_9e49 >= 1 && c.g.dg_9e49 <= 6;
}

// DGROUP:0x8680 + 2k (World::w8680) with the port's bound.
std::int16_t served(PilotCtx& c, int k, const char* address) {
    if (ship_ok(k)) return c.w.w8680[static_cast<std::size_t>(k)];
    guard_todo(g_once.ship, "DGROUP:0x8680 past its 8 words", address);
    return 0;
}

// The nearest-gate loop of seg4:0x429f and seg4:0x4648: for k = 0, 2, ... < 2·DGROUP:0x9318,
// d = seg8:0x285a(own, pool-12 record k) widened by `cwd`; each record nearer than the best so
// far becomes the estimate (12, k) with message 0xb7 [BYTES seg4:0x42a7-0x433c seg4:0x4678-0x4702].
void head_for_nearest_gate(PilotCtx& c, int i, std::int32_t best, bool with_9ae8) {
    Pilot P = c.p(i);
    for (int k = 0; k < 2 * c.g.gates; k += 2) {
        const std::int32_t d = sx(cells(c.obj(kPoolShips, i), c.obj(kPoolGateCentres, k)));
        if (!(d < best)) continue;
        if (with_9ae8) c.host.seg13_9ae8(i);  // [BYTES seg4:0x42fd]
        best = d;
        c.host.message(c.g.player, kPoolShips, i, 0xb7);  // "I'm Heading For The Exit" [BYTES seg4:0x430b-0x4317]
        P.est_pool(12);
        P.est_rec(static_cast<std::int16_t>(k));
        P.est_act(2);
        P.est_app(2);
    }
}

}  // namespace

// --- seg4:0x84de, the greed level -------------------------------------------------------------
void greed_level(PilotCtx& c) {
    std::int16_t v = add16(mul16(c.w.score_level, 7), 0x2d);  // [BYTES seg4:0x84ec-0x84f1]
    if (v > 100) v = 100;
    std::int16_t& gr = c.pg.greed;
    gr = v;  // [BYTES seg4:0x84fc]
    const std::uint32_t frames = c.g.game_frames;  // DGROUP:0x1e68
    if (frames < 500u) {  // [BYTES seg4:0x84ff-0x8529]
        const std::uint32_t t = (700u - frames) >> 3;  // `sub eax; shr eax, 3`
        gr = sub16(v, to_i16(t));
    } else {  // [BYTES seg4:0x852f-0x8557]: frame#/100 (seg5:0x2b4, unsigned) + v, at most 100
        const std::uint32_t q = frames / 100u;
        const std::uint32_t s = q + static_cast<std::uint32_t>(static_cast<std::int32_t>(v));
        gr = (s > 100u) ? std::int16_t{100} : to_i16(s);  // `or bx, bx; jne` then `cmp cx, 0x64; jbe`
    }
    if (gr < 3) gr = 3;  // [BYTES seg4:0x855b-0x8562]
    const ShipRecord& Pl = ship_or_zero(c, c.g.player, "seg4:0x8568");
    const std::int32_t m = money(Pl);
    if (m < 100) gr = sub16(gr, 20);    // [BYTES seg4:0x8579-0x8581]
    if (m > 250) gr = add16(gr, 20);    // [BYTES seg4:0x8597-0x859f]
    if (m > 1000) gr = add16(gr, 30);   // [BYTES seg4:0x85b5-0x85bd]
    // The live icons among pool-0 records 0 ... 7 (`state` 0, whatever the in-use count)
    // [BYTES seg4:0x85c2-0x85e5].
    std::int16_t n = 0;
    for (int k = 0; k < 8; ++k)
        if (c.obj(kPoolIcons, k).state == 0) n = add16(n, 1);
    // (n·10)/(DGROUP:0x9316 + 1) [BYTES seg4:0x85e7-0x85f1]; a divisor of 0 (DGROUP:0x9316 = -1, no
    // wave) would trap: the port takes r = 0 [DESIGN ADR 0016].
    const std::int16_t div = add16(c.g.last_ship, 1);
    std::int16_t r = 0;
    if (div == 0)
        guard_todo(g_once.greed_div, "a zero divisor in the greed level", "seg4:0x85f1");
    else
        r = idiv16(mul16(n, 10), div);
    if (r < 15) gr = sub16(gr, 10);
    if (r < 10) gr = sub16(gr, 10);
    if (r < 5) gr = sub16(gr, 10);
    if (c.w.w9000 == 0) gr = -10;  // [BYTES seg4:0x8611-0x8618]
}

// --- seg4:0x2f98, the goal choice ----------------------------------------------------------------
void choose_goal(PilotCtx& c, int i) {
    WaveGlobals& g = c.g;
    Pilot P = c.p(i);
    ShipRecord& S = c.s(i);
    // [BYTES seg4:0x2fa7-0x2fc5]: an order of -1 becomes 0 and keeps the old goal out of the search
    bool keep = false;  // [bp-0x5e]
    if (P.order() == -1) {
        keep = true;
        P.order(0);
    }
    P.replan_at(g.now + 3000);                 // [BYTES seg4:0x2fc5-0x2fdb]
    const std::int16_t team = c.team(i);       // [bp-0x12] [BYTES seg4:0x2fdf-0x2fe9]
    // The ambulance-served ship while the ambulance (pool 7 record 0) is in play [BYTES seg4:0x2fec-0x302f]
    if (c.obj(kPool7, 0).state == 0 && i == g.dg_952e) {
        P.order(7);
        P.order_until(g.now + 10000);
        P.order_by(static_cast<std::int16_t>(i));
        P.d9c(g.now + 5000);
        P.one_shot(0);
        return;
    }
    // Enforcers [BYTES seg4:0x3032-0x3146]
    if (team == 7) {
        if (P.cls() >= 2) {  // Patrol class
            if (P.goal_pool() == 11 && c.obj(11, P.goal_rec()).state == 0) {  // [BYTES seg4:0x30cf-0x311a]
                P.est_pool(11);
                P.est_rec(0);  // `mov dword [+0x06], 0xb`
                P.est_act(2);
                P.est_app(2);
                // seg4:0x6700(i) [BYTES seg4:0x3112-0x3116]
                rivals(c, i);
                return;
            }
            P.order(0x14);  // [BYTES seg4:0x30eb-0x30f0]
            return;
        }
        if (P.order_pool() == 14 && g.dg_913c == 0 && g.dg_913a == 0) {  // [BYTES seg4:0x3048-0x305b]
            c.host.message(g.player, kPoolShips, i, 0xb7);  // [BYTES seg4:0x311d-0x3129]
            P.est_pool(12);
            P.est_rec(0);
            P.est_act(2);
            P.est_app(2);  // [BYTES seg4:0x473a]
            return;
        }
        P.order(P.cls() == 1 ? std::int16_t{0x16} : std::int16_t{0x17});  // [BYTES seg4:0x305f-0x3078]
        P.order_pool(14);                                                 // [BYTES seg4:0x3083]
        if (P.called_for() >= 0) {
            P.order_rec(P.called_for());  // [BYTES seg4:0x3089-0x3095]
        } else if (P.order_rec() < 0 || P.order_rec() > g.last_ship) {  // [BYTES seg4:0x309b-0x30bc]
            P.order_rec(g.player);
            P.order(0x14);
        }
        P.one_shot(1);  // [BYTES seg4:0x30c7]
        return;
    }

    // ---- the ships [BYTES seg4:0x3149-0x36c4]
    std::int32_t help = 80000, att = 80000;      // [bp-0x26], [bp-0x2a]
    std::int16_t best_help = -1, best_att = -1;  // [bp-0x4c], [bp-0x4e]
    std::array<std::int16_t, kShipCount> a{};    // [bp-0x22 + 2j]: the desire to attack j
    const int last = last_ship(c);
    const std::int16_t e = static_cast<std::int16_t>(P.cls());      // cbw(+0x5e)
    const std::int16_t allies = static_cast<std::int16_t>(P.allies());  // cbw(+0x5d)
    for (int j = 0; j <= last; ++j) {
        // One draw per j, whatever follows [BYTES seg4:0x3683-0x36a1].
        a[static_cast<std::size_t>(j)] = sub16(add16(static_cast<std::int16_t>(c.rng.rand() % 2), 1), e);
        std::int16_t& aj = a[static_cast<std::size_t>(j)];
        const bool consider = !(keep && P.goal_pool() == 14 && P.goal_rec() == j);  // [bp-0x60]
        if (c.obj(kPoolShips, j).state != 0 || c.s(j).bec == 0 || j == i || !consider) continue;  // [BYTES seg4:0x316c-0x3195]
        Pilot J = c.p(j);
        // The distance from j to i plus 300 (16-bit `add`, then `cwd`) [BYTES seg4:0x3198-0x31bf]
        std::int32_t cost = sx(add16(cells(c.obj(kPoolShips, j), c.obj(kPoolShips, i)), 300));
        if (cost < 250) aj = add16(aj, 1);                     // [BYTES seg4:0x31c2-0x31d2]
        const std::int16_t str = c.host.strength(i, j);        // [bp-0xe] [BYTES seg4:0x31d5-0x31e2]
        std::int16_t rel = c.host.relation(i, j);              // [bp-0x10] [BYTES seg4:0x31e5-0x31f3]
        if (j == g.player) cost = sub32(cost, sar32(cost, 2));  // [BYTES seg4:0x31f6-0x3206]
        if (abs16(rel) > 160) cost = sub32(cost, sar32(cost, 2));  // [BYTES seg4:0x320a-0x3220]
        aj = sub16(aj, idiv16(rel, 100));                      // [BYTES seg4:0x3224-0x3232]
        if (P.revenge(j) > 3) {                                // word +0x72 + 2j [BYTES seg4:0x3235-0x3252]
            cost = sub32(cost, sar32(cost, 2));
            aj = add16(aj, 1);
        }
        if (i == g.player) aj = add16(aj, 1);  // [BYTES seg4:0x3255-0x3262]
        if (P.goal_pool() == 14 && P.goal_rec() == j) {  // [BYTES seg4:0x3265-0x32d7]
            if (P.goal_act() == 1) {
                cost = sar32(cost, 1);  // `sar dx, 1; rcr cx, 1`
                aj = add16(aj, 2);
                if (ship_b(S, 0x146) == 8) {  // byte +0x146 [BYTES seg4:0x3299]
                    aj = add16(aj, 2);
                    cost = sar32(cost, 3);
                    if (cost > 70) cost = 70;
                }
            } else {
                aj = add16(aj, 1);
                cost = sub32(cost, sar32(cost, 2));
            }
        }
        if (J.goal_pool() == 14 && J.goal_rec() == i) {  // [BYTES seg4:0x32db-0x334e]
            if (J.goal_act() == 1) {
                const std::int8_t f = P.fight(j);  // byte +0x13e + j
                if (f < -2) {
                    // -(cbw(f) idiv 2), a byte quotient, times the cost (seg5:0x314)
                    const std::int16_t q = static_cast<std::int16_t>(static_cast<std::int8_t>(f / 2));
                    cost = mul32(cost, sx(sub16(0, q)));
                } else {
                    cost = sar32(cost, 1);
                    aj = add16(aj, 2);
                    rel = sub16(rel, 300);
                }
            } else {
                aj = add16(aj, 1);
                cost = sar32(cost, 1);
            }
        }
        const std::int8_t gr = P.grudge(j);  // byte +0xc2 + j [BYTES seg4:0x3351-0x33c6]
        if (gr == -1) {
            cost = mul32(add32(cost, 1000), 5);
        } else if (gr > 0 && gr < 9) {
            cost = ldiv32(mul32(cost, sx(sub16(9, gr))), 10);
            aj = add16(aj, static_cast<std::int16_t>(static_cast<std::int8_t>(gr / 2)));
        }
        // A liked ship competes for help, the others for attack [BYTES seg4:0x33c9-0x33fa]
        const bool liked = mul16(sub16(allies, 1), 50) < rel || (team == 4 && rel > -100) || c.team(j) == team;
        if (liked) {  // [BYTES seg4:0x33fe-0x34a5]
            if (J.goal_act() == 1) {
                cost = sar32(cost, 1);
                if (J.goal_pool() == P.goal_pool() && J.goal_rec() == P.goal_rec())
                    cost = ldiv32(add32(cost, cost), 3);
            }
            // `mov al, 3; imul byte [+0x5d]`: 3·allies in 16 bits, + 4
            cost = ldiv32(mul32(cost, sx(add16(mul16(3, allies), 4))), 6);
            if (team == 4) cost = sub32(cost, sar32(cost, 2));  // seg5:0x346
            if (cost < help) {
                help = cost;
                best_help = static_cast<std::int16_t>(j);
            }
            continue;
        }
        // [BYTES seg4:0x34a8-0x3675]
        cost = ldiv32(mul32(cost, sx(add16(add16(e, 2), add16(e, 2)))), 6);
        aj = sub16(aj, e);
        if (str > 300 || str < 50) {
            cost = add32(add32(cost, 100), add32(cost, 100));
            aj = sub16(aj, 1);
        }
        if (served(c, j, "seg4:0x350d") == i) cost = add32(cost, 100000);
        if (str < 25 || S.w26 < 40) {
            cost = mul32(add32(cost, 150), 2);
            aj = sub16(aj, 2);
        }
        if (team == 2 || team == 4 || team == 5) {
            cost = mul32(add32(cost, 500), 2);
            aj = sub16(aj, 2);
        }
        if (team == 3 || team == 6) {
            cost = sub32(cost, sar32(cost, 2));
            aj = add16(aj, 1);
            if (team == 3) {
                if (J.resources() > 250) cost = sub32(cost, sar32(cost, 2));
                if (P.called_for() == j) cost = add32(cost, ldiv32(cost, -3));
            }
        }
        if (team == 1 && c.team(j) == 2) {
            cost = ldiv32(cost, 2);
            aj = add16(aj, 2);
        }
        if (P.ee(j) > 5) cost = add32(cost, 80000);  // byte +0xee + j
        if (J.cloaked() != 0) cost = shl32(cost, 2);
        if (P.goal_pool() == 14 && P.goal_rec() == j) cost = ldiv32(add32(cost, cost), 3);
        if (cost < att) {
            att = cost;
            best_att = static_cast<std::int16_t>(j);
        }
    }

    // ---- the objects of pools 0, 3, 4, 8, 9, 11 [BYTES seg4:0x36c7-0x3cab]
    std::int32_t cap = 80000, attobj = 80000;  // [bp-0x2e], [bp-0x32]
    std::int16_t cap_pool = -1, cap_rec = -1, ao_pool = -1, ao_rec = -1;  // [bp-0x50 ... -0x56]
    const std::int16_t greed10 = c.pg.greed < 10 ? std::int16_t{10} : c.pg.greed;
    static constexpr int kPools[] = {0, 3, 4, 8, 9, 11};
    for (int pool : kPools) {
        for (int rec = 0; rec < c.pools.in_use(pool); ++rec) {
            const bool consider = !(keep && P.goal_pool() == pool && P.goal_rec() == rec);
            ObjectRecord& O = c.obj(pool, rec);
            if (O.state != 0 || !consider) continue;  // [BYTES seg4:0x36fe-0x370b]
            std::int32_t cost = sx(add16(cells(O, c.obj(kPoolShips, i)), 300));  // [BYTES seg4:0x370e-0x373c]
            if (P.goal_pool() == pool && P.goal_rec() == rec) {  // [BYTES seg4:0x373f-0x3782]
                std::int32_t t = sub32(cost, 100);
                if (t < 1) t = 1;
                cost = ldiv32(add32(t, t), 3);
            }
            bool to_capture = false;
            switch (pool) {  // [BYTES seg4:0x3786-0x37a6]
            case 3:  // [BYTES seg4:0x37a8-0x37be]
                cost = shl32(add32(cost, 450), 2);
                break;
            case 8:  // [BYTES seg4:0x380e-0x3860]
                if (rec < 2 * g.gates) {
                    cost = shl32(add32(cost, 1500), 3);
                } else {
                    // The side table is indexed by the pool number, 8, not by the record (`imul bx,
                    // [bp-4], 0x34`): every hazard reads entry 8's kind and class. The original's
                    // fault, kept (ruling R1) [BYTES seg4:0x3823 seg4:0x3845].
                    const Pool8Entry& E = c.w.pool8[8];
                    if (E.w[0x1c / 2] == 2) {
                        if (E.w[0x0a / 2] == -3) cost = shl32(add32(cost, 2500), 3);
                    } else {
                        cost = add32(add32(cost, 350), add32(cost, 350));
                    }
                }
                break;
            case 0:  // [BYTES seg4:0x3863-0x38b1]
                if (O.b54 != 0 && S.w26 < 50) cost = mul32(add32(cost, 200), 2);
                if (c.w.w9000 > 1) cost = sub32(cost, ldiv32(cost, 4));
                to_capture = true;
                break;
            case 11: {  // [BYTES seg4:0x38b4-0x391c]
                const bool kind3 = c.host.pod_word(rec, 0) == 3;
                if (kind3 || g.wave_kind == 7) {
                    if (served(c, c.host.pod_word(rec, 0x0a), "seg4:0x38d9") == i) cost = ldiv32(cost, 5);
                    if (team == 3) cost = ldiv32(cost, 3);
                } else if (team == 3) {
                    cost = ldiv32(cost, 3);
                } else {
                    cost = mul32(cost, 3);
                }
                to_capture = true;
                break;
            }
            case 4:  // [BYTES seg4:0x391e-0x3974]
                if (S.fuel > 200) {
                    cost = shl32(cost, 2);
                    const std::int16_t tank = c.host.design_word(S.design_index, 0x1e);
                    if (!(sub16(tank, 100) >= S.fuel)) cost = add32(cost, 80000);
                } else if (S.fuel < 150) {
                    cost = sar32(cost, 3);
                }
                to_capture = true;
                break;
            default:  // pool 9
                break;
            }
            if (to_capture && O.b53 == 1) {  // the capture branch [BYTES seg4:0x3977-0x3b47]
                if (team == 2 || team == 5) cost = sar32(cost, 2);
                if (team == 6) cost = sub32(cost, sar32(cost, 2));
                if (c.w.w9000 < 2 && i != g.player) {
                    if (pool == 0)
                        cost = ldiv32(mul32(add32(sub32(cost, sx(mul16(c.pg.greed, 5))), 300), 100), greed10);
                    else
                        cost = ldiv32(mul32(cost, greed10), 100);
                }
                for (int k = 0; k <= last; ++k) {  // the other claimants [BYTES seg4:0x3a22-0x3b24]
                    if (k == i) continue;
                    Pilot K = c.p(k);
                    if (K.claim_pool() != pool || K.claim_rec() != rec) continue;
                    if (c.s(k).bec == 0 || c.obj(kPoolShips, k).state != 0 || team == 6) continue;
                    const std::int16_t rk = c.host.relation(i, k);
                    if (c.team(k) != team && rk <= -200) continue;
                    std::int32_t v = add32(add32(sx(rk), cost), 300);
                    if (v < 800) v = 800;
                    cost = mul32(v, sx(add16(sub16(6, allies), sub16(6, allies))));
                    if (rk > 100) cost = add32(cost, 80000);
                }
                if (cost < cap) {
                    cap = cost;
                    cap_pool = static_cast<std::int16_t>(pool);
                    cap_rec = static_cast<std::int16_t>(rec);
                }
            }
            if (O.b53 == 1) continue;  // [BYTES seg4:0x37c1-0x37d4]
            // the attack-object branch [BYTES seg4:0x37d8-0x3c3e]
            cost = add32(cost, 100);
            if (i == g.player)
                cost = mul32(add32(cost, 400), 2);
            else
                cost = ldiv32(mul32(cost, greed10), 100);
            cost = ldiv32(mul32(cost, sx(add16(add16(e, 2), add16(e, 2)))), 6);
            if (team == 6)
                cost = sub32(cost, sar32(cost, 2));
            else if (team == 2)
                cost = add32(cost, sar32(cost, 1));
            if (mul16(add16(e, 6), 5) > S.w26 || !(money(S) > 200)) {
                if (pool == 0)
                    cost = mul32(cost, 3);
                else if (mul16(sub16(9, c.w.score_level), 20) < add16(c.pg.greed, c.pg.greed))
                    cost = add32(cost, 80000);
                else
                    cost = shl32(cost, 2);
            }
            if (cost < attobj) {
                attobj = cost;
                ao_pool = static_cast<std::int16_t>(pool);
                ao_rec = static_cast<std::int16_t>(rec);
            }
        }
    }

    // ---- help: a friend under attack [BYTES seg4:0x3cad-0x3e00]
    help = add32(help, sar32(help, 2));
    std::int16_t defender = -1;  // [bp-0x58]
    for (int k = 0; k <= last; ++k) {
        Pilot K = c.p(k);
        if (c.obj(kPoolShips, k).state != 0 || c.s(k).bec == 0 || k == i || k == best_help) continue;
        if (K.goal_act() != 1 || K.goal_pool() != 14 || K.goal_rec() != best_help) continue;
        if (served(c, k, "seg4:0x3ddb") == i) continue;
        defender = static_cast<std::int16_t>(k);
        if (c.host.strength(best_help, k) >= 60)
            help = sub32(help, sar32(help, 2));
        else
            help = sar32(help, 2);
        // The same team adds twice (the test is made twice) [BYTES seg4:0x3cd2-0x3d24]
        if (c.team(k) == team) help = add32(help, 80000);
        if (c.team(k) == team)
            help = add32(help, 80000);
        else if (add16(c.host.relation(i, k), 100) > c.host.relation(i, best_help))
            help = add32(help, 80000);
        if (mul16(add16(allies, 3), 10) > S.w26) help = add32(help, 80000);
        if (money(S) < -50 && P.cls() > 0 && S.w26 < 65) help = add32(help, 80000);
        if (team == 4) help = sub32(help, sar32(help, 2));
    }

    // ---- the choice [BYTES seg4:0x3e03-0x3eec]
    std::array<std::int32_t, 4> cost4 = {help, att, cap, attobj};  // [bp-0x46], -0x42, -0x3e, -0x3a
    if (help > 20000 && att > 20000 && cap > 20000) {
        cost4 = {90000, 90000, 90000, 90000};
        for (int k = 0; k < c.pools.in_use(kPoolIcons); ++k) {
            const ObjectRecord& I = c.obj(kPoolIcons, k);
            if (I.state != 0) continue;
            if (I.b53 == 1)
                cost4[2] = 100;
            else
                cost4[3] = 100;
            break;
        }
    }
    std::int32_t best = 80000;  // [bp-0x4a]
    std::int16_t pick = -1;     // [bp-0x5a]
    for (int k = 0; k < 4; ++k) {
        if (cost4[static_cast<std::size_t>(k)] < best) {
            best = cost4[static_cast<std::size_t>(k)];
            pick = static_cast<std::int16_t>(k);
        }
    }
    const std::int32_t help4 = cost4[0], att4 = cost4[1], cap4 = cost4[2], attobj4 = cost4[3];
    const std::uint32_t now = g.now;

    auto take = [&](std::int16_t pool, std::int16_t rec, std::int16_t act) {
        P.est_pool(pool);
        P.est_rec(rec);
        P.est_act(act);
        P.est_app(2);
    };

    enum class At { k4129, k40d4 };
    At at = At::k4129;
    if (pick == 0 && best_help != -1) {  // help [BYTES seg4:0x3eee-0x40d0]
        if (defender != -1) {
            const std::int16_t st = c.host.strength(i, defender);
            const bool kept = keep && P.goal_pool() == 14 && P.goal_rec() == defender;
            if (shl16(allies, 5) < st && P.grudge(defender) != -1 && !kept) {
                take(14, defender, 1);
                if (best_help == g.player) c.host.seg4_474c(i, 14, defender, 5);
                ship_reaction(c, i, 14, defender, 1);
                return;
            }
        }
        ShipRecord& Hs = ship_or_zero(c, best_help, "seg4:0x3f91");
        Pilot H(Hs);
        if (H.goal_act() == 1) {
            bool ok = true;
            if (H.goal_pool() == 14) {
                if (c.team(H.goal_rec()) == team)
                    ok = false;
                else if (!(add16(c.host.relation(i, H.goal_rec()), 100) < c.host.relation(i, best_help)))
                    ok = false;
            }
            if (ok && S.w26 <= 35) ok = false;
            if (ok && c.obj(H.goal_pool(), H.goal_rec()).state != 0) ok = false;
            if (ok && keep && H.goal_pool() == P.goal_pool() && H.goal_rec() == P.goal_rec()) ok = false;
            if (ok && i == g.player && (H.goal_pool() == 3 || H.goal_pool() == 8) &&
                !(P.goal_pool() == H.goal_pool() && P.goal_rec() == H.goal_rec())) {
                if (c.rng.rand() % 30 != 2) ok = false;  // [BYTES seg4:0x4069-0x4077]
            }
            if (ok) {  // [BYTES seg4:0x4079-0x40d0]
                take(H.goal_pool(), H.goal_rec(), 1);
                if (best_help == g.player) c.host.seg4_474c(i, H.goal_pool(), H.goal_rec(), 2);
                if (ship_reaction(c, i, P.est_pool(), P.est_rec(), 2) != 0) return;
            }
        }
        at = At::k40d4;
    }
    if (at == At::k40d4) {  // [BYTES seg4:0x40d4-0x4124]
        bool to4239 = true;
        if (cap4 < 80000) {
            const std::int16_t r = add16(static_cast<std::int16_t>(c.rng.rand() % 30), add16(c.pg.greed, c.pg.greed));
            if (r > add16(mul16(c.w.score_level, -20), 0xbe) || i == g.player || c.w.w9000 == 2 ||
                (P.goal_pool() == 0 && P.goal_act() == 2)) {
                pick = 2;
                to4239 = false;
            }
        }
        if (to4239) {  // [BYTES seg4:0x4239-0x429b]
            if (cap4 < 80000 && (mul16(add16(e, 6), 5) > S.w26 || !(money(S) > 200))) {
                pick = 2;
            } else if (attobj4 < 80000) {
                pick = 3;
            } else if (att4 < 80000) {
                pick = 1;
            } else if (cap4 < 80000) {
                pick = 2;
            } else {
                head_for_nearest_gate(c, i, 30000, true);  // [BYTES seg4:0x429f-0x433c]
                return;
            }
        }
    }
    // [BYTES seg4:0x4129-0x438d]
    if (pick == 1 && best_att != -1) {
        const std::int16_t r = static_cast<std::int16_t>(c.rng.rand() % 3 + 1);
        if (r < a[static_cast<std::size_t>(best_att)] && mul16(add16(e, 2), 10) < S.w26 && money(S) > -50) {
            take(14, best_att, 1);
            c.host.target(i, 14, best_att);  // [BYTES seg4:0x4195-0x419d]
            if (team == 3 && c.s(best_att).resources() > 400) {  // the Pirate's robbery [BYTES seg4:0x41bb-0x4236]
                P.order_pool(14);
                P.order_rec(best_att);
                P.called_for(static_cast<std::int8_t>(best_att & 0xff));
                ship_reaction(c, i, 14, best_att, 0xd);
                P.order(4);
                P.order_until(now + 30000);
                P.order_by(static_cast<std::int16_t>(i));
                P.one_shot(0);
                c.host.clear_tallies();  // DGROUP:0x9680/0x9682 and dwords DGROUP:0x8e00/0x8e04 = 0
                c.host.message(best_att, kPoolShips, i, 0x67);
                P.replan_at(now + 500);
                return;
            }
            ship_reaction(c, i, 14, best_att, 3);  // [BYTES seg4:0x433f-0x4344]
            return;
        }
        // [BYTES seg4:0x4347-0x438d]
        if (cap4 < 80000) {
            if (mul16(sub16(9, c.w.score_level), 20) < add16(c.pg.greed, c.pg.greed) || i == g.player ||
                c.w.w9000 == 2 || !(attobj4 < 80000))
                pick = 2;
            else
                pick = 3;
        } else if (attobj4 < 80000) {
            pick = 3;
        }
    }
    if (pick == 2 && cap_pool != -1 && cap_rec != -1) {  // capture [BYTES seg4:0x4394-0x4531]
        if (team != 6 && P.cls() < 1 && team == 4) {  // the Knight's test [BYTES seg4:0x43c3-0x4429]
            const std::int16_t r = static_cast<std::int16_t>(c.rng.rand() % 600);
            const std::int32_t v = sx(add16(mul16(add16(e, 2), 150), r));
            if (v < money(S) && S.w26 > 50) {
                take(cap_pool, cap_rec, 1);
                ship_reaction(c, i, cap_pool, cap_rec, 4);
                return;
            }
        }
        c.host.seg13_9ae8(i);  // [BYTES seg4:0x442c]
        if (S.w26 < 40 && cap_pool == 0 && c.obj(0, cap_rec).b54 != 0) {  // [BYTES seg4:0x4435-0x44b6]
            c.host.target(i, cap_pool, cap_rec);
            c.host.seg13_9ce0(i);
            if (att4 < 20000) {
                take(14, best_att, 3);
                return;
            }
            if (help4 < 20000) {
                take(14, best_help, 3);
                return;
            }
        }
        P.est_pool(cap_pool);  // [BYTES seg4:0x44b9-0x4531]
        P.est_rec(cap_rec);
        P.est_act(2);
        if (mul16(sub16(9, c.w.score_level), 20) > add16(c.pg.greed, c.pg.greed) && i != g.player && c.w.w9000 < 2)
            P.est_act(3);
        P.est_app(2);
        if (cap_pool == 0 && static_cast<std::int8_t>(c.obj(0, cap_rec).flame) < 0) {
            P.est_act(1);
            choose_weapon(c, i);  // [BYTES seg4:0x452d]
        }
        rivals(c, i);  // seg4:0x6700 [BYTES seg4:0x3112-0x3116]
        return;
    }
    if (pick == 3) {  // attack an object [BYTES seg4:0x4534-0x4645]
        bool nothing = false;
        if (i == g.player && g.demo != 0 && (attobj >= 600 || S.w26 <= 45) && cap >= 80000 && ao_pool != 0 &&
            ao_pool != 9 && ao_pool != 2)
            nothing = true;
        if (!nothing && (ao_pool == -1 || ao_rec == -1 || ao_pool > 14)) nothing = true;
        if (!nothing) {
            bool far_branch = false;  // to seg4:0x4604
            if (team != 6) {
                const std::int16_t r = static_cast<std::int16_t>(c.rng.rand());
                const std::int16_t dd = add16(e, 2);
                std::int16_t m = 0;
                if (dd == 0 || (r == -32768 && dd == -1)) {  // `idiv` traps [DESIGN ADR 0016]
                    guard_todo(g_once.divisor, "a zero divisor (enemies byte -2)", "seg4:0x45b5");
                } else {
                    m = imod16(r, dd);
                }
                if (m >= 2 && P.est_act() != 1 && (ao_pool == 3 || ao_pool == 8)) far_branch = true;
            }
            if (!far_branch && mul16(add16(e, 2), 10) < S.w26) {  // [BYTES seg4:0x45d0-0x4601]
                take(ao_pool, ao_rec, 1);
                ship_reaction(c, i, ao_pool, ao_rec, 5);
                return;
            }
            // [BYTES seg4:0x4604-0x4645]
            if (g.game_frames < 1200u && cap < 40000 && (help < 10000 || att < 10000)) {
                c.host.seg13_9ae8(i);
                take(ao_pool, ao_rec, 3);
                return;
            }
        }
    }
    // Nothing chosen [BYTES seg4:0x4648-0x473a]
    if (g.game_frames > 800u || (P.est_pool() == 8 && P.est_rec() == 0 && P.est_act() == 3)) {
        head_for_nearest_gate(c, i, 20000, false);
        return;
    }
    P.order_until(now + 3000);
    P.order_by(static_cast<std::int16_t>(i));
    take(8, 0, 3);
}

// --- seg3:0x21fe, the command line's re-plan ------------------------------------------------------
void replan_think(PilotCtx& c, int i) {
    if (!ship_ok(i)) return;
    Pilot P = c.p(i);
    P.replan_at(0);  // [BYTES seg3:0x2203]
    P.order(-1);     // [BYTES seg3:0x220c]
    if (i == c.g.player && c.g.demo != 0) return;  // [BYTES seg3:0x2212-0x221f]
    think(c, i);                                    // [BYTES seg3:0x2226]
}

// --- seg4:0x78, the think -----------------------------------------------------------------------
void think(PilotCtx& c, int i) {
    WaveGlobals& g = c.g;
    if (g.wave_kind == 6 && g.dg_9eb5 == 0) return;  // [BYTES seg4:0x87-0x93]
    if (!ship_ok(i)) {
        guard_todo(g_once.ship, "a ship index outside the ship table", "seg4:0x97");
        return;
    }
    Pilot P = c.p(i);
    ShipRecord& S = c.s(i);
    const std::uint32_t now = g.now;
    P.no_cap(0);  // [BYTES seg4:0x9c]
    const std::int16_t team = c.team(i);
    // ---- clean-ups [BYTES seg4:0xa1-0x16b]
    if (team == 7 && P.goal_pool() == 14 && c.team(P.goal_rec()) == 7) {
        P.goal_pool(0);
        P.goal_rec(0);
        P.replan_at(0);
        P.order_rec(0);
        P.order_pool(0);
        P.order(0);
    }
    if (P.goal_pool() == 14 && ship_or_zero(c, P.goal_rec(), "seg4:0xf1").bec == 0) P.replan_at(0);
    if (g.play_mode >= 2 && i == g.player && c.w.w8690 < now && S.w26 < 40) P.replan_at(0);
    if (P.goal_act() == 1 && P.goal_pool() == 14 && served(c, P.goal_rec(), "seg4:0x152") == i && team != 7) {
        P.order(0);
        P.replan_at(0);
    }
    if (g.demons_gate != 0) c.host.demons_gate(i);  // [BYTES seg4:0x174-0x249] (M6)
    if (g.wave_kind == 7 && (g.demo == 0 || i != g.player)) c.host.duel(i);  // [BYTES seg4:0x24f-0x382] (M6)
    if (g.wave_kind == 6 && P.goal_pool() != 12 && team != 7) P.replan_at(now);  // [BYTES seg4:0x388-0x3ac]

    // ---- the re-plan [BYTES seg4:0x3b1-0x863]
    const bool due = P.replan_at() < now || c.obj(P.goal_pool(), P.goal_rec()).state != 0;
    if (due) {
        // a. A gone goal whose order object has gone too [BYTES seg4:0x3ec-0x49e]
        if (c.obj(P.goal_pool(), P.goal_rec()).state != 0 && P.order() != 0 && P.order() != 7 &&
            P.order_pool() != -1 && c.obj(P.order_pool(), P.order_rec()).state != 0) {
            if (g.wave_kind == 7)
                c.host.duel(i);  // the duel's retarget [BYTES seg4:0x451-0x497] (M6)
            else
                P.order(0);  // [BYTES seg4:0x499-0x49e]
        }
        // b. The Strategy player's step back [BYTES seg4:0x4a4-0x4e8]
        if (i == g.player && g.play_mode >= 2) {
            switch (g.play_mode) {
            case 2: g.play_mode = 0; g.demo = 1; break;
            case 3: g.play_mode = 1; g.demo = 1; break;
            case 4: g.play_mode = 1; break;
            default: break;
            }
            c.w.w8690 = 0;
            c.host.message(g.player, 7, 0, 0xa4);
        }
        // c. [BYTES seg4:0x4eb-0x562]
        bool to_f = revenge_recall(c, i) != 0;  // seg4:0x702e
        if (!to_f && gate_kind(P.goal_pool(), P.goal_rec()) != 0 && P.goal_app() == 2 && P.goal_act() == 2 &&
            !(i == g.player && g.demo != 0) && team != 7)
            to_f = true;
        if (to_f) {
            // f. [BYTES seg4:0x816-0x857]
            if (gate_kind(P.goal_pool(), P.goal_rec()) == 2) c.host.gate_goal(i);
            c.host.target(i, P.goal_pool(), P.goal_rec());
            P.replan_at(now + 3000);
        } else {
            c.host.reactions(i);  // seg13:0x7ba2
            if (P.d9c() == 1) P.d9c(now + 5000);
            // d. [BYTES seg4:0x567-0x6eb]
            if (!(P.goal_act() == 4 || (g.wave_kind == 6 && team != 7))) {
                const bool strategy_player = i == g.player && g.play_mode != 0 && g.demo != 0;
                if (P.order() < 1 && !strategy_player) {
                    P.one_shot(0);                         // [BYTES seg4:0x5b2]
                    if (P.est_act() != 4) choose_goal(c, i);  // seg4:0x2f98(i, 1)
                    after_choice(c, i, -1);                // seg4:0x485c(i, -1)
                } else {
                    order_executor(c, i);  // seg13:0x6900(i)
                }
                if (P.est_pool() < 0) {  // `mov dword [+0x06], 0` [BYTES seg4:0x5f4-0x5fb]
                    P.est_pool(0);
                    P.est_rec(0);
                }
                if (i == g.player && g.demo == 0 && c.host.visible(P.est_pool(), P.est_rec()) != 0) {
                    c.host.set_manual_target();  // DGROUP:0xa62 = 1 [BYTES seg4:0x62b]
                    c.host.target(i, P.est_pool(), P.est_rec());
                }
                if (g.wave_kind != 6) {  // [BYTES seg4:0x641-0x681]
                    bool report = false;
                    if ((P.order() == 3 || P.order() == 4) && g.wave_kind != 7) report = true;
                    if (!report && i == g.player && g.play_mode != 0 && g.demo != 0) report = true;
                    if (report)
                        c.host.command_report();  // seg13:0xa65a
                    else
                        needs(c, i);  // seg4:0x4d00
                }
                if (!P.estimate_is_goal()) {  // [BYTES seg4:0x686-0x6eb]
                    P.goal_since2(now);
                    P.goal_since(now);
                    P.copy_estimate_to_goal();
                }
            }
            // e. The Enforcer's self-order [BYTES seg4:0x6ef-0x814]
            if (team == 7 && P.order() < 0x14 && P.goal_pool() != 11 && P.goal_pool() != 12 && P.goal_pool() != 4) {
                P.order(0x16);
                if (P.called_for() >= 0 && P.cls() < 2) {  // [BYTES seg4:0x726-0x784]
                    if (ship_or_zero(c, P.called_for(), "seg4:0x739").bec == 0) {
                        c.host.message(g.player, kPoolShips, i, 0xb7);
                        P.est_pool(12);
                        P.est_rec(0);
                        P.est_act(2);
                        P.est_app(2);
                    } else {
                        P.order_pool(14);
                        P.order_rec(P.called_for());
                    }
                } else if (P.goal_pool() == 14 || P.cls() == 0) {  // [BYTES seg4:0x786-0x7b9]
                    P.order_pool(14);
                    P.order_rec(P.goal_rec());
                    if (P.cls() == 0) P.order(0x17);
                } else if (P.order_pool() == 14 && P.cls() == 0) {  // [BYTES seg4:0x7bb-0x7cc]
                    P.order(0x17);
                } else if (P.order_pool() == 14 && P.cls() == 1) {  // [BYTES seg4:0x7ce-0x7e0]
                    P.order(0x16);
                } else {
                    P.order(0x14);  // [BYTES seg4:0x7e2-0x7e7]
                }
                P.one_shot(2);  // [BYTES seg4:0x7ed-0x808]
                P.order_by(static_cast<std::int16_t>(i));
                P.replan_at(now);
                P.goal_act(3);
                // seg13:0x6900(i): for team 7 the arrest executor seg13:0xa6bc, a stage 4 host
                // call (ruling R3) [BYTES seg4:0x80e-0x80f].
                order_executor(c, i);
            }
        }
        cloak_shield(c, i);  // g. seg4:0x79ae(i) [BYTES seg4:0x85c-0x860]
    }

    // ---- every think [BYTES seg4:0x864-0xbd2]
    P.thinks(add16(P.thinks(), 1));
    if (g.play_mode != 2) hazard_step(c, i);  // seg4:0x4fc8 [BYTES seg4:0x86d-0x878]
    const std::int16_t ep = P.est_pool(), er = P.est_rec();  // [bp-0x3e], [bp-0x40]
    std::int16_t ax = 0, ay = 0;  // the aim point [bp-0x24], [bp-0x22]
    if (ep == 14 && ship_or_zero(c, er, "seg4:0x893").be8 != 0 && g.demons_gate == 0) {  // [BYTES seg4:0x88e-0x8ad]
        Pilot T(ship_or_zero(c, er, "seg4:0x8a5"));
        ax = T.guess_x();
        ay = T.guess_y();
    } else {
        const ObjectRecord& O = c.obj(ep, er);
        ax = O.x;
        ay = O.y;
    }
    std::int16_t ddx = 0, ddy = 0;
    const std::int16_t d = cells_between(c.obj(kPoolShips, i), ax, ay, ddx, ddy);  // [bp-0x12] [BYTES seg4:0x8c9-0x8e9]
    if (P.goal_act() == 4) {  // defend [BYTES seg4:0x8ec-0x967]
        if (d < 400 && ep == 14 && P.goal_pool() == ep && P.goal_rec() == er && !held_by_ambulance(c, i)) {
            if (static_cast<std::int8_t>(P.weapon_wait()) <= 0) c.host.weapon_system(i);  // seg4:0x5654
            P.weapon_wait(static_cast<std::uint8_t>(P.weapon_wait() + static_cast<std::uint8_t>(4 - g.detail)));
            if (d < 100) c.host.thrust(i, 2);
        }
        P.no_cap(1);
        P.est_app(5);
    }
    if (held_by_ambulance(c, i)) return;  // [BYTES seg4:0x97c-0x990]
    if (P.weapon_class() == 2 && P.goal_act() != 4) {  // the class-2 weapon [BYTES seg4:0x994-0xa47]
        if (P.goal_act() == 1) {
            if (d < 160) {
                bool none = true;  // [bp-0x4a]: no live pool-6 record of its own
                for (int k = 0; k < c.pools.in_use(6); ++k) {
                    const MissileSlot& M = c.pools.missile(k);
                    if (c.obj(6, k).state == 0 && M.launcher_pool == 14 && M.launcher_rec == i) none = false;
                }
                if (none) {
                    P.est_app(5);
                    P.goal_app(5);
                    P.weapon_wait(0);
                } else {
                    P.est_app(2);
                    P.goal_app(2);
                }
            } else if (d > 210) {
                P.est_app(2);
                P.goal_app(2);
            } else if (P.goal_app() == 5) {
                P.est_app(5);
            }
        } else {
            P.weapon_class(0);
        }
    }
    // The waits [BYTES seg4:0xa4c-0xa76]: cbw, dec, a negative result becomes 0.
    auto dec_floor_b = [](std::uint8_t b) {
        const std::int16_t v = sub16(static_cast<std::int16_t>(static_cast<std::int8_t>(b)), 1);
        return static_cast<std::uint8_t>(v < 0 ? 0 : v & 0xff);
    };
    P.weapon_wait(dec_floor_b(P.weapon_wait()));
    P.gun_wait(dec_floor_b(P.gun_wait()));
    {
        const std::int16_t v = sub16(P.wait2(), 1);
        P.wait2(v < 0 ? std::int16_t{0} : v);
    }
    if (P.est_act() != 1) return;  // [BYTES seg4:0xa7a-0xa7f]
    const ObjectRecord& own = c.obj(kPoolShips, i);
    if (d < 400 && (g.detail > 1 || own.on_screen != 0)) {  // the near path [BYTES seg4:0xa83-0xba6]
        if (i != g.player && static_cast<std::uint32_t>(P.goal_since() + 19000u) < now &&
            (P.order() == 0 ||
             (P.order_until() < now && P.order() != 4 && P.order() != 1 && P.order() != 0x17))) {
            if (P.goal_pool() == 14) {  // grudge byte +0xc2 + goal rec = 0xff [BYTES seg4:0xaed-0xafd]
                const int off = pw::kGrudge0 + P.goal_rec();
                if (off >= 0 && off < static_cast<int>(kShipRecordSize))
                    set_ship_b(S, off, 0xff);
                else
                    guard_todo(g_once.grudge, "a grudge byte past the ship record", "seg4:0xafd");
            }
            P.replan_at(0);
            P.order(-1);
            choose_goal(c, i);  // seg4:0x2f98(i, 1) [BYTES seg4:0xb1e]
            P.goal_since2(now);
            P.goal_since(now);
            P.copy_estimate_to_goal();
        }
        std::int16_t w = 0;  // [bp-6]
        if (static_cast<std::int8_t>(P.weapon_wait()) <= 0 && own.on_screen != 0) w = c.host.weapon_system(i);
        if (P.est_app() == 2 && w == 0 && static_cast<std::int8_t>(P.gun_wait()) <= 0 &&
            !(ep == 0 && selected_weapon(S) == 5))
            c.host.gun(i);  // seg11:0x4e0a(i) [BYTES seg4:0xba3-0xba6]
        return;
    }
    if (S.fuel > 150 && d > 700) {  // [BYTES seg4:0xbad-0xbcd]
        c.host.thrust(i, 2);
        P.no_cap(1);
    }
}

// --- more PilotHost defaults ------------------------------------------------------------------------
namespace {
struct Once2 {
    bool wword = false, dword_ = false, vcount = false, ventry = false, mark = false;
    bool estimate0 = false, help = false;
} g_once2;
}  // namespace
std::int16_t PilotHost::weapon_word(int, int) {
    guard_todo(g_once2.wword, "the weapon table", "DGROUP:0x2f54");
    return 0;
}
std::int16_t PilotHost::defender_word(int, int) {
    guard_todo(g_once2.dword_, "the defender table (stage 4)", "DGROUP:0x52fe");
    return 0;
}
int PilotHost::visible_count() {
    guard_todo(g_once2.vcount, "the visibility list", "DGROUP:0x9146");
    return 0;
}
void PilotHost::visible_entry(int, std::int8_t& pool, std::int8_t& rec) {
    guard_todo(g_once2.ventry, "the visibility list", "DGROUP:0x507c");
    pool = -1;
    rec = 0;
}
void PilotHost::mark_6e52(std::int16_t& pool, std::int16_t& rec) {
    guard_todo(g_once2.mark, "the words DGROUP:0x6e52/0x6e54", "seg4:0x8b55");
    pool = -1;
    rec = -1;
}
namespace {
struct Once3 {
    bool steal = false, watch = false, tail = false, notice = false;
} g_once3;
}  // namespace
void PilotHost::steal_text(int, int, int) {
    guard_todo(g_once3.steal, "the player's activity text as a rival", "seg4:0x6906");
}
void PilotHost::watch_ship(int) { guard_todo(g_once3.watch, "the watch windows' ship refresh", "seg13:0x6890"); }
void PilotHost::watch_tail() { guard_todo(g_once3.tail, "the watch windows' tail calls", "seg13:0x67f2"); }
bool PilotHost::smart_window() { return false; }
std::int16_t PilotHost::smwnd() { return 0; }
std::int16_t PilotHost::ship_anim() { return 0; }  // hud/target.h dg_131c
void PilotHost::all_ships_notice(int) {
    guard_todo(g_once3.notice, "the All Ships notice", "seg13:0x6881");
}

namespace {

// The pool-8 side table word DGROUP:0x3d50 + 0x34·rec + off; past the table 0 [DESIGN ADR 0016].
std::int16_t pool8_word(PilotCtx& c, int rec, int off) {
    if (rec >= 0 && rec < kPool8Max) return c.w.pool8[static_cast<std::size_t>(rec)].w[static_cast<std::size_t>(off / 2)];
    guard_todo(g_once.obj, "a pool-8 side-table index past the table", "DGROUP:0x3d50");
    return 0;
}

}  // namespace

// seg13:0x78b8(i, j), cdecl (pilot_think.h) [BYTES seg13:0x78b8-0x7b3f].
std::int16_t help_answer(PilotCtx& c, int i, int j) {
    ShipRecord& S = c.s(i);
    Pilot P(S);
    ShipRecord& J = ship_or_zero(c, j, "seg13:0x7905");
    if (idiv16(c.host.design_word(S.design_index, 0x1e), 10) > S.fuel) return -6;  // [BYTES seg13:0x78c6-0x78e4]
    std::int16_t l8 = c.host.relation(i, j);  // [BYTES seg13:0x78e7-0x78f3]
    if (c.team(j) == c.team(i)) l8 = add16(l8, 200);
    const std::int8_t ja = ship_sb(J, pw::kAllies);
    if (ja < 3) l8 = add16(l8, shl16(sub16(3, ja), 2));  // [BYTES seg13:0x7920-0x7934]
    // `imul byte`: 20·i's byte +0xfc + 9j and 40·byte +0xfe + 9j [BYTES seg13:0x7937-0x7953]
    auto byte_i = [&](int off) -> std::int16_t {
        if (off < 0 || off >= static_cast<int>(kShipRecordSize)) {
            guard_todo(g_once.grudge, "a hurt byte past the ship record", "seg13:0x7944");
            return 0;
        }
        return ship_sb(S, off);
    };
    l8 = add16(l8, mul16(20, byte_i(0xfc + 9 * j)));
    l8 = add16(l8, mul16(40, byte_i(0xfe + 9 * j)));
    if (P.called_for() == j) l8 = add16(l8, 0x258);
    for (int k = 0; k <= last_ship(c); ++k) {  // [BYTES seg13:0x7a61-0x7b32]
        if (c.obj(kPoolShips, k).state != 0 || k == i || k == j || c.s(k).bec == 0) continue;
        Pilot K = c.p(k);
        if (K.goal_pool() != P.order_pool() || K.goal_rec() != P.order_rec()) continue;
        if (K.goal_act() != 1 && !(c.host.relation(k, j) < 0)) continue;
        std::int16_t l6 = c.host.relation(i, k);
        if (c.team(k) == c.team(i)) l6 = add16(l6, 200);
        if (c.team(k) == 7) {
            const std::int16_t ti = c.team(i);
            if (ti == 3 || ti == 6)
                l6 = sub16(l6, 150);
            else if (ti == 1 || ti == 5)
                l6 = add16(l6, 20);
            else
                l6 = add16(l6, 100);
        }
        // k's hurt byte +0xf7 + 9j, times 25 (`imul byte`) [BYTES seg13:0x7990-0x799f]
        l6 = add16(l6, mul16(25, ship_sb(c.s(k), (0xf7 + 9 * j) < static_cast<int>(kShipRecordSize) ? 0xf7 + 9 * j : 0xf7)));
        std::int16_t la = c.host.strength(i, k);
        la = add16(la, idiv16(mul16(static_cast<std::int16_t>(P.allies()), la), -5));  // [BYTES seg13:0x79b3-0x79c6]
        // j's team, not k's: `imul bx, [bp+8], 0x15a` [BYTES seg13:0x79c9-0x79e9]
        if (c.team(j) == c.team(i) || c.team(i) == 4) la = shl16(la, 1);
        if (P.called_for() == j) la = add16(la, 0x32);
        if (la < 0x32) return -2;
        if (l8 < l6) {  // [BYTES seg13:0x7a0b-0x7a58]
            if (c.team(k) == 7) return -3;
            const std::int16_t a = add16(ship_sb(J, (0xf7 + 9 * k) < static_cast<int>(kShipRecordSize) ? 0xf7 + 9 * k : 0xf7), 2);
            const std::int16_t b = ship_sb(c.s(k), (0xf7 + 9 * j) < static_cast<int>(kShipRecordSize) ? 0xf7 + 9 * j : 0xf7);
            return a < b ? std::int16_t{-4} : std::int16_t{-5};
        }
        return static_cast<std::int16_t>(k);
    }
    return -1;  // [BYTES seg13:0x7b35]
}

namespace {

// frame#/10 (seg5:0x2b4, unsigned 32-bit) minus a sign-extended stamp, with the original's test
// "high word 0 and low word below n" [BYTES seg4:0x5aa1-0x5acb].
bool within(std::uint32_t frames, std::uint32_t div, std::int16_t stamp, std::uint16_t n) {
    const std::uint32_t q = frames / div;
    const std::uint32_t v = q - static_cast<std::uint32_t>(static_cast<std::int32_t>(stamp));
    return (v >> 16) == 0 && (v & 0xffffu) < n;
}

}  // namespace

// --- seg4:0x485c, the call to help a ship under attack ------------------------------------------
void after_choice(PilotCtx& c, int i, std::int16_t /*a*/) {
    Pilot P = c.p(i);
    const std::int16_t team = c.team(i);
    if (team == 7 || team == 6) return;  // [BYTES seg4:0x486f-0x4893]
    std::int16_t chosen = -1;            // [bp-0x12], kept across the j loop
    const int last = last_ship(c);
    for (int j = 0; j <= last; ++j) {    // [BYTES seg4:0x4cbb-0x4cf1]
        if (c.obj(kPoolShips, j).state != 0 || j == i || c.s(j).bec == 0) continue;
        bool skip = false;  // [bp-0x16]
        for (int k = 0; k <= last; ++k) {  // already helped [BYTES seg4:0x489e-0x48ca]
            const Pilot K = c.p(k);
            if ((K.order() == 5 || K.order() == 1) && K.order_rec() == j) {
                skip = true;
                break;
            }
        }
        if (team == 3 && c.team(j) != 3) skip = true;
        if (P.grudge(j) > 0) skip = true;
        if (P.ee(j) > 0 && P.ee(j) <= 5) skip = true;
        const Pilot J = c.p(j);
        if (J.goal_pool() == 14 && J.goal_rec() == i && J.goal_act() == 1) skip = true;
        for (int k = 1; k <= 5; ++k)  // i's bytes +0xf6 + 9j + k [BYTES seg4:0x4944-0x496a]
            if (ship_sb(c.s(i), 0xf6 + 9 * j + k) > 5) skip = true;
        if (skip) continue;
        std::int16_t l8 = 10, l18 = 0;
        const ShipRecord& Js = c.s(j);
        if (Js.w26 < 50) l18 = 1;
        if (J.goal_act() == 4) l18 = add16(l18, 1);
        std::int16_t hunters = 0, haters = 0;  // [bp-0x10], [bp-0x14]
        for (int k = 0; k <= last; ++k) {  // [BYTES seg4:0x49ac-0x4a4f]
            const Pilot K = c.p(k);
            if (c.obj(kPoolShips, k).state == 0 && k != i && k != j && c.s(k).bec != 0 && K.goal_pool() == 14 &&
                K.goal_rec() == j && K.goal_act() == 1) {
                l8 = add16(l8, idiv16(c.host.relation(i, k), 45));
                const std::int16_t st = c.host.strength(j, k);
                hunters = add16(hunters, 1);
                if (st < mul16(hunters, 40)) chosen = static_cast<std::int16_t>(j);
            }
            if (c.host.relation(k, j) < -50 && k != j && c.s(k).bec != 0) haters = add16(haters, 1);
        }
        if (chosen != -1) l18 = add16(l18, 1);
        if (hunters > 1) l18 = add16(l18, 1);
        if (hunters != 0) l18 = add16(l18, 1);
        if (add16(static_cast<std::int16_t>(P.cls()), static_cast<std::int16_t>(P.allies())) < 3) l18 = add16(l18, 1);
        if (l18 <= 1) continue;
        // [BYTES seg4:0x4a91-0x4c5e]
        if (cells(c.obj(kPoolShips, i), c.obj(kPoolShips, j)) < 1000) l8 = sub16(l8, 10);
        if (c.team(j) == team || team == 4) l8 = sub16(l8, 15);
        l8 = add16(l8, shl16(static_cast<std::int16_t>(P.allies()), 2));
        if (!(c.host.relation(i, j) < mul16(l8, 10))) {
            for (int k = 0; k < c.pools.in_use(2); ++k)  // defenders of j [BYTES seg4:0x4b13-0x4b41]
                if (c.host.defender_word(k, 0) == 15 && c.host.defender_word(k, 0x0a) == 14 &&
                    c.host.defender_word(k, 0x0c) == j)
                    hunters = add16(hunters, 1);
            if (add16(haters, hunters) > 0) {
                if (hunters > 2) chosen = static_cast<std::int16_t>(j);
                std::int16_t v = add16(mul16(hunters, 9), 14);
                if (Js.w28 < v || Js.w2a < v) chosen = static_cast<std::int16_t>(j);
                v = add16(add16(shl16(haters, 2), mul16(hunters, 10)), 0x15);
                if (v > Js.w26) chosen = static_cast<std::int16_t>(j);
                const std::int16_t r = c.host.relation(i, j);
                l8 = add16(l8, 15);
                if (mul16(sub16(l8, mul16(hunters, 6)), 10) > r || hunters == 0) {  // [BYTES seg4:0x4c2e-0x4c59]
                    if (haters > 2 && Js.w26 < 0x23 && mul16(add16(l8, 10), 10) < c.host.relation(i, j))
                        chosen = static_cast<std::int16_t>(j);
                } else if (ship_sb(Js, pw::kClass) > 0 && ship_sb(Js, pw::kAllies) < 3) {  // [BYTES seg4:0x4bc1-0x4c2c]
                    if (hunters > 1) chosen = static_cast<std::int16_t>(j);
                    if (Js.w28 < 0x1e || Js.w2a < 0x1e) chosen = static_cast<std::int16_t>(j);
                    if (Js.w26 < 0x2d || !(mul16(add16(l8, 12), 10) >= c.host.relation(i, j)))
                        if (j == c.g.player) chosen = static_cast<std::int16_t>(j);
                }
            }
        }
        if (chosen < 0) continue;  // [BYTES seg4:0x4c61-0x4c65]
        if (help_answer(c, i, j) < -1) continue;  // seg13:0x78b8(i, j) [BYTES seg4:0x4c67-0x4c78]
        P.one_shot(1);  // [BYTES seg4:0x4c7a-0x4cb4]
        P.order(1);
        P.order_pool(14);
        P.order_rec(static_cast<std::int16_t>(j));
        P.order_by(static_cast<std::int16_t>(i));
        P.order_until(c.g.now + 18000);
        P.replan_at(c.g.now + 300);
        return;
    }
}

// --- seg4:0x4d00, the needs -------------------------------------------------------------------------
void needs(PilotCtx& c, int i) {
    Pilot P = c.p(i);
    ShipRecord& S = c.s(i);
    if (P.est_pool() == 12 && P.est_act() == 2) return;  // leaving [BYTES seg4:0x4d0f-0x4d20]
    if (c.g.demons_gate != 0) return;                     // [BYTES seg4:0x4d24-0x4d29]
    // Defenders hunting i [BYTES seg4:0x4d2d-0x4e17]
    if (c.pools.in_use(2) != 0 && c.team(i) != 7 && c.g.detail >= 2 && (P.order() == 0 || P.order() == 7)) {
        const std::int16_t bound = 20000;  // [bp-0xe], never lowered: the last match is kept
        std::int16_t last_k = -1, n = 0;
        for (int k = 0; k < c.pools.in_use(2); ++k) {
            if (c.obj(2, k).state != 0) continue;
            if (c.host.defender_word(k, 0x0a) != 14 || c.host.defender_word(k, 0x0c) != i ||
                c.host.defender_word(k, 0) != 15)
                continue;
            n = add16(n, 1);
            const std::int16_t d = cells(c.obj(kPoolShips, i), c.obj(2, k));
            if (d < bound) last_k = static_cast<std::int16_t>(k);
        }
        if (n > 1 || bound < 180) {
            P.est_pool(2);
            P.est_rec(last_k);
            P.est_act(1);
            P.est_app(2);
            ship_reaction(c, i, 2, last_k, 6);
        }
    }
    // Fuel [BYTES seg4:0x4e1a-0x4f57]
    if (idiv16(c.host.design_word(S.design_index, 0x1e), 10) > S.fuel) {
        if (S.fuel != 0) {
            const std::int16_t r = static_cast<std::int16_t>(c.rng.rand());
            const std::int16_t m = (S.fuel == -1) ? std::int16_t{0} : imod16(r, S.fuel);
            if (m > 150) return;  // [BYTES seg4:0x4e40-0x4e4e]
        }
        std::int16_t best = 20000, k4 = -1;
        for (int k = 0; k < c.pools.in_use(4); ++k) {
            if (c.obj(4, k).state != 0) continue;
            const std::int16_t d = cells(c.obj(kPoolShips, i), c.obj(4, k));
            if (d < best) {
                best = d;
                k4 = static_cast<std::int16_t>(k);
            }
        }
        if (k4 >= 0) {  // [BYTES seg4:0x4eb9-0x4ee6]
            P.est_pool(4);
            P.est_rec(k4);
            P.est_act(2);
            P.replan_at(c.g.now + 4000);
            P.est_app(2);
            P.order(0);
            return;
        }
        if (money(S) < 200) {  // [BYTES seg4:0x4ee9-0x4f57]
            if (c.g.wave_kind != 7) {
                c.host.message(c.g.player, kPoolShips, i, 0xb7);
                P.est_pool(12);
                P.est_rec(0);
                P.est_act(2);
                P.est_app(2);
            }
        } else if (c.host.ambulance_call(i) != 0) {
            P.est_pool(7);
            P.est_rec(0);
            P.est_act(3);
            P.est_app(2);
        }
    }
    // Held by another ship's tether: attack the holder [BYTES seg4:0x4f5b-0x4fb6]
    const ObjectRecord& O = c.obj(kPoolShips, i);
    if (O.owner_pool != 14 || P.est_act() == 4) return;
    if (P.est_pool() == 14 && O.owner_rec == P.est_rec()) return;
    P.est_pool(14);
    P.est_rec(O.owner_rec);
    P.est_app(2);
    P.est_act(1);
    P.order(0);
}

// --- seg13:0x1076, the obstacle ahead --------------------------------------------------------------
std::int16_t obstacle_ahead(PilotCtx& c, int pool, int rec) {
    std::int16_t gp = 0, gr = 0, ga = 0;  // [bp-0x2a], -0x2c, -0x2e
    if (pool == c.g.ship_pool) {  // [BYTES seg13:0x108b-0x10b2]
        const Pilot P = c.p(rec);
        gp = P.goal_pool();
        gr = P.goal_rec();
        ga = P.goal_act();
    } else if (pool == 6) {  // [BYTES seg13:0x10b4-0x10d0]
        const MissileSlot& M = c.pools.missile(rec);
        gp = M.target_pool;
        gr = M.target_rec;
        ga = 2;
    }
    const ObjectRecord& own = c.obj(pool, rec);
    std::int16_t best = 20000, bp_ = -1, br = 0;  // [bp-0x20], [bp-0x1c], [bp-0x1e]
    const int nvis = c.host.visible_count();
    for (int k = 0; k < c.g.gates + nvis; ++k) {  // [BYTES seg13:0x12cb-0x12f4]
        std::int16_t cp = 12, cr = 0;
        if (k < nvis) {
            std::int8_t vp = 0, vr = 0;
            c.host.visible_entry(k, vp, vr);
            cp = vp;
            cr = vr;
        } else {
            cr = add16(shl16(static_cast<std::int16_t>(k - nvis), 1), 1);
        }
        bool use = true;  // [bp-0x4c] [BYTES seg13:0x110e-0x1157]
        if (cp == pool && cr == rec) use = false;
        if (cp == 2 || cp == 9 || cp == 4) use = false;
        if (ga == 2 && cp == gp && cr == gr) use = false;
        if (ga == 2 && cp == 0) use = false;
        if (!use) continue;
        const ObjectRecord& O = c.obj(cp, cr);
        std::int16_t dx = 0, dy = 0;
        const std::int16_t d = cells_between(own, O.x, O.y, dx, dy);  // [BYTES seg13:0x1164-0x11a5]
        if (d >= 300 || d <= 0) continue;
        const std::int32_t dvx = sx(sub16(own.vx, O.vx)), dvy = sx(sub16(own.vy, O.vy));
        (void)magnitude(dvx, dvy);  // seg8:0x35d0, its result [bp-0x64] has no reader [BYTES seg13:0x11ee-0x1200]
        // The closing speed toward O: (dvy·dy + dvx·dx) / d, 32-bit [BYTES seg13:0x1203-0x1252]
        const std::int32_t s = ldiv32(add32(mul32(dvy, sx(dy)), mul32(dvx, sx(dx))), sx(d));
        if (s <= 0) continue;
        const std::int16_t t = sub16(d, lo16(ldiv32(mul32(s, 3), 2)));  // [BYTES seg13:0x1261-0x127e]
        if (t >= best) continue;
        if (!(t < 60)) {
            if (!(cp == 8 && cr < 2 * c.g.gates && t < 120)) continue;
        }
        if (c.g.wave_kind == 6 && c.g.dg_9eb5 == 0 && cp == 8) continue;  // [BYTES seg13:0x12a5-0x12b7]
        best = t;
        bp_ = cp;
        br = cr;
    }
    if (bp_ == -1) return -1;  // 0xffff [BYTES seg13:0x12f7-0x130a]
    // `mov al, [bp-0x1e]; cbw; mov ch, [bp-0x1c]; sub cl, cl; or ax, cx` [BYTES seg13:0x12fd-0x1306]
    const std::uint16_t lo = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(br & 0xff)));
    return to_i16(lo | static_cast<std::uint16_t>((bp_ & 0xff) << 8));
}

// --- seg4:0x4fc8, the hazard step ------------------------------------------------------------------
namespace {
// +0x3a = 4 when the cross product of the relative position and the relative velocity is
// positive, else 3 [BYTES seg4:0x50aa-0x5186 seg4:0x5212-0x5312].
std::int16_t veer(const ObjectRecord& O, const ObjectRecord& own) {
    const std::int32_t dxr = sx(sub16(O.x, own.x)), dyr = sx(sub16(O.y, own.y));
    const std::int32_t dvx = sx(sub16(O.vx, own.vx)), dvy = sx(sub16(O.vy, own.vy));
    const std::int32_t cross = sub32(mul32(dxr, dvy), mul32(dyr, dvx));
    return cross > 0 ? std::int16_t{4} : std::int16_t{3};
}
}  // namespace

void hazard_step(PilotCtx& c, int i) {
    Pilot P = c.p(i);
    const ObjectRecord& own = c.obj(kPoolShips, i);
    bool copy_goal = own.on_screen == 0;  // [BYTES seg4:0x4fef-0x4ff4]
    if (!copy_goal) {
        const std::int16_t h = obstacle_ahead(c, 14, i);  // seg13:0x1076(14, i) [BYTES seg4:0x4ff8-0x5003]
        if (h != -1) {  // [BYTES seg4:0x5014-0x5186]
            const std::int16_t hp = static_cast<std::int16_t>((static_cast<std::uint16_t>(h) >> 8) & 0xff);
            const std::int16_t hr = static_cast<std::int16_t>(h & 0xff);
            P.est_pool(hp);
            P.est_rec(hr);
            P.est_act(3);
            if (hp == 8 && pool8_word(c, hr, 0x0a) == -3) {
                P.est_app(5);
            } else if (P.goal_pool() == 12 && P.goal_act() == 2 && hp == 8 && hr < 2 * c.g.gates) {
                P.est_app((hr & 1) == 0 ? 4 : 3);  // the posts of the gate it leaves by
            } else {
                P.est_app(veer(c.obj(hp, hr), own));
            }
        } else {
            // the nearest virus [BYTES seg4:0x5189-0x5318]
            std::int16_t best = 350, kv = 0;  // [bp-0xc], [bp-0xe]
            for (int k = 0; k < c.pools.in_use(9); ++k) {
                const ObjectRecord& V = c.obj(9, k);
                if (V.state != 0 || V.on_screen == 0) continue;
                if (P.goal_pool() == 9 && P.goal_rec() == k) continue;
                const std::int16_t d = cells(own, V);
                if (d < best) {
                    best = d;
                    kv = static_cast<std::int16_t>(k);
                }
            }
            if (best < 250) {
                P.est_pool(9);
                P.est_rec(kv);
                P.est_act(3);
                P.est_app(veer(c.obj(9, kv), own));
                best = 20000;
            } else {
                best = 18888;  // 0x49c8 [BYTES seg4:0x531f]
            }
            // the nearest missile [BYTES seg4:0x5324-0x539c]
            std::int16_t km = kv;
            for (int k = 0; k < c.pools.in_use(6); ++k) {
                if (c.obj(6, k).state != 0) continue;
                const MissileSlot& M = c.pools.missile(k);
                if (M.launcher_pool == 14 && M.launcher_rec == i) continue;
                if (M.weapon == 0x1b) continue;
                const std::int16_t d = cells(own, c.obj(6, k));
                if (d < best) {
                    best = d;
                    km = static_cast<std::int16_t>(k);
                }
            }
            if (best < 160) {  // fire the first slot whose weapon has W+0x46 [BYTES seg4:0x539e-0x5408]
                ShipRecord& S = c.s(i);
                for (int k = 1; k <= static_cast<std::int8_t>(S.slots_cb()); ++k) {
                    const std::int8_t w = ship_sb(S, 0xb8 + k);
                    if (c.host.weapon_word(w, 0x46) == 0) continue;
                    bool fire = true;
                    if ((w & 0xff) == 2) {
                        // the hook only while idle, and the launch-kind test on the missile entry
                        // indexed by the slot number k (the original's index) [BYTES seg4:0x53d9-0x53fd]
                        if (own.w36 != -1) fire = false;
                        const std::int16_t lk = c.host.weapon_word(c.pools.missile(k).weapon, 0x0a);
                        if (fire && lk != 1 && lk != 5) fire = false;
                    }
                    if (fire) c.host.weapon_use(k, i, 0);  // seg4:0x584c(k, i, 0)
                    break;
                }
            }
            if (best < 400) {  // the dodge [BYTES seg4:0x540b-0x5439]
                if (add16(mul16(15, static_cast<std::int16_t>(P.cls())), 0x28) > c.s(i).w26) {
                    P.est_pool(6);
                    P.est_rec(km);
                    P.est_act(3);
                    P.est_app(5);
                }
            } else if (best == 18888) {
                copy_goal = true;
            }
        }
    }
    if (copy_goal) P.copy_goal_to_estimate();  // [BYTES seg4:0x5443-0x5464]
    // Off screen, through the gate of a gate goal [BYTES seg4:0x5468-0x5505]
    if (gate_kind(P.goal_pool(), P.goal_rec()) == 0 || own.on_screen != 0) return;
    const Rect16 box = rect16(own.x - 0x320, own.y - 0x320, own.x + own.size + 0x320, own.y + own.size + 0x320);
    const int g = idiv16(P.goal_rec(), 2);
    if (g < 0 || g >= kMaxGates) {
        guard_todo(g_once.obj, "a gate index outside the gate table", "seg4:0x54d3");
        return;
    }
    if (!intersect(c.w.gate[static_cast<std::size_t>(g)].beam, box)) return;
    if (!(static_cast<std::uint32_t>(P.entry_time() + 3000u) < c.g.now)) return;
    c.host.through_gate(14, i);  // seg12:0x2288(14, i)
}

// --- seg4:0x6a78 and seg4:0x6e70, the weapon choice -----------------------------------------------
std::int16_t weapon_score(PilotCtx& c, int i, int k) {
    ShipRecord& S = c.s(i);
    Pilot P(S);
    const std::int16_t w = ship_sb(S, 0xb8 + k);  // [bp-4]
    const std::int16_t gp = P.goal_pool(), gr = P.goal_rec();
    auto W = [&](int off) { return c.host.weapon_word(w, off); };
    if (W(0x2a) == 0) return -2000;  // [BYTES seg4:0x6aa4-0x6ab1]
    if (gp != 14) {
        if (ship_sb(S, 0xae + k) < 14 && c.host.weapon_word(ship_sb(S, 0xb8 + k), 0x0a) != 0) return -2000;
        if (W(0x2c) > 3) return -2000;
    }
    std::int16_t sc = 0;  // [bp-0x16]
    std::int16_t rel = -100, str = 100;  // [bp-0x10], [bp-0x12]
    if (gp == 14) {
        rel = c.host.relation(i, gr);
        str = c.host.strength(i, gr);
    }
    std::int16_t want = rel < 0 ? std::int16_t{8} : std::int16_t{6};  // [bp-0x14] [BYTES seg4:0x6b25-0x6b37]
    if (rel < -60) want = add16(want, 1);
    if (rel > 60) want = sub16(want, 1);
    if (rel < -270) want = add16(want, 1);
    if (rel > 250) want = sub16(want, 1);
    want = sub16(want, static_cast<std::int16_t>(P.cls()));
    if (str < 60) want = add16(want, 1);
    if (str < 25) want = add16(want, 1);
    const std::int16_t team = c.team(i);
    if (team == 3 || team == 1 || team == 4) want = add16(want, 2);
    {
        const int off = pw::kFight0 + gr;  // byte +0x13e + goal rec
        std::int8_t f = 0;
        if (off >= 0 && off < static_cast<int>(kShipRecordSize))
            f = ship_sb(S, off);
        else
            guard_todo(g_once.grudge, "a fight byte past the ship record", "seg4:0x6bac");
        want = sub16(want, static_cast<std::int16_t>(static_cast<std::int8_t>(f / 2)));
    }
    if (P.order() == 3 || P.order() == 4) want = add16(want, 2);
    if (W(0x2a) < want) {  // [BYTES seg4:0x6bcb-0x6bea]: += (3·(W − want)) / 4 toward zero
        const std::int16_t t = mul16(sub16(W(0x2a), want), 3);
        want = add16(want, sar16(add16(t, t < 0 ? std::int16_t{3} : std::int16_t{0}), 2));
    }
    if (gp == 14 && ship_or_zero(c, gr, "seg4:0x6bf8").be2 != 0 && W(0x42) == 0) sc = add16(sc, 7);
    if (W(0x42) != 0 && gp != 14) return 0;
    if (W(0x2a) < 3) return 0;
    if (sub16(W(0x2a), 3) > want) return 0;
    const std::int16_t r = static_cast<std::int16_t>(c.rng.rand() % 3);  // [BYTES seg4:0x6c3f-0x6c63]
    sc = add16(sc, add16(sub16(r, abs16(sub16(want, W(0x2a)))), 12));
    if (w == 2) {  // heavy rocks and posts near the ship [BYTES seg4:0x6c6e-0x6d47]
        std::int16_t n = 0;
        for (int pool : {3, 8}) {
            for (int q = 0; q < c.pools.in_use(pool); ++q) {
                const ObjectRecord& R = c.obj(pool, q);
                if (R.state != 0 || !(R.mass > 0x1f4)) continue;
                if (!(cells(R, c.obj(kPoolShips, i)) < 0x140)) continue;
                n = add16(n, 1);
                if (n > 1) break;
            }
        }
        if (n == 0) return 0;
        if (n > 1) sc = add16(sc, 2);
    }
    if (P.goal_act() == 4) {  // [BYTES seg4:0x6d4b-0x6d71]
        if (W(0x04) == 2) sc = add16(sc, 12);
        if (W(0x04) == 3) sc = add16(sc, 7);
    }
    if (S.fuel < 250 && W(0x2e) != 0) {  // [BYTES seg4:0x6d75-0x6dbe]
        sc = sub16(sc, 2);
        if (S.fuel < 0x4b) sc = sub16(sc, 2);
        if (W(0x2e) > 0x32) sc = sub16(sc, 2);
        if (add16(W(0x2e), W(0x2e)) > S.fuel) return 0;
    }
    if (W(0x2c) != 0 && mul16(add16(W(0x2c), 5), 5) > S.w26) {  // [BYTES seg4:0x6dc2-0x6dfd]
        sc = sub16(sc, W(0x2c));
        if (add16(mul16(W(0x2c), 3), 0xd) > S.w26 || gp != 14) return -100;
    }
    if (sc > 0 && W(0x42) != 0 && gp == 14) {  // [BYTES seg4:0x6dff-0x6e62]
        if (P.wait2() > 0) return 0;
        if (P.goal_act() == 4) sc = add16(sc, 3);
        if (str > 0xfa) return 0;
        const int off = pw::kFight0 + gr;
        const std::int8_t f = (off >= 0 && off < static_cast<int>(kShipRecordSize)) ? ship_sb(S, off) : std::int8_t{0};
        if (f < 0) sc = add16(sc, sub16(3, add16(f, f)));
    }
    return sc;
}

void choose_weapon(PilotCtx& c, int i) {
    ShipRecord& S = c.s(i);
    Pilot P(S);
    P.weapon_class(0);  // [BYTES seg4:0x6e8a]
    if (static_cast<std::int8_t>(S.slots_cb()) <= 1) return;  // [BYTES seg4:0x6e99-0x6e9e]
    std::int16_t best = -1000, bk = -1;  // [bp-6], [bp-8]
    for (int k = 1; k < static_cast<std::int8_t>(S.slots_cb()); ++k) {  // [BYTES seg4:0x6ea2-0x6f14]
        if (!(ship_sb(S, 0xae + k) > 0)) continue;
        std::int16_t sc = 0;
        if (P.goal_pool() == 0 && static_cast<std::int8_t>(c.obj(0, P.goal_rec()).flame) < 0 && ship_sb(S, 0xb8 + k) == 5)
            sc = 10000;
        else
            sc = weapon_score(c, i, k);
        if (sc > best) {
            bk = static_cast<std::int16_t>(k);
            best = sc;
        }
    }
    auto select = [&](std::int16_t k) {  // [BYTES seg4:0x6f1c-0x7002]
        const std::int16_t cls = c.host.weapon_word(ship_sb(S, 0xb8 + k), 0x04);
        P.weapon_class(cls == 2 ? 2 : cls == 3 ? 3 : 1);
        S.weapon_slot = k;
    };
    if (best > 0) {
        select(bk);
    } else if (best == -100) {  // the cheapest affordable slot [BYTES seg4:0x6f5c-0x6fbc]
        std::int16_t cheap = 100;
        for (int k = 1; k < static_cast<std::int8_t>(S.slots_cb()); ++k) {
            if (ship_sb(S, 0xae + k) < 0) continue;
            const std::int8_t w = ship_sb(S, 0xb8 + k);
            if (!(c.host.weapon_word(w, 0x2e) < S.fuel)) continue;
            if (!(c.host.weapon_word(w, 0x2c) < cheap)) continue;
            cheap = c.host.weapon_word(w, 0x2c);
            bk = static_cast<std::int16_t>(k);
        }
        if (cheap < 100) {
            select(bk);
        } else {
            S.weapon_slot = 0;
            P.weapon_class(1);
        }
    } else {
        S.weapon_slot = 0;  // the gun [BYTES seg4:0x7008-0x7013]
        P.weapon_class(1);
    }
    if (i == c.g.player) c.host.weapon_display();  // seg11:0x2f8c [BYTES seg4:0x7018-0x7020]
}

// --- seg4:0x59bc and seg4:0x8626, the player's activity estimate ------------------------------------
std::int16_t estimate_score(PilotCtx& c, int pool, int rec, int /*a*/) {
    WaveGlobals& g = c.g;
    const std::uint32_t now = g.now;
    std::int16_t l8 = 10, sc = 0;  // [bp-8], [bp-6]
    c.pg.w968a = 0;
    c.pg.attacked = (c.w.w8e28 + 2000u > now) ? std::int16_t{1} : std::int16_t{0};  // `jbe` [BYTES seg4:0x8642-0x865a]
    std::int16_t att = c.pg.attacked;  // [bp-0x36]
    const Pilot Pl = c.p(g.player);
    if (Pl.goal_act() == 1 && c.w.w8e28 + 3000u > now && Pl.goal_pool() == pool && Pl.goal_rec() == rec) {
        c.pg.attacked = 1;
        att = 1;
    }
    c.pg.capture_gate = 0;  // DGROUP:0x9688
    const ObjectRecord& O = c.obj(pool, rec);
    if (pool == 14 || O.b53 != 0) {  // [BYTES seg4:0x86a1-0x86c0]
        sc = 30;
    } else if (pool == 9 || (pool == 8 && pool8_word(c, rec, 0x1c) == 2) ||
               (pool == 2 && c.host.defender_word(rec, 0) == 15)) {
        sc = 20;
    } else if (pool == 5) {
        sc = -200;
    }
    if (sc != 0 && att != 0) sc = add16(sc, 25);  // [BYTES seg4:0x86fc-0x8706]
    const ObjectRecord& me = c.obj(kPoolShips, g.player);
    std::int16_t dx = 0, dy = 0;
    const std::int16_t d = cells_between(O, me.x, me.y, dx, dy);  // [bp-0xe] [BYTES seg4:0x870a-0x873a]
    // The bearing of O from the player against the heading, in degrees [BYTES seg4:0x873d-0x8773]
    const std::int16_t dir = direction16(sub16(0, dy), sub16(0, dx));  // seg8:0x19b0(den -dx, num -dy)
    const std::int16_t t = mul16(sub16(me.frame, dir), 0x168);
    const std::int16_t ang = sar16(add16(t, t < 0 ? std::int16_t{0xf} : std::int16_t{0}), 4);
    const std::int32_t facing = sx(cos128(ang));  // [bp-0x24] seg6:0x720c
    const std::int32_t vx = sx(me.vx), vy = sx(me.vy);
    const std::int32_t speed = sx(magnitude(vx, vy));  // [bp-0x20] seg8:0x35d0
    std::int32_t q = 0;
    if (d == 0) {  // seg5:0x110 traps [DESIGN ADR 0016]
        guard_todo(g_once2.estimate0, "the player on the scored object (a zero divisor)", "seg4:0x87ed");
    } else {
        q = ldiv32(add32(mul32(vy, sx(dy)), mul32(vx, sx(dx))), sx(d));  // [BYTES seg4:0x87bb-0x87ed]
    }
    const std::int32_t closing = sub32(0, q);  // [bp-0x34] `neg ax; adc dx, 0; neg dx`
    // seg8:0x35d0 returns at least 1 [BYTES seg8:0x367b]; the guard keeps the port defined
    // [DESIGN ADR 0016].
    const std::int32_t approach =
        speed == 0 ? 0 : ldiv32(shl32(closing, 7), speed);  // [bp-0x28] [BYTES seg4:0x87ff-0x8815]
    std::int16_t hits = 0, recent = 0, fresh = 0;  // [bp-0x1a], [bp-0x18], [bp-0x16]
    if (pool == 14) {  // the target ship's hit stamps +0x44 ... +0x4c [BYTES seg4:0x8824-0x88ab]
        const Pilot T(ship_or_zero(c, rec, "seg4:0x883c"));
        for (int k = 0; k < 5; ++k) {
            const std::int16_t st = T.hit(k);
            if (st == 0) continue;
            hits = add16(hits, 1);
            if (within(g.game_frames, 10, st, 0x24)) fresh = add16(fresh, 1);
            if (within(g.game_frames, 10, st, 0x12c)) recent = add16(recent, 1);
        }
    } else {  // the hit history DGROUP:0x938a/0x9392/0x937a [BYTES seg4:0x88ad-0x8936]
        for (int k = 0; k < 8; ++k) {
            const std::int16_t hp = static_cast<std::int8_t>(c.w.hit_pool[static_cast<std::size_t>(k)]);
            const std::int16_t hr = static_cast<std::int8_t>(c.w.hit_rec[static_cast<std::size_t>(k)]);
            if (hp != pool) continue;
            if (hr != rec && pool != 3) continue;
            const std::int16_t st = c.w.w937a[static_cast<std::size_t>(k)];
            if (st == 0) continue;
            hits = add16(hits, 1);
            if (within(g.game_frames, 10, st, 0x24)) fresh = add16(fresh, 1);
            if (within(g.game_frames, 10, st, 0x12c)) recent = add16(recent, 1);
        }
    }
    if (att != 0) {  // [BYTES seg4:0x893a-0x8997]
        std::int16_t v = shl16(add16(add16(sub16(recent, 3), sub16(recent, 3)), hits), 2);
        v = add16(v, lo16(ldiv32(facing, 2)));
        v = add16(v, lo16(ldiv32(approach, 8)));
        v = add16(v, mul16(fresh, 0xb));
        sc = add16(sc, v);
        if (hits == 0) sc = sub16(sc, 30);
        if (hits == 1) sc = sub16(sc, 15);
        if (recent == 0) sc = sub16(sc, 15);
    } else {  // [BYTES seg4:0x899a-0x8a7c]
        const std::int16_t base = add16(lo16(ldiv32(approach, 3)), lo16(ldiv32(facing, 5)));
        std::int16_t v = add16(base, add16(recent, recent));
        v = add16(v, mul16(fresh, 3));
        v = add16(v, hits);
        sc = add16(sc, v);
        if (pool == 12 && approach > 0x4b && facing > 0x4b) sc = add16(sc, add16(base, 10));
        bool close_in = approach > 0x69 || facing > 0x69 || (approach > 0x50 && facing > 0x50) ||
                    (approach > 0x37 && facing > 0x37 && O.b53 == 1);
        if (close_in && d < 0x168) {
            c.pg.capture_gate = 1;
            if (O.b53 == 1) sc = add16(sc, add16(lo16(ldiv32(approach, 3)), lo16(ldiv32(facing, 5))));
        }
    }
    // A ship hunting the player, met head on [BYTES seg4:0x8a7f-0x8ae2]
    if (approach < -0x3c && facing < -0x46 && closing < -0x1e && pool == 14) {
        const Pilot T(ship_or_zero(c, rec, "seg4:0x8a9a"));
        if (T.goal_pool() == 14 && T.goal_rec() == g.player &&
            (T.goal_act() == 1 ||
             (T.order() >= 2 && T.order() <= 4 && T.order_pool() == 14 && T.order_rec() == g.player))) {
            c.pg.w968a = 1;
            att = 0;
            sc = add16(sc, sub16(25, lo16(closing)));
        }
    }
    sc = add16(sc, pool == 12 ? add16(idiv16(d, -7), 0x37) : add16(idiv16(d, -10), 0x28));  // [BYTES seg4:0x8ae5-0x8b05]
    if (Pl.est_pool() == pool && Pl.est_rec() == rec && sc > 0) l8 = 12;
    if (Pl.order() >= 2 && Pl.order() <= 4 && Pl.order_pool() == pool && Pl.order_rec() == rec) sc = add16(sc, 15);
    std::int16_t mp = -1, mr = -1;
    c.host.mark_6e52(mp, mr);
    if (pool == mp && rec == mr) sc = add16(sc, 30);
    return idiv16(mul16(l8, sc), 10);  // `imul; cwd; idiv` keeps the low word [BYTES seg4:0x8b69-0x8b73]
}

std::int16_t player_estimate(PilotCtx& c) {
    WaveGlobals& g = c.g;
    Pilot P = c.p(g.player);
    bool none = true;  // [bp-0x2c]
    if (P.order() == 1) {  // [BYTES seg4:0x59fa-0x5a3b]
        for (int j = 0; j <= last_ship(c); ++j) {
            const Pilot J = c.p(j);
            if (J.goal_pool() == P.order_pool() && J.goal_rec() == P.order_rec()) none = false;
        }
    }
    greed_level(c);  // seg4:0x84de [BYTES seg4:0x5a3e]
    if (P.order() >= 2 && P.order() <= 4) {  // [BYTES seg4:0x5a41-0x5b3c]
        none = false;
        bool on = false;
        if (P.est_pool() == P.order_pool()) {
            if (P.est_rec() == P.order_rec())
                on = true;
            else if (P.est_pool() == 9 && c.rng.rand() % 5 == 1)
                on = true;
        }
        if (on && P.est_act() == 1) {
            P.order(add16(P.order(), 1));
            if (P.order_pool() == 14 && within(g.game_frames, 10, ship_or_zero(c, P.order_rec(), "seg4:0x5ab1").w48, 0x28))
                P.order(4);
            if (P.order() >= 4) {
                if (P.order_by() != g.player) {
                    bidding_report(c, P.order_by());  // seg13:0x7b40 (pilot_orders.h) [BYTES seg4:0x5ae9-0x5aed]
                } else if (P.order_pool() == 14) {
                    const Pilot T(ship_or_zero(c, P.order_rec(), "seg4:0x5b0e"));
                    if (T.order() == 2 && T.order_rec() == g.player && T.order_by() == P.order_rec())
                        c.host.message(g.player, P.order_pool(), P.order_rec(), 0x1f);
                }
                P.order(0);
            }
        }
    }
    if (P.order() != 0) {  // [BYTES seg4:0x5b42-0x5b99]
        bool drop = false;
        if (!(P.order_until() + 20000u >= g.now)) {
            const std::int16_t st = ship_or_zero(c, P.order_rec(), "seg4:0x5b71").w44;
            if (!within(g.game_frames, 10, st, 0x1a)) drop = true;  // `cmp cx, 0x19; ja`
            else if (none) drop = true;
        } else if (none) {
            drop = true;
        }
        if (drop) P.order(-1);
    }
    if (P.order() == -1) P.order(0);  // [BYTES seg4:0x5b9f-0x5bac]
    // The best candidate [BYTES seg4:0x5bb2-0x5c73]
    std::int16_t best = -40;
    std::int16_t bp_ = 0, br = 0;
    // [bp-0x2e], [bp-0x30], [bp-0x32]: written only by a candidate that beats -40; the original
    // leaves the stack's word otherwise (pitfalls.md), the port 0 [DESIGN ADR 0016].
    std::int16_t k9686 = 0, k968a = 0, k9688 = 0;
    const int nvis = c.host.visible_count();
    for (int k = 0; k < nvis; ++k) {
        std::int8_t vp = 0, vr = 0;
        c.host.visible_entry(k, vp, vr);
        if (vp == 14 && vr == g.player) continue;
        const std::int16_t s = estimate_score(c, vp, vr, 1);
        if (s > best) {
            best = s;
            bp_ = vp;
            br = vr;
            k9686 = c.pg.attacked;
            k968a = c.pg.w968a;
            k9688 = c.pg.capture_gate;
        }
    }
    for (int k = 0; k < g.gates; ++k) {
        const std::int16_t r = add16(shl16(static_cast<std::int16_t>(k), 1), 1);
        const std::int16_t s = estimate_score(c, 12, r, 1);
        if (s > best) {
            best = s;
            bp_ = 12;
            br = r;
            k9686 = c.pg.attacked;
            k968a = c.pg.w968a;
            k9688 = c.pg.capture_gate;
        }
    }
    if (best == -40)  // no candidate wrote [bp-0x2e]: the original stores a stale stack word
        guard_todo(g_once2.help, "seg4:0x59bc's [bp-0x2e] unwritten: DGROUP:0x9686 = 0 (ADR 0016)",
                   "seg4:0x5c75");
    c.pg.attacked = k9686;  // [BYTES seg4:0x5c75-0x5c78]
    if (!(best > 0) && (k9686 != 0 || !(best > -40))) {  // [BYTES seg4:0x5c7b-0x5c8b]
        P.est_app(0);  // [BYTES seg4:0x5d71-0x5d92]
        P.est_pool(8);
        P.est_rec(0);
        P.est_act(3);
        P.est_app(2);
        return 0;
    }
    P.est_pool(bp_);  // [BYTES seg4:0x5c8f-0x5ca5]
    P.est_rec(br);
    P.est_app(2);
    if (c.pg.attacked == 0 && bp_ == 14) {  // [BYTES seg4:0x5cab-0x5cf9]
        const ShipRecord& T = ship_or_zero(c, br, "seg4:0x5cb7");
        // T's grudge byte against the player, +0xc2 + player of T (`imul si, rec, 0x15a; mov bx,
        // player`) [BYTES seg4:0x5ce8-0x5cf7]
        if ((T.w46 != 0 && within(g.game_frames, 36, T.w46, 10)) || ship_sb(T, pw::kGrudge0 + g.player) > 2)
            c.pg.attacked = 1;
    }
    if (c.pg.attacked != 0) {
        P.est_act(1);
    } else if ((c.obj(bp_, br).b53 != 0 || bp_ == 12) && k9688 != 0) {
        P.est_act(2);
    } else if (k968a != 0) {
        P.est_act(4);
        P.est_app(5);
    } else {
        P.est_act(3);
    }
    return best;
}

}  // namespace is::world
