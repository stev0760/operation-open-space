// seg12:0x6d64, a weapon into a new slot (src/world/weapon_load.h): the vectors a-f of
// work/analysis/m4/s5-weapons.md section 3 (verified claims 19-21), the capture host's and the
// purchase host's defaults, and core-0006's visits [ORACLE core-0006].
#include <vector>

#include "doctest.h"
#include "hud/weapons.h"
#include "world/capture.h"
#include "world/missile.h"
#include "world/purchases.h"
#include "world/weapon_load.h"
#include "world/world.h"

using namespace is::world;

namespace {

WeaponWords table_words() {
    WeaponWords W;
    const is::hud::WeaponTable t(0);
    for (int k = 0; k < kWeaponTableBlocks; ++k) W.block(k) = t.block(k);
    return W;
}

// Ship 0 with the slots given (weapon, count) and +0xcb their number.
void slots(World& w, const std::vector<std::pair<int, int>>& s) {
    ShipRecord& r = w.ship(0);
    r.weapon.fill(0);
    r.weapon_count.fill(0);
    for (std::size_t l = 0; l < s.size(); ++l) {
        r.weapon[l] = static_cast<std::uint8_t>(s[l].first);
        r.weapon_count[l] = static_cast<std::uint8_t>(s[l].second);
    }
    r.slots_cb() = static_cast<std::uint8_t>(s.size());
}

}  // namespace

TEST_CASE("weapon load: (a) the SKY TALON after its sale takes DUMB MISSILES in slot 1 [ORACLE core-0006]") {
    World w;
    const WeaponWords W = table_words();
    slots(w, {{0, 1}});
    std::vector<int> images;
    const WeaponLoad r = load_weapon(w, W, 0, 11, [&](int x) { images.push_back(x); });
    CHECK(r.added);
    CHECK(r.slot == 1);
    ShipRecord& s = w.ship(0);
    CHECK(s.slots_cb() == 2);
    CHECK(s.weapon[1] == 11);        // +0xb9
    CHECK(s.weapon_count[1] == 20);  // +0xaf
    CHECK(images == std::vector<int>{11});  // seg10:0x533a(11), W(11)+0x0a = 1
    CHECK(s.weapon_slot == 0);  // +0x18 unchanged
}

TEST_CASE("weapon load: (b) the TRANSFORMER takes REAR-DEPLOYED MINES and DUMB MISSILES [ORACLE core-0006]") {
    World w;
    const WeaponWords W = table_words();
    slots(w, {{0, 1}});
    std::vector<int> images;
    const auto hook = [&](int x) { images.push_back(x); };
    load_weapon(w, W, 0, 10, hook);
    load_weapon(w, W, 0, 11, hook);
    ShipRecord& s = w.ship(0);
    CHECK(s.slots_cb() == 3);
    CHECK(s.weapon[0] == 0);
    CHECK(s.weapon[1] == 10);
    CHECK(s.weapon[2] == 11);
    CHECK(s.weapon_count[1] == 20);
    CHECK(s.weapon_count[2] == 20);
    CHECK(images == std::vector<int>{10, 11});
}

TEST_CASE("weapon load: (c) a held weapon is neither added nor reloaded [BYTES seg12:0x6d90-0x6d9a]") {
    World w;
    const WeaponWords W = table_words();
    slots(w, {{0, 1}, {7, 17}});
    int images = 0;
    const WeaponLoad r = load_weapon(w, W, 0, 7, [&](int) { ++images; });
    CHECK_FALSE(r.added);
    CHECK(w.ship(0).slots_cb() == 2);
    CHECK(w.ship(0).weapon_count[1] == 17);
    CHECK(images == 0);
}

TEST_CASE("weapon load: (d) ten slots are full [BYTES seg12:0x6da1-0x6da6]") {
    World w;
    const WeaponWords W = table_words();
    slots(w, {{0, 1}, {1, 20}, {2, 20}, {3, 20}, {4, 20}, {5, 20}, {6, 20}, {7, 20}, {8, 20}, {9, 20}});
    const ShipRecord before = w.ship(0);
    int images = 0;
    const WeaponLoad r = load_weapon(w, W, 0, 11, [&](int) { ++images; });
    CHECK_FALSE(r.added);
    CHECK(w.ship(0).slots_cb() == 10);
    CHECK(w.ship(0).weapon == before.weapon);
    CHECK(w.ship(0).weapon_count == before.weapon_count);
    CHECK(images == 0);
}

TEST_CASE("weapon load: (e) slot 0 is never searched, and weapon 0 has no image [BYTES seg12:0x6d72 seg12:0x6dcf]") {
    World w;
    const WeaponWords W = table_words();
    slots(w, {{0, 1}, {7, 20}});
    int images = 0;
    const WeaponLoad r = load_weapon(w, W, 0, 0, [&](int) { ++images; });
    CHECK(r.added);
    CHECK(r.slot == 2);
    CHECK(w.ship(0).slots_cb() == 3);
    CHECK(w.ship(0).weapon[2] == 0);
    CHECK(w.ship(0).weapon_count[2] == 20);
    CHECK(images == 0);  // W(0)+0x0a = 0
}

TEST_CASE("weapon load: (f) UNIFY FORCES lowers +0xcb by 2, then seg12:0x6d64(player, 26) [BYTES seg16:0x4016 seg16:0x4021]") {
    World w;
    w.player = 0;
    slots(w, {{0, 1}, {24, 20}, {25, 20}, {8, 20}});
    Shop shop_words;
    WorldPurchaseHost h(w);  // on_load_weapon unset: weapon_load.h load_weapon_program
    const WeaponWords W = table_words();
    PurchaseCtx p{w, shop_words, W, h};
    CHECK(unify_forces(p, 0) == 250);
    ShipRecord& s = w.ship(0);
    CHECK(s.slots_cb() == 3);
    CHECK(s.weapon[0] == 0);
    CHECK(s.weapon[1] == 8);
    CHECK(s.weapon[2] == 26);
    CHECK(s.weapon_count[2] == 20);
}

TEST_CASE("weapon load: the program's hook and the capture host's default seg12_6d64 [BYTES seg13:0x1e4e]") {
    World w;
    slots(w, {{0, 1}});
    std::vector<int> images;
    set_weapon_image([&](int x) { images.push_back(x); });
    // The program's words: W(w)+0x0a decides the call; filled from the table here.
    const WeaponWords saved = missile_state().words;
    missile_state().words = table_words();
    CaptureHost ch;
    ch.seg12_6d64(w, 0, 10);
    CHECK(w.ship(0).slots_cb() == 2);
    CHECK(w.ship(0).weapon[1] == 10);
    CHECK(images == std::vector<int>{10});
    // A ship index outside the table changes nothing
    CHECK_FALSE(load_weapon_program(w, kShipCount, 11).added);
    set_weapon_image(nullptr);
    missile_state().words = saved;
}
