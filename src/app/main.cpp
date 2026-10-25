// inspace.exe: the port's game program. WinMain follows the start-up order of
// docs/re/shell.md "Start-up" as far as stage 1 of M2 reaches; the message loop is timing.md's
// "Message loop and idle step" with the frame scheduler of ADR 0001.
//
//   inspace.exe [--game <dir>] --ini <path>        (src/shell/options.h)
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include "app/app.h"
#include "formats/bytes.h"
#include "shell/command_table.h"
#include "shell/options.h"

namespace is::app {

App g;

namespace {

LARGE_INTEGER g_qpc_freq{}, g_qpc_start{};

void init_clock() {
    QueryPerformanceFrequency(&g_qpc_freq);
    QueryPerformanceCounter(&g_qpc_start);
}

std::wstring utf16_of_utf8(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// The DGROUP literals of the start-up messages [BYTES DGROUP:0x4a0 DGROUP:0x4ea].
constexpr const char* kAnotherInstance = "Another version of game already running.";
constexpr const char* kAllTimersUsed = "All timers used. Can't run game.";
constexpr const char* kAppName = "Inner Space";  // DGROUP:0x82

void fail_box(const std::string& cp1252_text) {
    MessageBoxW(nullptr, widen(cp1252_text).c_str(), widen(kAppName).c_str(), MB_ICONEXCLAMATION);
}

void usage_box(const std::string& utf8) {
    log(utf8);
    MessageBoxW(nullptr,
                utf16_of_utf8(utf8 + "\n\nUsage: inspace.exe [--game <dir>] --ini <path>").c_str(),
                L"Inner Space", MB_ICONEXCLAMATION);
}

std::filesystem::path exe_dir() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) return {};
        if (n < buf.size()) {
            buf.resize(n);
            break;
        }
        buf.resize(buf.size() * 2);
    }
    return std::filesystem::path(buf).parent_path();
}

// The class icon from INSPACE.EXE's GROUP_ICON GAME: its 32 x 32 image, else its first.
HICON load_game_icon() {
    try {
        const assets::IconGroup gi = g.store->group_icon(assets::kMainProgram, "GAME");
        const auto& ico = gi.ico;
        if (ico.size() < 6) return nullptr;
        const unsigned count = ico[4] | (ico[5] << 8);
        std::size_t pick = 0;
        for (unsigned i = 0; i < count; ++i)
            if (ico.size() >= 6 + 16 * (i + 1) && ico[6 + 16 * i] == 32 && ico[7 + 16 * i] == 32) {
                pick = i;
                break;
            }
        const std::size_t e = 6 + 16 * pick;
        if (ico.size() < e + 16) return nullptr;
        const auto rd32 = [&](std::size_t o) {
            return static_cast<std::uint32_t>(ico[o]) | (ico[o + 1] << 8) | (ico[o + 2] << 16) |
                   (static_cast<std::uint32_t>(ico[o + 3]) << 24);
        };
        const std::uint32_t bytes = rd32(e + 8), offset = rd32(e + 12);
        if (offset > ico.size() || bytes > ico.size() - offset) return nullptr;
        return CreateIconFromResourceEx(const_cast<BYTE*>(ico.data() + offset), bytes, TRUE,
                                        0x00030000, 32, 32, LR_DEFAULTCOLOR);
    } catch (const std::exception& x) {
        log(std::string("class icon: ") + x.what());
        return nullptr;
    }
}

// Step 12: the seven window classes, INSPACE.EXE's only RegisterClass calls.
bool register_classes() {
    struct ClassDef {
        const wchar_t* name;
        UINT style;
        WNDPROC proc;
        bool icon;
        bool arrow;
        int brush;
        const char* original_proc;
    };
    // The procedures of the six other classes come with the windows that use them; until
    // then they are DefWindowProcW.
    const ClassDef defs[] = {
        {kMainClass, 0x300b, main_wndproc, true, true, BLACK_BRUSH, "seg2:0x224"},
        {L"IS_WATCH", 3, DefWindowProcW, true, false, BLACK_BRUSH, "seg13:0x4f4e"},  // TODO M8
        {L"IS_ICON", 3, DefWindowProcW, true, false, BLACK_BRUSH, "seg7:0x6184"},    // TODO M3
        {L"IS_SMART", 3, DefWindowProcW, true, false, BLACK_BRUSH, "seg13:0x613a"},  // TODO M8
        {L"IS_NAV", 0xb, DefWindowProcW, true, true, LTGRAY_BRUSH, "seg6:0x1f22"},   // TODO stage 3
        {L"IS_HELP", 3, DefWindowProcW, true, true, BLACK_BRUSH, "seg17:0x5782"},    // TODO stage 3
        {L"IS_INFO", 0xb, DefWindowProcW, false, true, BLACK_BRUSH, "seg11:0x22fa"},  // TODO M3
    };
    for (const ClassDef& d : defs) {
        WNDCLASSW wc{};
        wc.style = d.style;
        wc.lpfnWndProc = d.proc;
        wc.hInstance = g.instance;
        wc.hIcon = d.icon ? g.icon : nullptr;
        wc.hCursor = d.arrow ? LoadCursorW(nullptr, IDC_ARROW) : nullptr;
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(d.brush));
        wc.lpszClassName = d.name;
        if (!RegisterClassW(&wc)) {
            log(std::string("RegisterClass failed for the class of ") + d.original_proc);
            return false;  // no message, as the original [BYTES seg6:0x16f8]
        }
    }
    return true;
}

HMENU load_menu(std::uint16_t id) {
    try {
        const auto t = g.store->menu_template(id);
        return LoadMenuIndirectW(reinterpret_cast<const MENUTEMPLATEW*>(t.data()));
    } catch (const std::exception& x) {
        log(std::string("menu ") + std::to_string(id) + ": " + x.what());
        return nullptr;
    }
}

HACCEL load_accelerators() {
    try {
        std::vector<ACCEL> a;
        for (const fmt::Accel16& e : g.store->accelerators("MAINACCEL"))
            a.push_back(ACCEL{static_cast<BYTE>(e.flags & 0x7f), e.key, e.cmd});
        if (a.empty()) return nullptr;
        return CreateAcceleratorTableW(a.data(), static_cast<int>(a.size()));
    } catch (const std::exception& x) {
        log(std::string("MainAccel: ") + x.what());
        return nullptr;
    }
}

// The message loop of seg2:0x0 (timing.md "Message loop and idle step").
int message_loop() {
    MSG m;
    for (;;) {
        // PeekMessage while running; a modeless Navigation window (DGROUP:0xd1e with
        // DGROUP:0x95bc = 0) and the help dialog DGROUP:0x9460 join the condition in stage 3
        // [BYTES seg2:0xc9].
        const bool poll = g.running;
        if (poll) {
            if (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {  // seg2:0xfa
                if (m.message == WM_QUIT) return static_cast<int>(m.wParam);
            } else {
                // Empty queue: the idle step, then sleep until the next frame or a message
                // instead of spinning (ADR 0001 decision 1).
                DWORD wait = INFINITE;
                if (g.idle_allowed) {  // seg2:0x171
                    const bool ran_on = idle_step();
                    const Tick now = now_tick();
                    if (g.game.change_pending()) {
                        wait = 0;
                    } else {
                        wait = g.scheduler.wait(now);
                        // A pass that ended early ran no frame, so a due frame stays due: wait
                        // for the next pass instead of spinning (state 2 with DGROUP:0x1e74
                        // passed, a lasting DGROUP:0x1a) [DESIGN].
                        if (!ran_on && wait == 0) wait = shell::kIdlePassMs;
                    }
                }
                if (wait != 0)
                    MsgWaitForMultipleObjectsEx(0, nullptr, wait, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                continue;
            }
        } else {
            const BOOL r = GetMessageW(&m, nullptr, 0, 0);  // seg2:0x184: paused
            if (r == 0) return static_cast<int>(m.wParam);
            if (r < 0) return 1;
        }
        // TranslateAccelerator only while the main window is active [BYTES seg2:0x196]; the
        // IsDialogMessage calls for DGROUP:0x9460 and DGROUP:0xd1e come with those windows.
        if (g.accel && g.main && GetActiveWindow() == g.main &&
            TranslateAcceleratorW(g.main, g.accel, &m))
            continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

int run(HINSTANCE instance) {
    g.instance = instance;
    init_clock();

    // ---- the port's command line, the asset directory and the INI
    std::vector<std::filesystem::path> args;
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i < argc; ++i) args.emplace_back(argv[i]);
        if (argv) LocalFree(argv);
    }
    const shell::OptionsResult opts = shell::parse_options(args);
    if (!opts.error.empty()) {
        usage_box(opts.error);
        return 2;
    }
    std::string why;
    const auto asset_dir = shell::choose_asset_dir(exe_dir(), opts.options.game_dir, &why);
    if (!asset_dir) {
        usage_box("No game directory found (ADR 0002):\n" + why);
        return 2;
    }

    // Step 1: the cursor to the bottom centre, only x halved; the position saved
    // [BYTES seg2:0x37-0x55].
    g.screen_cx = GetSystemMetrics(SM_CXSCREEN);
    g.screen_cy = GetSystemMetrics(SM_CYSCREEN);
    SetCursorPos(g.screen_cx / 2, g.screen_cy);
    GetCursorPos(&g.start_cursor);

    // Step 3: single instance. Win32 has no hPrevInstance; a named mutex stands in for it
    // and FindWindow for the previous instance's DGROUP:0x16 [DESIGN].
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\InnerSpacePort.Instance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND prev = FindWindowW(kMainClass, nullptr);
        if (prev && IsWindow(prev)) {
            // seg6:0x1c0c would forward DEMO or PLAY first (TODO with the command line).
            PostMessageW(prev, WM_SYSCOMMAND, SC_MAXIMIZE, 0);  // seg6:0x1309
        } else {
            fail_box(kAnotherInstance);  // seg6:0x1305
        }
        CloseHandle(mutex);
        return 0;
    }
    // Step 4: SetMessageQueue has no Win32 counterpart; Win32 queues grow as needed.

    // Step 5: the timer probe [BYTES seg6:0x138a-0x13b1]. timeGetDevCaps's fine-timer flag
    // DGROUP:0x912e only feeds the millisecond gates, which ADR 0001 decision 7 drops.
    const UINT_PTR probe = SetTimer(nullptr, 1, 1000, nullptr);
    if (!probe) {
        fail_box(kAllTimersUsed);
        return 0;
    }
    // The original kills id 1 of the NULL window and so leaves its probe running; the port
    // kills the id it got [DESIGN].
    KillTimer(nullptr, probe);

    // Step 6: the INI loader. --ini names the file until ADR 0004 is accepted.
    try {
        g.ini = std::make_unique<settings::IniFile>(*opts.options.ini);
        g.keys = shell::read_window_keys(*g.ini);
        if (g.ini->get_string(settings::kGameSection, "Path", "").empty())
            log_todo("INI regeneration of an empty Path (ADR 0004 pending)", "seg6:0x7e9");
    } catch (const std::exception& x) {
        usage_box(std::string("INSPACE.INI: ") + x.what());
        return 2;
    }

    // Step 7: the language module in place of the DLL loader seg10:0x6656 (ADR 0002).
    try {
        g.store = std::make_unique<assets::AssetStore>(*asset_dir);
        if (!g.keys.language.empty()) g.store->set_language(g.keys.language);
        g.store->language_dll();
    } catch (const std::exception& x) {
        usage_box(std::string("Game files: ") + x.what());
        return 2;
    }
    log("game directory: " + fmt::path_utf8(*asset_dir) + ", INI: " +
        fmt::path_utf8(*opts.options.ini));

    // Steps 8-10: the edition check (TODO stage 3, needs ISPACE ordinal 24), the low-resource
    // check (GetFreeSystemResources has no Win32 counterpart) and the pools (the game state
    // of M3 and M4) are not ported yet.

    // Step 11: seg6:0x0 defaults and clamps the windowed rectangle.
    g.keys.show = shell::clamp_show_rect(g.keys.show, g.screen_cx, g.screen_cy);

    // Step 12: the seven classes.
    g.icon = load_game_icon();
    if (!register_classes()) return 0;

    // Step 13: display capabilities [BYTES seg6:0x170b-0x177c].
    if (HDC dc = CreateDCW(L"DISPLAY", nullptr, nullptr, nullptr)) {
        g.depth = std::max(GetDeviceCaps(dc, BITSPIXEL), GetDeviceCaps(dc, PLANES));
        DeleteDC(dc);
    }

    // Step 14: menu 101; attached only when Menu is non-zero [BYTES seg6:0x17f5-0x189a].
    g.menu = load_menu(101);

    // Step 15: the main window [BYTES seg6:0x180e-0x18a5], without WS_VISIBLE.
    const std::wstring title = lang_string(g.edition == 0 ? 2338 : 2337);  // seg6:0x185c
    DWORD style;
    int x, y, w, h;
    if (g.keys.full) {
        style = shell::kStyleFull;
        x = 0;
        y = 0;
        w = g.screen_cx;
        h = g.screen_cy;
    } else {
        style = shell::kStyleWindowed;
        x = g.keys.show.left;
        y = g.keys.show.top;
        w = g.keys.show.width();
        h = g.keys.show.height();
    }
    install_state_hooks();
    // The result is discarded; DGROUP:0x16 comes from WM_CREATE [BYTES seg2:0x390].
    CreateWindowExW(0, kMainClass, title.c_str(), style, x, y, w, h, nullptr,
                    g.keys.menu ? g.menu : nullptr, instance, nullptr);
    if (!g.main) return 0;
    log_window("after CreateWindow");
    if (g.keys.full && shell::read_on_top(*g.ini))  // seg6:0x18aa-0x18d3
        SetWindowPos(g.main, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

    // Step 17: strings 2900-2902 (DGROUP:0x94e4, DGROUP:0x94fa, DGROUP:0x967a) feed the
    // intro: TODO stage 2. Step 18: CTL3D is not loaded; Win32 controls draw in 3D already.
    // Step 20: seg6:0x1c0c's DEMO and PLAY: TODO stage 3.

    // Step 19: the start time and MainAccel, then the loop [BYTES seg2:0xa5 seg2:0xb9].
    g.start_tick = now_tick();
    g.accel = load_accelerators();
    const int code = message_loop();

    if (g.accel) DestroyAcceleratorTable(g.accel);
    if (g.icon) DestroyIcon(g.icon);
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    log("exit " + std::to_string(code));
    return code;
}

}  // namespace

Tick now_tick() {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    const auto ms = static_cast<unsigned long long>(c.QuadPart - g_qpc_start.QuadPart) * 1000ull /
                    static_cast<unsigned long long>(g_qpc_freq.QuadPart ? g_qpc_freq.QuadPart : 1);
    return static_cast<Tick>(ms);
}

std::wstring widen(const std::string& cp1252) {
    const std::u16string u = fmt::cp1252_to_utf16(cp1252);
    return std::wstring(u.begin(), u.end());
}

std::wstring lang_string(std::uint16_t id) {
    try {
        if (g.store)
            if (auto s = g.store->find_string(id)) return widen(*s);
    } catch (const std::exception& x) {
        log("string " + std::to_string(id) + ": " + x.what());
    }
    return {};
}

void log(const std::string& utf8) {
    const std::string line = "inspace: " + utf8 + "\n";
    std::fputs(line.c_str(), stderr);
    std::fflush(stderr);
    OutputDebugStringW(utf16_of_utf8(line).c_str());
}

void log_todo(const std::string& what, const char* address) {
    log("unimplemented: " + what + " (" + address + ")");
}

// GameFrame seg2:0x1c08: a pending state change runs the state-change step instead of a
// pass [BYTES seg2:0x1c64]; otherwise one pass, and the state's frame when it is due.
bool idle_step() {
    const Tick now = now_tick();
    if (g.game.change_pending()) {
        g.game.change_step(now);
        return true;
    }
    g.frame_running = true;  // DGROUP:0x968c
    // The help window's dialog pass seg17:0x60d8 replaces the tick while DGROUP:0x9460 is set
    // (stage 3).
    g.scheduler.set_pace(shell::pace_for_state(g.game.state()), now);
    const bool ran_on = g.game.pass(now);
    if (ran_on && g.scheduler.due(now)) {
        g.game.frame();
        g.scheduler.ran(now);
    }
    g.frame_running = false;
    return ran_on;
}

// The parts of the rate-settings reader seg6:0xb3e the shell has: FrameRateSet, then Border
// and Menu. The other timing and game keys come with the wave (M3).
namespace {
void read_rate_settings(Tick now) {
    g.scheduler.set_rate(shell::read_frame_rate(*g.ini), now);  // seg6:0xb7d
    const shell::BorderMenu b = shell::read_border_menu(*g.ini);  // seg6:0xd20 seg6:0xd36
    if (b.border == g.keys.border && b.menu == g.keys.menu) return;  // seg6:0xd3e-0xd4c
    g.keys.border = b.border;  // seg6:0xd51
    g.keys.menu = b.menu;      // seg6:0xd57
    // SetWindowWord(GWW_ID) sets a top-level window's menu in Win16 [BYTES seg6:0xd72].
    SetMenu(g.main, g.keys.menu ? g.menu : nullptr);
    DrawMenuBar(g.main);                  // seg6:0xd7b
    g.start_cursor.y = 0x244;             // seg6:0xd80: DGROUP:0x1d4a = 0x244
    g.relayout = true;                    // seg6:0xd86: DGROUP:0x1038 = 1
    // seg6:0xd98 posts WM_PAINT; Win32 asks for it through the update region [DESIGN].
    InvalidateRect(g.main, nullptr, FALSE);
}
}  // namespace

void install_state_hooks() {
    auto& h = g.game.hooks();
    // The state-0 entry seg15:0x5786 holds the first ShowWindow [BYTES seg15:0x5840]. The rest
    // of the intro is stage 2.
    h.enter[shell::kIntro] = [](shell::GameState&) { ShowWindow(g.main, SW_SHOW); };
    h.enter[shell::kWave] = [](shell::GameState&) {
        log_todo("state-1 set-up", "seg10:0x0");  // M3
    };
    h.play_area = [](shell::GameState&) {
        // seg2:0x1ac8: the play area from the client rectangle and the clip rectangle without
        // the border inset; srand(GetCurrentTime()) waits for the RTL rand of rng.md (M3).
        RECT rc;
        GetClientRect(g.main, &rc);
        g.area = shell::play_area_from_client(rc.right, rc.bottom, g.screen_cx, g.screen_cy);
        g.clip.play = {0, 0, g.area.w, g.area.play_h};  // seg2:0x1b5d
    };
    // State 0's frame: the intro is stage 2. The shell keeps the end of the Calibrate
    // measurement, which turns on keys, commands and the double click (DGROUP:0x1212). The
    // port does not measure (ADR 0001 decision 9); the measurement's window of 1000 ms from
    // the first state-0 frame [BYTES seg15:0x58fb seg15:0x5f98] is kept, after which
    // seg15:0x6009 sets DGROUP:0x1212 and seg6:0xa6c reads the rate settings and writes
    // Calibrate back [BYTES seg6:0xae7 seg6:0xb31].
    h.frame[shell::kIntro] = [](shell::GameState& s) {
        if (g.calibrated) return;
        if (!g.calibrate_started) {
            g.calibrate_started = true;
            g.calibrate_start = s.now();
            return;
        }
        if (s.now() - g.calibrate_start < 1000) return;
        g.calibrated = true;
        try {
            read_rate_settings(s.now());
            g.ini->set_int(settings::kGameSection, "Calibrate", shell::kCalibrate);
            g.ini->save();
        } catch (const std::exception& x) {
            log(std::string("INSPACE.INI: ") + x.what());
        }
        log("Calibrate done: FrameRateSet " + std::to_string(g.scheduler.rate()));
    };
    h.route_input = [](shell::GameState& s, const shell::Input& in) {
        if (in.msg != shell::kWmKeyUp && in.msg != kMsgAltKey && in.msg != kMsgCtrlKey) return;
        char buf[96];
        std::snprintf(buf, sizeof buf, "input msg 0x%x key 0x%x in state %d", in.msg, in.wparam,
                      s.state());
        log_todo(buf, s.state() == shell::kIntro ? "seg11:0x7902" : "seg11:0x6aa8");
    };
    h.open_navigation = [](shell::GameState&) { log_todo("Navigation window", "seg6:0x1c72"); };
    h.save_scores = [](shell::GameState&) { log_todo("score list save", "seg6:0x626c"); };
}

}  // namespace is::app

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    try {
        return is::app::run(instance);
    } catch (const std::exception& x) {
        is::app::log(std::string("fatal: ") + x.what());
        return 1;
    }
}
