// The wave's update (src/world/update.h; docs/re/world.md "Update loop", "Rocks"; timing.md
// "Wave passes", "Game time"): the pass and frame calls in address order, the counters, the
// ship entry seg7:0x1d0c and the rock mover seg7:0x3bae.
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/update.h"

using namespace is::world;

namespace {

class Recorder : public UpdateHost {
public:
    std::vector<std::string> calls;
    int beams = 0, sprites = 0, fires = 0;
    std::vector<int> marks;
    std::vector<std::int16_t> drawn;
    void fuel_mass() override { calls.push_back("1bcc"); }
    void turn_followup() override { calls.push_back("894"); }
    void radar_refresh() override { calls.push_back("3396"); }
    void fire(int) override { ++fires; }
    std::int16_t beam(int, int) override { return static_cast<std::int16_t>(++beams); }
    void burst() override { calls.push_back("375a"); }
    void stars(int) override { calls.push_back("158a"); }
    void visibility() override { calls.push_back("4e14"); }
    void lines() override { calls.push_back("3778"); }
    void direction_mark(int d) override { marks.push_back(d); }
    void sound_frame() override { calls.push_back("7b48"); }
    void mover(Mover m) override { calls.push_back(mover_address(m)); }
    void player_sprite() override { ++sprites; }
    void draw_sprite(std::int16_t pr, int, int, int) override { drawn.push_back(pr); }
    void seg13_447a() override { calls.push_back("447a"); }
    std::uint16_t joy_rc = 0;
    int joy_reads = 0;
    std::uint16_t joy_read() override { return ++joy_reads, joy_rc; }
};

struct Rig {
    Pools pools;
    ShipTable ships{};
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    Recorder host;
    is::shell::Rng rng;
    JoyState joy;
    JoyButtons jb;
    UpdateCtx c{FlightCtx{pools, ships, g, keys, host, rng}, f, view, host, joy, jb};

    Rig() {
        g.now = 50000;
        view.play_w = 1024;
        view.play_h = 683;
        view.shift = 3;
        ShipRecord& s = ships[0];
        s.thrust = 60;
        s.drag = 10;
        s.fuel = 600;
        s.w2a = 100;
        s.roster_index = -1;
        s.has_flame = 1;
        s.bec = 1;
        s.turn_acc = 100;
        ObjectRecord& o = pools.rec(kPoolShips, 0);
        o.mass = 50;
        o.x = 12800;
        o.y = 12800;
        o.extent = 256;
        pools.set_in_use(kPoolRocks, 0);
        pools.set_in_use(kPoolIcons, 0);
    }
};

std::size_t index_of(const std::vector<std::string>& v, const std::string& s) {
    for (std::size_t i = 0; i < v.size(); ++i)
        if (v[i] == s) return i;
    return v.size();
}

}  // namespace

TEST_CASE("update: the pass and the frame in address order") {
    Rig r;
    wave_pass(r.c, false);
    wave_frame(r.c);
    const auto& c = r.host.calls;
    // The pass: the turn follow-up and the radar before a due frame, then the burst.
    CHECK(index_of(c, "1bcc") < index_of(c, "894"));
    CHECK(index_of(c, "894") < index_of(c, "3396"));
    CHECK(index_of(c, "3396") < index_of(c, "375a"));
    // 5 x (3 + 2) beam calls (world.md step 4)
    CHECK(r.host.beams == 25);
    CHECK(r.f.beam_passes == 5);
    // The frame: stars, (view), visibility, lines, sound, the movers in their order, seg13:0x447a.
    const std::vector<std::string> order = {"158a", "4e14", "3778", "7b48", "seg12:0x532c",
                                            "seg12:0x633e", "seg8:0x8256", "seg7:0x309c",
                                            "seg7:0x32d0", "seg7:0x40c4", "seg12:0x0",
                                            "seg6:0x73f6", "seg6:0x7c14", "seg12:0x3148",
                                            "seg12:0x6b1e", "seg12:0x3e98", "447a"};
    for (std::size_t i = 1; i < order.size(); ++i) {
        INFO(order[i - 1] << " before " << order[i]);
        CHECK(index_of(c, order[i - 1]) < index_of(c, order[i]));
    }
    CHECK(index_of(c, "seg7:0x5e04") == c.size());  // DGROUP:0x9456 = 0
    CHECK(index_of(c, "seg12:0xae4") == c.size());  // DGROUP:0x9600 = 0
    CHECK(r.g.frames == 1);
    CHECK(r.g.game_frames == 1);
    CHECK(r.g.cycle == 1);
    CHECK(r.f.dg_9142 == 1);
    // The view followed the player before the ships moved.
    CHECK(r.view.x0 == 12800 - 4096);
}

TEST_CASE("update: the turn follow-up waits for a turn or a due frame") {
    Rig r;
    r.g.frames = 1;
    wave_pass(r.c, true);  // FrameRateSet > 1000 and frames % 3 != 0: no follow-up
    CHECK(index_of(r.host.calls, "1bcc") == r.host.calls.size());
    r.g.turned = 1;
    wave_pass(r.c, true);
    CHECK(index_of(r.host.calls, "1bcc") < r.host.calls.size());
    CHECK(r.g.turned == 0);
}

TEST_CASE("update: the fire poll, the OverThruster re-arm and the counters") {
    Rig r;
    r.keys.key_down(kVkSpace, false);
    r.keys.key_up(kVkSpace);
    wave_pass(r.c, false);
    CHECK(r.host.fires == 1);  // the tap fires once [DESIGN ADR 0013 decision 6]
    wave_pass(r.c, false);
    CHECK(r.host.fires == 1);
    r.ships[0].overthrust_cool = 8;
    for (int i = 0; i < 8; ++i) wave_frame(r.c);
    CHECK(r.ships[0].overthrust_cool == 7);  // once every 8 frames
    for (int i = 0; i < 54 - 8 + 1; ++i) wave_frame(r.c);
    CHECK(r.f.dg_9142 == 0);  // 0 ... 54
    CHECK(r.g.game_frames == 55);
}

TEST_CASE("update: the information displays every 280 ms") {
    Rig r;
    r.f.info_mode = 2;
    r.f.info_due = 0;
    wave_frame(r.c);
    CHECK(r.host.marks == std::vector<int>{0});
    CHECK(r.f.info_due == r.g.now + 280);
    wave_frame(r.c);
    CHECK(r.host.marks.size() == 1);
    r.g.now += 281;
    r.f.info_mode = 3;
    r.f.dg_95fc = 1;
    wave_frame(r.c);
    CHECK(r.host.marks == std::vector<int>{0, 1, 2});
}

TEST_CASE("update: the player enters at the first gate") {
    Rig r;
    // Gate 0's posts: (5000, 9000) and (5000 + 272 + 1200, 9000), extent 272 (world.md "Gates").
    r.pools.rec(kPoolGatePosts, 0).x = 5000;
    r.pools.rec(kPoolGatePosts, 0).y = 9000;
    r.pools.rec(kPoolGatePosts, 0).extent = 272;
    r.pools.rec(kPoolGatePosts, 1).x = 5000 + 272 + 1200;
    r.pools.rec(kPoolGatePosts, 1).y = 9000;
    ObjectRecord& p = r.pools.rec(kPoolShips, 0);
    p.state = 100;
    p.vy = -30;
    r.ships[0].d88.set(r.g.now);  // due when now has passed it
    r.f.entry_pending = 1;
    wave_frame(r.c);
    CHECK(p.state == 100);  // +0x88 < now is strict
    r.g.now += 1;
    const std::uint32_t before = r.rng.state();
    wave_frame(r.c);
    CHECK(r.rng.state() != before);  // rand() % gates is drawn even for the player
    // x = post x + (1472 / 2 - 256 / 2 + 272 / 2), y = post y; then the ship moves by v.
    CHECK(p.x == 5000 + 736 - 128 + 136);
    CHECK(p.y == 9000 - 30);
    CHECK(p.state == 0);
    CHECK(r.f.entry_pending == 0);
}

TEST_CASE("update: the entry pushes back only the first later waiting ship") {
    // world.md "Ship entry (seg7:0x1d0c)" step 2: when ship i is in state 100, the first later
    // ship j with state 100 gets +0x88 = now + 5000 and no other ship does (M3 s3 fixer, C036)
    // [BYTES seg7:0x1d4c seg7:0x1d70 seg7:0x1d78].
    Rig r;
    r.g.last_ship = 3;
    r.pools.rec(kPoolGatePosts, 0).x = 5000;
    r.pools.rec(kPoolGatePosts, 0).y = 9000;
    r.pools.rec(kPoolGatePosts, 0).extent = 272;
    r.pools.rec(kPoolGatePosts, 1).x = 5000 + 272 + 1200;
    r.pools.rec(kPoolGatePosts, 1).y = 9000;
    for (int i = 0; i <= 3; ++i) {
        r.pools.rec(kPoolShips, i).state = 100;
        r.ships[static_cast<std::size_t>(i)].d88.set(i == 0 ? 0u : 900000u);
    }
    r.pools.rec(kPoolShips, 1).state = 2;  // not waiting: passed over by the j search
    r.f.entry_pending = 1;
    wave_frame(r.c);
    CHECK(r.pools.rec(kPoolShips, 0).state == 0);
    CHECK(r.ships[1].d88.get() == 900000u);
    CHECK(r.ships[2].d88.get() == r.g.now + 5000u);
    CHECK(r.ships[3].d88.get() == 900000u);
}

TEST_CASE("update: the joystick read and the unplugged result") {
    // world.md "Joystick", "Read" (M3 s3 fixer, C039) [BYTES seg11:0x72c2 seg11:0x72ce
    // seg11:0x72d3]: every 50 ms, only while the player's byte +0xe2 is 0; 0xa7 clears JoyStick.
    Rig r;
    r.g.joystick = 1;
    r.ships[0].be2 = 1;
    wave_frame(r.c);
    CHECK(r.host.joy_reads == 0);
    CHECK(r.f.joy_due == r.g.now + 50u);
    r.ships[0].be2 = 0;
    r.g.now += 51;
    wave_frame(r.c);
    CHECK(r.host.joy_reads == 1);
    CHECK(r.g.joystick == 1);
    wave_frame(r.c);  // not due yet
    CHECK(r.host.joy_reads == 1);
    r.host.joy_rc = 0xa7;
    r.g.now += 51;
    wave_frame(r.c);
    CHECK(r.host.joy_reads == 2);
    CHECK(r.g.joystick == 0);
}

TEST_CASE("update: rocks on and off screen") {
    Rig r;
    r.pools.set_in_use(kPoolRocks, 2);
    ObjectRecord& a = r.pools.rec(kPoolRocks, 0);
    a.on_screen = 1;
    a.w1c = 1;  // no collision test
    a.x = 1000;
    a.y = 1000;
    a.vx = -10;
    a.vy = 20;
    a.angle = 1790;
    a.size = 22;
    a.gap = 2;
    ObjectRecord& b = r.pools.rec(kPoolRocks, 1);
    b.x = 2000;
    b.y = 2000;
    b.vx = 5;
    b.vy = 5;
    r.f.dg_9142 = 0;  // the frame makes it 1: off-screen rocks wait
    wave_frame(r.c);
    CHECK(a.x == 990);
    CHECK(a.y == 1020);
    CHECK(a.frame == 17);           // 1790 / 100, before the spin
    CHECK(a.src_x == 24 * 17 + 2);  // (size + gap) * frame + gap
    CHECK(a.angle == 1760);         // + (|vx| + |vy|) with the sign of vx
    CHECK(r.host.drawn == std::vector<std::int16_t>{pool_rec(kPoolRocks, 0)});
    CHECK(b.x == 2000);
    wave_frame(r.c);  // 9142 = 2
    CHECK(b.x == 2000);
    wave_frame(r.c);  // 9142 = 3: three steps
    CHECK(b.x == 2015);
    CHECK(b.y == 2015);
}
