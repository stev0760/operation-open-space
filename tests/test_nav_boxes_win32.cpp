// The RACE and DUEL boxes' window side (src/nav/boxes_win32.cpp): created on a plain parent,
// filled from the strings, selected from the INI, placed, shown and read back. Win32 only.
#ifdef _WIN32
#include <windows.h>

#include <filesystem>
#include <string>

#include "doctest.h"
#include "nav/boxes.h"
#include "settings/settings.h"

using namespace is::nav;

namespace {

std::string text_of(HWND combo, int i) {
    wchar_t buf[64] = L"";
    if (SendMessageW(combo, CB_GETLBTEXTLEN, static_cast<WPARAM>(i), 0) >= 63) return {};
    SendMessageW(combo, CB_GETLBTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(buf));
    std::string s;
    for (const wchar_t* p = buf; *p; ++p) s += static_cast<char>(*p);
    return s;
}

RECT rect_in(HWND parent, HWND child) {
    RECT r{};
    GetWindowRect(child, &r);
    MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&r), 2);
    return r;
}

}  // namespace

TEST_CASE("nav boxes win32: create, place, show and read the RACE and DUEL boxes") {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    HWND parent = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 560, 350, nullptr, nullptr,
                                  inst, nullptr);
    REQUIRE(parent);
    is::settings::IniFile ini(std::filesystem::temp_directory_path() / "is_test_nav_boxes_w32.ini");
    ini.set_int(is::settings::kGameSection, "Racers", 3);
    ini.set_int(is::settings::kGameSection, "DuelOpp", 9);  // outside the list: no selection
    const auto lang = [](int id) { return "s" + std::to_string(id); };
    const int ext = create_boxes(parent, inst, nullptr, lang, &ini);
    CHECK(ext > 0);

    HWND racers = GetDlgItem(parent, kRacersId);
    REQUIRE(racers);
    CHECK(SendMessageW(racers, CB_GETCOUNT, 0, 0) == 4);
    CHECK(text_of(racers, 0) == "s2147");
    CHECK(text_of(racers, 3) == "s2150");
    HWND opp = GetDlgItem(parent, kDuelOppId);
    REQUIRE(opp);
    CHECK(SendMessageW(opp, CB_GETCOUNT, 0, 0) == 6);
    CHECK(text_of(opp, 2) == "s2162");  // Similar, shared with RaceOpp
    CHECK(text_of(opp, 5) == "s2169");
    wchar_t cap[16] = L"";
    GetWindowTextW(GetDlgItem(parent, kRaceBoxId), cap, 16);
    CHECK(std::wstring(cap) == L"s2145");
    GetWindowTextW(GetDlgItem(parent, kDuelBoxId), cap, 16);
    CHECK(std::wstring(cap) == L"s2146");
    CHECK((GetWindowLongW(racers, GWL_STYLE) & 0xffff) == 0x0003);  // CBS_DROPDOWNLIST, unsorted

    BoxValues v = read_boxes(parent);
    CHECK(v.racers == 3);
    CHECK(v.race_diff == 0);
    CHECK(v.race_laps == 1);
    CHECK(v.race_opp == 2);
    CHECK(v.duelers == 0);
    CHECK(v.duel_diff == 0);
    CHECK(v.duel_opp == -1);

    place_boxes(parent, 552, 316, ext);
    RECT r = rect_in(parent, GetDlgItem(parent, kRaceBoxId));
    CHECK(r.left == 442);
    CHECK(r.top == 161);
    CHECK(r.right - r.left == 102);
    CHECK(r.bottom - r.top == 122);
    r = rect_in(parent, GetDlgItem(parent, kDuelersId));
    CHECK(r.left == 453);
    CHECK(r.top == 179);
    CHECK(r.right - r.left == 83);

    show_boxes(parent, BoxKind::kRace);
    CHECK(IsWindowVisible(GetDlgItem(parent, kRaceLapsId)) == FALSE);  // the parent is hidden
    CHECK((GetWindowLongW(GetDlgItem(parent, kRaceLapsId), GWL_STYLE) & WS_VISIBLE) != 0);
    CHECK((GetWindowLongW(GetDlgItem(parent, kDuelDiffId), GWL_STYLE) & WS_VISIBLE) == 0);
    show_boxes(parent, BoxKind::kDuel);
    CHECK((GetWindowLongW(GetDlgItem(parent, kRaceLapsId), GWL_STYLE) & WS_VISIBLE) == 0);
    CHECK((GetWindowLongW(GetDlgItem(parent, kDuelBoxId), GWL_STYLE) & WS_VISIBLE) != 0);
    show_boxes(parent, BoxKind::kNone);
    CHECK((GetWindowLongW(GetDlgItem(parent, kDuelBoxId), GWL_STYLE) & WS_VISIBLE) == 0);

    SendMessageW(GetDlgItem(parent, kRaceLapsId), CB_SETCURSEL, 2, 0);
    CHECK(read_boxes(parent).race_laps == 2);
    DestroyWindow(parent);
}
#endif
