// The saved game on the program's state (src/dialogs/save_game.h), the Restore Game description
// (src/briefing/save_text.h) and the champions table (src/hud/screens.h), with a fake host: the
// writer's gates and names, a save and restore round trip, a refused file leaving the running
// game alone (ADR 0014 decision 2), the version rule, the purge, the state-5 deadline, and
// oracle/digests/core-0003.md's Restore Game and desk texts (M4 s3 SAVE).
#include "doctest.h"

#include <map>
#include <string>

#include "briefing/save_text.h"
#include "dialogs/save_game.h"
#include "formats/savegame.h"
#include "hud/screens.h"
#include "nav/tree.h"
#include "world/rescale.h"
#include "world/roster.h"
#include "world/world.h"

using namespace is;
using namespace is::dialogs::save;
namespace fs = is::fmt::save;

namespace {

class FakeHost final : public SaveHost {
public:
    std::map<std::string, std::vector<std::uint8_t>> files;
    std::map<std::string, std::string> ini_values;
    std::vector<int> messages;
    std::vector<std::string> removed, logs;
    int ini_size = -1;
    bool fail_write = false;

    std::optional<std::vector<std::uint8_t>> read(const std::string& path, std::size_t max) override {
        auto it = files.find(path);
        if (it == files.end()) return std::nullopt;
        std::vector<std::uint8_t> v = it->second;
        if (v.size() > max) v.resize(max);
        return v;
    }
    bool write_atomic(const std::string& path, const std::vector<std::uint8_t>& bytes) override {
        if (fail_write) return false;
        files[path] = bytes;
        return true;
    }
    bool remove(const std::string& path) override {
        removed.push_back(path);
        return files.erase(path) != 0;
    }
    std::vector<std::string> find(const std::string& dir, const std::string& pattern) override {
        const std::string ext = pattern.substr(1);  // "*.ded" -> ".ded"
        std::vector<std::string> out;
        for (const auto& kv : files) {
            if (kv.first.compare(0, dir.size(), dir) != 0) continue;
            const std::string name = kv.first.substr(dir.size());
            if (name.find('\\') != std::string::npos) continue;
            if (name.size() >= ext.size() && name.compare(name.size() - ext.size(), ext.size(), ext) == 0)
                out.push_back(name);
        }
        return out;
    }
    std::string ini(const char* key, const char* def, std::size_t n) override {
        auto it = ini_values.find(key);
        std::string v = it == ini_values.end() ? std::string(def) : it->second;
        if (n > 0 && v.size() > n - 1) v.resize(n - 1);
        return v;
    }
    void ini_set_int(const char* key, int v) override {
        ini_values[key] = std::to_string(v);
        if (std::string(key) == "Size") ini_size = v;
    }
    void message(int id) override { messages.push_back(id); }
    void log(const std::string& s) override { logs.push_back(s); }
    std::tm now() override {
        std::tm t{};
        t.tm_mon = 10;
        t.tm_mday = 2;
        t.tm_year = 126;
        return t;
    }
    std::uint32_t time() override { return 0x5a5a5a5au; }
    int shp_count() override { return 30; }
    bool ship_ok(int) override { return true; }
    std::string ship_name(int s) override { return "SHIP" + std::to_string(s); }
};

struct Fixture {
    world::World w;
    nav::Records nav;
    std::uint32_t frames = 4005;
    SessionValues s;
    SaveGlobals g;
    FakeHost host;
    Ctx c{w, nav, frames, s, g, host, 2, 1};

    Fixture() {
        host.ini_values["Games"] = "GAMES";
        host.ini_values["Dead"] = "RETIRED";
        host.ini_values["Restore"] = "ORACLE03";
        s.game_id = 0x01020304u;
        s.game_date = (126u << 16) | (10u << 8) | 2u;
        s.pilot = "Test Pilot";
        s.wave = 1;
        s.size = 1;
        w.score = 900;
        w.player = 0;
        world::RosterRecord r{};
        r.shp = 5;
        r.credit10 = 15;
        r.fuel40 = 29;
        r.systems = {82, 83, 82, 80};
        w.roster.push_back(r);
        w.player_roster = 0;
        world::ShipRecord& p = w.ship(0);
        p.w26 = 82, p.w28 = 83, p.w2a = 82, p.w2c = 80;
        world::set_ship_credit(p, 150);
        p.weapon_slot = 1;
        nav.list.push_back(nav::DirRecord{"E:\\GAMES\\", 0, 0, 1});
        nav.current = 0;
        w.record = 0;
        w.records.resize(1);
    }
};

}  // namespace

TEST_CASE("save flow: a manual .sav round trip restores the header's globals and blocks") {
    Fixture f;
    CHECK(save_game(f.c, 1, 0) == 1);
    REQUIRE(f.host.files.count("GAMES\\ORACLE03.sav"));
    const auto& file = f.host.files["GAMES\\ORACLE03.sav"];
    CHECK(file.size() == 1248);  // [ORACLE core-0003]
    const auto p = fs::parse(file, fs::Kind::kSav);
    REQUIRE(p);
    CHECK(p->header.wave == 1);
    CHECK(p->header.score == 900);
    CHECK(p->header.shape == 100);
    CHECK(p->header.credit10 == 15);
    CHECK(p->header.frames == 4005);
    CHECK(p->header.ship == 5);
    CHECK(p->header.size == 1);
    CHECK(p->header.weapon_slot == 1);
    CHECK(p->header.pilot_name() == "Test Pilot");
    CHECK(p->roster[0][0x1c] == 29);
    // The directory block holds Navigation's path.
    CHECK(std::string(reinterpret_cast<const char*>(&p->dirs[0][0x152])) == "E:\\GAMES\\");

    Fixture r;  // a fresh process
    r.host.files = f.host.files;
    r.w.score = 0;
    r.w.roster.clear();
    r.nav.list.clear();
    r.w.records.clear();
    r.frames = 0;
    r.s.pilot.clear();
    r.s.wave = 0;
    CHECK(restore_game(r.c) == 1);
    CHECK(r.s.restored == 1);
    CHECK(r.w.restored == 1);  // setup.cpp's new-game reset must skip
    CHECK(r.s.wave == 1);
    CHECK(r.w.wave == 1);
    CHECK(r.w.score == 900);
    CHECK(r.frames == 4005);
    CHECK(r.s.pilot == "Test Pilot");
    CHECK(r.s.game_id == 0x01020304u);  // the file's id replaces step 1's
    REQUIRE(r.w.roster.size() == 1);
    CHECK(r.w.roster[0].fuel40 == 29);
    CHECK(r.w.roster[0].shp == 5);
    REQUIRE(r.nav.list.size() == 1);
    CHECK(r.nav.list[0].path == "E:\\GAMES\\");
    CHECK(r.w.records.size() == 1);
    CHECK(r.w.records[0].b[0xad] == 1);  // the current record's +0xad = 1 when 0
    CHECK(r.g.granularity == fs::Granularity::k32);
}

TEST_CASE("save flow: the trial refuses inside the writer, 2324 for a manual .sav only") {
    Fixture f;
    f.c.edition = 0;
    CHECK(save_game(f.c, 1, 0) == 0);
    CHECK(f.host.messages == std::vector<int>{2324});
    CHECK(save_game(f.c, 0, 0) == 0);  // LASTGAME: silent
    f.w.score = 5000;
    CHECK(save_game(f.c, 1, 1) == 0);  // .ded: silent
    CHECK(f.host.messages.size() == 1);
    CHECK(f.host.files.empty());
    // The demo (DGROUP:0x95bc = 0) passes the gate and writes demogame.sav.
    f.c.player_game = 0;
    CHECK(save_game(f.c, 0, 0) == 1);
    CHECK(f.host.files.count("GAMES\\demogame.sav"));
}

TEST_CASE("save flow: .ded and .don need a score of 1000, purge this game's .ded and find a free name") {
    Fixture f;
    f.host.ini_values["Restore"] = "NEW GAME";
    f.w.score = 999;
    CHECK(save_game(f.c, 1, 1) == 0);
    f.w.score = 1000;
    // An old .ded of this game (purged) and one of another game (kept, so the name moves on).
    std::vector<std::uint8_t> mine(0x1c0 + 0x80, 0), other(0x1c0 + 0x80, 0);
    mine[0] = 0x04, mine[1] = 0x03, mine[2] = 0x02, mine[3] = 0x01;
    other[0] = 0x77;
    f.host.files["RETIRED\\OLD.ded"] = mine;
    f.host.files["RETIRED\\Test.ded"] = other;
    CHECK(save_game(f.c, 1, 1) == 1);
    CHECK(f.host.removed == std::vector<std::string>{"RETIRED\\OLD.ded"});
    CHECK(f.host.files.count("RETIRED\\Test1.ded"));
    CHECK(f.host.files["RETIRED\\Test1.ded"].size() == 0x1c0 + 0x80);  // no directory block
    // A .don is never purged.
    f.host.files["RETIRED\\X.don"] = mine;
    CHECK(save_game(f.c, 1, 2) == 1);
    CHECK(f.host.files.count("RETIRED\\X.don"));
    CHECK(f.host.files.count("RETIRED\\Test.don"));
}

TEST_CASE("save flow: no file without a roster, and a failed write keeps the old file") {
    Fixture f;
    f.host.files["GAMES\\ORACLE03.sav"] = {1, 2, 3};
    f.w.roster.clear();
    CHECK(save_game(f.c, 1, 0) == 0);
    CHECK(f.host.files["GAMES\\ORACLE03.sav"].size() == 3);
    Fixture g;
    g.host.files["GAMES\\ORACLE03.sav"] = {1, 2, 3};
    g.host.fail_write = true;
    CHECK(save_game(g.c, 1, 0) == 0);
    CHECK(g.host.files["GAMES\\ORACLE03.sav"].size() == 3);
}

TEST_CASE("save flow: a short or refused file leaves the running game's blocks alone") {
    Fixture f;
    REQUIRE(save_game(f.c, 1, 0) == 1);
    auto file = f.host.files["GAMES\\ORACLE03.sav"];
    file.resize(file.size() - 1);
    f.host.files["GAMES\\ORACLE03.sav"] = file;
    f.w.roster[0].fuel40 = 7;
    f.w.score = 1234;
    CHECK(restore_game(f.c) == 0);
    CHECK(f.w.roster.size() == 1);
    CHECK(f.w.roster[0].fuel40 == 7);
    CHECK(f.w.score == 1234);
    CHECK(f.nav.list.size() == 1);
    CHECK(f.w.records.size() == 1);
    CHECK(f.s.restored == 0);
    CHECK(f.s.game_id == 0x5a5a5a5au);  // step 1 ran before the open (decision 2)
    f.host.files.clear();
    CHECK(restore_game(f.c) == 0);  // does not open
}

TEST_CASE("save flow: the version rule and the Size write-back") {
    Fixture f;
    world::RosterRecord r2{};
    r2.shp = 3;
    r2.credit10 = 450;
    f.w.roster[0].credit10 = -20000;
    f.w.roster.push_back(r2);
    REQUIRE(save_game(f.c, 1, 0) == 1);
    auto& file = f.host.files["GAMES\\ORACLE03.sav"];
    file[0x15e] = 99;  // an older file
    file[0x15f] = 0;
    file[0x171] = 2;   // Size 2
    Fixture r;
    r.host.files = f.host.files;
    world::size_session().dg_92e2 = 0;
    CHECK(restore_game(r.c) == 1);
    REQUIRE(r.w.roster.size() == 2);
    // Records k < DGROUP:0x931a: -20000 is below -15000, so (uint16) / 10 = 4553.
    CHECK(r.w.roster[0].credit10 == static_cast<std::int16_t>(static_cast<std::uint16_t>(-20000) / 10u));
    CHECK(r.w.roster[1].credit10 == 450);  // the last record is not divided
    CHECK(r.s.size == 2);
    CHECK(r.host.ini_size == 2);
    CHECK(world::size_session().dg_92e2 == 1);  // DGROUP:0x92e2's one home (world/rescale.h)
    world::size_session().dg_92e2 = 0;
}

TEST_CASE("save flow: the roster check with decision 7's last record") {
    std::vector<RosterBytes> r(3);
    r[0][0] = 40;      // not below the SHP count: 1
    r[0][0x64] = 7;    // team 7 → 0
    r[1][0] = 2;
    r[2][0] = 31;      // the last record: the bounds part (decision 7)
    r[2][0x64] = 7;    // not checked for the last record
    roster_check(r, 30, [](int v) { return v != 2; });
    CHECK(r[0][0] == 1);
    CHECK(r[0][0x64] == 0);
    CHECK(r[1][0] == 2);  // only record 0 takes seg14:0x455c's refusal
    CHECK(r[2][0] == 1);
    CHECK(r[2][0x64] == 7);
}

TEST_CASE("save flow: the directory records' one byte store") {
    world::World w;
    nav::Records nav;
    nav.list.push_back(nav::DirRecord{"D:\\A\\", 0, 0, 0});
    nav.list.push_back(nav::DirRecord{"D:\\B\\", 0, 0, 2});
    sync_from_nav(nav, w);
    REQUIRE(w.records.size() == 2);
    CHECK(w.records[1].b[0x1d0] == 2);
    CHECK(w.records[0].b[0x1c0] == 0xff);
    w.records[0].set_word(0x14e, 4);  // the wave's counters (seg7:0x5ae8)
    w.records[0].set_word(0x150, 5);
    w.records[0].b[0x1d0] = 1;
    sync_to_nav(w, nav);
    CHECK(nav.list[0].destroyed == 4);
    CHECK(nav.list[0].captured == 5);
    CHECK(nav.list[0].state == 1);
    CHECK(nav.list[0].path == "D:\\A\\");
}

TEST_CASE("save flow: the score list from the files and the state-5 deadline") {
    Fixture f;
    f.w.score = 5000;
    f.host.ini_values["Restore"] = "NEW GAME";
    REQUIRE(save_game(f.c, 1, 1) == 1);  // this game's .ded, the same score: dropped
    f.host.files["RETIRED\\BAD.don"] = {1, 2};  // short: skipped (decision 9)
    const fs::ScoreList& l = score_list(f.c);
    REQUIRE(l.list.size() == 1);
    CHECK(l.list[0].file() == "--");
    CHECK(f.g.rank == 1);
    CHECK(table_deadline(1000, 1, false) == 1000 + 9800);
    CHECK(table_deadline(1000, 2, false) == 1000 + 6000 + 1000);
    CHECK(table_deadline(1000, 0, false) == 1000 + 7200 + 1000);
    CHECK(table_deadline(1000, 11, false) == 1000);
    CHECK(table_deadline(1000, 3, true) == 12000);
    CHECK_FALSE(table_due(1000, 1000, false));  // strict (claim 43, CORRECTED)
    CHECK(table_due(1001, 1000, false));
    CHECK_FALSE(table_due(1001, 1000, true));
    free_scores(f.g);
    CHECK_FALSE(f.g.scores.has_value());
}

namespace {
std::string lang(int id) {
    static const std::map<int, std::string> m = {
        {304, "SHIP"}, {311, "GOOD"}, {312, "FAIR"}, {313, "BAD"}, {318, "DIRECTORY"}, {797, " AND"},
        {2500, "%d OF THEM"}, {2501, "ALL OF THEM"}, {2502, "NONE OF THEM"}, {2503, "IT"},
        {2504, "SEC"}, {2505, "MIN"}, {2515, "ICON"}, {2529, "SOME OF IT"}, {12515, "ICONS"},
        {12504, "SEC"}, {12505, "MIN"}, {10318, "DIRECTORIES"}, {10304, "SHIPS"},
        {3000, "%<SO FAR, %>YOU HAVE REACHED WAVE %d AFTER PLAYING FOR %# %#, VISITING %# AND COMPLETING %s. "},
        {3001, "OF THE %# TOTAL IN THE GAME, YOU %<CAPTURED %#%<DESTROYED %#%>. "},
        {3002, "YOUR SHIP HAS %d.%d ICONS OF CREDIT AND IS IN %A SHAPE. "},
        {3027, "%<YOU HAVE%>%<%s HAS%> %<DESTROYED NO SHIPS%>%<DESTROYED %#%>."},
        {3031, "%N%N%<WATCH OUT FOR THOSE PIRATES. THEY WANT YOUR LOOT!%N%N%>YOUR SHIP IS IN %S SHAPE."},
    };
    auto it = m.find(id);
    return it == m.end() ? std::string() : it->second;
}
}  // namespace

TEST_CASE("save text: core-0003's Restore Game descriptions and the desk lines") {
    fs::Header h;
    h.wave = 1;
    h.frames = 4005;  // ORACLE03: 1 MIN 51 SEC
    h.dir_count = 1;
    h.completed = 0;
    h.credit10 = 15;
    h.shape = 100;
    h.set_pilot("P");
    auto b = fs::encode_header(h);
    CHECK(briefing::restore_description(b.data(), "P", lang, false) ==
          "SO FAR, YOU HAVE REACHED WAVE 1 AFTER PLAYING FOR 1 MIN 51 SEC, VISITING 1 DIRECTORY AND "
          "COMPLETING NONE OF THEM. YOUR SHIP HAS 1.5 ICONS OF CREDIT AND IS IN GOOD SHAPE. ");
    h.wave = 2;
    h.frames = 11956;  // ORACLE3X: 5 MIN 32 SEC, BAD
    h.shape = 102;
    b = fs::encode_header(h);
    CHECK(briefing::restore_description(b.data(), "P", lang, false) ==
          "SO FAR, YOU HAVE REACHED WAVE 2 AFTER PLAYING FOR 5 MIN 32 SEC, VISITING 1 DIRECTORY AND "
          "COMPLETING NONE OF THEM. YOUR SHIP HAS 1.5 ICONS OF CREDIT AND IS IN BAD SHAPE. ");
    // The desk: no "SO FAR, ", 2529 for +0x160 = 0 with +0x32 = 1, then 3031.
    h.wave = 1;
    h.frames = 4005;
    h.shape = 100;
    b = fs::encode_header(h);
    CHECK(briefing::desk_progress(b.data(), lang) ==
          "YOU HAVE REACHED WAVE 1 AFTER PLAYING FOR 1 MIN 51 SEC, VISITING 1 DIRECTORY AND COMPLETING "
          "SOME OF IT. \r\n\r\nYOUR SHIP IS IN GOOD SHAPE.");
    CHECK(briefing::completion_text(1, 1, lang, false) == "IT");
    CHECK(briefing::completion_text(0, 0, lang, false) == "ALL OF THEM");
    CHECK(briefing::completion_text(2, 5, lang, false) == "2 OF THEM");
}

TEST_CASE("save text: 3001, the old credit and 3027 with the help window") {
    fs::Header h;
    h.wave = 3;
    h.dir_count = 2;
    h.completed = 2;
    h.captured = 1;
    h.destroyed = 0;
    h.w945c = 2;
    h.w945e = 1;
    h.credit10 = -25;  // below 100: an older file, divided again
    h.shape = 99;
    h.set_pilot("OTHER");
    const auto b = fs::encode_header(h);
    const std::string d = briefing::restore_description(b.data(), "ME", lang, true);
    CHECK(d.find("COMPLETING ALL OF THEM. ") != std::string::npos);
    CHECK(d.find("OF THE 3 ICONS TOTAL IN THE GAME, YOU CAPTURED 1 ICON. ") != std::string::npos);
    CHECK(d.find("YOUR SHIP HAS 0.2 ICONS OF CREDIT") != std::string::npos);
    CHECK(d.find("OTHER HAS DESTROYED NO SHIPS.") != std::string::npos);
}

TEST_CASE("champions table: seg6:0x62b6's texts") {
    std::vector<hud::ChampionRow> rows = {{"A - SHIP", 9000, 1}, {"Me - SHIP", 5000, 2}, {"", 0, 3}};
    const auto t = hud::champion_texts(1024, 768, 768, "Champions of INNER SPACE", rows, 2, 5000, "YOUR ", "SCORE");
    REQUIRE(t.size() == 1 + 6 + 1);
    CHECK(t[0] == hud::TableText{0, 6, 0x000000FFu, 512, 384 - 175, "Champions of INNER SPACE"});
    CHECK(t[1] == hud::TableText{0, 2, 0x0200E6FFu, 512 - 360 + 7, 384 - 130, "1"});
    CHECK(t[2].text == "A - SHIP");
    CHECK(t[2].x == 512 - 360 + 46);
    CHECK(t[3] == hud::TableText{0, 2, 0x0200E6FFu, 512 + 360 - 30, 384 - 130, "9000"});
    CHECK(t[4].colour == 0x02FFFFFFu);  // the running score's row
    // The footer at the empty row's y.
    CHECK(t[7] == hud::TableText{0, 6, 0x02FFFFFFu, 512, 30 * 2 + 384 - 130 + 50, "YOUR SCORE: 5000"});
    rows[1].score = 1;
    const auto u = hud::champion_texts(1024, 768, 400, "T", rows, 8, 5000, "YOUR ", "SCORE");
    CHECK(u[0].y == 384 + 15 - 175);
    CHECK(u[1].colour == 0x0200FFFFu);  // i < 3 and rank > 7
    CHECK(u[4].colour == 0x0200FFFFu);
    // Row 1 of rank 5: g = (1 x 0xff01, 16-bit) / 10 + 230 = 205.
    const auto v = hud::champion_texts(1024, 768, 768, "T", rows, 5, 5000, "YOUR ", "SCORE");
    CHECK(v[4].colour == 0x0200CDFFu);
}
