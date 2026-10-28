// The HUD's hook point (hooks.cpp, Win32 only). The program's HUD and the slots the other roles
// fill are in hud/program.h.
#pragma once

namespace is::app {
struct App;
// Installs the hooks (main.cpp, after install_world_hooks and install_flight_hooks, whose
// state-1 entry and frame it wraps): the repaint of states 1 and 7, seg11:0x0's HUD loads at
// the state-1 entry, the IS_INFO panels after each wave frame, the scrollbar's relayout and
// seg11:0x2f0 when the wave states end.
void install_hud_hooks(App& app);
}  // namespace is::app
