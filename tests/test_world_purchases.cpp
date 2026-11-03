// The purchase-applier seg16:0x3784 and its helpers (src/world/purchases.h): the repair
// seg16:0x3468, the reload seg16:0x3730, the refuel seg16:0x3e80, DOUBLE GUNS seg16:0x3ece, UNIFY
// FORCES seg16:0x3f88, a powerup seg16:0x4030 and the mode-1 pass of the wave set-up
// (work/analysis/m4/s3-visit.md section 4 with s3-visit-verify.md).
#include <string>
#include <vector>

#include "doctest.h"
#include "hud/weapons.h"
#include "world/purchases.h"
#include "world/world.h"

using namespace is::world;

namespace {

// The trial's TRANSFORMER after core-0003's flail: systems 74, 61, 74, 58, fuel 1121 of 1320,
// credit 1.50, RADAR-GUIDED MISSILES (8) with 20 in slot 1 [ORACLE core-0003].
struct Rig {
    World w;
    Shop s;
    WeaponWords W;
    struct Host final : WorldPurchaseHost {
        using WorldPurchaseHost::WorldPurchaseHost;
        std::vector<std::pair<int, int>> loads;
        std::vector<int> sheets, rates;
        int paints = 0;
        void load_weapon(int ship, int wpn) override { loads.push_back({ship, wpn}); }
        void build_sheet(int ship) override { sheets.push_back(ship); }
        bool sheet_dc(int) override { return true; }
        void rate(int ship) override { rates.push_back(ship); }
        void post_info_paint() override { ++paints; }
        int game_state() override { return state; }
        int state = 6;
    } h{w};
    Rig() {
        const is::hud::WeaponTable t(0);
        for (int k = 0; k < kWeaponTableBlocks; ++k) W.block(k) = t.block(k);
        w.player = 0;
        w.w952e = 0;
        w.design_ratings[0].tank = 1320;
        w.design_ratings[0].special = 3;  // the Pull-off Wings lamp row
        ShipRecord& S = w.ship(0);
        S.design_index = 0;
        S.w26 = 74;
        S.w28 = 61;
        S.w2a = 74;
        S.w2c = 58;
        S.fuel = 1121;
        set_credit(S, 150);
        S.w0a = 1;
        S.slots_cb() = 2;
        S.weapon[1] = 8;
        S.weapon_count[1] = 20;
        S.roster_index = 0;
        set_byte_at(S, 0xac, 1);
        set_byte_at(S, 0xad, 1);
        w.roster.resize(1);
    }
    PurchaseCtx ctx() { return PurchaseCtx{w, s, W, h, true, 0, 0}; }
};

}  // namespace

TEST_CASE("seg16:0x3468: the repair sets 100 and returns the cost rounded down to tens") {
    Rig r;
    PurchaseCtx c = r.ctx();
    CHECK(repair_system(c, 0, 0) == 20);  // 100 - 74 = 26 -> 20
    CHECK(r.w.ship(0).w26 == 100);
    CHECK(repair_system(c, 0, 1) == 30);  // 39 -> 30
    CHECK(r.w.ship(0).d8c.get() == 2);    // k = 1: the gun interval
    CHECK(sbyte_at(r.w.ship(0), 0xec) == 1);
    // DGROUP:0x92e4 != 0 leaves the word (verified claim 89).
    c.restored = 1;
    CHECK(repair_system(c, 0, 2) == 20);
    CHECK(r.w.ship(0).w2a == 74);
}

TEST_CASE("seg16:0x3468: the early return of a design row -1 (verified claim 89, CORRECTED)") {
    Rig r;
    r.w.design_ratings[0].special = -1;
    PurchaseCtx c = r.ctx();
    // k = 3 with a sheet DC: return 0, nothing written.
    CHECK(repair_system(c, 0, 3) == 0);
    CHECK(r.w.ship(0).w2c == 58);
    // With the new-body flag the test is skipped.
    r.s.new_body = 1;
    CHECK(repair_system(c, 0, 3) == 40);
}

TEST_CASE("seg16:0x3730: the reload fills 20 and marks slot 0's ammo 0xff") {
    Rig r;
    PurchaseCtx c = r.ctx();
    r.w.ship(0).weapon_count[1] = 13;
    CHECK(reload_slot(c, 0, 1) == 70);
    CHECK(r.w.ship(0).weapon_count[1] == 20);
    CHECK(r.w.ship(0).weapon_count[0] == 0xff);
    CHECK(reload_slot(c, 0, 0) == 0);  // 0xff: nothing (verified claim 87, CORRECTED)
}

TEST_CASE("seg16:0x3e80: the refuel to the ship's own tank") {
    Rig r;
    PurchaseCtx c = r.ctx();
    CHECK(refuel_ship(c, 0) == 20);  // (1320 - 1121)/7 = 28 -> 20
    CHECK(r.w.ship(0).fuel == 1320);
}

TEST_CASE("seg16:0x3ece: DOUBLE GUNS sets +0x0a, the barrels and the gun flags") {
    Rig r;
    PurchaseCtx c = r.ctx();
    CHECK(double_guns(c, 0) == 250);
    CHECK(r.w.ship(0).w0a == 2);
    CHECK(r.w.ship(0).w24 == 2);
    CHECK(sbyte_at(r.w.ship(0), 0xe0) == 1);
    CHECK(sbyte_at(r.w.ship(0), 0xe1) == 1);
    CHECK(double_guns(c, 0) == 0);  // already 2
    // Both wings off with lamp row 3 = 3: one barrel, flag 0 on.
    Rig q;
    set_byte_at(q.w.ship(0), 0xac, 0);
    set_byte_at(q.w.ship(0), 0xad, 0);
    PurchaseCtx d = q.ctx();
    CHECK(double_guns(d, 0) == 250);
    CHECK(q.w.ship(0).w24 == 1);
    CHECK(sbyte_at(q.w.ship(0), 0xe0) == 1);
    CHECK(sbyte_at(q.w.ship(0), 0xe1) == 0);
}

TEST_CASE("seg16:0x3f88: UNIFY on the player removes 24 and 25, -2 even when absent") {
    Rig r;
    ShipRecord& P = r.w.ship(0);
    P.slots_cb() = 4;
    P.weapon[2] = 24;
    P.weapon[3] = 25;
    P.weapon_count[3] = 7;
    PurchaseCtx c = r.ctx();
    CHECK(unify_forces(c, 0) == 250);
    CHECK(P.weapon[1] == 8);
    CHECK(P.weapon[2] == 0);
    CHECK(P.slots_cb() == 2);
    REQUIRE(r.h.loads.size() == 1);
    CHECK(r.h.loads[0] == std::pair<int, int>{0, 26});
    // Neither present: the count still drops by 2 (verified claim 84).
    unify_forces(c, 0);
    CHECK(P.slots_cb() == 0);
}

TEST_CASE("seg16:0x4030: a powerup's byte and its price") {
    Rig r;
    PurchaseCtx c = r.ctx();
    CHECK(powerup(c, 0, 0) == 400);  // b 0 -> 1: (1 + 3)·100
    CHECK(powerup(c, 0, 2) == 200);  // (1 + 1)·100
    CHECK(r.h.roster_byte(0, 0x32) == 1);
    CHECK(r.h.roster_byte(0, 0x34) == 1);
}

TEST_CASE("seg16:0x3784(player, 0): the player pays nothing here; the flags clear") {
    Rig r;
    PurchaseCtx c = r.ctx();
    r.w.ship(0).set_resources(100);
    r.s.basic = 1;
    r.s.refuel = 1;
    r.s.alive = 0;
    r.s.sold_total = 30;
    apply_purchases(c, 0, 0);
    const ShipRecord& S = r.w.ship(0);
    CHECK(credit_of(S) == 250);  // credit += resources, then restored after the helpers
    CHECK(S.resources() == 0);
    CHECK(S.w26 == 100);
    CHECK(S.w28 == 100);
    CHECK(S.w2a == 100);
    CHECK(S.w2c == 58);   // no wing flag
    CHECK(S.fuel == 1320);
    CHECK(r.s.basic == 0);
    CHECK(r.s.refuel == 0);
    CHECK(r.s.alive == 1);       // the dword write DGROUP:0x9e5b = 1 ...
    CHECK(r.s.sold_total == 0);  // ... clears DGROUP:0x9e5d (verified claim 82)
    CHECK(r.h.paints == 0);      // state 6, not 1
}

TEST_CASE("seg16:0x3784: a bought weapon is loaded or reloaded, a sold one removed") {
    Rig r;
    PurchaseCtx c = r.ctx();
    ShipRecord& S = r.w.ship(0);
    S.weapon_count[1] = 12;
    r.s.bought[8] = 1;   // owned: reloaded
    r.s.bought[4] = 1;   // not owned: seg12:0x6d64
    apply_purchases(c, 0, 0);
    CHECK(S.weapon_count[1] == 20);
    REQUIRE(r.h.loads.size() == 1);
    CHECK(r.h.loads[0] == std::pair<int, int>{0, 4});
    r.s.sold[8] = 1;
    apply_purchases(c, 0, 0);
    CHECK(S.weapon[1] == 0);
    CHECK(S.slots_cb() == 1);
}

TEST_CASE("seg14:0xd32's mode-1 pass repairs and refuels at no cost") {
    Rig r;
    r.h.state = 1;
    wave_setup_purchases(r.w, r.h, r.W, 0, true, 0, 0);
    const ShipRecord& S = r.w.ship(0);
    CHECK(S.w26 == 100);
    CHECK(S.w2c == 100);  // the wing repair always runs in mode 1
    CHECK(S.fuel == 1320);
    CHECK(credit_of(S) == 150);
    CHECK(r.h.paints == 1);  // state 1 and the player: WM_PAINT to the IS_INFO panels
}

TEST_CASE("seg16:0x3b18: the player's own shop in a player game returns before any draw") {
    Rig r;
    PurchaseCtx c = r.ctx();
    r.s.basic = 1;
    computer_shop(c, 0, 0);
    CHECK(r.s.basic == 0);  // the clears run first
}
