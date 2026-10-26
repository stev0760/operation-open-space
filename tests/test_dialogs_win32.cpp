// The ordinal-10 dialogs under Win32 (Wine in CTest): each one created modeless from its
// ENG_LANG.DLL template with its ported procedure, its client size checked against the
// oracle's table (dialogs-0001), its controls against the documented ids and its procedure's
// fills and OK writes against shell.md. Skips without the original files (IS_ORIGINAL_DIR).
#ifdef _WIN32
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "dialogs/dialogs.h"
#include "doctest.h"
#include "settings/settings.h"

using namespace is::dialogs;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

struct Rig {
    std::filesystem::path dir = original_dir();
    std::unique_ptr<is::assets::AssetStore> store;
    is::settings::IniFile ini{std::filesystem::temp_directory_path() / "is_dialogs_test.ini"};
    Env env;
    std::vector<std::string> boxes;
    std::vector<std::filesystem::path> opened;
    std::vector<std::string> todos;
    int answer = IDNO;

    bool ok() const { return !dir.empty(); }
    Rig() {
        if (dir.empty()) return;
        store = std::make_unique<is::assets::AssetStore>(dir);
        env.instance = GetModuleHandleW(nullptr);
        env.store = store.get();
        env.ini = &ini;
        env.local_path = std::filesystem::temp_directory_path() / "is_dialogs_none";
        env.text_dir = dir;
        env.ship_dir = dir;
        env.screen_cx = 1024;
        env.screen_cy = 768;
        env.x5 = 1;  // the limited version
        env.tm_mday = 24;
        env.tm_mon = 9;
        env.tm_year = 126;
        env.lang = [this](int id) {
            auto s = store->find_string(static_cast<std::uint16_t>(id));
            return s ? *s : std::string();
        };
        env.exe_string = [this](int id) {
            auto s = is::fmt::load_string(store->module(is::assets::kMainProgram),
                                          static_cast<std::uint16_t>(id));
            return s ? *s : std::string();
        };
        env.todo = [this](const std::string& w, const char*) { todos.push_back(w); };
        env.message_box = [this](HWND, const std::string& text, const std::string&, UINT) {
            boxes.push_back(text);
            return answer;
        };
        env.open_text = [this](const std::filesystem::path& p) { opened.push_back(p); };
        ini.set(is::settings::kGameSection, "MainShip", "SKY TALON");
        ini.set(is::settings::kGameSection, "ThePlayer", "Your Name");
        const char* teams[] = {"AVENGERS", "PREDATORS", "FUZZY ONES",   "PIRATES",
                               "KNIGHTS",  "SPEED DEMONS", "RENEGADES", "ENFORCERS"};
        for (int i = 0; i < 8; ++i) ini.set("Teams", "Team" + std::to_string(i + 1), teams[i]);
    }
};

SIZE client(HWND h) {
    RECT r;
    GetClientRect(h, &r);
    return {r.right, r.bottom};
}

bool has(HWND h, int id) { return GetDlgItem(h, id) != nullptr; }

std::string text_of(HWND h, int id) {
    wchar_t buf[1024] = L"";
    GetDlgItemTextW(h, id, buf, 1024);
    return narrow(buf);
}

std::string cls(HWND h) {
    wchar_t buf[64] = L"";
    GetClassNameW(h, buf, 64);
    return narrow(buf);
}

void click(HWND h, int id) {
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(h, id)));
}

void choose(HWND h, int id, int index) {
    SendDlgItemMessageW(h, id, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(GetDlgItem(h, id)));
}

int combo_count(HWND h, int id) {
    return static_cast<int>(SendDlgItemMessageW(h, id, CB_GETCOUNT, 0, 0));
}

std::string combo_item(HWND h, int id, int i) {
    wchar_t buf[256] = L"";
    SendDlgItemMessageW(h, id, CB_GETLBTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(buf));
    return narrow(buf);
}

// The child's rectangle in the dialog's client coordinates.
RECT child_rect(HWND dlg, HWND c) {
    RECT r;
    GetWindowRect(c, &r);
    MapWindowPoints(nullptr, dlg, reinterpret_cast<POINT*>(&r), 2);
    return r;
}

HWND lev_child(HWND dlg) {
    for (HWND c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        if (cls(c) == "LevClass") return c;
    return nullptr;
}

}  // namespace

TEST_CASE("dialogs win32: the client sizes of dialogs-0001 and the documented controls") {
    Rig r;
    if (!r.ok()) return;
    struct Expect {
        std::unique_ptr<Dialog> d;
        int w, h;
        std::vector<int> ids;
    };
    std::vector<Expect> all;
    all.push_back({std::make_unique<RestoreGame>(r.env, std::vector<std::string>{}), 315, 232,
                   {1, 333, 332, 331, 329, 269, 330}});
    all.push_back({std::make_unique<NewGame>(r.env, false, std::vector<NewGame::Drive>{{'C', {}}}),
                   297, 190, {1, 333, 322, 324, 325, 326, 330, 332}});
    all.push_back({std::make_unique<NewGame>(r.env, true, std::vector<NewGame::Drive>{{'C', {}}}),
                   297, 171, {1, 333, 324, 326, 330}});
    all.push_back({std::make_unique<ExitDialog>(r.env, kCmdExit), 339, 93, {1, 333, 593, 590, 591, 326}});
    all.push_back({std::make_unique<Benefits>(r.env, kCmdBenefits), 416, 414, {1, 2, 591, 800, 801}});
    all.push_back({std::make_unique<Benefits>(r.env, kCmdPlusPack), 516, 442, {1, 2, 591, 800, 801}});
    all.push_back({std::make_unique<Important>(r.env), 465, 401, {1, 2, 245, 247, 591, 989}});
    all.push_back({std::make_unique<Order>(r.env), 453, 382,
                   {1, 333, 694, 695, 700, 701, 704, 709, 710, 711, 712}});
    all.push_back({std::make_unique<SaveGame>(r.env, std::vector<std::string>{}), 270, 70, {1, 333, 332, 334}});
    for (Expect& e : all) {
        CAPTURE(e.d->template_id());
        HWND h = e.d->create_modeless();
        REQUIRE(h != nullptr);
        const SIZE s = client(h);
        CHECK(s.cx == e.w);
        CHECK(s.cy == e.h);
        for (int id : e.ids) {
            CAPTURE(id);
            CHECK(has(h, id));
        }
        // centred as MoveWindow does it: the window's centre at the screen's (512, 384)
        RECT w;
        GetWindowRect(h, &w);
        CHECK(std::abs((w.left + w.right) - 1024) <= 1);
        CHECK(std::abs((w.top + w.bottom) - 768) <= 1);
        CHECK_FALSE(e.d->ended());
        // Win16 edits keep a 1-pixel border: no 3D client edge (start-0001's crops)
        for (HWND c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
            if (cls(c) != "Edit") continue;
            CAPTURE(GetDlgCtrlID(c));
            CHECK((GetWindowLongW(c, GWL_EXSTYLE) & WS_EX_CLIENTEDGE) == 0);
            RECT wr, cr;
            GetWindowRect(c, &wr);
            GetClientRect(c, &cr);
            const LONG st = GetWindowLongW(c, GWL_STYLE);
            const int sb = (st & WS_VSCROLL) ? GetSystemMetrics(SM_CXVSCROLL) : 0;
            CHECK((wr.right - wr.left) - cr.right - sb <= 2);
            CHECK((wr.bottom - wr.top) - cr.bottom <= 2);
        }
    }
    // New Demo lacks ZOOM LEVEL and PLAY
    CHECK_FALSE(has(all[2].d->hwnd(), 332));
    CHECK_FALSE(has(all[2].d->hwnd(), 325));
}

TEST_CASE("dialogs win32: Restore Game's fill and OK") {
    Rig r;
    if (!r.ok()) return;
    RestoreGame d(r.env, {"DEMOGAME.SAV"});
    HWND h = d.create_modeless();
    REQUIRE(h);
    CHECK(combo_count(h, 332) == 1);  // DEMOGAME is skipped
    CHECK(combo_item(h, 332, 0) == " NEW GAME");
    CHECK(SendDlgItemMessageW(h, 332, CB_GETCURSEL, 0, 0) == 0);
    CHECK(text_of(h, 331) == "?");
    CHECK(text_of(h, 329) == "?");
    CHECK(text_of(h, 269) == "?");
    CHECK(text_of(h, 330) == "NEW GAME, STARTING AT WAVE 1.");
    CHECK(d.found() == 1);
    // the GAME combo: h * (1 + found) + 30 high, the preview at (4, client bottom - 113)
    HWND lev = lev_child(h);
    REQUIRE(lev);
    const RECT lr = child_rect(h, lev);
    CHECK(lr.left == 4);
    CHECK(lr.top == 119);  // dialogs-0001: (4, 119) in the 315 x 232 client
    CHECK(lr.right - lr.left == 96);
    CHECK(GetDlgCtrlID(lev) == 100);
    // the normal font on the statics, the bold one on the buttons
    const auto font = [&](int id) {
        HFONT f = reinterpret_cast<HFONT>(SendDlgItemMessageW(h, id, WM_GETFONT, 0, 0));
        LOGFONTW lf{};
        GetObjectW(f, sizeof lf, &lf);
        return lf;
    };
    CHECK(font(1).lfWeight == 730);
    CHECK(font(1).lfHeight == 14);
    CHECK(font(331).lfWeight == 0);
    CHECK(std::wstring(font(331).lfFaceName) == L"Arial");
    click(h, 1);
    CHECK(d.ended());
    CHECK(d.result() == 1);
    CHECK(d.writes() == std::vector<IniWrite>{{"Restore", " NEW GAME"}});
    CHECK(r.ini.get_string(is::settings::kGameSection, "Restore", "") == "NEW GAME");
    RestoreGame c(r.env, {});
    HWND hc = c.create_modeless();
    click(hc, 333);
    CHECK(c.result() == 0);
    CHECK(c.writes().empty());
}

TEST_CASE("dialogs win32: New Game's fill, the limited refusal and the OK writes") {
    Rig r;
    if (!r.ok()) return;
    NewGame d(r.env, false, {{'C', {}}, {'D', std::string("FIXTURE")}});
    HWND h = d.create_modeless();
    REQUIRE(h);
    CHECK(text_of(h, 322) == "Your Name");
    CHECK(combo_count(h, 324) == 61);  // newgame-0001: 61 ships
    CHECK(combo_item(h, 324, 0) == "AQUATIC BULLET");
    CHECK(text_of(h, 324) == "TRANSFORMER");  // the limited version selects TRANSFORMER
    CHECK(text_of(h, 326) == "AVENGERS");
    CHECK(combo_count(h, 325) == 2);
    CHECK(combo_item(h, 325, 1) == "COMMAND");
    CHECK(combo_item(h, 330, 0) == "C: Unknown_HD");
    CHECK(combo_item(h, 330, 1) == "D: FIXTURE");
    CHECK(SendDlgItemMessageW(h, 330, CB_GETCURSEL, 0, 0) == 0);  // Drive defaults to C
    CHECK(combo_item(h, 332, 2) == "1/2x");
    CHECK(SendDlgItemMessageW(h, 332, CB_GETCURSEL, 0, 0) == 1);  // 1x
    HWND lev = lev_child(h);
    REQUIRE(lev);
    const RECT lr = child_rect(h, lev);
    CHECK(lr.left == 193);  // dialogs-0001: (193, 87) in the 297 x 190 client
    CHECK(lr.top == 87);
    CHECK(r.ini.get_string(is::settings::kGameSection, "MainShip", "") == "SKY TALON");  // no write
    // another ship: the 2326 + 2327 box and the dialog stays
    choose(h, 324, 0);
    CHECK(text_of(h, 326) == "SPEED DEMONS");  // newgame-0001: AQUATIC BULLET's team
    click(h, 1);
    REQUIRE(r.boxes.size() == 1);
    CHECK(r.boxes[0].find("only use the Transformer ship") != std::string::npos);
    CHECK_FALSE(d.ended());
    // back to TRANSFORMER, drive D, Command mode: the StratAsk question; No keeps it open
    const int t = static_cast<int>(SendDlgItemMessageW(h, 324, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1),
                                                       reinterpret_cast<LPARAM>(L"TRANSFORMER")));
    choose(h, 324, t);
    choose(h, 330, 1);
    choose(h, 325, 1);
    r.answer = IDNO;
    click(h, 1);
    REQUIRE(r.boxes.size() == 2);
    CHECK(r.boxes[1].find("Command Mode") != std::string::npos);
    CHECK_FALSE(d.ended());
    r.answer = IDYES;
    click(h, 1);
    CHECK(d.ended());
    CHECK(d.result() == 1);
    // the writes of both OKs in order; Yes adds StratAsk = 1
    const std::vector<IniWrite> once{{"MainShip", "TRANSFORMER"}, {"Drive", "D"},
                                     {"Size", "1"},               {"SetSize", "1"},
                                     {"ThePlayer", "Your Name"},  {"PlayMode", "1"}};
    std::vector<IniWrite> expect = once;
    expect.insert(expect.end(), once.begin(), once.end());
    expect.push_back({"StratAsk", "1"});
    CHECK(d.writes() == expect);
}

TEST_CASE("dialogs win32: New Demo writes MainShip and Drive only") {
    Rig r;
    if (!r.ok()) return;
    r.env.x5 = 0;  // a registered copy: the INI ship stays
    r.env.availability_cache = 1;
    NewGame d(r.env, true, {{'C', {}}});
    HWND h = d.create_modeless();
    REQUIRE(h);
    CHECK(text_of(h, 324) == "SKY TALON");
    CHECK(combo_count(h, 324) > 61);
    click(h, 1);
    CHECK(d.result() == 1);
    CHECK(d.writes() == std::vector<IniWrite>{{"MainShip", "SKY TALON"}, {"Drive", "C"}});
}

TEST_CASE("dialogs win32: the exit dialog's texts, Save & Exit and results") {
    Rig r;
    if (!r.ok()) return;
    {
        ExitDialog d(r.env, kCmdExit);  // x4 = 0: the title
        HWND h = d.create_modeless();
        CHECK(text_of(h, 590) == "Are you sure you want to exit Inner Space?");
        CHECK_FALSE(IsWindowEnabled(GetDlgItem(h, 593)));
        click(h, 1);
        CHECK(d.result() == kExitExit);
    }
    {
        r.env.x4 = 1;  // the wave
        ExitDialog d(r.env, kCmdAbort);
        HWND h = d.create_modeless();
        CHECK(text_of(h, 590) == "Do you want to exit from the current game back to the intro?");
        CHECK(IsWindowEnabled(GetDlgItem(h, 593)));
        click(h, 593);
        CHECK(d.result() == kExitSave);
    }
    {
        ExitDialog d(r.env, kCmdExit);
        HWND h = d.create_modeless();
        click(h, 333);
        CHECK(d.ended());
        CHECK(d.result() == kExitCancel);
    }
}

TEST_CASE("dialogs win32: #799, #1872, Important Information and Save Game") {
    Rig r;
    if (!r.ok()) return;
    {
        Benefits d(r.env, kCmdBenefits);
        HWND h = d.create_modeless();
        CHECK(text_of(h, 801).find("EXPERIENCE THE REAL INNER SPACE") == 0);
        click(h, 1);
        CHECK(d.result() == 1);
    }
    {
        Benefits d(r.env, kCmdPlusPack);
        HWND h = d.create_modeless();
        CHECK_FALSE(text_of(h, 801).empty());
        click(h, 1);  // Order Form: the text file, the dialog stays
        CHECK_FALSE(d.ended());
        REQUIRE(r.opened.size() == 1);
        CHECK(r.opened[0].filename() == "ORDER.TXT");
        click(h, 2);
        CHECK(d.result() == 0);
    }
    {
        Important d(r.env);
        HWND h = d.create_modeless();
        CHECK(text_of(h, 245).find("OPERATION: INNER SPACE") == 0);
        click(h, 1);
        CHECK(d.result() == 1);
        CHECK(d.writes() == std::vector<IniWrite>{{"Accept", "42"}});
        CHECK(r.ini.get_int(is::settings::kGameSection, "Accept", 0) == 42);
    }
    {
        r.ini.set(is::settings::kGameSection, "Restore", "ORACLE1");
        SaveGame d(r.env, {"LASTGAME.SAV", "ORACLE1.SAV", "DEMOGAME.SAV"});
        HWND h = d.create_modeless();
        HWND c = GetDlgItem(h, 332);
        REQUIRE(c);
        // 0x50210102; the combo box keeps WS_VSCROLL for its list, not its own style
        CHECK((static_cast<DWORD>(GetWindowLongW(c, GWL_STYLE)) & ~static_cast<DWORD>(WS_VSCROLL)) ==
              0x50010102u);
        const RECT cr = child_rect(h, c);
        CHECK(cr.left == 65);
        CHECK(cr.top == 10);
        CHECK(combo_count(h, 332) == 2);
        CHECK(text_of(h, 332) == "ORACLE1");
        SetDlgItemTextW(h, 332, L"LASTGAME");
        click(h, 1);
        CHECK_FALSE(d.ended());
        CHECK(text_of(h, 332).empty());
        SetDlgItemTextW(h, 332, L"NEW ONE");
        click(h, 1);
        CHECK(d.result() == 1);
        CHECK(d.writes() == std::vector<IniWrite>{{"Restore", "NEW"}});
    }
}

TEST_CASE("dialogs win32: Order's serial, day text and disabled Register") {
    Rig r;
    if (!r.ok()) return;
    r.env.x4 = 12087;
    r.env.x5 = 12341;
    Order d(r.env);
    HWND h = d.create_modeless();
    REQUIRE(h);
    // the shipdisk's IGRAPHIC.DLL #5500 110 is all zero: the template text stays [OPEN]
    CHECK(text_of(h, 700) == "xxxxx");
    CHECK(text_of(h, 710) == "You are on day 12087 of your demo trial period.");
    for (int id : {694, 695, 710, 704, 696, 706}) {  // bold [BYTES ISPACE.DLL seg10:0x74]
        CAPTURE(id);
        HFONT f = reinterpret_cast<HFONT>(SendDlgItemMessageW(h, id, WM_GETFONT, 0, 0));
        LOGFONTW lf{};
        GetObjectW(f, sizeof lf, &lf);
        CHECK(lf.lfWeight == 730);
    }
    CHECK(text_of(h, 712).find("If today is not 10/24/126,") != std::string::npos);
    CHECK_FALSE(IsWindowVisible(GetDlgItem(h, 694)));
    CHECK_FALSE(IsWindowEnabled(GetDlgItem(h, 1)));
    CHECK((GetWindowLongW(GetDlgItem(h, 701), GWL_STYLE) & ES_READONLY) != 0);
    HWND lev = lev_child(h);
    REQUIRE(lev);
    const RECT lr = child_rect(h, lev);
    CHECK(lr.right - lr.left == 146);
    CHECK(lr.bottom - lr.top == 114);
    CHECK(lr.top == 70);
    click(h, 711);
    REQUIRE(r.opened.size() == 1);
    CHECK(r.opened[0].filename() == "README.TXT");
    click(h, 333);
    CHECK(d.result() == 0);
    CHECK_FALSE(IsWindow(lev));
}
#endif

#ifdef _WIN32
TEST_CASE("dialogs win32: saved-game names come back upper case, as _dos_findfirst's") {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "is_dialogs_saves";
    fs::remove_all(dir);
    fs::create_directories(dir / "GAMES");
    for (const char* n : {"demogame.sav", "lastgame.sav", "Mine.sav"})
        std::ofstream(dir / "GAMES" / n) << "x";
    std::vector<std::string> names = list_saves(dir);
    std::sort(names.begin(), names.end());
    CHECK(names == std::vector<std::string>{"DEMOGAME.SAV", "LASTGAME.SAV", "MINE.SAV"});
    // so the exact DEMOGAME and LASTGAME skips apply [BYTES ISPACE.DLL seg10:0x15af]
    CHECK(restore_game_names(names, nullptr) == std::vector<std::string>{"LASTGAME", "MINE"});
    CHECK(save_game_names(names, nullptr) == std::vector<std::string>{"DEMOGAME", "MINE"});
    fs::remove_all(dir);
}
#endif
