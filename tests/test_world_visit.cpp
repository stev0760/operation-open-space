// The ambulance visit seg16:0xcfe (src/world/visit.h): the layout seg16:0x1f78, the typing
// seg16:0x2e3c (4 characters per 40 ms), the phase deadlines (2500 ms at phase 11, the 50 ms
// repair and refuel ticks, 800 ms per item, the 1300 ms wait before the dialog, 1000 ms before
// the launch), the repair step ending at 101 for an odd value (ADR 0014 decision 6), the charge
// into debt and the free spot of phase 64 (work/analysis/m4/s3-visit.md section 1 with
// s3-visit-verify.md; the oracle's core-0003).
#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "desk/desk.h"
#include "doctest.h"
#include "hud/weapons.h"
#include "shell/frame_scheduler.h"
#include "world/ambulance.h"
#include "world/visit.h"
#include "world/world.h"

using namespace is::world;

namespace {

// [BYTES ENG_LANG.DLL STRING 73] ids 1154, 1157-1159 [BYTES ENG_LANG.DLL STRING 74] ids 1173-1180
// [BYTES ENG_LANG.DLL STRING 75] ids 1183-1184 [BYTES ENG_LANG.DLL STRING 20] ids 311-314 [BYTES
// ENG_LANG.DLL STRING 158] ids 2516-2517, 2527 [BYTES ENG_LANG.DLL STRING 159] id 2540.
const std::map<int, std::string> kStrings = {
    {311, "GOOD"}, {312, "FAIR"}, {313, "BAD"}, {314, "SORRY"},
    {1154, "REPAIRS COMPLETE."}, {1157, "%S REQUIRED"}, {1158, "YOUR CREDIT IS NOW %s ICONS. "},
    {1173, "TRANSFERRING %s ICONS... "}, {1174, "TRANSFER COMPLETE. "}, {1175, "%S IN PROGRESS.."},
    {1176, "REFUEL%< COMPLETE.%>"}, {1177, "REFUELING"}, {1179, "TOTAL COST OF WORK: %s ICONS."},
    {1180, "PREPARE FOR LAUNCH"}, {1183, "YOU'RE IN %S SHAPE, %s"}, {1184, "%D HAS LANDED.."},
    {2702, "THE "},
    {2516, "BASIC REPAIRS"}, {2517, "WING REPAIR"}, {2527, "%D HAS NO RESOURCES..."},
    {2540, "REFUELING %D..."}};

struct Screen final : VisitScreen {
    std::string typed;
    int incoming_ret = 1, lefts = 0;
    std::vector<std::pair<int, int>> bars;  // (y, v)
    int scrolls = 0, redraws = 0;
    std::vector<std::pair<int, int>> frames;  // descent_frame's (y, flag)
    std::optional<LandingSlots> landing_scene(const VisitLayout&, const std::string&, bool, bool) override {
        return std::nullopt;
    }
    std::vector<int> incoming_calls;  // seg15:0x3caa's `first` per call
    int incoming(int first) override {
        incoming_calls.push_back(first);
        return incoming_ret;
    }
    void descent_start(const VisitLayout&, bool, bool) override {}
    void descent_frame(const VisitLayout&, int y, int, int flag) override { frames.push_back({y, flag}); }
    void redraw(const VisitLayout&, int, int, bool) override { ++redraws; }
    void ship_on_pad(const VisitLayout&, int, int, int, int) override {}
    std::vector<std::pair<int, int>> strips;  // pad_strip's (offset, s4)
    void pad_strip(const VisitLayout&, int off, int s4) override { strips.push_back({off, s4}); }
    int rebuilds = 0;
    void rebuild_ship() override { ++rebuilds; }
    int body_s4 = -1;  // seg15:0x4c76's 4·trunc(bmHeight(DGROUP:0x92b0)/k); -1: the roster's
    int pad_s4(int roster_s4) override { return body_s4 < 0 ? roster_s4 : body_s4; }
    void bar(int, int y, int, int, int v) override { bars.push_back({y, v}); }
    void monitor_begin(int) override {}
    void monitor_end() override {}
    int text_extent(const std::string& s) override { return 6 * static_cast<int>(s.size()); }
    void glyph(int, int, char c) override { typed += c; }
    void black(int, int, int, int) override {}
    void block(int, int, int, int, std::uint32_t) override {}
    void scroll_up(int, int, int, int, int) override { ++scrolls; }
    std::vector<std::pair<int, int>> flashes;
    void flash(int x, int y) override { flashes.push_back({x, y}); }
    void save_monitor(int, int, int, int) override {}
    void left_monitor(const VisitLayout&, bool) override { ++lefts; }
    void black_screen() override {}
    void free_scene() override {}
};

struct Host final : VisitHost {
    std::uint32_t t = 100000;
    std::vector<int> busy, pumps, sounds;
    int dialogs = 0, state = 6, messages = 0;
    // With `clock` the waits pass on `t` (the timeline tests); every sound with its time.
    bool clock = false;
    std::vector<std::pair<int, std::uint32_t>> heard;
    std::uint32_t dialog_t = 0;
    std::uint32_t now() override { return t; }
    std::uint32_t current_time() override { return t; }
    void busy_wait(int ms) override {
        busy.push_back(ms);
        if (clock) t += static_cast<std::uint32_t>(ms);
    }
    void pump_wait(int ms) override {
        pumps.push_back(ms);
        if (clock) t += static_cast<std::uint32_t>(ms);
    }
    void sound(int id) override {
        sounds.push_back(id);
        heard.push_back({id, t});
    }
    std::vector<std::vector<int>> sounds_at;  // (id, a, b, mode)
    void sound_at(int id, int a, int b, int mode) override {
        sounds_at.push_back({id, a, b, mode});
        if (id != 0x2b) heard.push_back({id, t});
    }
    std::string lang(int id) override {
        const auto it = kStrings.find(id);
        return it == kStrings.end() ? std::string() : it->second;
    }
    std::string ship_name(int) override { return "THE TRANSFORMER"; }
    std::string pilot() override { return "YOUR NAME"; }
    bool ctrl = false;
    bool ctrl_held() override { return ctrl; }
    int w8f70() override { return 0; }
    int cal = 2000;
    int calibrate() override { return cal; }
    int depth() override { return 32; }
    bool limited() override { return true; }
    void options_dialog() override {
        ++dialogs;
        dialog_t = t;
    }
    void body_chooser() override {}
    void enter_calls() override {}
    int entry_steps = 0;  // seg7:0x158e calls
    void entry_step() override { ++entry_steps; }
    void palette_back() override {}
    // The launch: the arguments of phase 61's call, and the desk machine when one is attached.
    int launch_end_y = 0, launch_pad_w = 0, launch_pad_c = 0, launch_calls = 0;
    bool launch_done = true;
    std::function<void(const VisitLayout&, int)> on_launch_enter;
    std::function<bool()> on_launch_frame;
    void launch_enter(const VisitLayout& l, int end_y) override {
        launch_end_y = end_y;
        launch_pad_w = l.pad_w;
        launch_pad_c = l.pad_c;
        if (on_launch_enter) on_launch_enter(l, end_y);
    }
    bool launch_frame() override {
        ++launch_calls;
        return on_launch_frame ? on_launch_frame() : launch_done;
    }
    void recreate_dcs() override {}
    void seg18_7b2() override {}
    void to_screen(std::int16_t, std::int16_t, std::int16_t& sx, std::int16_t& sy) override { sx = sy = 0; }
    void watch_windows(int) override {}
    void cursor_home() override {}
    void music_back() override {}
    void message(int, int) override { ++messages; }
    void repaint_wave() override {}
    void set_state(int s) override { state = s; }
    void demon_end() override {}
    void demon_click() override {}
    void box_2322() override {}
    int rand() override { return 0; }
    std::vector<std::string> todos;
    void todo(const std::string& what, const char*) override { todos.push_back(what); }
};

struct Rig {
    World w;
    Shop s;
    VisitState v;
    Host h;
    Screen scr;
    WorldPurchaseHost ph{w};
    WeaponWords W;
    VisitCtx c{w, s, v, h, scr, ph, W};
    Rig() {
        w.W = 1024;
        w.H = 768;
        w.player = 0;
        w.w952e = 0;
        w.design_ratings[0].tank = 1320;
        w.design_ratings[0].special = 3;
        ShipRecord& S = w.ship(0);
        S.w26 = 74;
        S.w28 = 61;
        S.w2a = 74;
        S.w2c = 58;
        S.fuel = 1121;
        set_credit(S, 150);
        S.roster_index = 0;
        w.roster.resize(1);
        v.lay = visit_layout(w.W, w.H);
    }
    // Runs the frame until the phase changes, at most n calls.
    void until_phase_changes(int n = 1000) {
        const int p = v.phase;
        for (int i = 0; i < n && v.phase == p; ++i) visit(c, 0);
    }
};

}  // namespace

// seg15:0x374c, the descent (core.md "The descent", steps 1-5; sound.md "The descent") (M4 s4).
TEST_CASE("seg15:0x374c: the descent's first call, flag, speed, step and IGNITE") {
    Rig r;
    r.v.phase = 1;
    r.v.s4 = 72;
    // end = (768 - pad_c - 0x34) - 72 + pad_c - 10 = 634; start = end - 300 = 334.
    const int end = 768 - 0x34 - 72 - 10, start = end - 300;
    visit(r.c, 0);  // sub-step 0: seg15:0x374c(1)
    REQUIRE(r.v.descent == 1);
    CHECK(r.v.end_y == end);
    CHECK(r.v.start_y == start);
    // The first call's 150 falls to 145 and is clamped to 120 (C050); the flag stays 0; the
    // step at Calibrate >= 1300 with the flag 0 is 2·120 / 6 = 40 (C053, C054).
    CHECK(r.v.speed == 120);
    CHECK(r.v.flag == 0);
    CHECK(r.v.pos10 == 10 * start + 40);
    REQUIRE(r.scr.frames.size() == 1);
    CHECK(r.scr.frames[0] == std::pair<int, int>{start, 0});
    CHECK(r.h.sounds.empty());
    // Later calls: the speed falls by 5 to 50; the flag is 1 past (start + 60)·10, 0 past
    // (end - 60)·10 and 1 past (end - 10)·10.
    int calls = 0;
    std::vector<int> flags;
    while (r.v.descent == 1 && r.v.descent_due == 0 && calls < 1000) {
        const int before = r.v.pos10;
        const int sp = std::max(50, std::min(120, r.v.speed - 5));
        int f = r.v.flag;
        if ((end - 10) * 10 < before)
            f = 1;
        else if ((end - 60) * 10 < before)
            f = 0;
        else if ((start + 60) * 10 < before)
            f = 1;
        visit(r.c, 0);
        ++calls;
        CHECK(r.v.speed == sp);
        CHECK(r.v.flag == f);
        CHECK(r.v.pos10 == before + 2 * sp / (f == 0 ? 6 : 4));
        if (flags.empty() || flags.back() != f) flags.push_back(f);
    }
    CHECK(flags == std::vector<int>{0, 1, 0, 1});
    // One IGNITE at the change to 0, then sound 9 at the end and dword DGROUP:0x17f2 = now +
    // 1000; no clamp of the position.
    CHECK(r.h.sounds == std::vector<int>{0x20, 9});
    CHECK(r.v.pos10 > 10 * end);
    CHECK(r.v.descent_due == r.h.t + 1000);
}

TEST_CASE("seg15:0x374c: Calibrate below 1300 steps by 3 with the divisors 5 and 3") {
    Rig r;
    r.h.cal = 1000;
    r.v.phase = 1;
    visit(r.c, 0);
    const int start = r.v.start_y;
    CHECK(r.v.speed == 120);                  // 150 - 3, clamped
    CHECK(r.v.pos10 == 10 * start + 240 / 5);  // flag 0: d = 5
    visit(r.c, 0);
    CHECK(r.v.speed == 117);
    CHECK(r.v.pos10 == 10 * start + 48 + 234 / 5);
}

TEST_CASE("seg15:0x374c: DGROUP:0x928 sets the ship down at end + 1 with speed 0") {
    Rig r;
    r.v.phase = 1;
    r.v.fast = 1;
    visit(r.c, 0);
    const int end = r.v.end_y;
    CHECK(r.v.start_y == end + 1);
    CHECK(r.v.pos10 == 10 * (end + 1));  // the step adds 2·0 / 4
    CHECK(r.v.speed == 0);
    CHECK(r.v.flag == 1);  // (end - 10)·10 < pos
    visit(r.c, 0);         // returns 1 at once: 10·end < pos
    CHECK(r.v.descent_due == r.h.t + 1000);
    CHECK(r.h.sounds == std::vector<int>{9});
}

TEST_CASE("seg16:0x1f78 / seg16:0x27ea: the 1024 x 768 layout") {
    const VisitLayout l = visit_layout(1024, 768);
    CHECK(l.pad_w == 162);  // k = 4·768/3 - 1100 = -76, k/36 = -2 (idiv truncates)
    CHECK(l.panel_w == 260);
    CHECK(l.panel_h == 200);
    CHECK(l.mon_w == 230);
    CHECK(l.mon_h == 164);
    CHECK(l.rx == 680);  // min(78 + 512 + 90, W - panelW = 764)
    CHECK(l.ry == 297);         // min(307, 398) - 10
}

TEST_CASE("seg16:0x2e3c: four characters per 40 ms, then the NUL advances the phase") {
    Rig r;
    r.v.text = "ABCDEFG";
    r.v.phase = 3;
    visit_type(r.c, 1, 0);
    visit(r.c, 0);  // the 500 ms of the reset have not passed
    CHECK(r.scr.typed.empty());
    r.h.t += 500;
    visit(r.c, 0);
    CHECK(r.scr.typed == "ABCD");
    visit(r.c, 0);  // the next 40 ms not yet
    CHECK(r.scr.typed == "ABCD");
    r.h.t += 40;
    visit(r.c, 0);
    CHECK(r.scr.typed == "ABCDEFG");
    CHECK(r.v.phase == 4);  // the NUL in the same call
}

// The typing reset's CRT flash seg15:0x84b0 plays 0x14 at its point, mode 4, as the oracle's
// visits do 1.02 s after sound 9 (shell.md "CRT flash"; core.md the reset; amb.md D5) [BYTES
// seg15:0x84b0 seg16:0x2e79-0x2ecb] [ORACLE core-0003] (M4 s4 fixer B).
TEST_CASE("seg16:0x2e3c: the reset's CRT flash plays 0x14 at the monitor's middle") {
    Rig r;
    r.v.lay = visit_layout(1024, 768);
    visit_type(r.c, 1, 0);
    const VisitLayout& l = r.v.lay;
    const int x = l.mon_w / 2 + l.L() + 3, y = l.mon_h / 2 + l.T() + 3;
    REQUIRE(r.h.sounds_at.size() == 1u);
    CHECK(r.h.sounds_at[0] == std::vector<int>{0x14, x, y, 4});
    REQUIRE(r.scr.flashes.size() == 1u);
    CHECK(r.scr.flashes[0] == std::make_pair(x, y));
}

TEST_CASE("phase 11 waits 2500 ms after phase 10, then TRANSFER COMPLETE") {
    Rig r;
    r.w.ship(0).set_resources(100);
    r.v.phase = 10;
    visit(r.c, 0);
    CHECK(r.v.phase == 11);
    r.h.t += 2500;
    visit(r.c, 0);
    CHECK(r.v.phase == 11);  // not before now > deadline
    r.h.t += 1;
    visit(r.c, 0);
    CHECK(r.v.phase == 12);
    CHECK(r.v.text == "TRANSFER COMPLETE. ");
}

TEST_CASE("phase 13 moves the resources into the credit") {
    Rig r;
    r.w.ship(0).set_resources(100);
    r.v.phase = 13;
    visit(r.c, 0);
    CHECK(credit_of(r.w.ship(0)) == 250);
    CHECK(r.v.text == "YOUR CREDIT IS NOW 2.50 ICONS. ");
}

TEST_CASE("phase 15: 1300 ms, then the dialog; without purchases on to phase 22") {
    Rig r;
    r.v.phase = 15;
    visit(r.c, 0);
    CHECK(r.h.busy == std::vector<int>{1300});
    CHECK(r.h.dialogs == 1);
    CHECK(r.v.phase == 22);
}

TEST_CASE("phase 24: +2 per 50 ms tick, an odd value ends at 101 (ADR 0014 decision 6)") {
    Rig r;
    ShipRecord& S = r.w.ship(0);
    S.w26 = 99;
    S.w28 = 61;
    S.w2a = 100;
    r.s.basic = 1;
    r.v.phase = 24;
    int ticks = 0;
    while (r.v.phase == 24 && ticks < 100) {
        r.h.t += 51;  // one tick per 50 ms: due once now > the deadline
        visit(r.c, 0);
        ++ticks;
    }
    CHECK(S.w26 == 101);
    CHECK(S.w28 == 101);  // 61 + 2·20
    CHECK(S.w2a == 100);
    CHECK(r.v.phase == 25);
    CHECK(ticks == 21);  // 20 steps for 61 -> 101 and the tick that finds nothing to do
    CHECK(r.ph.roster_byte(0, 0x38) == 100);
}

TEST_CASE("phase 35: +16 per tick to the player's tank") {
    Rig r;
    r.s.refuel = 1;
    r.w.ship(0).fuel = 1300;
    r.v.phase = 35;
    r.h.t += 1;
    visit(r.c, 0);
    CHECK(r.w.ship(0).fuel == 1316);
    r.h.t += 51;
    visit(r.c, 0);
    CHECK(r.w.ship(0).fuel == 1320);
    r.h.t += 51;
    visit(r.c, 0);
    CHECK(r.v.phase == 36);
}

TEST_CASE("phases 40, 43, 47, 50, 53: 800 ms per item") {
    Rig r;
    r.v.phase = 40;
    visit(r.c, 0);
    CHECK(r.h.busy == std::vector<int>{800});
    r.v.phase = 43;
    visit(r.c, 0);
    CHECK(r.h.pumps == std::vector<int>{800});
    r.s.sold[1] = 1;
    r.v.weapon = 1;
    r.v.phase = 50;
    visit(r.c, 0);
    CHECK(r.h.busy.size() == 2);
    CHECK(r.v.phase == 48);
    CHECK(r.v.weapon == 2);
}

TEST_CASE("phase 58: the charge into debt, then phase 60's 1000 ms before the launch") {
    Rig r;
    set_credit(r.w.ship(0), 40);
    r.s.cost = 70;
    r.v.phase = 58;
    visit(r.c, 0);
    CHECK(credit_of(r.w.ship(0)) == -30);  // [ORACLE core-0003: 0xffffffe2]
    CHECK(r.s.cost == 0);
    CHECK(r.v.phase == 59);
    r.v.phase = 60;
    r.v.d1c48 = 0;
    visit(r.c, 0);  // arms GetCurrentTime + 1000
    CHECK(r.v.phase == 60);
    r.h.t += 1000;
    visit(r.c, 0);
    CHECK(r.v.phase == 60);
    r.h.t += 1;
    visit(r.c, 0);
    CHECK(r.v.phase == 61);
}

TEST_CASE("phase 4: the shape line; phase 6 asks for nothing when all is well") {
    Rig r;
    r.v.phase = 4;
    visit(r.c, 0);
    CHECK(r.v.text == "YOU'RE IN FAIR SHAPE, YOUR NAME. ");  // mean 66 [ORACLE core-0003]
    ShipRecord& S = r.w.ship(0);
    S.w26 = S.w28 = S.w2a = S.w2c = 100;
    S.fuel = 1314;
    r.v.phase = 6;
    visit(r.c, 0);
    CHECK(r.v.phase == 8);
    S.fuel = 200;
    r.v.phase = 6;
    visit(r.c, 0);
    CHECK(r.v.text == "REFUELING REQUIRED. ");
}

TEST_CASE("phase 64: the hand-back, the free spot and the purchases") {
    Rig r;
    r.w.roles.ships = kPoolShips;
    r.w.pools.rec(kPool7, 0).x = 5000;
    r.w.pools.rec(kPool7, 0).y = 5000;
    r.v.phase = 64;
    visit(r.c, 0);
    CHECK(r.h.state == 1);
    CHECK(r.w.ship_object(0).x == 5008);
    CHECK(r.w.ship_object(0).state == 1);
    CHECK(r.h.messages == 1);  // message 84 [BYTES seg16:0x889]
}

// ---- M4 stage 6, track PACING (work/analysis/m4/s6-pacing.md section 9 with s6-pacing-verify.md)

// seg16:0x1f78 / seg16:0x27ea / seg16:0x619e and seg16:0x2144's tail (verified claims 70-73).
TEST_CASE("seg16:0x27ea: the left monitor's record opens with DGROUP:0x9530 = 3 and DGROUP:0x958a cleared") {
    Rig r;
    visit_panels(r.c);
    CHECK(r.v.left_on == 1);
    CHECK(r.v.left_mode == 3);
    CHECK(r.v.clicked == 0);  // set by seg16:0x619e, cleared by seg16:0x2144's seg16:0x64fa
    // The Inner Demon visit (DGROUP:0x93a6 != 0): seg16:0x27ea stores nothing (verified claim 71).
    Rig d;
    d.v.demon = 1;
    d.v.left_on = 0;
    d.v.left_mode = 2;
    visit_panels(d.c);
    CHECK(d.v.left_on == 0);
    CHECK(d.v.left_mode == 2);
}

// seg16:0x64fa's tail writes DGROUP:0x958a = [bp-0x76] = 0 (verified claim 75): a click is drawn by
// the next frame's tail once, as one frame of the counters (verified claim 77; core-0005 run3's
// DGROUP:0x1e68 holds over a whole visit without one) [ORACLE core-0005].
TEST_CASE("seg16:0x64fa: a click reaches the tail once, and the redraw clears it too") {
    Rig r;
    r.v.left_on = 1;
    r.v.left_mode = 3;
    r.v.phase = 3;
    r.v.type_due = r.h.t + 10000;  // the typing waits
    visit_click(r.c, 0, 0, 0);     // outside the left monitor
    CHECK(r.v.clicked == 1);
    const auto frames = r.w.frames;
    visit(r.c, 0);
    CHECK(r.scr.lefts == 1);
    CHECK(r.v.clicked == 0);
    CHECK(r.w.frames == frames + 1);
    visit(r.c, 0);
    CHECK(r.scr.lefts == 1);
    CHECK(r.w.frames == frames + 1);
    // seg16:0xcfe(1), the repaint: seg16:0x2144 and its tail's seg16:0x64fa (verified claim 73).
    visit_click(r.c, 0, 0, 0);
    REQUIRE(r.v.clicked == 1);
    visit(r.c, 1);
    CHECK(r.scr.redraws == 1);
    CHECK(r.v.clicked == 0);
}

// seg15:0x3590 sub-step 1 holds while dword DGROUP:0x17f2 >= DGROUP:0x1e50 and calls seg15:0x374c
// no more; the frame that sees it passed sets DGROUP:0x92a2 = 2, the next raises the phase, and
// phase 2's handler runs on the one after (verified claims 16-18, 18 CORRECTED) [ORACLE
// core-0008: DGROUP:0x92a2 = 2 1.03 s after the last position, phase 2 26 ms later].
TEST_CASE("seg15:0x3590: sub-step 1 waits 1000 ms after sound 9; phase 2 two frames after DGROUP:0x92a2 = 2") {
    Rig r;
    r.v.phase = 1;
    r.v.fast = 1;   // the ship set down at once (seg15:0x3876)
    visit(r.c, 0);  // sub-step 0's end: seg15:0x374c(1)
    REQUIRE(r.v.descent == 1);
    CHECK(r.v.descent_due == 0);
    visit(r.c, 0);  // seg15:0x374c(0) returns 1: sound 9, DGROUP:0x17f2 = now + 1000
    REQUIRE(r.h.sounds == std::vector<int>{9});
    CHECK(r.v.descent == 1);
    CHECK(r.v.descent_due == r.h.t + 1000);
    const std::size_t calls = r.scr.frames.size();
    r.h.t += 1000;
    visit(r.c, 0);  // DGROUP:0x17f2 = DGROUP:0x1e50: not yet (`jae`)
    CHECK(r.v.descent == 1);
    CHECK(r.scr.frames.size() == calls);  // no seg15:0x374c while DGROUP:0x17f2 != 0
    r.h.t += 1;
    visit(r.c, 0);  // frame N: DGROUP:0x92a2 = 2
    CHECK(r.v.descent == 2);
    CHECK(r.v.phase == 1);
    visit(r.c, 0);  // frame N + 1: sub-step 2, DGROUP:0x914a = 2
    CHECK(r.v.phase == 2);
    CHECK(r.h.sounds_at.empty());
    visit(r.c, 0);  // frame N + 2: phase 2's handler, 1184 and the reset's 0x14
    CHECK(r.v.phase == 3);
    REQUIRE(r.h.sounds_at.size() == 1u);
    CHECK(r.h.sounds_at[0][0] == 0x14);
}

// DGROUP:0x952c is 0 from phase 0 (seg16:0xd85) until seg15:0x374c(1)'s seg16:0x1f78 stores
// seg16:0x619e()'s 1 (seg16:0x2e31), so Ctrl sets DGROUP:0x928 at phase 0's frame and at the
// sub-step 0 frames (the incoming jet or the fly-in), and not after the descent's first call
// (verified claims 68-72) [BYTES seg16:0xd44-0xd5e seg16:0xd83-0xd90 seg15:0x3859 seg16:0x2e31]
// [ORACLE core-0007: the fast flag in the poll of the state's change to 6, v4 and v5].
TEST_CASE("DGROUP:0x952c opens at the descent's first call: Ctrl counts at phase 0 and sub-step 0") {
    {  // Ctrl at phase 0's frame
        Rig r;
        r.v.descent = 2;  // as the previous visit left DGROUP:0x92a2
        visit_begin(r.c);
        r.h.ctrl = true;
        visit(r.c, 0);
        CHECK(r.v.phase == 1);
        CHECK(r.v.fast == 1);
        CHECK(r.v.left_on == 0);
    }
    {  // Ctrl during the incoming jet
        Rig r;
        r.v.descent = 2;
        visit_begin(r.c);
        visit(r.c, 0);  // phase 0
        CHECK(r.v.left_on == 0);
        CHECK(r.v.fast == 0);
        r.scr.incoming_ret = 0;
        visit(r.c, 0);  // sub-step 0, the jet still flying
        CHECK(r.v.descent == 0);
        r.h.ctrl = true;
        const std::size_t jets = r.scr.incoming_calls.size();
        visit(r.c, 0);
        CHECK(r.v.fast == 1);
        // seg15:0x3caa(0) returns 1 at once in the same frame: the descent starts (claim 14).
        CHECK(r.scr.incoming_calls.size() == jets);
        CHECK(r.v.descent == 1);
    }
    {  // Ctrl after the descent's first call: DGROUP:0x952c = 1 and DGROUP:0x92a2 = 1 block it
        Rig r;
        r.v.descent = 2;
        visit_begin(r.c);
        visit(r.c, 0);  // phase 0
        visit(r.c, 0);  // sub-step 0's end: seg15:0x374c(1), seg16:0x1f78
        CHECK(r.v.descent == 1);
        CHECK(r.v.left_on == 1);
        CHECK(r.v.left_mode == 3);  // seg16:0x64c8
        CHECK(r.v.clicked == 0);    // seg16:0x619e's 1, cleared by seg16:0x2144's seg16:0x64fa
        r.h.ctrl = true;
        visit(r.c, 0);
        CHECK(r.v.fast == 0);
    }
}

// seg15:0x3caa tests DGROUP:0x928 before anything else and returns 1 (verified claims 14-15):
// with Ctrl at phase 0's frame the landing calls no seg15:0x3caa(1), so no 0x1c and no jet
// [BYTES seg15:0x3cb9-0x3cc3 seg15:0x3cf9] (s6-pacing.md section 10).
TEST_CASE("seg15:0x3caa: the fast flag set at phase 0's frame skips the incoming jet") {
    Rig r;
    r.v.descent = 2;  // as the previous visit left DGROUP:0x92a2
    visit_begin(r.c);
    r.h.ctrl = true;
    visit(r.c, 0);  // phase 0: the Ctrl test runs before the phase switch (verified claim 68)
    CHECK(r.v.fast == 1);
    CHECK(r.scr.incoming_calls.empty());
    CHECK(r.v.phase == 1);
    Rig q;  // without Ctrl: seg15:0x3caa(1) at the landing
    q.v.descent = 2;
    visit_begin(q.c);
    visit(q.c, 0);
    CHECK(q.v.fast == 0);
    CHECK(q.scr.incoming_calls == std::vector<int>{1});
}

TEST_CASE("phase 61 enters the desk machine with the layout and DGROUP:0x92d6; phase 63 waits for step 9") {
    Rig r;
    r.v.phase = 61;
    r.v.end_y = 574;
    visit(r.c, 0);
    CHECK(r.v.text == "PREPARE FOR LAUNCH...");
    CHECK(r.h.launch_end_y == 574);
    CHECK(r.h.launch_pad_w == r.v.lay.pad_w);
    CHECK(r.h.launch_pad_c == r.v.lay.pad_c);
    CHECK(r.v.phase == 62);
    r.v.phase = 63;
    r.h.launch_done = false;
    visit(r.c, 0);
    visit(r.c, 0);
    CHECK(r.v.phase == 63);
    CHECK(r.h.launch_calls == 2);
    r.h.launch_done = true;  // step 9 raised DGROUP:0x914a (verified claims 39-40)
    visit(r.c, 0);
    CHECK(r.v.phase == 64);
}

TEST_CASE("phase 64 with DGROUP:0x93a6 = 1 leaves through the Inner Demon's exit") {
    Rig r;
    r.w.roles.ships = kPoolShips;
    ambulance_state().reentry_due = 0;
    r.v.demon = 1;
    r.v.phase = 64;
    visit(r.c, 0);
    CHECK(r.h.state == 9);
    CHECK(std::find(r.h.todos.begin(), r.h.todos.end(), "phase 64's Inner Demon exit (M6)") != r.h.todos.end());
    CHECK(ambulance_state().reentry_due == 0);  // no DGROUP:0x6da8 (verified claim 55, CORRECTED)
    CHECK(r.h.messages == 0);                   // no seg16:0x872
}

namespace {

// The desk machine's canvas and clock for the timeline test: the launch's drawing calls only.
struct LaunchCanvas final : is::desk::Canvas {
    int draws = 0;
    void palette_save(int, int) override {}
    void palette_scheme(int, int) override {}
    void palette_restore(int) override {}
    void palette_fade(int, int) override {}
    void scene(const is::desk::Render&) override {}
    void monitor(const is::desk::Rect&) override {}
    void tower(const is::desk::Rect&) override {}
    void meal(int, const is::desk::Rect&) override {}
    void scene_to_screen() override {}
    void screen_to_scene(int, int, int, int) override {}
    void scene_to_screen(int, int, int, int) override {}
    void black(int, int, int, int) override {}
    void block(int, int, int, int, is::desk::Colour) override {}
    void text_font(int, int) override {}
    void text_char(int, int, char) override {}
    int text_extent(const std::string&) override { return 0; }
    void scroll_up(const is::desk::Rect&, int) override {}
    void flash(int, int) override {}
    void make_pad(int, int) override {}
    int make_ship(const std::string&) override { return 33; }  // the TRANSFORMER's s
    void rise(int, int, int, int, int) override {}
    void free_rise() override {}
    void pad_to_screen(int, int) override {}
    void launch_frame(const is::desk::LaunchDraw&) override { ++draws; }
    void free_all() override {}
    bool pad_alive() override { return false; }
};
using VisitFake = Host;  // the fake above; inside LaunchHost, `Host` would name its base
struct LaunchHost final : is::desk::Host {
    explicit LaunchHost(VisitFake& v) : v_(v) {}
    is::desk::Tick now() override { return v_.t; }
    void sound(int id) override { v_.sound(id); }
    void sound_at(int id, int x, int y, int m) override { v_.sound_at(id, x, y, m); }
    void music(int) override {}
    void pump() override {}
    void pump_until(is::desk::Tick u) override {
        if (static_cast<std::int32_t>(u - v_.t) > 0) v_.t = u;
    }
    void busy_until(is::desk::Tick u) override { pump_until(u); }
    std::string lang(int) override { return {}; }
    std::string lang_text(int, int) override { return {}; }
    std::string ini(const char*, const char* d) override { return d; }
    int ini_int(const char*, int d) override { return d; }
    is::desk::Clock clock() override { return {}; }
    int rand() override { return 0; }
    int calibrate_value() override { return v_.cal; }
    is::desk::Conditions conditions() override { return {}; }
    is::desk::Render render(int, int, int) override { return {}; }
    void todo(const std::string&, const char*) override {}

private:
    VisitFake& v_;
};

// The program's loop for state 6 (src/app/main.cpp idle_step): a pass at each due tick, the
// pace from shell::pace_for_state with the visit's inputs, one seg16:0xcfe(0) per due frame
// (seg7:0x18f) [BYTES seg7:0x17e seg7:0x18f]. It logs the tick of each frame that changed the
// phase (the oracle's 5 ms watch of DGROUP:0x914a) and of each frame that typed.
struct Loop {
    Rig& r;
    is::shell::FrameScheduler s;
    int frames = 0;
    std::vector<std::pair<int, std::uint32_t>> phases;  // (the new phase, the frame's tick)
    std::vector<std::pair<std::size_t, std::uint32_t>> typing;  // (characters typed, the frame's tick)
    explicit Loop(Rig& rig) : r(rig) {}
    void pass() {
        const std::uint32_t now = r.h.t;
        s.set_pace(is::shell::pace_for_state(6, {visit_pace_gate(r.v), visit_pace_slow(r.v)}), now);
        if (!s.due(now)) {
            r.h.t = now + s.wait(now);
            return;
        }
        const int p = r.v.phase;
        const std::size_t typed = r.scr.typed.size();
        visit(r.c, 0);
        s.ran(now);
        ++frames;
        if (r.v.phase != p) phases.push_back({r.v.phase, now});
        if (r.scr.typed.size() != typed) typing.push_back({r.scr.typed.size() - typed, now});
    }
    // Passes until `done` holds (at most n frames).
    template <class F>
    void until(F done, int n = 20000) {
        const int f0 = frames;
        while (!done() && frames - f0 < n) pass();
    }
    // The tick of the frame that first set `phase`, 0 when none did.
    std::uint32_t at(int phase) const {
        for (const auto& e : phases)
            if (e.first == phase) return e.second;
        return 0;
    }
};

std::uint32_t heard_at(const Host& h, int id, int nth = 0) {
    for (const auto& e : h.heard)
        if (e.first == id && nth-- == 0) return e.second;
    return 0;
}

}  // namespace

// The trial visit from the state's change to 6 to the dialog and on to phase 64 at the screen
// pace (ruling R1: every state-6 frame at seg7:0x2206's 72 Hz, 32 Hz while DGROUP:0x914a = 1
// and DGROUP:0x92a2 = 0) with Calibrate 14000 (ADR 0001 decision 9), the fast machine's branches:
// 134 descent calls with IGNITE at call 100, 122 rise calls. The oracle's visits take the slow
// branches (Calibrate -27858 in core-0007, -28267 in core-0008): 96 descent calls with IGNITE at
// call 70 and 57 rise calls [ORACLE core-0007] [ORACLE core-0008]. The ship is core-0007 v1's:
// systems 90 87 90 87, fuel 1320 of 1320, credit 1.50, no resources, nothing bought.
TEST_CASE("state 6 at the screen pace: the descent, the waits, the typing, the dialog, the launch") {
    Rig r;
    r.h.clock = true;
    r.h.cal = 14000;
    r.v.s4 = 132;  // the TRANSFORMER's 132-px frame 0 [ORACLE core-0007: 574/274 at the descent]
    ShipRecord& S = r.w.ship(0);
    S.w26 = 90;
    S.w28 = 87;
    S.w2a = 90;
    S.w2c = 87;
    S.fuel = 1320;
    r.w.roles.ships = kPoolShips;
    r.w.pools.rec(kPool7, 0).x = 5000;
    r.w.pools.rec(kPool7, 0).y = 5000;
    LaunchCanvas lc;
    LaunchHost lh(r.h);
    is::desk::Desk desk(lc, lh);
    r.h.on_launch_enter = [&](const VisitLayout& l, int end_y) { desk.enter_visit(r.h.t, l.W, l.H, l.pad_w, l.pad_c, end_y); };
    r.h.on_launch_frame = [&] { return desk.frame(r.h.t); };
    const std::uint32_t t0 = r.h.t;
    visit_begin(r.c);  // seg7:0x5fa-0x608: phase 0, seg16:0xcfe(1)
    Loop loop(r);
    // The descent: 134 seg15:0x374c calls, the IGNITE change at call 100, at 72 Hz.
    loop.until([&] { return r.h.sounds.size() >= 1 && r.h.sounds.back() == 9; });
    CHECK(r.scr.frames.size() == 134u);
    int ignite_call = 0;
    for (std::size_t i = 1; i < r.scr.frames.size(); ++i)
        if (r.scr.frames[i].second == 0 && r.scr.frames[i - 1].second == 1) ignite_call = static_cast<int>(i) + 1;
    CHECK(ignite_call == 100);  // the first call is call 1 (s6-pacing.md section 4)
    const std::uint32_t t20 = heard_at(r.h, 0x20), t9 = heard_at(r.h, 9);
    // 34 calls on the 72 Hz grid; the oracle's paced descent (slow branch) takes 26 calls from
    // 0x20 to 9 in 0.368 s [ORACLE core-0008 sky: 0x20 53.172, 0x09 53.540].
    CHECK(t9 - t20 == 472);
    // Phase 15, then the dialog after its 1300 ms busy-wait [ORACLE core-0007: 1.29 - 1.31 s]
    // [ORACLE core-0008: 1.30 - 1.32 s].
    loop.until([&] { return r.h.dialogs > 0; });
    REQUIRE(r.h.dialogs == 1);
    const std::uint32_t t15 = loop.at(15);
    REQUIRE(t15 != 0);
    CHECK(r.h.dialog_t - t15 >= 1300);
    CHECK(r.h.dialog_t - t15 <= 1300 + 14);
    // Typing: 4 characters per tick, the ticks 41 or 42 ms apart on the 72 Hz grid (the 40 ms
    // deadline met on the third frame), twice that across a wrap or a line's NUL, which end
    // their tick without a character [BYTES seg16:0x2f98-0x304a] [ORACLE core-0007: 4 per
    // tick, 41 - 47 ms].
    REQUIRE(loop.typing.size() > 10);
    int fours = 0;
    for (std::size_t i = 1; i < loop.typing.size(); ++i) {
        CHECK(loop.typing[i].first <= 4u);
        if (loop.typing[i].first == 4u) ++fours;
        const std::uint32_t gap = loop.typing[i].second - loop.typing[i - 1].second;
        CHECK((gap == 41 || gap == 42 || gap == 83 || gap == 84));
    }
    CHECK(fours * 10 > static_cast<int>(loop.typing.size()) * 7);
    const std::uint32_t t14 = heard_at(r.h, 0x14);
    MESSAGE("state 6 -> phase 1 " << (loop.at(1) - t0) << " ms, -> 0x20 " << (t20 - t0) << " ms, -> 9 " << (t9 - t0)
                                    << " ms, -> phase 2 " << (loop.at(2) - t0) << " ms, -> 0x14 " << (t14 - t0)
                                    << " ms, -> phase 15 " << (t15 - t0) << " ms, -> dialog " << (r.h.dialog_t - t0)
                                    << " ms; 0x14 -> phase 15 " << (t15 - t14) << " ms");
    // On to phase 64.
    loop.until([&] { return r.h.state == 1; });
    REQUIRE(r.h.state == 1);
    // Phase 60's 1000 ms (DGROUP:0x1c48 = GetCurrentTime + 1000) [ORACLE core-0007: 1.00 - 1.03 s].
    const std::uint32_t t60 = loop.at(60), t61 = loop.at(61);
    CHECK(t61 - t60 >= 1000);
    CHECK(t61 - t60 <= 1000 + 28);
    // Phase 61: 0x4b, then 1550 ms of seg10:0x740c with nothing drawn; phase 62's first
    // characters on the next frame, which is late and runs at once [ORACLE core-0007: 0x4b
    // 41.890, phase 62 43.444, 0x2b 43.455].
    const std::uint32_t t4b = heard_at(r.h, 0x4b);
    std::uint32_t t2b = 0;
    for (const auto& e : loop.typing)
        if (e.second > t4b) {
            t2b = e.second;
            break;
        }
    CHECK(t2b - t4b == 1550);
    // The second 0x1c: the first is seg15:0x3caa(1)'s at the landing (M4 s6 PAD, binding 7).
    const std::uint32_t t20b = heard_at(r.h, 0x20, 1), t1c = heard_at(r.h, 0x1c, 1), t15b = heard_at(r.h, 0x15);
    // The launch: 122 seg15:0x0 calls at the fast branch (55 in the oracle's paced launches).
    CHECK(lc.draws == 122);
    CHECK(t1c - t20b >= 951);
    CHECK(t1c - t20b <= 951 + 14);  // the 950 ms on the 72 Hz grid [ORACLE core-0008: 0.956 - 0.968 s]
    CHECK(t15b - t1c >= 120 * 1000 / 72);
    CHECK(t15b - t1c <= 120 * 1000 / 72 + 14);
    // Phase 64's DGROUP:0x6da8 = DGROUP:0x1e50 + 3000, the re-entry's deadline [ORACLE core-0007:
    // 0x27 3.02 - 3.07 s after the 0x14 and 0x15].
    const std::uint32_t t64 = loop.at(64);
    REQUIRE(t64 != 0);
    CHECK(ambulance_state().reentry_due == r.h.t + 3000u);
    CHECK(r.h.t - t64 <= 2 * 14);  // phase 64 runs one or two frames after the 0x15
    MESSAGE("dialog -> 0x4b " << (t4b - r.h.dialog_t) << " ms, 0x4b -> 0x20 " << (t20b - t4b) << " ms, 0x20 -> 0x1c "
                              << (t1c - t20b) << " ms, 0x1c -> 0x15 " << (t15b - t1c) << " ms, frames " << loop.frames);
}

// Review H3: seg16:0x2144 calls seg7:0x158e 120 times at the descent's first redraw while the back
// buffer DGROUP:0x9580 exists, with DGROUP:0x93a6 = 0 [BYTES seg16:0x2764-0x2775].
TEST_CASE("seg16:0x2144: 120 calls of seg7:0x158e at the descent's first call, none for the demon") {
    Rig r;
    r.v.phase = 1;
    r.v.s4 = 132;
    visit(r.c, 0);  // sub-step 0
    REQUIRE(r.v.descent == 1);
    CHECK(r.h.entry_steps == 120);
    visit(r.c, 0);  // later descent calls make none
    CHECK(r.h.entry_steps == 120);
    Rig d;
    d.v.phase = 1;
    d.v.s4 = 132;
    d.v.demon = 1;
    visit(d.c, 0);
    CHECK(d.h.entry_steps == 0);
}

// Compare item 1: phase 0's s4 reads the roster's +0x7a (size) and +0x7b (gap), which seg14:0x11a8
// writes back (5 for an odd size): 4·(33 + trunc((5 - 6)/2)) = 132, the oracle's DGROUP:0x92d8 -
// DGROUP:0x92d6 [BYTES seg16:0xdc1-0xdea seg14:0x1337] [ORACLE core-0007].
TEST_CASE("phase 0: s4 from the roster's size and gap bytes") {
    Rig r;
    r.w.roster[0].size = 33;
    r.w.roster[0].gap = 5;
    CHECK(r.ph.roster_byte(0, 0x7b) == 5);
    r.v.phase = 0;
    visit(r.c, 0);
    CHECK(r.v.s4 == 132);
    // set_roster_byte reaches the record's own byte (+0x72 is the left wing, records.h)
    r.ph.set_roster_byte(0, 0x72, 0);
    CHECK(r.w.roster[0].wing0 == 0);
    r.ph.set_roster_byte(0, 0x7b, 6);
    CHECK(r.w.roster[0].gap == 6);
}

// Review H1 and M6: seg15:0x4c76 takes s4 from the body image on every call (DGROUP:0x181c's
// bound s4 + 0x3e), the move down by 2 to the bound and up by 2 to 5, the phase raised at each
// end, sound 5 on first calls only, and the new body's rebuild with its 2000 ms on the first
// upward call [BYTES seg15:0x4c8b-0x4ce7 seg15:0x4cec-0x4d1c seg15:0x4dab-0x4dd1
// seg15:0x5238-0x52d8].
TEST_CASE("seg15:0x4c76: the pad move's s4 is the body image's; down to s4 + 0x3e, up to 5") {
    Rig r;
    r.v.s4 = 120;        // the roster's
    r.scr.body_s4 = 132;  // the body image's
    r.v.phase = 17;
    visit_pad(r.c, 1, 1);  // the first downward call
    CHECK(r.v.pad_max == 132 + 0x3e);
    REQUIRE(r.scr.strips.size() == 1);
    CHECK(r.scr.strips[0] == std::pair<int, int>{0, 132});
    CHECK(std::count(r.h.sounds.begin(), r.h.sounds.end(), 5) == 1);
    int calls = 0;
    while (r.v.phase == 17 && calls < 1000) {
        visit_pad(r.c, 0, 1);
        ++calls;
    }
    // 0 -> 2 on the first call, then 96 more steps of 2 to 194, and one call at the bound
    CHECK(calls == (132 + 0x3e) / 2);
    CHECK(r.v.pad_off == 132 + 0x3e);
    CHECK(r.scr.strips.back().first == 132 + 0x3e);
    CHECK(std::count(r.h.sounds.begin(), r.h.sounds.end(), 5) == 1);
    // the first upward call with a new body rebuilds the ship and waits 2000 ms
    r.s.new_body = 1;
    r.v.phase = 20;
    visit_pad(r.c, 1, 0);
    CHECK(r.scr.rebuilds == 1);
    CHECK(std::count(r.h.busy.begin(), r.h.busy.end(), 2000) == 1);
    CHECK(std::count(r.h.sounds.begin(), r.h.sounds.end(), 5) == 2);
    CHECK(r.v.pad_off == 132 + 0x3e - 2);
    calls = 0;
    while (r.v.phase == 20 && calls < 1000) {
        visit_pad(r.c, 0, 0);
        ++calls;
    }
    CHECK(r.v.pad_off == 5);
    CHECK(r.scr.rebuilds == 1);
}
