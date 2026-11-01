// owner: world set-up (the single writer of this file in later stages).
// The wave set-up's hook points (hooks.cpp, Win32 only): the state-1 entry seg10:0x0 (the load),
// the briefing entry's world part seg9:0xc06 / seg9:0x5cca (the icon fill and the placement), the
// wave start seg9:0x60bc (the ships) and the teardown. The program's World lives here.
#pragma once

#include <cstdint>
#include <functional>

#include "world/world.h"

#ifdef _WIN32
#include "gfx/gfx.h"
#endif

namespace is::app {
struct App;
// Installs the hooks (main.cpp, after install_briefing_hooks, whose state-1 and state-3 entries
// it wraps).
void install_world_hooks(App& app);
}  // namespace is::app

namespace is::world {

// The program's World (DGROUP), one for the whole run.
World& current();

#ifdef _WIN32
// The icon strip DGROUP:0x3a02 the briefing entry built: 38 N + 6 x 44 pixels, N = the pool-0
// in-use count, black, slot s's 32 x 32 cell at (38 s + 6, 6) (world.md "Extraction"). Null
// before the first briefing entry.
gfx::ImagePtr icon_strip();
#endif

// seg9:0x60bc's calls after the ships (seg11:0x3cc2(player) and the HUD repaint): src/hud sets
// it; the world hooks run it after start_wave (M3 s2 fixer).
void on_wave_start(std::function<void(World&)> f);
// The image work of the placement seg9:0x6624 at the briefing entry: seg14:0x0(-1)'s sheets
// (seg14:0x11a8 into slot 0x28 + i) and flame strip (seg10:0x5d3c, slot 0x22), which phase 11
// of the briefing draws from [BYTES seg9:0x688a seg14:0xc7e seg14:0xd32]. src/hud sets it; the
// world hooks run it after enter_briefing's placement (M5 s2 BRIEFING).
void on_briefing_placement(std::function<void(World&)> f);
// The design mass DGROUP:0x9704 + 0xad·design of the ship set-up, for seg11:0x1bcc (0 for a
// design the set-up did not make) (M3 s2 fixer).
std::int16_t design_mass(int design);

}  // namespace is::world
