// The wave's integration (src/hud/wave_win32.cpp, M3 s2 fixer): the calls of the wave pass and
// frame reach the drawing, the radar, the status tick and the HUD. Win32 only.
#pragma once

#ifdef _WIN32
#include <windows.h>

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
// The METAL pattern brush DGROUP:0x500a of the HUD's canvas (src/hud/hooks.cpp), null before
// seg11:0x0 ran.
HBRUSH metal_brush();
}  // namespace is::hud
#endif
