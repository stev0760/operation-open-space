#include "shell/window_keys.h"

#include "settings/settings.h"

namespace is::shell {

using settings::kGameSection;

WindowKeys read_window_keys(const settings::IniFile& ini) {
    WindowKeys k;
    k.full = ini.get_int(kGameSection, "Full", 1);           // seg6:0x14
    k.show.left = ini.get_int(kGameSection, "ShowLeft", 0);  // seg6:0x2b
    k.show.top = ini.get_int(kGameSection, "ShowTop", 0);    // seg6:0x41
    k.show.right = ini.get_int(kGameSection, "ShowRight", 0);    // seg6:0x57
    k.show.bottom = ini.get_int(kGameSection, "ShowBottom", 0);  // seg6:0x6d
    k.icon_wnd = ini.get_int(kGameSection, "IconWnd", 1);  // seg6:0x112
    k.vscale = ini.get_int(kGameSection, "VScale", 0);     // seg6:0x128
    k.sm_wnd = ini.get_int(kGameSection, "SmWnd", 0);      // seg6:0x13e
    k.sscale = ini.get_int(kGameSection, "SScale", 0);     // seg6:0x154
    k.menu = ini.get_int(kGameSection, "Menu", 0);         // seg6:0x180
    k.language = ini.get_string(kGameSection, "Language", "", 5);  // S 5
    return k;
}

int read_on_top(const settings::IniFile& ini) { return ini.get_int(kGameSection, "OnTop", 1); }

int read_frame_rate(const settings::IniFile& ini) {
    return ini.get_int(kGameSection, "FrameRateSet", 36);
}

BorderMenu read_border_menu(const settings::IniFile& ini) {
    BorderMenu b;
    b.border = ini.get_int(kGameSection, "Border", 1);  // seg6:0xd20
    b.menu = ini.get_int(kGameSection, "Menu", 0);      // seg6:0xd36
    return b;
}

void write_window_keys(settings::IniFile& ini, WindowKeys& k) {
    ini.set_int(kGameSection, "Full", k.full);  // seg6:0x37e
    ini.set_int(kGameSection, "Menu", k.menu);  // seg6:0x3a4
    // RadarLeft/RadarTop, WatchLeft/WatchRight/WatchTop: their windows come with M3 and M8.
    ini.set_int(kGameSection, "IconWnd", k.icon_wnd);  // seg6:0x496
    ini.set_int(kGameSection, "VScale", k.vscale);     // seg6:0x4bc
    ini.set_int(kGameSection, "SmWnd", k.sm_wnd);      // seg6:0x4e2
    ini.set_int(kGameSection, "SScale", k.sscale);     // seg6:0x508
    if (k.icon_dirty) {  // DGROUP:0x8fee [BYTES seg6:0x50d]
        ini.set_int(kGameSection, "IconLeft", k.icon_left);  // seg6:0x535
        ini.set_int(kGameSection, "IconTop", k.icon_top);    // seg6:0x55b
    }
    k.icon_dirty = false;
    // SmLeft/SmTop, HelpLeft/HelpTop: their windows come later.
    if (k.show_dirty) {
        ini.set_int(kGameSection, "ShowLeft", k.show.left);      // seg6:0x630
        ini.set_int(kGameSection, "ShowRight", k.show.right);    // seg6:0x656
        ini.set_int(kGameSection, "ShowTop", k.show.top);        // seg6:0x67c
        ini.set_int(kGameSection, "ShowBottom", k.show.bottom);  // seg6:0x6a2
    }
    k.show_dirty = false;  // seg6:0x6be
}

}  // namespace is::shell
