// The briefing's pages for the ships other than the player's (src/briefing/briefing.h
// other_ship_text, other_ship_labels, other_ship_line, ship_page_stats; M5 stage 4, track WATCH),
// against seg17:0x33ee-0x3745 and the pages of ships-0005 br1 (the teammate GOLDEN VIPER, the
// PLATYPUS) and br2 (the COLD HEART), read from the shots brief24 and brief34.
#include "doctest.h"

#include <map>
#include <string>

#include "briefing/briefing.h"
#include "world/relations.h"
#include "world/world.h"

using namespace is;
using namespace is::briefing;

namespace {

struct Lang final : Strings {
    std::map<int, std::string> s = {
        {921, "THIS IS YOUR FIRST ENCOUNTER WITH %D."}, {922, " TWICE"}, {923, " ONCE BEFORE"},
        {3034, "YOU HAVE MET %D %<%?%>%<%d TIMES%>."}, {2046, "YOU INTERACTED WITH HIM IN THE PAST"},
        {790, "DOESN'T LIKE"}, {791, "IS NEUTRAL TO"}, {792, "LIKES"}, {793, "IS ON"},
        {794, "HE %S YOUR TEAM %? HE %S YOU.%N%N"}, {795, "AND"}, {2048, "BUT"},
        {3023, "%<YOUR%>%<THIS%> SHIP IS %S."}, {720, "A PUSHOVER"}, {2521, "%S SHIP"},
        {2522, "(%<this is you%>%<from %? team%>)"}, {581, "HOSTILE"}, {582, "NEUTRAL"}, {583, "FRIENDLY"},
        {527, "another"}, {528, "your"}, {327, "THIS TEAMMATE"}, {310, " IS "},
        {2740, "THIS "}, {2741, "THIS "}, {2742, "THIS "}, {2743, "THIS "}};
    std::string lang(int id) override {
        auto it = s.find(id);
        return it == s.end() ? std::string() : it->second;
    }
};

ShipStats ship(const char* name, int team, int met, int team_view, int view) {
    ShipStats st;
    st.is_player = false;
    st.team = team;
    st.player_team = 0;
    st.roster = true;
    st.met = met;
    st.team_view = team_view;
    st.view = view;
    st.this_name = std::string("THIS ") + name;
    return st;
}

}  // namespace

TEST_CASE("seg17:0x33ee the other ships' text, as the oracle's pages read") {
    Lang l;
    // br1 brief24: the teammate met once before (roster byte +0x77 = 2); the double space and the
    // missing space after "BEFORE." are the strings' [ORACLE ships-0005].
    CHECK(other_ship_text(l, ship("GOLDEN VIPER", 0, 2, 0, 100), 720) ==
          "YOU HAVE MET THIS GOLDEN VIPER  ONCE BEFORE.YOU INTERACTED WITH HIM IN THE PAST. "
          "HE IS ON YOUR TEAM AND HE LIKES YOU.\r\n\r\nTHIS SHIP IS A PUSHOVER.");
    // br1: the Platypus of another team that likes the player's team.
    CHECK(other_ship_text(l, ship("PLATYPUS", 2, 2, 90, 100), 720) ==
          "YOU HAVE MET THIS PLATYPUS  ONCE BEFORE.YOU INTERACTED WITH HIM IN THE PAST. "
          "HE LIKES YOUR TEAM AND HE LIKES YOU.\r\n\r\nTHIS SHIP IS A PUSHOVER.");
    // br2 brief34: the first encounter, "AND" for two dislikes.
    CHECK(other_ship_text(l, ship("COLD HEART", 3, 1, -200, -200), 720) ==
          "THIS IS YOUR FIRST ENCOUNTER WITH THIS COLD HEART.HE DOESN'T LIKE YOUR TEAM AND HE DOESN'T LIKE "
          "YOU.\r\n\r\nTHIS SHIP IS A PUSHOVER.");
    // The counts: 3 is " TWICE", 5 is "4 TIMES".
    CHECK(other_ship_text(l, ship("X", 2, 3, 0, 0), 720).rfind("YOU HAVE MET THIS X  TWICE.", 0) == 0);
    CHECK(other_ship_text(l, ship("X", 2, 5, 0, 0), 720).rfind("YOU HAVE MET THIS X 4 TIMES.", 0) == 0);
    // An Enforcer (team 7) and a ship without a roster record take 921 whatever the count.
    CHECK(other_ship_text(l, ship("X", 7, 5, 0, 0), 720).rfind("THIS IS YOUR FIRST", 0) == 0);
    ShipStats nr = ship("X", 2, 5, 0, 0);
    nr.roster = false;
    CHECK(other_ship_text(l, nr, 720).rfind("THIS IS YOUR FIRST", 0) == 0);
    // "BUT" when the words differ: the team likes, the player's view is neutral (79 < 80).
    CHECK(other_ship_text(l, ship("X", 2, 0, 80, 79), 720).find("HE LIKES YOUR TEAM BUT HE IS NEUTRAL TO YOU.") !=
          std::string::npos);
    // Same team and "LIKES" count as the same word.
    CHECK(other_ship_text(l, ship("X", 0, 0, -300, 80), 720).find("HE IS ON YOUR TEAM AND HE LIKES YOU.") !=
          std::string::npos);
}

TEST_CASE("seg17:0x4354 and seg17:0x3ba6 the other ships' labels and lower-left line") {
    Lang l;
    const LabelLines a = other_ship_labels(l, ship("GOLDEN VIPER", 0, 2, 0, 100));
    CHECK(a.line1 == "FRIENDLY SHIP");
    CHECK(a.line2 == "(from your team)");
    CHECK(a.colour == 0x02FFAA00);
    const LabelLines b = other_ship_labels(l, ship("COLD HEART", 3, 1, -200, -200));
    CHECK(b.line1 == "HOSTILE SHIP");
    CHECK(b.line2 == "(from another team)");
    CHECK(b.colour == 0x020080FF);
    CHECK(other_ship_labels(l, ship("X", 2, 0, 0, 79)).line1 == "NEUTRAL SHIP");
    CHECK(other_ship_labels(l, ship("X", 2, 0, 0, 79)).colour == 0x02D2D2D2);
    CHECK(other_ship_line(l, ship("GOLDEN VIPER", 0, 2, 0, 100)) == "THIS TEAMMATE IS ");
    CHECK(other_ship_line(l, ship("COLD HEART", 3, 1, -200, -200)) == "THIS COLD HEART IS ");
}

TEST_CASE("ship_page_stats reads a ship's page words from the World") {
    world::World w;
    w.roles.ships = 14;
    w.player = 0;
    w.last_ship = 1;
    w.pools.resize(14, 2);
    w.pools.set_in_use(14, 2);
    w.design_names[1] = "Golden Viper";
    w.ship(0).design_index = 0;
    w.ship(1).design_index = 1;
    w.ship(1).roster_index = 0;
    w.roster.resize(1);
    w.roster[0].met = 2;
    w.roster[0].size = 40;
    w.pools.rec(14, 1).size = 33;
    w.pools.rec(14, 1).gap = 5;
    Lang l;
    ShipStats st;
    REQUIRE(ship_page_stats(w, 1, st, [&l](int id) { return l.lang(id); }));
    CHECK_FALSE(st.is_player);
    CHECK(st.index == 1);
    CHECK(st.roster);
    CHECK(st.met == 2);
    CHECK(st.size == 40);
    CHECK(st.cell == 33);
    CHECK(st.gap == 5);
    CHECK(st.this_name == "THIS Golden Viper");
    CHECK(st.view == world::relation(w, 0, 1));
    CHECK_FALSE(ship_page_stats(w, 5, st, {}));
}
