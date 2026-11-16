// The computer pilot's orders and reactions (src/world/pilot_orders.h) against numbers worked
// by hand from the listings: seg4:0x8346, seg4:0x79ae, seg4:0x702e, seg13:0x6900 and seg13:0xa6bc
// (work/world/m5s3/fix-report.md, "Readings no verifier has checked").
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/flight.h"
#include "world/pilot.h"
#include "world/pilot_orders.h"
#include "world/pilot_think.h"
#include "world/relations.h"
#include "world/world.h"

using namespace is::world;

namespace {

struct Msg {
    int to, pool, rec, code;
};

class OrdersHost : public PilotHost {
public:
    std::array<std::int16_t, 8> team{}, tank{};  // by design index
    std::array<std::array<std::int16_t, 8>, 8> rel{};
    std::int16_t str = 100;
    std::vector<Msg> msgs;
    std::vector<std::array<int, 3>> calls, defenders, targets;
    std::vector<int> arrests, shields, cloaks, thrusts;
    std::vector<std::uint32_t> notices;
    std::vector<std::array<int, 4>> sounds;
    std::int16_t toggle_result = 1;

    std::int16_t design_word(int n, int off) override {
        if (n < 0 || n >= 8) return 0;
        if (off == 0x38) return team[static_cast<std::size_t>(n)];
        if (off == 0x1e) return tank[static_cast<std::size_t>(n)];
        return 0;
    }
    std::int16_t relation(int a, int b) override { return rel[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)]; }
    std::int16_t strength(int, int) override { return str; }
    std::int16_t team_word(int, int) override { return 0; }
    void message(int to, int pool, int rec, int code) override { msgs.push_back({to, pool, rec, code}); }
    std::int16_t call_in(int s, int kind, int msg) override {
        calls.push_back({s, kind, msg});
        return 1;
    }
    void target(int s, int p, int r) override { targets.push_back({s, p, r}); }
    std::int16_t visible(int, int) override { return 0; }
    void thrust(int i, int) override { thrusts.push_back(i); }
    void send_defenders(int i, int p, int r) override { defenders.push_back({i, p, r}); }
    std::int16_t cloak_toggle(int i) override {
        cloaks.push_back(i);
        return toggle_result;
    }
    std::int16_t shield_toggle(int i) override {
        shields.push_back(i);
        return toggle_result;
    }
    void notice(std::uint32_t ids) override { notices.push_back(ids); }
    void sound(int id, int p, int r, int m) override { sounds.push_back({id, p, r, m}); }
    void arrest(int e) override { arrests.push_back(e); }
    std::string lang(int id) override {
        if (id == 3033)  // [BYTES ENG_LANG.DLL STRING 190] id 3033
            return "%<Hey! %>%s! %<I Remember That %>You %<%S%>%<Killed a %s %# Ago!%>";
        return "<" + std::to_string(id) + ">";
    }
    std::string pilot_name() override { return "PILOT"; }
    std::string shp_name(int n) override { return "SHP" + std::to_string(n); }
    void reactions(int) override {}
    void seg13_9ae8(int) override {}
    void seg13_9ce0(int) override {}
};

struct Rig {
    World w;
    WaveGlobals g;
    PilotGlobals pg;
    is::shell::Rng rng;
    OrdersHost host;
    PilotCtx c{w.pools, w.ships, g, w, pg, rng, host};

    Rig() {
        g.now = 100000;
        g.player = 0;
        g.last_ship = 2;
        g.gates = 1;
        g.detail = 3;
        w.last_ship = 2;
        for (int i = 0; i < kShipCount; ++i) {
            Pilot P = pilot(w.ships, i);
            P.claim_pool(-1);
            P.claim_rec(-1);
            P.called_for(-1);
            w.ships[static_cast<std::size_t>(i)].design_index = static_cast<std::int16_t>(i);
            w.ships[static_cast<std::size_t>(i)].bec = 1;
        }
        for (int p = 0; p < kPoolCount; ++p)
            if (w.pools.exists(p)) w.pools.set_in_use(p, 0);
        w.pools.set_in_use(kPoolShips, 3);
        for (int i = 0; i < 3; ++i) o(kPoolShips, i).on_screen = 1;
    }
    ShipRecord& s(int i) { return w.ships[static_cast<std::size_t>(i)]; }
    Pilot p(int i) { return pilot(w.ships, i); }
    ObjectRecord& o(int pool, int rec) { return w.pools.rec(pool, rec); }
    void place(int pool, int rec, int x, int y) {
        o(pool, rec).x = static_cast<std::int16_t>(x);
        o(pool, rec).y = static_cast<std::int16_t>(y);
    }
};

int draws(is::shell::Rng before, const is::shell::Rng& after) {
    for (int n = 0; n <= 64; ++n) {
        if (before.state() == after.state()) return n;
        before.rand();
    }
    return -1;
}

// A state whose rand() sequence satisfies `pred` (the draws given in order).
template <class Pred>
std::uint32_t state_where(Pred pred) {
    for (std::uint32_t s = 1;; ++s) {
        is::shell::Rng r;
        r.set_state(s);
        if (pred(r)) return s;
    }
}

}  // namespace

TEST_CASE("seg4:0x8346: kind 3 against the own attack goal keeps the last kind; returns 1") {
    Rig r;
    Pilot P = r.p(1);
    set_ship_b(r.s(1), pw::kReaction, 7);
    P.goal_act(1);
    P.goal_pool(14);
    P.goal_rec(2);
    CHECK(ship_reaction(r.c, 1, 14, 2, 3) == 1);
    CHECK(ship_sb(r.s(1), pw::kReaction) == 7);  // `cbw` of +0x146 written back
    CHECK(ship_reaction(r.c, 1, 14, 0, 3) == 1);  // not the goal: kind 3 stays
    CHECK(ship_sb(r.s(1), pw::kReaction) == 3);
    REQUIRE(r.host.defenders.size() == 2);
    CHECK(r.host.defenders[0] == std::array<int, 3>{1, 14, 2});
    // The player in a player game (DGROUP:0x95bc != 0) sends no defenders [BYTES seg4:0x837c-0x8389].
    r.g.demo = 1;
    CHECK(ship_reaction(r.c, 0, 14, 1, 5) == 1);
    CHECK(r.host.defenders.size() == 2);
    CHECK(ship_sb(r.s(0), pw::kReaction) == 5);
}

TEST_CASE("seg4:0x79ae: the gates draw nothing; a raised shield with no hunter near") {
    Rig r;
    Pilot P = r.p(1);
    r.host.team = {0, 1, 0, 0, 0, 0, 0, 0};
    cloak_shield(r.c, 1);  // no cloak, no shield
    CHECK(r.rng.state() == 1u);
    P.has_shield(1);
    set_ship_b(r.s(1), pw::kWing0, 0);  // wing 0 off: nothing [BYTES seg4:0x79e3]
    cloak_shield(r.c, 1);
    CHECK(r.rng.state() == 1u);
    set_ship_b(r.s(1), pw::kWing0, 1);
    set_ship_w(r.s(1), pw::kShieldStep, 7);  // up: u = 0
    P.goal_act(3);
    // rand() % 3 = 0 and team 1: down through seg11:0x4b00, reason 0x411 [BYTES seg4:0x7b38-0x7b90].
    r.rng.set_state(state_where([](is::shell::Rng& g) { return g.rand() % 3 == 0; }));
    is::shell::Rng before = r.rng;
    cloak_shield(r.c, 1);
    CHECK(draws(before, r.rng) == 1);
    CHECK(r.host.shields == std::vector<int>{1});
    CHECK(r.host.notices.empty());  // not the player
    // rand() % 3 != 0: block D (not hunting, +0x38 != 1) lowers it with reason 0x414.
    r.rng.set_state(state_where([](is::shell::Rng& g) { return g.rand() % 3 != 0; }));
    before = r.rng;
    cloak_shield(r.c, 1);
    CHECK(draws(before, r.rng) == 1);
    CHECK(r.host.shields.size() == 2);
    // The player gets seg3:0x332e(0x0408 << 16 | 0x411).
    Pilot Q = r.p(0);
    Q.has_shield(1);
    set_ship_b(r.s(0), pw::kWing0, 1);
    set_ship_w(r.s(0), pw::kShieldStep, 7);
    r.rng.set_state(state_where([](is::shell::Rng& g) { return g.rand() % 3 == 0; }));
    cloak_shield(r.c, 0);
    REQUIRE(r.host.notices.size() == 1);
    CHECK(r.host.notices[0] == 0x04080411u);
}

TEST_CASE("seg4:0x79ae: a hunter within 500 cells raises the cloak by rand() % 5") {
    Rig r;
    r.host.team = {0, 1, 0, 0, 0, 0, 0, 0};
    Pilot P = r.p(1);
    P.has_cloak(1);
    r.s(1).fuel = 500;
    r.place(kPoolShips, 1, 1000, 1000);
    r.place(kPoolShips, 2, 1000 + 8 * 100, 1000);  // 100 cells
    Pilot H = r.p(2);
    H.goal_pool(14);
    H.goal_rec(1);
    H.goal_act(1);
    // u = 1 (down), h = 2; rand() % 5 = 0 and fight byte +0x13e + 2 = 0 < 2: cloak, 0x407 / 0x412.
    r.rng.set_state(state_where([](is::shell::Rng& g) { return g.rand() % 5 == 0; }));
    is::shell::Rng before = r.rng;
    cloak_shield(r.c, 1);
    CHECK(draws(before, r.rng) == 1);
    CHECK(r.host.cloaks == std::vector<int>{1});
    // rand() % 5 != 0 with the fight byte >= -1: no toggle, and the Pirate block is not reached.
    r.rng.set_state(state_where([](is::shell::Rng& g) { return g.rand() % 5 != 0; }));
    before = r.rng;
    cloak_shield(r.c, 1);
    CHECK(draws(before, r.rng) == 1);
    CHECK(r.host.cloaks.size() == 1);
    // A hunter at 600 cells is no hunter: u = 1 and h = -1, not a Pirate: return, no draw.
    r.place(kPoolShips, 2, 1000 + 8 * 600, 1000);
    before = r.rng;
    cloak_shield(r.c, 1);
    CHECK(draws(before, r.rng) == 0);
    // Low fuel (< 100) with a cloak: nothing [BYTES seg4:0x79fc].
    r.s(1).fuel = 99;
    r.place(kPoolShips, 2, 1000 + 8 * 100, 1000);
    cloak_shield(r.c, 1);
    CHECK(draws(before, r.rng) == 0);
}

TEST_CASE("seg4:0x702e: the gates and the countdown") {
    Rig r;
    r.host.team = {0, 1, 0, 0, 0, 0, 0, 0};
    Pilot P = r.p(1);
    CHECK(revenge_recall(r.c, 1) == 0);  // no memory of the player
    P.ee(0, 1);
    P.revenge(0, 7);
    set_ship_w(r.s(1), pw::kCountdown, 4);
    CHECK(revenge_recall(r.c, 1) == 0);  // 4: message 0x88, then 3
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0].code == 0x88);
    CHECK(ship_w(r.s(1), pw::kCountdown) == 3);
    CHECK(revenge_recall(r.c, 1) == 0);  // 3: -1 below 4, then -1 again on screen
    CHECK(ship_w(r.s(1), pw::kCountdown) == 1);
    CHECK(r.rng.state() == 1u);
}

TEST_CASE("seg4:0x702e: an enemy of another team is taunted and attacked; three draws") {
    Rig r;
    r.host.team = {0, 1, 0, 0, 0, 0, 0, 0};  // the player team 0, ship 1 team 1
    r.w.w934e = 10;
    r.s(1).w26 = 80;
    Pilot P = r.p(1);
    P.ee(0, 1);       // "Attacked Me!" (849 + 1)
    P.revenge(0, 7);  // > 5, < 9
    P.cls(1);         // enemies byte 1 < 3: 669
    // rand() % 3 != 0 (a class name), then rand() & 1 even (remember), rand() & 1 odd (no "Hey!").
    r.rng.set_state(state_where([](is::shell::Rng& g) {
        return g.rand() % 3 != 0 && (g.rand() & 1) == 0 && (g.rand() & 1) == 1;
    }));
    is::shell::Rng before = r.rng;
    CHECK(revenge_recall(r.c, 1) == 1);
    CHECK(draws(before, r.rng) == 3);
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0].to == 0);
    CHECK(r.host.msgs[0].code == 0x50);  // 0x4f + k, k = 1
    CHECK(P.goal_pool() == 14);
    CHECK(P.goal_rec() == 0);
    CHECK(P.est_act() == 1);
    CHECK(P.goal_app() == 2);
    CHECK(P.order() == 0);
    CHECK(P.replan_at() == 108000u);
    CHECK(P.revenge(0) == 0);
    REQUIRE(r.host.defenders.size() == 1);  // seg4:0x8346(1, 14, 0, 8)
    CHECK(ship_sb(r.s(1), pw::kReaction) == 8);
    // The format 3033 with the lookups' placeholders.
    CHECK(r.pg.taunt == "<669>! I Remember That You <850>");
}

TEST_CASE("seg4:0x702e: a killer of the team: the killer slot names it and is cleared; two draws") {
    Rig r;
    r.host.team = {0, 1, 0, 0, 0, 0, 0, 0};
    r.w.w934e = 10;
    r.w.wave = 3;
    r.s(1).w26 = 80;
    r.s(0).roster_index = 5;
    Pilot P = r.p(1);
    P.ee(0, 4);       // "Killed My Friend!"
    P.revenge(0, 7);
    r.w.w4eec[3] = 5;  // team 1's first killer slot: roster record 5
    ContactWords& cw = contact_words();
    const ContactWords saved = cw;
    cw.w6b10[3] = 7;  // the dead ship's MASTER.SHP record
    cw.w4fd0[3] = 1;  // killed in wave 1: 2 waves ago
    // rand() % 3 = 0 (the pilot name), no remember draw (found), rand() & 1 even ("Hey! ").
    r.rng.set_state(state_where([](is::shell::Rng& g) { return g.rand() % 3 == 0 && (g.rand() & 1) == 0; }));
    is::shell::Rng before = r.rng;
    CHECK(revenge_recall(r.c, 1) == 1);
    CHECK(draws(before, r.rng) == 2);
    CHECK(r.pg.taunt == "Hey! PILOT! You Killed a SHP7 2 <12518> Ago!");
    CHECK(r.w.w4eec[3] == -1);
    cw = saved;
}

TEST_CASE("seg4:0x702e: a large grudge becomes command 3; a teammate with a small memory is forgiven") {
    Rig r;
    r.host.team = {0, 1, 0, 0, 0, 0, 0, 0};
    r.w.w934e = 10;
    r.s(1).w26 = 80;
    Pilot P = r.p(1);
    P.ee(0, 2);
    P.revenge(0, 15);  // 14 <= 15 < 18: order 3
    CHECK(revenge_recall(r.c, 1) == 1);
    CHECK(P.order() == 3);
    CHECK(P.order_pool() == 14);
    CHECK(P.order_rec() == 0);
    CHECK(P.order_by() == 1);
    CHECK(P.one_shot() == 1);
    CHECK(P.order_until() == 120000u);
    REQUIRE(r.host.targets.size() == 1);
    CHECK(r.host.targets[0] == std::array<int, 3>{1, 14, 0});
    // Ship 2 of team 0 with the player (team 0) in mind, memory 2: forgiven.
    Rig t;
    t.host.team = {0, 0, 0, 0, 0, 0, 0, 0};
    t.w.w934e = 10;
    t.s(2).w26 = 80;
    Pilot Q = t.p(2);
    Q.ee(0, 2);
    Q.revenge(0, 7);
    t.g.last_ship = 2;
    CHECK(revenge_recall(t.c, 2) == 0);
    CHECK(Q.ee(0) == 0);
    CHECK(Q.revenge(0) == 0);
}

TEST_CASE("seg13:0x6900: the head and the RENEGADE's refusal") {
    Rig r;
    r.host.team = {0, 6, 0, 0, 0, 0, 0, 0};
    Pilot P = r.p(1);
    P.order(2);
    P.order_by(0);
    P.one_shot(1);
    order_executor(r.c, 1);
    CHECK(P.order() == 0);
    CHECK(P.replan_at() == 100000u);  // now + 4000, then now
    CHECK(P.one_shot() == 0);
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0].to == 0);
    CHECK(r.host.msgs[0].code == 0x83);
}

TEST_CASE("seg13:0x6900: order 1 with nobody to fight becomes order 5") {
    Rig r;
    r.host.team = {0, 1, 0, 0, 0, 0, 0, 0};
    Pilot P = r.p(1);
    P.order(1);
    P.order_pool(14);
    P.order_rec(0);
    P.order_by(1);
    P.one_shot(1);
    order_executor(r.c, 1);  // seg13:0x78b8(1, 0) = -1: no ship hunts 0
    CHECK(P.order() == 5);
    CHECK(P.replan_at() == 100200u);
    CHECK(P.one_shot() == 0);
    REQUIRE(r.host.msgs.size() == 2);
    CHECK(r.host.msgs[0].to == 1);  // +0x58
    CHECK(r.host.msgs[1].to == 0);  // +0x56
    CHECK(r.host.msgs[1].code == 1);
}

TEST_CASE("seg13:0x6900: order 1 against an attacker k: estimate (14, k), the help message") {
    Rig r;
    r.host.team = {0, 1, 2, 0, 0, 0, 0, 0};
    Pilot P = r.p(1);
    P.order(1);
    P.order_pool(14);
    P.order_rec(0);
    P.order_by(1);
    P.one_shot(1);
    Pilot K = r.p(2);  // ship 2 hunts the player
    K.goal_pool(14);
    K.goal_rec(0);
    K.goal_act(1);
    r.host.rel[1][0] = 100;  // l8 = 100 + 12 + 0 >= l6 = 0: k accepted
    order_executor(r.c, 1);
    CHECK(P.est_pool() == 14);
    CHECK(P.est_rec() == 2);
    CHECK(P.est_act() == 1);
    CHECK(P.est_app() == 2);
    REQUIRE(r.host.msgs.size() == 2);
    CHECK(r.host.msgs[0].to == 0);
    CHECK(r.host.msgs[0].code == 1);
    CHECK(r.host.msgs[1].to == 2);
    CHECK(r.host.msgs[1].code == 0x81);  // not a Knight: 1418
    REQUIRE(r.host.defenders.size() == 1);  // seg4:0x8346(1, 14, 2, 14)
    CHECK(ship_sb(r.s(1), pw::kReaction) == 14);
}

TEST_CASE("seg13:0x6900: orders 9, 10, 0x0c and the default") {
    Rig r;
    r.host.team = {0, 1, 2, 0, 0, 0, 0, 0};
    Pilot P = r.p(1);
    // Order 10: the nearest gate's beam record, message 0xb7 for each nearer one.
    r.g.gates = 2;
    r.w.pools.set_in_use(kPoolGateCentres, 4);
    r.place(kPoolShips, 1, 1000, 1000);
    r.place(kPoolGateCentres, 0, 1000 + 8 * 300, 1000);
    r.place(kPoolGateCentres, 2, 1000 + 8 * 100, 1000);
    P.order(10);
    order_executor(r.c, 1);
    CHECK(P.est_pool() == 12);
    CHECK(P.est_rec() == 2);
    CHECK(r.host.msgs.size() == 2);
    // Order 0x0c: the nearest live fuel tank, or message 0x38 and the order dropped.
    r.host.msgs.clear();
    P.order(0x0c);
    order_executor(r.c, 1);
    CHECK(P.order() == 0);
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0].code == 0x38);
    r.w.pools.set_in_use(4, 2);
    r.place(4, 0, 1000 + 8 * 50, 1000);
    r.place(4, 1, 1000 + 8 * 20, 1000);
    P.order(0x0c);
    order_executor(r.c, 1);
    CHECK(P.order_pool() == 4);
    CHECK(P.order_rec() == 1);
    CHECK(P.est_act() == 2);
    // Order 9 with two hunters: message 0x21, +0x54 = 0, the order dropped.
    r.host.msgs.clear();
    P.order(9);
    P.order_pool(0);
    P.order_rec(0);
    r.o(kPoolIcons, 0).state = 0;
    for (int j : {0, 2}) {
        Pilot J = r.p(j);
        J.goal_pool(14);
        J.goal_rec(1);
        J.goal_act(1);
    }
    order_executor(r.c, 1);
    CHECK(P.order() == 0);
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0].code == 0x21);
    CHECK(P.est_act() == 2);
    // Any other order (6) is dropped.
    P.order(6);
    order_executor(r.c, 1);
    CHECK(P.order() == 0);
}

TEST_CASE("seg13:0xa6bc: +0x94, the patrol's next ship, the warning, the arrest call, the give-up") {
    Rig r;
    r.host.team = {0, 7, 1, 0, 0, 0, 0, 0};
    Pilot E = r.p(1);
    r.o(kPoolShips, 1).w36 = 0;  // the hook holds something: no doughnut search
    // Patrol: the watched ship 0 is fine but the deadline is near: the next ship after 0 that is
    // not an Enforcer is 2; +0x98 = now + 13000; follow it.
    E.order(0x14);
    E.order_pool(14);
    E.order_rec(0);
    E.order_until(100000u + 4000u);
    order_executor(r.c, 1);
    CHECK(E.replan_at() == 100750u);
    CHECK(E.order_rec() == 2);
    CHECK(E.order_until() == 113000u);
    CHECK(E.est_pool() == 14);
    CHECK(E.est_rec() == 2);
    CHECK(E.est_act() == 3);
    // The watched ship hunts the Enforcer: attack and seg4:0x8346(e, 14, w, 0x12).
    Pilot W = r.p(2);
    W.goal_pool(14);
    W.goal_rec(1);
    W.goal_act(1);
    order_executor(r.c, 1);
    CHECK(E.est_act() == 1);
    CHECK(ship_sb(r.s(1), pw::kReaction) == 0x12);
    // The warning: +0xed = 1 sends 0x3b to +0x56; then it turns to patrol.
    E.order(0x15);
    E.one_shot(1);
    order_executor(r.c, 1);
    CHECK(E.one_shot() == 0);
    CHECK(r.host.msgs.back().code == 0x3b);
    CHECK(r.host.msgs.back().to == 2);
    CHECK(E.replan_at() == 102000u);
    order_executor(r.c, 1);
    CHECK(E.order() == 0x14);
    CHECK(E.order_until() == 105000u);
    // The arrest is the law's host call (ruling R3).
    E.order(0x16);
    order_executor(r.c, 1);
    CHECK(r.host.arrests == std::vector<int>{1});
    // Terminate with +0xed = 2: give up, leave by gate 0, a Terminator for the target.
    E.order(0x17);
    E.order_rec(2);
    E.one_shot(2);
    order_executor(r.c, 1);
    CHECK(E.est_pool() == 12);
    CHECK(E.est_rec() == 0);
    CHECK(E.one_shot() == 0);
    REQUIRE(r.host.calls.size() == 1);
    CHECK(r.host.calls[0] == std::array<int, 3>{2, 0x14, 0x58});
    // Terminate with +0xed = 0 against a live target: attack it.
    E.order(0x17);
    order_executor(r.c, 1);
    CHECK(E.est_pool() == 14);
    CHECK(E.est_rec() == 2);
    CHECK(E.est_act() == 1);
}

TEST_CASE("seg13:0xa6bc: a doughnut in flight is taken first") {
    Rig r;
    r.host.team = {0, 7, 1, 0, 0, 0, 0, 0};
    Pilot E = r.p(1);
    r.o(kPoolShips, 1).w36 = -1;
    r.w.pools.set_in_use(6, 2);
    r.w.pools.missile(0).weapon = 3;
    r.w.pools.missile(1).weapon = 0x10;
    E.order(0x14);
    order_executor(r.c, 1);
    CHECK(E.est_pool() == 6);
    CHECK(E.est_rec() == 1);
    CHECK(E.est_act() == 2);
    CHECK(E.est_app() == 2);
}

TEST_CASE("seg13:0x7b40: message 3, the order cleared in Action mode, the attitude change") {
    Rig r;
    r.host.team = {0, 1, 2, 0, 0, 0, 0, 0};
    Pilot P = r.p(0);
    P.order(4);
    P.order_pool(14);
    P.order_rec(2);
    const auto rng0 = r.rng;
    bidding_report(r.c, 1);
    CHECK(draws(rng0, r.rng) == 0);
    REQUIRE(r.host.msgs.size() >= 1);
    CHECK(r.host.msgs[0].to == 0);  // seg3:0x6ec(DGROUP:0x9646, 0xe, k, 3) [BYTES seg13:0x7b4a-0x7b55]
    CHECK(r.host.msgs[0].pool == 14);
    CHECK(r.host.msgs[0].rec == 1);
    CHECK(r.host.msgs[0].code == 3);
    CHECK(P.order() == 0);                        // Action mode [BYTES seg13:0x7b5d-0x7b6a]
    CHECK(ship_w(r.s(1), 0x62 + 2 * 0) > 0);      // A(k, P) moved by ds = 0x41 > 0
    // In Strategy mode (DGROUP:0x9134 != 0) the order stays.
    Rig s;
    s.host.team = r.host.team;
    s.g.play_mode = 1;
    Pilot Q = s.p(0);
    Q.order(4);
    Q.order_pool(9);  // against a virus: seg13:0x2714(k, P, 10, 0x16, 0, 6)
    bidding_report(s.c, 2);
    CHECK(Q.order() == 4);
    CHECK(ship_w(s.s(2), 0x62 + 2 * 0) > 0);
    CHECK(ship_w(s.s(2), 0x62 + 2 * 0) < ship_w(r.s(1), 0x62 + 2 * 0));  // 0x16 against 0x41
}
