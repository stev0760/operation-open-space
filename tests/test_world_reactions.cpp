// A computer ship's reactions (src/world/reactions.h): seg13:0x7ba2 and seg13:0x9bb8 against
// numbers worked by hand from the listings (work/analysis/m5/s4-commands.md section 2 with
// s4-commands-verify.md claims 50-56 and the Omissions on both routines).
#include <array>
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/commands.h"
#include "world/flight.h"
#include "world/pilot.h"
#include "world/pilot_think.h"
#include "world/reactions.h"
#include "world/relations.h"
#include "world/roster_build.h"
#include "world/ship_react.h"
#include "world/world.h"

using namespace is::world;

namespace {

struct Msg {
    int to, pool, rec, code;
    bool operator==(const Msg& o) const { return to == o.to && pool == o.pool && rec == o.rec && code == o.code; }
};

class Host : public PilotHost {
public:
    std::array<std::int16_t, 8> team{0, 0, 2, 3, 1, 1, 1, 1};
    std::int16_t str = 100;
    std::vector<Msg> msgs;
    std::vector<std::array<int, 3>> targets;
    std::vector<int> ambulance;
    std::int16_t design_word(int n, int off) override {
        if (n < 0 || n >= 8) return 0;
        if (off == 0x38) return team[static_cast<std::size_t>(n)];
        if (off == 0x1e) return 1000;
        return 0;
    }
    std::int16_t relation(int, int) override { return -500; }
    std::int16_t strength(int, int) override { return str; }
    void message(int to, int pool, int rec, int code) override { msgs.push_back({to, pool, rec, code}); }
    void target(int s, int p, int r) override { targets.push_back({s, p, r}); }
    std::int16_t call_in(int, int, int) override { return 0; }
    void send_defenders(int, int, int) override {}
    std::int16_t ambulance_call(int i) override {
        ambulance.push_back(i);
        return 1;
    }
};

class CHost : public CommandHost {
public:
    std::vector<DefenderEntry> table;
    void capture_count(int, int) override {}
    void attitude_change(World&, int, int, int, int, int, int) override {}
    std::vector<DefenderEntry>* defenders() override { return &table; }
};

struct Rig {
    World w;
    WaveGlobals g;
    PilotGlobals pg;
    is::shell::Rng rng;
    Host host;
    CHost ch;
    CommandWords cw;
    PilotCtx pc{w.pools, w.ships, g, w, pg, rng, host};
    CommandCtx c{pc, ch, cw};

    Rig() {
        g.now = 100000;
        g.player = 0;
        g.last_ship = 3;
        g.demo = 1;
        g.detail = 3;
        w.last_ship = 3;
        w.w934e = 30;
        w.wave = 3;
        for (int i = 0; i < kShipCount; ++i) {
            s(i).design_index = static_cast<std::int16_t>(i);
            s(i).bec = 1;
            s(i).w26 = 80;
            s(i).w28 = 80;
            s(i).w2a = 80;
            s(i).fuel = 500;
        }
        for (int p = 0; p < kPoolCount; ++p)
            if (w.pools.exists(p)) w.pools.set_in_use(p, 0);
        w.pools.set_in_use(kPoolShips, 4);
        for (int i = 4; i < 10; ++i) w.pools.rec(kPoolShips, i).state = 1;
        w.pools.set_in_use(7, 1);
    }
    ShipRecord& s(int i) { return w.ships[static_cast<std::size_t>(i)]; }
    Pilot p(int i) { return pilot(w.ships, i); }
    ObjectRecord& o(int pool, int rec) { return w.pools.rec(pool, rec); }
};

int draws(is::shell::Rng before, const is::shell::Rng& after) {
    for (int n = 0; n <= 64; ++n) {
        if (before.state() == after.state()) return n;
        before.rand();
    }
    return -1;
}

}  // namespace

TEST_CASE("seg13:0x7ba2: the flee from the strongest hunter, message 0x47 to it") {
    Rig r;
    // Ship 3 hunts ship 2 (goal (14, 2), +0x38 = 1); strength 30
    r.p(3).goal_pool(14);
    r.p(3).goal_rec(2);
    r.p(3).goal_act(1);
    r.host.str = 30;
    r.p(2).fight(3, -2);                   // < -1: n = 2
    set_ship_sb(r.s(2), pw::kClass, 2);    // e6 = 2·2·6 = 24
    r.s(2).w26 = 30;                       // 24 + 15 > 30: the trigger
    reactions(r.c, 2);
    // 20·2 + 35 = 75 > 30 and the fight byte < -1: flee from 3
    CHECK(r.p(2).goal_pool() == 14);
    CHECK(r.p(2).goal_rec() == 3);
    CHECK(r.p(2).est_pool() == 14);
    CHECK(r.p(2).est_rec() == 3);
    CHECK(r.p(2).goal_act() == 4);
    CHECK(r.p(2).est_act() == 4);
    CHECK(r.p(2).goal_app() == 5);
    CHECK(r.p(2).est_app() == 5);
    CHECK(r.p(2).replan_at() == 104000u);
    CHECK(r.host.msgs.front() == Msg{3, 14, 2, 0x47});
}

TEST_CASE("seg13:0x7ba2: the repair (order 0x0d) when the flee does not hold") {
    Rig r;
    r.p(3).goal_pool(14);
    r.p(3).goal_rec(2);
    r.p(3).goal_act(1);
    r.host.str = 500;                      // best = 500: 20·1 + 35 = 55 < 500
    set_ship_sb(r.s(2), pw::kClass, 2);    // e6 = 12; 15·2 + 20 = 50 > Engine? Engine 45: yes
    r.s(2).w26 = 45;
    r.s(2).w28 = 20;                       // 12 + 17 > 20: the trigger
    r.s(2).w2a = 40;                       // < 50, 8·2 + 35 = 51 > 45: the repair
    // flee_ok holds (50 > 45) but the fight byte is 0: no flee, the repair test runs
    r.g.last_ship = 3;
    reactions(r.c, 2);
    CHECK(r.p(2).order() == 0x0d);
    CHECK(r.p(2).order_by() == 2);
    CHECK(r.p(2).order_until() == 101000u);
    CHECK(r.p(2).replan_at() == 101000u);
    CHECK(r.p(2).one_shot() == 0);
}

TEST_CASE("seg13:0x7ba2: exits for the player, an Enforcer, a busy ship; the virus ask draws once") {
    Rig r;
    const is::shell::Rng before = r.rng;
    reactions(r.c, 0);  // the player in a player game
    r.host.team[1] = 7;
    reactions(r.c, 1);  // an Enforcer
    r.host.team[1] = 0;
    r.p(1).order(2);
    r.p(1).order_until(100001);
    r.w.pools.set_in_use(9, 2);
    reactions(r.c, 1);  // busy
    CHECK(draws(before, r.rng) == 0);
    CHECK(r.host.msgs.empty());
    // The virus: pool 9 with two records, the goal pool not 14, rand() % 3 = 0, +0x9c < now
    Rig q;
    q.w.pools.set_in_use(9, 2);
    q.o(9, 0).state = 1;  // the first in play is record 1
    std::uint32_t st = 1;
    for (;; ++st) {
        is::shell::Rng t;
        t.set_state(st);
        if (t.rand() % 3 == 0) break;
    }
    q.rng.set_state(st);
    const is::shell::Rng b2 = q.rng;
    reactions(q.c, 1);
    CHECK(q.host.targets.front() == std::array<int, 3>{1, 9, 1});
    // seg13:0x9970 -> seg13:0x8570: the wave clock is 30, so the attack routine runs its loop and
    // the coin flips may draw: at least the one draw of the reactions
    CHECK(draws(b2, q.rng) >= 1);
    Rig n;
    n.w.pools.set_in_use(9, 2);
    for (st = 1;; ++st) {
        is::shell::Rng t;
        t.set_state(st);
        if (t.rand() % 3 != 0) break;
    }
    n.rng.set_state(st);
    const is::shell::Rng b3 = n.rng;
    reactions(n.c, 1);
    CHECK(n.host.targets.empty());
    CHECK(draws(b3, n.rng) == 1);
}

TEST_CASE("seg13:0x9bb8: the ambulance out of the wave, the money, the lamps, once per wave") {
    Rig r;
    CHECK(ambulance_time(r.c, 1, 0) == 0);  // pool-7 record 0 in play
    r.o(7, 0).state = 1;
    r.p(1).credit(100);
    r.p(1).resources(100);  // m = 200 < 250 for a computer ship
    CHECK(ambulance_time(r.c, 1, 0) == 0);
    r.p(1).credit(200);     // m = 300; clamp(20·2 + 30, 70, 180) = 70; lamps 80·3 = 240
    CHECK(ambulance_time(r.c, 1, 0) == 0);
    r.s(1).w26 = 10;
    r.s(1).w28 = 10;
    r.s(1).w2a = 49;        // 69 < 70
    CHECK(ambulance_time(r.c, 1, 0) == 1);
    CHECK(r.cw.ambulance_wave[1] == 3);
    CHECK(ambulance_time(r.c, 1, 0) == 0);  // once per wave (DGROUP:0x1c8e + 2i = DGROUP:0x91f8)
    // A lamp at 0
    r.cw.ambulance_wave[1] = 0;
    r.s(1).w26 = 80;
    r.s(1).w28 = 0;
    CHECK(ambulance_time(r.c, 1, 0) == 1);
    // A wing gone with m > 500
    Rig q;
    q.o(7, 0).state = 1;
    q.p(1).credit(600);
    set_ship_b(q.s(1), 0xac, 1);
    set_ship_b(q.s(1), 0xad, 0);
    CHECK(ambulance_time(q.c, 1, 0) == 1);
    // The player and the ship hunting each other: only with +0x82 >= 1700 and Engine <= 1
    Rig h;
    h.o(7, 0).state = 1;
    h.p(1).credit(600);
    set_ship_b(h.s(1), 0xad, 0);
    h.p(1).goal_pool(14);
    h.p(1).goal_rec(0);
    CHECK(ambulance_time(h.c, 1, 0) == 0);
}

// ---- seg4:0x5e8e's liking branch reads the allies byte +0x5d, not the one-shot byte +0xed
// [BYTES seg4:0x64fa seg4:0x654e] (the fix of ship_react.cpp)
namespace {

class ReactCalls : public CollideHost {
public:
    std::vector<Msg> messages;
    std::vector<std::array<int, 3>> targets;
    std::vector<int> challenges;
    void message(int r, int p, int rec, int code) override { messages.push_back({r, p, rec, code}); }
    void seg3_f4(int s, int p, int r) override { targets.push_back({s, p, r}); }
    void seg13_915c(int s) override { challenges.push_back(s); }
};

struct ReactRig {
    World w;
    WaveGlobals g;
    CollideGlobals c;
    ReactCalls h;
    ViewState view;
    VisibilityGlobals vis;
    is::shell::Rng rng;
    std::int16_t dg_943a = 0, dg_943c = 0, dg_9454 = 0;
    CollideCtx x{w, g, c, h, view, &vis, rng, dg_943a, dg_943c, dg_9454};

    ReactRig() {
        contact_words() = ContactWords{};
        roster_words().team_table.fill(0);
        w.player = 0;
        w.last_ship = 1;
        w.player_game = 1;
        g.player = 0;
        g.last_ship = 1;
        g.now = 50000;
        g.demo = 1;
        w.pools.set_in_use(kPoolShips, 2);
        for (int i = 0; i < 2; ++i) {
            ShipRecord& s = w.ships[static_cast<std::size_t>(i)];
            s.design_index = static_cast<std::int16_t>(i);
            s.roster_index = -1;
            s.bec = 1;
            s.w26 = 80;
            s.kill_value = 300;
            s.fuel = 1000;
            w.design_ratings[static_cast<std::size_t>(i)].armor4a = 8;
            w.design_ratings[static_cast<std::size_t>(i)].tank = 1320;
            w.design_ratings[static_cast<std::size_t>(i)].team = static_cast<std::int16_t>(i);
        }
        // Ship 1 likes the player: A(1, 0) = 150 keeps the relation above 0 after the hit
        pilot(w.ships, 1).attitude(0, 150);
        pilot(w.ships, 1).grudge(0, 1);  // not a first hurt: the d < 33 return of step 2 is skipped
    }
    ~ReactRig() {
        roster_words().team_table.fill(0);
        contact_words() = ContactWords{};
    }
};

}  // namespace

TEST_CASE("seg4:0x5e8e: lim = cnt - sbyte +0x5d + 4, whatever the one-shot byte +0xed holds") {
    // cnt = 0 (another team, strength 100), grudge 1 + 1 = 2 at the test
    ReactRig r;
    set_ship_sb(r.w.ships[1], pw::kAllies, 2);   // lim = 0 - 2 + 4 = 2 >= 2: message 0x2a
    set_ship_sb(r.w.ships[1], pw::kOneShot, 9);  // the old read gave lim = -5: a turn
    attack_reaction(r.x, 1, 0, 10, 1);
    CHECK(r.h.messages == std::vector<Msg>{{0, 14, 1, 0x2a}});
    CHECK(r.h.challenges.empty());
    ReactRig q;
    set_ship_sb(q.w.ships[1], pw::kAllies, 3);   // lim = 1 < 2: ship 1 turns on the player
    set_ship_sb(q.w.ships[1], pw::kOneShot, 0);
    attack_reaction(q.x, 1, 0, 10, 1);
    CHECK(q.h.messages.empty());
    CHECK(q.h.targets == std::vector<std::array<int, 3>>{{1, 14, 0}});
    CHECK(q.h.challenges == std::vector<int>{1});
}
