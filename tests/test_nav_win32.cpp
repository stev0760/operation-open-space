// The Navigation window under Win32 (Wine in CTest) on the in-memory fixture drive `basic`:
// class, style, title and placement from the Nav keys, the children with their ids and styles,
// the tree of seg6:0x37ca with its rows in listbox order, the initial selection, the file pane
// and the status of seg6:0x5524, expand and collapse, the parts' hooks, Enter's refusal and
// acceptance with its record, key writes and state request, Help and the SC_CLOSE forward
// (shell.md "Game-start dialogs" items 11 and 12; nav.md; newgame-0001 and newgame-0002).
#ifdef _WIN32
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "fixture_dir.h"
#include "nav/nav.h"
#include "nav/nav_rules.h"
#include "nav_fixture.h"
#include "settings/settings.h"

namespace {

std::string narrow_ascii(const wchar_t* w) {
    std::string s;
    for (; *w; ++w) s.push_back(static_cast<char>(*w));
    return s;
}

std::vector<std::string> items(HWND list) {
    std::vector<std::string> v;
    const auto n = SendMessageW(list, LB_GETCOUNT, 0, 0);
    for (LRESULT i = 0; i < n; ++i) {
        wchar_t buf[256] = L"";
        SendMessageW(list, LB_GETTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(buf));
        v.push_back(narrow_ascii(buf));
    }
    return v;
}

void pick(HWND nav, HWND list, int row) {
    SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(row), 0);
    SendMessageW(nav, WM_COMMAND, MAKEWPARAM(is::nav::kDirListId, LBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
}

void dblclk(HWND nav, HWND list) {
    SendMessageW(nav, WM_COMMAND, MAKEWPARAM(is::nav::kDirListId, LBN_DBLCLK), reinterpret_cast<LPARAM>(list));
}

bool has_line(const std::vector<std::string>& log, const std::string& s) {
    return std::find(log.begin(), log.end(), s) != log.end();
}

}  // namespace

TEST_CASE("nav win32: the IS_NAV window on the fixture drive") {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    HWND main = CreateWindowExW(0, L"STATIC", L"main", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr,
                                inst, nullptr);
    REQUIRE(main);
    is::settings::IniFile ini(std::filesystem::temp_directory_path() / "is_nav_test.ini");
    ini.set_int(is::settings::kGameSection, "NavLeft", 40);  // NavTop, NavRight, NavBottom default
    std::vector<int> requests;
    int helps = 0, blacks = 0, arms = 0;
    std::vector<std::string> log, todos;
    auto strings = std::make_shared<navtest::FakeStrings>();
    strings->s[2140] = "Inner Space Navigation";
    strings->s[2143] = "&Enter";
    strings->s[2144] = "&Help";
    auto fs = std::make_shared<is::nav::MemFs>();
    navtest::build_basic(*fs);
    const std::filesystem::path fixture = fixture_dir();
    is::nav::Records records;
    is::nav::Host h;
    h.instance = inst;
    h.ini = &ini;
    h.lang = [strings](int id) { return strings->lang(id); };
    h.state = [] { return 0; };
    h.black_play_area = [&] { ++blacks; };
    h.wave = [] { return 0; };
    h.restored = [] { return 0; };
    h.player = [] { return 1; };
    h.edition = [] { return 0; };  // limited: the races after the first, the duel and the gate locked
    h.request_state = [&](int s) { requests.push_back(s); };
    h.arm_debriefing = [&] { ++arms; };
    h.help = [&] { ++helps; };
    h.log = [&](const std::string& s) { log.push_back(s); };
    h.todo = [&](const std::string& s, const char*) { todos.push_back(s); };
    std::vector<std::string> entered;
    h.entered = [&](const std::string& d) { entered.push_back(d); };
    h.torn_down = [] { return false; };
    h.fs = fs;
    h.records = &records;
    // The fixture's programs when they are built (oracle/fixtures/make_fixtures.py); otherwise
    // no module parses and every file counts one icon, as ExtractIcon's 0 does.
    h.read_module = [fixture](const std::string& dos) {
        if (fixture.empty() || dos.size() < 3) return is::fmt::IconSource(std::vector<std::uint8_t>{});
        std::string rel = dos.substr(3);
        for (char& ch : rel)
            if (ch == '\\') ch = '/';
        return is::fmt::IconSource::load(fixture / rel);
    };
    REQUIRE(is::nav::install(h));
    int creates = 0, layouts = 0;
    std::vector<int> boxes;
    is::nav::Parts parts;
    parts.create_boxes = [&](HWND, int cw, int ch, HFONT) {
        ++creates;
        CHECK(cw > 0);
        CHECK(ch > 0);
    };
    parts.layout_boxes = [&](HWND, int, int) { ++layouts; };
    parts.show_box = [&](HWND, int kind) { boxes.push_back(kind); };
    is::nav::set_parts(parts);
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
    HWND list = GetDlgItem(nav, is::nav::kDirListId), files = GetDlgItem(nav, is::nav::kFileListId);
    REQUIRE(enter);
    REQUIRE(help);
    REQUIRE(list);
    REQUIRE(files);
    CHECK((static_cast<unsigned long>(GetWindowLongW(files, GWL_STYLE)) & 0xffffu) == 0x0b53u);
    CHECK(creates == 1);
    CHECK(layouts >= 1);
    RECT c;
    GetClientRect(nav, &c);
    RECT er;
    GetWindowRect(enter, &er);
    MapWindowPoints(nullptr, nav, reinterpret_cast<POINT*>(&er), 2);
    CHECK(er.left == c.right - 105);
    CHECK(er.top == 5);
    RECT fr;
    GetWindowRect(files, &fr);
    MapWindowPoints(nullptr, nav, reinterpret_cast<POINT*>(&fr), 2);
    CHECK(fr.left == 164);
    CHECK(fr.right - fr.left == c.right - 284);

    // seg6:0x37ca's rows in listbox order (newgame-0001, newgame-0002)
    const std::vector<std::string> tree0 = {" D:\\112",  "DEEP110",  "DEMON'S GATE103", "Dodge City - DUEL103",
                                            "EMPTY100", "GAMES100", "Grand Prix - RACE103", "Lazy Loop - RACE100",
                                            "SDi Speedway - RACE103", "TOOLS100"};
    CHECK(items(list) == tree0);
    CHECK(is::nav::row_count() == 10);
    // the opening walk: the root probes, then "D:\*.*" attribute 0x15, logged in nav.md's form
    CHECK(has_line(log, "nav fs seg6:0x4190 findfirst \"D:\\*.exe\" 0x00 -> end"));
    CHECK(has_line(log, "nav fs seg6:0x37ca findfirst \"D:\\*.*\" 0x15 -> \"DEEP\""));
    CHECK(has_line(log, "nav fs seg6:0x4748 findfirst \"D:\\DEEP\\*.*\" 0x15 -> \".\""));
    // the initial selection: row 1 (a player game), D:\DEEP\ with no programs
    CHECK(SendMessageW(list, LB_GETCURSEL, 0, 0) == 1);
    CHECK(is::nav::selected() == 1);
    CHECK(is::nav::directory() == "D:\\DEEP\\");
    CHECK(has_line(log, "nav fs seg6:0x420a findfirst \"D:\\DEEP\\*.exe\" 0x00 -> end"));
    CHECK(has_line(log, "nav fs seg6:0x420a findfirst \"D:\\DEEP\\*.dll\" 0x00 -> end"));
    CHECK(is::nav::selection().status == "No apps IN THIS DIRECTORY. Select another directory to enter.");
    CHECK(SendMessageW(files, LB_GETCOUNT, 0, 0) == 0);
    CHECK(IsWindowEnabled(enter));  // sub digit 1 leaves Enter as it was
    CHECK(boxes == std::vector<int>{0});

    // games: three programs, the status of seg6:0x5524 and the preview of the first
    pick(nav, list, 5);
    CHECK(is::nav::directory() == "D:\\GAMES\\");
    CHECK(items(files) == std::vector<std::string>{"ALPHA.EXE 0", "BRAVO.EXE 0", "CHARLIE.EXE 0"});
    CHECK(is::nav::selection().status == "THERE ARE 3 ICONS IN 3 FILES.YOU HAVE NOT BEEN HERE BEFORE. ");
    CHECK(has_line(log, "nav fs seg6:0x4368 ExtractIcon \"D:\\GAMES\\ALPHA.EXE\" -1 -> " +
                            std::string(fixture.empty() ? "0" : "1")));
    CHECK(has_line(log, "nav fs seg6:0x4368 ExtractIcon \"\\\" -1 -> 0"));
    const is::nav::Preview& pv = is::nav::selection().pane.preview;
    if (fixture.empty()) {
        CHECK(pv.kind == is::nav::Preview::Kind::kPlaceholder);
        CHECK(pv.name == "ALPHA.EXE");
    } else {
        CHECK(pv.kind == is::nav::Preview::Kind::kIcon);
        CHECK(pv.path == "D:\\GAMES\\ALPHA.EXE");
    }
    CHECK(IsWindowEnabled(enter));
    // empty: Enter disabled (no programs, no subdirectories)
    pick(nav, list, 4);
    CHECK(is::nav::selection().status == "No apps IN THIS DIRECTORY. Select another directory to enter.");
    CHECK_FALSE(IsWindowEnabled(enter));
    // the built-in levels: the duel box, the locked text, the 'D' icon
    pick(nav, list, 3);
    CHECK(boxes.back() == 2);
    CHECK(is::nav::selection().status ==
          "This is the arena for Inner Space duels. To enter, you must pay for the full game version.");
    CHECK(is::nav::selection().pane.preview.kind == is::nav::Preview::Kind::kBuiltin);
    CHECK(is::nav::selection().pane.preview.icon_id == 107);
    CHECK(IsWindowEnabled(enter));
    pick(nav, list, 7);  // lazy loop is open in the limited edition
    CHECK(boxes.back() == 1);
    CHECK(is::nav::selection().status == "This is an Inner Space race track. Enter here for a thrill!");
    // Enter on a built-in level without the parts' Enter: refused, nothing expands
    SendMessageW(nav, WM_COMMAND, MAKEWPARAM(is::nav::kEnterId, BN_CLICKED), reinterpret_cast<LPARAM>(enter));
    CHECK(requests.empty());
    CHECK(is::nav::window() == nav);

    // expand deep, then a; collapse deep (newgame-0002)
    pick(nav, list, 1);
    dblclk(nav, list);
    std::vector<std::string> t = items(list);
    REQUIRE(t.size() == 11);
    CHECK(t[1] == "DEEP120");
    CHECK(t[2] == "A210");
    CHECK(SendMessageW(list, LB_GETCURSEL, 0, 0) == 1);
    pick(nav, list, 2);
    CHECK(is::nav::directory() == "D:\\DEEP\\A\\");
    dblclk(nav, list);
    t = items(list);
    REQUIRE(t.size() == 12);
    CHECK(t[2] == "A220");
    CHECK(t[3] == "B300");
    CHECK(is::nav::row_count() == 12);
    pick(nav, list, 3);
    CHECK(is::nav::directory() == "D:\\DEEP\\A\\B\\");
    CHECK(items(files) == std::vector<std::string>{"FOXTROT.EXE 0"});
    CHECK(is::nav::selection().status == "THERE IS 1 ICON IN 1 FILE.YOU HAVE NOT BEEN HERE BEFORE. ");
    pick(nav, list, 1);
    dblclk(nav, list);
    CHECK(items(list) == tree0);
    CHECK(is::nav::row_count() == 12);  // DGROUP:0x94b6 is never decremented
    // Enter on deep (no programs) refuses and toggles the row instead
    SendMessageW(nav, WM_COMMAND, MAKEWPARAM(is::nav::kEnterId, BN_CLICKED), reinterpret_cast<LPARAM>(enter));
    CHECK(requests.empty());
    CHECK(items(list)[1] == "DEEP120");
    CHECK(has_line(log, "nav fs seg6:0x301a via seg6:0x4190 findfirst \"D:\\DEEP\\*.exe\" 0x00 -> end"));

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
    // a repaint draws the status band and the preview without trouble
    InvalidateRect(nav, nullptr, TRUE);
    UpdateWindow(nav);

    // Enter on games: the record, the directory for the briefing, state 1, the keys
    t = items(list);
    const auto games = std::find(t.begin(), t.end(), "GAMES100") - t.begin();
    pick(nav, list, static_cast<int>(games));
    MoveWindow(nav, 50, 60, 600, 400, TRUE);
    SendMessageW(nav, WM_COMMAND, MAKEWPARAM(is::nav::kEnterId, BN_CLICKED), reinterpret_cast<LPARAM>(enter));
    CHECK(requests == std::vector<int>{1});
    CHECK(entered == std::vector<std::string>{"D:\\GAMES\\"});  // DGROUP:0x38c8 for the briefing
    REQUIRE(records.list.size() == 1);
    CHECK(records.list[0].path == "D:\\GAMES\\");
    CHECK(records.current == 0);
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

    // reopened: games is visited now (state 1), the selection is row 1 again
    is::nav::open(1, 1);
    nav = is::nav::window();
    REQUIRE(nav);
    list = GetDlgItem(nav, is::nav::kDirListId);
    t = items(list);
    CHECK(std::find(t.begin(), t.end(), "GAMES101") != t.end());
    CHECK(SendMessageW(list, LB_GETCURSEL, 0, 0) == 1);
    pick(nav, list, static_cast<int>(std::find(t.begin(), t.end(), "GAMES101") - t.begin()));
    CHECK(is::nav::selection().record == 0);
    CHECK(is::nav::selection().status == "THERE ARE 3 ICONS IN 3 FILES.0 ICONS WERE CAPTURED AND 0 ICONS WERE DESTROYED.");
    is::nav::close();
    is::nav::set_parts({});
    DestroyWindow(main);
}

TEST_CASE("nav win32: a demo opens on the root row") {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    HWND main = CreateWindowExW(0, L"STATIC", L"main", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr,
                                inst, nullptr);
    REQUIRE(main);
    auto strings = std::make_shared<navtest::FakeStrings>();
    auto fs = std::make_shared<is::nav::MemFs>();
    navtest::build_basic(*fs);
    is::nav::Records records;
    is::nav::Host h;
    h.instance = inst;
    h.lang = [strings](int id) { return strings->lang(id); };
    h.player = [] { return 0; };  // DGROUP:0x95bc = 0
    h.play_mode = [] { return 0; };
    h.edition = [] { return 0; };
    h.fs = fs;
    h.records = &records;
    REQUIRE(is::nav::install(h));
    is::nav::attach(main, nullptr);
    is::nav::open(1, 1);
    HWND nav = is::nav::window();
    REQUIRE(nav);
    HWND list = GetDlgItem(nav, is::nav::kDirListId);
    CHECK(SendMessageW(list, LB_GETCURSEL, 0, 0) == 0);
    CHECK(is::nav::directory() == "D:\\");
    // in the demo nothing is locked
    const auto t = items(list);
    CHECK(std::find(t.begin(), t.end(), "Grand Prix - RACE100") != t.end());
    CHECK(std::find(t.begin(), t.end(), "Dodge City - DUEL100") != t.end());
    CHECK(std::find(t.begin(), t.end(), "DEMON'S GATE103") != t.end());  // the gate needs edition -1 or 2
    is::nav::close();
    DestroyWindow(main);
}
TEST_CASE("nav win32: the glyphs and icons from the user's files (golden)") {
    // IS_ORIGINAL_DIR names the distribution (CMake default work/extracted/game); with
    // IS_NAV_SHOW=<ms> the window stays up that long after each step, for screenshots.
    const char* orig = std::getenv("IS_ORIGINAL_DIR");
    std::unique_ptr<is::assets::AssetStore> store;
    try {
        if (orig && *orig) store = std::make_unique<is::assets::AssetStore>(std::filesystem::path(orig));
    } catch (const std::exception&) {
    }
    if (!store) {
        MESSAGE("IS_ORIGINAL_DIR absent: skipped");
        return;
    }
    const char* show = std::getenv("IS_NAV_SHOW");
    const int show_ms = show ? std::atoi(show) : 0;
    const auto pump = [&] {
        const DWORD end = GetTickCount() + static_cast<DWORD>(show_ms);
        do {
            MSG m;
            while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }
            if (show_ms) Sleep(10);
        } while (show_ms && GetTickCount() < end);
    };
    HINSTANCE inst = GetModuleHandleW(nullptr);
    HWND main = CreateWindowExW(0, L"STATIC", L"main", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, inst, nullptr);
    REQUIRE(main);
    auto fs = std::make_shared<is::nav::MemFs>();
    navtest::build_basic(*fs);
    // IS_NAV_NO_ICONS shows the program-name placeholder of seg9:0x352 instead of the icons.
    const std::filesystem::path fixture = std::getenv("IS_NAV_NO_ICONS") ? std::filesystem::path() : fixture_dir();
    is::nav::Records records;
    std::vector<std::string> todos;
    is::nav::Host h;
    h.instance = inst;
    h.lang = [&store](int id) {
        const auto s = store->find_string(static_cast<std::uint16_t>(id));
        return s ? *s : std::string();
    };
    h.player = [] { return 1; };
    h.edition = [] { return 0; };
    h.todo = [&](const std::string& s, const char*) { todos.push_back(s); };
    h.store = [&store] { return store.get(); };
    h.fs = fs;
    h.records = &records;
    h.read_module = [fixture](const std::string& dos) {
        if (fixture.empty() || dos.size() < 3) return is::fmt::IconSource(std::vector<std::uint8_t>{});
        std::string rel = dos.substr(3);
        for (char& ch : rel)
            if (ch == '\\') ch = '/';
        return is::fmt::IconSource::load(fixture / rel);
    };
    REQUIRE(is::nav::install(h));
    is::nav::attach(main, nullptr);
    is::nav::open(1, 1);
    HWND nav = is::nav::window();
    REQUIRE(nav);
    pump();
    HWND list = GetDlgItem(nav, is::nav::kDirListId);
    CHECK(todos.empty());  // FLDR and FILES load
    CHECK(is::nav::selection().status == "No apps IN THIS DIRECTORY. Select another directory to enter.");
    pick(nav, list, 5);  // games
    CHECK(is::nav::selection().status == "THERE ARE 3 ICONS IN 3 FILES.YOU HAVE NOT BEEN HERE BEFORE. ");
    pump();
    pick(nav, list, 1);  // deep, expanded, then a
    dblclk(nav, list);
    pick(nav, list, 2);
    dblclk(nav, list);
    pump();
    pick(nav, list, 4);  // demon's gate: the 'S' icon
    CHECK(is::nav::selection().pane.preview.icon_id == 105);
    pump();
    pick(nav, list, 5);  // dodge city
    pump();
    is::nav::close();
    DestroyWindow(main);
}
#endif
