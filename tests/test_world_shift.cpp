// Perspective shifting (src/world/shift.h, the pan of src/world/view.h; docs/re/ships.md
// "Perspective shifting"): the 'P' latch seg11:0x6dc4, seg7:0x7d5, seg11:0xaece with
// s3-roster-verify.md's correction 61 (no roster swap) and the pan seg8:0x65-0x247, worked from
// the listings cited at each check.
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "world/flight.h"
#include "world/shift.h"
#include "world/view.h"
#include "world/world.h"

using namespace is::world;

namespace {

struct TestShiftHost final : ShiftHost {
    std::vector<std::string> log;
    int target = 0x0302;  // pool 3, record 2
    void message(int recipient, int pool, int rec, int code) override {
        log.push_back("message " + std::to_string(recipient) + " " + std::to_string(pool) + " " +
                      std::to_string(rec) + " " + std::to_string(code));
    }
    int next_target() override {
        log.push_back("next_target");
        return target;
    }
    void set_target(int ship, int pool, int rec) override {
        log.push_back("set_target " + std::to_string(ship) + " " + std::to_string(pool) + " " + std::to_string(rec));
    }
    void lamps(int ship, int arg) override { log.push_back("lamps " + std::to_string(ship) + " " + std::to_string(arg)); }
    void weapon_slot() override { log.push_back("weapon_slot"); }
    void weapon_picture() override { log.push_back("weapon_picture"); }
    void black_cell(int x, int y, int w, int h) override {
        log.push_back("black_cell " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(w) + " " +
                      std::to_string(h));
    }
    void pin_player() override { log.push_back("pin_player"); }
    void draw_player() override { log.push_back("draw_player"); }
    void info_mode(int mode) override { log.push_back("info_mode " + std::to_string(mode)); }
};

// Ships 0 ... 3 in play and alive, all of team 0, design n = ship n, roster index 10 + n.
void team_wave(World& w) {
    w.player = 0;
    w.last_ship = 3;
    w.player_game = 1;
    for (int i = 0; i <= 3; ++i) {
        ShipRecord& s = w.ship(i);
        s.design_index = static_cast<std::int16_t>(i);
        s.roster_index = static_cast<std::int16_t>(10 + i);
        s.bec = 1;
        set_word_at(s, 0xd0, 100 + i);
        set_word_at(s, 0xd2, 200 + i);
        w.ship_object(i).x = static_cast<std::int16_t>(1000 + 100 * i);
        w.ship_object(i).y = static_cast<std::int16_t>(2000 + 100 * i);
    }
    w.player_roster = 10;
}

ViewState view() {
    ViewState v;
    v.play_w = 1024;
    v.play_h = 683;
    v.shift = 3;
    return v;
}

}  // namespace

TEST_CASE("shift: WM_KEYUP 'P' latches DGROUP:0x1032 in a player game outside edition 3") {
    World w;
    shift_words().latch = 0;
    w.player_game = 0;  // the demo [BYTES seg11:0x6d19-0x6d1e]
    shift_key_up(w);
    CHECK(shift_words().latch == 0);
    w.player_game = 1;
    w.w920c = 3;  // [BYTES seg11:0x6dc4-0x6dc9]
    shift_key_up(w);
    CHECK(shift_words().latch == 0);
    w.w920c = 1;
    shift_key_up(w);
    CHECK(shift_words().latch == 1);  // [BYTES seg11:0x6dcd]
    shift_words().latch = 0;
}

TEST_CASE("shift: seg11:0xaece makes the next teammate the player, the roster indices stay") {
    World w;
    team_wave(w);
    ViewState v = view();
    TestShiftHost h;
    WaveGlobals g;
    shift_words().latch = 1;
    w.w1e84 = 5;
    w.w1e80 = 6;
    w.design_ratings[1].captures = 0;
    CHECK(shift_pass(w, v, h, &g) == 1);
    CHECK(shift_words().latch == 0);
    CHECK(w.player == 1);
    CHECK(g.player == 1);
    // +0xd0 / +0xd2 exchanged [BYTES seg11:0xafa9-0xafda]
    CHECK(word_at(w.ship(0), 0xd0) == 101);
    CHECK(word_at(w.ship(1), 0xd0) == 100);
    CHECK(word_at(w.ship(0), 0xd2) == 201);
    CHECK(word_at(w.ship(1), 0xd2) == 200);
    // No roster swap: DGROUP:0x931c = the new body's own +0xd6 (correction 61)
    CHECK(w.ship(0).roster_index == 10);
    CHECK(w.ship(1).roster_index == 11);
    CHECK(w.player_roster == 11);
    CHECK(w.design_ratings[1].captures == 1);  // [BYTES seg11:0xaffe]
    CHECK(w.w1e84 == 0u);
    CHECK(w.w1e80 == 0u);
    CHECK(w.play_mode == 1);
    CHECK(g.play_mode == 1);
    CHECK(w.w951a == 1);
    CHECK(g.autopilot == 1);
    CHECK(v.cam_x == 1000);  // DGROUP:0x6b50 = the old ship's (x, y) [BYTES seg11:0xb05c-0xb070]
    CHECK(v.cam_y == 2000);
    // seg3:0x6ec is sent while DGROUP:0x9646 is still the old ship; then the target and the HUD
    const std::vector<std::string> want = {"message 0 14 1 113", "next_target", "set_target 1 3 2",
                                           "lamps 1 1",          "weapon_slot", "weapon_picture"};
    CHECK(h.log == want);
    // A second 'P' moves on to the next live teammate, ship 2; DGROUP:0x6b50 is not stored again
    // while DGROUP:0x951a is set.
    v.cam_x = 7;
    shift_words().latch = 1;
    CHECK(shift_pass(w, v, h, &g) == 1);
    CHECK(w.player == 2);
    CHECK(v.cam_x == 7);
}

TEST_CASE("shift: the search skips dead, out-of-play and other-team ships and wraps") {
    World w;
    team_wave(w);
    ViewState v = view();
    TestShiftHost h;
    w.player = 2;
    w.ship(3).bec = 0;                   // dead [BYTES seg11:0xaf51]
    w.ship_object(0).state = 100;        // off-world [BYTES seg11:0xaf44]
    w.design_ratings[1].team = 4;        // another team [BYTES seg11:0xaf6c-0xaf8b]
    CHECK(perspective_shift(w, v, h) == 0);  // back at the player [BYTES seg11:0xaf96-0xaf9c]
    CHECK(w.player == 2);
    CHECK(h.log.empty());
    w.design_ratings[1].team = 0;
    CHECK(perspective_shift(w, v, h) == 1);  // 3 dead, 0 off-world, then 1
    CHECK(w.player == 1);
}

TEST_CASE("shift: the early returns of seg11:0xaece") {
    World w;
    team_wave(w);
    ViewState v = view();
    TestShiftHost h;
    // A race or duel returns before the latch is cleared [BYTES seg11:0xaedc-0xaeec]
    w.kind = 7;
    shift_words().latch = 1;
    CHECK(shift_pass(w, v, h) == 0);
    CHECK(shift_words().latch == 1);
    w.kind = 0;
    // The player out of play: 0, latch cleared [BYTES seg11:0xaf0c-0xaf10]
    w.ship_object(0).state = 1;
    CHECK(shift_pass(w, v, h) == 0);
    CHECK(shift_words().latch == 0);
    w.ship_object(0).state = 0;
    // Demon's Gate [BYTES seg11:0xaf14-0xaf18]
    w.handover.demons_gate = 1;
    CHECK(perspective_shift(w, v, h) == 0);
    CHECK(w.player == 0);
    CHECK(h.log.empty());
}

TEST_CASE("pan: the view travels 96 units a frame to the new ship and arrives within 16 cells") {
    World w;
    team_wave(w);
    ViewState v = view();
    TestShiftHost h;
    WaveGlobals g;
    // The new player, ship 1, at (2600, 2000); the pan point at (1000, 2000): 200 cells apart.
    w.player = 1;
    w.w951a = 1;
    w.play_mode = 1;
    shift_words().saved_mode = 0;  // DGROUP:0x9378, the placement's PlayMode
    w.ship_object(1).x = 2600;
    w.ship_object(1).y = 2000;
    w.ship_object(1).scr_x = 512;
    w.ship_object(1).scr_y = 341;
    w.ship_object(1).size = 33;
    v.cam_x = 1000;
    v.cam_y = 2000;
    int frames = 0;
    for (; frames < 40 && w.w951a != 0; ++frames) {
        ViewInputs in;
        in.player_x = w.ship_object(1).x;
        in.player_y = w.ship_object(1).y;
        in.autopilot = w.w951a;
        in.world = &w;
        in.shift = &h;
        in.frame = &g;
        in.now = 1000;
        recentre(v, in);
        if (w.w951a != 0) {
            // (lo16(12 dx) / d) << 3 = 96 while dy = 0 [BYTES seg8:0x186-0x1a1]
            CHECK(v.pan_dx == 96);
            CHECK(v.pan_dy == 0);
            CHECK(v.x0 == v.cam_x - 4096);  // the pan point centred
            // The activity rectangles round the pan point while it is set [BYTES seg8:0x3e5]
            ViewState c2 = v;
            activity_rects(c2, v.cam_x, v.cam_y, 0, 0);
            CHECK(c2.active[0] == v.active[0]);
            CHECK(c2.active_count == v.active_count);
        }
    }
    // dx = 200, 188, ..., 20: 16 frames of 12 cells, the 17th arrives at dx = 8 < 16
    CHECK(frames == 17);
    CHECK(v.cam_x == 1000 + 16 * 96);
    CHECK(v.x0 == 2600 - 4096);  // the player centred [BYTES seg8:0xcf-0xef]
    CHECK(w.play_mode == 0);     // DGROUP:0x9378 [BYTES seg8:0x148]
    CHECK(g.play_mode == 0);
    CHECK(g.autopilot == 0);
    const std::vector<std::string> want = {"black_cell 512 341 33 33", "pin_player", "draw_player", "info_mode 2"};
    CHECK(h.log == want);
}

TEST_CASE("pan: PlayMode 2 and 3 make a player game on arrival, and the wrap of the pan point") {
    World w;
    team_wave(w);
    ViewState v = view();
    TestShiftHost h;
    w.player = 1;
    w.w951a = 1;
    w.player_game = 0;
    w.play_mode = 2;
    shift_words().saved_mode = 3;
    v.cam_x = w.ship_object(1).x;
    v.cam_y = w.ship_object(1).y;
    CHECK(pan_view(v, w, h, nullptr, 0) == 0);
    CHECK(w.player_game == 1);  // [BYTES seg8:0x13c-0x142]
    CHECK(w.play_mode == 3);    // then DGROUP:0x9378 in every case
    // The wrap: from x = 25590 toward a ship at 100 across the edge (the short way), the step goes
    // past 25600 and wraps to the left [BYTES seg8:0x1bb-0x1c7]
    w.w951a = 1;
    w.ship_object(1).x = 1500;
    w.ship_object(1).y = 2000;
    v.cam_x = 25590;
    v.cam_y = 2000;
    pan_view(v, w, h, nullptr, 0);
    CHECK(v.pan_dx == 96);
    CHECK(v.cam_x == static_cast<std::int16_t>(25590 + 96 - 25600));
}

TEST_CASE("pan: a new ship out of play arms the wave's deadlines; a dead one shifts again") {
    World w;
    team_wave(w);
    ViewState v = view();
    TestShiftHost h;
    w.player = 1;
    w.w951a = 1;
    v.cam_x = 1000;
    v.cam_y = 2000;
    w.ship_object(1).x = 9000;
    // state 1: seg11:0xaece returns 0 at once (the player is out of play), then the deadlines
    w.ship_object(1).state = 1;
    pan_view(v, w, h, nullptr, 500);
    CHECK(w.player == 1);
    CHECK(w.w1e80 == 2500u);  // [BYTES seg8:0x225-0x22f]
    CHECK(w.w1e84 == 2000u);  // [BYTES seg8:0x233-0x244]
    // Alive = 0 in play: the search runs and moves on to ship 2
    w.ship_object(1).state = 0;
    w.ship(1).bec = 0;
    w.w1e80 = w.w1e84 = 0;
    pan_view(v, w, h, nullptr, 500);
    CHECK(w.player == 2);
    CHECK(w.w1e80 == 0u);
}
