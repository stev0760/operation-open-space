// The main window procedure seg2:0x224 (docs/re/globals.md "WndProc") and the handlers it
// calls: WM_CREATE seg6:0x18e0, WM_DESTROY and the teardown seg6:0x19bc, WM_SIZE seg2:0xa74,
// WM_MOVE seg2:0x11c0, WM_TIMER seg2:0x11f8, WM_SYSCOMMAND seg2:0xd78, WM_COMMAND seg2:0x13b2
// and the double click seg2:0x1046 (shell.md "Start-up", "Exit", "Window geometry").
// WM_QUERYNEWPALETTE and WM_PALETTECHANGED go to DefWindowProc: the port has no hardware palette
// (ADR 0007 decision 5). Drawing goes to the surface of src/gfx and WM_PAINT presents it.
#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <exception>
#include <string>

#include "app/app.h"
#include "formats/bytes.h"
#include "shell/command_table.h"

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

// The paint routine seg2:0x970. The HUD layout seg11:0xafe (M3) runs while DGROUP:0x8f64 is
// set; a clear flag is only set [BYTES seg2:0x9a3-0x9b7]. The state's repaint follows. A state
// that does not draw yet (no repaint hook) shows a black play area, PatBlt BLACKNESS on the
// surface [DESIGN]; then the surface is presented.
void paint_routine() {
    if (g.hud_dirty) {
        // TODO M3: seg11:0xafe lays out the HUD children (shell::hud_layout).
    } else {
        g.hud_dirty = true;
    }
    const int s = g.game.state();
    if (s >= 0 && s < shell::kStateCount && !g.game.hooks().repaint[s] && !g.gfx->surface().empty())
        PatBlt(g.gfx->dc(), 0, 0, g.area.w, g.area.h, BLACKNESS);
    g.game.repaint();
    g.gfx->present(g.main, true);
}

// seg6:0x19bc: the teardown, once [BYTES seg6:0x19c9-0x1a5a].
void teardown() {
    if (g.torn_down) return;  // DGROUP:0x49e
    g.torn_down = true;
    // 1: the Navigation window and its keys (stage 3).
    g.game.teardown();  // 2: the current state (seg2:0x1d58)
    // 3: ISPACE ordinal 7 command 1 (M7); 4: the clip region (the port keeps a rectangle).
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

// seg2:0xd78 SC_CLOSE: the exit sequence of shell.md "Exit" without its dialogs.
void exit_sequence() {
    log_window("at SC_CLOSE");
    // 1: sounds 0x6e and 11 + rand() % 3, a 450 ms wait and sound 0x50: TODO M7.
    g.game.set_suspended(true);  // 2: DGROUP:0x1a = 1 [BYTES seg2:0xe07]
    // 2-3: dialog #244 through ISPACE ordinal 10 command 5 decides; until stage 3 ports it the
    // result is 1, Exit. Exit sets state 9 and opens #799 when DGROUP:0x920c is 0 or 1: both
    // skipped for now (state 9 never runs a frame on this path: DGROUP:0x1a is set).
    log_todo("exit dialog #244, state 9 and dialog #799", "seg2:0xe0f");
    // 4: seg10:0x87de acts only while DGROUP:0x11aa = 0 and DGROUP:0x920c is -1 or 3
    // [BYTES seg10:0x87eb seg10:0x87f2].
    if (g.edition == -1 || g.edition == 3) {
        try {
            int count = g.ini->get_int(settings::kGameSection, "PlusCount", 13) + 1;
            if (count > 12) {
                count = 0;
                log_todo("state 9 and dialog #1872", "seg10:0x882d");
            }
            g.ini->set_int(settings::kGameSection, "PlusCount", count);  // seg10:0x8852
            g.ini->save();
        } catch (const std::exception& x) {
            log(std::string("PlusCount: ") + x.what());
        }
    }
    DestroyWindow(g.main);  // seg2:0xe44
}

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
    case shell::Call::kOptions: log_todo("Options dialog", "seg10:0x7f18"); return;
    case shell::Call::kSave: {
        const int arg = shell::save_argument(g.game.state());
        if (arg < 0) return;
        std::snprintf(what, sizeof what, "save seg10:0x8568(%d)", arg);
        log_todo(what, a.address);
        return;
    }
    case shell::Call::kHelpTopic:
        std::snprintf(what, sizeof what, "Online Help topic %d", a.arg);
        log_todo(what, "seg2:0x88e");
        return;
    case shell::Call::kF1:
        // DGROUP:0xa7c, the targeting display, comes with the HUD (M3); without it F1 calls
        // seg2:0x190a(0, 0).
        log_todo("WinHelp seg2:0x190a(0, 0)", a.address);
        return;
    case shell::Call::kHelpFile:
        std::snprintf(what, sizeof what, "WinHelp seg2:0x190a(%d, 0)", a.arg);
        log_todo(what, a.address);
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
            log_todo("focus lost: ISPACE 7:3", "seg10:0x8686");
        }
    } else if (!g.focus_own && !IsIconic(g.main)) {  // seg2:0x1302: 7:4 (M7)
        g.focus_own = true;
        log_todo("focus back: ISPACE 7:4", "seg10:0x86a8");
    }
    // DGROUP:0x934c and the Navigation window DGROUP:0xd1e also count as the game's own.
    if (active != g.main) {
        if (g.running) pause_game();  // seg2:0x1327
    } else if (!g.running && !IsIconic(g.main)) {
        resume_game();  // seg2:0x1341
    }
}

// The input path of globals.md "Input path".
void input(UINT msg, WPARAM wparam, LPARAM lparam) {
    // DEMO mode (DGROUP:0x90f8 and DGROUP:0x8e5e) quits on input after 4000 ms: TODO with the
    // DEMO command line. Otherwise input needs DGROUP:0x1212, DGROUP:0x8f68 and
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
    // LoadBitmap "ALPHABET" (stage 2); seg6:0x2ca's class icon, Aux, QuickIntro and Rate
    // (stage 2).
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

}  // namespace

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
            g.clip = shell::clip_rect(g.area, g.keys.border != 0, g.play_mode != 0);  // seg2:0xca4
            if (g.timer_seen) g.hud_dirty = true;  // seg2:0xb61
            if (!g.keys.full) {                    // seg2:0xb67-0xb7c
                g.keys.show_dirty = true;
                g.keys.show = window_rect(hwnd);
            }
        }
        break;
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
    case WM_ERASEBKGND:  // the presented surface covers the client area [DESIGN]
        // seg2:0x224 leaves WM_ERASEBKGND to DefWindowProc, which fills the client with the
        // class brush BLACK_BRUSH, clipped to the update region that the wParam DC's clip box
        // bounds [REF Win32 SDK WM_ERASEBKGND]. State 0 draws only what changes, so the erase
        // is kept there: on the surface, over the logical cover of that box, before the repaint
        // seg11:0xafe [DESIGN]. Other states black the play area in the paint routine until
        // they draw.
        if (g.game.state() == shell::kIntro && !g.gfx->surface().empty()) {
            RECT cb;
            if (GetClipBox(reinterpret_cast<HDC>(wparam), &cb) != ERROR) {
                const Rect r = shell::logical_cover(Rect{cb.left, cb.top, cb.right, cb.bottom},
                                                    g.gfx->factor(), g.gfx->logical_width(),
                                                    g.gfx->logical_height());
                if (r.width() > 0 && r.height() > 0)
                    PatBlt(g.gfx->dc(), r.left, r.top, r.width(), r.height(), BLACKNESS);
            }
        }
        return 1;
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
    case WM_VSCROLL:  // seg11:0x7798, the HUD scrollbar (M3)
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
    case kMsgAbort:
        log_todo("abort dialog #244 (ordinal 10 command 7)", "seg2:0x536");
        return 0;
    case kMsgShipWatch: log_todo("Ship Watch window", "seg13:0x5878"); return 0;
    case kMsgMenuReload: reload_menu(); return 0;  // seg2:0x4e9
    case kMsgIconWatch: log_todo("Icon Watch toggle", "seg2:0x4f0"); return 0;
    case kMsgAllShips: log_todo("All Ships toggle", "seg2:0x513"); return 0;
    case kMsgDemo:
    case kMsgPlay: log_todo("DEMO and PLAY", "seg2:0x86a"); return 0;
    case kMsgMusic: log_todo("music selector seg10:0x7480", "seg2:0x4cc"); return 0;
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
