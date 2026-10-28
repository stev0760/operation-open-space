// Enter of the Navigation window, seg6:0x301a (src/nav/enter.h), against docs/re/nav.md "Enter",
// "Values handed to the wave set-up" and "Race and duel boxes" on the in-memory fixture `basic`.
#include <filesystem>

#include "doctest.h"
#include "nav/enter.h"
#include "nav_fixture.h"
#include "settings/settings.h"

using namespace is::nav;
using navtest::lines;

namespace {

struct World {
    MemFs fs;
    navtest::FakeStrings s;
    Records records;
    std::vector<FsCall> calls;
    Walker w{fs};
    EnterState st;
    is::settings::IniFile ini{std::filesystem::temp_directory_path() / "is_test_nav_enter_none.ini"};
    int rand_value = 0;
    int rand_calls = 0;
    World() {
        navtest::build_basic(fs);
        w.record(&calls);
    }
    EnterResult enter(std::string& dir, const EnterInputs& in) {
        return is::nav::enter(st, dir, in, w, s, records, &ini, [this] {
            ++rand_calls;
            return rand_value;
        });
    }
    EnterResult enter(const std::string& path, const EnterInputs& in) {
        std::string d = path;
        return enter(d, in);
    }
};

EnterInputs player(int edition) {
    EnterInputs in;
    in.game = Game{edition, true, 0};
    return in;
}
EnterInputs demo() {
    EnterInputs in;
    in.game = Game{0, false, 0};
    return in;
}

}  // namespace

TEST_CASE("nav enter: returns 0 at once while unarmed or in state 1") {
    World x;
    EnterInputs in = player(-1);
    in.armed = false;
    std::string d = "D:\\GAMES\\";
    EnterResult r = x.enter(d, in);
    CHECK_FALSE(r.accepted);
    CHECK(x.calls.empty());
    CHECK(x.st.handover.directory.empty());  // DGROUP:0x38c8 untouched
    in.armed = true;
    in.state = 1;
    r = x.enter(d, in);
    CHECK_FALSE(r.accepted);
    CHECK(x.calls.empty());
}

TEST_CASE("nav enter: an ordinary directory needs a *.exe and a state below 2") {
    World x;
    std::string d = "D:\\GAMES\\";
    EnterResult r = x.enter(d, player(0));
    REQUIRE(r.accepted);
    CHECK(r.action == EnterAction::kRequestWave);
    CHECK(r.handover.directory == "D:\\GAMES\\");
    CHECK(r.handover.demons_gate == 0);
    CHECK(r.handover.race == 0);
    CHECK(r.handover.duel == 0);
    CHECK(r.ini_writes.empty());
    CHECK(r.added);
    CHECK(r.record == 0);
    CHECK(d == "D:\\GAMES\\");
    REQUIRE(x.records.list.size() == 1);
    CHECK(x.records.list[0].path == "D:\\GAMES\\");
    // The acceptance probe: one find-first of DGROUP:0x38c8 + "*.exe", attribute 0.
    REQUIRE(x.calls.size() == 1);
    CHECK(x.calls[0].routine == "seg6:0x301a via seg6:0x4190");
    CHECK(x.calls[0].path == "D:\\GAMES\\*.exe");
    CHECK(x.calls[0].arg == 0);
    CHECK(x.calls[0].ok);

    // A visited directory (state 1) can be entered again; the record is found.
    r = x.enter("D:\\GAMES\\", player(0));
    CHECK(r.accepted);
    CHECK_FALSE(r.added);
    CHECK(r.record == 0);
    CHECK(x.records.list.size() == 1);

    // A complete one (state 2) cannot.
    x.records.list[0].state = 2;
    CHECK_FALSE(x.enter("D:\\GAMES\\", player(0)).accepted);

    // No *.exe: refused after the probe; DGROUP:0x38c8 still took the path.
    x.calls.clear();
    r = x.enter("D:\\EMPTY\\", player(0));
    CHECK_FALSE(r.accepted);
    CHECK(x.st.handover.directory == "D:\\EMPTY\\");
    CHECK(lines(x.calls) == std::vector<std::string>{
                                "seg6:0x301a via seg6:0x4190 findfirst \"D:\\EMPTY\\*.exe\" 0x00 -> end"});
}

TEST_CASE("nav enter: the state request or the debriefing deadline") {
    World x;
    EnterInputs in = player(0);
    in.wave = 3;
    CHECK(x.enter("D:\\GAMES\\", in).action == EnterAction::kArmDebriefing);
    in.restored = 1;
    CHECK(x.enter("D:\\TOOLS\\", in).action == EnterAction::kRequestWave);
}

TEST_CASE("nav enter: Demon's Gate") {
    World x;
    // A registered game: accepted, DGROUP:0x92e8 = 1 and the name replaces both paths.
    std::string d = "D:\\DEMON'S GATE\\";
    EnterResult r = x.enter(d, player(-1));
    REQUIRE(r.accepted);
    CHECK(r.handover.demons_gate == 1);
    CHECK(r.handover.directory == "DEMON'S GATE");
    CHECK(d == "DEMON'S GATE");
    CHECK(x.records.list.back().path == "DEMON'S GATE");
    CHECK(x.calls.empty());  // no probe

    // The unregistered edition locks it; DGROUP:0x92e8 is cleared.
    World y;
    r = y.enter("D:\\DEMON'S GATE\\", player(0));
    CHECK_FALSE(r.accepted);
    CHECK(r.handover.demons_gate == 0);
    CHECK(y.st.handover.directory == "D:\\DEMON'S GATE\\");
    CHECK(y.records.list.empty());

    // The demo never enters it.
    World z;
    CHECK_FALSE(z.enter("D:\\DEMON'S GATE\\", demo()).accepted);
}

TEST_CASE("nav enter: a race from the boxes") {
    World x;
    std::string d = "D:\\Lazy Loop - RACE\\";
    EnterInputs in = player(0);  // Lazy Loop is open in the unregistered game
    EnterResult r = x.enter(d, in);
    REQUIRE(r.accepted);
    // The defaults: "2 Ships" (1), "Normal" (0), "Similar" (2), "5 Laps" (1).
    CHECK(r.handover.race == 2);
    CHECK(r.handover.difficulty == 0);
    CHECK(r.handover.opponent == 2);
    CHECK(r.handover.laps == 5);
    CHECK(r.handover.directory == "Lazy Loop");
    CHECK(d == "Lazy Loop");
    CHECK(r.ini_writes == std::vector<std::pair<std::string, int>>{
                              {"Racers", 1}, {"RaceDiff", 0}, {"RaceOpp", 2}, {"RaceLaps", 1}});
    CHECK(x.ini.get_int(is::settings::kGameSection, "RaceOpp", -9) == 2);
    CHECK(x.calls.empty());
    CHECK(x.records.list.back().path == "Lazy Loop");
    const WaveKind k = decode(r.handover);
    CHECK(k.kind == 6);
    CHECK(k.opponents == 1);
    CHECK(k.course == 0);

    // Grand Prix in a registered game, solo, 10 laps.
    World y;
    in = player(-1);
    in.boxes.racers = 0;
    in.boxes.race_laps = 2;
    in.boxes.race_diff = 2;
    in.boxes.race_opp = 5;
    r = y.enter("D:\\Grand Prix - RACE\\", in);
    REQUIRE(r.accepted);
    CHECK(r.handover.race == 1 + 8);
    CHECK(r.handover.laps == 10);
    CHECK(r.handover.difficulty == 2);
    CHECK(r.handover.opponent == 5);
    CHECK(r.handover.directory == "Grand Prix");
    CHECK(decode(r.handover).course == 2);
    CHECK(decode(r.handover).opponents == 0);

    // SDi Speedway, 4 ships; a laps index outside 0-2 leaves DGROUP:0x9ea0.
    in.boxes.racers = 3;
    in.boxes.race_laps = -1;
    y.st.handover.race = 0;  // seg6:0x37ca's clear on the next opening
    r = y.enter("D:\\SDi Speedway - RACE\\", in);
    REQUIRE(r.accepted);
    CHECK(r.handover.race == 4 + 4);
    CHECK(r.handover.laps == 10);
    CHECK(r.ini_writes.back() == std::pair<std::string, int>{"RaceLaps", -1});
    CHECK(decode(r.handover).opponents == 3);
    CHECK(decode(r.handover).course == 1);
}

TEST_CASE("nav enter: a locked race or duel clears DGROUP:0x94e and 0x950") {
    World x;
    EnterResult r = x.enter("D:\\Grand Prix - RACE\\", player(0));
    CHECK_FALSE(r.accepted);
    CHECK(r.handover.race == 0);
    CHECK(r.handover.duel == 0);
    CHECK(r.ini_writes.empty());
    r = x.enter("D:\\Dodge City - DUEL\\", player(0));
    CHECK_FALSE(r.accepted);
    CHECK(r.handover.duel == 0);
}

TEST_CASE("nav enter: a duel from the boxes") {
    World x;
    std::string d = "D:\\Dodge City - DUEL\\";
    EnterInputs in = player(2);
    in.boxes.duelers = 2;
    in.boxes.duel_diff = 1;
    in.boxes.duel_opp = 4;
    EnterResult r = x.enter(d, in);
    REQUIRE(r.accepted);
    CHECK(r.handover.duel == 4);
    CHECK(r.handover.race == 0);
    CHECK(r.handover.difficulty == 1);
    CHECK(r.handover.opponent == 4);
    CHECK(r.handover.directory == "Dodge City");
    CHECK(d == "Dodge City");
    CHECK(r.ini_writes == std::vector<std::pair<std::string, int>>{
                              {"Duelers", 2}, {"DuelDiff", 1}, {"DuelOpp", 4}});
    const WaveKind k = decode(r.handover);
    CHECK(k.kind == 7);
    CHECK(k.opponents == 3);
}

TEST_CASE("nav enter: the demo alternates races and duels with DGROUP:0xe32") {
    World x;
    EnterInputs in = demo();
    EnterResult r = x.enter("D:\\Lazy Loop - RACE\\", in);
    REQUIRE(r.accepted);
    CHECK(x.st.demo_toggle == 1);
    // The next opening clears DGROUP:0x94e; the next race Enter is refused: the gate fails, both
    // stay 0, DGROUP:0xe32 is cleared and the ordinary probe finds no *.exe.
    x.st.handover.race = 0;
    x.calls.clear();
    r = x.enter("D:\\SDi Speedway - RACE\\", in);
    CHECK_FALSE(r.accepted);
    CHECK(x.st.demo_toggle == 0);
    CHECK(lines(x.calls) == std::vector<std::string>{
                                "seg6:0x301a via seg6:0x4190 findfirst \"D:\\SDi Speedway - RACE\\*.exe\" 0x00 -> end"});
    // The one after it succeeds.
    r = x.enter("D:\\SDi Speedway - RACE\\", in);
    CHECK(r.accepted);
    CHECK(x.st.demo_toggle == 1);
    // An ordinary Enter clears the flag as well.
    x.st.handover.race = 0;
    CHECK(x.enter("D:\\GAMES\\", in).accepted);
    CHECK(x.st.demo_toggle == 0);

    // DemoType 521 under a DEMO or PLAY command line lets a race through the flag.
    World y;
    y.st.demo_toggle = 1;
    in.command_line = true;
    in.demo_type = kDemoTypeRace;
    CHECK(y.enter("D:\\Lazy Loop - RACE\\", in).accepted);
    // 522 for a duel.
    World z;
    z.st.demo_toggle = 1;
    in.demo_type = kDemoTypeDuel;
    CHECK(z.enter("D:\\Dodge City - DUEL\\", in).accepted);
    CHECK(z.st.handover.duel == 2);
}

TEST_CASE("nav enter: the DEMO keys") {
    World x;
    x.ini.set_int(is::settings::kGameSection, "DRacers", 2);
    x.ini.set_int(is::settings::kGameSection, "DDiff", 1);
    EnterInputs in = demo();
    in.command_line = true;
    in.demo = true;
    in.boxes.racers = 3;  // ignored
    x.rand_value = 4;     // 4 % 7 = 4: 1 lap
    EnterResult r = x.enter("D:\\Lazy Loop - RACE\\", in);
    REQUIRE(r.accepted);
    CHECK(r.handover.race == 3);
    CHECK(r.handover.difficulty == 1);
    CHECK(r.handover.opponent == 2);
    CHECK(r.handover.laps == 1);
    CHECK(r.ini_writes.empty());
    CHECK(x.rand_calls == 1);

    const auto laps = [&](int dlaps, int rnd, int before) {
        World w;
        w.ini.set_int(is::settings::kGameSection, "DLaps", dlaps);
        w.rand_value = rnd;
        w.st.handover.laps = before;
        return w.enter("D:\\Lazy Loop - RACE\\", in).handover.laps;
    };
    CHECK(laps(0, 12, 0) == 10);  // 12 % 7 = 5
    CHECK(laps(0, 7, 0) == 5);
    CHECK(laps(1, 0, 0) == 1);
    CHECK(laps(2, 0, 0) == 5);
    CHECK(laps(3, 0, 0) == 10);
    CHECK(laps(4, 0, 7) == 7);  // unchanged

    // Without DGROUP:0x8e5e a PLAY race uses the boxes.
    World p;
    in.demo = false;
    r = p.enter("D:\\Lazy Loop - RACE\\", in);
    CHECK(r.handover.race == 4);
    CHECK(r.ini_writes.size() == 4);

    // A duel under any command line takes DuelDiff, opponent 2 and DGROUP:0x950 = 2.
    World q;
    q.ini.set_int(is::settings::kGameSection, "DuelDiff", 2);
    in.boxes.duelers = 2;
    r = q.enter("D:\\Dodge City - DUEL\\", in);
    REQUIRE(r.accepted);
    CHECK(r.handover.duel == 2);
    CHECK(r.handover.difficulty == 2);
    CHECK(r.handover.opponent == 2);
    CHECK(r.ini_writes.empty());
}

TEST_CASE("nav enter: CB_ERR in the racers box gives DGROUP:0x94e = 0 + 4i") {
    World x;
    EnterInputs in = player(-1);
    in.boxes.racers = -1;
    EnterResult r = x.enter("D:\\Grand Prix - RACE\\", in);
    REQUIRE(r.accepted);
    CHECK(r.handover.race == 8);
    CHECK(r.ini_writes.front() == std::pair<std::string, int>{"Racers", -1});
}
