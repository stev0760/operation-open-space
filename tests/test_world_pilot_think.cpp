// The computer pilot's decisions (src/world/pilot_think.h, src/world/pilot_tick.h), against
// numbers worked by hand from the listings (work/world/m5s3/think-report.md) and the goal order
// of ships-0001's g1 run (rocks first, then a fuel tank, then an icon) [ORACLE ships-0001].
#include <array>
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/flight.h"
#include "world/pilot.h"
#include "world/pilot_think.h"
#include "world/pilot_tick.h"
#include "world/world.h"

using namespace is::world;

namespace {

struct Msg {
    int to, pool, rec, code;
};
struct Call {
    int ship, kind, msg;
};

class TestHost : public PilotHost {
public:
    std::array<std::int16_t, 8> team{}, tank{};   // by design index
    std::array<std::array<std::int16_t, 8>, 8> rel{};
    std::array<std::array<std::int16_t, 8>, 8> T{};  // the team table
    std::int16_t str = 100;
    std::vector<Msg> msgs;
    std::vector<Call> calls;
    std::vector<int> watched;
    std::int16_t anim = 0;
    int gun_calls = 0;
    std::vector<std::array<int, 3>> stolen;

    std::int16_t design_word(int n, int off) override {
        if (n < 0 || n >= 8) return 0;
        if (off == 0x38) return team[static_cast<std::size_t>(n)];
        if (off == 0x1e) return tank[static_cast<std::size_t>(n)];
        return 0;
    }
    std::int16_t relation(int a, int b) override {
        return rel[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)];
    }
    std::int16_t strength(int, int) override { return str; }
    std::int16_t team_word(int x, int y) override {
        return T[static_cast<std::size_t>(x)][static_cast<std::size_t>(y)];
    }
    void message(int to, int pool, int rec, int code) override { msgs.push_back({to, pool, rec, code}); }
    std::int16_t call_in(int s, int kind, int msg) override {
        calls.push_back({s, kind, msg});
        return 1;
    }
    void target(int, int, int) override {}
    std::int16_t visible(int, int) override { return 0; }
    std::int16_t weapon_system(int) override { return 0; }
    void gun(int) override { ++gun_calls; }
    void thrust(int, int) override {}
    void reactions(int) override {}
    void seg13_9ae8(int) override {}
    void seg13_9ce0(int) override {}
    void watch_ship(int i) override { watched.push_back(i); }
    void watch_tail() override {}
    std::int16_t ship_anim() override { return anim; }
    void steal_text(int p, int r, int s) override { stolen.push_back({p, r, s}); }
};

struct Rig {
    World w;
    WaveGlobals g;
    PilotGlobals pg;
    is::shell::Rng rng;
    TestHost host;
    PilotCtx c{w.pools, w.ships, g, w, pg, rng, host};

    Rig() {
        g.now = 100000;
        g.player = 0;
        g.last_ship = 1;
        g.gates = 1;
        g.detail = 3;
        g.game_frames = 2000;
        w.last_ship = 1;
        for (int i = 0; i < kShipCount; ++i) {
            Pilot P = pilot(w.ships, i);
            P.claim_pool(-1);
            P.claim_rec(-1);
            P.called_for(-1);
        }
        for (int p = 0; p < kPoolCount; ++p)
            if (w.pools.exists(p)) w.pools.set_in_use(p, 0);
        w.pools.set_in_use(kPoolShips, 2);
    }
    ShipRecord& s(int i) { return w.ships[static_cast<std::size_t>(i)]; }
    ObjectRecord& o(int pool, int rec) { return w.pools.rec(pool, rec); }
    void place(int pool, int rec, int x, int y) {
        o(pool, rec).x = static_cast<std::int16_t>(x);
        o(pool, rec).y = static_cast<std::int16_t>(y);
    }
};

// The number of rand() draws between two Rng states (at most 64).
int draws(is::shell::Rng before, const is::shell::Rng& after) {
    for (int n = 0; n <= 64; ++n) {
        if (before.state() == after.state()) return n;
        before.rand();
    }
    return -1;
}

// A state whose next rand() satisfies `pred`.
template <class Pred>
std::uint32_t state_where(Pred pred) {
    for (std::uint32_t s = 1;; ++s) {
        is::shell::Rng r;
        r.set_state(s);
        if (pred(r.rand())) return s;
    }
}

}  // namespace

TEST_CASE("pilot.h: the words sit at the original's offsets in the ship record") {
    ShipTable ships{};
    Pilot P = pilot(ships, 3);
    P.goal_pool(14);
    P.goal_rec(2);
    P.order(0x16);
    P.replan_at(0x12345678u);
    P.claim_pool(-1);
    CHECK(ships[3].w32 == 14);  // +0x32
    CHECK(ships[3].w34 == 2);   // +0x34
    CHECK(ships[3].w52 == 0x16);
    CHECK(ships[3].d94.get() == 0x12345678u);  // +0x94
    CHECK(ships[3].checkpoint == 0xff);        // byte +0x148
    P.grudge(5, -1);
    CHECK(ships[3].rc2[0xc7 - 0xc2] == 0xff);  // +0xc2 + 5
    P.due156(7);
    CHECK(ships[3].due156.get() == 7u);
}

TEST_CASE("seg4:0x84de: the greed level, worked from the listing") {
    Rig r;
    r.w.w9000 = 1;
    r.w.score_level = 2;                   // v = 2·7 + 45 = 59
    for (int k = 0; k < 8; ++k) r.o(kPoolIcons, k).state = k < 3 ? 0 : 1;  // 3 live icons
    r.g.game_frames = 100;                  // 59 - (700 - 100) >> 3 = -16, floor 3
    greed_level(r.c);                       // money 0 < 100: -20; 30 / 2 = 15: no step
    CHECK(r.pg.greed == -17);
    r.g.game_frames = 5000;                 // 5000/100 + 59 = 109, at most 100
    r.s(0).set_resources(300);              // money 300 > 250: +20
    for (int k = 0; k < 8; ++k) r.o(kPoolIcons, k).state = 1;  // 0 icons: three steps of -10
    greed_level(r.c);
    CHECK(r.pg.greed == 90);
    r.w.w9000 = 0;
    greed_level(r.c);
    CHECK(r.pg.greed == -10);  // [BYTES seg4:0x8611-0x8618]
}

namespace {

// The g1 scene: the player (ship 0) is dead to the search (alive 0); ship 1, team 0, flies at
// (8000, 8000) with a rock 200 cells east, a fuel tank 300 cells north, an icon 400 cells west.
void g1_scene(Rig& r) {
    r.host.team = {0, 0, 2, 0, 0, 0, 0, 0};
    r.host.tank = {1000, 1000, 1000, 0, 0, 0, 0, 0};
    r.w.w9000 = 1;
    r.w.score_level = 0;
    r.s(0).bec = 0;
    r.s(1).bec = 1;
    r.s(1).design_index = 1;
    r.s(1).fuel = 1000;
    r.s(1).w26 = 100;
    Pilot P = pilot(r.w.ships, 1);
    P.cls(1);
    P.allies(2);
    P.credit(300);
    // the previous plan: attack (8, 0) [pool 8 has no record in use]
    P.goal_pool(8), P.goal_rec(0), P.goal_act(1), P.goal_app(2);
    P.copy_goal_to_estimate();
    r.place(kPoolShips, 1, 8000, 8000);
    r.w.pools.set_in_use(kPoolIcons, 1);
    r.place(kPoolIcons, 0, 8000 - 8 * 400, 8000);
    r.o(kPoolIcons, 0).b53 = 1;
    r.w.pools.set_in_use(kPoolRocks, 1);
    r.place(kPoolRocks, 0, 8000 + 8 * 200, 8000);
    r.w.pools.set_in_use(4, 1);
    r.place(4, 0, 8000, 8000 - 8 * 300);
    r.o(4, 0).b53 = 1;
}

}  // namespace

TEST_CASE("seg4:0x78 and seg4:0x2f98: rocks first, then the fuel tank, then the icon (ships-0001 g1)") {
    Rig r;
    g1_scene(r);
    Pilot P = pilot(r.w.ships, 1);

    // A. Early in the wave (greed 3, so the divisor 10) with the tank full: the rock's
    // attack-object cost ((500 + 450)·4 + 100)·10/100·6/6 = 390 beats the icon's capture
    // ((700 - 15 + 300)·100/10 = 9850) and the full tank's ((600·4 + 80000)·10/100 = 8240).
    r.pg.greed = 3;
    is::shell::Rng before = r.rng;
    think(r.c, 1);
    CHECK(P.goal_pool() == 3);
    CHECK(P.goal_rec() == 0);
    CHECK(P.goal_act() == 1);  // attack
    CHECK(P.goal_app() == 2);
    CHECK(P.goal_since() == r.g.now);
    CHECK(P.replan_at() == r.g.now + 3000u);
    CHECK(draws(before, r.rng) == 3);  // one per ship j, then the object attack's rand() % (e + 2)
    CHECK(r.host.gun_calls == 1);      // 200 cells, the head-for approach: seg11:0x4e0a

    // B. 3001 ms later the fuel is 850, below tank - 100: the tank's capture 600·4·10/100 = 240
    // beats the rock, now the goal ((2·400/3 + 450)·4 + 100)·10/100 = 296.
    r.g.now += 3001;
    r.s(1).fuel = 850;
    before = r.rng;
    think(r.c, 1);
    CHECK(P.goal_pool() == 4);
    CHECK(P.goal_rec() == 0);
    CHECK(P.goal_act() == 3);  // 20·(9 - 0) > 2·greed: go to it [BYTES seg4:0x44d2-0x44f5]
    CHECK(draws(before, r.rng) == 2);

    // C. Later (greed 100) with the tank full again: the icon's capture (700 - 500 + 300)·100/100
    // = 500 beats the rock's 3900 and the tank's 81332.
    r.g.now += 3001;
    r.s(1).fuel = 1000;
    r.pg.greed = 100;
    before = r.rng;
    think(r.c, 1);
    CHECK(P.goal_pool() == 0);
    CHECK(P.goal_rec() == 0);
    CHECK(P.goal_act() == 2);  // 180 > 200 fails: capture
    CHECK(draws(before, r.rng) == 2);
}

TEST_CASE("seg4:0x78: an attacker gives up a goal it has held 19000 ms and leaves it out") {
    Rig r;
    g1_scene(r);
    Pilot P = pilot(r.w.ships, 1);
    r.pg.greed = 3;
    think(r.c, 1);
    REQUIRE(P.goal_pool() == 3);
    const std::uint32_t since = P.goal_since();
    r.g.now = since + 19001;
    is::shell::Rng before = r.rng;
    think(r.c, 1);
    // The re-plan keeps the rock (no change, so +0xa4 stays), then the near path's give-up
    // re-chooses with the rock left out: the tank, 8240 [BYTES seg4:0xaa4-0xb4b].
    CHECK(P.goal_pool() == 4);
    CHECK(P.goal_act() == 3);
    CHECK(P.order() == 0);  // -1, then 0 at seg4:0x2fb8
    CHECK(P.goal_since() == r.g.now);
    CHECK(draws(before, r.rng) == 5);  // 2 + 1 for the re-plan, 2 for the give-up
}

TEST_CASE("seg4:0x6a78: a slot's score, worked from the listing") {
    Rig r;
    g1_scene(r);
    class WHost : public TestHost {
    public:
        std::int16_t weapon_word(int w, int off) override {
            if (w != 3) return 0;
            switch (off) {
            case 0x2a: return 6;
            case 0x04: return 1;
            default: return 0;
            }
        }
    } host;
    host.team = r.host.team;
    PilotCtx c{r.w.pools, r.w.ships, r.g, r.w, r.pg, r.rng, host};
    ShipRecord& S = r.s(1);
    S.weapon[1] = 3;
    S.weapon_count[1] = 5;
    Pilot P(S);
    P.goal_pool(3);
    P.goal_rec(0);
    P.goal_act(1);
    // want = 8 (rel -100 < 0) + 1 (< -60) - 1 (enemies) = 8; 6 < 8: 8 + (3·(6 - 8))/4 = 7;
    // score = rand() % 3 - |7 - 6| + 12.
    is::shell::Rng mirror = r.rng;
    const int expect = mirror.rand() % 3 + 11;
    CHECK(weapon_score(c, 1, 1) == expect);
    CHECK(r.rng.state() == mirror.state());
}

TEST_CASE("seg4:0x8626: an at-rest player's score of a rock ahead") {
    Rig r;
    r.host.team = {0, 0, 0, 0, 0, 0, 0, 0};
    r.s(0).bec = 1;
    r.place(kPoolShips, 0, 8000, 8000);
    r.w.pools.set_in_use(kPoolRocks, 1);
    r.place(kPoolRocks, 0, 8000 + 8 * 100, 8000);
    // dx = player - rock = -100 cells: seg8:0x19b0(num -dy = 0, den -dx = 100) = 4; the heading 4
    // gives the angle 0 and the facing cos128(0).
    REQUIRE(direction16(0, 100) == 4);
    r.o(kPoolShips, 0).frame = 4;
    const std::int16_t facing = cos128(0);
    // not attacked (DGROUP:0x8e28 + 2000 <= now); approach 0 (at rest); sc = facing/5 + 100/-10 + 40
    const std::int16_t sc = estimate_score(r.c, kPoolRocks, 0, 1);
    CHECK(r.pg.attacked == 0);
    CHECK(sc == facing / 5 + 30);
    CHECK(r.pg.capture_gate == (facing > 0x69 ? 1 : 0));  // near: 100 < 360 cells
}

TEST_CASE("seg4:0x7e2a: the Pirate's warning, and the call 7000 ms later") {
    Rig r;
    r.host.team = {0, 0, 0, 0, 0, 0, 0, 0};
    r.host.T[3][0] = 0;     // T(3, 0) < 150
    r.host.T[4][0] = -100;  // the Knight stays away
    r.w.budget = 130;
    r.w.score_level = 3;
    r.s(0).bec = 1;
    r.s(1).bec = 1;
    r.s(0).w26 = 100;
    r.s(0).set_resources(1000);
    // c = (1000 - 220)/32 + 0/-100 + 3/3 - 0 - 1 = 24; (2·24)/2 = 24
    r.rng.set_state(state_where([](int v) { return v % 720 < 24; }));
    callins(r.c);
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0].to == 0);
    CHECK(r.host.msgs[0].pool == 7);
    CHECK(r.host.msgs[0].code == 0x6b);
    CHECK(r.pg.pirate_due == r.g.now + 7000u);
    CHECK(r.host.calls.empty());
    // Before the deadline the Pirate block is skipped: no draw in it.
    r.g.now += 6000;
    is::shell::Rng before = r.rng;
    callins(r.c);
    CHECK(draws(before, r.rng) == 0);  // the Knight fails its T(4, ·) test, the Patrolman his
    r.g.now += 1001;
    r.rng.set_state(state_where([](int v) { return v % 720 < 24; }));
    callins(r.c);
    REQUIRE(r.host.calls.size() == 1);
    CHECK(r.host.calls[0].ship == 0);
    CHECK(r.host.calls[0].kind == 3);
    CHECK(r.host.calls[0].msg == 0x69);
    CHECK(r.pg.pirate_due == 0u);
    // Before frame 500 the deadlines are cleared and nothing is drawn.
    r.pg.knight_due = 5;
    r.g.game_frames = 450;
    before = r.rng;
    callins(r.c);
    CHECK(r.pg.knight_due == 0u);
    CHECK(draws(before, r.rng) == 0);
}

TEST_CASE("seg4:0x7e2a: the Patrolman, with no Enforcer in the wave") {
    Rig r;
    r.host.team = {0, 0, 0, 0, 0, 0, 0, 0};
    r.host.T[3][0] = 200;   // no Pirate
    r.host.T[4][0] = -100;  // no Knight
    r.w.budget = 130;
    r.w.score_level = 2;
    r.w.w964a = 8;
    r.pg.crimes_wave = 6;
    // c = 8/2 + 0/-140 + 2/2 = 5
    r.rng.set_state(state_where([](int v) { return v % 300 < 5; }));
    is::shell::Rng before = r.rng;
    callins(r.c);
    REQUIRE(r.host.calls.size() == 1);
    CHECK(r.host.calls[0].kind == 0x16);
    CHECK(r.host.calls[0].msg == 0x6d);
    CHECK(draws(before, r.rng) == 1);
    // An Enforcer in the wave: no draw.
    r.s(1).design_index = 1;
    r.host.team[1] = 7;
    before = r.rng;
    callins(r.c);
    CHECK(draws(before, r.rng) == 0);
}

TEST_CASE("seg4:0x83ae: the hunters' guess at a cloaked ship, wrapped once") {
    Rig r;
    r.s(0).bec = 1;
    r.s(1).bec = 1;
    r.s(1).be8 = 1;  // cloaked
    Pilot H = pilot(r.w.ships, 0);
    H.goal_pool(14);
    H.goal_rec(1);
    r.place(kPoolShips, 1, 100, 25000);
    Pilot T = pilot(r.w.ships, 1);
    T.guess_x(12000);
    T.guess_y(12000);
    is::shell::Rng mirror = r.rng;
    int x = mirror.rand() % 4000 + 100 - 2000;
    int y = mirror.rand() % 4000 + 25000 - 2000;
    if (x < 0) x += 25600; else if (x > 25600) x -= 25600;
    if (y < 0) y += 25600; else if (y > 25600) y -= 25600;
    cloak_guess(r.c);
    CHECK(T.guess_x() == x);
    CHECK(T.guess_y() == y);
    CHECK(r.rng.state() == mirror.state());
}

TEST_CASE("seg13:0x3302: a friend's claim on the player's goal is given up") {
    Rig r;
    r.g.last_ship = 2;
    r.host.rel[2][0] = 10;
    Pilot Pl = pilot(r.w.ships, 0);
    Pl.goal_pool(0), Pl.goal_rec(1), Pl.goal_act(2);
    Pilot A = pilot(r.w.ships, 1);  // still after its capture goal: the claim stays
    A.goal_pool(0), A.goal_rec(2), A.goal_act(2), A.claim_pool(0), A.claim_rec(2);
    Pilot B = pilot(r.w.ships, 2);
    B.goal_pool(3), B.goal_rec(0), B.goal_act(1), B.claim_pool(0), B.claim_rec(1);
    claim_reset(r.c);
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0].to == 0);
    CHECK(r.host.msgs[0].pool == 14);
    CHECK(r.host.msgs[0].rec == 2);
    CHECK(r.host.msgs[0].code == 0xb8);
    CHECK(A.claim_pool() == 0);
    CHECK(A.claim_rec() == 2);
    CHECK(B.claim_pool() == -1);
    CHECK(B.claim_rec() == -1);
}

TEST_CASE("seg4:0x6700: a FUZZY ONE steals the capture goal and a claimant is told") {
    Rig r;
    r.g.last_ship = 2;
    r.g.demo = 0;
    r.host.team = {1, 2, 0, 0, 0, 0, 0, 0};  // s = 0 of team 1, ship 1 a FUZZY ONE
    for (int i = 0; i < 3; ++i) {
        r.s(i).bec = 1;
        r.s(i).design_index = static_cast<std::int16_t>(i);
    }
    r.w.pools.set_in_use(kPoolIcons, 2);
    r.o(kPoolIcons, 1).b53 = 1;
    Pilot S = pilot(r.w.ships, 0);
    S.est_pool(0), S.est_rec(1);
    Pilot C2 = pilot(r.w.ships, 2);  // already after the icon: claims it
    C2.goal_pool(0), C2.goal_rec(1);
    r.place(kPoolShips, 0, 1000, 1000);
    r.place(kPoolShips, 1, 1000 + 8 * 100, 1000);
    rivals(r.c, 0);
    Pilot F = pilot(r.w.ships, 1);
    CHECK(F.goal_pool() == 0);
    CHECK(F.goal_rec() == 1);
    CHECK(F.est_act() == 2);   // within 1900 cells
    CHECK(F.goal_act() == 2);
    CHECK(F.order() == 0x0e);
    CHECK(F.order_pool() == 0x0e);
    CHECK(F.order_rec() == 0);
    CHECK(F.replan_at() == r.g.now + 500u);
    REQUIRE(r.host.msgs.size() == 2);
    CHECK(r.host.msgs[0].to == 0);  // seg3:0x6ec(s, 14, j, 0x17)
    CHECK(r.host.msgs[0].rec == 1);
    CHECK(r.host.msgs[0].code == 0x17);
    CHECK(r.host.msgs[1].to == 2);  // the claimant: seg3:0x6ec(j, 14, s, 0x66)
    CHECK(r.host.msgs[1].rec == 0);
    CHECK(r.host.msgs[1].code == 0x66);
    CHECK(C2.claim_pool() == 0);
    CHECK(C2.claim_rec() == 1);
}

TEST_CASE("seg13:0x675e: the 5000 ms watch refresh") {
    Rig r;
    r.host.anim = 1;
    pilot(r.w.ships, 0).due156(r.g.now - 1);  // the player: only while out of play
    pilot(r.w.ships, 1).due156(r.g.now - 1);
    watch_refresh(r.c);
    REQUIRE(r.host.watched.size() == 1);
    CHECK(r.host.watched[0] == 1);
    CHECK(pilot(r.w.ships, 1).due156() == r.g.now + 5000u);
    CHECK(pilot(r.w.ships, 0).due156() == r.g.now - 1);
    // ShipAnim 0: only 10000 ms past the due time.
    r.host.anim = 0;
    r.host.watched.clear();
    pilot(r.w.ships, 1).due156(r.g.now - 5000);
    watch_refresh(r.c);
    CHECK(r.host.watched.empty());
    pilot(r.w.ships, 1).due156(r.g.now - 10001);
    watch_refresh(r.c);
    CHECK(r.host.watched.size() == 1);
}

TEST_CASE("seg4:0x485c and seg13:0x78b8: an offer of help, doubled by j's team") {
    Rig r;
    r.g.last_ship = 3;
    r.w.pools.set_in_use(kPoolShips, 4);
    r.host.team = {0, 0, 0, 1, 0, 0, 0, 0};
    r.host.tank = {0, 1000, 1000, 1000, 0, 0, 0, 0};
    r.host.str = 70;
    r.host.rel[3][2] = -100;  // the hunter dislikes j: a hater
    for (int i = 1; i <= 3; ++i) {
        r.s(i).bec = 1;
        r.s(i).design_index = static_cast<std::int16_t>(i);
    }
    Pilot I = pilot(r.w.ships, 1);
    I.cls(1), I.allies(2), I.order_pool(14), I.order_rec(2);
    r.s(1).fuel = 1000;
    r.s(2).w26 = 30, r.s(2).w28 = 100, r.s(2).w2a = 100;
    pilot(r.w.ships, 2).allies(3);
    Pilot K = pilot(r.w.ships, 3);
    K.goal_pool(14), K.goal_rec(2), K.goal_act(1);
    r.place(kPoolShips, 1, 1000, 1000);
    r.place(kPoolShips, 2, 1000 + 8 * 100, 1000);
    // seg4:0x485c: l18 = 1 (+0x26 < 50) + 1 (a hunter); 4 + 10 + 21 = 35 > 30 chooses j = 2.
    // seg13:0x78b8(1, 2): l8 = 0 + 200; the hunter k = 3: l6 = 0, la = 70 + (2·70)/-5 = 42,
    // doubled to 84 because j's team is i's [BYTES seg13:0x79c9-0x79e9]; 200 >= 0: k, accepted.
    after_choice(r.c, 1, -1);
    CHECK(I.order() == 1);
    CHECK(I.order_pool() == 14);
    CHECK(I.order_rec() == 2);
    CHECK(I.order_by() == 1);
    CHECK(I.one_shot() == 1);
    CHECK(I.order_until() == r.g.now + 18000u);
    CHECK(I.replan_at() == r.g.now + 300u);
}

TEST_CASE("seg4:0x4d00: low fuel goes to the nearest tank, then to the gate without money") {
    Rig r;
    r.host.tank = {0, 1000, 0, 0, 0, 0, 0, 0};
    r.s(1).bec = 1;
    r.s(1).design_index = 1;
    r.s(1).fuel = 50;  // below 1000/10; rand() % 50 > 150 cannot hold
    r.w.pools.set_in_use(4, 2);
    r.place(kPoolShips, 1, 1000, 1000);
    r.place(4, 0, 1000 + 8 * 300, 1000);
    r.place(4, 1, 1000 + 8 * 100, 1000);
    Pilot P = pilot(r.w.ships, 1);
    is::shell::Rng before = r.rng;
    needs(r.c, 1);
    CHECK(P.est_pool() == 4);
    CHECK(P.est_rec() == 1);
    CHECK(P.est_act() == 2);
    CHECK(P.replan_at() == r.g.now + 4000u);
    CHECK(draws(before, r.rng) == 1);
    r.w.pools.set_in_use(4, 0);
    P.est_pool(3);
    needs(r.c, 1);  // money 0 < 200: the gate (12, 0) and code 0xb7
    CHECK(P.est_pool() == 12);
    CHECK(P.est_rec() == 0);
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0].code == 0xb7);
    // Leaving ((12, ·) with +0x36 = 2) returns at once [BYTES seg4:0x4d0f-0x4d20]; held by ship
    // 0's tether, full of fuel, an attacker turns on the holder.
    r.s(1).fuel = 1000;
    P.est_pool(3);
    r.o(kPoolShips, 1).owner_pool = 14;
    r.o(kPoolShips, 1).owner_rec = 0;
    needs(r.c, 1);
    CHECK(P.est_pool() == 14);
    CHECK(P.est_rec() == 0);
    CHECK(P.est_act() == 1);
}

TEST_CASE("seg4:0x59bc: the player's estimate takes a gate ahead, or idles at (8, 0)") {
    Rig r;
    r.s(0).bec = 1;
    r.w.pools.set_in_use(kPoolGateCentres, 2);
    r.place(kPoolShips, 0, 8000, 8000);
    r.o(kPoolShips, 0).frame = 4;  // facing the gate: angle 0
    r.place(kPoolGateCentres, 1, 8000 + 8 * 100, 8000);
    const std::int16_t facing = cos128(0);
    REQUIRE(facing > 0x69);
    // gate record 1: facing/5 + 100/-7 + 55; near (facing > 105, 100 < 360 cells): DGROUP:0x9688 = 1
    const std::int16_t expect = static_cast<std::int16_t>(facing / 5 + 41);
    Pilot P = pilot(r.w.ships, 0);
    CHECK(player_estimate(r.c) == expect);
    CHECK(P.est_pool() == 12);
    CHECK(P.est_rec() == 1);
    CHECK(P.est_act() == 2);  // a gate with DGROUP:0x9688 set [BYTES seg4:0x5d14-0x5d44]
    CHECK(P.est_app() == 2);
    // A gate far behind (1200 cells after the wrap) scores below -40: the idle estimate.
    r.place(kPoolGateCentres, 1, 8000 - 8 * 2000, 8000);
    CHECK(player_estimate(r.c) == 0);
    CHECK(P.est_pool() == 8);
    CHECK(P.est_rec() == 0);
    CHECK(P.est_act() == 3);
    CHECK(P.est_app() == 2);
    CHECK(r.pg.attacked == 0);  // [bp-0x2e] unwritten: 0 (ADR 0016)
}
