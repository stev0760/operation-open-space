// Damage (src/world/damage.h): the ship's on_damage seg8:0x45e0 and its wings and thruster
// rules, the rock's on_damage seg7:0x5090 and the break-up, and the auto-repair seg11:0x1d7c
// with core-0001's repair steps [ORACLE core-0001].
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

#include "doctest.h"
#include "formats/ships.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/damage.h"
#include "world/design_words.h"
#include "world/rating.h"

using namespace is::world;
namespace fs = std::filesystem;

namespace {

class Calls : public CollideHost {
public:
    std::vector<int> sounds;
    std::vector<std::pair<int, int>> destroyed, ships_down, wings, in_use;
    std::vector<int> flames;
    bool debris_free = true;
    std::vector<int> cuts, debris_asked;
    void sound(int id, int, int, int) override { sounds.push_back(id); }
    void destroy(int pool, int rec) override { destroyed.push_back({pool, rec}); }
    void ship_destroyed(int pool, int ship) override { ships_down.push_back({pool, ship}); }
    // seg8:0x2cd8's picture part: `wings` keeps (ship, side) with side = 1 - cut (M4 s5 SHEET).
    bool wing_picture(int ship, int cut, bool make_debris) override {
        wings.push_back({ship, cut != 0 ? 0 : 1});
        cuts.push_back(cut);
        debris_asked.push_back(make_debris ? 1 : 0);
        return make_debris && debris_free;
    }
    void flame_off(int ship) override { flames.push_back(ship); }
    void seg7_2080(int pool, int rec) override { in_use.push_back({pool, rec}); }
};

struct Rig {
    World w;
    WaveGlobals g;
    CollideGlobals c;
    Calls h;
    ViewState view;
    is::shell::Rng rng;
    std::int16_t dg_943a = kPoolRocks, dg_943c = 0, dg_9454 = 0;
    CollideCtx x{w, g, c, h, view, nullptr, rng, dg_943a, dg_943c, dg_9454};

    Rig() {
        w.size = 1;
        w.shift = 3;
        w.player = 0;
        w.last_ship = 0;
        w.w9000 = 0;
        w.player_game = 1;
        DesignRating& d = w.design_ratings[0];
        d.armor4a = 8, d.armor5a = 6, d.armor4b = 8, d.armor5b = 6;  // TRANSFORMER (SHP Armor 34)
        d.special = 3;  // body Pull-off Wings (MASTER.BOD flags 0x06) [BYTES seg14:0x1d8c]
        d.repair = 1;
        d.tank = 40 * 33;
        // Design words +0x2a (the raw side 33) and +0x3c (D 5) as the sheet build and the set-up
        // write them (design_words.h) [ORACLE core-0001].
        reset_design_words();
        DesignWordSet dw;
        dw.w2a = 33;
        dw.w3c = 5;
        set_design_words(0, dw);
        w.roster.assign(1, RosterRecord{});
        g.now = 100000;
        g.size = 1;
        w.pools.set_in_use(kPoolShips, 1);
        w.pools.set_in_use(kPoolRocks, 1);
        ShipRecord& s = w.ships[0];
        s.bec = 1;
        s.roster_index = 0;
        s.fuel = 0x528;
        s.w26 = 82, s.w28 = 81, s.w2a = 82, s.w2c = 81;
        ObjectRecord& p = w.pools.rec(kPoolShips, 0);
        p.mass = 160;
        p.size = 33;
        p.hit_sound = 7;
        ObjectRecord& r = w.pools.rec(kPoolRocks, 0);
        r.size = 44;
        r.mass = 1400;
        r.max_damage = 1000;
    }
    ShipRecord& ship() { return w.ships[0]; }
};

}  // namespace

TEST_CASE("damage: seg8:0x45e0, the amount that gives core-0001's hit 5 [ORACLE core-0001]") {
    // Engine and Thrust lose d/8, Gun and Shield d/6 (the TRANSFORMER's divisors): 82, 81, 82, 81
    // -> 76, 72, 76, 72 needs d in {54, 55}. Backwards through the steps: d = 2·d3 (no shield,
    // no cloak) so d3 = 27; d3 = d2·3/4 so d2 in {36, 37}; d2 = d1·6/8 (Complex 0:
    // DGROUP:0x9000 = 0) so d1 in {48, 49, 50}; d1 = a·7/8 so a in {55, 56, 57, 58}.
    for (std::int16_t a = 54; a <= 59; ++a) {
        Rig r;
        CHECK(ship_damage(r.x, kPoolShips, 0, a) == 0);
        const bool hit5 = r.ship().w26 == 76 && r.ship().w28 == 72 && r.ship().w2a == 76 && r.ship().w2c == 72;
        CHECK(hit5 == (a >= 55 && a <= 58));
    }
    Rig r;
    ship_damage(r.x, kPoolShips, 0, 56);
    CHECK(r.w.pools.rec(kPoolShips, 0).damage == 54);
    REQUIRE(r.h.sounds.size() == 1);
    CHECK(r.h.sounds[0] == 7);  // seg10:0x7944 with SHP +0x1f = 7 (DGROUP:0xb0a = 0)
    // The player-game rating loss min(rating, 35000)·d / -9000 [BYTES seg8:0x473f-0x476c]
    Rig q;
    q.w.rating = 90000;
    ship_damage(q.x, kPoolShips, 0, 56);  // d = 36 at the rating step
    CHECK(q.w.rating == 90000u - 140u);   // 35000·36 / -9000 = -140
}

TEST_CASE("damage: the smaller hits of core-0001: equal pairs of losses [ORACLE core-0001]") {
    Rig r;
    r.ship().w26 = 83, r.ship().w28 = 83, r.ship().w2a = 83, r.ship().w2c = 82;
    ship_damage(r.x, kPoolShips, 0, 20);  // 20 -> 17 -> 12 -> 9 -> 18: 18/8 = 2, 18/6 = 3
    CHECK(r.ship().w26 == 81);
    CHECK(r.ship().w28 == 80);
    CHECK(r.ship().w2a == 81);
    CHECK(r.ship().w2c == 79);
    Rig s;
    ship_damage(s.x, kPoolShips, 0, 8);  // the loss is at least 1 for each lamp
    CHECK(s.ship().w26 == 81);
    CHECK(s.ship().w2c == 80);
}

TEST_CASE("damage: the side word, the shield and cloak factors, the dead ship") {
    Rig r;
    r.ship().bea = 1;  // 4d/3
    ship_damage(r.x, kPoolShips, 0, 56);
    CHECK(r.w.pools.rec(kPoolShips, 0).damage == 36);
    Rig c;
    c.ship().be8 = 1;  // 3d
    ship_damage(c.x, kPoolShips, 0, 56);
    CHECK(c.w.pools.rec(kPoolShips, 0).damage == 81);
    // A dead ship hit in the last 5.5 s of its tumble explodes at once [BYTES seg8:0x463f-0x4662]
    Rig d;
    d.ship().bec = 0;
    d.ship().lockout.set(d.g.now + 5000u);
    CHECK(ship_damage(d.x, kPoolShips, 0, 56) == 0);
    REQUIRE(d.h.destroyed.size() == 1);
    CHECK(d.h.destroyed[0].first == kPoolShips);
    d.ship().lockout.set(d.g.now + 6000u);
    ship_damage(d.x, kPoolShips, 0, 56);
    CHECK(d.h.destroyed.size() == 1);
}

TEST_CASE("damage: the thruster out turns the flame off; engine 0 calls on_destroy") {
    Rig r;
    r.ship().w2a = 3;
    r.ship().thrusting = 1;
    ship_damage(r.x, kPoolShips, 0, 56);
    CHECK(r.ship().w2a == 0);
    CHECK(r.ship().thrusting == 0);
    REQUIRE(r.h.flames.size() == 1);
    Rig d;
    d.ship().w26 = 5;
    CHECK(ship_damage(d.x, kPoolShips, 0, 56) == 1);
    REQUIRE(d.h.ships_down.size() == 1);
    CHECK(d.h.ships_down[0].second == 0);
    // Its own collision keeps it flying at engine 0 [BYTES seg8:0x4ca4-0x4cb5]
    Rig o;
    o.ship().w26 = 5;
    o.dg_943a = kPoolShips;
    o.dg_943c = 0;
    CHECK(ship_damage(o.x, kPoolShips, 0, 56) == 0);
    CHECK(o.h.ships_down.empty());
}

TEST_CASE("damage: the wings of a wing ship, core-0001's second wing [ORACLE core-0001]") {
    Rig r;
    set_byte_at(r.ship(), 0xac, 1);  // one wing left
    r.ship().w26 = 57, r.ship().w28 = 42, r.ship().w2a = 57, r.ship().w2c = 6;
    r.ship().w20 = -2;
    r.ship().w22 = 1;
    r.ship().w24 = 2;
    // core-0001 at 23:12:51.626: 57, 42, 57, 6 -> 53, 36, 53, 0, so d/8 = 4 and d/6 = 6, d in
    // {36 ... 39}: a in {37 ... 40} by the steps of the hit-5 case; a = 38: 33, 24, 18, 36
    ship_damage(r.x, kPoolShips, 0, 38);
    CHECK(r.ship().w26 == 53);
    CHECK(r.ship().w28 == 36);
    CHECK(r.ship().w2a == 53);
    CHECK(r.ship().w2c == 0);
    // The last wing torn: +0x20 = design +0x2a - 14 = 33 - 14 = 19, +0x22 = 7 [ORACLE core-0001]
    CHECK(sbyte_at(r.ship(), 0xac) == 0);
    CHECK(r.ship().w20 == 19);
    CHECK(r.ship().w22 == 7);
    CHECK(r.ship().w24 == 1);
    CHECK(sbyte_at(r.ship(), 0xe0) == 1);
    REQUIRE(r.h.wings.size() == 1);
    CHECK(r.h.wings[0].second == 0);
    // 0x07, then the debris' 0x6a (first slot 0x20), before the rock's 0x08 [ORACLE core-0001]
    REQUIRE(r.h.sounds.size() == 2);
    CHECK(r.h.sounds[0] == 7);
    CHECK(r.h.sounds[1] == 0x6a);
    CHECK(r.ship().due156.get() == r.g.now - 10000u);
    // Below 50 with both wings on, the hit side's wing goes: object +0x26 = 1 tears side 1
    Rig b;
    set_byte_at(b.ship(), 0xac, 1);
    set_byte_at(b.ship(), 0xad, 1);
    b.ship().w2c = 52;
    ship_damage(b.x, kPoolShips, 0, 56);  // 52 - 9 = 43
    REQUIRE(b.h.wings.size() == 1);
    const int side = b.h.wings[0].second;
    CHECK(sbyte_at(b.ship(), side != 0 ? 0xad : 0xac) == 0);
    CHECK(b.w.pools.rec(kPoolShips, 0).angle == 0);
}

TEST_CASE("damage: seg7:0x5090, heat, damage and the break into three small rocks") {
    Rig r;
    ObjectRecord& rock = r.w.pools.rec(kPoolRocks, 0);
    rock.flame = 7;
    rock.x = 8000, rock.y = 8000;
    rock.scr_x = 300, rock.scr_y = 200;
    rock.vx = 5, rock.vy = -3;
    // B of the last collision: the player
    r.c.b_pool = kPoolShips;
    r.c.b_rec = 0;
    // Below max_damage: 3d/2 twice, the heat by d/4 at most 9, the 3000 ms cooling deadline
    CHECK(rock_damage(r.x, kPoolRocks, 0, 10) == 0);
    CHECK(rock.damage == 30);
    CHECK(static_cast<std::int8_t>(rock.flame) == 9);
    CHECK(rock.d6a.get() == r.g.now + 3000u);
    // Over max_damage with the heat above 6: the big rock splits
    rock.damage = 990;
    CHECK(rock_damage(r.x, kPoolRocks, 0, 10) == 1);
    REQUIRE(r.h.sounds.size() == 1);
    CHECK(r.h.sounds[0] == 0x45);
    // The lowest records with state != 0: the parent's own (0), then new records 1 and 2
    const ObjectRecord& s0 = r.w.pools.rec(kPoolRocks, 0);
    CHECK(s0.state == 0);
    CHECK(s0.mass == 350);
    CHECK(s0.extent == 168);
    CHECK(s0.max_damage == 300);
    CHECK(s0.damage == 0);
    CHECK(static_cast<std::int8_t>(s0.flame) == 9);
    CHECK(s0.size == 22);
    CHECK(s0.x == 8000);
    CHECK(s0.y == 8000 - 80);
    CHECK(s0.vx == 5);  // left from the reused record
    // Records 1 ... 9 exist in the startup pool with state 0 (zeroed): the search skips them
    // and grows the pool at 10 and 11
    REQUIRE(r.w.pools.records(kPoolRocks) >= 12);
    const ObjectRecord& s1 = r.w.pools.rec(kPoolRocks, 10);
    const ObjectRecord& s2 = r.w.pools.rec(kPoolRocks, 11);
    CHECK(s1.state == 0);
    CHECK(s1.x == 8000 + 120);
    // The parent is read again for each small rock, and the first one reused it: its y moved by
    // -80 already [BYTES seg7:0x5223-0x525e]
    CHECK(s1.y == 8000 - 80 + 96);
    CHECK(s1.hit_sound == 8);
    CHECK(s1.owner_pool == -1);
    CHECK(s1.on_damage.get() == kDamageRock);
    CHECK(s2.x == 8000 - 120);
    CHECK(s2.y == 8000 - 80 + 104);
    CHECK(r.w.pools.in_use(kPoolRocks) == 12);
    // A small rock over its max_damage at heat > 6 goes through seg12:0x274a
    ObjectRecord& sm = r.w.pools.rec(kPoolRocks, 11);
    sm.damage = 299;
    CHECK(rock_damage(r.x, kPoolRocks, 11, 4) == 1);
    REQUIRE(r.h.destroyed.size() == 1);
    CHECK(r.h.destroyed[0].second == 11);
    REQUIRE(!r.h.in_use.empty());
    CHECK(r.h.in_use.back().second == -1);
}

TEST_CASE("damage: seg7:0x5090's guard and the cool rock that does not break") {
    Rig r;
    ObjectRecord& rock = r.w.pools.rec(kPoolRocks, 0);
    rock.flame = 2;
    rock.damage = 995;
    r.c.b_pool = kPoolShips;
    r.c.b_rec = 0;
    CHECK(rock_damage(r.x, kPoolRocks, 0, 10) == 0);  // heat 2 before the hit: no break
    CHECK(rock.damage == 995 + 30);
    // The last collision's B party beyond its pool's in-use count: nothing (s1-hit-verify.md 25)
    r.c.b_rec = 5;
    CHECK(rock_damage(r.x, kPoolRocks, 0, 10) == 0);
    CHECK(rock.damage == 995 + 30);
}

TEST_CASE("damage: seg11:0x1d7c, core-0001's repair steps [ORACLE core-0001]") {
    Rig r;
    Calls h;
    RepairState st;
    ShipRecord& s = r.ship();
    s.w26 = 81, s.w28 = 80, s.w2a = 81, s.w2c = 79;
    r.w.frames = 1;
    auto_repair(r.w, st, h);  // the first tick reloads the countdown: 9 - 160/75 = 7
    CHECK(st.countdown[0] == 7);
    CHECK(st.due == 181u);
    CHECK(s.w26 == 81);
    r.w.frames = 181;
    auto_repair(r.w, st, h);  // not due: frame# must exceed the deadline
    CHECK(s.w26 == 81);
    r.w.frames = 182;
    auto_repair(r.w, st, h);
    // +2, +2, +2 for the player's first three lamps, +1 for the fourth, 2 fuel each
    CHECK(s.w26 == 83);
    CHECK(s.w28 == 82);
    CHECK(s.w2a == 83);
    CHECK(s.w2c == 80);
    CHECK(s.fuel == 0x520);
    CHECK(st.countdown[0] == 6);
    CHECK(s.d8c.get() == 1u);  // max(27, 4·(75 - 82)) / 27
    r.w.frames = 400;
    auto_repair(r.w, st, h);
    CHECK(s.w2c == 81);
    CHECK(s.fuel == 0x51e);
    r.w.frames = 600;
    auto_repair(r.w, st, h);
    CHECK(s.w2c == 82);
    r.w.frames = 800;
    auto_repair(r.w, st, h);  // every lamp at 82 or 83: repair stops
    CHECK(s.w2c == 82);
    CHECK(s.w26 == 83);
    CHECK(s.fuel == 0x51c);
    // Lamp 3 at 50 or 0 is not repaired; fuel 90 or less stops the repair
    s.w2c = 50;
    s.w26 = 60;
    r.w.frames = 1000;
    auto_repair(r.w, st, h);
    CHECK(s.w2c == 50);
    CHECK(s.w26 == 62);
    s.fuel = 90;
    r.w.frames = 1200;
    auto_repair(r.w, st, h);
    CHECK(s.w26 == 62);
}

TEST_CASE("damage golden: the shipped TRANSFORMER's divisors are 8, 6, 8, 6") {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !fs::is_directory(v)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const auto read = [](const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
    };
    const auto bod_bytes = read(fs::path(v) / "MASTER.BOD");
    const auto shp_bytes = read(fs::path(v) / "MASTER.SHP");
    const is::fmt::MasterBod bod = is::fmt::parse_master_bod(bod_bytes.data(), bod_bytes.size());
    const is::fmt::MasterShp shp = is::fmt::parse_master_shp(shp_bytes.data(), shp_bytes.size());
    const is::fmt::ShipRecord* t = nullptr;
    for (const auto& s : shp.records)
        if (s.name == "TRANSFORMER") t = &s;
    REQUIRE(t != nullptr);
    REQUIRE(t->body < bod.records.size());
    CHECK(t->armor == 34);
    CHECK(t->hit_sound == 7);
    CHECK(t->fuel_tank == 33);
    World w;
    w.design_ratings[0] = design_rating(*t, bod.records[t->body]);
    // design +0x3c from BOD +0x1a (10) at Size 1 [BYTES seg14:0x2070-0x208b]; +0x2a is the sheet
    // build's (tests/test_world_sheet.cpp checks the body heights).
    reset_design_words();
    DesignWordSet dw;
    dw.w3c = design_band(bod.records[t->body].byte_1a, 1);
    set_design_words(0, dw);
    const DamageDesign d = design_from_world(w, 0);
    CHECK(d.hit_div[0] == 8);
    CHECK(d.hit_div[1] == 6);
    CHECK(d.hit_div[2] == 8);
    CHECK(d.hit_div[3] == 6);
    CHECK(d.lamp_row[3] == 3);  // Pull-off Wings: BOD flags 0x06 [BYTES seg14:0x1d8c]
    CHECK(d.repair == 1);
    CHECK(bod.records[t->body].byte_1a == 10);
    CHECK(d.w3c == 5);
    reset_design_words();
}

TEST_CASE("damage: seg8:0x2cd8 runs the picture once, 0x6a only after a debris entry (M4 s5 SHEET)") {
    // work/analysis/m4/s5-sheet.md 1.1-1.2 (verified claims 1, 2, 4): side 0 tears +0xac with
    // ordinal 15's [bp-4] = 1, the recompute of design +0x3c while DGROUP:0x95a6 = 0, one picture
    // call, and 0x6a only when it set up a debris entry.
    Rig r;
    set_design_body(0, 10, 4, 6);
    DesignWordSet d = design_words(0);
    d.w3c = 0;
    set_design_words(0, d);
    set_byte_at(r.ship(), 0xac, 1);
    set_byte_at(r.ship(), 0xad, 1);
    tear_wing(r.x, 0, 0);
    CHECK(sbyte_at(r.ship(), 0xac) == 0);
    CHECK(sbyte_at(r.ship(), 0xad) == 1);
    REQUIRE(r.h.cuts.size() == 1);
    CHECK(r.h.cuts[0] == 1);
    CHECK(r.h.debris_asked[0] == 1);
    CHECK(design_words(0).w3c == 5);
    REQUIRE(r.h.sounds.size() == 1);
    CHECK(r.h.sounds[0] == 0x6a);
    tear_wing(r.x, 0, 1);
    REQUIRE(r.h.cuts.size() == 2);
    CHECK(r.h.cuts[1] == 0);
    CHECK(sbyte_at(r.ship(), 0xad) == 0);
    // No free entry: the cut alone, no sound.
    Rig f;
    f.h.debris_free = false;
    tear_wing(f.x, 0, 1);
    CHECK(f.h.cuts.size() == 1);
    CHECK(f.h.sounds.empty());
    // A restored game (DGROUP:0x92e4 != 0): no debris asked, no sound.
    Rig g;
    g.w.restored = 1;
    tear_wing(g.x, 0, 0);
    REQUIRE(g.h.debris_asked.size() == 1);
    CHECK(g.h.debris_asked[0] == 0);
    CHECK(g.h.sounds.empty());
    // With the MASTER.SHP block loaded the recompute does not run.
    Rig h;
    h.c.w95a6 = 1;
    set_design_body(0, 10, 4, 6);
    d = design_words(0);
    d.w3c = 9;
    set_design_words(0, d);
    tear_wing(h.x, 0, 0);
    CHECK(design_words(0).w3c == 9);
    reset_design_words();
}
