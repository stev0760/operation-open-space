#include "desk/main_ship.h"

#include <set>

namespace is::desk {

const fmt::ShipRecord* main_ship_record(const assets::Ships& ships, const std::string& name,
                                        const std::function<void(const std::string&, const char*)>& log) {
    static const fmt::ShipRecord zero{};
    if (const fmt::ShipRecord* r = ships.find(name)) return r;  // [BYTES seg15:0x2a55-0x2a7f]
    static std::set<std::string> logged;
    if (logged.insert(name).second && log)
        log("launch ship: MainShip " + name + " matches no MASTER.SHP record; the zeroed record (ADR 0016 decision 2)",
            "seg15:0x2a55");
    return &zero;  // record DGROUP:0x95aa, past the table [DESIGN ADR 0016 decision 2]
}

}  // namespace is::desk
