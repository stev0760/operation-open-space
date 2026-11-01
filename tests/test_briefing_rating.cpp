// seg13:0x33d6, the rating word +0xd8 the briefing's ship page ranks (src/world/rating.h), and
// seg17:0x4068's rank: the shipped TRANSFORMER of a wave-1 briefing reads "YOUR SHIP IS
// RESPECTABLE." [ORACLE start-0001]. The golden reads IS_ORIGINAL_DIR and skips without it.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "briefing/briefing.h"
#include "doctest.h"
#include "formats/ships.h"
#include "world/flight.h"
#include "world/rating.h"
#include "world/world.h"

using namespace is::world;
namespace fs = std::filesystem;

TEST_CASE("briefing rating: seg13:0x33d6 without a roster record and the clamp") {
    ShipRecord s{};
    ObjectRecord o{};
    DesignRating d;
    d.armor4a = 10;  // q = design +0x10 when +0xd6 = -1 [BYTES seg13:0x3491-0x349c]
    d.tank = 400;    // t = 20
    d.repair = 1;    // no 3/4
    d.captures = 1;  // no extra 5/6
    o.mass = 66;     // (66 + 20) 10 / 86 = 10
    s.thrust = 100;
    s.drag = 0;
    s.turn = 0;
    s.w26 = 100;
    // c = 2 (2000 / 86) = 46, then 5c/6 = 38; a = 6 10 / 5 = 12; c = min(38, 60) = 38;
    // (100/2 + 50) (38 + 0 + 12) = 5000 [BYTES seg13:0x350b-0x366a]
    CHECK(ship_rating(s, o, d, nullptr) == 5000);
    s.thrust = 1000;  // c = 2 (20000 / 86) = 464, 5c/6 = 386, then c = min(c, 5a) = 60
    CHECK(ship_rating(s, o, d, nullptr) == 100 * (60 + 12));
    d.armor4a = 200;  // a = 200, 6a/5 = 240: 100 (386 + 240) = 62600 > 0x7fbc -> 32700
    CHECK(ship_rating(s, o, d, nullptr) == 32700);
}

TEST_CASE("briefing rating golden: the shipped TRANSFORMER is RESPECTABLE [ORACLE start-0001]") {
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
    const is::fmt::BodyRecord& b = bod.records[t->body];
    // The ship set-up's player record (src/world/hooks.cpp ship_setup_player): the flight
    // fields, weapon slots 0 and 1 with +0xcb = 2 [ORACLE probe-0001], the new roster record.
    ShipRecord s{};
    ObjectRecord o{};
    const FlightDesign fd =
        flight_design(t->thruster, t->armor, b.strength_base, b.byte_1c, b.byte_1d, b.flags, t->flags);
    apply_flight_design(s, o, fd, true, 0, 0);
    CHECK(s.thrust == 205);  // probe-0001's +0x0c, +0x0e, +0x14 [ORACLE probe-0001]
    CHECK(s.drag == 25);
    set_byte_at(s, 0xb9, t->weapon);
    set_byte_at(s, 0xcb, 2);
    s.w26 = 100;  // seg17:0x4068 [BYTES seg17:0x407b-0x4096]
    RosterRecord r;
    const DesignRating d = design_rating(*t, b);
    const std::int16_t rating = ship_rating(s, o, d, &r);
    MESSAGE("TRANSFORMER rating " << rating << ", turn " << s.turn << ", mass " << o.mass);
    CHECK(is::briefing::rank_string(rating) == 717);  // RESPECTABLE: 5500 < rating <= 6300
}
