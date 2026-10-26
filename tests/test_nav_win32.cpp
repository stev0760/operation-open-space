// The Navigation window stub under Win32 (Wine in CTest): class, style, title and placement
// from the Nav keys, the children with their ids, Enter's state request and key writes, Help and
// the SC_CLOSE forward (shell.md "From ESC to the wave", steps 11 and 12; newgame-0001).
#ifdef _WIN32
#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

#include "doctest.h"
#include "nav/nav.h"
#include "nav/nav_rules.h"
#include "settings/settings.h"

namespace {

std::string narrow_ascii(const wchar_t* w) {
    std::string s;
    for (; *w; ++w) s.push_back(static_cast<char>(*w));
    return s;
}

}  // namespace

TEST_CASE("nav win32: the IS_NAV stub opens, forwards and closes as documented") {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    HWND main = CreateWindowExW(0, L"STATIC", L"main", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr,
                                inst, nullptr);
    REQUIRE(main);
    is::settings::IniFile ini(std::filesystem::temp_directory_path() / "is_nav_test.ini");
    ini.set_int(is::settings::kGameSection, "NavLeft", 40);  // NavTop, NavRight, NavBottom default
    std::vector<int> requests;
    int helps = 0, blacks = 0, arms = 0;
    is::nav::Host h;
    h.instance = inst;
    h.ini = &ini;
    h.lang = [](int id) {
        if (id == 2140) return std::string("Inner Space Navigation");
        if (id == 2143) return std::string("&Enter");
        if (id == 2144) return std::string("&Help");
        return std::string();
    };
    h.state = [] { return 0; };
    h.black_play_area = [&] { ++blacks; };
    h.wave = [] { return 0; };
    h.restored = [] { return 0; };
    h.request_state = [&](int s) { requests.push_back(s); };
    h.arm_debriefing = [&] { ++arms; };
    h.help = [&] { ++helps; };
    std::vector<std::string> entered;
    h.entered = [&](const std::string& d) { entered.push_back(d); };
    h.torn_down = [] { return false; };
    REQUIRE(is::nav::install(h));
    is::nav::attach(main, nullptr);
    is::nav::open(1, 1);  // drive D:
    HWND nav = is::nav::window();
    REQUIRE(nav);
    wchar_t buf[128] = L"";
    GetClassNameW(nav, buf, 128);
    CHECK(narrow_ascii(buf) == "IS_NAV");
    GetWindowTextW(nav, buf, 128);
    CHECK(narrow_ascii(buf) == "Inner Space Navigation");
    // 0x10cc0000; user32 adds WS_CLIPSIBLINGS to every overlapped window
    CHECK((static_cast<unsigned long>(GetWindowLongW(nav, GWL_STYLE)) & 0xffff0000ul &
           ~static_cast<unsigned long>(WS_CLIPSIBLINGS)) == 0x10cc0000ul);
    CHECK(GetWindow(nav, GW_OWNER) == main);
    RECT r;
    GetWindowRect(nav, &r);
    CHECK(r.left == 40);
    CHECK(r.top == 30);
    CHECK(r.right - r.left == 560);
    CHECK(r.bottom - r.top == 350);
    CHECK(blacks == 1);  // the play area goes black outside state 2
    CHECK_FALSE(IsWindowEnabled(main));
    HWND enter = GetDlgItem(nav, is::nav::kEnterId), help = GetDlgItem(nav, is::nav::kHelpId);
    HWND list = GetDlgItem(nav, is::nav::kDirListId);
    REQUIRE(enter);
    REQUIRE(help);
    REQUIRE(list);
    RECT c;
    GetClientRect(nav, &c);
    RECT er;
    GetWindowRect(enter, &er);
    MapWindowPoints(nullptr, nav, reinterpret_cast<POINT*>(&er), 2);
    CHECK(er.left == c.right - 105);
    CHECK(er.top == 5);
    CHECK(SendMessageW(list, LB_GETCOUNT, 0, 0) == 1);
    SendMessageW(list, LB_GETTEXT, 0, reinterpret_cast<LPARAM>(buf));
    CHECK(narrow_ascii(buf) == "d:\\");
    // a second open only shows it
    is::nav::open(1, 0);
    CHECK(is::nav::window() == nav);
    // Help and SC_CLOSE
    SendMessageW(nav, WM_COMMAND, MAKEWPARAM(is::nav::kHelpId, BN_CLICKED), reinterpret_cast<LPARAM>(help));
    CHECK(helps == 1);
    SendMessageW(nav, WM_SYSCOMMAND, SC_CLOSE, 0);
    MSG m;
    bool forwarded = false;
    while (PeekMessageW(&m, main, WM_SYSCOMMAND, WM_SYSCOMMAND, PM_REMOVE))
        if (m.wParam == SC_CLOSE) forwarded = true;
    CHECK(forwarded);
    CHECK(IsWindow(nav));
    // Enter: a new game requests state 1, closes the window and writes the keys
    MoveWindow(nav, 50, 60, 600, 400, TRUE);
    SendMessageW(nav, WM_COMMAND, MAKEWPARAM(is::nav::kEnterId, BN_CLICKED), reinterpret_cast<LPARAM>(enter));
    CHECK(requests == std::vector<int>{1});
    CHECK(entered == std::vector<std::string>{"D:\\"});  // DGROUP:0x38c8 for the briefing
    CHECK(arms == 0);
    CHECK(is::nav::window() == nullptr);
    CHECK_FALSE(IsWindow(nav));
    CHECK(IsWindowEnabled(main));
    const auto e = ini.entries(is::settings::kGameSection);
    std::vector<std::string> keys;
    for (const auto& kv : e) keys.push_back(kv.first + "=" + kv.second);
    CHECK(keys == std::vector<std::string>{"NavLeft=50", "NavRight=600", "NavTop=60", "NavBottom=400"});
    // close() without a window does nothing
    is::nav::close();
    CHECK(is::nav::window() == nullptr);
    DestroyWindow(main);
}
#endif
