// The ambulance visit seg16:0xcfe (src/world/visit.h): the layout seg16:0x1f78, the typing
// seg16:0x2e3c (4 characters per 40 ms), the phase deadlines (2500 ms at phase 11, the 50 ms
// repair and refuel ticks, 800 ms per item, the 1300 ms wait before the dialog, 1000 ms before
// the launch), the repair step ending at 101 for an odd value (ADR 0014 decision 6), the charge
// into debt and the free spot of phase 64 (work/analysis/m4/s3-visit.md section 1 with
// s3-visit-verify.md; the oracle's core-0003).
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "hud/weapons.h"
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
    {2516, "BASIC REPAIRS"}, {2517, "WING REPAIR"}, {2527, "%D HAS NO RESOURCES..."},
    {2540, "REFUELING %D..."}};

struct Screen final : VisitScreen {
    std::string typed;
    std::vector<std::pair<int, int>> bars;  // (y, v)
    int scrolls = 0, redraws = 0;
    std::vector<std::pair<int, int>> frames;  // descent_frame's (y, flag)
    void landing_scene(const VisitLayout&, const std::string&) override {}
    int incoming(int) override { return 1; }
    void descent_start(const VisitLayout&) override {}
    void descent_frame(const VisitLayout&, int y, int, int flag) override { frames.push_back({y, flag}); }
    void redraw(const VisitLayout&, int, int, bool) override { ++redraws; }
    void ship_on_pad(const VisitLayout&, int, int, int, int) override {}
    void pad_strip(const VisitLayout&, int, int) override {}
    void rebuild_ship() override {}
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
    void left_monitor(const VisitLayout&, bool) override {}
    void black_screen() override {}
    void free_scene() override {}
};

struct Host final : VisitHost {
    std::uint32_t t = 100000;
    std::vector<int> busy, pumps, sounds;
    int dialogs = 0, state = 6, messages = 0;
    std::uint32_t now() override { return t; }
    std::uint32_t current_time() override { return t; }
    void busy_wait(int ms) override { busy.push_back(ms); }
    void pump_wait(int ms) override { pumps.push_back(ms); }
    void sound(int id) override { sounds.push_back(id); }
    std::vector<std::vector<int>> sounds_at;  // (id, a, b, mode)
    void sound_at(int id, int a, int b, int mode) override { sounds_at.push_back({id, a, b, mode}); }
    std::string lang(int id) override {
        const auto it = kStrings.find(id);
        return it == kStrings.end() ? std::string() : it->second;
    }
    std::string ship_name(int) override { return "THE TRANSFORMER"; }
    std::string pilot() override { return "YOUR NAME"; }
    bool ctrl_held() override { return false; }
    int w8f70() override { return 0; }
    int cal = 2000;
    int calibrate() override { return cal; }
    int depth() override { return 32; }
    bool limited() override { return true; }
    void options_dialog() override { ++dialogs; }
    void body_chooser() override {}
    void enter_calls() override {}
    void palette_back() override {}
    void launch_enter() override {}
    bool launch_frame() override { return true; }
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
    void todo(const std::string&, const char*) override {}
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
    while (r.v.descent == 1 && calls < 1000) {
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
    // One IGNITE at the change to 0, then sound 9 at the end; no clamp of the position.
    CHECK(r.h.sounds == std::vector<int>{0x20, 9});
    CHECK(r.v.pos10 > 10 * end);
    CHECK(r.v.descent == 2);
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
    CHECK(r.v.descent == 2);
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
