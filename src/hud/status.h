// seg11:0x1bcc, the status tick of the wave pass (docs/re/world.md "Update loop" step 1, "Mass,
// thrust and drag"): the score digits when the score changed, the 300 ms lamp blink
// DGROUP:0x93fe, the clock when the wave's second changed, and every 1000 ms the fuel burn and
// mass of every ship and the fuel gauge; then seg11:0x1d7c. The panel updates go through
// seg11:0x4a0 (Panels::update).
//
// Ported from the bytes by the M3 s2 fixer [BYTES seg11:0x1bcc-0x1d7b]; world.md names the
// routine and its 1000 ms mass rule, the rest of the reading awaits a verifier. Portable.
#pragma once

#include <cstdint>
#include <functional>

#include "world/world.h"

namespace is::hud {

// The DGROUP words seg11:0x1bcc keeps.
struct StatusState {
    std::uint32_t blink_due = 0;  // dword DGROUP:0x1854
    std::uint32_t fuel_due = 0;   // dword DGROUP:0x5074
    std::int16_t blink = 0;       // DGROUP:0x93fe, toggled every 300 ms
};

// What seg11:0x1bcc calls outside itself.
struct StatusHooks {
    std::function<void(int arg)> update;            // seg11:0x4a0(arg)
    std::function<std::uint32_t()> score_shown;     // dword DGROUP:0x5018
    std::function<std::uint32_t()> score;           // dword DGROUP:0x1e58
    std::function<std::int16_t(int design)> design_mass;  // word DGROUP:0x9704 + 0xad·design
    std::function<void(int ship)> seg11_4c72;       // [OPEN] (the fuel-out damage, M4)
    std::function<void(std::uint32_t)> seg3_332e;   // [OPEN] (a status message)
    std::function<void()> seg13_2dcc;               // the radar brushes [OPEN]
    std::function<void()> seg11_1d7c;               // [OPEN]
};

// seg11:0x1bcc with `now` = DGROUP:0x1e50 and the wave's frame count w.frames = DGROUP:0x1e68.
void status_tick(world::World& w, std::uint32_t now, StatusState& s, const StatusHooks& h);

}  // namespace is::hud
