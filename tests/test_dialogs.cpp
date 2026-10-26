// The portable rules of the ordinal-10 dialogs (src/dialogs/rules.h) against docs/re/shell.md
// "Game-start dialogs" and "Exit": the list fills from synthetic file lists, the availability
// rule, the OK writes in their order, the drive-letter and zoom arithmetic and the result
// mapping. A golden case checks the shipdisk's MASTER.SHP against the oracle's New Game list.
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "dialogs/rules.h"
#include "doctest.h"

using namespace is::dialogs;
using is::fmt::BodyRecord;
using is::fmt::MasterBod;
using is::fmt::MasterShp;
using is::fmt::ShipRecord;

namespace {

ShipRecord ship(const std::string& name, int team, int body) {
    ShipRecord r;
    r.name = name;
    r.team = static_cast<std::uint8_t>(team);
    r.body = static_cast<std::uint16_t>(body);
    return r;
}
BodyRecord body(int bitmap) {
    BodyRecord b;
    b.name = "B";
    b.bitmap_id = static_cast<std::uint16_t>(bitmap);
    return b;
}

// Bodies 0-3 with bitmap ids 100 (allowed), 178 (refused), 604 (refused) and 601 (allowed).
struct Tables {
    MasterShp ships;
    MasterBod bodies;
    Tables() {
        bodies.records = {body(100), body(178), body(604), body(601)};
        ships.records = {ship("EAGLE", 0, 1),       ship("TRANSFORMER", 0, 0),
                         ship("SKY TALON", 2, 3),   ship("", 0, 0),
                         ship("RENEGADE ONE", 7, 0), ship("DOGGY", 1, 2)};
    }
    Availability limited() const { return {0, 1, &ships, &bodies}; }
    Availability full() const { return {1, 0, &ships, &bodies}; }
};

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

}  // namespace

TEST_CASE("dialogs: x5 and the date number of seg10:0x82a2") {
    CHECK(x5_for_edition(0) == 1);
    CHECK(x5_for_edition(3) == 3);
    CHECK(x5_for_edition(-1) == 0);
    CHECK(x5_for_edition(1) == 0);
    CHECK(x5_for_edition(2) == 0);
    CHECK(date_number(126, 297) == 12342);  // 2026-10-25: (126 - 93) * 365 + 297
    CHECK(date_number(93, 0) == 0);
    CHECK(date_number(300, 0) == static_cast<std::int16_t>(207 * 365));  // a 16-bit word
}

TEST_CASE("dialogs: Restore Game's GAME list and result") {
    int found = 0;
    const auto names =
        restore_game_names({"LASTGAME.SAV", "DEMOGAME.SAV", "ZED.SAV", "demogame.SAV", "AB.SAV"},
                           &found);
    CHECK(found == 5);  // DEMOGAME counts for the combo height
    CHECK(names == std::vector<std::string>{"LASTGAME", "ZED", "demogame", "AB"});  // lstrcmp
    std::vector<std::string> all{kNewGameEntry};
    all.insert(all.end(), names.begin(), names.end());
    const auto shown = sorted_like_combo(all);
    CHECK(shown.front() == " NEW GAME");  // the leading space sorts first: entry 0
    CHECK(shown[1] == "AB");
    CHECK(combo_height(14, 1) == 44);  // dialogs-0001: 44 with only NEW GAME
    CHECK(combo_height(14, 1 + found) == 114);
    CHECK(play_combo_height(14) == 58);  // dialogs-0001: PLAY 58 high
    CHECK(combo_height(14, 2) == 58);    // DRIVE with C: and D:
    CHECK(restore_game_result(true, 0) == 1);
    CHECK(restore_game_result(true, 3) == 2);
    CHECK(restore_game_result(false, 0) == 0);
    CHECK(restore_game_result(false, 2) == 0);
}

TEST_CASE("dialogs: Save Game's list and name rule") {
    int added = 0;
    const auto names = save_game_names({"LASTGAME.SAV", "DEMOGAME.SAV", "ORACLE1.SAV"}, &added);
    CHECK(names == std::vector<std::string>{"DEMOGAME", "ORACLE1"});
    CHECK(added == 2);
    CHECK(save_name("ORACLE2") == std::optional<std::string>("ORACLE2"));
    CHECK(save_name("ABCDEFGHIJ") == std::optional<std::string>("ABCDEFGH"));
    CHECK(save_name("MY GAME") == std::optional<std::string>("MY"));
    CHECK_FALSE(save_name("").has_value());
    CHECK_FALSE(save_name(" LEADING").has_value());
    CHECK_FALSE(save_name("LASTGAME").has_value());
    CHECK_FALSE(save_name("LASTGAME2").has_value());  // cut to 8 first
}

TEST_CASE("dialogs: seg10:0x8568's SavePrompt and Restore rule") {
    CHECK(save_dialog_opens(1, 1, "ORACLE1"));
    CHECK_FALSE(save_dialog_opens(1, 0, "ORACLE1"));
    CHECK(save_dialog_opens(1, 0, ""));
    CHECK(save_dialog_opens(-1, 0, "NEW GAME"));      // the stored value: a space at index 3
    CHECK_FALSE(save_dialog_opens(3, 0, " NEW GAME"));  // the combo text would not pass
    CHECK(save_dialog_opens(0, 0, "ORACLE1"));         // 0 and 2 always open it
    CHECK(save_dialog_opens(2, 0, "ORACLE1"));
    CHECK(save_destroys_window(-1));
    CHECK(save_destroys_window(1));
    CHECK_FALSE(save_destroys_window(0));
    CHECK_FALSE(save_destroys_window(3));
}

TEST_CASE("dialogs: the save header fields of seg10:0x1a80") {
    std::vector<std::uint8_t> h(kSaveHeaderRead - 1, 0);
    CHECK_FALSE(parse_save_header(h).has_value());  // a short read
    h.resize(0x1c0, 0);
    const char pilot[] = "ACE PILOT";
    std::copy(pilot, pilot + 9, h.begin() + 4);
    h[0x1e] = 0x40;  // score 0x00012340
    h[0x1f] = 0x23;
    h[0x20] = 0x01;
    h[0x22] = 5;  // ship
    h[0x24] = 7;  // wave
    h[0x170] = 3;
    const auto s = parse_save_header(h);
    REQUIRE(s.has_value());
    CHECK(s->pilot == "ACE PILOT");
    CHECK(s->score == 0x12340);
    CHECK(s->ship == 5);
    CHECK(s->wave == 7);
    CHECK(s->preview == 3);
}

TEST_CASE("dialogs: the availability rule seg10:0x611c") {
    Tables t;
    for (int w : {178, 310, 320, 400, 440, 450, 520, 550, 560, 590, 600, 604, 605, 710, 720, 730,
                  750, 890, 900, 930, 950})
        CHECK(limited_body(w));
    for (int w : {0, 100, 177, 601, 602, 603, 606, 951}) CHECK_FALSE(limited_body(w));
    CHECK(availability_cache(3, 1) == 0);  // command 2 refused: the limited cache
    CHECK(availability_cache(0, 1) == 1);
    CHECK(availability_cache(0, 0) == 0);
    // the limited cache: the body list decides
    CHECK_FALSE(ship_allowed(t.limited(), 0));  // body 178
    CHECK(ship_allowed(t.limited(), 1));
    CHECK(ship_allowed(t.limited(), 2));         // body 601
    CHECK_FALSE(ship_allowed(t.limited(), 5));  // body 604
    // a registered cache allows every ship
    CHECK(ship_allowed(t.full(), 0));
    CHECK(ship_allowed(t.full(), 5));
    // x5 = 3 refuses every ship but 1, whatever the cache
    Availability three{1, 3, &t.ships, &t.bodies};
    CHECK(ship_allowed(three, 1));
    CHECK_FALSE(ship_allowed(three, 2));
    // the New Game list: named, team not 7, allowed, in record order
    CHECK(new_game_ships(t.limited()) == std::vector<int>{1, 2});
    CHECK(new_game_ships(t.full()) == std::vector<int>{0, 1, 2, 5});
    // a save's ship: above count - 1 gives 0, then a refused one gives 1
    CHECK(restore_ship_index(2, t.limited()) == 2);
    CHECK(restore_ship_index(40, t.limited()) == 1);  // 0 is refused in turn
    CHECK(restore_ship_index(40, t.full()) == 0);
    CHECK(restore_ship_index(5, t.limited()) == 1);
}

TEST_CASE("dialogs: New Game's default ship and the limited refusal") {
    Tables t;
    // the limited version: TRANSFORMER, no INI write (newgame-0001 with MainShip SKY TALON)
    DefaultShip d = default_ship("SKY TALON", t.limited());
    CHECK(d.name == "TRANSFORMER");
    CHECK_FALSE(d.write_main_ship);
    CHECK(d.record == 1);
    // registered: the INI ship, unless its team is 7 or the rule refuses it
    d = default_ship("SKY TALON", t.full());
    CHECK(d.name == "SKY TALON");
    CHECK(d.record == 2);
    d = default_ship("RENEGADE ONE", t.full());
    CHECK(d.name == "TRANSFORMER");
    CHECK(d.write_main_ship);
    Availability no_cache_x5_0{0, 0, &t.ships, &t.bodies};
    d = default_ship("EAGLE", no_cache_x5_0);  // body 178 refused
    CHECK(d.name == "TRANSFORMER");
    CHECK(d.write_main_ship);
    d = default_ship("NO SUCH SHIP", t.full());  // no record: the name stays, no preview
    CHECK(d.name == "NO SUCH SHIP");
    CHECK(d.record == -1);
    CHECK(limited_ship_refused(1, "EAGLE"));
    CHECK_FALSE(limited_ship_refused(1, "TRANSFORMER"));
    CHECK_FALSE(limited_ship_refused(0, "EAGLE"));
}

TEST_CASE("dialogs: zoom and drive arithmetic") {
    CHECK(zoom_index(1) == 1);  // 1x, dialogs-0001
    CHECK(zoom_index(2) == 0);
    CHECK(zoom_index(0) == 2);
    CHECK(size_for_zoom(0) == 2);
    CHECK(size_for_zoom(1) == 1);
    CHECK(size_for_zoom(2) == 0);
    CHECK(label_text(std::nullopt) == "Unknown_HD");
    CHECK(label_text(std::string()) == "Unknown_HD");
    CHECK(label_text(std::string("DATA")) == "DATA");
    CHECK(label_text(std::string("BACKUPDI.SK1")) == "BACKUPDISK1");  // the '.' at index 8
    CHECK(label_text(std::string("ABCDEFG.H")) == "ABCDEFG.H");      // index 7: kept
    CHECK(label_as_find_first("") == std::nullopt);
    CHECK(label_as_find_first("DATA") == std::optional<std::string>("DATA"));
    CHECK(label_as_find_first("BACKUPDISK1XYZ") == std::optional<std::string>("BACKUPDI.SK1"));
    CHECK(drive_entry('C', std::nullopt) == "C: Unknown_HD");  // dialogs-0001
    CHECK(drive_entry('D', std::string("GAMES")) == "D: GAMES");
    const std::vector<std::string> entries{"C: Unknown_HD", "D: Unknown_HD", "Z: ROOT"};
    CHECK(drive_index(entries, "D") == 1);
    CHECK(drive_index(entries, "Z:\\") == 2);
    CHECK(drive_index(entries, "Q") == 0);
    CHECK(drive_index(entries, "") == 0);
    CHECK(drive_index(entries, "d") == 0);  // compared as stored
    CHECK(drive_text(0, ":", std::nullopt) == "C:Unknown_HD");
    CHECK(drive_text(1, ":", std::string("FIXTURE")) == "D:FIXTURE");
}

TEST_CASE("dialogs: the OK writes in their order") {
    // New Game (newgame-0001: MainShip, Drive, Size, SetSize, ThePlayer, PlayMode)
    auto w = new_game_writes(false, "TRANSFORMER", "D: Unknown_HD", 1, "Your Name", 0);
    CHECK(w == std::vector<IniWrite>{{"MainShip", "TRANSFORMER"},
                                     {"Drive", "D"},
                                     {"Size", "1"},
                                     {"SetSize", "1"},
                                     {"ThePlayer", "Your Name"},
                                     {"PlayMode", "0"}});
    w = new_game_writes(false, "EAGLE", "C: X", 0, "P", 1);
    CHECK(w[2] == IniWrite{"Size", "2"});
    CHECK(w[5] == IniWrite{"PlayMode", "1"});
    // New Demo writes only MainShip and Drive
    w = new_game_writes(true, "EAGLE", "C: X", 0, "P", 1);
    CHECK(w == std::vector<IniWrite>{{"MainShip", "EAGLE"}, {"Drive", "C"}});
    // no DRIVE selection writes an empty Drive
    CHECK(new_game_writes(true, "EAGLE", "", 0, "", 0)[1] == IniWrite{"Drive", ""});
    // Restore Game: Restore, then MainShip and ThePlayer for a save
    CHECK(restore_game_writes(" NEW GAME", -1, "", "") ==
          std::vector<IniWrite>{{"Restore", " NEW GAME"}});
    CHECK(restore_game_writes("ORACLE1", 4, "EAGLE", "ACE") ==
          std::vector<IniWrite>{{"Restore", "ORACLE1"}, {"MainShip", "EAGLE"}, {"ThePlayer", "ACE"}});
    // StratAsk asks only in Command mode with 0 or 10
    CHECK(strat_ask(1, 0));
    CHECK(strat_ask(1, 10));
    CHECK_FALSE(strat_ask(1, 1));
    CHECK_FALSE(strat_ask(1, 11));
    CHECK_FALSE(strat_ask(0, 0));
}

TEST_CASE("dialogs: the exit dialog and the exit steps") {
    CHECK_FALSE(save_exit_enabled(0, 1));  // the title (dialogs-0001: disabled)
    CHECK(save_exit_enabled(1, 1));        // the wave (enabled)
    CHECK(save_exit_enabled(2, 1));
    CHECK(save_exit_enabled(3, 1));
    CHECK_FALSE(save_exit_enabled(4, 1));
    CHECK_FALSE(save_exit_enabled(1, 0));  // a demo
    CHECK(kExitCancel == 0);
    CHECK(kExitExit == 1);
    CHECK(kExitSave == 0x251);
    CHECK(save_exit_argument(2) == -1);
    CHECK(save_exit_argument(1) == 1);
    CHECK(save_exit_argument(0) == 1);
    CHECK(benefits_on_exit(0));
    CHECK(benefits_on_exit(1));
    CHECK_FALSE(benefits_on_exit(-1));
    CHECK_FALSE(benefits_on_exit(2));
    CHECK_FALSE(benefits_on_exit(3));
    // seg10:0x87de
    PlusCountStep s = plus_count_step(0, 0, 13);
    CHECK_FALSE(s.acts);
    s = plus_count_step(1, -1, 13);  // a Plus Edition: nothing
    CHECK_FALSE(s.acts);
    s = plus_count_step(0, -1, 13);  // the default 13 + 1 is above 12
    CHECK(s.acts);
    CHECK(s.count == 0);
    CHECK(s.plus_pack);
    s = plus_count_step(0, 3, 0);
    CHECK(s.acts);
    CHECK(s.count == 1);
    CHECK_FALSE(s.plus_pack);
    s = plus_count_step(0, -1, 11);
    CHECK(s.count == 12);
    CHECK_FALSE(s.plus_pack);
}

TEST_CASE("dialogs: Order's texts and Important Information's rule") {
    CHECK(order_day_text(0).string_id == 2344);
    CHECK_FALSE(order_day_text(0).value.has_value());
    CHECK(order_day_text(12087).string_id == 2341);
    CHECK(order_day_text(12087).value == 12087);
    CHECK(order_day_text(-5).string_id == 2342);
    CHECK(order_day_text(-5).value == 5);
    CHECK(order_x4(0, 12087) == 12087);
    CHECK(order_x4(1, 12087) == 0);
    CHECK(order_tshirt(0x4bf));
    CHECK_FALSE(order_tshirt(0x4c0));
    CHECK(order_phone_hidden(2));
    CHECK_FALSE(order_phone_hidden(3));
    CHECK_FALSE(order_phone_hidden(50));
    CHECK(order_phone_hidden(51));
    CHECK(order_phone_hidden(12087));  // dialogs-0001: the phone line does not show
    // dialogs-0001: "If today is not 10/24/126, ..."
    CHECK(format_ints("If today is not %d/%d/%d, use", {10, 24, 126}) ==
          "If today is not 10/24/126, use");
    CHECK(format_ints("You are on day %d of your demo trial period.", {12087}) ==
          "You are on day 12087 of your demo trial period.");
    CHECK(format_ints("100%%", {}) == "100%");
    // the serial record: count +4, dword +6
    std::vector<std::uint8_t> rec(10, 0);
    CHECK_FALSE(order_serial(rec).has_value());  // the shipdisk's zero record
    rec[6] = 0x32;  // 10197298 = 0x009B9932, the serial of dialogs-0001
    rec[7] = 0x99;
    rec[8] = 0x9b;
    CHECK(order_serial(rec) == std::optional<std::string>("10197298"));
    rec[4] = 9;  // a count above 8
    CHECK_FALSE(order_serial(rec).has_value());
    CHECK_FALSE(order_serial(std::vector<std::uint8_t>(9, 1)).has_value());
    CHECK(needs_important(2, 0));
    CHECK(needs_important(2, 1));  // 35-day codes write Accept = 1
    CHECK_FALSE(needs_important(2, 42));
    CHECK_FALSE(needs_important(0, 0));
    CHECK(restore_refused(0, 1));
    CHECK_FALSE(restore_refused(0, 0));
    CHECK_FALSE(restore_refused(-1, 1));
}

TEST_CASE("dialogs: centring, fonts and resource text") {
    // newgame-0001: New Game's 303 x 222 window at 361,273 on 1024 x 768
    const Placement p = centred(93, 29, 93 + 303, 29 + 222, 1024, 768);
    CHECK(p.x == 361);
    CHECK(p.y == 273);
    CHECK(p.w == 303);
    CHECK(p.h == 222);
    // Restore Game: 321 x 264 at 352,252
    const Placement r = centred(0, 0, 321, 264, 1024, 768);
    CHECK(r.x == 352);
    CHECK(r.y == 252);
    CHECK(bold_child("Button", 0x50010000));  // BS_PUSHBUTTON
    CHECK(bold_child("BUTTON", 0x50010001));  // BS_DEFPUSHBUTTON
    CHECK_FALSE(bold_child("Button", 0x50000007));  // a group box
    CHECK_FALSE(bold_child("Static", 0x50020000));
    CHECK(resource_text({'a', 'b', 0, 'c'}) == "ab");
    CHECK(resource_text({}) == "");
    CHECK(team_key(0) == "Team1");
    CHECK(team_key(7) == "Team8");
}

TEST_CASE("dialogs: golden: the limited New Game list of the shipdisk") {
    const auto dir = original_dir();
    if (dir.empty()) return;  // skips without the original files
    is::assets::AssetStore store(dir);
    const is::assets::Ships s = store.ships();
    // The limited version (command 2 refuses the zero install record): the body rule.
    const Availability limited{0, 1, &s.ships, &s.bodies};
    const auto list = new_game_ships(limited);
    std::vector<std::string> names;
    for (int i : list) names.push_back(s.ships.records[static_cast<std::size_t>(i)].name);
    const auto shown = sorted_like_combo(names);
    // newgame-0001: "drop-down list of 61 ships, alphabetical, AQUATIC BULLET ... WINTER DUCK"
    CHECK(shown.size() == 61);
    REQUIRE_FALSE(shown.empty());
    CHECK(shown.front() == "AQUATIC BULLET");
    CHECK(shown.back() == "WINTER DUCK");
    const DefaultShip d = default_ship("SKY TALON", limited);
    CHECK(d.name == "TRANSFORMER");
    REQUIRE(d.record >= 0);
    CHECK(s.ships.records[static_cast<std::size_t>(d.record)].team == 0);  // AVENGERS
}
