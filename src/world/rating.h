// A ship's rating, the routine seg13:0x33d6 that stores ship word +0xd8 (DGROUP:0x7168 + 0x15a·i),
// with the design-record words and the roster-record bytes it reads. The briefing's ship page
// (seg17:0x4068, src/briefing rank_string) ranks the player by it; the kill scorer reads the same word. Portable
// (M5 s2 BRIEFING).
//
// The listing was read from `isre pack seg13:0x33d6` (work/world/m5s2/briefing/pack-seg13_0x33d6.md),
// not from Ghidra's C (which drops a block); world.md "Ship rating" holds the verified reading
// (C055-C060).
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
    // +0x08 ... +0x0e DGROUP:0x96da ... 0x96e0: a shot's divisor per lamp k = 0 ... 3, each a
    // `cbw; idiv bl|cl; cbw` of the signed SHP byte +2: +0x08 = s/4, +0x0a = s/4, +0x0c = s/5,
    // +0x0e = s/5, stored in the order +0x0a, +0x0c, +0x0e, +0x08 [BYTES seg14:0x1cae-0x1cde]
    // (s3-contact.md 2.1, verified claim 5); seg8:0x45e0 divides by them on the shot path
    // (selector 2) [BYTES seg8:0x4a1e] (M5 s3).
    std::array<std::int16_t, 4> shot_div{};
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
    // +0x2e DGROUP:0x9700: the hit sound, SHP word +0x1f [BYTES seg14:0x1e18 seg14:0x1e1e] (M5 s3).
    std::int16_t hit_sound = 0;
    // +0x32 DGROUP:0x9704: the mass, flight.h FlightDesign::mass as seg14:0x1b84 stores it
    // (roster_build.h design_fill_portable fills it) [BYTES seg14:0x20bc] (M5 s3).
    std::int16_t mass = 0;
    // +0x38 DGROUP:0x970a: the design team word. Four writers: seg14:0x3e9 (the wave-1 fresh
    // roster, design 0), seg14:0x47e (later waves, the player's design), seg14:0x14ca (every
    // seg14:0x1430: sext(roster +0x64), or sext(SHP +0x03) for a ship with +0xd6 = -1) and
    // seg9:0xdbf (GAME OVER: 0) [BYTES seg14:0x3e9 seg14:0x47e seg14:0x14ca seg9:0xdbf]
    // (s3-roster.md 1.1, verified claims 1-3). design_rating leaves it 0: the placement writes it
    // (roster_build.h set_design_team) (M5 s3).
    std::int16_t team = 0;
    // +0x3a DGROUP:0x970c: seg14:0x0's tail loop stores design i's own index i [BYTES
    // seg14:0xd0e-0xd12] (M5 s3).
    std::int16_t w3a = 0;
    // +0x4e DGROUP:0x9720: zeroed for an old slot's design no new ship keeps (seg14:0x930) and at
    // GAME OVER (seg9:0xdbf); its other writers and readers are [OPEN] (M5 s3).
    std::int16_t w4e = 0;
    // +0xa6 DGROUP:0x9778: the byte SHP +2 [BYTES seg14:0x1d12-0x1d16] (M5 s3).
    std::int8_t armor = 0;
    // +0x2c DGROUP:0x96fe: 7 on every path, the object's radar brush +0x46 [BYTES seg14:0x1e08
    // seg14:0x20d7-0x20e6] (M5 s3).
    std::int16_t radar_brush = 7;
    // +0x30 DGROUP:0x9702: the mass +0x32 / 22 (`cwd; idiv`), the object's fragment count +0x48
    // [BYTES seg14:0x1e93-0x1e99 seg14:0x20fb-0x210a] (M5 s3). roster_build.h
    // design_fill_portable fills it with the mass.
    std::int16_t blast = 0;
    // +0xa2 DGROUP:0x9774: 1 for a body with flag 0x40, else 0; ship byte +0xe6 (anim_wrap) takes
    // it [BYTES seg14:0x1f71-0x1f89 seg14:0x21e2-0x21e6] (M5 s3).
    std::uint8_t wrap = 0;
    // +0xa3 DGROUP:0x9775: 6 for a Pull-off Wings body; seg14:0x1b84 writes nothing for another
    // body, which keeps the design slot's old byte (roster_build.h design_fill_portable carries
    // it); ship byte +0xe5 = (it != 0) [BYTES seg14:0x1f1f-0x1f2b seg14:0x223c-0x2253] (M5 s3).
    std::uint8_t b_a3 = 0;
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
