// The parts of a ship's design record (DGROUP:0x96d2 + 0xad·n, built by seg14:0x1b84 from a
// MASTER.SHP record and its MASTER.BOD body) and of the ship entry (seg14:0x21ea-0x21f6) that
// the status panels read: the tank word, the four lamp rows and the cloak and shield flags.
// The design table itself is the world set-up's (M5); this is the HUD's reading of the same
// bytes, so the fuel gauge and the damage panel show the original's values. Portable.
#pragma once

#include <cstdint>

#include "formats/ships.h"

namespace is::hud {

struct DesignPanels {
    // +0x1e (DGROUP:0x96f0): 40 x the SHP byte +0 (imul byte: signed)
    // [BYTES seg14:0x1d1a-0x1d1f].
    std::int16_t tank = 0;
    // +0x22 ... +0x28 (DGROUP:0x96f4 ... 0x96fa): 0, 1, 2 for every design (one dword store
    // 0x00010000 and a word store 2), then 3 when the body has Pull-off Wings (BOD +0x20 bit
    // 0x04), else 4 when the ship has Cloak (SHP +9 bit 0x08), else 5 when the body has Shield
    // (BOD +0x20 bit 0x02), else -1 [BYTES seg14:0x1d3b seg14:0x1d44 seg14:0x1d80 seg14:0x1d8c
    // seg14:0x1d9c seg14:0x1da8 seg14:0x1dbd seg14:0x1dc9 seg14:0x1dd6].
    int lamp_row[4] = {0, 1, 2, -1};
    // Design byte +0x4a (DGROUP:0x971c) = SHP +9 bit 0x08, copied to ship byte +0xe7
    // [BYTES seg14:0x1fd6 seg14:0x1fe2 seg14:0x1fef seg14:0x21ea seg14:0x21ee].
    bool cloak = false;
    // Design byte +0xa7 (DGROUP:0x9779) = BOD +0x20 bit 0x02, copied to ship byte +0xe9
    // [BYTES seg14:0x1f9f seg14:0x1fab seg14:0x1fc8 seg14:0x21f2 seg14:0x21f6].
    bool shield = false;
};

DesignPanels design_panels(const fmt::ShipRecord& shp, const fmt::BodyRecord& body);

}  // namespace is::hud
