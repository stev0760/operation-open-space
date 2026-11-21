// The Ship Watch's key, title and painter and the All Ships rows (src/hud/watch.h; M5 stage 4,
// track WATCH), against the listings of seg13:0x563c, seg13:0x57ac, seg13:0x50b0 and seg13:0x6450
// and the panels of ships-0005 ("FUZZY ONES - ALLY +28", "TEAMMATE - ALLY +29").
#include "doctest.h"

#include <map>
#include <string>
#include <vector>

#include "hud_fakes.h"
#include "hud/watch.h"

using namespace is::hud;
using hudtest::Call;
using hudtest::RecordingCanvas;

namespace {

struct WHost final : WatchHost {
    bool win = false;
    int tp = -1, tr = 0;  // the player's target
    std::vector<DefenderWords> defs;
    std::map<std::pair<int, int>, int> states;
    std::vector<int> visible = {1, 2};  // ships that pass seg3:0x0
    Mark m;
    int nexts = 0, creates = 0, paints = 0, destroys = 0;
    std::string title;
    std::uint32_t clock = 1000;
    std::map<int, int> teams = {{0, 0}, {1, 0}, {2, 2}};
    std::map<int, int> views = {{1, 290}, {2, 280}, {0, 0}};
    std::string l1, l2;
    int activities = 0;
    std::vector<int> next_pool = {14};  // seg3:0x45a(6)'s results
    std::vector<int> next_rec = {2};

    bool window() const override { return win; }
    int last_ship() const override { return 2; }
    int player_target_pool() const override { return tp; }
    int player_target_rec() const override { return tr; }
    int defenders() const override { return static_cast<int>(defs.size()); }
    DefenderWords defender(int k) const override { return defs[static_cast<std::size_t>(k)]; }
    int object_state(int p, int r) const override {
        auto it = states.find({p, r});
        return it == states.end() ? 0 : it->second;
    }
    bool target_ok(int p, int r) const override {
        if (p != 14) return p == 2;
        for (int v : visible)
            if (v == r) return true;
        return false;
    }
    Mark& mark() override { return m; }
    void next_mark() override {
        const std::size_t k = static_cast<std::size_t>(nexts) % next_pool.size();
        m.pool = next_pool[k];
        m.rec = next_rec[k];
        ++nexts;
    }
    void post_create() override { ++creates; }
    void post_paint() override { ++paints; }
    void set_title(const std::string& t) override { title = t; }
    void destroy() override {
        ++destroys;
        win = false;
    }
    std::uint32_t now() const override { return clock; }
    int attitude(int a, int) const override { return views.at(a); }
    int team(int s) const override { return teams.at(s); }
    std::string team_name(int t) override { return t == 2 ? "FUZZY ONES" : "OTHERS"; }
    std::string attitude_words(int v) override { return "ALLY +" + std::to_string(v / 10); }
    std::string lang(int id) override {
        if (id == 2041) return "REALITY CHECK";
        if (id == 2042) return "TEAMMATE";
        if (id == 2531) return "%D %D";
        return {};
    }
    void activity(int, int r) override {
        ++activities;
        l1 = r == 2 ? "I'M APPROACHING FUEL TANK" : "I'M ATTACKING ROCK.";
        l2 = "THERE'S NOTHING BETTER TO DO.";
    }
    const std::string& line1() const override { return l1; }
    const std::string& line2() const override { return l2; }
    std::string name(int, int p, int r, int) override {
        if (p == 14) return r == 1 ? "GOLDEN VIPER" : "PLATYPUS";
        return "DEFENDER";
    }
    std::string pilot_name(int s) override { return s == 1 ? "FUZZY 19" : "pilot"; }
    bool cell(int, int, CellInputs& in) override {
        in = CellInputs{};
        return true;
    }
    int client_width() const override { return 280; }
};

std::vector<Call> texts(const RecordingCanvas& c) {
    std::vector<Call> v;
    for (const Call& k : c.calls)
        if (k.op == Call::kText) v.push_back(k);
    return v;
}

}  // namespace

TEST_CASE("seg13:0x563c W without a window: the mark, the next ship, the create") {
    WHost h;
    h.m.pool = 3;
    const int r = watch_key_563c(h, false, false);
    CHECK(r == 1);
    // (ship pool, 0) and the origin (2, 2), then seg3:0x45a(6) since no ship is targeted.
    CHECK(h.nexts == 1);
    CHECK(h.m.pool == 14);
    CHECK(h.m.rec == 2);
    CHECK(h.m.ox == 2);
    CHECK(h.m.oy == 2);
    CHECK(h.creates == 1);
    CHECK(h.paints == 0);
}

TEST_CASE("seg13:0x563c the player's ship target is watched at once") {
    WHost h;
    h.tp = 14;
    h.tr = 1;
    CHECK(watch_key_563c(h, false, false) == 1);
    CHECK(h.nexts == 0);
    CHECK(h.m.rec == 1);
    // A second W with the window up moves on, posts the paint and sets the title.
    h.win = true;
    CHECK(watch_key_563c(h, false, false) == 1);
    CHECK(h.nexts == 1);
    CHECK(h.paints == 1);
    CHECK(h.title == "Platypus - Pilot");
}

TEST_CASE("seg13:0x563c Ctrl closes, Ctrl+Alt watches yourself, Alt the first defender") {
    WHost h;
    h.win = true;
    CHECK(watch_key_563c(h, true, false) == 0);
    CHECK(h.destroys == 1);
    CHECK_FALSE(h.win);
    // Ctrl without a window opens like W.
    CHECK(watch_key_563c(h, true, false) == 1);
    CHECK(h.creates == 1);
    // Ctrl+Alt: (ship pool, P).
    h.win = true;
    CHECK(watch_key_563c(h, true, true) == 1);
    CHECK(h.m.pool == 14);
    CHECK(h.m.rec == 0);
    CHECK(h.title == "Platypus - Pilot");
    // Alt: the first defender owned by (ship pool, P) whose pool-2 object is in play.
    DefenderWords d;
    d.owner_pool = 14;
    d.owner_rec = 0;
    h.defs = {DefenderWords{}, d, d};
    h.states[{2, 1}] = 1;  // out of play
    CHECK(watch_key_563c(h, false, true) == 1);
    CHECK(h.m.pool == 2);
    CHECK(h.m.rec == 2);
    // The next mark leaving the ship pool and pool 2 returns 0.
    h.next_pool = {3};
    h.next_rec = {0};
    h.tp = 3;
    h.win = true;
    CHECK(watch_key_563c(h, false, false) == 0);
}

TEST_CASE("seg13:0x57ac the title cased by seg11:0x3fd4 mode 2") {
    WHost h;
    h.m.pool = 14;
    h.m.rec = 1;
    CHECK(watch_title(h) == "Golden Viper - Fuzzy 19");
    h.m.pool = 2;
    CHECK(watch_title(h) == "Defender");
}

TEST_CASE("seg13:0x50b0 the panel's texts and colours") {
    WHost h;
    h.win = true;
    h.m.pool = 14;
    h.m.rec = 2;
    RecordingCanvas c;
    WatchState ws;
    watch_paint(h, ws, c, 1);
    CHECK(h.activities == 1);
    CHECK(ws.text_due == 1500);
    // The strip PatBlt(0x30, 0, right, 0x30, BLACKNESS) and the three lines.
    bool strip = false;
    for (const Call& k : c.calls)
        if (k.op == Call::kPat && k.x == 0x30 && k.y == 0 && k.w == 280 && k.h == 0x30 && k.rop == kBlackness) strip = true;
    CHECK(strip);
    auto t = texts(c);
    REQUIRE(t.size() == 3);
    CHECK(t[0].text == "FUZZY ONES - ALLY +28");  // ships-0005 b1
    CHECK(t[0].x == 0x30);
    CHECK(t[0].y == 0);
    CHECK(t[0].colour == 0x02FFB900);  // v > 80
    CHECK(t[1].text == "I'M APPROACHING FUEL TANK");
    CHECK(t[1].y == 0x10);
    CHECK(t[1].colour == 0x0000FFFF);
    CHECK(t[2].y == 0x1e);
    // Within 500 ms the text is not rebuilt and nothing is drawn but the cell.
    c.calls.clear();
    h.clock = 1400;
    watch_paint(h, ws, c, 2);
    CHECK(h.activities == 1);
    CHECK(texts(c).empty());
    // After 500 ms it is rebuilt; unchanged lines draw nothing.
    h.clock = 1600;
    watch_paint(h, ws, c, 2);
    CHECK(h.activities == 2);
    CHECK(texts(c).empty());
    CHECK(ws.text_due == 2100);
    // The teammate: "TEAMMATE - ALLY +29"; the player: "REALITY CHECK" without the attitude.
    h.m.rec = 1;
    h.l1.clear();
    c.calls.clear();
    watch_paint(h, ws, c, 1);
    t = texts(c);
    REQUIRE(t.size() == 3);
    CHECK(t[0].text == "TEAMMATE - ALLY +29");
    h.m.rec = 0;
    h.l1.clear();
    c.calls.clear();
    watch_paint(h, ws, c, 1);
    t = texts(c);
    REQUIRE(t.size() == 3);
    CHECK(t[0].text == "REALITY CHECK");
    CHECK(t[0].colour == 0x02AAAAAA);
}

TEST_CASE("seg13:0x50b0 a watched ship that is gone moves the watch on") {
    WHost h;
    h.win = true;
    h.m.pool = 14;
    h.m.rec = 2;
    h.visible = {1};
    h.next_pool = {14};
    h.next_rec = {1};
    RecordingCanvas c;
    WatchState ws;
    watch_paint(h, ws, c, 2);
    CHECK(h.nexts == 1);
    CHECK(h.paints == 1);
    CHECK(h.destroys == 0);
    // Nothing left to watch: the window closes.
    h.visible.clear();
    h.next_pool = {3};
    h.next_rec = {0};
    h.m.pool = 14;
    h.m.rec = 2;
    watch_paint(h, ws, c, 2);
    CHECK(h.destroys == 1);
}

TEST_CASE("seg13:0x6450 the All Ships rows") {
    WHost h;
    WatchState ws;
    RecordingCanvas c;
    // The first call finds 2 rows where there were 0: a resize, nothing drawn.
    CHECK(smart_rows(h, ws, &c, 1, -1) == 2);
    CHECK(ws.rows == 2);
    CHECK(ws.row[0].rec == 1);
    CHECK(ws.row[1].rec == 2);
    CHECK(ws.row[1].y == 46 + 2);
    CHECK(smart_rows(h, ws, &c, 1, -1) == 0);
    // No ship left: close.
    h.visible.clear();
    CHECK(smart_rows(h, ws, &c, 1, -1) == 1);
    CHECK(ws.rows == 0);
    // The window's size [BYTES seg13:0x628d-0x62e1].
    CHECK(smart_width(0) == 50);
    CHECK(smart_width(1) == 25);
    CHECK(smart_height(18, 2, 0) == 18 + 92 + 6);
    CHECK(smart_height(18, 2, 1) == 18 + 46 + 3);
}
