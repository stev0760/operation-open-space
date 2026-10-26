// The Navigation window IS_NAV as the stage-3 stub (docs/re/shell.md "From ESC to the wave",
// steps 11 and 12): seg6:0x1c72 opens it, its procedure seg6:0x1f22 handles Enter, Help and
// SC_CLOSE, seg6:0x23d4 builds the children, seg6:0x2ebc closes it and writes the Nav keys, and
// seg6:0x301a is Enter. The directory tree, the file list, the race and duel boxes, the icon
// preview and the status line come with M3 (the tree walk seg6:0x37ca logs as unimplemented).
// The portable rules are nav_rules.h. Win32 only.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <functional>
#include <string>

namespace is::settings {
class IniFile;
}

namespace is::nav {

// What the window needs from the program.
struct Host {
    HINSTANCE instance = nullptr;
    HWND main = nullptr;  // DGROUP:0x16, the parent
    HICON icon = nullptr;  // the class icon (GROUP_ICON GAME)
    HFONT font = nullptr;  // DGROUP:0x90fe, the children's font
    settings::IniFile* ini = nullptr;
    std::function<void()> ini_changed;
    std::function<std::string(int)> lang;  // ENG_LANG.DLL, cp1252
    std::function<void(int)> sound;
    std::function<void(const std::string&, const char*)> todo;
    std::function<void(const std::string&)> log;
    std::function<int()> state;  // DGROUP:0x90f4
    // PatBlt(DGROUP:0x13dc, 0, 0, W, H, BLACKNESS) and the present [BYTES seg6:0x1d4a].
    std::function<void()> black_play_area;
    // DGROUP:0x91f8 and DGROUP:0x92e4 for Enter's rule.
    std::function<int()> wave;
    std::function<int()> restored;
    // DGROUP:0x9134 (PlayMode) and DGROUP:0x95bc (player game), for the demo pilot.
    std::function<int()> play_mode;
    std::function<int()> player;
    // A state request: DGROUP:0x13de = DGROUP:0x968e = s [BYTES seg6:0x369a seg6:0x36a0].
    std::function<void(int)> request_state;
    // DGROUP:0x1e74 = 0 and DGROUP:0x1e70 = now [BYTES seg6:0x36a5 seg6:0x36b2].
    std::function<void()> arm_debriefing;
    // Enter's accepted directory, for DGROUP:0x38c8 ("D:\", upper case, trailing backslash).
    std::function<void(const std::string&)> entered;
    // seg2:0x190a(5, 0), the Help button's WinHelp call [BYTES seg6:0x2305].
    std::function<void()> help;
    // DGROUP:0x49e: the teardown has run.
    std::function<bool()> torn_down;
};

// Registers the class IS_NAV with the procedure (CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, the
// GAME icon, IDC_ARROW, LTGRAY_BRUSH: start-up step 12's class), replacing the placeholder
// registration of src/app/main.cpp [DESIGN].
bool install(const Host& host);

// The main window and its font, once they exist (WM_CREATE comes after install()).
void attach(HWND main, HFONT font);

// seg6:0x1c72(arg) with the drive offset DGROUP:0x94ac.
void open(int arg, int drive_offset);
// DGROUP:0xd1e: the window, or null.
HWND window();
// seg6:0x2ebc: writes the Nav keys from DGROUP:0x6af0 and destroys the window.
void close();
// seg6:0x301a: 1 when it accepted Enter (and closed the window), 0 otherwise.
int enter();

}  // namespace is::nav
#endif
