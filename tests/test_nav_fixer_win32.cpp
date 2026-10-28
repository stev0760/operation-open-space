// M3 stage 1 fixer cases under Win32: the Host inputs install() checks, and the New Game drive
// list through nav::list_drives (ADR 0012 decision 2).
#ifdef _WIN32
#include <windows.h>

#include <algorithm>
#include <string>

#include "dialogs/dialogs.h"
#include "doctest.h"
#include "nav/nav.h"

TEST_CASE("nav fixer win32: install() names the Host inputs left unset") {
    is::nav::Host h;
    const auto v = is::nav::unset_inputs(h);
    for (const char* n : {"lang", "edition", "player", "store", "command_line", "demo_type", "wave",
                          "restored", "entered"})
        CHECK(std::find(v.begin(), v.end(), std::string(n)) != v.end());
    h.lang = [](int) { return std::string(); };
    h.edition = [] { return -1; };
    h.player = [] { return 1; };
    h.store = [] { return static_cast<const is::assets::AssetStore*>(nullptr); };
    h.command_line = [] { return false; };
    h.demo_type = [] { return 519; };
    h.wave = [] { return 0; };
    h.restored = [] { return 0; };
    h.entered = [](const std::string&) {};
    CHECK(is::nav::unset_inputs(h).empty());
}

TEST_CASE("nav fixer win32: the New Game drive list leaves network drives untouched") {
    is::nav::MemFs fs;
    fs.add_drive('C', is::nav::DriveType::kFixed, "WINDOWSSYSTEM");
    fs.add_drive('D', is::nav::DriveType::kFixed);
    fs.add_drive('E', is::nav::DriveType::kNoRoot);
    fs.add_drive('F', is::nav::DriveType::kRemote, "SHARE");
    const auto d = is::dialogs::NewGame::drives_from(fs, {});
    REQUIRE(d.size() == 3);
    CHECK(d[0].letter == 'C');
    REQUIRE(d[0].label);
    CHECK(*d[0].label == "WINDOWSS.YST");
    CHECK(d[1].letter == 'D');
    CHECK_FALSE(d[1].label);
    CHECK(d[2].letter == 'F');
    CHECK_FALSE(d[2].label);  // Unknown_HD, never probed
}
#endif
