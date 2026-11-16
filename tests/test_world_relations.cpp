// The ships' attitudes (src/world/relations.h): seg13:0x2688, seg4:0x5d9c, seg13:0x2e60,
// seg13:0x2714 and seg4:0x7d36, against numbers worked by hand from the listings (M5 stage 3
// part B, track CONTACT; work/world/m5s3/contact-report.md "Readings no verifier has checked").
#include <array>
#include <cstdint>
#include <tuple>
#include <vector>

#include "doctest.h"
#include "world/collide.h"
#include "world/pilot.h"
#include "world/relations.h"
#include "world/roster_build.h"

using namespace is::world;

namespace {

// Three ships 0 ... 2, ship i of design i with the design teams 2, 3, 4 (World::design_ratings),
// in play and alive; the program's team table (roster_build.h roster_words()) zeroed.
struct Rig {
    World w;
    std::vector<std::tuple<int, int, int, int>> messages;
    AttitudeOut out;
    explicit Rig(int ships = 3) {
        w.player = 0;
        w.last_ship = static_cast<std::int16_t>(ships - 1);
        w.kind = 0;
        w.pools.set_in_use(kPoolShips, static_cast<std::int16_t>(ships));
        for (int i = 0; i < ships; ++i) {
            ShipRecord& s = w.ships[static_cast<std::size_t>(i)];
            s.design_index = static_cast<std::int16_t>(i);
            s.bec = 1;
            set_ship_b(s, pw::kAllies, 3);
            set_ship_b(s, pw::kClass, 3);
            w.pools.rec(kPoolShips, i).state = 0;
        }
        for (int i = 0; i < ships; ++i) set_team(i, static_cast<std::int16_t>(2 + i));
        roster_words().team_table.fill(0);
        out.message = [this](int r, int p, int rec, int code) { messages.emplace_back(r, p, rec, code); };
    }
    ~Rig() { roster_words().team_table.fill(0); }
    void set_team(int i, std::int16_t t) { w.design_ratings[static_cast<std::size_t>(i)].team = t; }
    ShipRecord& s(int i) { return w.ships[static_cast<std::size_t>(i)]; }
    std::int16_t A(int a, int b) { return pilot(s(a)).attitude(b); }
    void setA(int a, int b, std::int16_t v) { pilot(s(a)).attitude(b, v); }
    std::int16_t& T(int x, int y) { return roster_words().team_table[static_cast<std::size_t>(8 * x + y)]; }
};

}  // namespace

TEST_CASE("seg13:0x2688: A(a, b) + T, then the allies or enemies byte, -300 in Demon's Gate") {
    Rig r;
    r.setA(0, 1, 50);
    r.T(2, 3) = 30;
    set_ship_b(r.s(0), pw::kAllies, 1);  // +0x5d
    // v = 80 > 0: + 20·(3 - 1) [BYTES seg13:0x26d2-0x26e5]
    CHECK(relation(r.w, 0, 1) == 120);
    r.setA(0, 1, -50);
    r.T(2, 3) = 10;
    set_ship_b(r.s(0), pw::kClass, 5);  // +0x5e
    // v = -40 <= 0: + 20·(5 - 3) [BYTES seg13:0x26ea-0x26fa]
    CHECK(relation(r.w, 0, 1) == 0);
    r.setA(0, 1, 0);
    r.T(2, 3) = 0;
    CHECK(relation(r.w, 0, 1) == 40);  // v = 0 takes the enemies branch
    r.w.handover.demons_gate = 1;      // DGROUP:0x92e8 [BYTES seg13:0x26fd-0x2707]
    CHECK(relation(r.w, 0, 1) == -300);
}

TEST_CASE("seg4:0x5d9c: 100 outside the ships, (E·K)/(E/2 + 50) in 32 bits, Sb 0 -> 10") {
    Rig r;
    CHECK(strength_ratio(r.w, -1, 0) == 100);
    CHECK(strength_ratio(r.w, 0, 3) == 100);  // above DGROUP:0x9316 = 2
    r.s(0).w26 = 80, r.s(0).kill_value = 300;  // Sa = 24000 / 90 = 266
    r.s(1).w26 = 50, r.s(1).kill_value = 200;  // Sb = 10000 / 75 = 133
    CHECK(strength_ratio(r.w, 0, 1) == 200);   // 26600 / 133
    r.s(1).w26 = 0;                            // Sb = 0 -> 10 [BYTES seg4:0x5e5f]
    CHECK(strength_ratio(r.w, 0, 1) == 2660);
    r.s(1).w26 = 50;
    r.s(0).kill_value = -300;                  // Sa = -266
    CHECK(strength_ratio(r.w, 0, 1) == -200);
    // 100·Sa above 16 bits keeps its low word only at the end: K = 30000, E = 101 gives Sa =
    // 3030000 / 100 = 30300, 100·Sa / 133 = 22781
    r.s(0).w26 = 101, r.s(0).kill_value = 30000;
    CHECK(strength_ratio(r.w, 0, 1) == 22781);
}

TEST_CASE("seg13:0x2e60: seg13:0x4eec twice, then the hurt byte +0xf6 + 9y + k") {
    Rig r;
    r.w.player = 2;
    grudge_note(r.w, 0, 1, 4);  // D[4] = 10: 50 to each w82, byte +0xf6 + 9 + 4 += 10
    CHECK(r.s(0).w82 == 50);    // (1000 - 0)·50 / 1000
    CHECK(r.s(1).w82 == 50);
    CHECK(ship_sb(r.s(0), 0xf6 + 9 + 4) == 10);
    grudge_note(r.w, 0, 1, 1);  // D[1] = 2, the grudge byte +0xf7 + 9y
    CHECK(pilot(r.s(0)).hurt(1, 0) == 2);
    // k = -1 -> 9: the weights only (7·5 = 35) [BYTES seg13:0x2e6e-0x2e74 seg13:0x2ec6-0x2eca]
    grudge_note(r.w, 2, 1, -1);
    CHECK(r.s(2).w82 == 35);
    // k = 2 between teammates writes no byte [BYTES seg13:0x2ed8-0x2ef6]
    r.set_team(1, 2);
    grudge_note(r.w, 0, 1, 2);
    CHECK(ship_sb(r.s(0), 0xf6 + 9 + 2) == 0);
    // y the player: 12·D[k] [BYTES seg13:0x2e9f-0x2eb6]
    Rig q;
    grudge_note(q.w, 1, 0, 6);  // D[6] = 3: 36
    CHECK(q.s(1).w82 == 36);
    CHECK(q.s(0).w82 == 36);
    CHECK(ship_sb(q.s(1), 0xf6 + 0 + 6) == 3);
}

TEST_CASE("seg13:0x2714: the ally byte, the team pair, the ship pair and the w82 notes") {
    Rig r;  // player 0; a = 1 (team 3), b = 0 (team 2)
    set_ship_b(r.s(0), pw::kAllies, 1);  // b's +0x5d = 1: ds = 10 + 2·10·10/100 = 12
    attitude_change(r.w, r.out, 1, 0, 5, 10, 0, -1);
    CHECK(r.T(3, 2) == 5);  // T·dt = 0: the full dt, and dt/3 the other way
    CHECK(r.T(2, 3) == 1);
    // relation(1, 0) = 0 + 5 + 20·(3 - 3) = 5; 5·12 > 0: A(1, 0) += 24/3, A(0, 1) += 12/4
    CHECK(r.A(1, 0) == 8);
    CHECK(r.A(0, 1) == 3);
    // seg13:0x2e60(1, 0, 9): y the player, 12·D[9] = 84 for both
    CHECK(r.s(1).w82 == 84);
    CHECK(r.s(0).w82 == 84);
    CHECK(r.messages.empty());
}

TEST_CASE("seg13:0x2714: a fleeing ship doubles ds and dt; the clamps at +-200") {
    Rig r;
    r.w.player = 2;
    for (int i = 0; i < 3; ++i) r.set_team(i, 2);  // one team: no team pair
    r.s(0).w38 = 4;                     // a flees b [BYTES seg13:0x2857-0x286e]
    r.s(0).w34 = 1;
    attitude_change(r.w, r.out, 0, 1, -40, -150, 0, 1);
    // ds = -300 (c = +0x5e = 3, no change); relation 0: ds·0 not > 0: A(0, 1) += -300 -> -200,
    // A(1, 0) += -100
    CHECK(r.A(0, 1) == -200);
    CHECK(r.A(1, 0) == -100);
    CHECK(ship_sb(r.s(0), 0xf6 + 9 + 1) == 2);  // seg13:0x2e60(0, 1, 1): D[1]
    CHECK(r.T(2, 2) == 0);
}

TEST_CASE("seg13:0x2714: dt halved outside +-300, divided by 3 outside +-450, 0 outside +-800") {
    Rig r;
    r.T(3, 2) = 400;
    attitude_change(r.w, r.out, 1, 0, 10, 0, 0, -1);
    // dt = 5; T·dt > 0: T(3, 2) += 5/2, T(2, 3) += 5/6
    CHECK(r.T(3, 2) == 402);
    CHECK(r.T(2, 3) == 0);
    Rig q;
    q.T(2, 3) = 500;  // the reverse word counts too: 30 -> 15 -> 5
    attitude_change(q.w, q.out, 1, 0, 30, 0, 0, -1);
    CHECK(q.T(3, 2) == 5);  // T(3, 2)·5 = 0: the full dt
    CHECK(q.T(2, 3) == 501);
    Rig z;
    z.T(3, 2) = -900;
    attitude_change(z.w, z.out, 1, 0, 30, 0, 0, -1);
    CHECK(z.T(3, 2) == -900);
    CHECK(z.T(2, 3) == 0);
    // A duel does nothing [BYTES seg13:0x2722-0x2727]
    Rig d;
    d.w.kind = 7;
    attitude_change(d.w, d.out, 1, 0, 30, 30, 0, -1);
    CHECK(d.T(3, 2) == 0);
    CHECK(d.A(1, 0) == 0);
}

TEST_CASE("seg13:0x2714: the spreading loop, message 0x51 and n = 3") {
    Rig r;  // a = 1, b = 0 (the player), k = 2 (team 4)
    r.setA(2, 1, -150);  // relation(2, 1) = -150 < -100
    r.s(1).w32 = kPoolShips, r.s(1).w34 = 2, r.s(1).w38 = 1;  // a hunts k
    r.s(0).w32 = kPoolShips, r.s(0).w34 = 1, r.s(0).w38 = 1;  // b hunts a
    attitude_change(r.w, r.out, 1, 0, 6, 30, 1, -1);
    REQUIRE(r.messages.size() == 1);
    CHECK(r.messages[0] == std::make_tuple(0, int{kPoolShips}, 2, 0x51));
    CHECK(r.T(4, 2) == -6);   // -= 18 / 3
    CHECK(r.A(2, 0) == -30);  // -= 90 / 3
    // A liking k moves a quarter of the amounts toward b
    Rig q;
    q.setA(2, 1, 150);
    attitude_change(q.w, q.out, 1, 0, 8, 40, 1, -1);
    CHECK(q.T(4, 2) == 2);
    CHECK(q.A(2, 0) == 10);
    CHECK(q.messages.empty());
    // A ship off the world or dead takes no part
    Rig o;
    o.setA(2, 1, 150);
    o.w.pools.rec(kPoolShips, 2).state = 1;
    attitude_change(o.w, o.out, 1, 0, 8, 40, 1, -1);
    CHECK(o.A(2, 0) == 0);
}

TEST_CASE("seg4:0x7d36: the fight bytes, the player's tallies and the hit stamps") {
    Rig r;
    ContactWords cw;
    r.w.frames = 12345;
    r.s(0).w32 = 14, r.s(0).w34 = 1;  // the player's goal is ship 1 (the literal 14)
    for (int k = 0; k < 5; ++k) set_ship_w(r.s(1), 0x44 + 2 * k, static_cast<std::int16_t>(k + 1));
    fight_record(r.w, cw, 0, 1, 40);  // k = 2 for d > 30
    CHECK(pilot(r.s(0)).fight(1) == 2);
    CHECK(pilot(r.s(1)).fight(0) == -2);
    CHECK(cw.w9680 == 1);
    CHECK(cw.d8e00 == 40u);
    CHECK(cw.w9682 == 0);
    // +0x44 ... +0x4a moved up, +0x44 = frame# / 10 [BYTES seg4:0x7def-0x7e1e]
    CHECK(ship_w(r.s(1), 0x44) == 1234);
    CHECK(ship_w(r.s(1), 0x46) == 1);
    CHECK(ship_w(r.s(1), 0x4c) == 4);
    // The clamps at 6 and -6
    pilot(r.s(0)).fight(1, 5);
    pilot(r.s(1)).fight(0, -5);
    fight_record(r.w, cw, 0, 1, 31);
    CHECK(pilot(r.s(0)).fight(1) == 6);
    CHECK(pilot(r.s(1)).fight(0) == -6);
    // b the player: DGROUP:0x9682 and 0x8e04; no stamps (a is not the player)
    fight_record(r.w, cw, 1, 0, -7);
    CHECK(cw.w9682 == 1);
    CHECK(cw.d8e04 == static_cast<std::uint32_t>(-7));
    CHECK(pilot(r.s(1)).fight(0) == -5);  // k = 1: -6 + 1
    // No goal between them: only the stamps
    Rig q;
    ContactWords c2;
    q.w.frames = 500;
    fight_record(q.w, c2, 0, 2, 5);
    CHECK(c2.w9680 == 0);
    CHECK(ship_w(q.s(2), 0x44) == 50);
}
