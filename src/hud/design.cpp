#include "hud/design.h"

namespace is::hud {

DesignPanels design_panels(const fmt::ShipRecord& shp, const fmt::BodyRecord& body) {
    DesignPanels d;
    // seg14:0x1d1a-0x1d1f: mov al, 0x28; imul byte ptr es:[di] (signed 8 x 8 -> 16).
    d.tank = static_cast<std::int16_t>(0x28 * static_cast<std::int8_t>(shp.fuel_tank));
    d.lamp_row[0] = 0;  // seg14:0x1d3b: dword 0x00010000 at +0x22
    d.lamp_row[1] = 1;
    d.lamp_row[2] = 2;  // seg14:0x1d44
    if (body.flags & fmt::kBodyPullOffWings)  // seg14:0x1d80: BOD +0x20 bit 0x04
        d.lamp_row[3] = 3;
    else if (shp.flags & fmt::kShipCloak)  // seg14:0x1d9c: SHP +9 bit 0x08
        d.lamp_row[3] = 4;
    else if (body.flags & fmt::kBodyShield)  // seg14:0x1dbd: BOD +0x20 bit 0x02
        d.lamp_row[3] = 5;
    else
        d.lamp_row[3] = -1;  // seg14:0x1dd6
    d.cloak = (shp.flags & fmt::kShipCloak) != 0;     // seg14:0x1fd6
    d.shield = (body.flags & fmt::kBodyShield) != 0;  // seg14:0x1f9f
    return d;
}

}  // namespace is::hud
