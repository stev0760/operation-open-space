// The violence decay seg7:0x2128 (aggression.h).
#include "world/aggression.h"

namespace is::world {

std::int16_t violence_step(std::int16_t a, std::int16_t div, std::int16_t floor) {
    std::int16_t d = idiv16(a, div);  // `cwd; idiv cx` [BYTES seg7:0x2179-0x217a]
    if (d < floor) d = floor;         // signed `jge` [BYTES seg7:0x217c-0x2181]
    const std::int16_t r = sub16(a, d);  // [BYTES seg7:0x2184]
    return r < 0 ? std::int16_t{0} : r;  // `jns`, else 0 [BYTES seg7:0x2188-0x218a]
}

namespace {

// (u32)(DGROUP:0x8e28 + 15000) < DGROUP:0x1e50: `add eax, 0x3a98; cmp; jae` skips the second
// step [BYTES seg7:0x2190-0x219f seg7:0x21d1-0x21e0].
bool shot_long_ago(const World& w, std::uint32_t now) { return w.w8e28 + 15000u < now; }

}  // namespace

void decay_violence(World& w, std::uint32_t now) {
    std::int16_t& a = w.w9432;  // DGROUP:0x9432
    if (w.last_ship == 0) {     // DGROUP:0x9316 [BYTES seg7:0x2135-0x2142]
        a = violence_step(a, 50, 2);  // [BYTES seg7:0x213c-0x2142 seg7:0x21e8-0x21f9]
        return;
    }
    // k < kShipCount: the port's table bound [DESIGN] [BYTES seg7:0x2145-0x2155]
    for (int k = 0; k < w.last_ship && k < kShipCount; ++k) {
        const ShipRecord& s = w.ships[static_cast<std::size_t>(k)];
        // DGROUP:0x70c2, 0x70c4, 0x70c8 + 0x15a·k [BYTES seg7:0x215c-0x2171]
        if (s.w32 != 0xe || s.w34 != w.player || s.w38 != 1) continue;
        a = violence_step(a, 18, 4);  // [BYTES seg7:0x2173-0x218a]
        if (shot_long_ago(w, now)) a = violence_step(a, 14, 4);  // [BYTES seg7:0x21a1-0x21b2 seg7:0x21f3-0x21f9]
        return;
    }
    a = violence_step(a, 40, 3);  // [BYTES seg7:0x21b4-0x21cb]
    if (shot_long_ago(w, now)) a = violence_step(a, 36, 2);  // [BYTES seg7:0x21d1-0x21f9]
}

}  // namespace is::world
