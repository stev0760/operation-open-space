// The visibility pass seg8:0x4e14 (docs/re/world.md "Visibility pass (seg8:0x4e14)"): which pool objects lie in the view's activity rectangles DGROUP:0x23e0 this frame. It
// writes object word +0x1a (1 inside, -1 for one frame after leaving, then 0 with the screen
// position parked at (-1000, -1000)), clears +0x1c of the objects it lists, lists them in the
// byte tables DGROUP:0x507c (pool) and DGROUP:0x50e0 (record) counted by DGROUP:0x9146, and on
// one frame in nine (rand() % 9 = 0) calls seg11:0x9fdc, LIGHTNING's search (src/world/
// lightning.h), for each listed object with byte +0x50 != 0 outside the ship pool when another
// listed object is live: the discharge of a charged object (work/analysis/m4/s7-ring-verify.md
// claim 27 and Omission O7).
//
// world.md "Visibility pass (seg8:0x4e14)" and structs.md object +0x1a, +0x1c (M3 s3 fixer,
// C043-C046) [BYTES seg8:0x4e14-0x50a8]; the player's +0x1a = 0 case stays [OPEN On-screen flag].
// Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "shell/rng.h"
#include "world/records.h"
#include "world/view.h"

namespace is::world {

// DGROUP:0x507c and DGROUP:0x50e0 are 100 bytes apart; the port bounds the list there [OPEN:
// the original writes on past entry 99].
constexpr int kVisibleMax = 100;

struct VisibilityGlobals {
    std::int16_t dg_9130 = 0;      // DGROUP:0x9130, the seg18 mode
    std::int16_t dg_9e49 = 0;      // DGROUP:0x9e49
    std::int16_t dg_952e = -1;     // DGROUP:0x952e
    std::int16_t radar_first = 0;  // DGROUP:0x9326, the first pool of the pass
    std::int16_t ship_pool = kPoolShips;  // DGROUP:0x9324, the last pool of the pass
    std::int16_t count = 0;        // DGROUP:0x9146 (the frame sets 0 before the call)
    std::array<std::uint8_t, kVisibleMax> pool{};  // DGROUP:0x507c
    std::array<std::uint8_t, kVisibleMax> rec{};   // DGROUP:0x50e0
};

// What the pass reads or calls outside the pools.
struct VisibilityHost {
    // Word DGROUP:0x3d80 + 0x34·rec of the pool-8 table: a pool-8 record whose word is 4 is
    // skipped [BYTES seg8:0x4f3e-0x4f4c].
    std::function<std::int16_t(int rec)> pool8_3d80;
    // seg11:0x9fdc(pool, rec), cdecl: LIGHTNING's search (lightning.h lightning_search) with
    // the listed object as the user [BYTES seg8:0x5049-0x505d]; an empty slot calls nothing.
    std::function<void(int pool, int rec)> seg11_9fdc;
};

// seg8:0x4e14 [BYTES seg8:0x4e14-0x50a8]: one rand() per call (seg5:0xdc at seg8:0x4ffb),
// drawn after the listing on both paths. The discharge loop (O7) runs only when that rand() % 9
// is 0 (`cwd; idiv 9`); for each listed index i < DGROUP:0x9146 (re-read every turn) whose
// object has byte +0x50 != 0 (any kind: a struck rock, a tank's wobble rate, a virus, a battery
// missile) and whose pool is not DGROUP:0x9324, the first listed j != i whose object has `state`
// 0 lets it call seg11:0x9fdc(pool[i], rec[i]) once; none, no call [BYTES seg8:0x4ffb-0x50a0].
void visibility(Pools& pools, const ViewState& view, VisibilityGlobals& g, shell::Rng& rng,
                const VisibilityHost& host);

// The program's visibility list DGROUP:0x507c / 0x50e0, for the routines later in the frame that
// walk it (the gate test seg12:0x1f7a, update.h): registered once by the program
// (src/hud/wave_win32.cpp registers its static); null until then. visibility() does not set it,
// so no caller's local is remembered [DESIGN] (M3 s4 fixer).
const VisibilityGlobals* last_visibility();
void set_last_visibility(const VisibilityGlobals* g);

}  // namespace is::world
