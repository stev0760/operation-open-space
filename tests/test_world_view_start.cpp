// The view at the wave start (M5 s1 VIEW, work/analysis/m5/s1-view.md): the player's cell sizes
// from seg14:0x11a8 and seg14:0x0 (setup.h ship_cell_raw, ship_cell_sizes), and the origin
// DGROUP:0x2400/0x2402 that seg8:0x0 leaves after the ship entry seg7:0x1d0c for the oracle's
// wave 1 of seed 37816 (tests/test_world_fixer.cpp). Portable.
#include <cstdint>
#include <optional>
#include <string>

#include "doctest.h"
#include "nav/fs.h"
#include "shell/rng.h"
#include "world/flight.h"
#include "world/setup.h"
#include "world/update.h"

using namespace is::world;
namespace nav = is::nav;

TEST_CASE("view start: the raw cell of seg14:0x11a8 [BYTES seg14:0x12e0-0x12f0]") {
    CHECK(ship_cell_raw(33).side == 33);
    CHECK(ship_cell_raw(33).gap == 5);
    CHECK(ship_cell_raw(34).gap == 6);
}

TEST_CASE("view start: seg14:0x0's cell sizes [BYTES seg14:0xb97-0xc30]") {
    ObjectRecord o{};
    ship_cell_sizes(o, ship_cell_raw(33), 1);  // TRANSFORMER (#4000 60, 132 x 33) at Size 1
    CHECK(o.size == 33);
    CHECK(o.gap == 5);
    CHECK(o.extent == 264);
    ship_cell_sizes(o, ship_cell_raw(33), 0);  // Size 0: an odd side + 1, gap 4
    CHECK(o.size == 17);
    CHECK(o.gap == 2);
    CHECK(o.extent == 272);
    ship_cell_sizes(o, ship_cell_raw(33), 2);
    CHECK(o.size == 66);
    CHECK(o.gap == 10);
    CHECK(o.extent == 264);
    ship_cell_sizes(o, ship_cell_raw(34), 0);
    CHECK(o.size == 17);
    CHECK(o.gap == 3);
    CHECK(o.extent == 272);
}

namespace {

class OneIcon : public IconReader {
public:
    int count(const std::string&) override { return 1; }
    std::optional<is::fmt::IconCell> cell(const std::string&, int) override {
        is::fmt::IconCell c;
        c.bgra[33] = 0xffffffffu;
        return c;
    }
};

// tests/test_world_fixer.cpp's OracleRig: probe-0001's game start, with the ship set-up giving
// the player TRANSFORMER's cell.
struct StartRig {
    nav::MemFs fs;
    nav::Walker walker{fs};
    OneIcon icons;
    is::shell::Rng rng;
    World w;
    SetupHost host{rng, walker, icons};
    StartRig() {
        fs.add_drive('D');
        fs.add_dir("D:\\GAMES");
        for (const char* n : {"D:\\GAMES\\ALPHA.EXE", "D:\\GAMES\\BRAVO.EXE", "D:\\GAMES\\CHARLIE.EXE"})
            fs.add_file(n);
        host.now = [] { return 1000u; };
        host.ship_setup = [](World& w) {
            ship_setup_minimal(w);
            ship_cell_sizes(w.ship_object(w.player), ship_cell_raw(33), w.size);
        };
        power_on(w);
        w.W = 1024;
        w.H = 683;
        w.size = 1;
        w.shift = 3;
        w.detail = 3;
        w.complex = 0;
        w.drive_offset = 1;
        w.handover.directory = "D:\\GAMES\\";
        w.added = 1;
        w.record = 0;
        w.dir();
    }
};

}  // namespace

// With post 0 at (6152, 13920) (DGROUP:0x6678 - (272, 112)), post 1 at 7624 and extent 264, the
// entry puts the player at x = 6152 + 736 - 132 + 136 = 6892, y = 13920, and seg8:0x0 gives the
// origin (6892 - 4096, 13920 - 2732) = (2796, 11188). The player keeps seg9:0x60bc's vy = -30, so
// at the wave frame count F the origin is (2796, 11188 - 30 (F - 1)): probe-0001's dumps hold
// (2796, 2278) at DGROUP:0x1e68 = 298 and (2796, 2158) at 302 [ORACLE probe-0001]
// [BYTES seg9:0x637e seg7:0x1dea-0x1e27 seg8:0x4f seg8:0x5f].
TEST_CASE("view start: seed 37816's entry origin is the oracle's (2796, 11188) [ORACLE probe-0001]") {
    StartRig r;
    r.rng.srand(37816);
    state1_load(r.w, r.host);
    new_game(r.w);
    enter_briefing(r.w, r.host);
    start_wave(r.w, r.host);
    REQUIRE(r.w.pools.rec(kPoolGatePosts, 0).x == 6152);
    REQUIRE(r.w.pools.rec(kPoolGatePosts, 0).y == 13920);
    REQUIRE(r.w.pools.rec(kPoolGatePosts, 1).x == 7624);
    const ObjectRecord& me = r.w.pools.rec(kPoolShips, r.w.player);
    CHECK(me.extent == 264);
    CHECK(me.size == 33);
    CHECK(me.gap == 5);
    CHECK(me.src_y == 5);  // seg9:0x6624's +0x12 = +0x24 after the set-up
    CHECK(me.state == 100);
    CHECK(me.vy == -30);

    WaveGlobals g;
    g.now = 2000;  // past ship 0's entry time (now + 0)
    g.player = r.w.player;
    g.last_ship = r.w.last_ship;
    g.gates = r.w.gates;
    FrameGlobals f;
    f.entry_pending = 1;
    ViewState view;
    view.play_w = r.w.W;
    view.play_h = r.w.H;
    view.shift = r.w.shift;
    KeyLatch keys;
    UpdateHost uh;
    JoyState joy;
    JoyButtons jb;
    UpdateCtx c{FlightCtx{r.w.pools, r.w.ships, g, keys, uh, r.rng}, f, view, uh, joy, jb};
    ship_entry(c);
    CHECK(me.state == 0);
    CHECK(me.x == 6892);
    CHECK(me.y == 13920);
    CHECK(view.x0 == 2796);
    CHECK(view.y0 == 11188);
    CHECK(me.vy == -30);  // the entry writes no velocity [BYTES seg7:0x1e2b-0x1e54]

    // The drift: the frames after the entry move y by vy (seg8:0x1b54 through integrate), and
    // seg8:0x0 recentres each frame (seg7:0xb8c).
    const std::int16_t pr = static_cast<std::int16_t>(kPoolShips * 256 + r.w.player);
    // F - 1 steps of 30 from the entry position reach the dump at frame F (s1-view.md claim 5).
    auto drift = [&](int n) {
        for (int k = 0; k < n; ++k) integrate(c.flight, 1, pr);
        ViewInputs vi;
        vi.player_x = me.x;
        vi.player_y = me.y;
        recentre(view, vi);
    };
    drift(297);  // F = 298
    CHECK(view.x0 == 2796);
    CHECK(view.y0 == 2278);
    drift(4);    // F = 302
    CHECK(view.x0 == 2796);
    CHECK(view.y0 == 2158);
}
