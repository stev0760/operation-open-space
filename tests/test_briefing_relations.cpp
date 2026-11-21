// The debriefing's relations paragraph seg9:0x8455-0x87d9 and its wave-start snapshot DGROUP:0x9350
// (src/briefing/debriefing.h relations_paragraph, snapshot_relations, debrief_relations), against
// rules worked from the listing (s4-hall.md section 4 with s4-hall-verify.md claims 53-59) and the
// oracle's "THE ENFORCERS HAVE BECOME LESS FRIENDLY." (ships-0003 k1 d-0).
#include <doctest.h>

#include <map>
#include <string>

#include "briefing/debriefing.h"
#include "world/relations.h"
#include "world/roster_build.h"
#include "world/world.h"

using namespace is;
using namespace is::briefing;

namespace {

std::string lang(int id) {
    static const std::map<int, std::string> m = {
        {248, "FRIENDLY"}, {789, "RELATIONS WITH OTHER TEAMS HAVE NOT CHANGED."}, {795, "AND"},
        {798, " HAVE BECOME "}, {799, "MORE "}, {878, "LESS "}, {884, " VENGEANCE LATER"},
        {917, " WILL SEEK"}, {918, " MAJOR"}, {919, " SUPREME"}, {2514, " THE "},
    };
    auto it = m.find(id);
    return it == m.end() ? std::string() : it->second;
}

DebriefRelations base() {
    DebriefRelations r;
    r.tp = 1;
    r.present = {false, true, false, true, false, false, false, true};
    r.names = {"T0", "FUZZY ONES", "T2", "RENEGADES", "T4", "T5", "T6", "ENFORCERS"};
    return r;
}

}  // namespace

TEST_CASE("relations: no change gives 789") {
    CHECK(relations_paragraph(base(), lang) == "RELATIONS WITH OTHER TEAMS HAVE NOT CHANGED.");
    CHECK(relations_paragraph(DebriefRelations{}, lang) == "RELATIONS WITH OTHER TEAMS HAVE NOT CHANGED.");
}

TEST_CASE("relations: ships-0003 k1, the Enforcers less friendly") {
    DebriefRelations r = base();
    r.now[7] = -40;
    r.start[7] = -20;
    CHECK(relations_paragraph(r, lang) == " THE ENFORCERS HAVE BECOME LESS FRIENDLY. ");
}

TEST_CASE("relations: two teams, one verb or two") {
    DebriefRelations r = base();
    r.now[3] = -30;
    r.now[7] = -10;
    // Pass 0 takes 3 (|-30| > 0), not 7 (|-10| < |-30|); pass 1 takes 7: both d < 0, one verb.
    CHECK(relations_paragraph(r, lang) == " THE RENEGADES AND THE ENFORCERS HAVE BECOME LESS FRIENDLY. ");
    r.now[7] = 10;  // opposite signs: each its own verb
    CHECK(relations_paragraph(r, lang) ==
          " THE RENEGADES HAVE BECOME LESS FRIENDLY AND THE ENFORCERS HAVE BECOME MORE FRIENDLY. ");
}

TEST_CASE("relations: vengeance and its levels") {
    DebriefRelations r = base();
    r.v[3] = 1;
    CHECK(relations_paragraph(r, lang) == " THE RENEGADES WILL SEEK VENGEANCE LATER. ");
    r.v[3] = 2;
    CHECK(relations_paragraph(r, lang) == " THE RENEGADES WILL SEEK MAJOR VENGEANCE LATER. ");
    r.v[3] = 3;
    CHECK(relations_paragraph(r, lang) == " THE RENEGADES WILL SEEK SUPREME VENGEANCE LATER. ");
    // A d = 0 team with v = 0 is not taken; a later v != 0 team replaces the slot's take.
    r = base();
    r.now[3] = -500;
    r.v[7] = 1;
    // pass 0: 3 (|-500| > 0), then 7 (v != 0) replaces it; pass 1: 3 again from 0 (v0 = 1, v1 = 0).
    CHECK(relations_paragraph(r, lang) ==
          " THE ENFORCERS WILL SEEK VENGEANCE LATER AND THE RENEGADES HAVE BECOME LESS FRIENDLY. ");
}

TEST_CASE("relations: the player's team and absent teams are skipped") {
    DebriefRelations r = base();
    r.now[1] = -100;  // the player's own team
    r.now[2] = -100;  // not present
    CHECK(relations_paragraph(r, lang) == "RELATIONS WITH OTHER TEAMS HAVE NOT CHANGED.");
}

TEST_CASE("relations: the snapshot and the inputs from the World") {
    world::World w;
    w.player = 0;
    w.last_ship = 2;
    w.wave = 4;
    w.player_roster = 0;
    w.ships[0].design_index = 0;
    w.ships[1].design_index = 1;
    w.ships[2].design_index = 2;
    w.design_ratings[0].team = 1;
    w.design_ratings[1].team = 7;
    w.design_ratings[2].team = 3;
    auto& t = world::roster_words().team_table;
    t.fill(0);
    t[8 * 7 + 1] = -20;  // T(7, 1)
    t[8 * 3 + 1] = 15;   // T(3, 1)
    snapshot_relations(w);
    CHECK(relations_snapshot().w9350[7] == -20);
    CHECK(relations_snapshot().w9350[3] == 15);
    t[8 * 7 + 1] = -60;  // the wave lowers the Enforcers' word
    w.w4eec[3 * 3 + 1] = 0;  // the Renegades remember the player in this wave
    world::contact_words().w4fd0[3 * 3 + 1] = 4;
    const DebriefRelations r = debrief_relations(w, [](int team) { return "TEAM" + std::to_string(team); });
    CHECK(r.tp == 1);
    CHECK(r.present[7]);
    CHECK(r.present[3]);
    CHECK_FALSE(r.present[4]);
    CHECK(r.now[7] == -60);
    CHECK(r.start[7] == -20);
    CHECK(r.v[3] == 1);
    CHECK(r.v[7] == 0);
    CHECK(r.names[7] == "TEAM7");
    world::contact_words().w4fd0[3 * 3 + 1] = 0;
    t.fill(0);
}
