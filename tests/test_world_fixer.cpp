// The M3 stage 2 fixer's cases: the oracle's wave 1 from seed 37816 (probe-0001's wave dump),
// the visibility pass seg8:0x4e14, the rock images seg10:0x3a2e, the status tick seg11:0x1bcc,
// the player's flight design, the B and M keys and the latch at the wave start. Portable.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "hud/status.h"
#include "nav/fs.h"
#include "shell/rng.h"
#include "world/flight.h"
#include "world/input.h"
#include "world/rock_images.h"
#include "world/setup.h"
#include "world/sprite.h"
#include "world/visibility.h"

using namespace is::world;
namespace nav = is::nav;

namespace {

// ExtractIcon on the fixture drive: one icon per program, not blank.
class OneIcon : public IconReader {
public:
    int count(const std::string&) override { return 1; }
    std::optional<is::fmt::IconCell> cell(const std::string&, int) override {
        is::fmt::IconCell c;
        c.bgra[33] = 0xffffffffu;
        return c;
    }
};

// The oracle's game start on oracle/fixtures `basic` as D: (newgame-0002, probe-0001): 1024 x
// 768 full screen, so W = 1024 and H = 768 - 85 = 683; the INI keys Size, Detail and Complex
// at their defaults 1, 3 and 0; Enter on D:\GAMES\ (ALPHA, BRAVO, CHARLIE), a new directory.
struct OracleRig {
    nav::MemFs fs;
    nav::Walker walker{fs};
    OneIcon icons;
    is::shell::Rng rng;
    World w;
    SetupHost host{rng, walker, icons};
    OracleRig() {
        fs.add_drive('D');
        fs.add_dir("D:\\GAMES");
        for (const char* n : {"D:\\GAMES\\ALPHA.EXE", "D:\\GAMES\\BRAVO.EXE", "D:\\GAMES\\CHARLIE.EXE"})
            fs.add_file(n);
        host.now = [] { return 1000u; };
        power_on(w);
        w.W = 1024;
        w.H = 683;
        w.size = 1;
        w.shift = 3;
        w.detail = 3;
        w.complex = 0;
        w.drive_offset = 1;
        w.handover.directory = "D:\\GAMES\\";
        w.added = 1;  // Enter added the record: DGROUP:0x94b0 through both runs
        w.record = 0;
        w.dir();
    }
};

}  // namespace

// The probe's wave dump holds DGROUP:0x6678 = 6424, 14032, 7624, 14080 (gate 0's beam),
// DGROUP:0x992 = 50, DGROUP:0x94e2 = 38, DGROUP:0x9332 = 2, DGROUP:0x933a/0x933c = 24/14,
// DGROUP:0x940e = 3 and the in-use counts 38 rocks, 2 gate posts, 2 centres, 1 ship
// [ORACLE probe-0001 work/probe/dumps/wave-inspace.bin]. Seed 37816 at seg2:0x1b6b, the
// state-1 load (with DGROUP:0x9346 = 0), seg9:0xc06 and the briefing entry, each with Enter's
// DGROUP:0x94b0 = 1, reproduce them (implementer A's seed search; restored by the M3 s2 fixer).
// The same sequence with the briefing entry's DGROUP:0x94b0 turned to 0 (the review's H1) gives
// no gate.
TEST_CASE("world setup: seed 37816 reproduces the oracle's wave 1 [ORACLE probe-0001]") {
    OracleRig r;
    r.rng.srand(37816);  // seg2:0x1b6b
    state1_load(r.w, r.host);
    new_game(r.w);
    enter_briefing(r.w, r.host);
    CHECK(r.w.kind == 0);
    CHECK(r.w.gates == 1);
    CHECK(r.w.categories[0] == 24);
    CHECK(r.w.categories[1] == 14);
    CHECK(r.w.pools.in_use(kPoolRocks) == 38);
    CHECK(r.w.pools.in_use(kPool4) == 3);
    CHECK(r.w.pools.in_use(kPoolGatePosts) == 2);
    CHECK(r.w.pools.in_use(kPoolGateCentres) == 2);
    CHECK(r.w.w992 == 50);
    CHECK(r.w.w94e2 == 38);
    CHECK(r.w.w9332 == 2);
    CHECK(r.w.gate[0].beam.l == 6424);
    CHECK(r.w.gate[0].beam.t == 14032);
    CHECK(r.w.gate[0].beam.r == 7624);
    CHECK(r.w.gate[0].beam.b == 14080);
    CHECK(r.w.pools.rec(kPoolGatePosts, 0).x == 6152);
    CHECK(r.w.pools.rec(kPoolGatePosts, 0).y == 13920);
    // seg9:0x60bc: the player at the first gate's centre, (gate x + 760, gate y - 150).
    start_wave(r.w, r.host);
    CHECK(r.w.pools.rec(kPoolShips, r.w.player).x == 6912);
    CHECK(r.w.pools.rec(kPoolShips, r.w.player).y == 13770);
    CHECK(r.w.pools.rec(kPoolShips, r.w.player).state == 100);
}

TEST_CASE("world setup: a revisited directory at the briefing entry loses the gate (review H1)") {
    OracleRig r;
    r.rng.srand(37816);
    state1_load(r.w, r.host);
    new_game(r.w);
    r.w.added = 0;  // what the old hooks did at the briefing entry
    enter_briefing(r.w, r.host);
    CHECK(r.w.kind == -1);
    CHECK(r.w.gates == 0);
}

// ---- seg8:0x4e14

namespace {
struct VisRig {
    Pools pools;
    ViewState view;
    VisibilityGlobals g;
    is::shell::Rng rng;
    VisibilityHost host;
    std::vector<std::pair<int, int>> calls;
    VisRig() {
        view.active_count = 1;
        view.active[0] = ViewRect{1000, 2000, 3000, 4000};
        g.radar_first = 0;
        g.ship_pool = kPoolShips;
        for (int p = 0; p < kStartupPools; ++p) pools.set_in_use(p, 0);
        host.seg11_9fdc = [this](int p, int r) { calls.emplace_back(p, r); };
        host.pool8_3d80 = [](int) -> std::int16_t { return 0; };
    }
    ObjectRecord& put(int p, int i, std::int16_t x, std::int16_t y) {
        if (pools.in_use(p) <= i) pools.set_in_use(p, static_cast<std::int16_t>(i + 1));
        ObjectRecord& o = pools.rec(p, i);
        o.x = x;
        o.y = y;
        o.mass = 10;
        o.state = 0;
        return o;
    }
};
}  // namespace

TEST_CASE("visibility: the activity rectangles set +0x1a [BYTES seg8:0x4e14-0x4ff9] [OPEN On-screen flag]") {
    VisRig v;
    ObjectRecord& in = v.put(kPoolRocks, 0, 1500, 2500);
    in.w1c = 7;
    ObjectRecord& edge = v.put(kPoolRocks, 1, 3000, 2500);  // right edge: outside (half-open)
    edge.on_screen = 1;
    ObjectRecord& away = v.put(kPoolRocks, 2, 9000, 9000);
    away.on_screen = 0;
    away.scr_x = 5;
    ObjectRecord& dead = v.put(kPoolRocks, 3, 1500, 2500);
    dead.state = 1;
    dead.on_screen = 5;
    ObjectRecord& massless = v.put(kPoolRocks, 4, 1500, 2500);
    massless.mass = 0;
    massless.on_screen = 5;
    v.g.count = 0;
    visibility(v.pools, v.view, v.g, v.rng, v.host);
    CHECK(in.on_screen == 1);
    CHECK(in.w1c == 0);
    CHECK(edge.on_screen == -1);  // one frame of -1 after leaving
    CHECK(away.on_screen == 0);
    CHECK(away.scr_x == -1000);
    CHECK(away.scr_y == -1000);
    CHECK(dead.on_screen == 5);
    CHECK(massless.on_screen == 5);
    REQUIRE(v.g.count == 1);
    CHECK(v.g.pool[0] == kPoolRocks);
    CHECK(v.g.rec[0] == 0);
    // The next frame: the -1 becomes 0 with the parked position.
    v.g.count = 0;
    visibility(v.pools, v.view, v.g, v.rng, v.host);
    CHECK(edge.on_screen == 0);
    CHECK(edge.scr_x == -1000);
}

TEST_CASE("visibility: one rand() per call, seg11:0x9fdc on rand() % 9 = 0 [BYTES seg8:0x4ffb-0x50a0]") {
    // Find a seed whose first rand() is a multiple of 9, and one whose first is not.
    std::uint16_t hit = 0, miss = 0;
    for (std::uint16_t s = 1; s < 1000 && (hit == 0 || miss == 0); ++s) {
        is::shell::Rng t;
        t.srand(s);
        const bool zero = t.rand() % 9 == 0;
        if (zero && hit == 0) hit = s;
        if (!zero && miss == 0) miss = s;
    }
    CHECK(hit != 0);
    CHECK(miss != 0);
    for (const std::uint16_t seed : {hit, miss}) {
        if (seed == 0) continue;
        VisRig v;
        ObjectRecord& a = v.put(kPoolRocks, 0, 1500, 2500);
        a.b50 = 1;
        v.put(kPoolRocks, 1, 1600, 2600);
        v.rng.srand(seed);
        is::shell::Rng ref;
        ref.srand(seed);
        const bool due = ref.rand() % 9 == 0;
        visibility(v.pools, v.view, v.g, v.rng, v.host);
        CHECK(v.rng.state() == ref.state());
        if (due) {
            REQUIRE(v.calls.size() == 1);
            CHECK(v.calls[0] == std::make_pair(static_cast<int>(kPoolRocks), 0));
        } else {
            CHECK(v.calls.empty());
        }
    }
}

TEST_CASE("visibility: the seg18 mode lists every record [BYTES seg8:0x4f9a-0x4ff9]") {
    VisRig v;
    v.g.dg_9130 = 1;
    ObjectRecord& o = v.put(kPoolRocks, 0, 9000, 9000);
    o.state = 1;
    o.mass = 0;
    visibility(v.pools, v.view, v.g, v.rng, v.host);
    CHECK(o.on_screen == 1);
    CHECK(v.g.count == 1);
}

// ---- seg10:0x3a2e

TEST_CASE("rock images: slots 6 to 9 [BYTES seg10:0x3a2e-0x3e98]") {
    RecordingBlitter out;
    ImageSlotTable images{};
    std::int16_t dg_740 = 1;
    build_rock_images(out, images, dg_740);
    CHECK(dg_740 == 0);
    // #4000 50 mode 3, BITMAP ROCKS, then 869 x 53 twice and 473 x 31 twice.
    REQUIRE(out.images.size() == 6);
    CHECK(out.images[0].graphic == 0x32);
    CHECK(out.images[1].name == "ROCKS");
    CHECK(out.images[2].w == 869);
    CHECK(out.images[2].h == 53);
    CHECK(out.images[4].w == 473);
    CHECK(out.images[4].h == 31);
    CHECK(images[kSlotRockMaskBig] == out.images[2].id);
    CHECK(images[kSlotRockBig] == out.images[3].id);
    CHECK(images[kSlotRockMaskSmall] == out.images[4].id);
    CHECK(images[kSlotRockSmall] == out.images[5].id);
    // Frame 17 of the colour rocks at (48·17 + 5, 5), from (43·17, 0).
    const BlitCall big17 = bit_blt_call(images[kSlotRockBig], 17 * 48 + 5, 5, 43, 43,
                                        out.images[0].id, 17 * 43, 0, kSrcCopy);
    bool found = false;
    for (const BlitCall& c : out.calls) found = found || c == big17;
    CHECK(found);
    // The small frames are stretched 43 -> 21 at (26k + 5, 5).
    int stretches = 0;
    for (const BlitCall& c : out.calls)
        if (c.op == BlitCall::kStretchBlt && c.dst == images[kSlotRockSmall]) {
            CHECK(c.w == 21);
            CHECK(c.sw == 43);
            CHECK((c.x - 5) % 26 == 0);
            ++stretches;
        }
    CHECK(stretches == 18);
    // Once: slot 6 holds a DC.
    const std::size_t n = out.calls.size();
    build_rock_images(out, images, dg_740);
    CHECK(out.calls.size() == n);
}

// ---- seg11:0x1bcc

TEST_CASE("status tick: clock, blink, fuel and mass [BYTES seg11:0x1bcc-0x1d7b]") {
    World w;
    w.last_ship = 0;
    w.player = 0;
    w.pools.resize(kPoolShips, 1);
    w.pools.set_in_use(kPoolShips, 1);
    ShipRecord& s = w.ship(0);
    s.fuel = 1320;
    s.thrust = 205;
    s.thrusting = 1;
    s.weapon_slot = 0;
    is::hud::StatusState st;
    std::vector<int> updates;
    is::hud::StatusHooks h;
    h.update = [&updates](int a) { updates.push_back(a); };
    h.design_mass = [](int) -> std::int16_t { return 259; };
    w.frames = 36;  // second 1 against DGROUP:0x934e = 0
    w.w934e = 0;
    is::hud::status_tick(w, 1000, st, h);
    // The blink (1), the clock (7), then the 1000 ms tick: 205 / -40 = -5 (idiv) of fuel, the
    // mass 1315 / 20 + 259, and the fuel gauge (6).
    CHECK(updates == std::vector<int>{1, 7, 6});
    CHECK(st.blink == 1);
    CHECK(st.blink_due == 1300);
    CHECK(st.fuel_due == 2000);
    CHECK(s.fuel == 1315);
    CHECK(w.ship_object(0).mass == 1315 / 20 + 259);
    // 200 ms later nothing is due but the clock, which waits for DGROUP:0x934e.
    updates.clear();
    w.w934e = 1;
    is::hud::status_tick(w, 1200, st, h);
    CHECK(updates.empty());
    // Not thrusting: no burn without byte +0xe8.
    s.thrusting = 0;
    is::hud::status_tick(w, 2001, st, h);
    CHECK(s.fuel == 1315);
}

// ---- the player's design (seg14) and the flight values

TEST_CASE("flight design: TRANSFORMER, SHP record 1 on body 33 [BYTES seg14:0x1c8c seg14:0x1e35-0x1e8f seg14:0x21a0-0x22f2]") {
    // MASTER.SHP TRANSFORMER: thruster 41, armor 34; MASTER.BOD 33: strength 24, +0x1c 25,
    // +0x1d 5, flags 0x06 (no Body Thrust).
    const FlightDesign d = flight_design(41, 34, 24, 25, 5, 0x06);
    CHECK(d.thrust == 205);
    CHECK(d.drag == 25);
    CHECK(d.mass == 259);  // 240 x (8 + 100) / 100
    CHECK(d.has_flame == 1);
    ShipRecord s{};
    s.roster_index = -1;
    ObjectRecord o{};
    apply_flight_design(s, o, d, true, 5000, 0);
    CHECK(s.bec == 1);
    CHECK(s.thrust == 205);
    CHECK(s.turn == 21);  // 1200 x 5 / 205 = 29, x 3 / 4 for the player
    CHECK(s.lockout.get() == 5000);
    CHECK(o.mass == 259);
}

// ---- keys

TEST_CASE("input: B and M reach the displays [BYTES seg11:0x6e6a seg11:0x6e9a]") {
    struct A : KeyActions {
        int b = 0, m = 0;
        void show_frame_rate() override { ++b; }
        void show_messages() override { ++m; }
    } a;
    WaveGlobals g;
    KeyState ks;
    CHECK(key_message(kWmKeyDown, 'B', g, 0, 0, false, false, ks, a) == KeyResult::kHandled);
    CHECK(key_message(kWmKeyDown, 'M', g, 0, 0, false, false, ks, a) == KeyResult::kHandled);
    CHECK(a.b == 1);
    CHECK(a.m == 1);
}

TEST_CASE("input: the wave start clears the taps, not the held keys [DESIGN ADR 0013 decision 6]") {
    KeyLatch k;
    k.key_down(kVkSpace, false);
    k.key_up(kVkSpace);
    k.key_down(kVkUp, false);
    k.clear_pressed();
    CHECK_FALSE(k.test(kVkSpace, kFireMask));
    CHECK(k.test(kVkUp, kMaskDown));
}

// ---- the placers' rules (implementer A's cases, restored by the M3 s2 fixer)

TEST_CASE("world setup: the centre exclusion at 1024 x 683 [BYTES seg10:0x998 seg10:0x99d]") {
    World w;
    w.W = 1024;
    w.H = 683;
    w.shift = 3;
    const Rect16 r = centre_exclusion(w);
    CHECK(r.l == 8704);   // (1600 - 512) << 3
    CHECK(r.t == 10072);  // (1600 - 341) << 3
    CHECK(r.r == 16896);
    CHECK(r.b == 15528);
}

TEST_CASE("world: distance_cells, one Newton step in 1/8-unit cells [BYTES seg8:0x285a]") {
    std::int16_t dx = 0, dy = 0;
    CHECK(distance_cells(0, 0, 800, 0, dx, dy) == 100);
    CHECK(dx == -100);
    CHECK(dy == 0);
    // 3-4-5: g = 4 + 3/2 = 5, (25 / 5 + 5) / 2 = 5.
    CHECK(distance_cells(0, 0, 24, 32, dx, dy) == 5);
    // The torus: 3125 cells apart becomes 75 the other way round.
    CHECK(distance_cells(25000, 0, 0, 0, dx, dy) == 75);
    CHECK(dx == -75);
    // The same point: g = 0 gives 1.
    CHECK(distance_cells(100, 100, 100, 100, dx, dy) == 1);
}

TEST_CASE("world setup: every placed rock avoids the centre and the posts lie in the world (seed 37816)") {
    OracleRig r;
    r.rng.srand(37816);
    state1_load(r.w, r.host);
    new_game(r.w);
    enter_briefing(r.w, r.host);
    const Rect16 ex = centre_exclusion(r.w);
    for (int i = 0; i < r.w.pools.in_use(kPoolRocks); ++i) {
        const ObjectRecord& o = r.w.pools.rec(kPoolRocks, i);
        CHECK_FALSE(pt_in_rect(ex, o.x, o.y));
        CHECK(o.x >= 0);
        CHECK(o.y >= 0);
    }
    // Gate 0's centre is (post x + 760, post y - 150) [BYTES seg9:0x10d4].
    CHECK(r.w.pools.rec(kPoolGateCentres, 0).x == r.w.pools.rec(kPoolGatePosts, 0).x + 760);
    CHECK(r.w.pools.rec(kPoolGateCentres, 0).y == r.w.pools.rec(kPoolGatePosts, 0).y - 150);
}
