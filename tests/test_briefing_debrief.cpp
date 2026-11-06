// State 2's page text seg9:0x6fee, its box seg9:0x6dec, the formatter seg10:0xa332 and the GAME
// OVER lines seg9:0x6a12 (src/briefing/debriefing.h, lang_format.h), against the three
// debriefings of oracle/digests/core-0001.md (M4 s1 END).
#include "doctest.h"

#include <map>
#include <string>

#include "briefing/debriefing.h"
#include "briefing/lang_format.h"

using namespace is::briefing;

namespace {

// The ENG_LANG.DLL strings the page and the screens use (work/assets/eng_lang.dll/strings.tsv).
const std::map<int, std::string>& strings() {
    static const std::map<int, std::string> m = {
        {318, "DIRECTORY"}, {545, ": "}, {629, " WOULD LIKE TO MEET YOU IN DODGE CITY."},
        {789, "RELATIONS WITH OTHER TEAMS HAVE NOT CHANGED."}, {797, " AND"},
        {870, "YOU PLAYED FOR"}, {879, "WAS KILLED BY "}, {885, "YOU "}, {1997, "YOU FOUGHT FOR"},
        {2146, "DUEL"}, {2238, "PRESS 'ESC' TO CONTINUE"}, {2239, "RACE TRACK"},
        {2242, "WHEW! THAT WAS A MONSTER DIRECTORY! "}, {2243, "THAT WAS A BIG DIRECTORY! "},
        {2506, "ICON WAS"}, {2507, "ICON HAS BEEN"}, {2508, "THERE IS"}, {2509, "THERE ARE"},
        {2510, "MINUTE"}, {2511, "SECOND"}, {2515, "ICON"}, {12506, "ICONS WERE"},
        {12507, "ICONS HAVE BEEN"}, {12510, "MINUTES"}, {12511, "SECONDS"}, {12515, "ICONS"},
        {325, "GAME OVER"}, {2532, "LAST WAVE: %d"}, {2533, "ICONS:"}, {2534, "CAPTURED: %d"},
        {2535, "DESTROYED: %d"}, {2536, "SCORE: %l"},
        {3003, "WAVE %d MISSION DEBRIEFING"}, {3004, "%<SCORE: %l%N%>%N"},
        {3005, "YOU HAVE FINISHED %s, %s! %<%?%>"},
        {3006, "%<OF ITS %#, %>%# CAPTURED AND %# DESTROYED. "},
        {3007, "THUS FAR, %# CAPTURED AND %# DESTROYED. %? %# REMAINING. "},
        {3010, "%? %<%#, %>%#"},
        {3012, ", %<CAPTURING %#%<DESTROYING %#%>%<BUT HAVE NO ICONS TO SHOW FOR IT%>. "},
        {3014, "%<YOUR TEAMMATE, %>%s%<,%> "},
    };
    return m;
}
std::string lang(int id) {
    auto it = strings().find(id);
    return it == strings().end() ? std::string() : it->second;  // 2311-2313 are missing
}

DebriefInputs base() {
    DebriefInputs in;
    in.directory = "D:\\GAMES\\";
    in.pilot = "Your Name";
    in.icons = 3;
    DebriefShip p;  // the player alone, alive
    in.ships.push_back(p);
    return in;
}

}  // namespace

TEST_CASE("seg10:0xa332: %< groups join with \", \" and \" AND\", %# and %? pick their ids") {
    CHECK(lang_format(lang(3012), {1, 1, 2515, 0, 0, 2515, 0}, lang) == ", CAPTURING 1 ICON. ");
    CHECK(lang_format(lang(3012), {1, 1, 2515, 2, 2, 2515, 0}, lang) ==
          ", CAPTURING 1 ICON AND DESTROYING 2 ICONS. ");
    CHECK(lang_format(lang(3012), {0, 0, 2515, 2, 2, 2515, 0}, lang) == ", DESTROYING 2 ICONS. ");
    CHECK(lang_format(lang(3012), {0, 0, 2515, 0, 0, 2515, 1}, lang) ==
          ", BUT HAVE NO ICONS TO SHOW FOR IT. ");
    CHECK(lang_format(lang(3010), {0, 870, 1997, 1, 7, 2510, 41, 2511}, lang) ==
          "YOU PLAYED FOR 7 MINUTES, 41 SECONDS");
    CHECK(lang_format(lang(3010), {0, 870, 1997, 0, 0, 2510, 3, 2511}, lang) == "YOU PLAYED FOR 3 SECONDS");
    CHECK(lang_format(lang(3004), {1, 4600L}, lang) == "SCORE: 4600\r\n\r\n");
    CHECK(lang_format(lang(3004), {0, 4600L}, lang) == "\r\n");
    // %N writes CR LF in briefing phase 11 too: the test needs a byte that holds the
    // directive letter (shell.md Formatter, Other directives; C076) [BYTES seg10:0xa6ec].
    CHECK(lang_format(lang(3004), {1, 4600L}, lang, 0xb) == "SCORE: 4600\r\n\r\n");
    // %S cuts a trailing caret pair at buf[len - 2], not a lone trailing caret (C076).
    const LangLookup carets = [](int id) {
        return id == 1 ? std::string("Icon ^N") : id == 2 ? std::string("A^") : std::string("%S|%A|%V");
    };
    CHECK(lang_format("%S|%A|%V", {1, 2, 1}, carets) == "Icon |A^|Icon ");
    CHECK(lang_format("%%^Pa%z", {}, lang) == "%a");
}

TEST_CASE("seg9:0x6fee: core-0001 hit, wave 1, the directory finished") {
    DebriefInputs in = base();
    in.score = 4600;
    in.captured = 1;
    in.destroyed = 2;
    in.dir_state = 2;
    in.seconds = 7 * 60 + 41;
    in.cap = 1;
    const std::string t = debrief_text(in, lang, 18, {});
    CHECK(t ==
          "DIRECTORY: D:\\GAMES\\ \r\n"
          "SCORE: 4600\r\n\r\n"
          "YOU HAVE FINISHED D:\\GAMES\\, Your Name! OF ITS 3 ICONS, 1 ICON WAS CAPTURED AND 2 ICONS "
          "WERE DESTROYED.  \r\n \r\n"
          "YOU PLAYED FOR 7 MINUTES, 41 SECONDS, CAPTURING 1 ICON.  \r\n \r\n"
          "RELATIONS WITH OTHER TEAMS HAVE NOT CHANGED.");
}

TEST_CASE("seg9:0x6fee: core-0001 death, wave 1, two icons left") {
    DebriefInputs in = base();
    in.score = 4000;
    in.captured = 1;
    in.destroyed = 0;
    in.seconds = 80;
    in.cap = 1;
    const std::string t = debrief_text(in, lang, 18, {});
    CHECK(t ==
          "DIRECTORY: D:\\GAMES\\ \r\n"
          "SCORE: 4000\r\n\r\n"
          "THUS FAR, 1 ICON HAS BEEN CAPTURED AND 0 ICONS HAVE BEEN DESTROYED. THERE ARE 2 ICONS "
          "REMAINING.  \r\n \r\n"
          "YOU PLAYED FOR 1 MINUTE, 20 SECONDS, CAPTURING 1 ICON.  \r\n \r\n"
          "RELATIONS WITH OTHER TEAMS HAVE NOT CHANGED.");
}

TEST_CASE("seg9:0x6fee: core-0001 death, wave 2, three seconds and nothing to show") {
    DebriefInputs in = base();
    in.score = 4000;
    in.captured = 1;
    in.seconds = 3;
    const std::string t = debrief_text(in, lang, 18, {});
    CHECK(t.find("YOU PLAYED FOR 3 SECONDS, BUT HAVE NO ICONS TO SHOW FOR IT.  \r\n") != std::string::npos);
    CHECK(t.find("THERE ARE 2 ICONS REMAINING.") != std::string::npos);
}

TEST_CASE("seg9:0x6fee: one ICON REMAINING, a dead ship and its one rand()") {
    DebriefInputs in = base();
    in.captured = 1;
    in.destroyed = 1;
    DebriefShip dead;
    dead.alive = false;
    dead.name = "ZAPPER";
    dead.killer_pool = 14;
    dead.killer_rec = 1;  // itself
    in.ships.push_back(dead);
    int draws = 0;
    const std::string t = debrief_text(in, lang, 18, [&] { return ++draws, 7; });
    CHECK(t.find("THERE IS 1 ICON REMAINING. ") != std::string::npos);
    // 3014, then rand() % 3 + 2311 (missing) and ". " [BYTES seg9:0x81c4].
    CHECK(t.find("ZAPPER . ") != std::string::npos);
    CHECK(draws == 1);
    in.ships[1].killer_rec = 0;  // the player
    CHECK(debrief_text(in, lang, 18, {}).find("ZAPPER WAS KILLED BY YOU . ") != std::string::npos);
}

TEST_CASE("seg9:0x6dec: the box at 1024 x 768") {
    const DebriefBox b = debrief_box(1024, 768, 1, lang);
    CHECK(b.L == 222);
    CHECK(b.T == 211);
    CHECK(b.R == 802);
    CHECK(b.B == 557);
    // The frame bevel (216, 196)-(808, 563); the oracle's box is 216,196-808,564 [ORACLE core-0001].
    CHECK(b.frame.width == 2);
    CHECK(b.frame.l == 216);
    CHECK(b.frame.t == 196);
    CHECK(b.frame.r == 808);
    CHECK(b.frame.b == 563);
    CHECK(b.title_bar.width == -1);
    CHECK(b.title_bar.b == 221);
    CHECK(b.title.text == "WAVE 1 MISSION DEBRIEFING");
    CHECK(b.title.x == 221);
    CHECK(b.title.y == 198);
    CHECK(b.esc.text == "PRESS 'ESC' TO CONTINUE");
    CHECK(b.esc.x == 801);
    CHECK(b.esc.y == 205);
    CHECK(b.esc.align == kTaRight);
    CHECK(b.ox == 236);
    CHECK(b.oy == 229);
    CHECK(b.clip_t == 229);
    // The clamps of a narrow client.
    const DebriefBox n = debrief_box(580, 400, 2, lang);
    CHECK(n.cL == 0);
    CHECK(n.cR == 580);
    CHECK(n.frame.l == -6);
}

TEST_CASE("seg9:0x6a12: the GAME OVER lines of core-0001 at 1024 x 768") {
    const auto v = game_over_lines(1024, 768, 3, 1, 0, 4900, true, lang);
    REQUIRE(v.size() == 6);
    CHECK(v[0].text == "GAME OVER");
    CHECK(v[0].x == 512);
    CHECK(v[0].y == 209);  // the trace's StretchBlt row 0xd1
    CHECK(v[0].colour == 0x020080ffu);
    CHECK(v[1].text == "LAST WAVE: 3");
    CHECK(v[1].y == 269);  // 0x10d
    CHECK(v[3].text == "CAPTURED: 1");
    CHECK(v[3].align == 2);
    CHECK(v[4].text == "DESTROYED: 0");
    CHECK(v[5].text == "SCORE: 4900");
    CHECK(v[5].colour == 0x000000ffu);
    CHECK(game_over_lines(1024, 768, 3, 1, 0, 4900, false, lang).empty());
}
