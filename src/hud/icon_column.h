// The IS_ICON column window (docs/re/world.md "Icon column", M3 stage 4): the wave's icons, one
// 46-pixel row per pool-0 record, in an owned top-level window beside the main window. Its
// class (seg6:0x152b), its window procedure (seg7:0x6184), its creation (seg7:0x641a) and
// destruction (seg7:0x63f2), the row paint seg7:0x64ac and the cell seg13:0x5a6a /
// seg13:0x5c22 (hud/target.h plan_cell). Win32 only.
#pragma once

#ifdef _WIN32
#include <windows.h>

namespace is::hud {

// seg6:0x152b: RegisterClass("IS_ICON") [BYTES seg6:0x152b-0x1579].
void register_icon_column(HINSTANCE instance);
// seg13:0x65f2(0)'s column part at the wave start: when `IconWnd` DGROUP:0x8f72 != 0 and the
// column DGROUP:0x9458 does not exist, PostMessage(main, 0x9aa) [BYTES seg13:0x66af-0x66ca].
// Returns true when it posted (seg13:0x65f2 returns 1).
bool icon_column_wave_start();
// seg7:0x641a: the rows and CreateWindow("IS_ICON"), then SetActiveWindow(main). The 0x9aa
// handler seg2:0x4f0 (src/app/wndproc.cpp) calls it through app::icon_watch().
void icon_column_create();
// seg7:0x64ac(0, 1, rec): one row repainted on the column's own DC (seg7:0x65aa's pool-0 part).
void icon_column_row(int rec);
// seg7:0x63f2: DestroyWindow(DGROUP:0x9458), DGROUP:0x9458 = 0.
void icon_column_destroy();
// DGROUP:0x9458 != 0.
bool icon_column_exists();

}  // namespace is::hud
#endif
