// The RACE and DUEL boxes of the Navigation window (src/nav/boxes.h) against docs/re/nav.md
// "Race and duel boxes", seg6:0x23d4, the WM_SIZE case of seg6:0x1f22 and seg6:0x2dba, and the
// oracle's newgame-0001 window (552 x 316 client).
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include "doctest.h"
#include "nav/boxes.h"
#include "settings/settings.h"

using namespace is::nav;

namespace {

// IS_ASSETS_DIR/eng_lang.dll/strings.tsv (isre all): "id<TAB>text" lines. Empty when absent.
std::map<int, std::string> lang_tsv() {
    std::map<int, std::string> m;
    const char* v = std::getenv("IS_ASSETS_DIR");
    if (!v || !*v) return m;
    std::ifstream f(std::filesystem::path(v) / "eng_lang.dll" / "strings.tsv", std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        try {
            m[std::stoi(line.substr(0, tab))] = line.substr(tab + 1);
        } catch (const std::exception&) {
        }
    }
    return m;
}

std::filesystem::path temp_ini() {
    return std::filesystem::temp_directory_path() / "is_test_nav_boxes_none.ini";
}

}  // namespace

TEST_CASE("nav boxes: the seven combos in seg6:0x23d4's order") {
    const auto& t = combos();
    REQUIRE(t.size() == 7);
    CHECK(t[0].id == 0x8d);
    CHECK(std::string(t[0].key) == "Racers");
    CHECK(t[0].def == 1);
    CHECK(t[0].strings == std::vector<int>{2147, 2148, 2149, 2150});
    CHECK(t[1].id == 0x91);
    CHECK(std::string(t[1].key) == "Duelers");
    CHECK(t[1].def == 0);
    CHECK(t[1].strings == std::vector<int>{2151, 2152, 2153});
    CHECK(t[2].id == 0x8e);
    CHECK(std::string(t[2].key) == "RaceDiff");
    CHECK(t[3].id == 0x92);
    CHECK(std::string(t[3].key) == "DuelDiff");
    CHECK(t[2].strings == t[3].strings);
    CHECK(t[3].strings == std::vector<int>{2154, 2155, 2156});
    CHECK(t[4].id == 0x8f);
    CHECK(std::string(t[4].key) == "RaceLaps");
    CHECK(t[4].def == 1);
    CHECK(t[4].strings == std::vector<int>{2157, 2158, 2159});
    CHECK(t[5].id == 0x90);
    CHECK(std::string(t[5].key) == "RaceOpp");
    CHECK(t[5].def == 2);
    CHECK(t[5].strings == std::vector<int>{2160, 2161, 2162, 2163, 2164, 2165});
    CHECK(t[6].id == 0x94);
    CHECK(std::string(t[6].key) == "DuelOpp");
    CHECK(t[6].def == 2);
    CHECK(t[6].strings == std::vector<int>{2166, 2167, 2162, 2163, 2168, 2169});
    for (const auto& c : t) CHECK(combo(c.id) == &c);
    CHECK(combo(0x8c) == nullptr);
    CHECK(combo(0x93) == nullptr);
}

TEST_CASE("nav boxes: seg6:0x2dba shows one box") {
    CHECK(box_controls(BoxKind::kRace) == std::vector<int>{0x8c, 0x8d, 0x8e, 0x8f, 0x90});
    CHECK(box_controls(BoxKind::kDuel) == std::vector<int>{0x93, 0x91, 0x92, 0x94});
    CHECK(box_controls(BoxKind::kNone).empty());
    for (int id : {0x8c, 0x8d, 0x8e, 0x8f, 0x90}) {
        CHECK(show_command(BoxKind::kRace, id) == 5);
        CHECK(show_command(BoxKind::kDuel, id) == 0);
        CHECK(show_command(BoxKind::kNone, id) == 0);
    }
    for (int id : {0x93, 0x91, 0x92, 0x94}) {
        CHECK(show_command(BoxKind::kRace, id) == 0);
        CHECK(show_command(BoxKind::kDuel, id) == 5);
        CHECK(show_command(BoxKind::kNone, id) == 0);
    }
}

TEST_CASE("nav boxes: the INI selection of seg6:0x23d4") {
    // Without keys: the defaults, which newgame-0001 shows as "2 Ships", "Normal", "5 Laps",
    // "Similar" and "1 Opponent", "Normal", "Similar".
    const BoxValues d = initial_values(nullptr);
    CHECK(d.racers == 1);
    CHECK(d.race_diff == 0);
    CHECK(d.race_laps == 1);
    CHECK(d.race_opp == 2);
    CHECK(d.duelers == 0);
    CHECK(d.duel_diff == 0);
    CHECK(d.duel_opp == 2);

    is::settings::IniFile ini(temp_ini());
    ini.set_int(is::settings::kGameSection, "Racers", 3);
    ini.set_int(is::settings::kGameSection, "RaceLaps", 2);
    ini.set_int(is::settings::kGameSection, "DuelOpp", 5);
    ini.set_int(is::settings::kGameSection, "Duelers", 3);  // past the three entries
    ini.set_int(is::settings::kGameSection, "RaceOpp", -1);
    const BoxValues v = initial_values(&ini);
    CHECK(v.racers == 3);
    CHECK(v.race_laps == 2);
    CHECK(v.duel_opp == 5);
    CHECK(v.duelers == -1);  // CB_SETCURSEL fails: no selection
    CHECK(v.race_opp == -1);
    CHECK(v.race_diff == 0);
    CHECK(v.at(0x8d) == 3);
    CHECK(v.at(0x94) == 5);
}

TEST_CASE("nav boxes: WM_SIZE places the boxes as newgame-0001 shows them") {
    const int cw = 552, ch = 316, ext = 16;
    CHECK(size_box(0x8c, cw, ch, ext) == Box{442, 161, 102, 122});
    CHECK(size_box(0x93, cw, ch, ext) == Box{442, 161, 102, 122});
    // RACE combos at x 455, 80 wide; DUEL combos at x 453, 83 wide; rows 179 ... 254.
    CHECK(size_box(0x8d, cw, ch, ext) == Box{455, 179, 80, 16 * 4 + 30});
    CHECK(size_box(0x8e, cw, ch, ext) == Box{455, 204, 80, 78});
    CHECK(size_box(0x8f, cw, ch, ext) == Box{455, 229, 80, 78});
    CHECK(size_box(0x90, cw, ch, ext) == Box{455, 254, 80, 126});
    CHECK(size_box(0x91, cw, ch, ext) == Box{453, 179, 83, 78});
    CHECK(size_box(0x92, cw, ch, ext) == Box{453, 204, 83, 78});
    CHECK(size_box(0x94, cw, ch, ext) == Box{453, 229, 83, 126});
    CHECK(size_box(0x65, cw, ch, ext) == Box{});
}

TEST_CASE("nav boxes: WM_CREATE's first rectangles") {
    const int cw = 552, ext = 16;
    CHECK(create_box(0x8c, cw, ext) == Box{462, 120, 90, 40});
    CHECK(create_box(0x93, cw, ext) == Box{462, 120, 90, 40});
    CHECK(create_box(0x8d, cw, ext) == Box{492, 120, 60, 78});
    CHECK(create_box(0x91, cw, ext) == Box{492, 120, 60, 78});
    CHECK(create_box(0x8e, cw, ext) == Box{492, 150, 60, 78});
    CHECK(create_box(0x92, cw, ext) == Box{492, 150, 60, 78});
    CHECK(create_box(0x8f, cw, ext) == Box{492, 180, 60, 78});
    CHECK(create_box(0x90, cw, ext) == Box{492, 180, 60, 126});
    CHECK(create_box(0x94, cw, ext) == Box{492, 180, 60, 126});
}

TEST_CASE("nav boxes: the box strings in ENG_LANG.DLL (golden)") {
    const auto m = lang_tsv();
    if (m.empty()) {
        MESSAGE("IS_ASSETS_DIR strings.tsv absent: skipped");
        return;
    }
    const std::map<int, std::string> want = {
        {2145, "RACE"},     {2146, "DUEL"},         {2147, "Solo"},        {2148, "2 Ships"},
        {2149, "3 Ships"},  {2150, "4 Ships"},      {2151, "1 Opponent"},  {2152, "2 Opponents"},
        {2153, "3 Opponents"}, {2154, "Normal"},    {2155, "Difficult"},   {2156, "Dangerous!"},
        {2157, "1 Lap"},    {2158, "5 Laps"},       {2159, "10 Laps"},     {2160, "Fastest"},
        {2161, "Faster"},   {2162, "Similar"},      {2163, "Same Ships"},  {2164, "Slower"},
        {2165, "Slowest"},  {2166, "Toughest"},     {2167, "Tougher"},     {2168, "Weaker"},
        {2169, "Weakest"},
    };
    for (const auto& [id, text] : want) {
        const auto it = m.find(id);
        REQUIRE(it != m.end());
        CHECK(it->second == text);
        CHECK(it->second.size() < static_cast<std::size_t>(kBoxStringMax));
    }
}
