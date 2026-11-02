// The main window procedure seg2:0x224 (docs/re/globals.md "WndProc") and the handlers it
// calls: WM_CREATE seg6:0x18e0, WM_DESTROY and the teardown seg6:0x19bc, WM_SIZE seg2:0xa74,
// WM_MOVE seg2:0x11c0, WM_TIMER seg2:0x11f8, WM_SYSCOMMAND seg2:0xd78, WM_COMMAND seg2:0x13b2
// and the double click seg2:0x1046 (shell.md "Start-up", "Exit", "Window geometry").
// WM_QUERYNEWPALETTE and WM_PALETTECHANGED go to DefWindowProc: the port has no hardware palette
// and emulates the first message's effect at every activation instead (ADR 0007 decision 5,
// palette_message below). Drawing goes to the surface of src/gfx and WM_PAINT presents it.
#include <windows.h>
#include <mmsystem.h>

#include <cstdio>
#include <cwchar>
#include <exception>
#include <string>

#include "app/app.h"
#include "audio/audio.h"  // M7 audio rewiring (implementer C)
#include "dialogs/audio_options.h"
#include "dialogs/hooks.h"
#include "formats/bytes.h"
#include "gfx/palette_message.h"
#include "help/help.h"
#include "hud/program.h"  // the HUD scrollbar (M3 stage 2, the HUD)
#include "intro/intro.h"
#include "nav/nav.h"
#include "shell/command_table.h"
#include "world/flight_hooks.h"  // the wave's key latch (M3 stage 2, flight)

namespace is::app {

namespace {

using shell::Rect;

Rect window_rect(HWND hwnd) {
    RECT r;
    GetWindowRect(hwnd, &r);
    return {r.left, r.top, r.right, r.bottom};
}

void set_caption(std::uint16_t id) {
    const std::wstring s = lang_string(id);
    SetWindowTextW(g.main, s.c_str());
}

}  // namespace

int message_box_ids(HWND owner, int id1, int id2) {
    std::wstring text = lang_string(static_cast<std::uint16_t>(id1));  // seg9:0x955c
    if (id2 != -1) text += lang_string(static_cast<std::uint16_t>(id2));  // seg9:0x9580
    log("seg16:0x68d6: MessageBox " + std::to_string(id1) + " " + std::to_string(id2));
    // DGROUP:0x9690 is the program's name, "Inner Space" (the caption the oracle shows) [ORACLE
    // core-0001].
    return MessageBoxW(owner, text.c_str(), L"Inner Space", MB_ICONEXCLAMATION | MB_OK);
}

namespace {

// seg2:0x9c4: pause [BYTES seg2:0x9d1-0x9f9]. DGROUP:0x592 (the frames-per-second hold of the
// adaptive thresholds) has no port equivalent (ADR 0001 decision 7).
void pause_game() {
    g.running = false;  // DGROUP:0x8f68 = 0
    set_caption(2044);  // "Inner Space - PAUSED.."
}

// seg2:0xa06: resume [BYTES seg2:0xa1a-0xa66].
void resume_game() {
    BYTE ks[256];
    if (GetKeyboardState(ks)) {
        for (int vk : {VK_SHIFT, VK_CONTROL, VK_MENU, VK_CAPITAL, VK_SPACE}) ks[vk] &= 0x7f;
        SetKeyboardState(ks);
    }
    set_caption(2337);  // "Inner Space"
    g.running = true;   // DGROUP:0x8f68 = 1
}

// seg2:0xfc4: to full screen [BYTES seg2:0xfd1-0x1039].
void go_full_screen() {
    g.keys.full = 1;
    const LONG style = GetWindowLongW(g.main, GWL_STYLE);
    SetWindowLongW(g.main, GWL_STYLE, static_cast<LONG>(static_cast<DWORD>(style) &
                                                        shell::kFullScreenMask));
    // Win32 caches the frame of a window; SWP_FRAMECHANGED applies the new style before the
    // move [DESIGN].
    SetWindowPos(g.main, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    MoveWindow(g.main, 0, 0, g.screen_cx, g.screen_cy, TRUE);
    if (shell::read_on_top(*g.ini))  // seg6:0x1290 reads OnTop afresh [BYTES seg2:0x101d]
        SetWindowPos(g.main, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

void add_windowed_bits() {
    const LONG style = GetWindowLongW(g.main, GWL_STYLE);
    SetWindowLongW(g.main, GWL_STYLE,
                   static_cast<LONG>(static_cast<DWORD>(style) | shell::kWindowedBits));
    SetWindowPos(g.main, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// The paint routine seg2:0x970 [BYTES seg2:0x97a-0x9b7]. It first forces DGROUP:0x8f64 in
// state 0 while the intro phase DGROUP:0x95b8 is 4 or 5 and in states 6 and 8. Then, while
// DGROUP:0x8f64 is set, it runs seg11:0xafe: the HUD layout (M3) and the state's repaint; a
// clear flag is only set, and nothing is drawn. A state that does not draw yet (no repaint
// hook) shows a black play area, PatBlt BLACKNESS on the surface [DESIGN]; then the surface is
// presented.
void paint_routine() {
    const int s = g.game.state();
    if (s == shell::kIntro) {  // [BYTES seg2:0x97a-0x98f]
        if (const intro::Intro* in = intro::Intro::current())
            if (in->phase() >= 4 && in->phase() < 6) g.hud_dirty = true;
    }
    if (s == shell::kAmbulance || s == shell::kJustice) g.hud_dirty = true;  // seg2:0x9a3
    if (!g.hud_dirty) {  // seg2:0x9b7
        g.hud_dirty = true;
        return;
    }
    // seg11:0xafe (seg2:0x9b0). TODO M3: it also lays out the HUD children (shell::hud_layout).
    if (s >= 0 && s < shell::kStateCount && !g.game.hooks().repaint[s] && !g.gfx->surface().empty())
        PatBlt(g.gfx->dc(), 0, 0, g.area.w, g.area.h, BLACKNESS);
    g.game.repaint();
    g.gfx->present(g.main, true);
}

// The palette messages, emulated (ADR 0007 decision 5) [DESIGN]: WM_QUERYNEWPALETTE seg2:0x5cb
// reached the main window at each of its activations on a 256-colour Windows, the first one
// and the return from every dialog or child window (the game-start dialogs, the Navigation
// window, Help, Notepad); start-0001 saw it there 8 times. With a palette (DGROUP:0x920e) its
// InvalidateRect(NULL, TRUE) + UpdateWindow ran WM_PAINT seg2:0x7ac at once: the paint routine
// in states 1 and 7, the post of 0x978 elsewhere, and the black erase of the class brush. The
// port runs that sequence on the surface (src/gfx/palette_message.h) and presents it at once,
// as UpdateWindow did, so the black is on the screen before the next dialog draws.
void palette_message() {
    const bool done = gfx::emulate_query_new_palette(
        *g.gfx, g.game.state(), g.timer_seen, [] { paint_routine(); },
        [] { PostMessageW(g.main, kMsgPaint, 0, 0); });
    // InvalidateRect(main, NULL, TRUE) also invalidates the child windows, as main has no
    // WS_CLIPCHILDREN (style 0x80CF0000 or WS_POPUP) [REF Win32 SDK RedrawWindow] [BYTES
    // seg6:0x180e seg6:0x1816]: the IS_INFO panels, drawn on the surface (ADR 0013 decision 4),
    // paint again after the erase as the oracle's do [ORACLE wave-0001 35-watch-icons-on.png]
    // (M3 s4 fixer).
    if (done && children_invalidated()) children_invalidated()();
    // UpdateWindow (seg2:0x607) handled the whole client region, so no WM_PAINT for main was
    // left pending: one that was (a ShowWindow's) would run seg2:0x970 again without its erase
    // and bring the erased HUD back. The port validates main to the same effect [DESIGN ADR 0013
    // decision 3] [BYTES seg2:0x5ff seg2:0x607]. The wave start's repaint after the erase is the
    // IS_ICON column's (src/hud/icon_column_win32.cpp) [ORACLE win98-0001] [DESIGN].
    if (done && g.main) {
        ValidateRect(g.main, nullptr);
        g.gfx->present(g.main, true);
    }
}

// seg6:0x19bc: the teardown, once [BYTES seg6:0x19c9-0x1a5a].
void teardown() {
    if (g.torn_down) return;  // DGROUP:0x49e
    g.torn_down = true;
    nav::close();  // 1: the Navigation window and its keys (seg6:0x2ebc)
    g.game.teardown();  // 2: the current state (seg2:0x1d58)
    // 3: seg10:0x22e(1) -> seg10:0x136, whose first act is the music wrapper seg10:0x7480(-1, 1)
    // (the MCI_PAUSE the oracle shows at game end), then ISPACE ordinal 7 command 1 [BYTES
    // seg6:0x19e4 seg10:0x252 seg10:0x148 seg10:0x22e] (M7 s3 fixer).
    audio::music(-1, 1);
    log("music seg10:0x7480(-1, 1) at exit (seg10:0x148)");
    audio::shutdown();
    // 4: the clip region (the port keeps a rectangle).
    if (g.main) SetMenu(g.main, nullptr);  // 5
    if (g.menu) {
        DestroyMenu(g.menu);
        g.menu = nullptr;
    }
    // 6: GDI and memory release (seg10:0x330): the images are released by their owners and the
    // surface with the renderer at the end of WinMain.
    // 7: the INI write-back seg6:0x33e, bracketed by the wait cursor [BYTES seg6:0x34b
    // seg6:0x6a7].
    SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    try {
        shell::write_window_keys(*g.ini, g.keys);
        g.ini->save();
        log("INI written back");
    } catch (const std::exception& x) {
        log(std::string("INI write-back: ") + x.what());
    }
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    KillTimer(g.main, kTimerId);  // 8
    if (g.font) {                 // 9
        DeleteObject(g.font);
        g.font = nullptr;
    }
    g.gfx->palette_delete();  // 10: seg10:0x6e26, ordinal 4 command 2 [BYTES seg6:0x1a53]
    // 11: the DLLs (none).
}

// seg2:0xd78 SC_CLOSE: the exit sequence of shell.md "Exit" (src/dialogs/hooks.cpp).
void exit_sequence() { dialogs::exit_sequence(); }

// seg2:0xd78: WM_SYSCOMMAND. Returns true when DefWindowProc also runs.
bool sys_command(WPARAM wparam) {
    const shell::SysEntry e = shell::sys_command(static_cast<std::uint32_t>(wparam));
    switch (e.action) {
    case shell::SysAction::kToggleMenu: {  // seg2:0xf4d
        HMENU sys = GetSystemMenu(g.main, FALSE);
        g.keys.menu = g.keys.menu ? 0 : 1;
        SetMenu(g.main, g.keys.menu ? g.menu : nullptr);
        CheckMenuItem(sys, shell::kScMainMenus, g.keys.menu ? MF_CHECKED : MF_UNCHECKED);
        DrawMenuBar(g.main);
        break;
    }
    case shell::SysAction::kGameOptions:  // seg2:0xfa7
        PostMessageW(g.main, WM_COMMAND, shell::kScGameOptions, 0);
        break;
    case shell::SysAction::kPause: pause_game(); break;  // seg2:0xe70
    case shell::SysAction::kFullScreen:                   // seg2:0xe7a
        if (g.timer_seen) go_full_screen();
        break;
    case shell::SysAction::kRestore: {  // seg2:0xe87
        g.running = true;               // DGROUP:0x8f68 = 1 [BYTES seg2:0xea6]
        MoveWindow(g.main, g.keys.show.left, g.keys.show.top, g.keys.show.width(),
                   g.keys.show.height(), TRUE);
        if (!g.sys_menu) {  // the first time: "&Main Menus" [BYTES seg2:0xeaf-0xf10]
            g.sys_menu = GetSystemMenu(g.main, FALSE);
            AppendMenuW(g.sys_menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(g.sys_menu, MF_STRING, shell::kScMainMenus, lang_string(2184).c_str());
            if (g.keys.menu) CheckMenuItem(g.sys_menu, shell::kScMainMenus, MF_CHECKED);
            DrawMenuBar(g.main);
        }
        if (g.keys.full) go_full_screen();       // seg2:0xf1d
        if (g.idle_allowed) set_caption(2337);   // seg2:0xf20-0xf45
        break;
    }
    case shell::SysAction::kClose: exit_sequence(); break;
    case shell::SysAction::kDefault: break;
    }
    return e.def_window_proc;
}

// seg2:0x13b2: WM_COMMAND.
void command(WORD id) {
    const shell::Action a = shell::command_action(id);
    switch (a.kind) {
    case shell::ActionKind::kNone: return;
    case shell::ActionKind::kPost:
        PostMessageW(g.main, a.msg, a.wparam, 0);
        return;
    case shell::ActionKind::kCall: break;
    }
    char what[80];
    switch (a.call) {
    case shell::Call::kOptions: dialogs::options_command(); return;  // seg10:0x7f18 (M7 rewiring)
    case shell::Call::kSave: {
        const int arg = shell::save_argument(g.game.state());
        if (arg < 0) return;
        dialogs::save_step(arg);  // seg10:0x8568(arg), src/dialogs/hooks.cpp
        return;
    }
    case shell::Call::kHelpTopic:
        std::snprintf(what, sizeof what, "Online Help topic %d", a.arg);
        log_todo(what, "seg2:0x88e");
        return;
    case shell::Call::kF1:
        // DGROUP:0xa7c, the targeting display, comes with the HUD (M3); without it F1 calls
        // seg2:0x190a(0, 0).
        help::open_manual(0);
        return;
    case shell::Call::kHelpFile:
        help::open_manual(static_cast<std::uint32_t>(a.arg));  // seg2:0x190a(arg, 0)
        return;
    case shell::Call::kBossKey:  // seg2:0x1486
        if (g.keys.full) add_windowed_bits();
        PostMessageW(g.main, WM_SYSCOMMAND, SC_MINIMIZE, 0);  // seg2:0x14b5
        return;
    }
}

// seg2:0x1046: the double click toggles the mode once DGROUP:0x1e is set.
void double_click(LPARAM lparam) {
    // TODO M3: the three HUD hotspots come first [BYTES seg2:0x1046-0x114f], tested with the
    // position in logical pixels (ADR 0006 Consequences).
    const POINT at = logical_point(lparam);
    (void)at;
    if (!g.timer_seen) return;  // seg2:0x1152
    if (g.keys.full) {
        add_windowed_bits();  // seg2:0x1177
        SetWindowPos(g.main, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        PostMessageW(g.main, WM_SYSCOMMAND, SC_RESTORE, 0);  // seg2:0x11a7
        g.keys.full = 0;                                       // seg2:0x11ac
    } else {
        go_full_screen();  // seg2:0x11b5
    }
}

// seg2:0x11f8: WM_TIMER.
void timer() {
    if (g.one_shot) {  // seg2:0x1205-0x1241
        g.one_shot = false;
        g.keys.show_dirty = false;
        // DGROUP:0x8f6e has no writer, so 0x8e2 is never posted here [BYTES seg2:0x1214].
        PostMessageW(g.main, kMsgShow, 0, 0);
        g.timer_seen = true;
        log_window("at the first WM_TIMER");
        return;
    }
    // The cursor nudge (DGROUP:0x1d44) is left to the window ADR (ADR 0001 decision 10).
    if (!g.idle_allowed) return;  // seg2:0x12b6
    // Win16's GetActiveWindow is system-wide; Win32's foreground window is its counterpart
    // [DESIGN].
    HWND active = GetForegroundWindow();
    DWORD pid = 0;
    if (active) GetWindowThreadProcessId(active, &pid);
    const bool own_task = active && pid == GetCurrentProcessId();
    if (!own_task) {
        if (g.focus_own) {  // seg2:0x12e7: seg10:0x8686, ISPACE ordinal 7 command 3 (M7)
            g.focus_own = false;
            audio::focus(false);  // M7 rewiring: ISPACE 7:3
            log("focus lost: ISPACE 7:3 (seg10:0x8686)");
        }
    } else if (!g.focus_own && !IsIconic(g.main)) {  // seg2:0x1302: 7:4 (M7)
        g.focus_own = true;
        audio::focus(true);  // M7 rewiring: ISPACE 7:4
        log("focus back: ISPACE 7:4 (seg10:0x86a8)");
    }
    // DGROUP:0x934c and the Navigation window DGROUP:0xd1e also count as the game's own.
    if (active != g.main && active != nav::window()) {
        if (g.running) pause_game();  // seg2:0x1327
    } else if (!g.running && !IsIconic(g.main)) {
        resume_game();  // seg2:0x1341
    }
}

// The input path of globals.md "Input path".
void input(UINT msg, WPARAM wparam, LPARAM lparam) {
    // The wave's key latch sees every key-down and key-up, Alt's included, before any gate:
    // GetAsyncKeyState reads the keyboard, not the game's input path [DESIGN ADR 0013 decision 6]
    // (src/world/flight_hooks.cpp).
    if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)
        world::feed_key(msg, wparam, lparam);
    // The latch only; DefWindowProc follows. seg2's WndProc has no case for WM_SYSKEYDOWN,
    // WM_SYSKEYUP, WM_CHAR or WM_SYSCHAR: they reach DefWindowProc [BYTES seg2:0x2d4 seg2:0x38a
    // seg2:0x445]. Alt+I, Alt+R and Alt+plus/minus come through the message loop instead:
    // TranslateAccelerator with MainAccel while the main window is active (main.cpp) turns them
    // into WM_COMMAND 491, 471, 401 and 411 [BYTES seg2:0x191 seg2:0x1a5 ENG_LANG.DLL ACCELERATOR
    // MAINACCEL], whose handler seg2:0x13b2 posts 0x99b with 'I', 'R', VK_ADD or VK_SUBTRACT
    // (src/shell/command_table.cpp) [BYTES seg2:0x646 seg2:0x13c5 seg2:0x13fb seg2:0x13da
    // seg2:0x13e5]; 0x99b then takes WM_KEYDOWN's path below into seg11:0x6aa8 and seg11:0x6b64
    // [BYTES seg2:0x345 seg2:0x70e].
    if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) return;
    // DEMO mode (DGROUP:0x90f8 and DGROUP:0x8e5e) quits on input after 4000 ms; the port has
    // no DEMO command line [OPEN] (M3, shell.md "DEMO and PLAY"). Otherwise input needs DGROUP:0x1212, DGROUP:0x8f68 and
    // DGROUP:0x3b94 <= DGROUP:0x1e50; DGROUP:0x3b94 belongs to the wave (M3) and is 0 here.
    if (!g.calibrated || !g.running) return;
    const shell::Input in{msg, static_cast<std::uint32_t>(wparam), static_cast<std::int32_t>(lparam)};
    if (!g.frame_running) {  // seg2:0x783-0x79d: store it and call GameFrame
        g.game.post_input(in);
        idle_step();
    }
    // During a frame the router runs at once [BYTES seg2:0x77b]; the port's frames do not
    // dispatch messages, so this cannot happen yet.
}

// WM_RBUTTONDOWN seg2:0x134c: POPUPMENU's popup at the click.
void popup_menu(LPARAM lparam) {
    try {
        const auto t = g.store->menu_template("POPUPMENU");
        HMENU m = LoadMenuIndirectW(reinterpret_cast<const MENUTEMPLATEW*>(t.data()));
        if (!m) return;
        POINT p{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
        ClientToScreen(g.main, &p);
        TrackPopupMenu(GetSubMenu(m, 0), 0, p.x, p.y, 0, g.main, nullptr);
        DestroyMenu(m);
    } catch (const std::exception& x) {
        log(std::string("POPUPMENU: ") + x.what());
    }
}

// WM_CREATE seg6:0x18e0. Returns false when the window timer failed.
bool create(HWND hwnd) {
    g.main = hwnd;  // DGROUP:0x16 [BYTES seg2:0x390]
    // The surface at the client's logical size, black.
    RECT rc;
    GetClientRect(hwnd, &rc);
    g.gfx->set_client_size(rc.right, rc.bottom, g.scale_key);
    g.gfx->blacken();
    // seg6:0x190c: the palette (seg10:0x6e00) and scheme -1000, all white, in mode 2
    // (seg10:0x6e76) [BYTES seg6:0x18eb seg6:0x18fa seg6:0x1925 seg6:0x1941].
    g.gfx->palette_create();
    g.gfx->palette_scheme(-1000, 2);
    g.font = CreateFontW(14, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, 0, 0, PROOF_QUALITY,
                         VARIABLE_PITCH | FF_SWISS, L"Arial");  // seg6:0x1934
    // LoadBitmap "ALPHABET" belongs to the palette manager's command 0 (src/gfx/gfx.cpp).
    // seg6:0x2ca's class icon is main.cpp's load_game_icon; its INI Aux, QuickIntro and Rate
    // reads are left to their readers (the briefing and dialog hooks read QuickIntro) [OPEN]
    // (M3).
    if (!SetTimer(hwnd, kTimerId, kTimerMs, nullptr)) {  // seg6:0x9f8 [BYTES seg6:0xa0f]
        // Caption DGROUP:0x9690 "Inner Space", flags 0x30 [BYTES seg6:0xa29-0xa2f].
        MessageBoxW(hwnd, lang_string(2116).c_str(), widen("Inner Space").c_str(),
                    MB_ICONEXCLAMATION);
        PostMessageW(hwnd, WM_DESTROY, 0, 0);
        return false;
    }
    g.game.set(shell::kIntro);  // seg6:0x18f0
    g.torn_down = false;        // DGROUP:0x49e = 0
    PostMessageW(hwnd, kMsgCreated, 0, 0);  // seg6:0x1900
    return true;
}

// seg2:0x16a2: menu 102 when DGROUP:0x9134 is set, else 101, attached when Menu is set.
void reload_menu() {
    if (g.menu) {
        SetMenu(g.main, nullptr);
        DestroyMenu(g.menu);
    }
    try {
        const auto t = g.store->menu_template(g.play_mode ? 102 : 101);
        g.menu = LoadMenuIndirectW(reinterpret_cast<const MENUTEMPLATEW*>(t.data()));
    } catch (const std::exception& x) {
        g.menu = nullptr;
        log(std::string("menu: ") + x.what());
    }
    if (g.keys.menu) SetMenu(g.main, g.menu);
}

// seg2:0x4f0 and seg2:0x513: a window that exists is destroyed and its INI key cleared, else the
// key is set and the window created (the creator applies its own conditions, world.md "Icon
// column") [BYTES seg2:0x4f0 seg2:0x4f7 seg2:0x4fd seg2:0x505 seg2:0x50b seg2:0x513 seg2:0x51a
// seg2:0x520 seg2:0x528 seg2:0x52e].
void toggle_watch(WatchToggle& w, int& key, const char* what, const char* create_at,
                  const char* destroy_at) {
    if (w.exists && w.exists()) {
        key = 0;
        if (w.destroy)
            w.destroy();
        else
            log_todo(std::string(what) + " destroy", destroy_at);
    } else {
        key = 1;
        if (w.create)
            w.create();
        else
            log_todo(std::string(what) + " create", create_at);
    }
}

}  // namespace

WatchToggle& icon_watch() {
    static WatchToggle w;
    return w;
}
WatchToggle& smart_watch() {
    static WatchToggle w;
    return w;
}
std::function<void()>& children_invalidated() {
    static std::function<void()> f;
    return f;
}

// A diagnostic line for the headless checks: class, styles, rectangles, caption and the
// topmost flag of the main window [DESIGN].
void log_window(const char* when) {
    wchar_t cls[64] = L"";
    GetClassNameW(g.main, cls, 64);
    wchar_t title[128] = L"";
    GetWindowTextW(g.main, title, 128);
    const Rect r = window_rect(g.main);
    RECT c;
    GetClientRect(g.main, &c);
    const auto style = static_cast<unsigned long>(GetWindowLongW(g.main, GWL_STYLE));
    const auto ex = static_cast<unsigned long>(GetWindowLongW(g.main, GWL_EXSTYLE));
    const std::u16string cls16(cls, cls + wcslen(cls)), title16(title, title + wcslen(title));
    char buf[512];
    std::snprintf(buf, sizeof buf,
                  "window %s: class \"%s\" title \"%s\" style 0x%08lx exstyle 0x%08lx "
                  "rect %d,%d,%d,%d client %ldx%ld menu %s state %d scale %d logical %dx%d",
                  when, fmt::utf8_from_utf16(cls16).c_str(), fmt::utf8_from_utf16(title16).c_str(),
                  style, ex, r.left, r.top, r.right, r.bottom, c.right, c.bottom,
                  GetMenu(g.main) ? "yes" : "no", g.game.state(), g.gfx ? g.gfx->factor() : 0,
                  g.gfx ? g.gfx->logical_width() : 0, g.gfx ? g.gfx->logical_height() : 0);
    log(buf);
}

namespace {

LRESULT handle(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
        if (!create(hwnd)) PostQuitMessage(0);  // seg6:0x19b0
        g.one_shot = true;                      // seg2:0x398: DGROUP:0x1c = 1
        return 0;
    case WM_DESTROY:  // seg2:0x833
        teardown();
        PostQuitMessage(0);
        g.main = nullptr;
        return 0;
    case WM_ENDSESSION:  // seg2:0x823
        if (wparam) teardown();
        break;
    case WM_MOVE:  // seg2:0x11c0
        if (!IsIconic(hwnd)) {
            if (!g.keys.full) g.keys.show = window_rect(hwnd);
            g.keys.show_dirty = true;  // DGROUP:0x8f6a = 1 [BYTES seg2:0x11ea]
        }
        return 0;
    case WM_SIZE:  // seg2:0xa74, then DefWindowProc
        if (!IsIconic(hwnd)) {
            const Rect r = window_rect(hwnd);
            if (shell::needs_full_screen_move(g.keys.full != 0, r, g.screen_cx, g.screen_cy)) {
                MoveWindow(hwnd, 0, 0, g.screen_cx, g.screen_cy, TRUE);  // seg2:0xaf5
            } else {
                // Logical pixels (ADR 0006): the surface follows the client size, and the play
                // area is the client and the screen divided by the factor.
                g.gfx->set_client_size(LOWORD(lparam), HIWORD(lparam), g.scale_key);
                g.area = shell::play_area_from_size(
                    static_cast<std::uint16_t>(logical(LOWORD(lparam))),
                    static_cast<std::uint16_t>(logical(HIWORD(lparam))), logical(g.screen_cx),
                    logical(g.screen_cy));
            }
            g.area.hud_top = g.area.h - 85;  // seg2:0xb51, on both paths
            audio::set_area(g.area.w, g.area.play_h);  // DGROUP:0x90ea, 0x90ec (M7 fixer)
            g.clip = shell::clip_rect(g.area, g.keys.border != 0, g.play_mode != 0);  // seg2:0xca4
            if (g.timer_seen) g.hud_dirty = true;  // seg2:0xb61
            if (!g.keys.full) {                    // seg2:0xb67-0xb7c
                g.keys.show_dirty = true;
                g.keys.show = window_rect(hwnd);
            }
        }
        break;
    case WM_ACTIVATE:  // DefWindowProc, then the emulated WM_QUERYNEWPALETTE [DESIGN]
        // Windows sends WM_QUERYNEWPALETTE to the window being activated, once per activation;
        // WM_ACTIVATEAPP precedes the WM_ACTIVATE of the same activation, so this one point
        // covers both (ADR 0007 decision 5).
        DefWindowProcW(hwnd, msg, wparam, lparam);
        if (LOWORD(wparam) != WA_INACTIVE) palette_message();
        return 0;
    case WM_SETFOCUS:  // seg2:0x7fa
        if (!IsIconic(hwnd) && !g.running) resume_game();
        return 0;
    case WM_PAINT: {  // seg2:0x7ac, then DefWindowProc's BeginPaint and EndPaint
        if (g.timer_seen) {
            const int s = g.game.state();
            if (s == shell::kWave || s == shell::kHelp)
                paint_routine();
            else
                PostMessageW(hwnd, kMsgPaint, 0, 0);
        }
        // The port presents the surface into the update region itself [DESIGN].
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (!g.gfx->present_dc(dc, rc.right, rc.bottom, true))
            FillRect(dc, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: {  // the presented surface covers the client area [DESIGN]
        // seg2:0x224 leaves WM_ERASEBKGND to DefWindowProc, which fills the client with the
        // class brush BLACK_BRUSH, clipped to the update region that the wParam DC's clip box
        // bounds [REF Win32 SDK WM_ERASEBKGND]. The erase is kept on the surface, over the
        // logical cover of that box [DESIGN]: in state 0, which draws only what changes, before
        // the repaint seg11:0xafe that 0x978 runs; in states 1 and 7 after it, as seg2:0x7ac runs
        // seg2:0x970 before DefWindowProc's BeginPaint [BYTES seg2:0x7c4 seg2:0x7c7 seg6:0x14a8]
        // (ADR 0013 decisions 1 and 3); the IS_INFO children paint again after it
        // (children_invalidated). At the wave start the IS_ICON column's reactivation of main
        // is followed by a second repaint, so the full status display stands from the first
        // frame (src/hud/icon_column_win32.cpp) [ORACLE win98-0001] [DESIGN].
        // Other states black the play area in the paint routine until they draw.
        const int erase_state = g.game.state();
        const bool wave_states = erase_state == shell::kWave || erase_state == shell::kHelp;
        if ((erase_state == shell::kIntro || wave_states) && !g.gfx->surface().empty()) {
            RECT cb;
            if (GetClipBox(reinterpret_cast<HDC>(wparam), &cb) != ERROR) {
                const Rect r = shell::logical_cover(Rect{cb.left, cb.top, cb.right, cb.bottom},
                                                    g.gfx->factor(), g.gfx->logical_width(),
                                                    g.gfx->logical_height());
                if (r.width() > 0 && r.height() > 0) {
                    PatBlt(g.gfx->dc(), r.left, r.top, r.width(), r.height(), BLACKNESS);
                    if (wave_states && children_invalidated()) children_invalidated()();
                }
            }
        }
        return 1;
    }
    case WM_DPICHANGED:  // per-monitor DPI awareness v2 (ADR 0006 decision 4)
        if (!g.keys.full) {
            const auto* r = reinterpret_cast<const RECT*>(lparam);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    case WM_GETMINMAXINFO: {  // seg2:0x3eb
        auto* mm = reinterpret_cast<MINMAXINFO*>(lparam);
        if (g.game.resizable()) {
            mm->ptMinTrackSize = {shell::kMinTrackWidth, shell::kMinTrackHeight};
        } else {
            const Rect r = window_rect(hwnd);
            mm->ptMinTrackSize = mm->ptMaxTrackSize = {r.width(), r.height()};
        }
        return 0;
    }
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:  // the wave's key latch only (input), then DefWindowProc
    case WM_SYSKEYUP:
        input(msg, wparam, lparam);
        break;
    case kMsgAltKey:
    case kMsgCtrlKey:
    case kMsgInput9c5:
    case kMmJoy1Move:
    case kMmJoy1ButtonDown:
    case kMmJoy1ButtonUp:
        input(msg, wparam, lparam);
        return 0;
    case WM_COMMAND:  // seg2:0x646: only once DGROUP:0x1212 is set
        if (g.calibrated) command(LOWORD(wparam));
        return 0;
    case WM_SYSCOMMAND:  // seg2:0x63b
        if (sys_command(wparam)) break;
        return 0;
    case WM_TIMER:  // seg2:0x814
        timer();
        return 0;
    case WM_VSCROLL:  // seg2:0x6a7: seg11:0x7798(code, pos), the HUD scrollbar
        hud::vscroll(LOWORD(wparam), HIWORD(wparam));
        return 0;
    case WM_LBUTTONDOWN:  // seg16:0x638a, the ambulance screen's buttons (M4)
        return 0;
    case WM_LBUTTONDBLCLK:  // seg2:0x65e
        if (g.calibrated && g.game.resizable()) double_click(lparam);
        return 0;
    case WM_RBUTTONDOWN:  // seg2:0x68e
        if (g.calibrated) popup_menu(lparam);
        return 0;
    case kMsgShow:  // seg2:0x7df
        ShowWindow(hwnd, SW_SHOWNORMAL);
        SetActiveWindow(hwnd);
        return 0;
    case kMsgPaint:  // seg2:0x7a6
        paint_routine();
        return 0;
    case kMsgCreated:  // seg2:0x458: DC, palette, seg10:0x6e26, seg10:0x6e00, seg10:0x6e76(8, 2)
        // The DC is the surface's; no palette is selected or realised (ADR 0007 decision 5).
        g.gfx->palette_delete();      // seg2:0x458
        g.gfx->palette_create();      // seg2:0x469
        g.gfx->palette_scheme(8, 2);  // seg2:0x484
        PostMessageW(hwnd, kMsgStart, 0, 0);  // seg2:0x4b6
        return 0;
    case kMsgStart:            // seg2:0x4be
        g.idle_allowed = true;  // DGROUP:0x8f66 = 1
        g.game.start(now_tick());  // seg6:0x1b3c
        return 0;
    case kMsgAbort:  // seg2:0x536 (src/dialogs/hooks.cpp)
        dialogs::abort_sequence();
        return 0;
    case kMsgShipWatch: log_todo("Ship Watch window", "seg13:0x5878"); return 0;
    case kMsgMenuReload: reload_menu(); return 0;  // seg2:0x4e9
    case kMsgIconWatch:  // seg2:0x358 -> seg2:0x4f0
        toggle_watch(icon_watch(), g.keys.icon_wnd, "IS_ICON", "seg7:0x641a", "seg7:0x63f2");
        return 0;
    case kMsgAllShips:  // seg2:0x35f -> seg2:0x513
        toggle_watch(smart_watch(), g.keys.sm_wnd, "IS_SMART", "seg13:0x638a", "seg13:0x6362");
        return 0;
    case kMsgDemo:
    case kMsgPlay: log_todo("DEMO and PLAY", "seg2:0x86a"); return 0;  // [OPEN] (M3)
    // seg2:0x4cf pushes only wParam; seg10:0x7480 reads a stale stack word as the flag
    // (sound.md "Wait or notify"); the port passes 1 [OPEN seg2:0x4cc] (M7 rewiring).
    case kMsgMusic: is::sound::music(static_cast<int>(static_cast<short>(wparam)), 1); return 0;
    case MM_MCINOTIFY:  // the MCI sequencer's notifications (ADR 0011 decision 3) (M7 rewiring)
        // Logged for the oracle comparison (sound-0001 lists every MM_MCINOTIFY) (M7 fixer).
        log("MM_MCINOTIFY wParam " + std::to_string(static_cast<unsigned>(wparam)));
        audio::on_mci_notify(static_cast<unsigned>(wparam), static_cast<unsigned long>(lparam));
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// Logs an exception caught in a window procedure; never throws.
void log_exception(UINT msg, const char* what) noexcept {
    try {
        char buf[64];
        std::snprintf(buf, sizeof buf, "window message 0x%x: ", msg);
        log(buf + std::string(what));
    } catch (...) {
    }
}

}  // namespace

// No exception may unwind through user32's frames: the procedure catches everything, logs it
// and leaves the message to DefWindowProc [DESIGN].
LRESULT CALLBACK main_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    try {
        return handle(hwnd, msg, wparam, lparam);
    } catch (const std::exception& x) {
        log_exception(msg, x.what());
    } catch (...) {
        log_exception(msg, "unknown exception");
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace is::app
