// The wave's integration (src/hud/wave_win32.cpp, M3 s2 fixer): the calls of the wave pass and
// frame reach the drawing, the radar, the status tick and the HUD. Win32 only.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <functional>
#include <utility>
#include <vector>

#include "gfx/gfx.h"
#include "hud/target.h"

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
// ---- M4 stage 2, track DISPLAY (src/hud/hooks.cpp)
// seg7:0x65aa(pool, rec): the IS_ICON column's row, the watch window (M5) and the left IS_INFO
// display's picture when its mark holds the object [BYTES seg7:0x65aa-0x6621].
void refresh_object(int pool, int rec);
// The icon fragments' bitmaps DGROUP:0x4e94 + 2k, made by the shatter seg7:0x2768 (GUN or
// CAPTURE); unset: none, and seg13:0x5f49's quarters are not drawn [OPEN].
void set_fragment_images(std::function<gfx::ImagePtr(int k)> f);
gfx::ImagePtr fragment_image(int k);
// The HUD's part of seg13:0x675e, which the ship mover seg13:0x0 calls at its head [BYTES
// seg13:0x15]: seg13:0x58dc(0, 0) when the left display's mark (DGROUP:0xa76 - 2) is not of the
// ship pool [BYTES seg13:0x67fd-0x6810] and seg7:0x64ac(0, 0, -1) [BYTES seg13:0x6816-0x681c].
// The ships' loop (seg13:0x6890), seg13:0x50b0(0, 0) and the tail from seg13:0x6824 are [OPEN].
// Unbound until the mover calls it (CAPTURE's src/world/flight.cpp; the fixer's link).
void pictures_tick();
// The launcher's PostMessage(DGROUP:0xa7e, WM_PAINT) [BYTES seg11:0x7e9a-0x7ea6]: the weapon
// panel repaints after the frame (for MISSILE's launch; until it is bound, the frame hook
// repaints when the line's inputs change).
void weapon_panel_repaint();
// seg13:0x5f49's inputs of icon rec (hud/target.h CellInputs::frags).
void fill_fragment_inputs(int rec, CellInputs& in);
// The METAL pattern brush DGROUP:0x500a of the HUD's canvas (src/hud/hooks.cpp), null before
// seg11:0x0 ran.
HBRUSH metal_brush();
}  // namespace is::hud
#endif
