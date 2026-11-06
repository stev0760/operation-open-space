// The ship between waves (src/world/roster.h): the wave-1 placement with its fresh roster, the
// wave end's write-back seg9:0x2b5d-0x2e59 and the next placement's fill seg14:0x1430, with
// core-0001's carry-over 39, 9, 39, 1 [ORACLE core-0001] and core-0003's fuel 1209 -> roster
// byte 30 -> 1200 [ORACLE core-0003]; seg14:0x2300's MainShip pick, the class bands, the +0x06
// update and the record's byte layout.
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "formats/ships.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/roster.h"
#include "world/setup.h"

using namespace is::world;

namespace {

is::fmt::ShipRecord shp_rec(const std::string& name, std::uint8_t tank, std::uint8_t weapon, std::uint16_t body) {
    is::fmt::ShipRecord r;
    r.name = name;
    r.fuel_tank = tank;
    r.weapon = weapon;
    r.team = 2;
    r.to_friends = 1;
    r.to_enemies = 2;
    r.body = body;
    return r;
}

// The weapon words the remap reads: block 0 word 0 = 186 (the table is filled), word +0x36 = -1
// but for weapon 1 (17), as hud/weapons.cpp's base table holds [BYTES seg10:0x1fb2].
struct WeaponWordsRig {
    WeaponWordsRig() {
        set_weapon_words([](int w, int off) -> std::int16_t {
            if (w == 0 && off == 0) return 186;
            if (off == 0x36) return w == 1 ? 17 : -1;
            return 0;
        });
    }
    ~WeaponWordsRig() { set_weapon_words(nullptr); }
};

struct Rig {
    World w;
    is::fmt::MasterShp ships;
    is::fmt::MasterBod bodies;
    is::shell::Rng rng;
    WeaponWordsRig words;
    Rig() {
        ship_setup_minimal(w);
        w.player_game = 1;
        bodies.records.resize(3);
        bodies.records[0].bitmap_id = 310;  // rejected by seg14:0x455c
        bodies.records[1].bitmap_id = 60;   // TRANSFORMER's #4000 60
        bodies.records[2].bitmap_id = 61;
        ships.records.push_back(shp_rec("EAGLE", 30, 2, 0));
        ships.records.push_back(shp_rec("TRANSFORMER", 33, 8, 1));
        ships.records.push_back(shp_rec("SKY TALON", 33, 8, 2));
    }
    // seg14:0x1b84's part the roster reads: the tank and the wing type (design +0x28 = 3).
    void design(const is::fmt::ShipRecord& s) {
        w.design_ratings[0].tank = static_cast<std::int16_t>(40 * s.fuel_tank);
        w.design_ratings[0].special = 3;
        w.ship(0).has_flame = 1;
        w.ship(0).bec = 1;
    }
    void place(const std::string& main_ship) {
        const RosterPlacement p = roster_placement(w, ships, bodies, main_ship, rng);
        REQUIRE(p.shp_index >= 0);
        const is::fmt::ShipRecord& s = ships.records[static_cast<std::size_t>(p.shp_index)];
        design(s);
        ship_from_roster(w, 0, s);
        if (purchase_due(w, p.fresh_ship)) purchase_pass(w, 0, 1);
    }
};

}  // namespace

TEST_CASE("roster: wave 1 placement, the wave end, wave 2 carries the ship [ORACLE core-0001 core-0003]") {
    set_purchase_pass(nullptr);  // the fallback of seg16:0x3784(i, 1)
    Rig r;
    World& w = r.w;
    w.wave = 1;  // DGROUP:0x91f8 during the first placement
    r.place("TRANSFORMER");
    ShipRecord& s = w.ship(0);
    REQUIRE(w.roster.size() == 1);
    const RosterRecord& rec = w.roster[0];
    CHECK(rec.shp == 1);
    CHECK(rec.credit10 == 15);
    CHECK(rec.slots == 2);
    CHECK(rec.weapon[1] == 8);
    CHECK(rec.count[1] == 20);
    CHECK(rec.rel_r[0] == 0xff);
    CHECK(rec.b32 == 0);  // the powerups start at 0 (claim 1)
    for (int k = 0; k < 5; ++k) CHECK(rec.b33[static_cast<std::size_t>(k)] == 0);
    // The oracle's wave-1 ship [ORACLE probe-0001]: systems 100, slot 1 = the SHP weapon with 20,
    // slot 0 = weapon 0 with 1, +0xcb = 2, fuel 0x528, +0x24 = 1, +0xe0 = +0xe1 = 1, credit 150.
    CHECK(s.w26 == 100);
    CHECK(s.w28 == 100);
    CHECK(s.w2a == 100);
    CHECK(s.w2c == 100);
    CHECK(s.slots_cb() == 2);
    CHECK(s.weapon[0] == 0);
    CHECK(s.weapon_count[0] == 1);
    CHECK(s.weapon[1] == 8);
    CHECK(s.weapon_count[1] == 20);
    CHECK(s.fuel == 0x528);
    CHECK(s.w24 == 1);
    CHECK(sbyte_at(s, 0xe0) == 1);
    CHECK(sbyte_at(s, 0xe1) == 1);
    CHECK(ship_credit(s) == 150);
    CHECK(s.resources() == 0);
    CHECK(w.roster[0].met == 1);
    CHECK(w.roster[0].last_wave == 1);

    // The wave: damage to 39, 9, 39, 1, two missiles fired, fuel burnt to core-0003's 1209,
    // credit and resources from a capture.
    s.w26 = 39, s.w28 = 9, s.w2a = 39, s.w2c = 1;
    s.weapon_count[1] = 18;
    s.fuel = 1209;
    set_ship_credit(s, 250);
    s.set_resources(3);
    s.kill_value = 500;

    // The wave end's write-back (seg9:0x2b5d-0x2e59).
    RosterUpdate u;
    roster_from_ship(w, 0, u);
    CHECK(w.roster[0].systems[0] == 39);
    CHECK(w.roster[0].systems[3] == 1);
    CHECK(w.roster[0].count[1] == 18);
    CHECK(w.roster[0].fuel40 == 30);
    CHECK(w.roster[0].credit10 == 25);
    CHECK(w.roster[0].resources == 3);
    // v = (250 x 3 + 500) / 100 = 12: class 1 [0, 28).
    CHECK(w.roster[0].cls == 1);
    // The player's index leaves the directory list (seg9:0x2d83).
    for (int j = 0; j < 8; ++j) CHECK(w.dir().byte(0x1c0 + static_cast<std::size_t>(j)) != 0);

    // Between the waves the ship record is the next wave's to fill: spoil it.
    s.w26 = s.w28 = s.w2a = s.w2c = 100;
    s.weapon_count[1] = 20;
    s.fuel = 0;
    set_ship_credit(s, 0);
    s.set_resources(0);
    set_byte_at(s, 0xe0, 0);
    s.weapon_slot = 1;

    w.wave = 2;
    r.place("EAGLE");  // a later wave never reads MainShip
    CHECK(w.roster.size() == 1);
    CHECK(w.roster[0].shp == 1);
    CHECK(s.w26 == 39);
    CHECK(s.w28 == 9);
    CHECK(s.w2a == 39);
    CHECK(s.w2c == 1);
    CHECK(s.weapon[1] == 8);
    CHECK(s.weapon_count[1] == 18);
    CHECK(s.fuel == 1200);  // 40 x 30, no refill [ORACLE core-0003]
    CHECK(ship_credit(s) == 250);
    CHECK(s.resources() == 3);
    CHECK(s.w24 == 1);
    CHECK(sbyte_at(s, 0xe0) == 0);  // kept in the ship record (claim 9)
    CHECK(s.weapon_slot == 1);
    CHECK(w.roster[0].met == 2);
    CHECK(w.roster[0].run == 2);
    CHECK(w.roster[0].last_wave == 2);
    CHECK(s.w00 == 0);
}

TEST_CASE("roster: seg14:0x2300's MainShip pick") {
    Rig r;
    World& w = r.w;
    SUBCASE("a match") {
        const MainShipPick p = pick_main_ship(w, r.ships, r.bodies, "SKY TALON", r.rng);
        CHECK(p.index == 2);
        CHECK(w.w95b4 == 20000);
        CHECK_FALSE(p.rewrite);
    }
    SUBCASE("no record of the name: index 1") {
        const MainShipPick p = pick_main_ship(w, r.ships, r.bodies, "NOSUCH", r.rng);
        CHECK(p.index == 1);
        CHECK(w.w95b4 == -1000);
    }
    SUBCASE("a rejected ship becomes TRANSFORMER") {
        const MainShipPick p = pick_main_ship(w, r.ships, r.bodies, "EAGLE", r.rng);
        CHECK(p.rewrite);
        CHECK(p.name == "TRANSFORMER");
        CHECK(p.index == 1);
    }
    SUBCASE("the Plus edition accepts every ship") {
        w.w11aa = 1;
        const MainShipPick p = pick_main_ship(w, r.ships, r.bodies, "EAGLE", r.rng);
        CHECK_FALSE(p.rewrite);
        CHECK(p.index == 0);
    }
    SUBCASE("fewer than two records keep index 0 [DESIGN]") {
        r.ships.records.resize(1);
        const MainShipPick p = pick_main_ship(w, r.ships, r.bodies, "NOSUCH", r.rng);
        CHECK(p.index == 0);
    }
}

TEST_CASE("roster: the second weapon's remap through DGROUP:0x2f8a and the cap of the fuel") {
    Rig r;
    World& w = r.w;
    const is::fmt::ShipRecord s1 = shp_rec("X", 33, 1, 1);
    CHECK(fresh_record(w, s1, 0).weapon[1] == 17);
    w.w11aa = 1;
    CHECK(fresh_record(w, s1, 0).weapon[1] == 1);
    const is::fmt::ShipRecord s0 = shp_rec("Y", 33, 0, 1);
    CHECK(fresh_record(w, s0, 0).slots == 1);

    // 40 x roster +0x1c above the tank: capped, +0x1c rewritten (seg14:0x1915).
    w.roster.assign(1, fresh_record(w, r.ships.records[1], 1));
    w.roster[0].fuel40 = 200;  // unsigned `mul`: 8000
    w.ship(0).roster_index = 0;
    r.design(r.ships.records[1]);
    ship_from_roster(w, 0, r.ships.records[1]);
    CHECK(w.ship(0).fuel == 1320);
    CHECK(w.roster[0].fuel40 == 33);
    // A computer ship (not DGROUP:0x9646) gets at least 2/3 of the tank (seg14:0x18e8).
    w.last_ship = 1;
    w.pools.resize(kPoolShips, 2);
    w.ship(1).roster_index = 0;
    w.roster[0].fuel40 = 1;
    ship_from_roster(w, 1, r.ships.records[1]);
    CHECK(w.ship(1).fuel == 880);
}

TEST_CASE("roster: a broken run of waves sets ship word +0x00 = 4 (seg14:0x19d6)") {
    Rig r;
    World& w = r.w;
    w.roster.assign(1, fresh_record(w, r.ships.records[1], 1));
    w.roster[0].last_wave = 1;
    w.roster[0].run = 2;
    w.ship(0).roster_index = 0;
    w.wave = 3;
    ship_from_roster(w, 0, r.ships.records[1]);
    CHECK(w.ship(0).w00 == 4);
    CHECK(w.roster[0].run == 1);
    CHECK(w.roster[0].met == 1);
    CHECK(w.roster[0].last_wave == 3);
}

TEST_CASE("roster: the class bands DGROUP:0x952 / DGROUP:0x96c and the stale class") {
    CHECK(roster_class(0, 0) == 1);
    CHECK(roster_class(27, 0) == 1);
    CHECK(roster_class(28, 0) == 2);
    CHECK(roster_class(96, 0) == 9);
    CHECK(roster_class(97, 0) == 10);
    CHECK(roster_class(113, 0) == 10);
    CHECK(roster_class(114, 7) == 7);  // outside: the previous ship's class
    CHECK(roster_class(-1, 0) == 0);   // none earlier: 0 (ADR 0014 decision 5)
}

TEST_CASE("roster: the +0x06 update seg9:0x2e97-0x2f74 with its 16-bit arithmetic") {
    World w;
    is::fmt::MasterShp ships;
    ships.records.push_back(shp_rec("A", 33, 0, 0));
    ships.records[0].to_enemies = 2;
    w.score_level = 6;  // DGROUP:0x9334
    w.w9332 = 5;        // m = |2 - 5| = 3
    w.roster.resize(5);
    w.roster[0].w06 = 300, w.roster[0].cls = 1;  // c + 3 < D: 300 / -3 = -100; 900 / -20 = -45
    w.roster[1].w06 = 300, w.roster[1].cls = 4;  // c + 1 < D: -(300 / 8) = -37; -45
    w.roster[2].w06 = 300, w.roster[2].cls = 5;  // else: 300 / -14 = -21; -45
    w.roster[3].w06 = 10, w.roster[3].cls = 0;   // -3 - 1: 6
    w.roster[4].w06 = 30000, w.roster[4].cls = 5;  // 30000 x 3 = 90000: lo16 24464 / -20 = -1223
    roster_ratings(w, ships);
    CHECK(w.roster[0].w06 == 155);
    CHECK(w.roster[1].w06 == 218);
    CHECK(w.roster[2].w06 == 234);
    CHECK(w.roster[3].w06 == 6);
    CHECK(w.roster[4].w06 == 30000 - 2142 - 1223);
    // The zero clamp on the 16-bit sum.
    w.roster.resize(1);
    w.w9332 = -37;  // m = 39
    w.roster[0].w06 = 10, w.roster[0].cls = 0;
    roster_ratings(w, ships);  // 10 / -3 = -3, 390 / -20 = -19: 10 - 22 < 0
    CHECK(w.roster[0].w06 == 0);
}

TEST_CASE("roster: the record's bytes at formats.md's offsets") {
    RosterRecord r;
    r.shp = 1;
    r.credit10 = 0x1234;
    r.resources = -2;
    r.fuel40 = 30;
    r.slots = 2;
    r.weapon[1] = 8;
    r.systems = {39, 9, 39, 1};
    r.cls = 3;
    r.gap = 6;
    std::uint8_t b[kRosterRecordSize] = {};
    roster_to_bytes(r, b);
    CHECK(b[0x00] == 1);
    CHECK(b[0x02] == 0x34);
    CHECK(b[0x03] == 0x12);
    CHECK(b[0x04] == 0xfe);
    CHECK(b[0x05] == 0xff);
    CHECK(b[0x1c] == 30);
    CHECK(b[0x1d] == 2);
    CHECK(b[0x29] == 8);
    CHECK(b[0x38] == 39);
    CHECK(b[0x3b] == 1);
    CHECK(b[0x76] == 3);
    CHECK(b[0x7b] == 6);
    const RosterRecord q = roster_from_bytes(b);
    CHECK(q.credit10 == 0x1234);
    CHECK(q.resources == -2);
    CHECK(q.systems[1] == 9);
    CHECK(roster_byte(r, 0x3b) == 1);
}

TEST_CASE("roster: seg9:0x6db0 refuels to the design tank; a computer ship joins (seg14:0xdd6)") {
    Rig r;
    World& w = r.w;
    w.design_ratings[0].tank = 1320;
    w.ship(0).fuel = 3;
    refuel_all(w);
    CHECK(w.ship(0).fuel == 1320);
    w.roster.assign(1, RosterRecord{});
    w.w95b4 = 1, w.w95b2 = 0;
    const int k = roster_add(w, 1, r.ships.records[2], 2, -1, r.rng);
    CHECK(k == 1);
    REQUIRE(w.roster.size() == 2);
    CHECK(w.roster[1].w06 == 15);
    CHECK(w.roster[1].credit10 >= 10);
    CHECK(w.roster[1].resources >= 150);
    CHECK(w.roster[1].systems[2] == 100);
    w.w95b4 = 0;
    CHECK(roster_add(w, 1, r.ships.records[2], 2, 7, r.rng) == 7);
}

TEST_CASE("roster: the sheet's path seg14:0xd1a -> seg16:0x3784 -> seg16:0x3468 (M4 s4, C067)") {
    // world.md Ship sheets, Path: built only when the placement ran seg16:0x3784(i, 1), the
    // new-body flag is 0, and not with design word +0x28 = -1 while the slot holds a DC.
    CHECK(placement_rebuilds_sheet(true, 0, 3, true));
    CHECK(placement_rebuilds_sheet(true, 0, -1, false));
    CHECK_FALSE(placement_rebuilds_sheet(true, 0, -1, true));
    CHECK_FALSE(placement_rebuilds_sheet(false, 0, 3, true));  // a later wave without the flag
    CHECK_FALSE(placement_rebuilds_sheet(true, 1, 3, true));   // the new body (M8)
    World w;
    w.wave = 2;
    CHECK_FALSE(purchase_due(w, false));
    CHECK(purchase_due(w, true));
    w.wave = 1;
    CHECK(purchase_due(w, false));
}
