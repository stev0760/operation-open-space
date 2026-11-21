// The commands to ships (src/world/commands.h) against numbers worked by hand from the listings
// (work/analysis/m5/s4-commands.md sections 0, 1, 1.9, 1.11, 5 with s4-commands-verify.md) and
// against oracle/digests/ships-0005.md: each recorded key with its answer and the commanded
// ship's order words afterwards. Ships: 0 the player (SKY TALON), 1 the Teammate GOLDEN VIPER
// (team 0, the player's), 2 the Platypus (FUZZY ONES, team 2), 3 a Pirate (team 3).
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/commands.h"
#include "world/flight.h"
#include "world/pilot.h"
#include "world/pilot_think.h"
#include "world/relations.h"
#include "world/world.h"

using namespace is::world;

namespace {

struct Msg {
    int to, pool, rec, code;
    bool operator==(const Msg& o) const { return to == o.to && pool == o.pool && rec == o.rec && code == o.code; }
};

class Host : public PilotHost {
public:
    std::array<std::int16_t, 8> team{}, tank{};  // by design index
    std::array<std::array<std::int16_t, 8>, 8> rel{};
    std::int16_t str = 100;
    std::vector<Msg> msgs;
    std::vector<std::array<int, 3>> targets, calls;
    std::vector<std::array<int, 4>> sounds;
    std::int16_t mark_pool = -1, mark_rec = -1;
    int manual = 0;
    std::int16_t design_word(int n, int off) override {
        if (n < 0 || n >= 8) return 0;
        if (off == 0x38) return team[static_cast<std::size_t>(n)];
        if (off == 0x1e) return tank[static_cast<std::size_t>(n)];
        return 0;
    }
    std::int16_t relation(int a, int b) override {
        if (a < 0 || a >= 8 || b < 0 || b >= 8) return 0;
        return rel[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)];
    }
    std::int16_t strength(int, int) override { return str; }
    void message(int to, int pool, int rec, int code) override { msgs.push_back({to, pool, rec, code}); }
    void target(int s, int p, int r) override {
        targets.push_back({s, p, r});
        if (s >= 0 && s < 8) {
            ships->at(static_cast<std::size_t>(s)).target_pool = static_cast<std::int16_t>(p);
            ships->at(static_cast<std::size_t>(s)).target_rec = static_cast<std::int16_t>(r);
        }
    }
    std::int16_t call_in(int s, int k, int m) override {
        calls.push_back({s, k, m});
        return 0;
    }
    void send_defenders(int, int, int) override {}
    void sound(int id, int p, int r, int m) override { sounds.push_back({id, p, r, m}); }
    void set_manual_target() override { ++manual; }
    void mark_6e52(std::int16_t& p, std::int16_t& r) override {
        p = mark_pool;
        r = mark_rec;
    }
    ShipTable* ships = nullptr;
};

class CHost : public CommandHost {
public:
    std::vector<std::array<int, 2>> counts;
    std::vector<std::array<int, 6>> attitudes;
    std::vector<int> notices;
    std::vector<std::array<int, 2>> pods;
    std::vector<DefenderEntry> table;
    void capture_count(int s, int v) override { counts.push_back({s, v}); }
    void attitude_change(World&, int a, int b, int dt, int ds, int prop, int kind) override {
        attitudes.push_back({a, b, dt, ds, prop, kind});
    }
    void order_notice(int n) override { notices.push_back(n); }
    std::vector<DefenderEntry>* defenders() override { return &table; }
    void drop_pod(int s, int f) override { pods.push_back({s, f}); }
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
        host.ships = &w.ships;
        g.now = 100000;
        g.player = 0;
        g.last_ship = 3;
        g.demo = 1;       // a player game
        g.play_mode = 0;  // Action mode
        g.detail = 3;
        g.gates = 1;
        w.last_ship = 3;
        w.w934e = 30;
        for (int i = 0; i < kShipCount; ++i) {
            s(i).design_index = static_cast<std::int16_t>(i);
            s(i).bec = 1;  // alive +0xec
            s(i).w26 = 80;
            s(i).w28 = 80;
            s(i).w2a = 80;
            s(i).fuel = 500;
        }
        for (int p = 0; p < kPoolCount; ++p)
            if (w.pools.exists(p)) w.pools.set_in_use(p, 0);
        w.pools.set_in_use(kPoolShips, 4);
        for (int i = 4; i < 10; ++i) o(kPoolShips, i).state = 1;
        host.team = {0, 0, 2, 3, 1, 1, 1, 1};
        host.tank = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
        set_ship_sb(s(1), pw::kAllies, 2);
        set_ship_sb(s(2), pw::kAllies, 1);
        set_ship_sb(s(3), pw::kAllies, 0);
    }
    ShipRecord& s(int i) { return w.ships[static_cast<std::size_t>(i)]; }
    Pilot p(int i) { return pilot(w.ships, i); }
    ObjectRecord& o(int pool, int rec) { return w.pools.rec(pool, rec); }
    void target(int i, int pool, int rec) {
        s(i).target_pool = static_cast<std::int16_t>(pool);
        s(i).target_rec = static_cast<std::int16_t>(rec);
    }
};

int draws(is::shell::Rng before, const is::shell::Rng& after) {
    for (int n = 0; n <= 64; ++n) {
        if (before.state() == after.state()) return n;
        before.rand();
    }
    return -1;
}

}  // namespace

TEST_CASE("seg13:0x81c2 Ctrl+H: the teammate takes order 1 for the player (ships-0005 Ctrl-H)") {
    Rig r;
    r.host.rel[0][1] = 100;  // the player's view of the Viper
    r.host.rel[0][2] = -200;
    const is::shell::Rng before = r.rng;
    // ships-0005: ship 1 words (+0x52, +0x54, +0x56) = (0, 0, 1) before the key
    r.p(1).order_rec(1);
    CHECK(help_command(r.c, 0) == 1);
    CHECK(r.p(1).order() == 1);
    CHECK(r.p(1).order_pool() == 14);
    CHECK(r.p(1).order_rec() == 0);  // (1, 14, 0) after the key
    CHECK(r.p(1).order_by() == 0);
    CHECK(r.p(1).order_until() == 115000u);
    CHECK(r.p(1).replan_at() == 100200u);
    CHECK(r.p(1).one_shot() == 1);
    CHECK(r.p(0).d9c() == 115000u);  // the asker's +0x9c = the helper's +0x98
    // +0xa4 = now - lo16(2000·sbyte +0x5d) + 8000 with +0x5d = 2
    CHECK(r.p(1).goal_since() == 100000u - 4000u + 8000u);
    CHECK(r.host.targets.back() == std::array<int, 3>{1, 14, 0});
    CHECK(r.host.msgs == std::vector<Msg>{{1, 14, 0, 2}});
    CHECK(r.ch.attitudes == std::vector<std::array<int, 6>>{{0, 1, 15, 15, 0, -1}});
    CHECK(r.ch.counts == std::vector<std::array<int, 2>>{{0, 15}, {0, 20}, {1, 40}});
    CHECK(draws(before, r.rng) == 0);  // no rand() for the player
}

TEST_CASE("seg13:0x81c2 score: -100·allies, Engine < 40, the team, a hunt -800, fuel -1000") {
    Rig r;
    r.host.rel[0][1] = -51;  // -51 - 200 (allies 2) + 100 (the team) = -151 <= -150: skipped
    r.host.rel[0][2] = -60;
    r.host.rel[0][3] = 0;
    set_ship_sb(r.s(2), pw::kAllies, 0);
    CHECK(help_command(r.c, 0) == 1);  // the Platypus: -60 + 0 = -60 > -150
    CHECK(r.p(1).order() == 0);
    CHECK(r.p(2).order() == 1);
    Rig q;
    q.host.rel[0][1] = 100;
    q.s(1).w26 = 39;       // -100 for a weak engine: 100 - 200 - 100 + 100 = -100
    q.p(1).goal_pool(14);  // the Viper hunts the player: -800
    q.p(1).goal_rec(0);
    q.p(1).goal_act(1);
    q.host.rel[0][2] = -500;
    q.host.rel[0][3] = -500;
    CHECK(help_command(q.c, 0) == 0);
    CHECK(q.host.msgs == std::vector<Msg>{{0, 7, 0, 6}});  // "You'll Have To Fend For Yourself"
}

TEST_CASE("seg13:0x81c2: nobody helps a computer ship: one rand() % 14, message 6 or the Knight") {
    Rig r;
    for (auto& row : r.host.rel) row.fill(-500);
    r.g.detail = 3;  // 12 - 3 = 9
    r.w.w992 = 0;
    r.w.budget = 100;  // 0 + 20 < 100: the call-in is asked
    // A state whose first rand() % 14 is below 9: message 6 and no call-in
    is::shell::Rng probe;
    std::uint32_t st = 1;
    for (;; ++st) {
        probe.set_state(st);
        if (probe.rand() % 14 < 9) break;
    }
    r.rng.set_state(st);
    const is::shell::Rng before = r.rng;
    r.p(2).d9c(0);
    CHECK(help_command(r.c, 2) == 0);
    CHECK(draws(before, r.rng) == 1);
    CHECK(r.host.calls.empty());
    CHECK(r.host.msgs == std::vector<Msg>{{2, 7, 0, 6}});
    // A draw at or above 9: seg13:0xadbe(c, 4, 0x68), refused (0), then message 6
    Rig q;
    for (auto& row : q.host.rel) row.fill(-500);
    q.w.budget = 100;
    for (st = 1;; ++st) {
        probe.set_state(st);
        if (probe.rand() % 14 >= 9) break;
    }
    q.rng.set_state(st);
    CHECK(help_command(q.c, 2) == 0);
    CHECK(q.host.calls == std::vector<std::array<int, 3>>{{2, 4, 0x68}});
    CHECK(q.host.msgs == std::vector<Msg>{{2, 7, 0, 6}});
}

TEST_CASE("seg13:0x81c2: the cooldown refuses a computer asker with -1") {
    Rig r;
    r.p(2).d9c(100001);
    CHECK(help_command(r.c, 2) == -1);
    CHECK(r.host.msgs.empty());
    r.p(0).d9c(200000);  // the player in Action mode is never refused
    r.host.rel[0][1] = 100;
    CHECK(help_command(r.c, 0) == 1);
}

TEST_CASE("seg13:0x8570 Ctrl+A at an icon: the Viper takes order 2 (ships-0005 Ctrl-A, b1)") {
    Rig r;
    r.w.pools.set_in_use(0, 3);
    r.target(0, 0, 1);  // icon 1 (BRAVO.EXE)
    r.host.rel[1][0] = 120;
    r.host.str = 100;
    // The Viper: b = -400 for a non-ship target; 62·2 - 90 + 60 = 94 > 120 fails (s = 120)
    CHECK(attack_command(r.c, 0) == 1);
    // ships-0005: ship 1 words (2, 0, 1, 0)
    CHECK(r.p(1).order() == 2);
    CHECK(r.p(1).order_pool() == 0);
    CHECK(r.p(1).order_rec() == 1);
    CHECK(r.p(1).order_by() == 0);
    CHECK(r.p(1).order_until() == 110000u);
    CHECK(r.p(1).replan_at() == 100400u);
    CHECK(r.p(0).d9c() == 115000u);
    CHECK(ship_sb(r.s(1), pw::kReaction) == 9);  // seg4:0x8346(j, 0, 1, 9)
    CHECK(r.w.w9432 == 30);
    CHECK(r.host.targets.back() == std::array<int, 3>{1, 0, 1});
    CHECK(r.host.msgs == std::vector<Msg>{{1, 14, 0, 2}});
}

TEST_CASE("seg13:0x8570 Ctrl+A at the Platypus: 0x91 from the Viper (ships-0005 c3)") {
    Rig r;
    r.g.last_ship = 2;  // no Pirate
    r.target(0, 14, 2);
    r.host.rel[1][0] = 120;  // a
    r.host.rel[1][2] = 80;   // b: 80 + 70 > 120
    r.host.rel[2][0] = 100;
    const is::shell::Rng before = r.rng;
    CHECK(attack_command(r.c, 0) == 0);
    // P 0xa3, the Viper 0x91 ("I Happen To Like Him", 1856), the Platypus 0x9e (the target):
    // 0x9e is not below 0x9e, so no coin flip
    CHECK(draws(before, r.rng) == 0);
    CHECK(r.host.msgs == std::vector<Msg>{{0, 14, 1, 0x91}});
    CHECK(r.host.msgs.back().rec == 1);
    CHECK(r.p(1).order() == 0);
}

TEST_CASE("seg13:0x8570 Ctrl+A at the Pirate: the Platypus answers 0x93 (ships-0005 b1)") {
    Rig r;
    r.target(0, 14, 3);
    r.host.rel[1][0] = 120;
    r.host.rel[1][3] = 0;
    set_ship_sb(r.s(1), pw::kAllies, 3);  // the Viper: another team than the Pirate's and allies >= 3: 0x9a
    const is::shell::Rng before = r.rng;
    CHECK(attack_command(r.c, 0) == 0);
    // P 0xa3; the Viper 0x9a < 0xa3 kept; the Platypus 0x93 < 0x9a kept; the Pirate 0x9e (the target)
    CHECK(r.host.msgs == std::vector<Msg>{{0, 14, 2, 0x93}});
    CHECK(draws(before, r.rng) == 0);  // every new code was lower or >= 0x9e: no coin flip
}

TEST_CASE("seg13:0x8570: the coin flip draws one rand() only for best > 0x90 and code < 0x9e") {
    Rig r;
    r.target(0, 14, 3);
    r.host.rel[1][0] = 120;
    r.host.rel[1][3] = 200;  // the Viper: b > 150: 0x91
    r.host.team = {0, 0, 4, 3, 1, 1, 1, 1};  // ship 2 a KNIGHT, the asker not: 0x94
    const is::shell::Rng before = r.rng;
    attack_command(r.c, 0);
    // best 0x91 after the Viper; the Knight's 0x94: best > 0x90 and 0x94 < 0x9e: one rand()
    CHECK(draws(before, r.rng) == 1);
}

TEST_CASE("seg13:0x8570: an Enforcer refuses 0x96; a computer asker passes the order to the player") {
    Rig r;
    r.host.team = {0, 7, 7, 7, 1, 1, 1, 1};
    r.target(2, 0, 1);
    r.w.pools.set_in_use(0, 3);
    r.host.rel[0][2] = -150;  // the player's view of the asker: 0xa2 (another team, a < -100)
    r.host.rel[2][0] = 0;     // the asker's view of the player > -100
    CHECK(attack_command(r.c, 2) == 1);
    CHECK(r.p(0).order() == 2);  // nobody obeyed: the player gets the order ("Please Attack %D")
    CHECK(r.p(0).order_by() == 2);
    CHECK(r.p(0).order_pool() == 0);
    CHECK(r.p(0).order_rec() == 1);
    CHECK(r.p(0).order_until() == 115000u);
    CHECK(r.p(2).d9c() == 115000u);
    CHECK(ship_sb(r.s(0), pw::kReaction) == 9);
    CHECK(r.host.msgs == std::vector<Msg>{{0, 14, 2, 2}});
    Rig q;
    q.g.last_ship = 1;
    q.host.team = {0, 7, 7, 7, 1, 1, 1, 1};
    q.target(0, 0, 1);
    CHECK(attack_command(q.c, 0) == 0);
    CHECK(q.host.msgs == std::vector<Msg>{{0, 14, 1, 0x96}});  // "...Enforcers" (1861)
}

TEST_CASE("seg13:0x8570: a busy ship asked to attack a cloaked ship answers 0x8d, never 0x8c") {
    // Each busy code jumps straight to the choice [BYTES seg13:0x8a54 seg13:0x8a6e seg13:0x8a95
    // seg13:0x8a9c]; the cloak test at seg13:0x8a9e is reached only with r = 1000 and skipped
    // (review M1).
    Rig r;
    r.g.last_ship = 1;
    r.target(0, 14, 3);
    r.host.rel[1][0] = 120;  // a
    r.host.rel[1][3] = 0;    // b + 70 <= a
    set_ship_b(r.s(3), pw::kCloaked, 1);
    set_ship_w(r.s(1), pw::kOrder, 2);  // busy: order 2 by ship 2 on icon 1, until now + 100000
    set_ship_w(r.s(1), pw::kOrderBy, 2);
    set_ship_w(r.s(1), pw::kOrderPool, 0);
    set_ship_w(r.s(1), pw::kOrderRec, 1);
    set_ship_d(r.s(1), pw::kOrderUntil, 200000u);
    const is::shell::Rng before = r.rng;
    CHECK(attack_command(r.c, 0) == 0);
    CHECK(r.host.msgs == std::vector<Msg>{{0, 14, 1, 0x8d}});
    CHECK(draws(before, r.rng) == 0);  // best 0xa3 -> 0x8d is lower: no coin flip
    // The same ship asked by its own order-giver's ship words: 0x9f for order 1 or 5
    Rig q;
    q.g.last_ship = 1;
    q.target(0, 14, 3);
    q.host.rel[1][0] = 120;
    q.host.rel[1][3] = 0;
    set_ship_b(q.s(3), pw::kCloaked, 1);
    set_ship_w(q.s(1), pw::kOrder, 5);
    set_ship_w(q.s(1), pw::kOrderBy, 2);
    set_ship_w(q.s(1), pw::kOrderPool, 14);
    set_ship_w(q.s(1), pw::kOrderRec, 0);
    set_ship_d(q.s(1), pw::kOrderUntil, 200000u);
    CHECK(attack_command(q.c, 0) == 0);
    CHECK(q.host.msgs == std::vector<Msg>{{0, 14, 1, 0x9f}});
}

TEST_CASE("seg13:0x8570: target record 14 reads ship 14 past the table as zeros (ADR 0016 site)") {
    Rig r;
    r.g.last_ship = 1;
    r.target(0, 4, 14);  // pool 4 record 14: the record, not the pool, is compared with 14
    r.host.rel[1][0] = 120;
    // ship 14 reads design 0: team 0 = the Viper's team: 0x98 (1863)
    CHECK(attack_command(r.c, 0) == 0);
    CHECK(r.host.msgs == std::vector<Msg>{{0, 14, 1, 0x98}});
}

TEST_CASE("seg13:0x8e98 Ctrl+D at the Platypus: the Viper takes order 5 (ships-0005 c3)") {
    Rig r;
    r.target(0, 14, 2);
    r.host.rel[1][2] = 100;  // 100 - 100 + 50 > 0
    r.host.rel[1][0] = 50;
    r.p(1).order_by(3);
    defend_command(r.c, 0);
    CHECK(r.p(1).order() == 5);
    CHECK(r.p(1).order_pool() == 14);
    CHECK(r.p(1).order_rec() == 2);  // ships-0005: (5, 14, 2)
    CHECK(r.p(1).order_until() == 120000u);
    CHECK(r.p(1).order_by() == 0);
    CHECK(r.p(0).d9c() == 115000u);
    // 0x13 to the old +0x58 always, then message 2 to the defender: neither reaches the player
    CHECK(r.host.msgs == std::vector<Msg>{{3, 14, 0, 0x13}, {1, 14, 0, 2}});
    CHECK(r.ch.attitudes == std::vector<std::array<int, 6>>{{2, 0, 15, 50, 1, -1}});
    Rig q;
    q.target(0, 0, 1);  // an icon: nothing (ships-0005 b1, c1)
    defend_command(q.c, 0);
    CHECK(q.host.msgs.empty());
    CHECK(q.p(0).d9c() == 0u);
}

TEST_CASE("seg13:0x915c Ctrl+F at an icon: message 0x14 from (7, 0) (ships-0005 b1, c3)") {
    Rig r;
    r.target(0, 0, 1);
    challenge_command(r.c, 0);
    CHECK(r.host.msgs == std::vector<Msg>{{0, 7, 0, 0x14}});
    CHECK(r.p(0).d9c() == 105000u);  // written first
}

TEST_CASE("seg13:0x915c Ctrl+F at the Pirate: accepted low, the Pirate's order 2 (ships-0005 b1)") {
    Rig r;
    r.target(0, 14, 3);
    r.p(3).order(4);
    r.p(3).order_until(90000);  // passed
    r.host.rel[3][0] = -50;     // a <= 50: w = -30 + 0 - (-10) - 0 + 100 = 80 > -90: q = 1
    r.host.str = 100;
    set_ship_sb(r.s(3), pw::kClass, 0);
    contact_words().w9680 = 7;
    contact_words().d8e00 = 0x00050009u;
    challenge_command(r.c, 0);
    CHECK(r.p(3).order() == 2);  // ships-0005: 4 -> 2
    CHECK(r.p(3).order_pool() == 14);
    CHECK(r.p(3).order_rec() == 0);
    CHECK(r.p(3).order_by() == 3);
    CHECK(r.p(3).goal_pool() == 14);
    CHECK(r.p(3).goal_rec() == 0);
    CHECK(r.p(3).goal_act() == 1);
    CHECK(r.p(3).order_until() == 140000u);
    CHECK(r.p(3).d9c() == 108000u);
    CHECK(r.host.manual == 0);
    // The player (team 0, an AVENGER) with q = 1: order 3; the replies 0x82 to the Pirate, then
    // 0x12 to the player: "I Love A Good Fight! Prepare To Die, Fool!"
    CHECK(r.p(0).order() == 3);
    CHECK(r.host.msgs.back() == Msg{0, 14, 3, 0x12});
    CHECK(r.p(0).grudge(3) == 0x15);
    CHECK(r.p(3).grudge(0) == 0x14);
    CHECK(contact_words().w9680 == 0);
    CHECK(contact_words().d8e00 == 0x00050000u);  // the low word only [BYTES seg13:0x970b]
    CHECK(r.ch.attitudes.back() == std::array<int, 6>{3, 0, -12, -55, 1, -1});
    contact_words() = ContactWords{};
}

TEST_CASE("seg13:0x915c: a strong target with +0x5e > 0 answers 0x4c and calls for help") {
    Rig r;
    r.target(0, 14, 3);
    r.host.str = 600;
    set_ship_sb(r.s(3), pw::kClass, 1);
    challenge_command(r.c, 0);
    CHECK(r.host.msgs.front() == Msg{0, 14, 3, 0x4c});
}

TEST_CASE("seg13:0x9810 Ctrl+S at a busy ship: 0x5a; a free friend takes 0x0d") {
    Rig r;
    r.target(0, 14, 3);
    r.p(3).order(2);
    r.p(3).order_until(140000);
    preserve_command(r.c, 0);
    CHECK(r.host.msgs == std::vector<Msg>{{0, 14, 3, 0x5a}});  // "DON'T WORRY! I'LL BE ALRIGHT."
    Rig q;
    q.target(0, 14, 1);
    q.host.rel[1][0] = 10;
    preserve_command(q.c, 0);
    CHECK(q.p(1).order() == 0x0d);
    CHECK(q.p(1).order_by() == 0);
    CHECK(q.p(1).order_until() == 115000u);
    CHECK(q.p(1).one_shot() == 0);
    CHECK(q.host.msgs == std::vector<Msg>{{1, 14, 0, 2}});
}

TEST_CASE("seg13:0x9f3e Ctrl+E at the Platypus: 0x32 from (7, 0) (ships-0005 c3)") {
    Rig r;
    r.target(0, 14, 2);
    exit_command(r.c, 0);
    CHECK(r.host.msgs == std::vector<Msg>{{0, 7, 0, 0x32}});
    Rig q;
    q.target(0, 14, 1);  // the teammate leaves: order 10, +0x94 = now
    exit_command(q.c, 0);
    CHECK(q.p(1).order() == 10);
    CHECK(q.p(1).replan_at() == 100000u);
    CHECK(q.host.msgs == std::vector<Msg>{{1, 14, 0, 2}});
}

TEST_CASE("seg13:0xa01c Ctrl+G: not a teammate 0x36; a teammate fetches fuel (order 0x0c)") {
    Rig r;
    r.target(0, 14, 2);
    gas_command(r.c, 0);
    CHECK(r.host.msgs == std::vector<Msg>{{0, 7, 0, 0x36}});
    Rig q;
    q.target(0, 14, 1);
    gas_command(q.c, 0);
    CHECK(q.p(1).order() == 0x0c);
    CHECK(q.p(1).goal_pool() == 14);
    CHECK(q.p(1).goal_rec() == 0);
    CHECK(q.p(1).goal_act() == 2);
    CHECK(q.p(1).goal_app() == 2);
    CHECK(q.p(1).order_until() == 125000u);
    CHECK(q.p(1).replan_at() == 100500u);
    CHECK(q.p(0).d9c() == 115000u);
}

TEST_CASE("seg13:0x9ce0 Ctrl+C: a rock is not capturable: 0x24 from the first teammate") {
    Rig r;
    r.w.pools.set_in_use(3, 2);
    r.target(0, 3, 1);
    r.o(3, 1).b53 = 0;
    CHECK(capture_command(r.c, 0) == 0);
    CHECK(r.host.msgs == std::vector<Msg>{{0, 14, 1, 0x24}});
    Rig q;
    q.w.pools.set_in_use(0, 2);
    q.target(0, 0, 1);
    q.o(0, 1).b53 = 1;
    CHECK(capture_command(q.c, 0) == 1);
    CHECK(q.p(1).order() == 9);
    CHECK(q.p(1).order_until() == 125000u);
    CHECK(q.host.targets.back() == std::array<int, 3>{1, 0, 1});
}

TEST_CASE("seg13:0x9a18, 0x9ae8 and Alt+E: the defenders' words") {
    Rig r;
    r.w.pools.set_in_use(2, 3);
    r.w.pools.set_in_use(3, 2);
    r.ch.table.resize(3);
    for (int k = 0; k < 3; ++k) {
        r.ch.table[static_cast<std::size_t>(k)].owner_pool = 14;
        r.ch.table[static_cast<std::size_t>(k)].owner_rec = k == 2 ? 1 : 0;
        r.cw.def(k).homing = 1;
    }
    send_defenders(r.c, 0, 3, 1);  // Alt+A at a rock
    CHECK(r.ch.table[0].w00 == 15);
    CHECK(r.ch.table[1].killer_pool == 3);
    CHECK(r.ch.table[1].killer_rec == 1);
    CHECK(r.ch.table[2].w00 == 0);  // another owner
    CHECK(r.cw.def(0).homing == 0);  // DGROUP:0x52f6 [BYTES seg13:0x9ab1]
    CHECK(r.cw.def(2).homing == 1);
    CHECK(r.host.sounds == std::vector<std::array<int, 4>>{{0x15, 2, 1, 1}});
    r.o(14, 0).x = 1000;
    r.o(14, 0).y = 2000;
    r.o(2, 0).x = 1100;
    r.o(2, 0).y = 1950;
    r.o(2, 0).damage = 9;
    defenders_return(r.c, 0);  // Alt+H
    CHECK(r.ch.table[0].w00 == 0);
    CHECK(r.cw.def(0).dx == 100);
    CHECK(r.cw.def(0).dy == -50);
    CHECK(r.o(2, 0).damage == 0);
    // Alt+E: state 15 toward (12, 1); an entry already in state 15 keeps its target
    r.ch.table[1].w00 = 15;
    defenders_leave(r.c);
    CHECK(r.ch.table[0].w00 == 15);
    CHECK(r.ch.table[0].killer_pool == 12);
    CHECK(r.ch.table[0].killer_rec == 1);
    CHECK(r.ch.table[1].killer_pool == 3);  // seg8:0x9028 returns for an owned entry in state 15
}

TEST_CASE("seg8:0x8256 state 15: no thrust until 220 cells, then lo16(8·o) / (d >> 2), clamped") {
    Rig r;
    r.w.pools.set_in_use(2, 1);
    r.w.pools.set_in_use(3, 2);
    r.ch.table.resize(1);
    DefenderEntry& e = r.ch.table[0];
    e.w00 = 15;
    e.killer_pool = 3;
    e.killer_rec = 1;
    r.o(2, 0).x = 0;
    r.o(2, 0).y = 0;
    r.o(3, 1).x = 8 * 100;  // 100 cells away in x
    r.o(3, 1).y = 0;
    CHECK(defender_attack_step(r.c, 0, true));
    CHECK(r.cw.def(0).homing == 0);  // d = 100 <= 220
    CHECK(r.o(2, 0).vx == 0);
    r.o(3, 1).x = 8 * 300;
    defender_attack_step(r.c, 0, true);
    CHECK(r.cw.def(0).homing == 1);
    CHECK(r.o(2, 0).vx == 0);
    defender_attack_step(r.c, 0, true);  // d = 300: 8·300 / 75 = 32
    CHECK(r.o(2, 0).vx == 32);
    CHECK(r.o(2, 0).vy == 0);
    defender_attack_step(r.c, 0, true);
    defender_attack_step(r.c, 0, true);
    CHECK(r.o(2, 0).vx == 70);  // clamped
    // A target out of play becomes (12, 1)
    r.o(3, 1).state = 1;
    defender_attack_step(r.c, 0, false);
    CHECK(e.killer_pool == 12);
    CHECK(e.killer_rec == 1);
    // The spin while the timer runs, on a frame without steering
    r.cw.def_timer(0) = r.g.now + 100;
    r.o(2, 0).frame = 15;
    defender_attack_step(r.c, 0, false);
    CHECK(r.o(2, 0).frame == 0);
}

TEST_CASE("seg13:0x4ac4 Ctrl+Alt+S: the pod, then the first hunter without a roster record arrests") {
    Rig r;
    for (int j = 1; j <= 3; ++j) {
        r.p(j).goal_pool(14);
        r.p(j).goal_rec(0);
        r.p(j).goal_act(1);
    }
    r.s(1).roster_index = 4;  // a roster record: the dropper's goal becomes (11, 0), +0x38 = 2
    r.s(2).roster_index = -1;
    r.s(3).roster_index = -1;
    r.w.ba14d[5] = 2;
    arrest_command(r.c, 0);
    CHECK(r.ch.pods == std::vector<std::array<int, 2>>{{0, 0}});
    CHECK(r.p(0).goal_act() == 2);
    CHECK(r.p(0).goal_pool() == 11);
    CHECK(r.p(0).goal_rec() == 0);
    CHECK(r.p(2).order() == 0x16);
    CHECK(r.p(2).order_rec() == 0);
    CHECK(r.p(2).order_until() == 150000u);
    CHECK(r.p(2).replan_at() == 100500u);
    CHECK(r.p(3).order() == 0);  // only the first
    CHECK(r.w.w8680[0] == 2);
    CHECK(contact_words().ba163 == 5);
    CHECK(r.w.ba14d[5] == 3);
    contact_words() = ContactWords{};
}

TEST_CASE("The Strategy player: the request record, the answer and the deferred reply") {
    Rig r;
    r.g.play_mode = 1;
    r.host.rel[2][0] = 100;
    r.host.rel[2][1] = -500;
    r.host.rel[2][3] = -500;
    r.cw.req_time = 0;
    CHECK(help_command(r.c, 2) == 0);  // the player is asked: recorded, message 0x34
    CHECK(r.cw.req_order == 1);
    CHECK(r.cw.req_pool == 14);
    CHECK(r.cw.req_rec == 2);
    CHECK(r.cw.req_by == 2);
    CHECK(r.cw.req_time == 100000u);
    CHECK(r.host.msgs.front() == Msg{0, 14, 2, 0x34});
    // D in Strategy mode with the Platypus marked: order 1 on (14, 2) answers the request
    r.host.mark_pool = 14;
    r.host.mark_rec = 2;
    r.g.now = 101000;
    strategy_help(r.c);
    CHECK(r.p(0).order() == 1);
    CHECK(r.p(0).order_until() == 131000u);
    CHECK(r.cw.reply_due == 101700u);
    CHECK(r.cw.req_time == 101000u);
    r.g.now = 101701;
    r.host.msgs.clear();
    request_reply(r.c);  // seg13:0x7b40(2): message 3 to the player
    CHECK(r.host.msgs.front() == Msg{0, 14, 2, 3});
    CHECK(r.cw.reply_due == 0u);
    CHECK(r.cw.req_order == 0);
    CHECK(r.cw.req_time == 0u);
}

TEST_CASE("The keys: Ctrl, Alt and neither call the bytes' routines in order (seg11:0x6f20-0x7130)") {
    Rig r;
    r.target(0, 0, 1);
    r.w.pools.set_in_use(0, 3);
    r.host.rel[1][0] = 120;
    key_a(r.c, false, false);  // Action mode: seg13:0xa186 returns at once
    CHECK(r.host.msgs.empty());
    CHECK(r.p(1).order() == 0);
    key_a(r.c, true, false);
    CHECK(r.p(1).order() == 2);
    Rig q;
    q.target(0, 14, 2);
    key_f(q.c, false, true);  // F without Ctrl: nothing
    CHECK(q.host.msgs.empty());
    key_e(q.c, true, false);
    CHECK(q.host.msgs == std::vector<Msg>{{0, 7, 0, 0x32}});
}

TEST_CASE("seg8:0x8256's head: DGROUP:0x9452 on a steering frame every max(4·(55 - wave), 110) ms") {
    Rig r;
    r.w.wave = 3;  // (55 - 3)·4 = 208
    CHECK(defender_mover_head(r.c));
    CHECK(r.cw.steer_due == 100208u);
    CHECK(r.cw.w1874 == 1);
    r.g.now = 100208;
    CHECK_FALSE(defender_mover_head(r.c));  // not below now
    r.g.now = 100209;
    CHECK(defender_mover_head(r.c));
    r.w.wave = 40;  // 60 < 110
    r.g.now = 200000;
    CHECK(defender_mover_head(r.c));
    CHECK(r.cw.steer_due == 200110u);
    r.cw.w1874 = 4;
    defender_mover_head(r.c);
    CHECK(r.cw.w1874 == 0);
}
