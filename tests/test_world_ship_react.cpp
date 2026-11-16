// A ship's reaction to a hit and a ship's death (src/world/ship_react.h): seg4:0x5e8e,
// seg4:0x7636, seg8:0x7ce0 and the wing taunts of seg8:0x2cd8, with D1 (seg8:0x45e0's first
// argument to seg4:0x5e8e) and D5 (pool 7's on_damage), and the message display's texts of the
// codes these raise (src/hud/messages.h) (M5 stage 3 part B, track CONTACT). The numbers are
// worked by hand from the listings (work/world/m5s3/contact-report.md).
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "doctest.h"
#include "hud/messages.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/damage.h"
#include "world/pilot.h"
#include "world/relations.h"
#include "world/roster_build.h"
#include "world/ship_react.h"

using namespace is::world;

namespace {

using Msg = std::tuple<int, int, int, int>;

class Calls : public CollideHost {
public:
    std::vector<Msg> messages;
    std::vector<std::pair<int, int>> crimes;
    std::vector<std::tuple<int, int, int>> sounds;
    std::vector<std::uint32_t> holds;
    std::vector<std::tuple<int, int, int>> targets;
    int other = 0;
    ShotSlotView slot;
    void message(int r, int p, int rec, int code) override { messages.emplace_back(r, p, rec, code); }
    void sound(int id, int pool, int rec, int) override { sounds.emplace_back(id, pool, rec); }
    void seg13_b6f2(int ship, int crime) override { crimes.emplace_back(ship, crime); }
    void music_hold(std::uint32_t t) override { holds.push_back(t); }
    void seg3_f4(int s, int p, int r) override { targets.emplace_back(s, p, r); }
    std::int16_t other_damage(std::uint32_t, int, int, std::int16_t) override { return static_cast<std::int16_t>(++other); }
    ShotSlotView shot(int) override { return slot; }
};

// Three ships: the player 0 (team 0), 1 (team 1) and 2 (team 2), ship i of design i, in play,
// alive, engine 80; the design teams in World::design_ratings, the program's team table
// (roster_build.h roster_words()) zeroed.
struct Rig {
    World w;
    WaveGlobals g;
    CollideGlobals c;
    Calls h;
    ViewState view;
    VisibilityGlobals vis;
    is::shell::Rng rng;
    std::int16_t dg_943a = 0, dg_943c = 0, dg_9454 = 0;
    CollideCtx x{w, g, c, h, view, &vis, rng, dg_943a, dg_943c, dg_9454};

    Rig() {
        contact_words() = ContactWords{};
        w.player = 0;
        w.last_ship = 2;
        w.kind = 0;
        w.player_game = 1;
        g.player = 0;
        g.last_ship = 2;
        g.now = 50000;
        g.play_mode = 0;
        g.demo = 1;
        w.pools.set_in_use(kPoolShips, 3);
        for (int i = 0; i < 3; ++i) {
            ShipRecord& s = w.ships[static_cast<std::size_t>(i)];
            s.design_index = static_cast<std::int16_t>(i);
            s.roster_index = -1;
            s.bec = 1;
            s.w26 = 80;
            s.kill_value = 300;
            s.fuel = 1000;
            set_ship_b(s, pw::kAllies, 3);
            set_ship_b(s, pw::kClass, 3);
            w.pools.rec(kPoolShips, i).state = 0;
            w.design_ratings[static_cast<std::size_t>(i)].armor4a = 8;
            w.design_ratings[static_cast<std::size_t>(i)].tank = 1320;
            set_team(i, static_cast<std::int16_t>(i));
        }
        roster_words().team_table.fill(0);
    }
    ~Rig() {
        roster_words().team_table.fill(0);
        contact_words() = ContactWords{};
    }
    void set_team(int i, std::int16_t t) { w.design_ratings[static_cast<std::size_t>(i)].team = t; }
    ShipRecord& s(int i) { return w.ships[static_cast<std::size_t>(i)]; }
    std::int16_t A(int a, int b) { return pilot(s(a)).attitude(b); }
    std::int16_t& T(int x0, int y0) { return roster_words().team_table[static_cast<std::size_t>(8 * x0 + y0)]; }
    void set_hurt(int ship, int j, int v) { set_ship_b(s(ship), 0xf7 + 9 * j, static_cast<std::uint8_t>(v)); }
};

}  // namespace

// ---- seg4:0x5e8e

TEST_CASE("seg4:0x5e8e: kind 2 by a computer ship, the attitudes, the weight and message 0x2d") {
    Rig r;  // v = 1, a = 2; the rivals' 90 becomes 50 [BYTES seg4:0x5e9c-0x5ea2]
    attack_reaction(r.x, 1, 2, 90, 2);
    // seg13:0x2714(1, 2, -16, -58, 0, -1), then seg13:0x2714(1, 2, -32, -60, 1, 2): the team
    // pair -16 then -16 (half: T·dt > 0), the ship pair -38 then -40, the other way -14 then -15
    CHECK(r.T(1, 2) == -32);
    CHECK(r.T(2, 1) == -10);
    CHECK(r.A(1, 2) == -78);
    CHECK(r.A(2, 1) == -29);
    // seg13:0x2e60(1, 2, 2) of the second call: D[2] = 4 on the hurt byte +0xf6 + 9·2 + 2
    CHECK(ship_sb(r.s(1), 0xf6 + 18 + 2) == 4);
    // relation -110 <= 0: 3 + 0 + 1 >= grudge 1: message 0x2d from (14, 1) to 2
    REQUIRE(r.h.messages.size() == 1);
    CHECK(r.h.messages[0] == Msg{2, 14, 1, 0x2d});
    // grudge 0 + 1, then the weight: d 50 > 24, > 36, relation < -100 (engine 80 - 50/8 = 74)
    CHECK(pilot(r.s(1)).grudge(2) == 4);
    CHECK(r.rng.state() == 1u);  // no rand() on this path
}

TEST_CASE("seg4:0x5e8e: a fleeing ship, its one rand() and crime 3, or message 0x5c") {
    Rig r;
    r.s(1).w38 = 4;  // v flees [BYTES seg4:0x5f7b-0x5f85]
    r.s(1).w26 = 30;
    attack_reaction(r.x, 1, 2, 20, 2);
    // rand() with the state 1: 41, % 4 = 1 <= 1, engine 30 < 40 [BYTES seg4:0x5fbc-0x5fd8]
    CHECK(r.rng.state() == 2745024u);
    REQUIRE(r.h.crimes.size() == 1);
    CHECK(r.h.crimes[0] == std::make_pair(2, 3));
    CHECK(r.h.messages.empty());
    CHECK(pilot(r.s(1)).grudge(2) == 0);  // this branch returns before the grudge step
    // Engine 50: no crime, the first Enforcer j < DGROUP:0x9316 sends 0x5c
    Rig q;
    q.set_team(1, 7);  // ship 1 is an Enforcer; j runs 0 ... 1
    q.s(2).w38 = 4;
    q.s(2).w26 = 50;
    attack_reaction(q.x, 2, 0, 20, 2);
    CHECK(q.rng.state() == 2745024u);
    CHECK(q.h.crimes.empty());
    REQUIRE(q.h.messages.size() == 1);
    CHECK(q.h.messages[0] == Msg{0, 14, 1, 0x5c});
    // d <= 10 draws nothing
    Rig z;
    z.s(1).w38 = 4;
    attack_reaction(z.x, 1, 2, 10, 2);
    CHECK(z.rng.state() == 1u);
    // T(7, team v) <= -100 skips the law and the message
    Rig t;
    t.s(1).w38 = 4;
    t.s(1).w26 = 30;
    t.T(7, 1) = -100;
    attack_reaction(t.x, 1, 2, 20, 2);
    CHECK(t.rng.state() == 1u);
    CHECK(t.h.crimes.empty());
}

TEST_CASE("seg4:0x5e8e: the player's light kind-1 hit on a ship it does not hunt returns early") {
    Rig r;
    attack_reaction(r.x, 1, 0, 20, 1);  // d < 33 [BYTES seg4:0x5f08-0x5f0c]
    CHECK(pilot(r.s(1)).grudge(0) == 0);
    CHECK(r.A(1, 0) != 0);  // the first seg13:0x2714 ran before the test
    // Hit stamp +0x46 within 80 tenth-frames: the reaction goes on
    Rig q;
    q.w.frames = 10000;                  // frame# / 10 = 1000
    set_ship_w(q.s(1), 0x46, 950);
    q.g.play_mode = 1;                   // (v is not the player either way)
    attack_reaction(q.x, 1, 0, 40, 1);
    CHECK(pilot(q.s(1)).grudge(0) >= 1);
    // A stamp 81 back returns [BYTES seg4:0x5f36-0x5f39]
    Rig z;
    z.w.frames = 10000;
    set_ship_w(z.s(1), 0x46, 919);
    attack_reaction(z.x, 1, 0, 40, 1);
    CHECK(pilot(z.s(1)).grudge(0) == 0);
}

TEST_CASE("seg4:0x5e8e: the low-fuel plea 0xbe, the grudge set to 4") {
    Rig r;
    r.s(1).fuel = 100;  // tank 1320 / 10 = 132 > 100 [BYTES seg4:0x613a-0x615b]
    attack_reaction(r.x, 1, 0, 40, 2);
    REQUIRE(r.h.messages.size() == 1);
    CHECK(r.h.messages[0] == Msg{0, 14, 1, 0xbe});
    // seg10:0x744c(0x3d): the reset 0x69, then 0x3d
    REQUIRE(r.h.sounds.size() == 2);
    CHECK(std::get<0>(r.h.sounds[1]) == 0x3d);
    CHECK(pilot(r.s(1)).grudge(0) == 4);
    CHECK(r.h.crimes.empty());
    // At grudge 4 the law instead of the sound
    attack_reaction(r.x, 1, 0, 40, 2);
    REQUIRE(r.h.crimes.size() == 1);
    CHECK(r.h.crimes[0] == std::make_pair(0, 3));
}

TEST_CASE("seg4:0x5e8e: a Patrol Enforcer warns: order 0x15 on the attacker, the crime words") {
    Rig r;
    r.set_team(1, 7);  // v = 1 a Patrol (class 3 > 1), a = 2 of team 2
    attack_reaction(r.x, 1, 2, 40, 2);
    REQUIRE(!r.h.messages.empty());
    CHECK(r.h.messages.back() == Msg{2, 14, 1, 0x3b});  // no Interceptor in play
    CHECK(r.s(1).w52 == 0x15);
    CHECK(r.s(1).w54 == 14);
    CHECK(r.s(1).w56 == 2);
    CHECK(ship_sb(r.s(1), pw::kOneShot) == 1);
    CHECK(ship_d(r.s(1), pw::kReplanAt) == 50000u);
    CHECK(r.w.w964a == 1);
    CHECK(contact_words().w9648 == 3);
}

TEST_CASE("seg8:0x45e0 passes the damaged ship to seg4:0x5e8e (D1); pool 7 takes nothing (D5)") {
    Rig r;
    r.dg_943a = 0x1e;  // a gun shot, slot 0 owned by ship 2
    r.dg_943c = 0;
    r.h.slot.owner_pool = 14;
    r.h.slot.owner_rec = 2;
    ObjectRecord& o = r.w.pools.rec(kPoolShips, 1);
    o.on_damage.set(kDamageShip);
    o.size = 33;
    ship_damage(r.x, 14, 1, 40);
    // seg4:0x5e8e(1, 2, 35, 1): its first seg13:0x2714(1, 2, -12, -43, 0, -1) moves A(1, 2)
    CHECK(r.A(1, 2) < 0);
    // D5: seg16:0x85e returns 0 and calls nothing [BYTES seg16:0x868]
    r.w.pools.set_in_use(7, 1);
    r.w.pools.rec(7, 0).on_damage.set(kDamagePool7);
    CHECK(call_on_damage(r.x, 7, 0, 50) == 0);
    CHECK(r.h.other == 0);
}

// ---- seg4:0x7636

TEST_CASE("seg4:0x7636: the player's kill: the killer words, the tally, line 0x5f or 0x63") {
    Rig r;
    ship_killed(r.x, 1, 14, 0, 0);
    REQUIRE(r.h.holds.size() == 1);
    CHECK(r.h.holds[0] == 55000u);  // DGROUP:0x2384 = now + 5000
    CHECK(r.s(1).w06 == 14);
    CHECK(r.s(1).w08 == 0);
    CHECK(kill_tally(contact_words(), 1) == 1);  // DGROUP:0x6b40 + 2·team
    REQUIRE(r.h.messages.size() == 1);
    CHECK(r.h.messages[0] == Msg{0, 14, 1, 0x5f});
    Rig q;
    q.set_team(1, 0);  // the player's team
    ship_killed(q.x, 1, 14, 0, 0);
    CHECK(q.h.messages.at(0) == Msg{0, 14, 1, 0x63});
    Rig k;  // an escorted kill (flag 1): no line
    ship_killed(k.x, 1, 14, 0, 1);
    CHECK(k.h.messages.empty());
}

TEST_CASE("seg4:0x7636: another ship's kill 0x60 / 0x61, a rock's 0x62, a ship's own 0x89") {
    Rig r;
    ship_killed(r.x, 1, 14, 2, 0);
    CHECK(r.s(1).w08 == 2);
    CHECK(r.h.messages.at(0) == Msg{0, 14, 1, 0x61});
    CHECK(kill_tally(contact_words(), 1) == 0);
    Rig q;
    q.set_team(1, 0);
    ship_killed(q.x, 1, 14, 2, 0);
    CHECK(q.h.messages.at(0) == Msg{0, 14, 1, 0x60});
    Rig k;  // a rock (3, 5): cp != 14
    ship_killed(k.x, 1, 3, 5, 0);
    CHECK(k.s(1).w06 == 3);
    CHECK(k.s(1).w08 == 5);
    CHECK(k.h.messages.at(0) == Msg{0, 14, 1, 0x62});
    Rig s;
    ship_killed(s.x, 1, 14, 1, 0);
    CHECK(s.h.messages.at(0) == Msg{0, 14, 1, 0x89});
    Rig p;  // the player's death: no line, no killer words for cp != 14
    ship_killed(p.x, 0, 3, 5, 0);
    CHECK(p.h.messages.empty());
    CHECK(p.s(0).w06 == 0);
    Rig e;  // killed by an Enforcer: the words only [BYTES seg4:0x7669-0x768f]
    e.set_team(2, 7);
    ship_killed(e.x, 1, 14, 2, 0);
    CHECK(e.s(1).w06 == 14);
    CHECK(e.s(1).w08 == 2);
    CHECK(e.h.messages.empty());
}

TEST_CASE("seg4:0x7636: the credit moves to the ship that hurt the dead one most") {
    Rig r;
    r.set_hurt(1, 2, 9);  // 9 > 7 [BYTES seg4:0x76b6-0x771a]
    ship_killed(r.x, 1, 3, 5, 0);
    CHECK(r.s(1).w08 == 2);
    CHECK(r.h.messages.at(0) == Msg{0, 14, 1, 0x61});
    Rig q;  // the goal ship with a hurt byte > 3 gets the credit, and then +5
    q.s(1).w32 = 14, q.s(1).w34 = 2;
    q.set_hurt(1, 2, 4);
    q.set_hurt(1, 0, 8);  // 8 > 7 but 4 + 5 = 9 wins
    ship_killed(q.x, 1, 3, 5, 0);
    CHECK(q.s(1).w08 == 2);
    Rig t;  // a tie goes to the first
    t.set_hurt(1, 0, 9);
    t.set_hurt(1, 2, 9);
    ship_killed(t.x, 1, 3, 5, 0);
    CHECK(t.s(1).w08 == 0);
    CHECK(t.h.messages.at(0) == Msg{0, 14, 1, 0x5f});
}

TEST_CASE("seg4:0x7636: the revenge slot, and a full team 7 overrunning into the kill tally") {
    Rig r;
    r.w.w4eec.fill(-1);
    r.w.wave = 4;
    r.w.roster.assign(2, RosterRecord{});
    r.w.roster[0].shp = 33;
    r.s(1).roster_index = 0;
    r.s(2).roster_index = 1;
    r.s(1).w38 = 4;  // fleeing: a revenge slot [BYTES seg4:0x77a1-0x77b5]
    ship_killed(r.x, 1, 14, 2, 0);
    CHECK(r.w.w4eec[3] == 1);  // team 1, slot 0: the killer's roster index
    CHECK(contact_words().w4fd0[3] == 4);
    CHECK(contact_words().w6b10[3] == 33);
    // Team 7, slots full: slot 0's DGROUP:0x4eec word, the other two at index 24
    Rig q;
    q.set_team(1, 7);
    q.w.w4eec.fill(-1);
    q.w.w4eec[21] = 5, q.w.w4eec[22] = 6, q.w.w4eec[23] = 7;
    q.w.wave = 9;
    q.w.roster.assign(2, RosterRecord{});
    q.w.roster[0].shp = 41;
    q.s(1).roster_index = 0;
    q.s(2).roster_index = 1;
    q.s(1).w38 = 4;
    set_ship_b(q.s(1), pw::kClass, 0);  // a Terminator: the crime bytes are copied
    q.w.ba14d[3] = 2;
    ship_killed(q.x, 1, 14, 2, 0);
    CHECK(q.w.w4eec[21] == 1);
    CHECK(q.w.w4eec[22] == 6);
    CHECK(contact_words().w4fd0[24] == 9);   // DGROUP:0x5000
    CHECK(kill_tally(contact_words(), 0) == 41);  // DGROUP:0x6b40, team 0's tally
    CHECK(q.w.ba157[3] == 2);
    // No slot outside a revenge
    Rig n;
    n.w.w4eec.fill(-1);
    ship_killed(n.x, 1, 14, 2, 0);
    CHECK(n.w.w4eec[3] == -1);
}

// ---- seg8:0x7ce0

TEST_CASE("seg8:0x7ce0: three of the player's hits in the last 20 tenth-frames score the kill") {
    Rig r;
    r.w.frames = 1000;  // frame# / 10 = 100
    for (int k = 0; k < 3; ++k) {
        r.w.hit_pool[static_cast<std::size_t>(k)] = 3;
        r.w.hit_rec[static_cast<std::size_t>(k)] = 5;
        r.w.w937a[static_cast<std::size_t>(k)] = 95;
    }
    assist_score(r.x, 3, 5, 10, 1);  // 9 > 6: seg8:0x7be8(3, 5, 10, 1)
    CHECK(r.w.score == static_cast<std::uint32_t>(kill_points(3)));
    Rig q;  // two: 6, not above 6
    q.w.frames = 1000;
    for (int k = 0; k < 2; ++k) {
        q.w.hit_pool[static_cast<std::size_t>(k)] = 3;
        q.w.hit_rec[static_cast<std::size_t>(k)] = 5;
        q.w.w937a[static_cast<std::size_t>(k)] = 81;
    }
    assist_score(q.x, 3, 5, 10, 1);
    CHECK(q.w.score == 0u);
    // A zero word counts 1 (and is 100 back: no 3): 6 + 1 = 7
    q.w.hit_pool[2] = 3, q.w.hit_rec[2] = 5, q.w.w937a[2] = 0;
    assist_score(q.x, 3, 5, 10, 1);
    CHECK(q.w.score == static_cast<std::uint32_t>(kill_points(3)));
    // 20 back is not below 20
    Rig z;
    z.w.frames = 1000;
    for (int k = 0; k < 8; ++k) {
        z.w.hit_pool[static_cast<std::size_t>(k)] = 3;
        z.w.hit_rec[static_cast<std::size_t>(k)] = 5;
        z.w.w937a[static_cast<std::size_t>(k)] = 80;
    }
    assist_score(z.x, 3, 5, 10, 1);
    CHECK(z.w.score == 0u);
}

// ---- seg8:0x2d96-0x2e5b

TEST_CASE("seg8:0x2cd8's taunts: 0x1b, 0x1e, 0x1c / 0x19, 0x1d / 0x1a") {
    const auto taunt = [](int sac, int sad, int jac, int jad, int ship_k) {
        Rig r;
        r.s(0).w32 = 14, r.s(0).w34 = 1, r.s(0).w38 = 1;  // the player hunts ship 1
        set_ship_b(r.s(1), 0xac, static_cast<std::uint8_t>(sac));
        set_ship_b(r.s(1), 0xad, static_cast<std::uint8_t>(sad));
        set_ship_b(r.s(0), 0xac, static_cast<std::uint8_t>(jac));
        set_ship_b(r.s(0), 0xad, static_cast<std::uint8_t>(jad));
        r.s(1).kill_value = static_cast<std::int16_t>(ship_k);
        wing_taunts(r.x, 1);
        REQUIRE(r.h.messages.size() == 1);
        CHECK(std::get<0>(r.h.messages[0]) == 0);
        CHECK(std::get<1>(r.h.messages[0]) == 14);
        CHECK(std::get<2>(r.h.messages[0]) == 1);
        return std::get<3>(r.h.messages[0]);
    };
    CHECK(taunt(0, 0, 0, 0, 300) == 0x1b);
    CHECK(taunt(1, 0, 1, 0, 300) == 0x1b);
    CHECK(taunt(1, 0, 0, 0, 300) == 0x1e);
    CHECK(taunt(1, 0, 1, 1, 300) == 0x1c);  // strength 100 > 95
    CHECK(taunt(0, 1, 1, 1, 100) == 0x19);  // 8800 / 266 = 33
    CHECK(taunt(0, 0, 1, 1, 300) == 0x1d);
    CHECK(taunt(0, 0, 0, 1, 100) == 0x1a);
    // No alive test, and only ships with the torn ship as attack goal
    Rig r;
    r.s(0).w32 = 14, r.s(0).w34 = 1, r.s(0).w38 = 1;
    r.s(0).bec = 0;
    r.s(2).w32 = 14, r.s(2).w34 = 1, r.s(2).w38 = 2;  // capturing, not attacking
    wing_taunts(r.x, 1);
    CHECK(r.h.messages.size() == 1);
}

// ---- the message display's texts (seg3:0x6ec)

namespace {

const std::map<int, std::string> kStrings = {
    // [BYTES ENG_LANG.DLL STRING 24] ids 373-383
    {373, "Oohh, That Smarts. You Owe Me A Wing!!"},
    {375, "There Goes The Other One. Must Be Wing Night Tonight."},
    {377, "You Ripped Off My Wing And I'm Still Tougher!!"},
    {379, "I May Have No Wings, But You're Still Weaker!"},
    {381, "I Still Have More Wings, You Lousy %D"},
    {383, "Would You Look At That.. Now Our Wings Match!"},
    // [BYTES ENG_LANG.DLL STRING 36] ids 570, 572 [BYTES ENG_LANG.DLL STRING 45] id 707
    {570, "Stop That! You Just Broke a Good Icon!"},
    {707, "We Do Have Laws Here. Do I Have To Send The Patrol?"},
    // [BYTES ENG_LANG.DLL STRING 66] ids 1050-1054 [BYTES ENG_LANG.DLL STRING 67] id 1058
    // [BYTES ENG_LANG.DLL STRING 90] ids 1425, 1426
    {1050, "This Ship Is A Write Off. I Hope My Allies Get You!"},
    {1052, "Flame Out..Arrrgghh! Get %D!"},
    {1054, "I'm Outta Here, For Good. I Was Killed By %D!"},
    {1058, "I Can't Believe You Did That. I'm Your Ally!"},
    {1425, "Silly Me. I Killed Myself."},
    {1426, "That's Enough, You Win. I'm Breaking Off Attack."},
    // [BYTES ENG_LANG.DLL STRING 25] id 392 [BYTES ENG_LANG.DLL STRING 26] ids 411-414
    // [BYTES ENG_LANG.DLL STRING 35] ids 550, 556, 558 [BYTES ENG_LANG.DLL STRING 41] ids 654, 655
    // [BYTES ENG_LANG.DLL STRING 42] id 665 [BYTES ENG_LANG.DLL STRING 60] id 950
    // [BYTES ENG_LANG.DLL STRING 126] id 2005
    {392, "I've Captured %D. Now To Other Business.."},
    {411, "Hey Buddy, What's up? Why Don't You Chill Out?"},
    {412, "My Patience Is Running Out. Stop It Or I'll Get You!"},
    {413, "If You Insist On A Fight, Let's Get It On!"},
    {414, "I've Had Enough Dancing, You Gonna Fight Or What?"},
    {415, "I Mean It, Get Out Of Here Before I Get Really Mad!"},
    {550, "Do Not Provoke Me! Or There Will Be Trouble."},
    {556, "Looking For Trouble? Let Me Do My Job!"},
    {558, "You Know, A Guy Could Get Arrested For That!"},
    {654, "Hey, I Wanted That %D! "},
    {655, "You Ordered Him To Take What Was Mine!"},
    {665, "He Deserved That, Nobody Messes With Me."},
    {950, "You're Not Supposed To Bully. Now Stop It!"},
    {2005, "Stop, I Can't Fight You. I'm Out Of Gas!"},
};

class TextHost : public is::hud::MessageHost {
public:
    std::vector<int> sounds, todos;
    std::map<int, std::pair<int, int>> killer;  // ship -> (+0x06, +0x08)
    int width() const override { return 600; }  // no upper-casing of the lines
    std::string lang(int id) override {
        auto it = kStrings.find(id);
        return it == kStrings.end() ? std::string{} : it->second;
    }
    std::string name(int pool, int rec) override {
        if (pool == 14) return rec == 0 ? "Player" : "Ship " + std::to_string(rec);
        if (pool == 3) return "Rock";
        return "?";
    }
    std::map<std::pair<int, int>, int> words;  // (ship, offset) -> word
    int ship_word(int rec, int off) const override {
        if (off == 0x06 && killer.count(rec)) return killer.at(rec).first;
        if (off == 0x08 && killer.count(rec)) return killer.at(rec).second;
        auto it = words.find({rec, off});
        return it == words.end() ? -1 : it->second;
    }
    void sound(int id) override { sounds.push_back(id); }
    void todo(int code) override { todos.push_back(code); }
    int next_rand = 0, rands = 0;
    int rand() override { return ++rands, next_rand; }
};

std::string show(TextHost& h, int pool, int rec, int code) {
    is::hud::MessageState s;
    is::hud::dispatch_message(s, h, 0, pool, rec, code);
    return s.text;
}

}  // namespace

TEST_CASE("seg3:0x6ec: the wing taunts' and the death lines' texts, sounds and %D") {
    TextHost h;
    CHECK(show(h, 14, 1, 0x19) == "Oohh, That Smarts. You Owe Me A Wing!!");
    CHECK(h.sounds == std::vector<int>{0x3f});
    CHECK(show(h, 14, 1, 0x1a) == "There Goes The Other One. Must Be Wing Night Tonight.");
    CHECK(show(h, 14, 1, 0x1b) == "Would You Look At That.. Now Our Wings Match!");
    CHECK(show(h, 14, 1, 0x1c) == "You Ripped Off My Wing And I'm Still Tougher!!");
    CHECK(show(h, 14, 1, 0x1d) == "I May Have No Wings, But You're Still Weaker!");
    CHECK(h.sounds == std::vector<int>{0x3f, 0x3f});
    // %D = (14, the player, 0): upper-cased
    CHECK(show(h, 14, 1, 0x1e) == "I Still Have More Wings, You Lousy PLAYER");
    CHECK(show(h, 14, 1, 0x5f) == "This Ship Is A Write Off. I Hope My Allies Get You!");
    h.killer[1] = {14, 2};
    CHECK(show(h, 14, 1, 0x60) == "Flame Out..Arrrgghh! Get Ship 2!");  // code 5: as named
    CHECK(show(h, 14, 1, 0x61) == "I'm Outta Here, For Good. I Was Killed By Ship 2!");
    h.killer[1] = {3, 5};
    CHECK(show(h, 14, 1, 0x62) == "I'm Outta Here, For Good. I Was Killed By ROCK!");  // code 2
    CHECK(show(h, 14, 1, 0x63) == "I Can't Believe You Did That. I'm Your Ally!");
    CHECK(show(h, 14, 1, 0x89) == "Silly Me. I Killed Myself.");
    h.sounds.clear();
    CHECK(show(h, 14, 1, 0x8a) == "That's Enough, You Win. I'm Breaking Off Attack.");
    CHECK(h.sounds == std::vector<int>{0x44});
    CHECK(show(h, 14, 1, 0x45) == "Stop That! You Just Broke a Good Icon!");
    h.sounds.clear();
    CHECK(show(h, 7, 0, 0x6e) == "We Do Have Laws Here. Do I Have To Send The Patrol?");
    CHECK(h.sounds == std::vector<int>{0x30});
    CHECK(h.todos.empty());
    for (int code : {0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x89, 0x8a})
        CHECK(is::hud::message_code_ported(code));
    // A message to another ship is not shown [BYTES seg3:0x6fa-0x700]
    is::hud::MessageState s;
    is::hud::dispatch_message(s, h, 2, 14, 1, 0x5f);
    CHECK(s.text.empty());
}

TEST_CASE("seg3:0x6ec: the attack reaction's, the attitudes' and the rivals' lines") {
    TextHost h;
    CHECK(show(h, 14, 1, 0x29) == "Hey Buddy, What's up? Why Don't You Chill Out?");  // code + 0x172
    CHECK(show(h, 14, 1, 0x2a) == "My Patience Is Running Out. Stop It Or I'll Get You!");
    CHECK(show(h, 14, 1, 0x2b) == "If You Insist On A Fight, Let's Get It On!");
    CHECK(show(h, 14, 1, 0x2d) == "I've Had Enough Dancing, You Gonna Fight Or What?");  // code + 0x171
    // 0x2e and 0x2f are the empty tail: the lines stay as they were [BYTES seg3:0x1356]
    {
        is::hud::MessageState s;
        is::hud::dispatch_message(s, h, 0, 14, 1, 0x2b);
        is::hud::dispatch_message(s, h, 0, 14, 1, 0x2e);
        CHECK(s.line1 + s.line2 == "If You Insist On A Fight, Let's Get It On!");
        is::hud::dispatch_message(s, h, 0, 14, 1, 0x2f);
        CHECK(s.line1 + s.line2 == "If You Insist On A Fight, Let's Get It On!");
    }
    // seg3:0xf61: rand() % 4 picks the sound: 0 -> 0x3d, 1 -> 0x3e, 2 -> 0x3f, 3 none
    // [BYTES seg3:0xf61-0xf93]
    for (int r = 0; r < 4; ++r) {
        h.next_rand = 400 + r;
        h.sounds.clear();
        CHECK(show(h, 14, 1, 0x3b) == "Do Not Provoke Me! Or There Will Be Trouble.");
        CHECK(h.rands == r + 1);
        CHECK(h.sounds == (r == 3 ? std::vector<int>{} : std::vector<int>{0x3d + r}));
    }
    CHECK(show(h, 14, 1, 0x41) == "Looking For Trouble? Let Me Do My Job!");
    CHECK(show(h, 14, 1, 0x43) == "You Know, A Guy Could Get Arrested For That!");
    CHECK(show(h, 14, 1, 0x44).empty());  // string 559 is not in the table
    CHECK(show(h, 14, 1, 0x5c) == "You're Not Supposed To Bully. Now Stop It!");
    CHECK(show(h, 14, 1, 0xbe) == "Stop, I Can't Fight You. I'm Out Of Gas!");
    h.sounds.clear();
    CHECK(show(h, 14, 1, 0x51) == "He Deserved That, Nobody Messes With Me.");
    CHECK(h.sounds == std::vector<int>{0x3b});
    // 0x49: %D of the sender's goal (+0x32, +0x34) with code 0, upper-cased
    h.words[{1, 0x32}] = 3;
    h.words[{1, 0x34}] = 5;
    CHECK(show(h, 14, 1, 0x49) == "Hey, I Wanted That ROCK! ");
    CHECK(show(h, 14, 1, 0x4a) == "You Ordered Him To Take What Was Mine!");
    // 0x20: %D of the sender's order words (+0x54, +0x56) with code 5, as named
    h.words[{1, 0x54}] = 14;
    h.words[{1, 0x56}] = 2;
    CHECK(show(h, 14, 1, 0x20) == "I've Captured Ship 2. Now To Other Business..");
    CHECK(h.todos.empty());
}
