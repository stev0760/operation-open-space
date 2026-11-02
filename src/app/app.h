// The Win32 program: the shared state of WinMain, the message loop and the window procedure.
// The fields carry the DGROUP word of INSPACE.EXE they stand for, so a reader can check them
// against docs/re/shell.md, globals.md and timing.md. Win32 W APIs only (ADR 0003): game text
// stays cp1252 and converts at the call through widen().
#pragma once

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include "assets/asset_store.h"
#include "gfx/gfx.h"
#include "settings/settings.h"
#include "shell/edition.h"
#include "shell/frame_scheduler.h"
#include "shell/game_state.h"
#include "shell/geometry.h"
#include "shell/window_keys.h"

namespace is::app {

using shell::Tick;

// Private messages of the main window (globals.md "WndProc", shell.md "Private messages").
constexpr UINT kMsgShow = 0x914;        // ShowWindow(SW_SHOWNORMAL), SetActiveWindow
constexpr UINT kMsgPaint = 0x978;       // the paint routine seg2:0x970
constexpr UINT kMsgAbort = 0x991;       // abort to the intro
constexpr UINT kMsgAltKey = 0x99b;      // a key with Alt held
constexpr UINT kMsgCtrlKey = 0x99e;     // a key with Ctrl held
constexpr UINT kMsgShipWatch = 0x9a0;   // create IS_WATCH
constexpr UINT kMsgMenuReload = 0x9a5;  // seg2:0x16a2
constexpr UINT kMsgIconWatch = 0x9aa;   // toggle IS_ICON
constexpr UINT kMsgAllShips = 0x9b4;    // toggle IS_SMART
constexpr UINT kMsgInput9c5 = 0x9c5;    // input path
constexpr UINT kMsgCreated = 0x9cf;     // posted by WM_CREATE
constexpr UINT kMsgStart = 0x9d0;       // posted by the 0x9cf handler
constexpr UINT kMsgDemo = 0x9f0;        // DEMO command line
constexpr UINT kMsgPlay = 0x9fa;        // PLAY command line
constexpr UINT kMsgMusic = 0xa04;       // seg10:0x7480(wParam)
// Joystick messages of the input path (mmsystem.h).
constexpr UINT kMmJoy1Move = 0x3a0, kMmJoy1ButtonDown = 0x3b5, kMmJoy1ButtonUp = 0x3b7;

// The window timer's id and period [BYTES seg6:0xa0f].
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 1000;

// "Inner Space" (DGROUP:0x82), the class and application name.
inline constexpr const wchar_t* kMainClass = L"Inner Space";

struct App {
    HINSTANCE instance = nullptr;
    HWND main = nullptr;            // DGROUP:0x16, set in WM_CREATE
    HMENU menu = nullptr;           // DGROUP:0x90fa, menu 101 (or 102)
    HMENU sys_menu = nullptr;       // DGROUP:0x14, set by the first SC_RESTORE
    HACCEL accel = nullptr;         // MainAccel
    HFONT font = nullptr;           // DGROUP:0x90fe
    HICON icon = nullptr;           // GROUP_ICON GAME of INSPACE.EXE

    std::unique_ptr<assets::AssetStore> store;
    std::unique_ptr<settings::IniFile> ini;
    std::filesystem::path asset_dir;   // the game directory (ADR 0002)
    std::filesystem::path ini_path;    // the resolved INSPACE.INI (ADR 0004 decision 2)
    std::filesystem::path local_path;  // DGROUP:0x9078, INI LocalPath (ADR 0004 decision 5)
    // The renderer (ADR 0006, ADR 0007): the surface is DGROUP:0x13dc's drawing target, the
    // palette handle DGROUP:0x920e (src/gfx/gfx.h).
    std::unique_ptr<gfx::Gfx> gfx;
    int scale_key = 0;  // [Port] Scale (ADR 0006 decision 3)
    shell::WindowKeys keys;  // Full DGROUP:0x20, Show DGROUP:0x1d20, Menu DGROUP:0x90f6, ...
    shell::GameState game;
    shell::FrameScheduler scheduler;
    shell::PlayArea area;  // DGROUP:0x90ea, 0x10, 0x90ec, 0x8e5a, in logical pixels
    shell::Clip clip;      // DGROUP:0x1d30 and the clip region DGROUP:0x12, logical pixels

    int screen_cx = 0, screen_cy = 0;  // SM_CXSCREEN, SM_CYSCREEN
    int depth = 0;                     // DGROUP:0x90ee: max(BITSPIXEL, PLANES)
    // DGROUP:0x920c, 0x11aa, 0x922a, 0x922c and 0x9224 with the decoded name: what the last
    // edition check seg10:0x885e left (run_edition_check).
    shell::Edition edition;
    int play_mode = 0;  // DGROUP:0x9134 (INI PlayMode, read at game start)

    bool one_shot = false;       // DGROUP:0x1c: the first WM_TIMER is the one-shot
    bool timer_seen = false;     // DGROUP:0x1e: set by the one-shot
    bool idle_allowed = false;   // DGROUP:0x8f66: set by the 0x9d0 handler
    bool running = false;        // DGROUP:0x8f68: not paused
    bool calibrated = false;     // DGROUP:0x1212: the Calibrate measurement is over
    Tick calibrate_start = 0;    // DGROUP:0x120e
    bool calibrate_started = false;
    bool frame_running = false;  // DGROUP:0x968c
    bool torn_down = false;      // DGROUP:0x49e
    bool focus_own = false;      // DGROUP:0x8e5c: which focus-watch command ran last
    bool hud_dirty = false;      // DGROUP:0x8f64
    bool relayout = false;       // DGROUP:0x1038: the next wave frame runs seg2:0xba4 (M3)
    Tick start_tick = 0;         // DGROUP:0x1d3c
    POINT start_cursor{};        // DGROUP:0x1d48
};

extern App g;

// The 32-bit millisecond tick (ADR 0001 decision 5), from the monotonic high-resolution clock,
// counted from start-up. The original's GetCurrentTime counts from boot and its deadlines
// compare unsigned (seg7:0x3f5), so a deadline armed shortly before the 32-bit wrap fires at
// once. Counting from start-up keeps the compares faithful and moves that wrap to 49.7 days of
// process uptime [DESIGN].
Tick now_tick();

// cp1252 game text to UTF-16 at the Win32 call (ADR 0003).
std::wstring widen(const std::string& cp1252);
// An ENG_LANG string as UTF-16; empty when the store has none.
std::wstring lang_string(std::uint16_t id);

// The port's log (UTF-8, ADR 0003 decision 5): standard error and OutputDebugString.
void log(const std::string& utf8);
// "unimplemented: <what> (<address>)": an action the shell reaches that a later stage ports.
void log_todo(const std::string& what, const char* address);

// A diagnostic line with the main window's class, styles, rectangles and caption [DESIGN].
void log_window(const char* when);

LRESULT CALLBACK main_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// The client size and the screen size in logical pixels: physical / the scale factor
// (ADR 0006 decision 2).
int logical(int physical);
// A mouse position from lParam in logical pixels, for the HUD hotspot tests (ADR 0006
// Consequences).
POINT logical_point(LPARAM lparam);
// Converts and shows the surface when it or the palette changed (the frame path).
void present_frame();

// The idle step seg2:0x1c08 (timing.md "Message loop and idle step"). Returns false when it
// ran a pass that ended early (pass() false: suspended, a deadline, routed input), so that the
// message loop does not spin on a frame the pass keeps from running.
bool idle_step();
// The state hooks of shell::GameState: state 0 is the intro of src/intro.
void install_state_hooks();

// The hook points of the parts that live outside src/app. main.cpp calls them right after
// install_state_hooks(), before the main window exists; each installs its own GameState hooks,
// handlers and windows. Their owners: src/dialogs/hooks.cpp, src/help/hooks.cpp and
// src/briefing/hooks.cpp.
void install_dialog_hooks(App& app);
void install_help_hooks(App& app);
void install_briefing_hooks(App& app);
void install_desk_hooks(App& app);  // src/desk/hooks.cpp: the desk scene of phase 6

// ---- access from outside src/app

// The flags of the last edition check (DGROUP:0x920c and its companions).
const shell::Edition& edition();
// seg10:0x885e(arg): runs the edition check again with the asset directory's INSPACE.EXE
// records, the registration values and the local date, keeps and returns the result. Callers:
// start-up (arg 1), the state-0 entry and title step 0 (through the intro host), and 'R' after
// the Order dialog returns non-zero (arg 0).
const shell::Edition& run_edition_check(int arg);
// PostMessage to the main window (the private kMsg* messages of WndProc). False when there is
// no main window or the post fails.
bool post(UINT msg, WPARAM wparam = 0, LPARAM lparam = 0);
// A state request: DGROUP:0x968e = state, DGROUP:0x13de = 1 (shell::GameState::request).
void request_state(int state);
// seg16:0x68d6(hwnd, id1, id2): string id1, then string id2 appended unless id2 = -1, in
// MessageBox(hwnd, text, DGROUP:0x9690 "Inner Space", 0x30 = MB_ICONEXCLAMATION) [BYTES
// seg16:0x68e4-0x691b]. The wave's death path passes (DGROUP:0x16, 2328, 2329), the main
// window [BYTES seg7:0x6c8-0x6d2]. While the box is up, WM_TIMER's focus test finds the box
// active and pauses: the caption reads "Inner Space - PAUSED.." (string 2044), and the first
// timer after OK resumes [BYTES seg2:0x1327 seg2:0x1341] [ORACLE core-0001]. Returns the
// MessageBox result (M4 s1 SCREENS).
int message_box_ids(HWND owner, int id1, int id2);

// The watch windows that 0x9aa and 0x9b4 toggle (seg2:0x4f0, seg2:0x513; Alt+I and Alt+R in a
// wave, seg11:0x716c and seg11:0x718b). The window's owner fills the slots: exists() stands for
// the handle DGROUP:0x9458 (IS_ICON) or DGROUP:0x95fe (IS_SMART) being non-zero, create() for
// seg7:0x641a or seg13:0x638a, destroy() for seg7:0x63f2 or seg13:0x6362. The handler sets
// IconWnd DGROUP:0x8f72 or SmWnd DGROUP:0x8f74 (App::keys) itself; an empty create or destroy
// slot logs [BYTES seg2:0x4f0 seg2:0x4f7 seg2:0x4fd seg2:0x505 seg2:0x50b seg2:0x513 seg2:0x51a
// seg2:0x520 seg2:0x528 seg2:0x52e].
struct WatchToggle {
    std::function<bool()> exists;
    std::function<void()> create;
    std::function<void()> destroy;
};
WatchToggle& icon_watch();   // IS_ICON, the icon column (world.md "Icon column")
WatchToggle& smart_watch();  // IS_SMART, the All Ships watch (shell.md "Watch windows")
// The surface-drawn child windows' WM_PAINT after the main window's emulated palette message
// erased the client (ADR 0007 decision 5): the HUD fills it to mark its IS_INFO panels due.
std::function<void()>& children_invalidated();

}  // namespace is::app
