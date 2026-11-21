// The activity text seg3:0x1d0a, the names seg3:0x1542, the casing seg11:0x3fd4 and the lines
// seg3:0x31d0 (src/hud/activity.h; M5 stage 4, track WATCH). The expected texts are the oracle's
// watch lines (ships-0001 g1/watch1.png, ships-0005 b1/c3, win98-0001) and numbers worked from the
// listings.
#include "doctest.h"

#include <map>
#include <string>

#include "hud/activity.h"
#include "world/pilot.h"
#include "world/relations.h"
#include "world/world.h"

using namespace is;
using namespace is::hud;

namespace {

struct AHost final : ActivityHost {
    std::map<int, std::string> s = {
        {21, ""},  // erased below: ENG_LANG.DLL has no string 21
        {152, "Icon ^N"}, {153, "Rock^N"}, {154, "Defender^N"}, {155, "Fuel Tank^N"}, {170, "Virus^N"},
        {169, "Object #"}, {347, "Ambulance^N"}, {363, "Hazard Block^N"}, {585, "Exit Gate^N"}, {788, "Officer"},
        {2700, "THE "}, {2701, "THE "}, {2702, "THE "}, {2703, "THE "}, {2706, "THE "}, {2707, "THE "},
        {2720, "A "}, {2721, "AN "}, {2722, "A "}, {2723, "AN "}, {2726, "A "}, {2727, "AN "},
        {2740, "THIS "}, {2741, "THIS "}, {2742, "THIS "}, {2743, "THIS "}, {2760, "FOR"}, {2762, "FOR "},
        {456, "You."}, {457, "Attacking %s"}, {458, "Because I'm In A Duel."}, {459, "As Requested By %D."},
        {460, "To Help %s."}, {462, "Because I'm Mad At %?."}, {463, "Him."}, {470, "Just Wandering."},
        {474, "Approaching %s"}, {475, "There's Nothing Better To Do."}, {478, "Capturing %s"},
        {481, ".I Need All I Can Get."}, {496, "Since You're So Loaded."}, {501, "And Trying To Repair."},
        {503, ".Target Practice."}, {1082, "You Are "}, {1083, "I'm "}, {1116, ".This Could Take A While."},
        {1128, ".Definitely A Waste Of Time."}, {1707, ".I Warned Him Enough Times."},
        {2218, "I'M STICKING CLOSE TO DEFEND %s."}, {2217, " OF "}, {2539, "To Defend"},
        {3039, "I'm Leaving The Wave, As Requested by %s."}, {3040, "I'm Attacking %s %? %s."},
        {3041, "I'm Orbiting %s While Shooting At %s."}};
    std::uint32_t clock = 0;
    std::vector<int> thinks;
    std::vector<std::string> todos;
    AHost() { s.erase(21); }
    bool load(int id, std::string& buf, int cap) override {
        auto it = s.find(id);
        if (it == s.end()) return false;
        buf = it->second;
        if (cap > 0 && static_cast<int>(buf.size()) > cap - 1) buf.resize(static_cast<std::size_t>(cap - 1));
        return true;
    }
    std::uint32_t now() const override { return clock; }
    void think(int i) override { thinks.push_back(i); }
    void todo(const std::string& w, const char* a) override { todos.push_back(w + " " + a); }
};

// A wave with the player (0), two computer ships (1, 2), rocks, icons and a fuel tank.
struct Wave {
    world::World w;
    AHost h;
    ActivityState st;
    Wave() {
        w.roles.ships = 14;
        w.player = 0;
        w.last_ship = 2;
        w.player_game = 1;
        for (int p : {0, 3, 4, 7, 14}) {
            w.pools.resize(p, 4);
            w.pools.set_in_use(p, 4);
        }
        w.design_names[0] = "Sky Talon";
        w.design_names[1] = "Golden Viper";
        w.design_names[2] = "Nemesis";
        for (int i = 0; i < 3; ++i) {
            w.ship(i).design_index = static_cast<std::int16_t>(i);
            w.ship(i).roster_index = -1;
        }
        w.icons[1].name = "BRAVO.EXE";
        w.icons[2].name = "CHARLIE.EXE";
        w.pools.rec(0, 1).b53 = 1;
        w.pools.rec(0, 2).b53 = 1;
        w.pools.rec(7, 0).state = 1;
    }
    world::Pilot p(int i) { return world::Pilot(w.ship(i)); }
    void goal(int i, int pool, int rec, int act) {
        p(i).goal_pool(static_cast<std::int16_t>(pool));
        p(i).goal_rec(static_cast<std::int16_t>(rec));
        p(i).goal_act(static_cast<std::int16_t>(act));
    }
    void text(int s, int a = 0) { activity_text(w, h, st, a, s, 3); }
};

}  // namespace

TEST_CASE("seg11:0x3fd4 casing modes") {
    CHECK(case_3fd4("Heart of Gold", 0) == "heart of gold");
    CHECK(case_3fd4("Heart of Gold", 1) == "HEART OF GOLD");
    CHECK(case_3fd4("HEART OF GOLD", 2) == "Heart Of Gold");
    CHECK(case_3fd4("fuzzy 19", 2) == "Fuzzy 19");
    // The character after '(' keeps its case; " I " keeps the I.
    CHECK(case_3fd4("ICON 1 (bRAVO.EXE)", 2) == "Icon 1 (bravo.exe)");
    CHECK(case_3fd4("so I said", 0) == "so i said");
    CHECK(case_3fd4("SO I SAID", 3) == "So I said");
    // Mode 3: upper after '.', '!' and the spaces that follow.
    CHECK(case_3fd4("one. two! three", 3) == "One. Two! Three");
    // Mode 1 skips the character after '(' too (the Hall's charge text, review L3).
    CHECK(case_3fd4("officer 2771 (interceptor)", 1) == "OFFICER 2771 (iNTERCEPTOR)");
}

TEST_CASE("seg3:0x1542 names and articles") {
    Wave v;
    // A ship: the design name, mode 5 becomes 1 ("THE "), g = 2 without design bit 0x20.
    CHECK(object_name(v.w, v.h, 1, 14, 2, 5) == "THE NEMESIS");
    CHECK(object_name(v.w, v.h, 0, 14, 1, 3) == "THIS Golden Viper");
    CHECK(object_name(v.w, v.h, 0, 14, 1, 0) == "Golden Viper");
    CHECK(object_name(v.w, v.h, 0, 14, 1, 6) == "FOR Golden Viper");  // 2760 + 0 + 2
    // An icon: "Icon " + the index, and with caps the file name in parentheses; mode 5 no article.
    CHECK(object_name(v.w, v.h, 1, 0, 2, 5) == "ICON 2 (CHARLIE.EXE)");
    CHECK(object_name(v.w, v.h, 0, 0, 1, 0) == "Icon 1");
    CHECK(object_name(v.w, v.h, 1, 0, 1, 2) == "ICON 1 (BRAVO.EXE)");  // mode 2 becomes 5
    // A rock: modes 1 and 5 become 2 ("A "); mode 0 none; the code cut.
    CHECK(object_name(v.w, v.h, 1, 3, 0, 5) == "A ROCK");
    CHECK(object_name(v.w, v.h, 1, 3, 0, 0) == "ROCK");
    // Pool 4 with DGROUP:0x9214 = 0: "Fuel Tank", mode 5 kept.
    CHECK(object_name(v.w, v.h, 1, 4, 0, 5) == "FUEL TANK");
    // The exit gate (pool 12, an even record): mode 5 becomes 1.
    CHECK(object_name(v.w, v.h, 1, 12, 0, 5) == "THE EXIT GATE");
    CHECK(object_name(v.w, v.h, 0, 7, 0, 0) == "Ambulance");
    CHECK(object_name(v.w, v.h, 0, 2, 3, 1) == "A Defender");
    CHECK(object_name(v.w, v.h, 0, 13, 0, 0) == "Object #13");
}

TEST_CASE("seg17:0x8e60 an Enforcer's name") {
    Wave v;
    world::set_ship_sb(v.w.ship(2), world::pw::kClass, 1);
    // (2·2 + 3)·(1 + 2) = 21, ·8 = 168, ·9 = 1512, ·2 = 3024, / 4 = 756, + 956 = 1712.
    CHECK(pilot_name_8e60(v.w, v.h, 2) == "Officer 1712");
}

TEST_CASE("seg3:0x31d0 the dot and the watch lines") {
    ActivityState st;
    lines_31d0(st, 3, "I'm Attacking Rock", ".Target Practice.");
    CHECK(st.w8694 == "I'M ATTACKING ROCK.");
    CHECK(st.w86d0 == "TARGET PRACTICE.");
    lines_31d0(st, 3, "I'm Approaching Fuel Tank", "");
    CHECK(st.w8694 == "I'M APPROACHING FUEL TANK.");
    lines_31d0(st, 1, "I'm Here!", "Now");
    CHECK(st.w8630 == "I'm Here! Now");
}

TEST_CASE("seg3:0x1d0a the oracle's watch lines") {
    Wave v;
    // ships-0001: "I'M APPROACHING ROCK / THERE'S NOTHING BETTER TO DO."
    v.goal(1, 3, 2, 3);
    v.text(1);
    CHECK(v.st.w8694 == "I'M APPROACHING ROCK");
    CHECK(v.st.w86d0 == "THERE'S NOTHING BETTER TO DO.");
    // ships-0005: "I'M ATTACKING ROCK." / "TARGET PRACTICE."
    v.goal(2, 3, 1, 1);
    v.text(2);
    CHECK(v.st.w8694 == "I'M ATTACKING ROCK.");
    CHECK(v.st.w86d0 == "TARGET PRACTICE.");
    // ships-0005 c3: "I'M APPROACHING FUEL TANK" and "I'M APPROACHING ICON 2 (CHARLIE.EXE)"
    v.goal(2, 4, 0, 3);
    v.text(2);
    CHECK(v.st.w8694 == "I'M APPROACHING FUEL TANK");
    v.goal(2, 0, 2, 3);
    v.text(2);
    CHECK(v.st.w8694 == "I'M APPROACHING ICON 2 (CHARLIE.EXE)");
    // ships-0005 c3: "CAPTURING ICON 2 (CHARLIE.EXE). / I NEED ALL I CAN GET."
    v.goal(2, 0, 2, 2);
    v.text(2);
    CHECK(v.st.w8694 == "I'M CAPTURING ICON 2 (CHARLIE.EXE).");
    CHECK(v.st.w86d0 == "I NEED ALL I CAN GET.");
    // ships-0005 b1: "I'M ATTACKING ICON 1 (BRAVO.EXE) AS REQUESTED BY SKY TALON."; the %D code is
    // a stack offset, so no article.
    v.goal(1, 0, 1, 1);
    v.p(1).order(2);
    v.p(1).order_by(0);
    v.text(1);
    CHECK(v.st.w8694 == "I'M ATTACKING ICON 1 (BRAVO.EXE)");
    CHECK(v.st.w86d0 == "AS REQUESTED BY SKY TALON.");
    // ships-0005 b1: "I'M ATTACKING YOU." / "SINCE YOU'RE SO LOADED." (reaction 0x0d at the viewer)
    v.p(1).order(0);
    v.goal(1, 14, 0, 1);
    world::set_ship_b(v.w.ship(1), world::pw::kReaction, 0x0d);
    v.text(1);
    CHECK(v.st.w8694 == "I'M ATTACKING YOU.");
    CHECK(v.st.w86d0 == "SINCE YOU'RE SO LOADED.");
    // win98-0001: "I'M ATTACKING THE NEMESIS. I WARNED HIM ENOUGH TIMES." (its own order, 0x14)
    world::set_ship_b(v.w.ship(1), world::pw::kReaction, 0x14);
    v.goal(1, 14, 2, 1);
    v.p(1).order(3);
    v.p(1).order_by(1);
    v.text(1);
    CHECK(v.st.w8694 == "I'M ATTACKING THE NEMESIS.");
    CHECK(v.st.w86d0 == "I WARNED HIM ENOUGH TIMES.");
    // ships-0001 w-19: "I'M ATTACKING A HAZARD BLOCK. / THIS COULD TAKE A WHILE." (pool 8 kind 1)
    v.p(1).order(0);
    world::set_ship_b(v.w.ship(1), world::pw::kReaction, 0);
    v.w.pool8[3].at(0x1c) = 1;
    v.goal(1, 8, 3, 1);
    v.text(1);
    CHECK(v.st.w8694 == "I'M ATTACKING A HAZARD BLOCK.");
    CHECK(v.st.w86d0 == "THIS COULD TAKE A WHILE.");
    // The player watching itself in a player game: "You Are " and no line 2.
    v.goal(0, 3, 0, 3);
    v.text(0, 100);
    CHECK(v.st.w8694 == "YOU ARE APPROACHING ROCK.");
    CHECK(v.st.w86d0.empty());
}

TEST_CASE("seg3:0x1d0a LoadString(21): the 501 text stays") {
    Wave v;
    v.goal(1, 3, 0, 3);
    v.p(1).order(7);
    v.p(1).goal_app(0);
    v.w.ship(1).w26 = 40;  // Engine < 50
    v.text(1);
    CHECK(v.st.w8694 == "I'M JUST WANDERING.");
    CHECK(v.st.w86d0 == "AND TRYING TO REPAIR.");
}

TEST_CASE("seg3:0x1d0a the demo's give-up") {
    Wave v;
    v.w.player_game = 0;
    v.goal(1, 14, 2, 1);
    v.p(1).goal_since(1000);
    world::set_ship_w(v.w.ship(1), world::pw::kAttitude0 + 4, -41);
    v.h.clock = 1000 + 19001;  // +0xa4 + 19000 < now
    v.text(1);
    CHECK(v.st.w86d0 == "DEFINITELY A WASTE OF TIME.");
    CHECK(v.st.w8694 == "I'M ATTACKING THE NEMESIS.");
    CHECK(v.p(1).order() == -1);
    CHECK(v.p(1).replan_at() == 0);
    CHECK(world::ship_w(v.w.ship(1), world::pw::kAttitude0 + 4) == -20);  // cwd; idiv 2
    CHECK(world::ship_b(v.w.ship(1), world::pw::kGrudge0 + 2) == 0xff);
    REQUIRE(v.h.thinks.size() == 1);
    CHECK(v.h.thinks[0] == 1);
    // Between 14000 and 19000 ms: 1127 and nothing changes.
    v.h.thinks.clear();
    v.p(1).order(0);
    v.h.clock = 1000 + 15000;
    v.h.s[1127] = "But Its Starting To Seem Futile.";
    v.text(1);
    CHECK(v.st.w86d0 == "BUT ITS STARTING TO SEEM FUTILE.");
    CHECK(v.h.thinks.empty());
}

TEST_CASE("seg3:0x304c a defender's text") {
    Wave v;
    ActivityState st;
    DefenderWords d;
    d.owner_pool = 14;
    d.owner_rec = 2;
    d.target_pool = 14;
    d.target_rec = 0;
    d.state = 10;
    defender_text(v.w, v.h, st, 0, d, 3);
    CHECK(st.w8d38 == "I'm Orbiting You. While Shooting At THE Nemesis.");
    d.state = 0;
    defender_text(v.w, v.h, st, 0, d, 3);
    CHECK(st.w8d38 == "I'M STICKING CLOSE TO DEFEND THE Nemesis.");
    CHECK(st.w8694 == "I'M STICKING CLOSE TO ");  // seg3:0x3498 keeps the space in line 1
    CHECK(st.w86d0 == "DEFEND THE NEMESIS.");
}
