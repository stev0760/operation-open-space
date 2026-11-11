// SNOWBALLS, FIREBALLS and the FIRE DEMONS' flames (M4 s7 FUEL): src/world/fuel_balls.h against
// work/analysis/m4/s7-fuel.md sections 0-4 with s7-fuel-verify.md and core-0009's SNOWBALLS,
// FIREBALLS and FIRE DEMONS sections [ORACLE core-0009].
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "formats/dib.h"
#include "hud/weapons.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/effects.h"
#include "world/fuel_balls.h"
#include "world/rock_images.h"
#include "world/sprite.h"
#include "world/update.h"

using namespace is::world;

namespace {

using Sound = std::tuple<int, int, int, int>;

class RecFuelHost : public FuelHost {
public:
    std::vector<Sound> sounds;
    std::int16_t cost = 8;  // W(4)+0x2e = W(5)+0x2e = 8 [BYTES seg10:0x205f seg10:0x2015]
    void sound(int id, int pool, int rec, int mode) override { sounds.emplace_back(id, pool, rec, mode); }
    std::int16_t weapon_word(int w, int off) override {
        if (off == 0x2e && (w == 4 || w == 5)) return cost;
        return 0;
    }
};

class BallHost : public UpdateHost {
public:
    World* world = nullptr;
    std::uint16_t answer = 0xffffu;
    RecordingBlitter* out = nullptr;
    std::vector<std::tuple<std::int16_t, int, int>> drawn;  // (pool_rec, kind, slot)
    std::vector<std::int16_t> sizes;                        // `size` at each draw
    std::vector<std::int16_t> src_x;                        // `src_x` at each draw
    std::vector<std::tuple<int, int, int>> puffs;
    std::vector<Sound> sounds;
    World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
    // The pair finder's answer; the pair words as seg8:0x6afa leaves them (the caller A).
    std::uint16_t collide(std::int16_t pr) override {
        if (answer != 0xffffu) {
            CollideGlobals& c = collide_globals();
            c.a_pool = static_cast<std::int16_t>(pool_of(pr));
            c.a_rec = static_cast<std::int16_t>(rec_of(pr));
            c.b_pool = static_cast<std::int16_t>(answer >> 8);
            c.b_rec = static_cast<std::int16_t>(answer & 0xff);
        }
        return answer;
    }
    void draw_sprite(std::int16_t pr, int kind, int slot, int) override {
        drawn.emplace_back(pr, kind, slot);
        sizes.push_back(world->pools.rec(pool_of(pr), rec_of(pr)).size);
        src_x.push_back(world->pools.rec(pool_of(pr), rec_of(pr)).src_x);
    }
    void puff(int pool, int rec, int arg) override { puffs.emplace_back(pool, rec, arg); }
    void sound(int id, int pool, int rec, int mode) override { sounds.emplace_back(id, pool, rec, mode); }
    Blitter* blitter() override { return out; }
};

struct BallRig {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    is::shell::Rng rng;
    BallHost host;
    RecFuelHost fh;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    FuelCtx fc{w, g, host.collide_globals(), fh};
    BallRig() {
        host.world = &w;
        g.now = 10000;
        g.player = 0;
        w.player = 0;
        w.size = 1;
        w.shift = 3;
        w.detail = 3;
        w.W = 640;
        w.H = 480;
        // Pool 5 as the wave load leaves it: counts 0 and 0 (world.md "Pool counts").
        w.pools.set_in_use(kPool5, 0);
        w.pools.resize(kPool5, 0);
        // The player: weapon 4 in slot 1 of two, on screen, heading 4 (+x), extent 0x100.
        ShipRecord& s = w.ship(0);
        s.slots_cb() = 2;
        set_byte_at(s, 0xb9, kBallSnow);
        s.fuel = 1437;  // ICE EAGLE's start fuel in core-0009 [ORACLE core-0009]
        ObjectRecord& o = w.pools.rec(kPoolShips, 0);
        o.on_screen = 1;
        o.frame = 4;
        o.extent = 0x100;
        o.x = 1000;
        o.y = 2000;
    }
    Pool5Entry& E(int r) { return host.collide_globals().pool5[static_cast<std::size_t>(r)]; }
    ObjectRecord& B(int r) { return w.pools.rec(kPool5, r); }
    // The player's launch of `kind`, with slot 1 holding it.
    bool launch(std::int16_t kind = kBallSnow) {
        set_byte_at(w.ship(0), 0xb9, kind);
        return launch_fuel_ball(fc, kPoolShips, 0, kBallLife, kind);
    }
};

// The guard log's lines (world.h set_guard_todo), restored at the end of the scope.
struct GuardLog {
    std::vector<std::string> lines;
    GuardLog() {
        set_guard_todo([this](const std::string& what, const char* address) { lines.push_back(what + " " + address); });
    }
    ~GuardLog() { set_guard_todo({}); }
    GuardLog(const GuardLog&) = delete;
    GuardLog& operator=(const GuardLog&) = delete;
};

// The pattern brushes DGROUP:0x5ea8 and DGROUP:0x5eaa as known ids, restored at the end.
struct Brushes {
    BrushId mask, checker;
    Brushes() : mask(wave_graphics().mask), checker(wave_graphics().checker) {
        wave_graphics().mask = 0x2a8;
        wave_graphics().checker = 0x2aa;
    }
    ~Brushes() {
        wave_graphics().mask = mask;
        wave_graphics().checker = checker;
    }
    Brushes(const Brushes&) = delete;
    Brushes& operator=(const Brushes&) = delete;
};

}  // namespace

TEST_CASE("fuel balls: the weapon words of weapons 4, 5, 13 and 27 [BYTES seg10:0x205f seg10:0x2015 seg10:0x2a3b seg10:0x2a6e]") {
    // seg10:0x1fb2's table for the registered edition (-1) through hud::fill_weapon_words.
    WeaponWords W;
    std::optional<int> filled;
    REQUIRE(is::hud::fill_weapon_words(-1, W, filled));
    CHECK(W.word(4, 0x2e) == 8);   // 8 fuel per SNOWBALL (core-0009: 1437 -> 1429 ...) [ORACLE core-0009]
    CHECK(W.word(5, 0x2e) == 8);   // 8 per FIREBALL (1146 -> 1138 ...) [ORACLE core-0009]
    CHECK(W.word(4, 0x0a) == 0);   // not launched: the switch's cases 3 and 4 [BYTES seg11:0xad10]
    CHECK(W.word(5, 0x0a) == 0);
    CHECK(W.word(13, 0x0a) == 0);
    CHECK(W.word(13, 0x2e) == 0);  // WILDCARD costs nothing [BYTES seg10:0x2a38]
    CHECK(W.word(13, 0x46) == 1);  // no violence [BYTES seg10:0x2a3b]
    CHECK(W.word(4, 0x46) == 0);
    CHECK(W.word(27, 0x1e) == 50);  // the fruit's lifetime [BYTES seg10:0x2a6e]
    CHECK(W.word(27, -4) == -1);    // the filler's image slot and end kind before seg11:0x9078 [BYTES seg10:0x2aad seg10:0x2a8f]
    CHECK(W.word(27, 0x24) == 1);
}

TEST_CASE("fuel balls: seg13:0x1318 spends 8 fuel and plays 0x69 then 0x1b at the literal pool 5 [BYTES seg13:0x135e-0x1374 seg13:0x164f-0x169c]") {
    BallRig r;
    REQUIRE(r.launch());
    CHECK(r.w.ship(0).fuel == 1429);
    // The player's reset seg10:0x744c(0x69), then life 30 > 24: 0x1b, for SNOWBALLS too (O16).
    CHECK(r.fh.sounds == std::vector<Sound>{{0x69, 0, 0, 0}, {kSoundFireball, 5, 0, 1}});
    CHECK(r.w.pools.in_use(kPool5) == 1);  // seg7:0x2080(5, -1): p5 = 1 [ORACLE core-0009]
    CHECK(r.w.pools.allocated(kPool5) == 1);
    // Five more: 1421 ... 1389, as core-0009's six presses [ORACLE core-0009].
    for (int k = 0; k < 5; ++k) REQUIRE(r.launch());
    CHECK(r.w.ship(0).fuel == 1389);
    CHECK(r.w.pools.in_use(kPool5) == 6);
}

TEST_CASE("fuel balls: fuel <= 40 or no slot of the kind returns with nothing [BYTES seg13:0x1343-0x1381]") {
    BallRig r;
    r.w.ship(0).fuel = 40;
    CHECK_FALSE(r.launch());
    CHECK(r.w.ship(0).fuel == 40);
    CHECK(r.fh.sounds.empty());
    CHECK(r.w.pools.allocated(kPool5) == 0);
    // 41 > 40: 41 - 8 = 33, not floored.
    r.w.ship(0).fuel = 41;
    CHECK(r.launch());
    CHECK(r.w.ship(0).fuel == 33);
    // A cost above the fuel floors at 0 (`jns`).
    r.fh.cost = 50;
    r.w.ship(0).fuel = 45;
    CHECK(r.launch());
    CHECK(r.w.ship(0).fuel == 0);
    // No slot 1 ... +0xcb - 1 holds kind 5 (slot 1 holds 4).
    BallRig q;
    CHECK_FALSE(launch_fuel_ball(q.fc, kPoolShips, 0, kBallLife, kBallFire));
    CHECK(q.w.ship(0).fuel == 1437);
    // Slot 2 is past +0xcb - 1 = 1: not searched.
    set_byte_at(q.w.ship(0), 0xba, kBallFire);
    CHECK_FALSE(launch_fuel_ball(q.fc, kPoolShips, 0, kBallLife, kBallFire));
    // With +0xcb = 3 it is.
    q.w.ship(0).slots_cb() = 3;
    CHECK(launch_fuel_ball(q.fc, kPoolShips, 0, kBallLife, kBallFire));
}

TEST_CASE("fuel balls: off screen or at the cap the fuel is spent and no ball made (O1) [BYTES seg13:0x1385-0x13a7]") {
    BallRig r;
    r.w.pools.rec(kPoolShips, 0).on_screen = 0;
    CHECK_FALSE(r.launch());
    CHECK(r.w.ship(0).fuel == 1429);
    CHECK(r.fh.sounds.empty());
    CHECK(r.w.pools.allocated(kPool5) == 0);
    // `on_screen` -1 (just left the screen) passes.
    r.w.pools.rec(kPoolShips, 0).on_screen = -1;
    CHECK(r.launch());
    // The cap 2·(Detail + 2): 10 at Detail 3, 4 at Detail 0.
    CHECK(launch_cap(3) == 10);
    CHECK(launch_cap(0) == 4);
    BallRig q;
    q.w.detail = 0;
    for (int k = 0; k < 4; ++k) REQUIRE(q.launch());
    CHECK(q.w.ship(0).fuel == 1437 - 32);
    CHECK_FALSE(q.launch());                  // in use 4, not below 4
    CHECK(q.w.ship(0).fuel == 1437 - 40);     // spent all the same
    CHECK(q.w.pools.in_use(kPool5) == 4);
}

TEST_CASE("fuel balls: the template and the take of a grown record [BYTES seg13:0x1408-0x14fc] (verified claims 5, 6)") {
    BallRig r;
    REQUIRE(r.launch(kBallSnow));
    const ObjectRecord& b = r.B(0);
    CHECK(b.mass == kMassSnowball);  // kind 4: 200
    CHECK(b.hit_sound == 0x64);
    CHECK(b.radar_color.get() == 0xffu);
    CHECK(b.radar_draw.get() == kRadarSmall);
    CHECK(b.on_damage.get() == kDamagePool5);
    CHECK(b.owner_pool == -1);
    CHECK(b.state == 0);
    CHECK(b.b2e == 0);
    const Pool5Entry& e = r.E(0);
    CHECK(e.age == 0);
    CHECK(e.life == kBallLife);
    CHECK(e.split == 0);
    CHECK(e.shooter_pool == kPoolShips);
    CHECK(e.shooter_rec == 0);
    CHECK(e.hit_pool == kPoolShips);  // the launcher is the first last-hit
    CHECK(e.hit_rec == 0);
    REQUIRE(r.launch(kBallFire));
    CHECK(r.B(1).mass == kMassFireball);  // kind 5: 120
}

TEST_CASE("fuel balls: a reused record keeps what the template does not write (O3) and its stale trail (O4) [BYTES seg13:0x1501-0x151c]") {
    BallRig r;
    // Record 0 exists with a free entry and old words: the search takes it without growing.
    r.w.pools.resize(kPool5, 1);
    r.B(0).hit_sound = 0x33;
    r.B(0).mass = 0x55;
    r.E(0).age = -1;
    r.E(0).tx = {1, 2, 3, 4, 5, 6};
    r.E(0).ty = {7, 8, 9, 10, 11, 12};
    r.E(0).ts = {13, 14, 15, 16, 17, 18};
    REQUIRE(r.launch(7));  // a kind that is neither 4 nor 5
    CHECK(r.B(0).hit_sound == 0x33);
    CHECK(r.B(0).mass == 0x55);
    // The loop writes -100 five times to Tx[0] and Ty[0] only.
    CHECK(r.E(0).tx == std::array<std::int16_t, 6>{-100, 2, 3, 4, 5, 6});
    CHECK(r.E(0).ty == std::array<std::int16_t, 6>{-100, 8, 9, 10, 11, 12});
    CHECK(r.E(0).ts == std::array<std::int16_t, 6>{13, 14, 15, 16, 17, 18});
}

TEST_CASE("fuel balls: the launch velocity and corner [BYTES seg13:0x153a-0x164b] (verified claims 8, 9)") {
    BallRig r;
    ObjectRecord& l = r.w.pools.rec(kPoolShips, 0);
    l.frame = 2;  // (11, -11)
    l.extent = 0x103;  // E/2 = 129, E/32 = 8: q = 14
    l.vx = 5;
    l.vy = -7;
    REQUIRE(r.launch());
    CHECK(r.B(0).vx == 66 + 5);
    CHECK(r.B(0).vy == -66 - 7);
    CHECK(r.B(0).x == 11 * 14 + 129 + 1000 - 32);
    CHECK(r.B(0).y == -11 * 14 + 129 + 2000 - 32);
    // A negative extent truncates toward zero: E = -33, E/2 = -16, E/32 = -1, q = 5.
    BallRig q;
    q.w.pools.rec(kPoolShips, 0).extent = -33;
    REQUIRE(q.launch());
    CHECK(q.B(0).x == 16 * 5 - 16 + 1000 - 32);
    CHECK(q.B(0).y == 0 * 5 - 16 + 2000 - 32);
}

TEST_CASE("fuel balls: another launcher plays no reset, and 0x19 for a life of 24 or less [BYTES seg13:0x1655-0x1689]") {
    BallRig r;
    r.w.pools.rec(kPoolShips, 1).on_screen = 1;
    ShipRecord& s = r.w.ship(1);
    s.slots_cb() = 2;
    set_byte_at(s, 0xb9, kBallFire);
    s.fuel = 100;
    REQUIRE(launch_fuel_ball(r.fc, kPoolShips, 1, 0x19, kBallFire));
    CHECK(r.fh.sounds == std::vector<Sound>{{kSoundFireball, 5, 0, 1}});
    r.fh.sounds.clear();
    REQUIRE(launch_fuel_ball(r.fc, kPoolShips, 1, 0, kBallFire));  // seg4:0x1381's life 0 (O2)
    CHECK(r.fh.sounds == std::vector<Sound>{{kSoundFire1, 5, 1, 1}});
    // A non-ship launcher pays nothing and needs no slot.
    r.w.pools.rec(8, 3).on_screen = 1;
    REQUIRE(launch_fuel_ball(r.fc, 8, 3, 0x18, kBallSnow));
    CHECK(r.E(2).shooter_pool == 8);
}

TEST_CASE("fuel balls: the frame by age [BYTES seg7:0x474f-0x4771 seg7:0x40db-0x40fc] (verified claim 13)") {
    const std::array<std::int16_t, 8> side = {10, 16, 22, 28, 34, 40, 46, 52};
    for (int t = 0; t < 28; ++t) {
        const BallFrame b = ball_frame(static_cast<std::int16_t>(t));
        CHECK(b.f == t / 5);
        CHECK(b.side == side[static_cast<std::size_t>(t / 5)]);
        CHECK(b.off == 3 * (t / 5));
    }
    CHECK(ball_frame(28).f == 7);
    CHECK(ball_frame(29).side == 52);
    CHECK(ball_frame(29).off == 21);
    CHECK(ball_frame(30).side == 0);
    CHECK(ball_frame(34).f == 0);
    CHECK(ball_frame(34).off == 0);
}

TEST_CASE("fuel balls: a life-30 ball draws 35 passes and ends; the in-use count drops to 0 [BYTES seg7:0x463f-0x472b] [ORACLE core-0009]") {
    BallRig r;
    REQUIRE(r.launch(kBallFire));
    int passes = 0;
    while (r.E(0).age != -1 && passes < 100) {
        move_pool5(r.c, r.w);
        ++passes;
    }
    // 35 passes, 0.93 - 0.97 s at core-0009's 26 - 28 ms a frame (pool 5 back to 0 after
    // 0.93 - 0.97 s) [ORACLE core-0009].
    CHECK(passes == 35);
    REQUIRE(r.host.sizes.size() == 35);
    std::vector<std::int16_t> want;
    for (int s : {10, 16, 22, 28, 34})
        for (int k = 0; k < 5; ++k) want.push_back(static_cast<std::int16_t>(s));
    for (int k = 0; k < 3; ++k) want.push_back(40);
    want.push_back(52);
    want.push_back(52);
    for (int k = 0; k < 5; ++k) want.push_back(0);
    CHECK(r.host.sizes == want);
    // The cells of DGROUP:0xb70 at Size 1; ages 30 ... 34 read cell 0.
    CHECK(r.host.src_x[0] == 0);
    CHECK(r.host.src_x[5] == 10);
    CHECK(r.host.src_x[27] == 110);
    CHECK(r.host.src_x[28] == 196);
    CHECK(r.host.src_x[34] == 0);
    // Every draw is kind 6 from slot 10 (FIREBALL).
    for (const auto& d : r.host.drawn) {
        CHECK(std::get<1>(d) == 6);
        CHECK(std::get<2>(d) == kSlotExplosion);
    }
    CHECK(r.B(0).state == 1);
    CHECK(r.w.pools.in_use(kPool5) == 0);  // seg7:0x2080: p5 = 0 [ORACLE core-0009]
    // A SNOWBALL draws from slot 11.
    BallRig q;
    REQUIRE(q.launch(kBallSnow));
    move_pool5(q.c, q.w);
    REQUIRE(q.host.drawn.size() == 1);
    CHECK(std::get<2>(q.host.drawn[0]) == kSlotSnowball);
    CHECK(std::get<0>(q.host.drawn[0]) == pool_rec(5, 0));
}

TEST_CASE("fuel balls: the centring offset, the screen corner and the record words of a pass [BYTES seg7:0x413c-0x4284]") {
    BallRig r;
    REQUIRE(r.launch());
    r.E(0).age = 12;  // frame 2: side 22, offset 6, 48 world units
    const std::int16_t x0 = r.B(0).x, y0 = r.B(0).y;
    move_pool5(r.c, r.w);
    // seg8:0x29ea on the corner less 8·offset; the view's origin is 0 at shift 3.
    CHECK(r.B(0).scr_x == static_cast<std::int16_t>((x0 - 48) >> 3));
    CHECK(r.B(0).scr_y == static_cast<std::int16_t>((y0 - 48) >> 3));
    CHECK(r.E(0).tx[0] == r.B(0).scr_x);
    CHECK(r.E(0).ts[0] == 22);
    CHECK(r.B(0).size == 22);
    CHECK(r.B(0).extent == 22 << 3);
    CHECK(r.B(0).src_x == 26);
    CHECK(r.B(0).src_y == 0);
    // `x`/`y` back, then the move by the saved velocity.
    CHECK(r.E(0).svx == 96);
    CHECK(r.B(0).x == x0 + 96);
    CHECK(r.B(0).y == y0);
    CHECK(r.E(0).age == 13);
}

TEST_CASE("fuel balls: the trail's black square and the two PATAND fades on the window [BYTES seg7:0x4288-0x4368] (verified claim 19)") {
    Brushes br;
    BallRig r;
    RecordingBlitter out;
    r.host.out = &out;
    r.w.pools.resize(kPool5, 1);
    r.E(0).age = -1;
    r.E(0).tx = {1, 2, 3, 4, 5, 6};
    r.E(0).ty = {7, 8, 9, 10, 11, 12};
    r.E(0).ts = {13, 14, 15, 16, 17, 18};
    REQUIRE(r.launch());
    move_pool5(r.c, r.w);
    // After the shift: element 5 = the old 4, element 4 = the old 3, element 3 = the old 2.
    std::vector<BlitCall> want;
    want.push_back(pat_blt_call(kDcWindow, 5 - 1, 11 - 1, 17 + 2, 17 + 2, kBlackness));
    BlitCall s1;
    s1.op = BlitCall::kSelectBrush, s1.dst = kDcWindow, s1.brush = 0x2a8;
    want.push_back(s1);
    want.push_back(pat_blt_call(kDcWindow, 4 - 1, 10 - 1, 16 + 2, 16 + 2, 0x00A000C9));
    BlitCall s2 = s1;
    s2.brush = kBrushWhite;  // the old brush back
    want.push_back(s2);
    BlitCall s3 = s1;
    s3.brush = 0x2aa;
    want.push_back(s3);
    want.push_back(pat_blt_call(kDcWindow, 3 - 1, 9 - 1, 15 + 2, 15 + 2, 0x00A000C9));
    want.push_back(s2);
    CHECK(out.calls == want);
    CHECK(r.E(0).tx[1] == -100);
}

TEST_CASE("fuel balls: the player's redraw tests the raw element-5 square (claim 20 as corrected) [BYTES seg7:0x42af-0x430b]") {
    // W/2 = 320, H/2 = 240, B = 40 at Size 1: 320 - s5 < x5 < 360 and 240 - s5 < y5 < 280.
    auto due = [](std::int16_t x5, std::int16_t y5, std::int16_t s5) {
        BallRig r;
        r.w.pools.resize(kPool5, 1);
        r.E(0).age = -1;
        r.E(0).tx[4] = x5;
        r.E(0).ty[4] = y5;
        r.E(0).ts[4] = s5;
        r.w.ship(0).due156.set(77);
        REQUIRE(r.launch());
        move_pool5(r.c, r.w);
        return r.w.ship(0).due156.get();
    };
    CHECK(due(320, 240, 10) == 9999u);  // now - 1
    CHECK(due(311, 231, 10) == 9999u);
    // x5 = W/2 - s5: the raw square ends at W/2, outside; the blacked one (x5 - 1, s5 + 2) would
    // reach it.
    CHECK(due(310, 240, 10) == 77u);
    CHECK(due(320, 230, 10) == 77u);
    CHECK(due(360, 240, 10) == 77u);
    CHECK(due(320, 280, 10) == 77u);
    CHECK(due(359, 279, 10) == 9999u);
}

TEST_CASE("fuel balls: the end's 54 x 54 square and six trail squares of size + 2 [BYTES seg7:0x4656-0x46e3] (verified claim 28)") {
    BallRig r;
    RecordingBlitter out;
    r.host.out = &out;
    REQUIRE(r.launch());
    r.E(0).age = 34;  // the last pass of a life-30 ball
    move_pool5(r.c, r.w);
    REQUIRE(r.E(0).age == -1);
    const ObjectRecord& b = r.B(0);
    // The last seven calls: the 54-px square at the uncentred corner (in seg8:0x7ba8's area),
    // then the six trail squares with `size` 0 + 2.
    REQUIRE(out.calls.size() >= 7);
    const std::size_t n = out.calls.size();
    CHECK(out.calls[n - 7] == pat_blt_call(kDcWindow, b.scr_x - 1, b.scr_y - 1, 0x36, 0x36, kBlackness));
    for (int k = 0; k < 6; ++k)
        CHECK(out.calls[n - 6 + static_cast<std::size_t>(k)] ==
              pat_blt_call(kDcWindow, r.E(0).tx[static_cast<std::size_t>(k)] - 1,
                           r.E(0).ty[static_cast<std::size_t>(k)] - 1, 2, 2, kBlackness));
    CHECK(b.state == 1);
    // Outside (-50, -50, W + 50, H + 50) the 54-px square is not made.
    BallRig q;
    RecordingBlitter out2;
    q.host.out = &out2;
    q.w.pools.rec(kPoolShips, 0).x = 20000;
    REQUIRE(q.launch());
    q.E(0).age = 34;
    move_pool5(q.c, q.w);
    for (const BlitCall& c : out2.calls) CHECK_FALSE((c.op == BlitCall::kPatBlt && c.w == 0x36));
}

TEST_CASE("fuel balls: the gate posts' heating after the dispatch [BYTES seg7:0x44d7-0x458d] (verified claim 24)") {
    auto run = [](std::int16_t kind, std::uint8_t heat) {
        BallRig r;
        r.w.pools.resize(8, 2);
        r.w.pools.set_in_use(8, 2);
        ObjectRecord& post = r.w.pools.rec(8, 1);
        post.mass = 0xdac;
        post.b54 = 1;  // a gate post: the response moves it not
        post.flame = heat;
        REQUIRE(r.launch(kind));
        r.B(0).on_screen = 1;
        r.host.answer = static_cast<std::uint16_t>(pool_rec(8, 1));
        move_pool5(r.c, r.w);
        return std::make_tuple(r.w.pools.rec(8, 1).flame, r.w.pools.rec(8, 1).d6a.get(), r.c.f.dg_943a,
                               r.c.f.dg_943c, r.E(0).hit_pool);
    };
    // FIREBALL: heat + 1, at most 9, and +0x6a = now + 5000; no puff.
    auto f = run(kBallFire, 3);
    CHECK(std::get<0>(f) == 4);
    CHECK(std::get<1>(f) == 15000u);
    CHECK(std::get<2>(f) == 5);  // DGROUP:0x943a / 0x943c before the pair finder
    CHECK(std::get<3>(f) == 0);
    CHECK(std::get<4>(f) == 8);  // the response's step 1 recorded the post
    CHECK(std::get<0>(run(kBallFire, 9)) == 9);
    // SNOWBALL: heat > 0 becomes -(5·heat) as a byte; heat <= 0 stays.
    CHECK(static_cast<std::int8_t>(std::get<0>(run(kBallSnow, 3))) == -15);
    CHECK(std::get<0>(run(kBallSnow, 0)) == 0);
    CHECK(std::get<1>(run(kBallSnow, 3)) == 0u);
}

TEST_CASE("fuel balls: the mass-160 branch ignites or puts out icon i = the hit record (O9) [BYTES seg7:0x4591-0x463d]") {
    GuardLog log;
    BallRig r;
    r.w.pools.rec(kPoolShips, 2).mass = 0xa0;  // a ship whose mass happens to be 160
    REQUIRE(r.launch(kBallFire));
    r.B(0).on_screen = 1;
    r.host.answer = static_cast<std::uint16_t>(pool_rec(kPoolShips, 2));
    move_pool5(r.c, r.w);
    CHECK(r.w.icons[2].w02 == 2);
    CHECK(r.w.icons[2].w00 == 0x59);  // seg7:0x477e's 89
    CHECK(r.w.icons[2].burn_until == 14000u);
    CHECK(std::find(r.host.sounds.begin(), r.host.sounds.end(), Sound{0x6a, 0, 2, 1}) != r.host.sounds.end());
    // A ported branch logs nothing; only the index guard at seg7:0x45c9 would (review L4).
    CHECK(std::none_of(log.lines.begin(), log.lines.end(),
                       [](const std::string& l) { return l.find("seg7:0x45") != std::string::npos; }));
    // SNOWBALL on a burning icon: both words 0, four puffs on the hit object.
    BallRig q;
    q.w.pools.rec(kPoolShips, 2).mass = 0xa0;
    q.w.icons[2].w00 = 30;
    q.w.icons[2].w02 = 3;
    REQUIRE(q.launch(kBallSnow));
    q.B(0).on_screen = 1;
    q.host.answer = static_cast<std::uint16_t>(pool_rec(kPoolShips, 2));
    move_pool5(q.c, q.w);
    CHECK(q.w.icons[2].w00 == 0);
    CHECK(q.w.icons[2].w02 == 0);
    REQUIRE(q.host.puffs.size() >= 4);
    CHECK(std::vector<std::tuple<int, int, int>>(q.host.puffs.end() - 4, q.host.puffs.end()) ==
          std::vector<std::tuple<int, int, int>>(4, {kPoolShips, 2, 1}));
}

TEST_CASE("fuel balls: seg7:0x570a restores a FIREBALL's saved velocity and returns 0 [BYTES seg7:0x5719-0x573a seg7:0x5a18-0x5a3a]") {
    BallRig r;
    REQUIRE(r.launch(kBallFire));
    r.E(0).svx = 12;
    r.E(0).svy = -4;
    r.B(0).vx = 99;
    r.B(0).vy = 98;
    CollideCtx x = frame_collide_ctx(r.c, r.w);
    CHECK(pool5_on_damage(x, kPool5, 0, 1234) == 0);
    CHECK(r.B(0).vx == 12);
    CHECK(r.B(0).vy == -4);
    CHECK(r.w.pools.in_use(kPool5) == 1);
}

TEST_CASE("fuel balls: the SNOWBALL split at about +-18 degrees, life 16, no cap [BYTES seg7:0x573d-0x5a10] (verified claims 31-33)") {
    BallRig r;
    r.w.detail = 0;  // cap 4: the split still passes it
    for (int k = 0; k < 4; ++k) REQUIRE(r.launch(kBallSnow));
    REQUIRE(r.w.pools.in_use(kPool5) == 4);
    r.E(1).svx = 90;
    r.E(1).svy = 30;
    r.E(1).shooter_pool = 14, r.E(1).shooter_rec = 0, r.E(1).hit_pool = 3, r.E(1).hit_rec = 7;
    r.B(1).x = 4321;
    r.B(1).y = 1234;
    CollideCtx x = frame_collide_ctx(r.c, r.w);
    CHECK(pool5_on_damage(x, kPool5, 1, 0) == 0);
    // The new half takes record 4 (grown): svy/3 + svx, svx/(-3) + svy.
    REQUIRE(r.w.pools.allocated(kPool5) == 5);
    CHECK(r.w.pools.in_use(kPool5) == 5);
    CHECK(r.B(4).vx == 100);
    CHECK(r.B(4).vy == 0);
    CHECK(r.B(4).x == 4321);
    CHECK(r.B(4).y == 1234);
    CHECK(r.B(4).mass == kMassSnowball);
    CHECK(r.B(4).state == 0);
    CHECK(r.E(4).age == 0);
    CHECK(r.E(4).life == 16);
    CHECK(r.E(4).split == 1);
    CHECK(r.E(4).hit_pool == 3);
    CHECK(r.E(4).hit_rec == 7);
    CHECK(r.E(4).shooter_pool == 14);
    for (int k = 0; k < 5; ++k) {
        CHECK(r.E(4).tx[static_cast<std::size_t>(k)] == -100);
        CHECK(r.E(4).ty[static_cast<std::size_t>(k)] == -100);
    }
    // The parent: svy/(-3) + svx, svx/3 + svy, split 1, life 16.
    CHECK(r.B(1).vx == 80);
    CHECK(r.B(1).vy == 60);
    CHECK(r.E(1).split == 1);
    CHECK(r.E(1).life == 16);
    // A half hit again restores its saved velocity (no second split).
    r.E(1).svx = 5, r.E(1).svy = 6;
    CHECK(pool5_on_damage(x, kPool5, 1, 0) == 0);
    CHECK(r.B(1).vx == 5);
    CHECK(r.w.pools.allocated(kPool5) == 5);
    // Every division truncates toward zero.
    BallRig q;
    REQUIRE(q.launch(kBallSnow));
    q.E(0).svx = -7;
    q.E(0).svy = 5;
    CollideCtx y = frame_collide_ctx(q.c, q.w);
    pool5_on_damage(y, kPool5, 0, 0);
    CHECK(q.B(1).vx == 1 - 7);
    CHECK(q.B(1).vy == 2 + 5);
    CHECK(q.B(0).vx == -1 - 7);
    CHECK(q.B(0).vy == -2 + 5);
}

TEST_CASE("fuel balls: the tables' 11 entries: no ball past them (ruling R2) [DESIGN ADR 0016]") {
    GuardLog log;
    BallRig r;
    r.w.detail = 20;  // a hand-edited Detail: the cap 44
    for (int k = 0; k < kPool5Table; ++k) REQUIRE(r.launch());
    const std::int16_t fuel = r.w.ship(0).fuel;
    CHECK_FALSE(r.launch());
    CHECK(r.w.ship(0).fuel == fuel - 8);  // the fuel goes before the search, as the bytes order it
    CHECK(r.w.pools.allocated(kPool5) == kPool5Table);
    REQUIRE_FALSE(log.lines.empty());
    // A split with no free entry: the parent's half runs alone.
    CollideCtx x = frame_collide_ctx(r.c, r.w);
    r.E(3).svx = 90;
    r.E(3).svy = 30;
    pool5_on_damage(x, kPool5, 3, 0);
    CHECK(r.w.pools.allocated(kPool5) == kPool5Table);
    CHECK(r.B(3).vx == 80);
    CHECK(r.E(3).split == 1);
    CHECK(r.E(3).life == 16);
}

TEST_CASE("fuel balls: seg12:0x2db6's cap, life, heading and sound [BYTES seg12:0x2dc5-0x313a] (verified claims 34, 34a, 34b)") {
    CHECK(spawn_cap(3, 0) == 5);    // 10·10 / 20
    CHECK(spawn_cap(3, 10) == 10);  // 10·20 / 20
    CHECK(spawn_cap(3, 15) == 10);  // the level is held at 10
    CHECK(spawn_cap(0, 0) == 2);    // 4·10 / 20
    CHECK(spawn_cap(1638, 10) == 3);  // lo16(3280·20) = 64: the high word dropped
    BallRig r;
    r.w.score_level = 0;
    ObjectRecord& m = r.w.pools.rec(6, 2);
    m.extent = 0x100;  // E/2 = 128, E/32 + 4 = 12
    m.x = 500;
    m.y = 600;
    m.vx = 33;
    REQUIRE(spawn_fuel_ball(r.fc, 6, 2, 4, 0xb, kBallFire));
    CHECK(r.B(0).mass == kMassFireball);
    CHECK(r.B(0).vx == 96);  // 6 · 16, no launcher velocity
    CHECK(r.B(0).vy == 0);
    CHECK(r.B(0).x == 128 + 16 * 12 + 500 - 32);
    CHECK(r.B(0).y == 128 + 0 + 600 - 32);
    CHECK(r.E(0).life == 0xb);
    CHECK(r.E(0).shooter_pool == 6);
    CHECK(r.E(0).hit_rec == 2);
    CHECK(r.fh.sounds == std::vector<Sound>{{kSoundFire1, 5, 0, 1}});  // 0x19 FIRE1 [ORACLE core-0009]
    for (int k = 0; k < 5; ++k) CHECK(r.E(0).tx[static_cast<std::size_t>(k)] == -100);
    // Life 30 is held at 24 and still plays 0x19 (O13).
    REQUIRE(spawn_fuel_ball(r.fc, 6, 2, 4, 30, kBallSnow));
    CHECK(r.E(1).life == 24);
    CHECK(r.B(1).mass == kMassSnowball);
    CHECK(std::get<0>(r.fh.sounds.back()) == kSoundFire1);
    // The cap 5 at Detail 3, level 0: the sixth is refused.
    for (int k = 0; k < 3; ++k) REQUIRE(spawn_fuel_ball(r.fc, 6, 2, 4, 0xb, kBallFire));
    CHECK_FALSE(spawn_fuel_ball(r.fc, 6, 2, 4, 0xb, kBallFire));
    CHECK(r.w.pools.in_use(kPool5) == 5);
    // A FIRE DEMONS flame (life 11) ends at age 16: 16 passes.
    BallRig q;
    REQUIRE(spawn_fuel_ball(q.fc, 6, 2, 4, 0xb, kBallFire));
    int passes = 0;
    while (q.E(0).age != -1 && passes < 100) {
        move_pool5(q.c, q.w);
        ++passes;
    }
    CHECK(passes == 16);
}

TEST_CASE("fuel balls: the gate's words and the wave start [BYTES seg12:0x22cf-0x22d6 seg9:0x6782-0x67af]") {
    BallRig r;
    REQUIRE(r.launch());
    pool5_gate(r.host.collide_globals(), 0);
    CHECK(r.E(0).age == kBallLife - 7);  // ends 12 passes later
    int passes = 0;
    while (r.E(0).age != -1 && passes < 100) {
        move_pool5(r.c, r.w);
        ++passes;
    }
    CHECK(passes == 12);
    REQUIRE(r.launch());
    REQUIRE(r.launch());
    pool5_wave_start(r.w.pools, kPool5, &r.host.collide_globals());
    CHECK(r.E(0).age == -1);
    CHECK(r.E(1).age == -1);
    CHECK(r.B(1).state == 1);
    // Without tables only the states.
    r.B(0).state = 0;
    r.E(0).age = 5;
    pool5_wave_start(r.w.pools, kPool5, nullptr);
    CHECK(r.B(0).state == 1);
    CHECK(r.E(0).age == 5);
}

TEST_CASE("fuel balls: graphics 170 and 175 (golden): frame 7 of a SNOWBALL lies past graphic 175 [BYTES IGRAPHIC.DLL 4000 170 IGRAPHIC.DLL 4000 175] (verified claim 18)") {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store{std::filesystem::path(v)};
    const is::fmt::Dib fire = store.sprite(static_cast<std::uint16_t>(kGraphicExplosion));
    CHECK(fire.width == 248);
    CHECK(fire.height == 52);
    CHECK(kExplosionX[7] + 52 == fire.width);  // the FIREBALL's frame 7 ends at the right edge
    const is::fmt::Dib snow = store.sprite(static_cast<std::uint16_t>(kGraphicSnowball));
    CHECK(snow.width == 196);
    CHECK(snow.height == 46);
    CHECK(kExplosionX[7] >= snow.width);  // the SNOWBALL's frame 7 reads past it
}

namespace {

// The dispatcher's handler call for kDamagePool5 is collide.cpp's own (call_on_damage ->
// pool5_on_damage, the integrator's binding), so CollideHost::other_damage is never reached for
// it: `splits` counts the calls that would take the old route and must stay 0.
class SplitHost : public BallHost {
public:
    class Collide final : public CollideHost {
    public:
        SplitHost* u = nullptr;
        void sound(int id, int pool, int rec, int mode) override { u->sound(id, pool, rec, mode); }
        void puff(int pool, int rec, int arg) override { u->puff(pool, rec, arg); }
        std::int16_t other_damage(std::uint32_t id, int pool, int rec, std::int16_t a) override {
            if (id != kDamagePool5 || !u->frame_ctx) return 0;
            ++u->splits;
            CollideCtx x = frame_collide_ctx(*u->frame_ctx, *u->world);
            return pool5_on_damage(x, pool, rec, a);
        }
    };
    Collide ch;
    int splits = 0;
    SplitHost() { ch.u = this; }
    CollideHost& collide_host() override { return ch; }
};

}  // namespace

TEST_CASE("fuel balls: a SNOWBALL into a gate post splits in the mover's pass; the new half runs the same pass (core-0009 p5 = 2) [BYTES seg7:0x4432 seg7:0x4736-0x4743]") {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    is::shell::Rng rng;
    SplitHost host;
    RecFuelHost fh;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    FuelCtx fc{w, g, host.collide_globals(), fh};
    host.world = &w;
    g.now = 10000;
    w.size = 1, w.shift = 3, w.detail = 3, w.W = 640, w.H = 480;
    w.pools.set_in_use(kPool5, 0);
    w.pools.resize(kPool5, 0);
    ShipRecord& s = w.ship(0);
    s.slots_cb() = 2;
    set_byte_at(s, 0xb9, kBallSnow);
    s.fuel = 1437;
    ObjectRecord& l = w.pools.rec(kPoolShips, 0);
    l.on_screen = 1, l.frame = 4, l.extent = 0x100, l.x = 1000, l.y = 2000;
    w.pools.resize(8, 2);
    w.pools.set_in_use(8, 2);
    w.pools.rec(8, 1).mass = 0xdac;
    w.pools.rec(8, 1).b54 = 1;
    REQUIRE(launch_fuel_ball(fc, kPoolShips, 0, kBallLife, kBallSnow));
    w.pools.rec(kPool5, 0).on_screen = 1;
    host.answer = static_cast<std::uint16_t>(pool_rec(8, 1));
    move_pool5(c, w);
    CHECK(host.splits == 0);  // call_on_damage dispatched kDamagePool5 itself
    REQUIRE(w.pools.in_use(kPool5) == 2);  // p5 = 2 [ORACLE core-0009]
    const CollideGlobals& cg = host.collide_globals();
    // svx = 96, svy = 0: the new half (96, -32), the parent (96, 32); both split, life 16.
    CHECK(w.pools.rec(kPool5, 1).vx == 96);
    CHECK(w.pools.rec(kPool5, 1).vy == -32);
    CHECK(w.pools.rec(kPool5, 0).vx == 96);
    CHECK(w.pools.rec(kPool5, 0).vy == 32);
    CHECK(cg.pool5[0].life == 16);
    CHECK(cg.pool5[1].split == 1);
    CHECK(cg.pool5[1].hit_pool == 8);  // copied from the parent after the response's step 1
    // The new half (record 1 < the raised count) ran in the same pass.
    CHECK(cg.pool5[1].age == 1);
    CHECK(cg.pool5[0].age == 1);
}
