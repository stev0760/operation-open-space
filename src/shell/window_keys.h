// The INSPACE.INI keys the shell reads at start-up and writes back at exit
// (docs/re/formats.md "INSPACE.INI [Inner Space]", "Window state and positions"; shell.md
// "Exit", INI write-back). Portable; the file layer is is::settings::IniFile.
#pragma once

#include <string>

#include "shell/geometry.h"

namespace is::settings {
class IniFile;
}

namespace is::shell {

struct WindowKeys {
    int full = 1;          // Full -> DGROUP:0x20 [BYTES seg6:0x14]
    Rect show;             // ShowLeft..ShowBottom -> DGROUP:0x1d20..0x1d26, default 0
    int icon_wnd = 1;      // IconWnd -> DGROUP:0x8f72
    int vscale = 0;        // VScale -> DGROUP:0xb6e
    int sm_wnd = 0;        // SmWnd -> DGROUP:0x8f74
    int sscale = 0;        // SScale -> DGROUP:0x95f8
    int menu = 0;          // Menu -> DGROUP:0x90f6 [BYTES seg6:0x180]
    // Border -> DGROUP:0xa78. Start-up does not read it: the initialised value is 1
    // [BYTES DGROUP:0xa78] until the rate-settings reader seg6:0xb3e stores the INI value
    // [BYTES seg6:0xd20 seg6:0xd51] (read_border).
    int border = 1;
    std::string language;  // Language, "" when absent (the asset store then keeps ENG)

    // DGROUP:0x8f6a: the Show keys are written back only while set.
    bool show_dirty = false;
};

// The start-up reads of seg6:0x0 (Full, the Show keys, IconWnd, VScale, SmWnd, SScale, Menu)
// and Language. Border keeps its initialised 1. The Show rectangle is not yet clamped
// (clamp_show_rect).
WindowKeys read_window_keys(const settings::IniFile& ini);
// OnTop (default 1), which seg6:0x1290 reads afresh at creation and in seg2:0xfc4
// [BYTES seg6:0x18b7 seg2:0x101d].
int read_on_top(const settings::IniFile& ini);
// FrameRateSet (default 36), read by seg6:0xb3e [BYTES seg6:0xb7d].
int read_frame_rate(const settings::IniFile& ini);
// Border (default 1) and Menu (default 0), which seg6:0xb3e reads again [BYTES seg6:0xd20
// seg6:0xd36]. It stores both when either differs from DGROUP:0xa78 or DGROUP:0x90f6 and
// then attaches or detaches the menu [BYTES seg6:0xd3e-0xd98].
struct BorderMenu {
    int border = 1;
    int menu = 0;
};
BorderMenu read_border_menu(const settings::IniFile& ini);

// seg6:0x33e: always Full, Menu, IconWnd, VScale, SmWnd and SScale; the Show keys while
// show_dirty, which it then clears. The order is the original's [BYTES seg6:0x37e seg6:0x3a4
// seg6:0x496 seg6:0x4bc seg6:0x4e2 seg6:0x508 seg6:0x630 seg6:0x656 seg6:0x67c seg6:0x6a2].
// The keys of windows the port does not have yet (Radar, Watch, Icon, Sm, Help) have no dirty
// flag set and are not written. Does not save the file.
void write_window_keys(settings::IniFile& ini, WindowKeys& keys);

// The constant Calibrate of ADR 0001 decision 9, which the port writes where seg6:0xa6c
// writes the measured value [BYTES seg6:0xb31].
constexpr int kCalibrate = 14000;

}  // namespace is::shell
