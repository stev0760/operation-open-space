// The gate exit seg12:0x2288 with its vanish (seg7:0x13b2 mode 1, seg7:0x158e, the end
// seg7:0x17dc-0x1a02), the 800 award's search, the tick's deadline block seg7:0x5ce-0x707,
// seg9:0x6bc2 / seg9:0x6cc4 and seg9:0x2828's remaining icons (src/world/gate_exit.h,
// wave_end.h) (M4 s1 END).
#include "doctest.h"

#include <string>
#include <vector>

#include "shell/game_state.h"
#include "shell/rng.h"
#include "world/entry_effect.h"
#include "world/gate_exit.h"
#include "world/sprite.h"
#include "world/wave_end.h"
#include "world/world.h"

using namespace is::world;
namespace shell = is::shell;

namespace {

struct GateHost : GateExitHost {
    std::vector<int> sounds;
    std::vector<int> messages;
    std::vector<std::pair<int, int>> removed_list, columns;
    int lines_n = 0, menus = 0;
    void sound(int id, int, int, int) override { sounds.push_back(id); }
    void removed(int pool, int rec) override { removed_list.push_back({pool, rec}); }
    void column_row(int pool, int rec) override { columns.push_back({pool, rec}); }
    void message(int, int id) override { messages.push_back(id); }
    void lines() override { ++lines_n; }
    void menu_refresh() override { ++menus; }
};

// The player (pool 14 record 0) with TRANSFORMER's cell at Size 1: size 33, gap 5, on screen.
struct GateFixture {
    World w;
    RecordingBlitter out;
    shell::Rng rng;
    EntryTable table;
    std::int16_t dg_9148 = 0;
    GateHost host;
    struct Hooks : EntryHooks {
        GateFixture* f = nullptr;
        std::vector<int> sounds;
        int ends = 0;
        void sound(int id, int, int, int) override { sounds.push_back(id); }
        void vanished(int pool, int rec) override {
            ++ends;
            GateExitCtx c{f->w, 0, f->host, &f->ec};
            vanish_end(c, pool, rec);
        }
    } hooks;
    EntryContext ec{w.pools, w.ships, table, out, rng, dg_9148};

    GateFixture() {
        clear_entries(table);
        rng.srand(4321);
        hooks.f = this;
        ec.hooks = &hooks;
        ec.size_level = 1;
        w.player = 0;
        w.last_ship = 0;
        w.ships[0].bec = 1;  // alive
        w.ships[0].roster_index = -1;
        ObjectRecord& o = w.pools.rec(kPoolShips, 0);
        o.size = 33;
        o.gap = 5;
        o.on_screen = 1;
        o.scr_x = 495;
        o.scr_y = 325;
        o.w3a = 0x50;
    }
    ObjectRecord& player() { return w.pools.rec(kPoolShips, 0); }
    void exit_player() {
        GateExitCtx c{w, 50000, host, &ec};
        gate_exit(c, kPoolShips, 0);
    }
};

}  // namespace

TEST_CASE("seg12:0x2288: the player through the gate starts a mode-1 vanish of 29 frames at Size 1") {
    GateFixture f;
    f.exit_player();
    CHECK(f.w.ships[0].w2e == 1);          // [BYTES seg12:0x239c]
    CHECK(f.player().b2e == 1);            // [BYTES seg7:0x14b2]
    CHECK(f.player().alt_dc != 0);         // seg12:0x2556's private buffer
    CHECK(f.w.ships[0].beb == 1);          // [BYTES seg7:0x14e5]
    CHECK(f.hooks.sounds == std::vector<int>{0x27});  // HYPER [BYTES seg7:0x13f2]
    CHECK(f.host.sounds.empty());
    CHECK(f.dg_9148 == 1);
    CHECK(vanish_frames(33, 1) == 29);
    int frames = 0;
    while (f.hooks.ends == 0 && frames < 100) {
        entry_step(f.ec);
        ++frames;
    }
    CHECK(frames == 29);
    CHECK(f.player().state == 1);        // [BYTES seg7:0x18b7]
    CHECK(f.player().b2e == 0xff);       // [BYTES seg7:0x19d1]
    CHECK(f.player().alt_dc == 0);       // seg12:0x26a4
    CHECK(f.w.ships[0].beb == 0);        // the tail [BYTES seg7:0x1adc]
    CHECK(f.dg_9148 == 0);
    CHECK(f.host.columns.size() == 1);   // seg7:0x65aa
    CHECK(f.host.lines_n == 1);          // seg12:0x3778
    CHECK(f.w.score == 0);               // no award for pool 14
    // The cloak edge: a second call while +0x2e > 0 plays nothing and takes no slot.
    f.player().b2e = 1;
    f.exit_player();
    CHECK(f.hooks.sounds.size() == 1);
}

TEST_CASE("seg12:0x2288: an off-screen object gets state 1, seg7:0x2080 and 0x27") {
    GateFixture f;
    f.player().on_screen = 0;
    f.exit_player();
    CHECK(f.player().state == 1);
    CHECK(f.player().alt_dc == 0);
    CHECK(f.host.sounds == std::vector<int>{0x27});
    CHECK(f.host.removed_list.size() == 1);
}

TEST_CASE("seg12:0x2288: a pool-5 object and a dead ship") {
    GateFixture f;
    f.w.roles.pool5 = 5;
    GateExitCtx c{f.w, 0, f.host, &f.ec};
    gate_exit(c, 5, 0);
    CHECK(f.w.pools.rec(5, 0).b2e == 0xff);
    CHECK(f.host.sounds == std::vector<int>{0x27});
    gate_exit(c, 5, 0);  // +0x2e != 0: nothing
    CHECK(f.host.sounds.size() == 1);
    struct DeadHost : GateExitHost {
        int destroyed = 0;
        void destroy(int, int) override { ++destroyed; }
    } dh;
    f.w.ships[0].bec = 0;
    GateExitCtx d{f.w, 0, dh, &f.ec};
    gate_exit(d, kPoolShips, 0);
    CHECK(dh.destroyed == 1);
    CHECK(f.player().b2e == 0);
}

TEST_CASE("seg12:0x2288: the pair note DGROUP:0x93b6") {
    GateFixture f;
    f.w.last_ship = 1;
    ShipRecord& p = f.w.ships[0];
    p.w52 = 2;
    p.w58 = 0;
    p.w54 = 14;
    p.w56 = 1;
    f.w.ships[1].roster_index = 5;
    f.w.pools.rec(14, 1).state = 0;
    f.exit_player();
    CHECK(f.w.w93b6 == 5);
}

TEST_CASE("seg7:0x18f1: the 800 award is one search over the hit history (correction 15)") {
    GateFixture f;
    World& w = f.w;
    w.frames = 1000;  // DGROUP:0x1e68 / 10 = 100
    w.w937a = {95, 90, 85, 0, 0, 0, 0, 0};
    w.hit_pool = {3, 0, 0, 0, 0, 0, 0, 0};
    w.hit_rec = {1, 2, 2, 0, 0, 0, 0, 0};
    CHECK(award_entry(w, 0, 2) == 1);
    CHECK(award_entry(w, 3, 1) == 0);
    CHECK(award_entry(w, 0, 3) == -1);
    // A stale entry first ends the search.
    World s = w;
    s.w937a[0] = 50;
    CHECK(award_entry(s, 0, 2) == -1);
    // An entry from the future (frames lower than its word) fails the 32-bit window too.
    s.w937a[0] = 120;
    CHECK(award_entry(s, 0, 2) == -1);
    // The vanish end of icon 2: 800 once, 0x73 and message 0x70 once each.
    ObjectRecord& icon = w.pools.rec(0, 2);
    icon.b2e = 1;
    GateExitCtx c{w, 0, f.host, &f.ec};
    vanish_end(c, 0, 2);
    CHECK(w.score == 800u);
    CHECK(f.host.sounds == std::vector<int>{0x73});
    CHECK(f.host.messages == std::vector<int>{0x70});
    CHECK(icon.state == 1);
    CHECK(icon.b2e == 0xff);
    // seg8:0x81f8 pushes the newest entry first.
    push_hit(w, 9, 4);
    CHECK(w.w937a[0] == 100);
    CHECK(w.hit_pool[0] == 9);
    CHECK(w.hit_rec[0] == 4);
    CHECK(w.w937a[1] == 95);
}

TEST_CASE("seg7:0x181e: a vanishing ship with word +0x2e = 2 keeps its buffer and gets 0xfe") {
    GateFixture f;
    f.w.ships[0].w2e = 2;
    f.player().b2e = 1;
    GateExitCtx c{f.w, 1000, f.host, &f.ec};
    vanish_end(c, kPoolShips, 0);
    CHECK(f.player().b2e == 0xfe);
    CHECK(f.player().state == 0);
    CHECK(f.host.columns.empty());
}

namespace {

struct EndHost : WaveEndHost {
    std::vector<std::string> calls;
    void music(int code, int arg) override { calls.push_back("music " + std::to_string(code) + " " + std::to_string(arg)); }
    void reset_sounds() override { calls.push_back("reset"); }
    void close_watch_windows() override { calls.push_back("watch"); }
    void close_hud() override { calls.push_back("hud"); }
    void black_client() override { calls.push_back("black"); }
    void debriefing_started() override { calls.push_back("debrief"); }
    void history_box() override { calls.push_back("box"); }
    void save_lastgame() override { calls.push_back("lastgame"); }
};

struct EndFixture {
    World w;
    shell::GameState s;
    EndHost host;
    WaveEndCtx c{w, s, host, nullptr};
    EndFixture() {
        w.player = 0;
        w.ships[0].bec = 1;
        w.ships[0].roster_index = -1;
        w.wave = 1;
        s.set(shell::kWave);
        s.pass(10000);  // DGROUP:0x1e50 = 10000
    }
};

}  // namespace

TEST_CASE("seg7:0x610: nothing armed, or DGROUP:0x1e80 not yet due, runs the wave pass") {
    EndFixture f;
    CHECK(wave_deadlines(f.c) == DeadlineResult::kContinue);
    f.w.w1e80 = 10001;
    CHECK(wave_deadlines(f.c) == DeadlineResult::kContinue);
    CHECK(f.w.w1e80 == 10001u);
    CHECK(f.s.state() == shell::kWave);
}

TEST_CASE("seg7:0x610: DGROUP:0x1e80 due with the player alive goes to state 2 through seg9:0x6bc2") {
    EndFixture f;
    f.w.w1e80 = 10000;  // not above now
    f.w.play_mode = 2;
    f.s.deadlines().debrief_nav = 5;
    CHECK(wave_deadlines(f.c) == DeadlineResult::kEndPass);
    CHECK(f.s.state() == shell::kDebriefing);
    CHECK(f.w.w1e80 == 0u);
    CHECK(f.w.play_mode == 1);                 // [BYTES seg7:0x6e4]
    CHECK(f.s.deadlines().debrief_nav == 0u);  // DGROUP:0x1e74
    CHECK(f.w.w94c4 == 1);
    CHECK(f.host.calls == std::vector<std::string>{"music -1 1", "reset", "watch", "hud", "black",
                                                   "lastgame", "debrief"});
}

TEST_CASE("seg7:0x610: DGROUP:0x1e80 due with the player dead arms DGROUP:0x1e84 = now") {
    EndFixture f;
    f.w.w1e80 = 9000;
    f.w.ships[0].bec = 0;
    CHECK(wave_deadlines(f.c) == DeadlineResult::kEndPass);
    CHECK(f.w.w1e84 == 10000u);
    CHECK(f.w.w1e80 == 0u);
    CHECK(f.s.state() == shell::kWave);
    CHECK(f.host.calls.empty());
}

TEST_CASE("seg7:0x6ac: DGROUP:0x1e84 due: the trial's history box, state 5, then GAME OVER") {
    EndFixture f;
    f.w.w1e84 = 10000;
    f.w.w920c = 0;  // the limited trial
    CHECK(wave_deadlines(f.c) == DeadlineResult::kEndPass);
    CHECK(f.host.calls == std::vector<std::string>{"box"});
    CHECK(f.w.w1e84 == 0u);
    CHECK(f.s.state() == shell::kHighScores);
    CHECK(f.s.deadlines().scores_end == 44000u);
    // The box stayed up 5.7 min with DGROUP:0x1e50 sampled before it: the next pass ends state 5
    // and GAME OVER follows for 25 s [ORACLE core-0001].
    f.s.pass(10000 + 342000);
    CHECK(f.s.state() == shell::kGameOver);
    CHECK(f.s.change_pending());
    f.s.change_step(10000 + 342000);
    CHECK(f.s.state() == shell::kGameOver);
    f.s.set_suspended(false);
    CHECK(f.s.pass(10000 + 342000 + 24999));
    f.s.pass(10000 + 342000 + 25000);
    CHECK(f.s.requested() == shell::kIntro);
    // A registered edition shows no box.
    EndFixture r;
    r.w.w1e84 = 10000;
    r.w.w920c = 2;
    wave_deadlines(r.c);
    CHECK(r.host.calls.empty());
    CHECK(r.s.state() == shell::kHighScores);
}

TEST_CASE("seg9:0x2f82: the remaining icons pack into the directory slots") {
    World w;
    w.wave = 1;
    for (int k = 0; k < 8; ++k) w.pools.rec(0, k).state = 1;
    w.pools.rec(0, 1).state = 0;
    w.pools.rec(0, 1).damage = 11;
    w.pools.rec(0, 1).flame = 0xff;  // heat < 0
    w.pools.rec(0, 4).state = 0;
    w.pools.rec(0, 4).damage = 22;
    w.pools.rec(0, 4).b53 = 2;
    DirRecord& d = w.dir();
    d.set_byte(0x1c8 + 5, 7);
    d.set_word(0x1d2 + 10, 99);
    pack_remaining_icons(w);
    CHECK(d.word(0x1d2) == 11);
    CHECK(d.byte(0x1c8) == 2);
    CHECK(d.word(0x1d4) == 22);
    CHECK(d.byte(0x1c9) == 1);
    CHECK(d.byte(0x1c8 + 5) == 0);
    CHECK(d.word(0x1d2 + 10) == 0);
}

TEST_CASE("seg9:0x2cdb: the wave end writes credit / 10 and resources into the roster") {
    EndFixture f;
    f.w.roster.resize(1);
    f.w.ships[0].roster_index = 0;
    f.w.ships[0].wdc = static_cast<std::int16_t>(0x2345);
    f.w.ships[0].wde = 0x0006;  // 0x62345 > 0x4fff6: capped
    set_word_at(f.w.ships[0], 0xce, 321);
    wave_into_directory(f.c);
    CHECK(f.w.roster[0].credit10 == 0x4fff6 / 10);
    CHECK(f.w.roster[0].resources == 321);
    CHECK(static_cast<std::uint16_t>(f.w.ships[0].wdc) == 0xfff6);
    CHECK(f.w.ships[0].wde == 4);
    CHECK(f.w.player_roster == 0);
}
