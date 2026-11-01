// The wave's integration (src/hud/wave_win32.cpp, M3 s2 fixer): the calls of the wave pass and
// frame reach the drawing, the radar, the status tick and the HUD. Win32 only.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <utility>
#include <vector>

namespace is::app {
struct App;
}

namespace is::hud {
// Fills world::wave_links(), the radar's links() slots, the zoom source and the wave-start call
// (install_hud_hooks calls it).
void install_wave(app::App& app);
// seg9:0x22b4's rock images and seg11:0x0's radar part, once the surface exists (the HUD's
// state-1 entry calls it after seg11:0x0's HUD loads).
void load_wave_resources();
// The visible list of the last visibility pass: DGROUP:0x507c / DGROUP:0x50e0, DGROUP:0x9146
// entries, as sign-extended (pool, record) pairs (seg13:0x2206's input).
std::vector<std::pair<int, int>> wave_visible();
// The player's design words the panels read (src/hud/hooks.cpp), re-read at each wave start.
void refresh_player_design();
}  // namespace is::hud
namespace is::fmt {
struct ShipRecord;
struct BodyRecord;
}  // namespace is::fmt
namespace is::hud {
// The MASTER.SHP record and MASTER.BOD body refresh_player_design found (false before it found
// them), for the player's sheet (M5 s1 SPRITE).
bool player_design_records(fmt::ShipRecord& shp, fmt::BodyRecord& body);
// seg11:0x2f8c: the weapon picture DGROUP:0x5040 of the player's weapon (src/hud/hooks.cpp).
void weapon_picture_refresh();
// The METAL pattern brush DGROUP:0x500a of the HUD's canvas (src/hud/hooks.cpp), null before
// seg11:0x0 ran.
HBRUSH metal_brush();
}  // namespace is::hud
#endif
