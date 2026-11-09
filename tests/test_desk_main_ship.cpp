// seg15:0x2a08's MainShip lookup (src/desk/main_ship.h): an exact name match, and ruling R4 of M4
// stage 6 for a name no record matches: the zeroed scratch record (body 0, SHP +0x07 = +0x08 =
// +0x09 = 0) logged once per name [DESIGN ADR 0016 decision 2] [BYTES seg15:0x2a2f-0x2a8e]
// (review M6).
#include <string>
#include <vector>

#include "desk/main_ship.h"
#include "doctest.h"

using namespace is;

TEST_CASE("desk main ship: an exact match, else the zeroed record logged once") {
    assets::Ships ships;
    fmt::ShipRecord a;
    a.name = "SKY TALON";
    a.body = 7;
    a.byte_07 = 3;
    a.flags = 1;
    ships.ships.records = {a};
    std::vector<std::string> log;
    const auto sink = [&](const std::string& w, const char*) { log.push_back(w); };
    const fmt::ShipRecord* hit = desk::main_ship_record(ships, "SKY TALON", sink);
    CHECK(hit->body == 7);
    CHECK(log.empty());
    // lstrcmp is exact: case and trailing spaces count
    const fmt::ShipRecord* miss = desk::main_ship_record(ships, "sky talon", sink);
    REQUIRE(miss != nullptr);
    CHECK(miss->body == 0);
    CHECK(miss->byte_07 == 0);
    CHECK(miss->byte_08 == 0);
    CHECK(miss->flags == 0);
    CHECK(log.size() == 1);
    desk::main_ship_record(ships, "sky talon", sink);
    CHECK(log.size() == 1);  // once per name
    desk::main_ship_record(ships, "EAGLE ", sink);
    CHECK(log.size() == 2);
}
