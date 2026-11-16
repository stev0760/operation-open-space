// The wave's ships (src/world/roster_build.h): seg14:0x0(mode) with its pickers seg14:0x2300 /
// seg14:0x3274, the class seg14:0x40d8, the record step seg14:0xdd6, the names seg9:0x5be0 and
// the relation memory seg14:0x421c, against numbers worked from the listings (work/world/m5s3/
// roster-report.md) and against ships-0001: wave 1 has no computer ship [ORACLE wave-0001], the
// restored ORACLE3X game places AVENGER 1 (GOLDEN VIPER) and FUZZY 2 (PLATYPUS) in its revisited
// directory [ORACLE ships-0001]. The MASTER files come from IS_ORIGINAL_DIR; without it the
// golden cases skip.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "doctest.h"
#include "formats/savegame.h"
#include "formats/ships.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/design_words.h"
#include "world/rating.h"
#include "world/roster.h"
#include "world/roster_build.h"
#include "world/setup.h"

using namespace is::world;
namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> read_all(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}
fs::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    std::error_code ec;
    if (v && *v && fs::exists(fs::path(v) / "MASTER.SHP", ec)) return fs::path(v);
    return {};
}

// The default [Teams] names of INSPACE.INI (Team1 ... Team8) [BYTES INSPACE.INI].
const char* kTeams[8] = {"AVENGERS", "PREDATORS", "FUZZY ONES", "PIRATES", "KNIGHTS", "SPEED DEMONS", "RENEGADES", "ENFORCERS"};

struct FakeHost : RosterHost {
    is::fmt::MasterShp shp;
    is::fmt::MasterBod bod;
    is::shell::Rng r;
    std::array<SlotWords, 8> slots{};
    std::vector<int> freed, bought, tails;
    std::vector<std::string> todos;
    int fills = 0;
    const is::fmt::MasterShp& ships() override { return shp; }
    const is::fmt::MasterBod& bodies() override { return bod; }
    is::shell::Rng& rng() override { return r; }
    std::string main_ship() override { return "TRANSFORMER"; }
    std::string pilot() override { return "PILOT"; }
    std::string team_name(int t) override { return t >= 0 && t < 8 ? kTeams[t] : ""; }
    SlotWords slot_words(int j) override { return slots[static_cast<std::size_t>(j)]; }
    void set_slot_words(int j, const SlotWords& v) override { slots[static_cast<std::size_t>(j)] = v; }
    void slot_free(int j) override {
        freed.push_back(j);
        slots[static_cast<std::size_t>(j)] = SlotWords{};
    }
    void design_fill(World& w, int i, int s) override {
        ++fills;
        design_fill_portable(w, i, shp.records[static_cast<std::size_t>(s)], bod, 0);
    }
    void purchases(World&, int i) override { bought.push_back(i); }
    void ship_tail(World&, int i) override { tails.push_back(i); }
    void todo(const std::string& what, const char*) override { todos.push_back(what); }
};

bool load_master(FakeHost& h) {
    const fs::path dir = original_dir();
    if (dir.empty()) return false;
    const auto s = read_all(dir / "MASTER.SHP");
    const auto b = read_all(dir / "MASTER.BOD");
    h.shp = is::fmt::parse_master_shp(s.data(), s.size());
    h.bod = is::fmt::parse_master_bod(b.data(), b.size());
    return h.shp.records.size() > 88;
}
int shp_index(const FakeHost& h, const std::string& name) {
    for (std::size_t v = 0; v < h.shp.records.size(); ++v)
        if (h.shp.records[v].name == name) return static_cast<int>(v);
    return -1;
}

// A World as the wave set-up leaves it before seg14:0x0: the player's ship alone, a player game,
// the Plus words (DGROUP:0x11aa != 0: seg14:0x455c accepts every body), a budget.
void base_world(World& w) {
    ship_setup_minimal(w);
    w.player_game = 1;
    w.w11aa = 1;
    w.budget = 1000;
    w.detail = 3;
    w.w9346 = -1;
    roster_words() = RosterWords{};
}

}  // namespace

TEST_CASE("seg14:0x40d8: a design's class from the listing's arithmetic") {
    FakeHost h;
    if (!load_master(h)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    // TRANSFORMER (SHP 1, BOD 33): a = 24·34/36 = 22, ·6/5 (BOD +0x20 bit 2) = 26; q = 22·5/41 =
    // 2; x = ((410 - 25)/24 - 2)·2 = 28; weapon 8 → 7; 7 + 28 + 26 = 61 → band 6 (55 ... 62).
    CHECK(design_class(h.shp, h.bod, shp_index(h, "TRANSFORMER")) == 6);
    // PATROLMAN (SHP 47, BOD 23): a = 600/36 = 16, ·6/5 (SHP bit 8) = 19; x = ((320 - 30)/20 -
    // 88/32)·2 = 24, ·5/6 (no SHP bit 0x10) = 20; weapon 15 → 7; 46 → band 4 (40 ... 47).
    CHECK(design_class(h.shp, h.bod, shp_index(h, "PATROLMAN")) == 4);
    // GOLDEN VIPER (SHP 44, BOD 25): a = 594/36 = 16; x = ((270 - 18)/22 - 88/27)·2 = 16; weapon
    // 7 → 6; 38 → band 3 (34 ... 39).
    CHECK(design_class(h.shp, h.bod, shp_index(h, "GOLDEN VIPER")) == 3);
}

TEST_CASE("seg14:0x1b84: the shot divisors, the hit sound and +0xa6 per design (s3-contact 2.1)") {
    is::fmt::ShipRecord s;
    s.armor = 34;
    s.hit_sound = 7;
    is::fmt::BodyRecord b;
    const DesignRating d = design_rating(s, b);
    // +0x08 = s/4, +0x0a = s/4, +0x0c = s/5, +0x0e = s/5; the hit row +0x10 ... +0x16
    // [BYTES seg14:0x1cae-0x1d0e].
    CHECK(d.shot_div == std::array<std::int16_t, 4>{8, 8, 6, 6});
    CHECK(d.armor4a == 8);
    CHECK(d.armor5a == 6);
    CHECK(d.hit_sound == 7);
    CHECK(d.armor == 34);
    CHECK(d.team == 0);  // the placement writes the team (seg14:0x14ca)
}

TEST_CASE("seg14:0x4520 and seg9:0x5be0: ship lookup and the names") {
    World w;
    base_world(w);
    FakeHost h;
    w.last_ship = 2;
    w.ship(0).roster_index = 0;
    w.ship(1).roster_index = 5;
    w.ship(2).roster_index = -1;
    CHECK(ship_with_roster(w, 5, 2) == 1);
    CHECK(ship_with_roster(w, -1, 2) == 2);
    CHECK(ship_with_roster(w, 7, 2) == -1);
    CHECK(ship_with_roster(w, 5, 0) == -1);
    w.roster.assign(4, RosterRecord{});
    w.roster[1].team = 0;
    w.roster[2].team = 2;
    w.roster[3].team = 1;
    w.player = 0;
    // The team name cut at its first ' ' or a final 'S', " " and the index [BYTES seg9:0x5c00-0x5ca4]:
    // ships-0001's AVENGER 1, FUZZY 2 and PREDATOR 3 [ORACLE ships-0001].
    roster_name(w, h, 1, 1);
    roster_name(w, h, 2, 2);
    roster_name(w, h, 3, 3);
    CHECK(std::string(w.roster[1].name.data()) == "AVENGER 1");
    CHECK(std::string(w.roster[2].name.data()) == "FUZZY 2");
    CHECK(std::string(w.roster[3].name.data()) == "PREDATOR 3");
    roster_name(w, h, 0, 1);  // the player's ship takes the pilot (DGROUP:0x380c)
    CHECK(std::string(w.roster[1].name.data()) == "PILOT");
}

TEST_CASE("seg14:0xdd6: a new record, a reused record and a known ship") {
    World w;
    base_world(w);
    FakeHost h;
    h.shp.records.resize(3);
    h.shp.records[2].fuel_tank = 30;
    h.shp.records[2].team = 4;
    h.shp.records[2].weapon = 0;
    w.roster.assign(1, RosterRecord{});
    w.score_level = 2;
    w.w95b4 = 5;
    w.w95b2 = 0;
    is::shell::Rng copy = h.r;
    const int r2 = copy.rand(), r4 = copy.rand();
    CHECK(roster_take(w, h, 1, 2, -1) == 1);
    REQUIRE(w.roster.size() == 2);
    const RosterRecord& n = w.roster[1];
    CHECK(n.shp == 2);
    CHECK(n.w06 == 15);
    CHECK(n.slots == 1);
    CHECK(n.fuel40 == 30);
    CHECK(n.team == 4);
    CHECK(n.rel_r[0] == 0xff);
    // rand() % 30 + 10 and rand() % 150 + 150 with DGROUP:0x9334 = 2 [BYTES seg14:0x106c-0x109f].
    CHECK(n.credit10 == r2 % 30 + 10);
    CHECK(n.resources == r4 % 150 + 150);
    CHECK(h.r.state() == copy.state());
    CHECK(w.ship(1).roster_index == 1);
    CHECK(w.w992 == 40);
    CHECK(std::string(n.name.data()) == "KNIGHT 1");

    // Known: r0 leaves every directory's list and joins the current one; a ship away earns credit
    // min((wave - last)·(D + 15), 200) [BYTES seg14:0x10d0-0x1189].
    w.w95b4 = 0;
    w.records.resize(2);
    w.record = 1;
    w.records[0].b[0x1c0 + 3] = 1;
    w.roster[1].last_wave = 2;
    w.roster[1].credit10 = 40;
    w.wave = 9;
    CHECK(roster_take(w, h, 2, 2, 1) == 1);
    CHECK(w.records[0].b[0x1c0 + 3] == 0xff);
    CHECK(w.records[1].b[0x1c0] == 1);
    CHECK(w.roster[1].credit10 == 40 + 119);  // (9 - 2)·17 = 119
    CHECK(w.ship(2).roster_index == 1);

    // Reuse: 40 records, the lowest +0x06 below 1000 that no earlier slot holds [BYTES seg14:0xdf9-0xe67].
    w.roster.assign(40, RosterRecord{});
    for (int k = 0; k < 40; ++k) w.roster[static_cast<std::size_t>(k)].w06 = static_cast<std::int16_t>(500 + k);
    w.roster[7].w06 = 3;
    w.roster[9].w06 = 2;
    w.ship(0).roster_index = 9;  // held by slot 0
    w.records[1].b[0x1c0 + 5] = 7;
    w.w95b4 = 5;
    CHECK(roster_take(w, h, 1, 2, -1) == 7);
    CHECK(w.records[1].b[0x1c0 + 5] == 0xff);
    CHECK(w.roster[7].w06 == 15);
}

TEST_CASE("seg14:0x421c: the five-entry memory written back at the wave end") {
    World w;
    base_world(w);
    w.last_ship = 2;
    w.roster.assign(3, RosterRecord{});
    for (auto& r : w.roster) r.rel_r.fill(0xff);
    for (int j = 0; j < 3; ++j) {
        w.ship(j).roster_index = static_cast<std::int16_t>(j);
        w.ship(j).bec = 1;
    }
    ShipRecord& I = w.ship(1);
    set_byte_at(I, 0x5d, 0);
    set_byte_at(I, 0x5e, 0);
    set_byte_at(I, 0xf6 + 9 * 0 + 1, 3);  // ship 0: an enemy event of kind 1
    set_byte_at(I, 0xf6 + 9 * 2 + 6, 5);  // ship 2: a friendly event of kind 6
    relation_memory(w, 1);
    const RosterRecord& R = w.roster[1];
    CHECK(R.rel_a == std::array<std::uint8_t, 5>{6, 1, 0, 0, 0});
    CHECK(R.rel_w == std::array<std::int16_t, 5>{5, 3, 0, 0, 0});
    CHECK(R.rel_r == std::array<std::uint8_t, 5>{2, 0, 0xff, 0xff, 0xff});
    CHECK(I.bee[0] == 1);
    CHECK(I.w72[0] == 3);
    CHECK(I.bee[2] == 6);
    CHECK(I.w72[2] == 5);
}

TEST_CASE("seg14:0x2300 kinds 2, 3 and 5: the Enforcer picks") {
    FakeHost h;
    if (!load_master(h)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    World w;
    base_world(w);
    w.w9332 = 0;
    // Kind 2: PATROLMAN 100 with DGROUP:0x9332 < 2, no rand() past the entry draw.
    is::shell::Rng copy = h.r;
    copy.rand();
    CHECK(pick_design(w, h, 2, 1) == shp_index(h, "PATROLMAN"));
    CHECK(w.w95b4 == 100);
    CHECK(h.r.state() == copy.state());
    // CONSTABLE 50 otherwise.
    w.w9332 = 3;
    CHECK(pick_design(w, h, 2, 1) == shp_index(h, "CONSTABLE"));
    CHECK(w.w95b4 == 50);
    // Kinds 3 and 5: rand() % 30 + 50 per matching record in record order, the first of equals.
    copy = h.r;
    copy.rand();
    const int a = copy.rand() % 30 + 50, b = copy.rand() % 30 + 50;
    const int k3 = pick_design(w, h, 3, 1);
    CHECK(k3 == (b > a ? shp_index(h, "LETHAL WEAPON") : shp_index(h, "INTERCEPTOR")));
    CHECK(h.r.state() == copy.state());
    const int k5 = pick_design(w, h, 5, 1);
    CHECK((k5 == shp_index(h, "TERMINATOR") || k5 == shp_index(h, "ELIMINATOR")));
}

TEST_CASE("seg14:0x0: wave 1 places no computer ship [ORACLE wave-0001]") {
    FakeHost h;
    if (!load_master(h)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    World w;
    base_world(w);
    w.wave = 1;
    w.w9336 = 0;
    w.opponents = 0;
    const RosterBuild b = build_wave_roster(w, h, -1);
    CHECK(b.count == 1);
    CHECK(w.last_ship == 0);
    CHECK(w.pools.in_use(w.roles.ships) == 1);
    REQUIRE(w.roster.size() == 1);
    CHECK(w.roster[0].shp == shp_index(h, "TRANSFORMER"));
    CHECK(design_team(w, 0) == 0);  // seg14:0x3e9: SHP +0x03 of the MainShip
    CHECK(h.bought == std::vector<int>{0});
    CHECK(h.tails == std::vector<int>{0});
}

TEST_CASE("seg14:0x0: wave 2's one teammate, in the original's rand() order") {
    FakeHost h;
    if (!load_master(h)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    World w;
    base_world(w);
    w.wave = 1;
    w.w9336 = 0;
    w.opponents = 0;
    build_wave_roster(w, h, -1);
    // The generator's wave 2: DGROUP:0x9336 = DGROUP:0x9338 = 1 (seg9:0x3d97).
    w.wave = 2;
    w.w9336 = 1;
    w.opponents = 1;
    w.w992 = 0;
    const is::shell::Rng before = h.r;
    const RosterBuild b = build_wave_roster(w, h, -1);
    REQUIRE(b.count == 2);
    CHECK(w.ship(1).roster_index == 1);
    REQUIRE(w.roster.size() == 2);
    const int s = w.roster[1].shp;
    CHECK(h.shp.records[static_cast<std::size_t>(s)].team == 0);  // the player's team (TRANSFORMER: AVENGERS)
    CHECK(design_team(w, w.ship(1).design_index) == 0);
    CHECK(w.ship(1).design_index == 1);
    // The draws: seg14:0x2300(1) one at entry and one per team-0 record, seg14:0x3274(1) one at
    // entry (record 0 is the player's, excluded), seg14:0xdd6 two.
    int team0 = 0;
    for (const auto& r : h.shp.records)
        if (!r.name.empty() && r.team == 0) ++team0;
    is::shell::Rng expect = before;
    for (int k = 0; k < 1 + team0 + 1 + 2; ++k) expect.rand();
    CHECK(h.r.state() == expect.state());
    CHECK(std::string(w.roster[1].name.data()) == "AVENGER 1");
}

TEST_CASE("seg14:0x0: a Patrol call-in (mode 0x16) and a warrant's Terminator") {
    FakeHost h;
    if (!load_master(h)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    World w;
    base_world(w);
    w.wave = 1;
    build_wave_roster(w, h, -1);
    w.wave = 3;
    w.w9332 = 0;
    const RosterBuild b = build_wave_roster(w, h, 0x16);
    CHECK(b.count == 2);
    const ShipRecord& P = w.ship(1);
    CHECK(P.roster_index == -1);
    CHECK(roster_words().enforcer_shp[3] == shp_index(h, "PATROLMAN"));
    CHECK(design_team(w, P.design_index) == 7);
    CHECK(sbyte_at(P, 0x5e) == 2);  // SHP +0x05 of the PATROLMAN (seg14:0x159f)
    CHECK(P.w52 == 0x14);           // seg14:0x1657
    CHECK(P.w56 == 0);
    CHECK(w.ship(1).slots_cb() == 3);  // the no-roster weapons: HANDCUFFS in slot 1
    CHECK(w.ship(1).weapon[1] == 15);
    CHECK(h.bought.back() == 1);  // a call-in runs seg16:0x3784 for the new ship

    // A standing warrant on the player's record brings a Terminator at the next wave start,
    // aimed at the ship holding it (+0x52 = 0x17, +0x56 = seg14:0x4520(DGROUP:0x9346, ...)).
    w.wave = 4;
    w.w9336 = 0;
    w.opponents = 0;
    w.w9346 = 0;
    build_wave_roster(w, h, -1);
    REQUIRE(w.last_ship == 1);
    const ShipRecord& T = w.ship(1);
    CHECK(T.roster_index == -1);
    const int s = roster_words().enforcer_shp[0];
    CHECK((s == shp_index(h, "TERMINATOR") || s == shp_index(h, "ELIMINATOR")));
    CHECK(T.w52 == 0x17);
    CHECK(T.w56 == 0);
    CHECK(T.bed == 1);
    CHECK(w.w9346 == -1);  // the tail clears the warrant [BYTES seg14:0xdbd]
}

TEST_CASE("seg14:0x0 on ships-0001's restored game: AVENGER 1 and FUZZY 2 in the revisited directory") {
    FakeHost h;
    const char* env = std::getenv("IS_ASSETS_DIR");
    if (!load_master(h) || !env || !*env) {
        MESSAGE("skipped: IS_ORIGINAL_DIR or IS_ASSETS_DIR not available");
        return;
    }
    const fs::path save = fs::path(env).parent_path() / "oracle" / "core-0003" / "reg" / "ORACLE3X.sav";
    std::error_code ec;
    if (!fs::exists(save, ec)) {
        MESSAGE("work/oracle/core-0003/reg/ORACLE3X.sav absent: skipped");
        return;
    }
    const auto bytes = read_all(save);
    const auto p = is::fmt::save::parse(bytes, is::fmt::save::Kind::kSav);
    REQUIRE(p);
    World w;
    base_world(w);
    for (const auto& rb : p->roster) w.roster.push_back(roster_from_bytes(rb.data()));
    for (const auto& db : p->dirs) {
        DirRecord d;
        std::copy(db.begin(), db.end(), d.b.begin());
        w.records.push_back(d);
    }
    w.record = p->header.record;
    w.player_roster = p->header.player_roster;
    REQUIRE(w.roster.size() == 3);
    // The save's names are seg9:0x5be0's (record 0, the pilot's, is not read here).
    CHECK(std::string(w.roster[1].name.data()) == "AVENGER 1");
    CHECK(std::string(w.roster[2].name.data()) == "FUZZY 2");
    CHECK(h.shp.records[static_cast<std::size_t>(w.roster[1].shp)].name == "GOLDEN VIPER");
    CHECK(h.shp.records[static_cast<std::size_t>(w.roster[2].shp)].name == "PLATYPUS");
    // The wave-3 placement of the restored game in its directory (DGROUP:0x94b0 = 0): the two known
    // ships come first, so no teammate and no other ship is drawn with DGROUP:0x9336 = 1 and
    // DGROUP:0x9338 = 2 (AVENGER 1 is the player's team and counts as `same`).
    w.wave = 3;
    w.restored = 1;
    w.added = 0;
    w.w9336 = 1;
    w.opponents = 2;
    w.ship(0).design_index = 0;
    set_design_team(w, 0, w.roster[static_cast<std::size_t>(w.player_roster)].team);
    const RosterBuild b = build_wave_roster(w, h, -1);
    CHECK(b.count == 3);  // DGROUP:0x9316 = 2, pool 14 in use 3 [ORACLE ships-0001]
    CHECK(w.ship(1).roster_index == 1);
    CHECK(w.ship(2).roster_index == 2);
    CHECK(design_team(w, w.ship(1).design_index) == 0);  // TEAMMATE
    CHECK(design_team(w, w.ship(2).design_index) == 2);  // FUZZY ONES
    CHECK(w.ship(0).design_index == 0);
    CHECK(w.ship(1).design_index == 1);
    CHECK(w.ship(2).design_index == 2);
}

namespace {

// Two synthetic MASTER.SHP records and a roster of the player (record 0, team 0) and one other
// record 1, for seg14:0x3274's scores [DESIGN: numbers worked from the listing].
void known_world(World& w, FakeHost& h) {
    base_world(w);
    h.shp.records.resize(2);
    h.shp.records[0].name = "A";
    h.shp.records[1].name = "B";
    w.roster.assign(2, RosterRecord{});
    w.player_roster = 0;
    w.roster[0].team = 0;
    w.roster[0].last_wave = 9;
    w.roster[0].met = 7;
    w.roster[0].systems[0] = 100;
    RosterRecord& R = w.roster[1];
    R.shp = 1;
    R.systems[0] = 100;
    R.w06 = 100;
    w.wave = 10;
    w.score_level = 3;
    w.last_ship = 0;
    w.ship(0).roster_index = 0;
    w.ship(0).design_index = 0;
    set_design_team(w, 0, 0);
    roster_words().mode = -1;
}

}  // namespace

TEST_CASE("seg14:0x3274 kind 1: the long term of a ship that played the last wave") {
    World w;
    FakeHost h;
    known_world(w, h);
    h.shp.records[1].to_enemies = 1;
    h.shp.records[1].team = 0;
    RosterRecord& R = w.roster[1];
    R.team = 0;
    R.cls = 3;
    R.last_wave = 9;  // DGROUP:0x91f8 - 1: the first block [BYTES seg14:0x36ab-0x36b9]
    R.run = 2;
    R.relation[0] = 5;  // +0x3c + DGROUP:0x931c
    w.w9332 = 1;
    is::shell::Rng c = h.r;
    c.rand();
    const int x = c.rand() % 30;
    // -10, 42 (e = F), +30 (c = D); R1 = 3: ((32·3)/2 + (32·9)/(-9) + 32)·100/32 = 150, / 9 = 16;
    // + 2·5 [BYTES seg14:0x362d-0x3824]; + rand() % 30 [BYTES seg14:0x3837-0x3d64].
    CHECK(pick_known(w, h, 1, 1) == 1);
    CHECK(w.w95b2 == 42 + 30 + 16 + 10 + x);
    CHECK(h.r.state() == c.state());
}

TEST_CASE("seg14:0x3274 kind 4: the long term of a ship away four waves, the team terms") {
    World w;
    FakeHost h;
    known_world(w, h);
    h.shp.records[1].to_enemies = 3;
    h.shp.records[1].team = 2;
    RosterRecord& R = w.roster[1];
    R.team = 2;
    R.cls = 1;
    R.last_wave = 5;  // the second block: G = 4, Rr = 4 [BYTES seg14:0x3a32-0x3adf]
    R.run = 1;
    R.met = 4;
    R.relation[0] = -3;
    w.w9332 = 2;
    is::shell::Rng c = h.r;
    c.rand();
    const int y = c.rand() % 40;
    // -10 + 50 (|e - F| = 1) - 22 ((c - D)·11) = 18; ((64·4)/4 - 32)·100/32 = 100 against 125,
    // / 10 = 10, + |-3| = 31; N = M = 4: 40 - (89·4)/5 = -31 -> 0; a team-0 slot and no team 2
    // with SHP team 2: +25 [BYTES seg14:0x38a4-0x3d55].
    CHECK(pick_known(w, h, 4, 1) == 1);
    CHECK(w.w95b2 == 25 + y);
    CHECK(h.r.state() == c.state());
    // Run byte 0 in the second block keeps the whole +0x06: 100 / 10 [BYTES seg14:0x3a3a-0x3a3f].
    R.run = 0;
    c = h.r;
    c.rand();
    const int y2 = c.rand() % 40;
    pick_known(w, h, 4, 1);
    CHECK(w.w95b2 == 18 + 10 + 3 - 31 + 25 + y2);
}

namespace {
struct PreloadHost : FakeHost {
    std::vector<int> preloads;
    void preload_sound(int id) override { preloads.push_back(id); }
};
}  // namespace

TEST_CASE("seg14:0x1b84: the object and ship words of every placed ship") {
    World w;
    base_world(w);
    PreloadHost h;
    h.shp.records.resize(1);
    is::fmt::ShipRecord& s = h.shp.records[0];
    s.armor = 34;
    s.thruster = 41;
    s.hit_sound = 7;
    s.body = 0;
    h.bod.records.resize(1);
    is::fmt::BodyRecord& b = h.bod.records[0];
    b.strength_base = 24;
    b.flags = is::fmt::kBodyFlag40 | is::fmt::kBodyShield;
    ShipRecord& S = w.ship(0);
    set_byte_at(S, 0xac, 1);
    set_byte_at(S, 0xad, 1);
    design_fill_portable(w, 0, s, h.bod, 1234, &h);
    const ObjectRecord& o = w.ship_object(0);
    const DesignRating& d = w.design_ratings[0];
    CHECK(o.max_damage == 200);                 // [BYTES seg14:0x20f5]
    CHECK(o.radar_brush == 7);                  // [BYTES seg14:0x1e08 seg14:0x20e6]
    CHECK(o.on_damage.get() == kDamageShip);    // [BYTES seg14:0x2158]
    CHECK(o.on_destroy.get() == kDestroyShip);  // [BYTES seg14:0x212a]
    CHECK(o.radar_draw.get() == kRadarShip);    // [BYTES seg14:0x2141]
    CHECK(o.hit_sound == 7);
    CHECK(d.w4e == 1);                          // [BYTES seg14:0x1b98]
    CHECK(d.blast == d.mass / 22);              // [BYTES seg14:0x1e93-0x1e99]
    CHECK(o.blast_size == d.blast);
    CHECK(S.anim_wrap == 1);                    // body flag 0x40 [BYTES seg14:0x1f71 seg14:0x21e6]
    CHECK(S.be5 == 0);
    CHECK(sbyte_at(S, 0xac) == 0);              // no Pull-off Wings [BYTES seg14:0x1de4-0x1dff]
    CHECK(h.preloads == std::vector<int>{7, 5});  // [BYTES seg14:0x1e2d seg14:0x1fb9]
    // +0xa3 = 6 for Pull-off Wings, and a later body without it keeps the 6 [BYTES seg14:0x1f1f-0x1f2b].
    b.flags = is::fmt::kBodyPullOffWings;
    design_fill_portable(w, 0, s, h.bod, 1234, &h);
    CHECK(w.design_ratings[0].b_a3 == 6);
    CHECK(S.be5 == 1);
    CHECK(S.anim_wrap == 0);
    b.flags = 0;
    design_fill_portable(w, 0, s, h.bod, 1234, &h);
    CHECK(w.design_ratings[0].b_a3 == 6);
    CHECK(S.be5 == 1);
}

TEST_CASE("seg14:0x0 in a new directory of ships-0001's restored game: two other-team ships") {
    FakeHost h;
    const char* env = std::getenv("IS_ASSETS_DIR");
    if (!load_master(h) || !env || !*env) {
        MESSAGE("skipped: IS_ORIGINAL_DIR or IS_ASSETS_DIR not available");
        return;
    }
    const fs::path save = fs::path(env).parent_path() / "oracle" / "core-0003" / "reg" / "ORACLE3X.sav";
    std::error_code ec;
    if (!fs::exists(save, ec)) {
        MESSAGE("work/oracle/core-0003/reg/ORACLE3X.sav absent: skipped");
        return;
    }
    const auto bytes = read_all(save);
    const auto p = is::fmt::save::parse(bytes, is::fmt::save::Kind::kSav);
    REQUIRE(p);
    World w;
    base_world(w);
    for (const auto& rb : p->roster) w.roster.push_back(roster_from_bytes(rb.data()));
    for (const auto& db : p->dirs) {
        DirRecord d;
        std::copy(db.begin(), db.end(), d.b.begin());
        w.records.push_back(d);
    }
    w.record = p->header.record;
    w.player_roster = p->header.player_roster;
    // The tools run of ships-0001 (a new directory, DGROUP:0x94b0 != 0): no known ship; with no
    // teammate and DGROUP:0x9338 = 2 the other-teams loop places ships 1 and 2, each a known
    // record of another team or a new one named after its team and index (PREDATOR 3 there)
    // [ORACLE ships-0001] [BYTES seg14:0x485-0x48a seg14:0x723-0x795].
    w.wave = 3;
    w.restored = 1;
    w.added = 1;
    w.w9336 = 0;
    w.opponents = 2;
    w.ship(0).design_index = 0;
    set_design_team(w, 0, w.roster[static_cast<std::size_t>(w.player_roster)].team);
    const RosterBuild b = build_wave_roster(w, h, -1);
    REQUIRE(b.count == 3);
    for (int i = 1; i <= 2; ++i) {
        const int r = w.ship(i).roster_index;
        REQUIRE(r >= 0);
        REQUIRE(r < static_cast<int>(w.roster.size()));
        const RosterRecord& R = w.roster[static_cast<std::size_t>(r)];
        CHECK(R.team != 0);
        CHECK(R.team != 7);
        CHECK(design_team(w, w.ship(i).design_index) == R.team);
        const std::string name(R.name.data());
        CHECK(name.size() > 2);
        CHECK(name.substr(name.rfind(' ') + 1) == std::to_string(r));
    }
    CHECK(w.ship(1).roster_index != w.ship(2).roster_index);
}

TEST_CASE("seg14:0x31d0: the demo's random MainShip in seg14:0x2300's entry") {
    FakeHost h;
    if (!load_master(h)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    World w;
    base_world(w);
    w.w9516 = 1;
    // The listing's loop on a copy of the stream: the entry draw, c = rand() % 20 + 10, then
    // v = rand() % count until c has counted down and v is named and not team 7 [BYTES
    // seg14:0x2310 seg14:0x31e3-0x3253].
    is::shell::Rng c = h.r;
    c.rand();
    int n = c.rand() % 20 + 10;
    const int count = static_cast<int>(h.shp.records.size());
    int v = 0;
    for (;;) {
        v = c.rand() % count;
        const bool ok = !h.shp.records[static_cast<std::size_t>(v)].name.empty() &&
                        h.shp.records[static_cast<std::size_t>(v)].team != 7;
        if (n > 0) --n;
        if (n == 0 && ok) break;
    }
    CHECK(pick_design(w, h, 0, 0) == v);
    CHECK(w.w95b4 == 20000);
    CHECK(h.r.state() == c.state());
}
