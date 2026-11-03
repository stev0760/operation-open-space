// The Ambulance Options dialog's portable rules (src/dialogs/ambulance_options.h): the entry
// text seg16:0x5762, the parser seg16:0x0, the list fill seg16:0x4bd8 for the trial's wave-1
// TRANSFORMER of core-0003, Add>> seg16:0x44fa with its refusals, OK seg16:0x5b24 and the Sell
// list seg16:0x541c (work/analysis/m4/s3-visit.md section 3 with s3-visit-verify.md).
#include <map>
#include <string>
#include <vector>

#include "dialogs/ambulance_options.h"
#include "doctest.h"
#include "hud/weapons.h"
#include "world/world.h"

using namespace is::dialogs;
using namespace is::world;

namespace {

// The ENG_LANG strings the shop reads [BYTES ENG_LANG.DLL STRING 12] ids 186-212 [BYTES
// ENG_LANG.DLL STRING 55] id 865 [BYTES ENG_LANG.DLL STRING 73] ids 1152-1156 [BYTES ENG_LANG.DLL
// STRING 74] id 1176 [BYTES ENG_LANG.DLL STRING 75] ids 1185-1186 [BYTES ENG_LANG.DLL STRING 124]
// ids 1969-1978 [BYTES ENG_LANG.DLL STRING 127] ids 2023-2024 [BYTES ENG_LANG.DLL STRING 145] id
// 2308 [BYTES ENG_LANG.DLL STRING 158] id 2516 [BYTES ENG_LANG.DLL STRING 783] id 12515.
const std::map<int, std::string> kStrings = {
    {186, "NO WEAPON"}, {187, "RING OF DEATH"}, {188, "GRAPPLING HOOK"}, {189, "LIGHTNING"},
    {190, "SNOWBALLS"}, {191, "FIREBALLS"}, {192, "TARGETTING MISSILES"},
    {193, "HEATSEEKER MISSILES"}, {194, "RADAR-GUIDED MISSILES"}, {195, "DEATH HEADS"},
    {196, "REAR-DEPLOYED MINES"}, {197, "DUMB MISSILES"}, {198, "FIRE DEMONS"},
    {199, "WILDCARD DEFENSE"}, {200, "BATTERIES"}, {201, "HANDCUFFS"}, {202, "DOUGHNUTS"},
    {203, "PROTECTIVE PELLETS"}, {204, "BEER BOTTLES"}, {205, "BOOMERANGS"},
    {206, "SPUD MISSILES"}, {207, "THROWING STARS"}, {208, "ENLIGHTENMENT"},
    {209, "UNCERTAINTY"}, {210, "REPULSIVE FORCE"}, {211, "ATTRACTIVE FORCE"},
    {212, "INNER FORCE"}, {865, "NEW SHIP BODY"}, {1152, "WING"}, {1153, " REPAIR"},
    {1155, "SHIELD"}, {1156, "CLOAK"}, {1176, "REFUEL%< COMPLETE.%>"},
    {1185, "%<INSTALLING %>DOUBLE GUNS"}, {1186, "RELOAD "}, {1969, "POWERUP "},
    {1970, "UNIFY FORCES"}, {1973, "GUNS"}, {1974, "THRUSTERS"}, {1975, "ENGINE ARMOUR"},
    {1976, "GUN ARMOUR"}, {1977, "THRUST ARMOUR"}, {1978, "WING ARMOUR"},
    {2023, "You have the maximum number of weapons already. "},
    {2024, "You must sell a weapon before buying another."}, {2308, "BODY"},
    {2516, "BASIC REPAIRS"}, {12515, "ICONS"}};

struct Rig {
    World w;
    Shop s;
    WeaponWords W;
    WorldPurchaseHost h{w};
    std::vector<std::string> boxes;
    std::vector<std::pair<int, int>> refusals;
    int choosers = 0;
    ShopEnv e{w, s, h, W};
    Rig() {
        const is::hud::WeaponTable t(0);
        for (int k = 0; k < kWeaponTableBlocks; ++k) W.block(k) = t.block(k);
        w.design_ratings[0].tank = 1320;
        w.design_ratings[0].special = 3;
        ShipRecord& S = w.ship(0);
        S.w26 = 74;
        S.w28 = 61;
        S.w2a = 74;
        S.w2c = 58;
        S.fuel = 1121;
        set_credit(S, 150);
        S.w0a = 1;
        S.be9 = 1;  // the shield
        S.slots_cb() = 2;
        S.weapon[1] = 8;
        S.weapon_count[1] = 20;
        S.roster_index = 0;
        set_byte_at(S, 0xac, 1);
        set_byte_at(S, 0xad, 1);
        w.roster.resize(1);
        s.credit = 150;
        e.edition = 0;  // the Limited edition [ORACLE core-0003: the trial's refusal box]
        e.lang = [](int id) {
            const auto it = kStrings.find(id);
            return it == kStrings.end() ? std::string() : it->second;
        };
        e.message_box = [this](const std::string& t, unsigned) { boxes.push_back(t); };
        e.refuse = [this](int a, int b) { refusals.push_back({a, b}); };
        e.body_chooser = [this] { ++choosers; };
    }
    int find(const ModelList& l, const std::string& t) {
        for (int i = 0; i < l.count(); ++i)
            if (l.items[static_cast<std::size_t>(i)] == t) return i;
        return -1;
    }
};

}  // namespace

TEST_CASE("seg16:0x0: the price parser") {
    CHECK(shop_price("a1.5 - X") == 150);
    CHECK(shop_price("12.3 - X") == 1230);
    CHECK(shop_price(" 0.2 - REFUEL") == 20);
    CHECK(shop_price("  6.0 - NEW SHIP BODY") == 600);
    CHECK(shop_price(" a10.0 - X") == 1000);
}

TEST_CASE("seg16:0x5762: the entry text") {
    CHECK(shop_entry_text(28, 1, "REFUEL") == " 0.2 - REFUEL");
    CHECK(shop_entry_text(600, 2, "NEW SHIP BODY") == "  6.0 - NEW SHIP BODY");
    CHECK(shop_entry_text(250, 0, "DOUBLE GUNS") == "2.5 - DOUBLE GUNS");
    CHECK(shop_entry_text(1000, 0, "X") == "a10.0 - X");
    CHECK(shop_entry_text(5, 0, "X") == "0.5 - X");  // no leading zero: the parser charges 50
    CHECK(shop_entry_text(0, 0, "X").empty());
    CHECK(shop_display_text(" 0.2 - REFUEL") == " 0.2 - REFUEL");  // a flag-1 entry keeps its indent
    CHECK(shop_display_text("  6.0 - NEW SHIP BODY") == " 6.0 - NEW SHIP BODY");
    CHECK(shop_display_text("a10.0 - X") == "10.0 - X");
    CHECK(shop_display_text(" a10.0 - X") == " 10.0 - X");
    CHECK(shop_grey("2.5 - DOUBLE GUNS", 150));
    CHECK_FALSE(shop_grey(" 0.9 - BASIC REPAIRS", 60));
}

TEST_CASE("seg16:0x4bd8: the trial's wave-1 list, as core-0003 reads it") {
    Rig r;
    ModelList o;
    shop_fill(r.e, o);
    const std::vector<std::string> want = {
        " 0.2 - REFUEL", " 0.8 - WING\\SHIELD REPAIR", " 0.9 - BASIC REPAIRS",
        "2.0 - POWERUP ENGINE ARMOUR", "2.0 - POWERUP GUN ARMOUR", "2.0 - POWERUP THRUST ARMOUR",
        "2.0 - POWERUP WING ARMOUR", "2.5 - DOUBLE GUNS", "4.0 - POWERUP GUNS",
        "4.0 - POWERUP THRUSTERS", "4.5 - DUMB MISSILES", "4.5 - REAR-DEPLOYED MINES",
        "5.0 - DOUGHNUTS", "6.0 - NEW SHIP BODY", "6.0 - PROTECTIVE PELLETS", "6.5 - SNOWBALLS",
        "6.5 - THROWING STARS", "7.0 - FIREBALLS", "7.5 - HANDCUFFS", "8.0 - HEATSEEKER MISSILES",
        "9.0 - BATTERIES"};
    CHECK(o.items == want);  // 21 entries; the oracle's "REPAIRS" is string 1153's [OPEN]
    REQUIRE(o.count() > 0);
    CHECK(o.sel[0]);  // LB_SETSEL(TRUE, 0)
}

TEST_CASE("seg16:0x44fa: Add>> charges, refuses the dear, the Limited 'P' and the eleventh weapon") {
    Rig r;
    ModelList o, t;
    shop_fill(r.e, o);
    o.set_sel(-1, false);
    // BASIC REPAIRS: 90 charged [ORACLE core-0003: 0x9e55 = 90, 0x9e57 = 60].
    o.set_sel(r.find(o, " 0.9 - BASIC REPAIRS"), true);
    shop_add(r.e, true, o, t);
    CHECK(r.s.cost == 90);
    CHECK(r.s.credit == 60);
    CHECK(t.items == std::vector<std::string>{" 0.9 - BASIC REPAIRS"});
    CHECK(r.find(o, " 0.9 - BASIC REPAIRS") == -1);
    // A dear entry stays, deselected.
    o.set_sel(r.find(o, "2.5 - DOUBLE GUNS"), true);
    shop_add(r.e, true, o, t);
    CHECK(r.find(o, "2.5 - DOUBLE GUNS") >= 0);
    CHECK(o.selected().empty());
    CHECK(r.s.cost == 90);
    // The Limited edition's 'P' test: PROTECTIVE PELLETS counts as a POWERUP; a new weapon
    // also counts the weapon (DGROUP:0x958e is not taken back).
    r.s.credit = 10000;
    o.set_sel(r.find(o, "6.0 - PROTECTIVE PELLETS"), true);
    shop_add(r.e, true, o, t);
    REQUIRE(r.refusals.size() == 1);
    CHECK(r.refusals[0] == std::pair<int, int>{2320, 2321});
    CHECK(r.s.weapons == 1);
    o.set_sel(r.find(o, "2.0 - POWERUP GUN ARMOUR"), true);
    shop_add(r.e, true, o, t);
    CHECK(r.refusals.size() == 2);
    CHECK(r.s.weapons == 1);
    // The full edition: the eleventh weapon gets the MessageBox 2023 + 2024 and -1.
    r.e.edition = 2;
    r.s.weapons = 10;
    o.set_sel(r.find(o, "4.5 - DUMB MISSILES"), true);
    shop_add(r.e, true, o, t);
    REQUIRE(r.boxes.size() == 1);
    CHECK(r.boxes[0] == "You have the maximum number of weapons already. You must sell a weapon before buying another.");
    CHECK(r.s.weapons == 10);
    CHECK(r.find(o, "4.5 - DUMB MISSILES") >= 0);
    // NEW SHIP BODY opens the chooser before the tests (verified claim 66).
    o.set_sel(r.find(o, "6.0 - NEW SHIP BODY"), true);
    shop_add(r.e, true, o, t);
    CHECK(r.choosers == 1);
    CHECK(r.find(t, "6.0 - NEW SHIP BODY") >= 0);
}

TEST_CASE("seg16:0x4900 and seg16:0x5b24: Remove refunds; OK sets the flags by suffix") {
    Rig r;
    ModelList o, t;
    shop_fill(r.e, o);
    o.set_sel(-1, false);
    o.set_sel(r.find(o, " 0.2 - REFUEL"), true);
    o.set_sel(r.find(o, " 0.9 - BASIC REPAIRS"), true);
    shop_add(r.e, true, o, t);
    CHECK(r.s.cost == 110);
    CHECK(r.s.credit == 40);  // [ORACLE core-0003: 0x9e55 = 110, 0x9e57 = 40]
    shop_ok(r.e, t);
    CHECK(r.s.basic == 1);
    CHECK(r.s.refuel == 1);
    CHECK(r.s.wing == 0);
    t.set_sel(0, true);
    shop_remove(r.e, true, o, t);
    CHECK(r.s.cost == 90);
    CHECK(r.s.credit == 60);
    CHECK(t.count() == 1);
    CHECK(shop_remaining_text(r.e) == "0.60 ICONS");
    CHECK(shop_total_text(r.e, true) == "0.90 ICONS");
}

TEST_CASE("seg16:0x541c: Sell prices RADAR-GUIDED at 7.5 for 20 and 7.1 for 19") {
    Rig r;
    ModelList o;
    sell_fill(r.e, o);
    CHECK(o.items == std::vector<std::string>{" 7.5 - RADAR-GUIDED MISSILES"});
    r.w.ship(0).weapon_count[1] = 19;
    sell_fill(r.e, o);
    CHECK(o.items == std::vector<std::string>{" 7.1 - RADAR-GUIDED MISSILES"});
}

TEST_CASE("seg16:0x5db0: Sell OK marks the weapon and deletes the matching Options items") {
    Rig r;
    ModelList todo, o;
    todo.add(" 7.1 - RADAR-GUIDED MISSILES");
    o.add("1.0 - RELOAD RADAR-GUIDED MISSILES");
    o.add("2.0 - X");
    sell_ok(r.e, todo, o);
    CHECK(r.s.sold[8] == 1);
    CHECK(o.items == std::vector<std::string>{"2.0 - X"});
}

TEST_CASE("seg16:0x51d6's placement") {
    const ShopPlacement p = shop_placement(432, 251, 1024, 768);
    CHECK(p.x == 296);
    CHECK(p.y == 109);  // [ORACLE core-0003: the frame at (296, 109)]
}
