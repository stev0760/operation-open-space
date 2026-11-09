// seg15:0x2a08's MainShip record lookup, portable (src/desk/hooks.cpp ship_art). Ruling R4 of M4
// stage 6: a name no MASTER.SHP record matches reads record DGROUP:0x95aa, one past the table,
// which the port reads as the zeroed scratch record (body 0, SHP +0x07 = +0x08 = +0x09 = 0) and
// logs once per name [DESIGN ADR 0016 decision 2] [BYTES seg15:0x2a2f-0x2a8e].
#pragma once

#include <functional>
#include <string>

#include "assets/asset_store.h"
#include "formats/records.h"

namespace is::desk {

// The first record whose name (SHP +0x0a) equals `name` by an exact lstrcmp, else the zeroed
// record (never null); `log` hears the miss the first time each name misses.
const fmt::ShipRecord* main_ship_record(const assets::Ships& ships, const std::string& name,
                                        const std::function<void(const std::string&, const char*)>& log);

}  // namespace is::desk
