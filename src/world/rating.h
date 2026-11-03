// A ship's rating, the routine seg13:0x33d6 that stores ship word +0xd8 (DGROUP:0x7168 + 0x15a·i),
// with the design-record words and the roster-record bytes it reads. The briefing's ship page
// (seg17:0x4068, src/briefing rank_string) ranks the player by it; the kill scorer reads the same word. Portable
// (M5 s2 BRIEFING).
//
// The listing was read from `isre pack seg13:0x33d6` (work/world/m5s2/briefing/pack-seg13_0x33d6.md),
// not from Ghidra's C (which drops a block), by the implementer; the analyst's claim 19 still
// waits for a verifier [OPEN: work/analysis/m5/s2-briefing.md claim 19].
#pragma once

#include <array>
#include <cstdint>

#include "world/records.h"

namespace is::fmt {
struct ShipRecord;
struct BodyRecord;
}  // namespace is::fmt

namespace is::world {

// The design-record words seg13:0x33d6 reads, each at DGROUP:0x96d2 + 0xad·n + offset (n = ship
// word +0x4e), as seg14:0x1b84 writes them from the MASTER.SHP record and its MASTER.BOD body.
struct DesignRating {
    std::int16_t armor4a = 0;   // +0x10 DGROUP:0x96e2: signed SHP +2 / 4 [BYTES seg14:0x1d06-0x1d0e]
    std::int16_t armor5a = 0;   // +0x12 DGROUP:0x96e4: SHP +2 / 5 [BYTES seg14:0x1ce2-0x1cea]
    std::int16_t armor4b = 0;   // +0x14 DGROUP:0x96e6: SHP +2 / 4 [BYTES seg14:0x1cee-0x1cf6]
    std::int16_t armor5b = 0;   // +0x16 DGROUP:0x96e8: SHP +2 / 5 [BYTES seg14:0x1cfa-0x1d02]
    std::int16_t tank = 0;      // +0x1e DGROUP:0x96f0: 40 x signed SHP +0 [BYTES seg14:0x1d1a-0x1d1f]
    std::int16_t special = -1;  // +0x28 DGROUP:0x96fa: 3 (body 0x04), 4 (SHP 0x08), 5 (body 0x02), else -1
                                // [BYTES seg14:0x1d8c seg14:0x1da8 seg14:0x1dc9 seg14:0x1dd6]
    std::int16_t captures = 0;  // +0x42 DGROUP:0x9714: SHP +9 bit 0x10 [BYTES seg14:0x1eb4]
    std::int16_t cloak = 0;     // +0x4a DGROUP:0x971c: SHP +9 bit 0x08 [BYTES seg14:0x1fd6]
    std::int16_t repair = 0;    // +0x4c DGROUP:0x971e: SHP +9 bit 0x04 [BYTES seg14:0x1ffd]
    std::int16_t shield = 0;    // +0xa7 DGROUP:0x9779, a word store of 1 or 0: body +0x20 bit 0x02
                                // [BYTES seg14:0x1fab seg14:0x1fc8]
};
DesignRating design_rating(const fmt::ShipRecord& shp, const fmt::BodyRecord& body);

// RosterRecord, the 0x7c-byte roster record, is records.h's.

// The signed bytes DGROUP:0x1332 + w that seg13:0x33d6 adds per weapon w (initialised data, no
// writer found) [BYTES DGROUP:0x1332-0x1351]. Where the table ends is [OPEN]: the port holds
// the 32 bytes up to the strings at DGROUP:0x1354 and reads 0 outside them [DESIGN].
std::int8_t weapon_strength(int weapon);

// seg13:0x33d6(i): the rating word +0xd8 of ship `s` with pool record `o` (object +0x44), its
// design `d` and its roster record `r` (null when +0xd6 = -1). All arithmetic is the original's
// 16-bit `imul` / `idiv` and the RTL long division seg5:0x110 [BYTES seg13:0x33d6-0x3676]. A
// zero divisor, which the original traps on, gives 0 for that quotient [DESIGN].
std::int16_t ship_rating(const ShipRecord& s, const ObjectRecord& o, const DesignRating& d,
                         const RosterRecord* r);


}  // namespace is::world
