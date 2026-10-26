// The Navigation window stub (see nav.h).
#ifdef _WIN32
#include "nav/nav.h"

#include <exception>
#include <string>

#include "nav/nav_rules.h"
#include "settings/settings.h"

namespace is::nav {

namespace {

Host g_host;
HWND g_nav = nullptr;          // DGROUP:0xd1e
int g_arg = 0;                 // DGROUP:0xd20: Enter is accepted only when set
int g_drive = 0;               // DGROUP:0x94ac at open; DGROUP:0xd18 = 'C' + it
int g_sel = 0;                 // DGROUP:0x94b4, the directory selection
int g_accepted = 0;            // DGROUP:0xd1c, the accepted selection
RECT g_rect{kDefaultLeft, kDefaultTop, kDefaultLeft + kDefaultRight, kDefaultTop + kDefaultBottom};  // DGROUP:0x6af0
bool g_logged_walk = false, g_logged_paint = false, g_logged_select = false;

constexpr const wchar_t* kClass = L"IS_NAV";

std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string lang(int id) { return g_host.lang ? g_host.lang(id) : std::string(); }
void todo(const std::string& what, const char* address) {
    if (g_host.todo) g_host.todo(what, address);
}
void log(const std::string& s) {
    if (g_host.log) g_host.log(s);
}

void place(HWND parent, int id, const Box& b) {
    if (HWND c = GetDlgItem(parent, id)) MoveWindow(c, b.x, b.y, b.w, b.h, TRUE);
}

HWND child(HWND parent, const wchar_t* cls, const std::string& text, DWORD style, const Box& b,
           int id) {
    HWND c = CreateWindowExW(0, cls, wide(text).c_str(), style, b.x, b.y, b.w, b.h, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_host.instance,
                             nullptr);
    // seg6:0x23d4 sends WM_SETFONT DGROUP:0x90fe with lParam 1 to the buttons and the controls
    // after them [BYTES seg6:0x25c1 seg6:0x261f seg6:0x2c89] (shell.md "Navigation window").
    // The call after each child, seg2:0x1a7a, is the Ctl3dSubclassCtl wrapper (win16-translation.md),
    // which does nothing without CTL3D, as in the port (start-up step 18) [BYTES seg2:0x1a84].
    if (c && g_host.font) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_host.font), TRUE);
    return c;
}

// seg6:0x23d4: the children of WM_CREATE, for the parts the stub builds.
void build(HWND h) {
    // seg6:0x23d4 starts with EnableWindow(main, FALSE) [BYTES seg6:0x23f4]; seg6:0x2ebc
    // enables it again.
    if (g_host.main) EnableWindow(g_host.main, FALSE);
    RECT c;
    GetClientRect(h, &c);
    child(h, L"button", lang(2143), 0x50030000, enter_box(c.right, c.bottom), kEnterId);  // "&Enter"
    child(h, L"button", lang(2144), 0x50010000, help_box(c.right, c.bottom), kHelpId);    // "&Help"
    HWND list = child(h, L"listbox", "", kDirListStyle, dir_list_box(c.right, c.bottom), kDirListId);
    if (list) {
        SendMessageW(list, LB_SETHORIZONTALEXTENT, 200, 0);  // Win16 0x415 [BYTES seg6:0x2507]
        // The fixture-independent minimum: the root of drive DGROUP:0xd18. The tree walk
        // seg6:0x37ca and the built-in levels come with M3.
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(drive_root(g_drive)).c_str()));
        SendMessageW(list, LB_SETCURSEL, 0, 0);
    }
    if (!g_logged_walk) {
        g_logged_walk = true;
        todo("Navigation directory tree, file list, race and duel boxes, preview and status line",
             "seg6:0x37ca");
    }
    // Sounds 0x6e and 0x4d while DGROUP:0xd20 != 0 [BYTES seg6:0x2d99 seg6:0x2da2 seg6:0x2daa].
    if (g_arg != 0 && g_host.sound) {
        g_host.sound(0x6e);
        g_host.sound(0x4d);
    }
}

// The owner-drawn directory list: the stub draws the entry's text [DESIGN].
void draw_item(const DRAWITEMSTRUCT* d) {
    if (d->itemID == static_cast<UINT>(-1)) return;
    const bool sel = (d->itemState & ODS_SELECTED) != 0;
    FillRect(d->hDC, &d->rcItem, GetSysColorBrush(sel ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    wchar_t buf[300] = L"";
    const auto n = SendMessageW(d->hwndItem, LB_GETTEXTLEN, d->itemID, 0);
    if (n > 0 && n < 300) SendMessageW(d->hwndItem, LB_GETTEXT, d->itemID, reinterpret_cast<LPARAM>(buf));
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    RECT r = d->rcItem;
    r.left += 2;
    DrawTextW(d->hDC, buf, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    if (!g_logged_paint) {
        g_logged_paint = true;
        todo("Navigation list drawing (folders and levels)", "seg6:0x4e04");
    }
}

LRESULT handle(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE:  // [BYTES seg6:0x1f35 seg6:0x1f82]
        g_nav = h;
        build(h);
        return 0;
    case WM_MOVE:  // GetWindowRect into DGROUP:0x6af0
        GetWindowRect(h, &g_rect);
        return 0;
    case WM_SIZE: {
        GetWindowRect(h, &g_rect);
        RECT c;
        GetClientRect(h, &c);
        place(h, kEnterId, enter_box(c.right, c.bottom));
        place(h, kHelpId, help_box(c.right, c.bottom));
        place(h, kDirListId, dir_list_box(c.right, c.bottom));
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* mm = reinterpret_cast<MINMAXINFO*>(l);
        mm->ptMinTrackSize = {kMinTrackW, kMinTrackH};
        return 0;
    }
    case WM_MEASUREITEM: {
        // tmHeight + tmExternalLeading + 3 of DGROUP:0x90fe, at least 14 [BYTES seg6:0x2016
        // seg6:0x2021 seg6:0x2034].
        auto* m = reinterpret_cast<MEASUREITEMSTRUCT*>(l);
        int height = 14;
        if (HDC dc = GetDC(g_host.main)) {
            HGDIOBJ old = g_host.font ? SelectObject(dc, g_host.font) : nullptr;
            TEXTMETRICW tm{};
            GetTextMetricsW(dc, &tm);
            if (old) SelectObject(dc, old);
            ReleaseDC(g_host.main, dc);
            height = tm.tmHeight + tm.tmExternalLeading + 3;
            if (height < 14) height = 14;
        }
        m->itemHeight = static_cast<UINT>(height);
        return TRUE;
    }
    case WM_DRAWITEM:
        draw_item(reinterpret_cast<const DRAWITEMSTRUCT*>(l));
        return TRUE;
    case WM_COMMAND: {
        const int id = LOWORD(w), code = HIWORD(w);
        if (id == kHelpId) {  // seg2:0x190a(5, 0) [BYTES seg6:0x2305]
            if (g_host.help) g_host.help();
        } else if (id == kEnterId) {  // seg6:0x301a [BYTES seg6:0x2384]
            if (enter() == 0) todo("Navigation Enter refused: the directory checks", "seg6:0x5260");
        } else if (id == kDirListId) {
            if (code == LBN_SELCHANGE) {  // DGROUP:0x94b4 = LB_GETCURSEL
                const int sel = static_cast<int>(SendMessageW(reinterpret_cast<HWND>(l), LB_GETCURSEL, 0, 0));
                if (sel != g_sel) {
                    g_sel = sel;
                    if (!g_logged_select) {
                        g_logged_select = true;
                        todo("Navigation selection status", "seg6:0x5318");
                    }
                }
            } else if (code == LBN_DBLCLK) {
                todo("Navigation directory expand", "seg6:0x5260");
            }
        }
        return 0;
    }
    case WM_SYSCOMMAND:  // SC_CLOSE goes to the main window [BYTES seg6:0x23a5 seg6:0x23b8]
        if ((w & 0xfff0) == SC_CLOSE) {
            PostMessageW(g_host.main, WM_SYSCOMMAND, SC_CLOSE, 0);
            return 0;
        }
        break;
    case WM_PAINT: {  // seg6:0x5b4c draws the status line and the boxes (M3)
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        return 0;
    }
    default: break;
    }
    return DefWindowProcW(h, msg, w, l);
}

LRESULT CALLBACK nav_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    // No exception may unwind through user32's frames [DESIGN].
    try {
        return handle(h, msg, w, l);
    } catch (const std::exception& x) {
        log(std::string("navigation: ") + x.what());
    } catch (...) {
    }
    return DefWindowProcW(h, msg, w, l);
}

int ini_int(const char* key, int def) {
    return g_host.ini ? g_host.ini->get_int(settings::kGameSection, key, def) : def;
}

}  // namespace

bool install(const Host& host) {
    g_host = host;
    UnregisterClassW(kClass, host.instance);
    WNDCLASSW wc{};
    wc.style = 0xb;  // CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, as start-up step 12
    wc.lpfnWndProc = nav_proc;
    wc.hInstance = host.instance;
    wc.hIcon = host.icon;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH));
    wc.lpszClassName = kClass;
    return RegisterClassW(&wc) != 0;
}

void attach(HWND main, HFONT font) {
    g_host.main = main;
    g_host.font = font;
}

HWND window() { return g_nav; }

void open(int arg, int drive_offset) {  // seg6:0x1c72
    // DGROUP:0xd24 = 1 (read by M3's paths).
    if (g_nav && IsWindow(g_nav)) {
        ShowWindow(g_nav, SW_SHOW);
        return;
    }
    g_arg = arg;
    g_sel = g_accepted;  // DGROUP:0x94b4 = DGROUP:0xd1c
    g_drive = drive_offset;
    const int x = ini_int("NavLeft", kDefaultLeft), y = ini_int("NavTop", kDefaultTop);
    const int cw = ini_int("NavRight", kDefaultRight), ch = ini_int("NavBottom", kDefaultBottom);
    g_rect = {x, y, x + cw, y + ch};
    CreateWindowExW(0, kClass, wide(lang(kTitleString)).c_str(), kStyle, x, y, cw, ch, g_host.main,
                    nullptr, g_host.instance, nullptr);
    log("navigation window " + std::string(g_nav ? "open" : "failed") + " at " + std::to_string(x) +
        "," + std::to_string(y) + " " + std::to_string(cw) + "x" + std::to_string(ch));
    // The play area goes black unless the state is 2 [BYTES seg6:0x1d45 seg6:0x1d4a seg6:0x1d51].
    if (!(g_host.state && g_host.state() == 2) && g_host.black_play_area) g_host.black_play_area();
    // seg6:0x1e68(1), the demo pilot, for PlayMode != 0 in a demo [BYTES seg6:0x1d6c seg6:0x1d82].
    if (g_host.play_mode && g_host.player && g_host.play_mode() != 0 && g_host.player() == 0)
        todo("Navigation demo pilot", "seg6:0x1e68");
}

void close() {  // seg6:0x2ebc
    if (!g_nav) return;
    // DGROUP:0x1d40 = now + 1000 [BYTES seg6:0x2ede]: no instruction reads that dword
    // (shell.md "DEMO and PLAY"), so the port does not keep it.
    if (g_host.ini) {
        try {
            for (const auto& kv : close_writes(g_rect.left, g_rect.top, g_rect.right, g_rect.bottom))
                g_host.ini->set_int(settings::kGameSection, kv.first, kv.second);
            if (g_host.ini_changed) g_host.ini_changed();
        } catch (const std::exception& x) {
            log(std::string("Nav keys: ") + x.what());
        }
    }
    const bool torn = g_host.torn_down && g_host.torn_down();
    if (!torn && g_host.main) EnableWindow(g_host.main, TRUE);
    HWND h = g_nav;
    g_nav = nullptr;  // [BYTES seg6:0x2ff1]
    if (IsWindow(h)) DestroyWindow(h);
    if (!torn && g_host.main) {
        ShowWindow(g_host.main, SW_SHOW);
        SetActiveWindow(g_host.main);
    }
}

int enter() {  // seg6:0x301a
    const EnterAction a = enter_action(g_arg != 0, g_host.wave ? g_host.wave() : 0,
                                       g_host.restored ? g_host.restored() : 0);
    if (a == EnterAction::kRefused) return 0;
    // The checks of the selected directory (records, locked levels, the full-version text)
    // come with M3; the stub accepts the selection.
    todo("Navigation Enter's directory checks; the selection is accepted", "seg6:0x301a");
    g_accepted = g_sel;  // DGROUP:0xd1c = DGROUP:0x94b4
    // The accepted entry becomes the next wave's directory DGROUP:0x38c8: Enter copies
    // DGROUP:0x6990 verbatim, the drive letter, ":\", the path and a trailing "\" in on-disk
    // case ("D:\GAMES\" in start-0001) [BYTES seg6:0x3044] [ORACLE start-0001]. The stub's
    // list holds only the drive root, whose letter the port writes in upper case as the oracle
    // shows it; the drive-letter form of seg6:0x5318 is [OPEN] until the tree walk (M3).
    if (g_host.entered && g_nav) {
        if (HWND list = GetDlgItem(g_nav, kDirListId)) {
            const LRESULT n = SendMessageW(list, LB_GETTEXTLEN, static_cast<WPARAM>(g_sel), 0);
            if (n > 0 && n < MAX_PATH) {
                wchar_t buf[MAX_PATH + 1] = L"";
                SendMessageW(list, LB_GETTEXT, static_cast<WPARAM>(g_sel), reinterpret_cast<LPARAM>(buf));
                if (buf[0] >= L'a' && buf[0] <= L'z') buf[0] = static_cast<wchar_t>(buf[0] - 32);
                char out[MAX_PATH * 2 + 1] = "";
                WideCharToMultiByte(1252, 0, buf, -1, out, sizeof out, nullptr, nullptr);
                g_host.entered(out);
            }
        }
    }
    if (a == EnterAction::kRequestWave) {
        log("navigation Enter: state request 1");
        if (g_host.request_state) g_host.request_state(1);
    } else if (g_host.arm_debriefing) {
        g_host.arm_debriefing();
    }
    close();
    return 1;
}

}  // namespace is::nav
#endif
