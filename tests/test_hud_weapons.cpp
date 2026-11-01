// The weapon table DGROUP:0x2f54 of seg10:0x1fb2 (src/hud/weapons.h) and the design panel words
// of seg14:0x1b84 / seg14:0x21ea (src/hud/design.h): values pinned to their DGROUP addresses,
// the weapon display line they give, and the shipped TRANSFORMER against probe-0001.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "doctest.h"
#include "formats/ships.h"
#include "hud/design.h"
#include "hud/hud.h"
#include "hud/info.h"
#include "hud/weapons.h"
#include "hud_fakes.h"
#include "shell/geometry.h"

using namespace is::hud;
namespace fs = std::filesystem;

TEST_CASE("hud weapons: seg10:0x1fb2's table, word by DGROUP address") {
    const WeaponTable t(0);
    const auto at = [&](unsigned addr) {
        const int off = static_cast<int>(addr) - kDgWeaponTable;
        return t.word(off / kWeaponBlockSize, off % kWeaponBlockSize);
    };
    CHECK(at(0x2f54) == 186);   // w0 NO WEAPON [BYTES seg10:0x1fbc]
    CHECK(at(0x2f7c) == 10);    // w0 +0x28 [BYTES seg10:0x1fc2]
    CHECK(at(0x2f8a) == -1);    // w0 +0x36 [BYTES seg10:0x1fe8]
    CHECK(at(0x30e4) == 191);   // w5 FIREBALLS [BYTES seg10:0x1ffa]
    CHECK(at(0x30e6) == 191);   // w5 +0x02 [BYTES seg10:0x1ff7]
    CHECK(at(0x30e8) == 1);     // w5 +0x04 [BYTES seg10:0x1fee]
    CHECK(at(0x3104) == 350);   // w5 +0x20 [BYTES seg10:0x2003]
    // w8 RADAR-GUIDED MISSILES, the oracle's wave-1 weapon (probe-0001)
    CHECK(at(0x31d4) == 194);
    CHECK(at(0x31de) == 3);    // +0x0a: counted
    CHECK(at(0x3202) == 0);    // +0x2e: the count is shown
    CHECK(at(0x320a) == -1);   // +0x36 for DGROUP:0x920c != 3
    // the 28th block the filler writes (globals.md counts 27)
    CHECK(at(0x37c4) == -1);
    CHECK(t.word(27, 0x06) == 5);
    // seg11:0x2ff6(w) reads words -8, -6, -2 of record w: block 7's +0x48 ... +0x4e
    CHECK(t.word(8, -8) == 23);
    CHECK(t.word(8, -6) == 1);
    CHECK(t.word(8, -4) == -1);
    CHECK(t.word(8, -2) == 370);
    CHECK(t.word(8, -8) == t.word(7, 0x48));
    // outside the table
    CHECK(t.word(28, 0) == 0);
    CHECK(t.word(-1, 0) == 0);
}

TEST_CASE("hud weapons: the 23 words of DGROUP:0x920c = 3") {
    const WeaponTable a(0), b(3);
    int differing = 0;
    for (int w = 0; w < kWeaponBlocks; ++w)
        for (int o = 0; o < kWeaponBlockSize; o += 2)
            if (a.word(w, o) != b.word(w, o)) {
                ++differing;
                CHECK(o == 0x36);  // every edition-dependent store is +0x36
            }
    CHECK(differing == 23);
    CHECK(a.word(1, 0x36) == 17);  // DGROUP:0x2fda
    CHECK(b.word(1, 0x36) == 5);
    CHECK(a.word(12, 0x36) == 5);  // DGROUP:0x334a
    CHECK(b.word(12, 0x36) == 10);
    CHECK(b.word(5, 0x36) == -1);  // w5 is not edition-dependent
    // any other edition is the first column
    const WeaponTable c(2);
    CHECK(c.word(1, 0x36) == 17);
}

TEST_CASE("hud weapons: the weapon line of the oracle's wave 1") {
    const WeaponTable t(0);
    // TRANSFORMER: slot 1, weapon 8, 20 rounds [ORACLE probe-0001]
    const WeaponView v = weapon_view(t, 8, 20);
    CHECK(v.name_id == 194);
    CHECK(v.counted);
    CHECK(!v.no_count);
    CHECK(v.count == 20);
    hudtest::RecordingCanvas cv;
    hudtest::FakeHost h;
    h.weapon_ = v;
    Info info(cv, h);
    const is::shell::PlayArea a = is::shell::play_area_from_size(1024, 768, 1024, 768);
    const is::shell::HudValues hv = is::shell::hud_values(1024, false);
    info.layout(is::shell::hud_layout(a, is::shell::hud_inputs(hv)), hv);
    info.paint(kInfoWeapon);
    const auto texts = cv.of(hudtest::Call::kText);
    REQUIRE(texts.size() == 2u);
    CHECK(texts[1].text == "20 - #194");  // "20 - RADAR-GUIDED MISSILES"
    // NO WEAPON: not counted and one of the three names without a prefix
    const WeaponView n = weapon_view(t, 0, 1);
    CHECK(n.name_id == 0xba);
    CHECK(!n.counted);
    // a weapon whose +0x0a is 0 (w1 RING OF DEATH) reads "FUEL - "
    CHECK(!weapon_view(t, 1, 0).counted);
}

TEST_CASE("hud design: seg14:0x1b84's tank and lamp rows, seg14:0x21ea's flags") {
    is::fmt::ShipRecord s;
    is::fmt::BodyRecord b;
    s.fuel_tank = 33;
    DesignPanels d = design_panels(s, b);
    CHECK(d.tank == 1320);  // TRANSFORMER's DGROUP:0x96f0 [ORACLE probe-0001]
    CHECK(d.lamp_row[0] == 0);
    CHECK(d.lamp_row[1] == 1);
    CHECK(d.lamp_row[2] == 2);
    CHECK(d.lamp_row[3] == -1);  // seg14:0x1dd6
    CHECK(!d.cloak);
    CHECK(!d.shield);
    // imul byte: the tank byte is signed
    s.fuel_tank = 200;
    CHECK(design_panels(s, b).tank == 40 * -56);
    s.fuel_tank = 33;
    // the order of seg14:0x1d80 / 0x1d9c / 0x1dbd: wings, then cloak, then shield
    b.flags = is::fmt::kBodyShield;
    d = design_panels(s, b);
    CHECK(d.lamp_row[3] == 5);
    CHECK(d.shield);
    s.flags = is::fmt::kShipCloak;
    d = design_panels(s, b);
    CHECK(d.lamp_row[3] == 4);
    CHECK(d.cloak);
    b.flags = is::fmt::kBodyShield | is::fmt::kBodyPullOffWings;
    d = design_panels(s, b);
    CHECK(d.lamp_row[3] == 3);
    CHECK(d.shield);
    CHECK(d.cloak);
}

TEST_CASE("hud design golden: the shipped TRANSFORMER") {
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
    const DesignPanels d = design_panels(*t, bod.records[t->body]);
    // probe-0001's wave dump: tank 1320, rows [0, 1, 2, 3], +0xe7 = 0, +0xe9 = 1, so the
    // fourth lamp reads "Shield" [ORACLE probe-0001]
    CHECK(d.tank == 1320);
    CHECK(d.lamp_row[3] == 3);
    CHECK(!d.cloak);
    CHECK(d.shield);
    // its weapon byte names RADAR-GUIDED MISSILES, weapon 8 of the table
    CHECK(t->weapon == 8);
    CHECK(WeaponTable(0).word(t->weapon, 0) == 194);
}

TEST_CASE("hud weapons: seg11:0x2ff6's picture choice") {
    const WeaponTable t(0);
    CHECK(weapon_picture(t, 0).kind == WeaponPicture::Kind::kNone);
    // w1 RING OF DEATH: +0x0a = 0, so graphic +0x02 = 194 [BYTES seg11:0x3012 seg11:0x302e]
    WeaponPicture p = weapon_picture(t, 1);
    CHECK(p.kind == WeaponPicture::Kind::kGraphic);
    CHECK(p.graphic == 194);
    // w8 RADAR-GUIDED MISSILES: counted, word -2 = 370: composed [OPEN]
    CHECK(weapon_picture(t, 8).kind == WeaponPicture::Kind::kComposed);
    // w12 FIRE DEMONS: counted, but word -2 (block 11's +0x4e) is not -1: composed; w22
    // ENLIGHTENMENT's word -2 is block 21's +0x4e = -1: its graphic +0x02 = 199
    CHECK(weapon_picture(t, 12).kind == WeaponPicture::Kind::kComposed);
    p = weapon_picture(t, 22);
    CHECK(p.kind == WeaponPicture::Kind::kGraphic);
    CHECK(p.graphic == 199);
    // weapon 0x18 never takes the graphic path
    CHECK(weapon_picture(t, 0x18).kind == WeaponPicture::Kind::kComposed);
}
