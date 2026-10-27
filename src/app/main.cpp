// inspace.exe: the port's game program. WinMain follows the start-up order of
// docs/re/shell.md "Start-up" as far as stage 1 of M2 reaches; the message loop is timing.md's
// "Message loop and idle step" with the frame scheduler of ADR 0001.
//
//   inspace.exe [--game <dir>] [--ini <path>]      (src/shell/options.h, ADR 0004)
//   inspace.exe --install <source> [--to <dir>]    (src/install, ADR 0008 decision 1)
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <mmsystem.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <exception>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "app/app.h"
#include "app/startup.h"
#include "audio/audio.h"
#include "dialogs/audio_options.h"  // M7 audio rewiring (implementer C)
#include "settings/ini_location.h"
#include "formats/bytes.h"
#include "formats/res16.h"
#include "install/install_win32.h"
#include "intro/gdi_canvas.h"
#include "intro/intro.h"
#include "nav/nav.h"
#include "shell/command_table.h"
#include "shell/options.h"
#include "shell/rng.h"

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
                utf16_of_utf8(utf8 + "\n\nUsage: inspace.exe [--game <dir>] [--ini <path>]\n"
                                     "       inspace.exe --install <source> [--to <dir>]")
                    .c_str(),
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
        // src/nav registers IS_NAV again with its procedure when it opens the window.
        {L"IS_NAV", 0xb, DefWindowProcW, true, true, LTGRAY_BRUSH, "seg6:0x1f22"},
        {L"IS_HELP", 3, DefWindowProcW, true, true, BLACK_BRUSH, "seg17:0x5782"},    // TODO M8
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

// Releases the intro's canvas and logic before the renderer goes (install_state_hooks).
void release_intro();

// The message loop of seg2:0x0 (timing.md "Message loop and idle step").
int message_loop() {
    MSG m;
    for (;;) {
        // PeekMessage while running; a modeless Navigation window (DGROUP:0xd1e with
        // DGROUP:0x95bc = 0) and the help dialog DGROUP:0x9460 also belong to the condition
        // [BYTES seg2:0xc9], which the port leaves out [OPEN] (M3 Navigation, M8 help).
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
        if (HWND nav = nav::window(); nav && IsDialogMessageW(nav, &m)) continue;  // [BYTES seg2:0x1dc]
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

// The executable's directory when it holds the game, else empty: its INSPACE.INI is the
// game's own, never the port's (ADR 0002 decision 5, ADR 0004 decision 2) [DESIGN].
std::filesystem::path exe_game_dir() {
    return shell::choose_asset_dir(exe_dir(), std::nullopt, nullptr)
        .value_or(std::filesystem::path{});
}

// The install step's Win32 driver with the port's INI location (ADR 0004 decision 2; the new
// game directory is not the executable's, so portable mode follows the usual rule) [DESIGN].
std::optional<std::filesystem::path> run_install_step(const std::filesystem::path& source,
                                                      const std::optional<std::filesystem::path>& to,
                                                      const std::optional<std::filesystem::path>& ini,
                                                      bool open_readme) {
    const settings::IniLocation loc =
        settings::resolve_ini_location(ini, exe_dir(), roaming_app_data(),
                                       exe_game_dir());
    install::Win32Options o;
    o.instance = g.instance;
    o.source = source;
    o.to = to;
    o.ini_path = loc.path;
    o.work_root = loc.path.parent_path() / "INSTALL";
    o.open_readme = open_readme;
    o.log = [](const std::string& s) { log(s); };
    log("install: " + fmt::path_utf8(source) + " -> INI " + fmt::path_utf8(loc.path));
    return install::run_install_win32(o);
}

// [Inner Space] Path of the port's INI when it names a directory that holds the game.
std::optional<std::filesystem::path> ini_named_game(const std::optional<std::filesystem::path>& ini) {
    try {
        const settings::IniLocation loc =
            settings::resolve_ini_location(ini, exe_dir(), roaming_app_data(),
                                       exe_game_dir());
        std::error_code ec;
        if (!std::filesystem::exists(loc.path, ec)) return std::nullopt;
        const settings::IniFile f(loc.path);
        const std::filesystem::path p =
            settings::path_from_ini(f.get_string(settings::kGameSection, "Path", "", 0x6d));
        if (p.empty()) return std::nullopt;
        return shell::choose_asset_dir({}, p, nullptr);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

int run(HINSTANCE instance) {
    g.instance = instance;
    init_clock();
    // ShellExecute (the manual, Order's text files) can use COM, so the GUI thread initialises
    // it first, as the ShellExecute documentation asks [REF Win32 SDK ShellExecute] [DESIGN].
    struct ComScope {
        HRESULT hr;
        ~ComScope() {
            if (SUCCEEDED(hr)) CoUninitialize();
        }
    } const com{CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)};
    if (FAILED(com.hr)) log("CoInitializeEx failed; ShellExecute runs without COM");

    // ---- the port's command line, the asset directory and the INI
    std::vector<std::filesystem::path> args;
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i < argc; ++i) args.emplace_back(argv[i]);
        if (argv) LocalFree(argv);
    }
    // --ini is optional (ADR 0004 decision 2).
    const shell::OptionsResult opts = shell::parse_options(args);
    if (!opts.error.empty()) {
        usage_box(opts.error);
        return 2;
    }
    // Step 3: single instance, done before the install step and the INI resolution, so two
    // copies never install at once. Win32 has no hPrevInstance; a named mutex stands in for
    // it (ADR 0010) [DESIGN]. DEMO and PLAY forwarding to the first instance (seg6:0x1c0c,
    // seg6:0x1309) is M3 [OPEN].
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\InnerSpacePort.SingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        fail_box(kAnotherInstance);  // seg6:0x1305
        CloseHandle(mutex);
        return 0;
    }
    // The install step (ADR 0008 decision 1) [DESIGN]: --install runs it and exits.
    if (opts.options.install) {
        const std::optional<std::filesystem::path> dir = run_install_step(
            *opts.options.install, opts.options.to, opts.options.ini, /*open_readme=*/true);
        return dir ? 0 : 1;
    }
    std::string why;
    std::optional<std::filesystem::path> asset_dir =
        shell::choose_asset_dir(exe_dir(), opts.options.game_dir, &why);
    if (!asset_dir && !opts.options.game_dir) {
        // ADR 0002 decision 1, second place: the directory the port's INI names (Path), which
        // the install step writes [DESIGN].
        asset_dir = ini_named_game(opts.options.ini);
    }
    if (!asset_dir && !opts.options.game_dir) {
        // No game: offer the install step, then continue into the game [DESIGN] (ADR 0008).
        std::filesystem::path set;
        if (install::choose_install_set(nullptr, &set))
            asset_dir = run_install_step(set, std::nullopt, opts.options.ini, /*open_readme=*/false);
        if (!asset_dir) return 0;
    }
    if (!asset_dir) {
        usage_box("No game directory found (ADR 0002):\n" + why);
        return 2;
    }
    g.asset_dir = *asset_dir;

    // Step 1: the cursor to the bottom centre, only x halved; the position saved
    // [BYTES seg2:0x37-0x55].
    g.screen_cx = GetSystemMetrics(SM_CXSCREEN);
    g.screen_cy = GetSystemMetrics(SM_CYSCREEN);
    SetCursorPos(g.screen_cx / 2, g.screen_cy);
    GetCursorPos(&g.start_cursor);

    // Step 4: SetMessageQueue has no Win32 counterpart; Win32 queues grow as needed [DESIGN]
    // (win16-translation.md).

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

    // Step 6: the INI loader seg6:0x6d8 with ADR 0004's location, first-run copy and
    // regeneration (src/app/startup.cpp).
    try {
        std::string ini_why;
        if (!open_ini(opts.options.ini, exe_dir(), &ini_why)) {
            usage_box("INSPACE.INI: " + ini_why);
            return 2;
        }
        g.keys = shell::read_window_keys(*g.ini);
        settings::PortSettings port;
        port.load(*g.ini);
        g.scale_key = port.scale;
    } catch (const std::exception& x) {
        usage_box(std::string("INSPACE.INI: ") + x.what());
        return 2;
    }

    // Step 7: the language module in place of the DLL loader seg10:0x6656 (ADR 0002).
    try {
        g.store = std::make_unique<assets::AssetStore>(*asset_dir);
        if (!g.keys.language.empty()) g.store->set_language(g.keys.language);
        g.store->language_dll();
        g.gfx = std::make_unique<gfx::Gfx>(g.store.get());
    } catch (const std::exception& x) {
        usage_box(std::string("Game files: ") + x.what());
        return 2;
    }
    log("game directory: " + fmt::path_utf8(*asset_dir) + ", INI: " + fmt::path_utf8(g.ini_path) +
        ", LocalPath: " + fmt::path_utf8(g.local_path));

    // Step 8: the edition check seg10:0x885e(1) [BYTES seg6:0x13e6]. The CD branch that can end
    // start-up with string 2543 is not ported: CDDrive counts as empty (ADR 0008 decision 4).
    run_edition_check(1);

    // Steps 9-10: the low-resource check never fires, since Win32 has no fixed resource heaps
    // and the port counts 100 % free (win16-translation.md, GetFreeSystemResources) [DESIGN];
    // the pools hold the game state of M3 and M4 [OPEN] (M3).

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
    const std::wstring title = lang_string(g.edition.edition == 0 ? 2338 : 2337);  // seg6:0x185c
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
    install_dialog_hooks(g);
    install_help_hooks(g);
    install_briefing_hooks(g);
    install_desk_hooks(g);  // src/desk/hooks.cpp
    // The result is discarded; DGROUP:0x16 comes from WM_CREATE [BYTES seg2:0x390].
    CreateWindowExW(0, kMainClass, title.c_str(), style, x, y, w, h, nullptr,
                    g.keys.menu ? g.menu : nullptr, instance, nullptr);
    if (!g.main) return 0;
    log_window("after CreateWindow");
    if (g.keys.full && shell::read_on_top(*g.ini))  // seg6:0x18aa-0x18d3
        SetWindowPos(g.main, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

    // Step 17: strings 2900-2902 (DGROUP:0x94e4, DGROUP:0x94fa, DGROUP:0x967a): src/intro
    // loads the strings it draws itself. Step 18: CTL3D is not loaded; Win32 controls draw in
    // 3D already, and dropping CTL3D needs an ADR (win16-translation.md "CTL3D") [OPEN]. Step 20: seg6:0x1c0c's DEMO and PLAY are
    // not ported [OPEN] (M3, with the demo game they start; shell.md "DEMO and PLAY").

    // Step 19: the start time and MainAccel, then the loop [BYTES seg2:0xa5 seg2:0xb9].
    g.start_tick = now_tick();
    g.accel = load_accelerators();
    const int code = message_loop();

    if (g.accel) DestroyAcceleratorTable(g.accel);
    if (g.icon) DestroyIcon(g.icon);
    release_intro();  // the intro's images belong to the renderer's palette
    g.gfx.reset();
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

int logical(int physical) { return shell::logical_length(physical, g.gfx ? g.gfx->factor() : 1); }

POINT logical_point(LPARAM lparam) {
    return {logical(static_cast<short>(LOWORD(lparam))), logical(static_cast<short>(HIWORD(lparam)))};
}

// The original draws on the screen DC during its frames; the port draws on the surface and
// presents it after each frame and state change (ADR 0007 decision 3) [DESIGN].
void present_frame() {
    if (!g.gfx || !g.main) return;
    const bool drew = g.gfx->present(g.main, false);
    static bool logged = false;  // one diagnostic line for the headless checks [DESIGN]
    if (!logged) {
        logged = true;
        log("presenting: logical " + std::to_string(g.gfx->logical_width()) + "x" +
            std::to_string(g.gfx->logical_height()) + ", factor " +
            std::to_string(g.gfx->factor()) + (drew ? ", converted" : ", unchanged"));
    }
}

// GameFrame seg2:0x1c08: a pending state change runs the state-change step instead of a
// pass [BYTES seg2:0x1c64]; otherwise one pass, and the state's frame when it is due.
bool idle_step() {
    const Tick now = now_tick();
    if (g.game.change_pending()) {
        g.game.change_step(now);
        present_frame();
        return true;
    }
    g.frame_running = true;  // DGROUP:0x968c
    // The help window's dialog pass seg17:0x60d8 replaces the tick while DGROUP:0x9460 is set
    // [OPEN] (M8, the Online Help window).
    g.scheduler.set_pace(shell::pace_for_state(g.game.state()), now);
    const bool ran_on = g.game.pass(now);
    if (ran_on && g.scheduler.due(now)) {
        g.game.frame();
        g.scheduler.ran(now);
        present_frame();
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

// ---- state 0: the intro (src/intro)

// The intro's view of the program (intro::Host).
class IntroHost final : public intro::Host {
public:
    Tick now() override { return now_tick(); }
    intro::Area area() override { return {g.area.w, g.area.h, g.area.play_h}; }
    // The sound wrappers seg10:0x744c and seg10:0x754a and the music selector seg10:0x7480
    // (src/dialogs/audio_options.h is::sound). The intro plays modes 0 and 4 only, so the host
    // keeps the wrapper's last p DGROUP:0x3b98 itself for the mode-4 sounds at it: (-6000,
    // -6000) after mode 0, (x, y) after mode 4 [DESIGN] [BYTES seg10:0x786f seg10:0x7b22]; the
    // wrapper stores it only on a successful play [OPEN seg10:0x786f: whether a refused play
    // leaves the previous p].
    void sound(int id) override {
        is::sound::play(id);
        last_p_ = {-6000, -6000};
    }
    void sound_at(int id, int x, int y, int mode) override {
        is::sound::play_at(id, x, y, mode);
        last_p_ = mode == 4 ? POINT{x, y} : POINT{-6000, -6000};
    }
    void sound_here(int id) override { sound_at(id, last_p_.x, last_p_.y, 4); }
    // seg10:0x73e2: ordinal 7 command 0 once (DGROUP:0x9228) with the INI's sound keys.
    void sound_start() override {
        if (sound_started_ || !g.store || !g.ini) return;
        sound_started_ = true;
        dialogs::load_live_settings(*g.ini);
        // The port's [Port] Music and SoundFont pick the music backend (ADR 0011 decision 3).
        audio::Settings s = dialogs::live_settings();
        settings::PortSettings port;
        port.load(*g.ini);
        s.port_music = port.music;
        // cp1252 as every INI path; a narrow std::filesystem::path would throw on a non-UTF-8
        // byte under MinGW (M7 s2 fixer).
        s.port_soundfont = settings::path_from_ini(port.soundfont);
        const bool ok = audio::init(s, *g.store, g.main);
        log(std::string("sound start: ISPACE ordinal 7 command 0 ") + (ok ? "ok" : "failed"));
    }
    void music(int code) override { is::sound::music(code, 1); }
    void set_mix_channels(int n) {  // seg6:0xc9c, from seg10:0x7f18
        mix_channels_ = n;
        audio::set_inspace_mix_channels(n);  // DGROUP:0x920a for seg10:0x7b48 (M7 fixer)
    }
    void todo(const std::string& what, const char* address) override { log_todo(what, address); }
    void show_window() override { ShowWindow(g.main, SW_SHOW); }
    // Calibrate (ADR 0001 decision 9): the port does not count passes, but keeps the 1000 ms
    // window from the state-0 entry [BYTES seg15:0x58fb seg15:0x5f68], after which seg15:0x6009
    // sets DGROUP:0x1212 and seg6:0xa6c reads the rate settings and writes Calibrate back
    // [BYTES seg6:0xae7 seg6:0xb31].
    void calibrate_start(Tick now) override {
        if (g.calibrated) return;
        g.calibrate_started = true;
        g.calibrate_start = now;
    }
    bool calibrating() override { return g.calibrate_started && !g.calibrated; }
    bool calibrated() override { return g.calibrated; }
    bool calibrate_step(Tick now) override;
    int calibrate_value() override { return shell::kCalibrate; }
    int mix_channels() override { return mix_channels_; }
    // The credits' and the desk exit's busy waits: the surface is shown, then the time passes
    // without messages, as the original's loops on GetCurrentTime [BYTES seg15:0x7f24
    // seg15:0x7fcd seg15:0x12aa]. MsgWaitForMultipleObjectsEx with an empty wake mask removes
    // no message but lets the display driver show the surface (Wine flushes window surfaces
    // there; a Sleep loop leaves the old picture on screen, start-0001) [DESIGN].
    void present_wait(Tick ms) override {
        if (g.gfx && g.main) g.gfx->present(g.main, true);
        GdiFlush();
        const Tick start = now_tick();
        for (Tick t = now_tick(); t - start < ms; t = now_tick())
            MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>(ms - (t - start)), 0, 0);
    }
    void demo_start() override { g.game.request(shell::kIntro); }
    // seg10:0x885e(0) from the state-0 entry and title step 0: the program's check
    // (src/app/startup.cpp), which keeps the flags in g.edition.
    shell::Edition edition_check() override { return run_edition_check(0); }
    intro::LocalDay local_day() override {
        const std::time_t t = std::time(nullptr);
        const std::tm* tm = std::localtime(&t);
        return tm ? intro::LocalDay{tm->tm_year, tm->tm_yday} : intro::LocalDay{};
    }
    std::string lang(int id) override {
        try {
            if (g.store)
                if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) return *s;
        } catch (const std::exception& x) {
            log("string " + std::to_string(id) + ": " + x.what());
        }
        return {};
    }
    std::string exe_string(int id) override {
        try {
            if (g.store)
                if (auto s = fmt::load_string(g.store->module(assets::kMainProgram),
                                              static_cast<std::uint16_t>(id)))
                    return *s;
        } catch (const std::exception& x) {
            log("INSPACE.EXE string " + std::to_string(id) + ": " + x.what());
        }
        return {};
    }
    std::vector<std::uint8_t> lang_resource(int type, int id) override {
        try {
            if (g.store) {
                const fmt::NeModule& m = g.store->language_dll();
                if (const fmt::NeResource* r = m.find(static_cast<std::uint16_t>(type),
                                                      static_cast<std::uint16_t>(id)))
                    return std::vector<std::uint8_t>(m.data(*r), m.data(*r) + m.size(*r));
            }
        } catch (const std::exception& x) {
            log("resource " + std::to_string(type) + " " + std::to_string(id) + ": " + x.what());
        }
        return {};
    }

private:
    POINT last_p_{-6000, -6000};  // DGROUP:0x3b98 as far as the intro moves it
    bool sound_started_ = false;  // DGROUP:0x9228
    int mix_channels_ = 1;  // DGROUP:0x920a before seg6:0xb3e reads MixChannels (default 1)
};

std::unique_ptr<IntroHost> g_intro_host;
std::unique_ptr<intro::GdiCanvas> g_intro_canvas;
std::unique_ptr<intro::Intro> g_intro;

bool IntroHost::calibrate_step(Tick now) {
    // seg15:0x5f68: the count goes on while now <= DGROUP:0x120e + 1000.
    if (g.calibrated || now - g.calibrate_start <= 1000) return false;
    g.calibrated = true;  // DGROUP:0x1212, and DGROUP:0x120e = 0
    try {
        read_rate_settings(now);
        // seg6:0xc9c reads MixChannels (default 1 in INSPACE) into DGROUP:0x920a.
        set_mix_channels(g.ini->get_int(settings::kGameSection, "MixChannels", 1));
        g.ini->set_int(settings::kGameSection, "Calibrate", shell::kCalibrate);
        g.ini->save();
    } catch (const std::exception& x) {
        log(std::string("INSPACE.INI: ") + x.what());
    }
    log("Calibrate done: FrameRateSet " + std::to_string(g.scheduler.rate()) + " at " +
        std::to_string(now) + " ms");
    return true;
}

// One line per intro phase and step, per credits entry and line, and per text page, with the
// tick and the frame count, for the headless checks [DESIGN].
struct IntroTrace {
    int phase = -1, step = -1, entry = -1, line = -1, page = -1;
    unsigned long frames = 0;
    void operator()(const intro::Intro& in) {
        ++frames;
        const bool credits = in.phase() == 3, pages = in.phase() == 4;
        if (in.phase() == phase && in.step() == step &&
            (!credits || (in.credit_entry() == entry && in.credit_line() == line)) &&
            (!pages || in.page() == page))
            return;
        phase = in.phase();
        step = in.step();
        entry = in.credit_entry();
        line = in.credit_line();
        page = in.page();
        std::string extra;
        if (credits)
            extra = " entry " + std::to_string(entry) + " line " + std::to_string(line);
        else if (pages)
            extra = " page " + std::to_string(page);
        char buf[160];
        std::snprintf(buf, sizeof buf, "intro: phase %d step %d%s at %lu ms, frame %lu", phase,
                      step, extra.c_str(), static_cast<unsigned long>(now_tick()), frames);
        log(buf);
    }
};
IntroTrace g_intro_trace;

void release_intro() {
    g_intro.reset();
    g_intro_canvas.reset();
    g_intro_host.reset();
}

}  // namespace

void install_state_hooks() {
    auto& h = g.game.hooks();
    g_intro_host = std::make_unique<IntroHost>();
    // seg6:0xb3e after the Options dialog (seg10:0x7f18 in src/dialogs) (M7 rewiring).
    dialogs::set_rate_settings_reader([] {
        try {
            read_rate_settings(now_tick());
            // seg6:0xc9c: MixChannels (default 1 in INSPACE) into DGROUP:0x920a.
            if (g_intro_host && g.ini)
                g_intro_host->set_mix_channels(g.ini->get_int(settings::kGameSection, "MixChannels", 1));
        } catch (const std::exception& x) {
            log(std::string("INSPACE.INI: ") + x.what());
        }
    });
    g_intro_canvas = std::make_unique<intro::GdiCanvas>(*g.gfx, [](const std::string& s) { log(s); });
    g_intro = std::make_unique<intro::Intro>(*g_intro_canvas, *g_intro_host, shell::game_rng());
    // State 0: the intro of src/intro (shell.md "Intro and title screen"). The entry seg15:0x5786
    // stores GetCurrentTime in DGROUP:0x1e50; the tick's frame passes redraw 0; the repaint
    // seg11:0xafe case 0 ends with the frame's redraw.
    h.enter[shell::kIntro] = [](shell::GameState&) {
        g_intro->enter(now_tick());
        g_intro_trace(*g_intro);
    };
    h.frame[shell::kIntro] = [](shell::GameState& s) {
        g_intro->frame(s.now(), false);
        g_intro_trace(*g_intro);
    };
    h.repaint[shell::kIntro] = [](shell::GameState& s) { g_intro->repaint(s.now()); };
    h.enter[shell::kWave] = [](shell::GameState&) {
        log_todo("state-1 set-up", "seg10:0x0");  // M3
    };
    h.play_area = [](shell::GameState&) {
        // seg2:0x1ac8: the play area from the client rectangle and the clip rectangle without
        // the border inset. Logical pixels: the client and the screen divided by the scale
        // factor (ADR 0006).
        RECT rc;
        GetClientRect(g.main, &rc);
        g.gfx->set_client_size(rc.right, rc.bottom, g.scale_key);
        g.area = shell::play_area_from_client(logical(rc.right), logical(rc.bottom),
                                              logical(g.screen_cx), logical(g.screen_cy));
        g.clip.play = {0, 0, g.area.w, g.area.play_h};  // seg2:0x1b5d
        audio::set_area(g.area.w, g.area.play_h);  // DGROUP:0x90ea, 0x90ec (M7 fixer)
        // srand(GetCurrentTime()) [BYTES seg2:0x1b6b]: the low word of the port's tick, which
        // counts from start-up (ADR 0001 decision 5) [DESIGN].
        shell::game_rng().srand(static_cast<std::uint16_t>(now_tick() & 0xffff));
    };
    h.route_input = [](shell::GameState& s, const shell::Input& in) {
        if (s.state() == shell::kIntro) {
            // seg11:0x7902 acts on WM_KEYUP only [BYTES seg11:0x7918].
            if (in.msg == shell::kWmKeyUp) g_intro->key_up(in.wparam, s.now());
            return;
        }
        if (in.msg != shell::kWmKeyUp && in.msg != kMsgAltKey && in.msg != kMsgCtrlKey) return;
        char buf[96];
        std::snprintf(buf, sizeof buf, "input msg 0x%x key 0x%x in state %d", in.msg, in.wparam,
                      s.state());
        log_todo(buf, "seg11:0x6aa8");
    };
    // The default until install_dialog_hooks (src/dialogs/hooks.cpp) puts in src/nav's window.
    h.open_navigation = [](shell::GameState&) { log_todo("Navigation window", "seg6:0x1c72"); };
    h.save_scores = [](shell::GameState&) { log_todo("score list save", "seg6:0x626c"); };
}

}  // namespace is::app

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    // Timer resolution (ADR 0001 decision 11) [DESIGN]: the scheduler's waits would otherwise
    // round up to the 15.6 ms tick on Windows 10 and 11. Wine wakes to the millisecond.
    struct TimerPeriod {
        bool on;
        explicit TimerPeriod() : on(timeBeginPeriod(1) == TIMERR_NOERROR) {}
        ~TimerPeriod() {
            if (on) timeEndPeriod(1);
        }
    } const period;
    try {
        return is::app::run(instance);
    } catch (const std::exception& x) {
        is::app::log(std::string("fatal: ") + x.what());
        return 1;
    }
}
