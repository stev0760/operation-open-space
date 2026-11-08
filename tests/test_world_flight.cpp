// The flight model (src/world/flight.h; docs/re/world.md "Flight model", "Helpers", "World
// coordinates"): hand-worked frames with the documented constants and the original's 16-bit
// truncations (ADR 0013 decision 7).
#include <cstdint>
#include <set>

#include "doctest.h"
#include "shell/rng.h"
#include "world/flight.h"
#include "world/input.h"

using namespace is::world;

namespace {

// One player ship in action mode with the documented design values: T0 = 60 (SHP thruster 12
// x 5), D = 10, m = 50; full fuel and +0x2a = 100 so the go flag holds on every thrust frame.
struct Rig {
    Pools pools;
    ShipTable ships{};
    WaveGlobals g;
    KeyLatch keys;
    FlightHost host;
    is::shell::Rng rng;
    FlightCtx c{pools, ships, g, keys, host, rng};

    Rig() {
        g.now = 100000;
        g.player = 0;
        g.last_ship = 0;
        ShipRecord& s = ships[0];
        s.thrust = 60;
        s.drag = 10;
        s.fuel = 600;
        s.w2a = 100;
        s.thrust_mult = 1;
        s.roster_index = -1;
        s.has_flame = 1;
        s.bec = 1;
        s.turn = 21;
        ObjectRecord& o = pools.rec(kPoolShips, 0);
        o.mass = 50;
        o.x = 12800;
        o.y = 12800;
        o.on_screen = 1;
    }
    ObjectRecord& o() { return pools.rec(kPoolShips, 0); }
    ShipRecord& s() { return ships[0]; }
};

class CountingHost : public FlightHost {
public:
    int sprites = 0;
    void player_sprite() override { ++sprites; }
};

}  // namespace

TEST_CASE("flight: tables and helpers") {
    CHECK(heading_dx(0) == 0);
    CHECK(heading_dy(0) == -16);
    CHECK(heading_dx(9) == -6);
    CHECK(heading_dy(9) == 16);  // not the mirror of entry 7 [BYTES DGROUP:0x1050]
    CHECK(kSineTable[25] == 55);
    CHECK(kSineTable[90] == 128);
    CHECK(sin128(0) == 0);
    CHECK(sin128(90) == 128);
    CHECK(sin128(180) == 0);
    CHECK(sin128(270) == -128);
    CHECK(sin128(-90) == -128);
    CHECK(cos128(0) == 128);
    CHECK(cos128(360) == 128);
    CHECK(cos128(180) == -128);
    CHECK(cos128(25) == kSineTable[65]);
    CHECK(abs16(-32768) == -32768);
    CHECK(magnitude(3, 4) == 5);
    CHECK(magnitude(0, 0) == 1);
    CHECK(magnitude(0, -100) == 100);
    // seg8:0x19b0's sectors (world.md "Direction")
    CHECK(direction16(-100, 0) == 0);    // |r| > 500, num <= 0
    CHECK(direction16(100, 0) == 8);
    CHECK(direction16(0, 10) == 4);      // r = 0, den > 0
    CHECK(direction16(0, -10) == 12);
    CHECK(direction16(5, 1) == 7);       // r = 500
    CHECK(direction16(-5, 1) == 1);      // r = -500, num <= 0
    CHECK(direction16(3, 2) == 6);       // r = 150
    CHECK(direction16(-1, 2) == 3);      // r = -50, num <= 0
}

TEST_CASE("flight: the integrator moves in 16 bits and wraps at 25600") {
    Rig r;
    ObjectRecord& o = r.o();
    o.x = 25590;
    o.vx = 20;
    o.y = 10;
    o.vy = -20;
    integrate(r.c, 1, pool_rec(kPoolShips, 0));
    CHECK(o.x == 0);       // 25610 > 25600
    CHECK(o.y == 25600);   // -10 < 0
    o.x = 25580;
    o.vx = 20;
    integrate(r.c, 1, pool_rec(kPoolShips, 0));
    CHECK(o.x == 25600);   // not greater: stays
    o.x = 32760;           // the 16-bit add wraps first, then x < 0 gives 25600
    o.vx = 20;
    integrate(r.c, 1, pool_rec(kPoolShips, 0));
    CHECK(o.x == 25600);
    o.x = 1000;
    o.vx = 12000;          // steps 3: lo16(36000) = -29536
    o.y = 1000;
    o.vy = 0;
    integrate(r.c, 3, pool_rec(kPoolShips, 0));
    CHECK(o.x == 25600);   // 1000 - 29536 < 0
    r.g.dg_9130 = 1;       // seg18 mode: no wrap
    o.x = 25590;
    o.vx = 20;
    integrate(r.c, 1, pool_rec(kPoolShips, 0));
    CHECK(o.x == 25610);
}

TEST_CASE("flight: the thrust's force and velocity change are 16-bit") {
    ShipRecord s{};
    s.thrust = 60;
    s.w2a = 100;
    s.fuel = 600;
    s.roster_index = -1;
    CHECK(thrust_force(s, 0) == 60);
    s.w2a = 0;
    CHECK(thrust_force(s, 0) == 20);  // T / 3
    s.w2a = 30;                       // T - lo16((40 - 30) * 60) >> 6 = 60 - 9
    CHECK(thrust_force(s, 0) == 51);
    s.w2a = 100;
    s.roster_index = 2;               // (lo16((b + 10) * 10) * T) / 100 with b = 5
    CHECK(thrust_force(s, 5) == 90);
    // Heading 0 at rest, T = 60, k = 1, m = 50: dvy = (lo16(-16 * 60) - 0) / 50 = -19
    Dv d = thrust_dv(0, 60, 1, 10, 50, 0, 0);
    CHECK(d.dx == 0);
    CHECK(d.dy == -19);
    // The second frame: vy = -19, drag in the product: (-960 - lo16(10 * -19)) / 50 = -15
    d = thrust_dv(0, 60, 1, 10, 50, 0, -19);
    CHECK(d.dy == -15);
    // 16 * 3000 = 48000 wraps to -17536; times k = 5 wraps again to -22144
    d = thrust_dv(4, 3000, 1, 0, 100, 0, 0);
    CHECK(d.dx == -175);
    d = thrust_dv(4, 3000, 5, 0, 100, 0, 0);
    CHECK(d.dx == -221);
}

TEST_CASE("flight: a thrust frame at heading 0 with Up held") {
    Rig r;
    r.keys.key_down(kVkUp, false);
    thrust_step(r.c, 4, 0);
    CHECK(r.o().vy == -19);
    CHECK(r.o().vx == 0);
    CHECK(r.s().thrusting == 1);
    CHECK(r.s().flame_until.get() == r.g.now + 250);
    CHECK(r.o().flame == 3);  // lo16(T * k) / 20
    thrust_step(r.c, 4, 0);
    CHECK(r.o().vy == -34);
}

TEST_CASE("flight: the go flag's rules") {
    ShipRecord s{};
    WaveGlobals g;
    g.now = 5000;
    s.fuel = 600;
    s.w2a = 0;
    CHECK_FALSE(go_flag(s, g, 4, true));  // +0x2a = 0
    s.w2a = 40;
    g.cycle = 2;
    CHECK_FALSE(go_flag(s, g, 4, true));  // +0x2a < 50 on DGROUP:0x95f6 < 4
    g.cycle = 4;
    CHECK(go_flag(s, g, 4, true));
    g.wave_kind = 6;  // kind 6 with fuel > 50 keeps the cycle rule
    g.cycle = 2;
    CHECK_FALSE(go_flag(s, g, 4, true));
    g.wave_kind = 0;
    s.fuel = 30;  // max(30 / 12, 2) = 2 >= cycle / 2
    g.cycle = 4;
    CHECK(go_flag(s, g, 4, true));
    g.cycle = 6;
    CHECK_FALSE(go_flag(s, g, 4, true));
    CHECK(go_flag(s, g, 1, true));         // mode 1
    s.w32 = 4;
    s.w38 = 2;
    CHECK_FALSE(go_flag(s, g, 4, true));   // the player in action mode
    CHECK(go_flag(s, g, 4, false));        // another ship
    s.fuel = 600;
    s.w2a = 100;
    s.lockout.set(5001);
    CHECK_FALSE(go_flag(s, g, 4, true));   // lock-out after now
    s.lockout.set(4800);
    CHECK(go_flag(s, g, 4, true));
    g.wave_kind = 6;                       // +0x84 + 400 > now in a kind-6 wave
    CHECK_FALSE(go_flag(s, g, 4, true));
}

TEST_CASE("flight: thrust on 5 of every 9 frames") {
    Rig r;
    r.o().vy = 0;
    r.keys.key_down(kVkUp, false);
    r.s().turn_acc = 100;  // no turn input in these frames
    int thrust_frames = 0;
    std::int16_t last_vy = r.o().vy;
    for (int frame = 0; frame < 9; ++frame) {
        move_ships(r.c);
        if (r.o().vy != last_vy) ++thrust_frames;
        last_vy = r.o().vy;
    }
    CHECK(thrust_frames == 5);
    CHECK(r.g.cycle == 0);  // 1, 2, ... 8, 0
}

TEST_CASE("flight: drift and drag") {
    Rig r;
    r.g.detail = 2;  // dv = -(lo16(v * D) / (m << 3)) = -(1000 / 400)
    r.o().vx = 100;
    thrust_step(r.c, 4, 0);
    CHECK(r.o().vx == 98);
    r.g.braking = 1;  // c = 50 / -60 - 0 / 8 - 10 - 30 = -40; dv = lo16(-40 * 98) / 50
    thrust_step(r.c, 4, 0);
    CHECK(r.o().vx == 98 - 78);
    // Detail 3, moving straight along heading 0: sp = 100, a = 360, along = 128000 / 5120
    Dv d = drift_dv(0, 10, 10, 0, -100, false);
    CHECK(d.dx == 0);
    CHECK(d.dy == 20);  // (-16 * 25) / -20
    d = drift_dv(0, 10, 10, 0, -100, true);
    CHECK(d.dy == 26);  // along (25 << 2) / 3 = 33
}

TEST_CASE("flight: the brake reaches exactly 0") {
    Rig r;
    r.o().vx = 100;
    // B = 50 / 30 + 60 / 8 + 10 + 30 = 48: dv = -(48 * 100 / 50) = -96
    brake(r.c, 0);
    CHECK(r.o().vx == 4);
    brake(r.c, 0);  // -(192 / 50) = -3
    CHECK(r.o().vx == 1);
    brake(r.c, 0);  // 0, so -sign(v)
    CHECK(r.o().vx == 0);
    r.keys.key_down(kVkDown, false);  // Down has priority over Up
    r.keys.key_down(kVkUp, false);
    r.o().vy = -50;
    thrust_step(r.c, 4, 0);
    CHECK(r.o().vy > -50);
}

TEST_CASE("flight: turning needs the key down; both keys cancel") {
    Rig r;
    CountingHost host;
    FlightCtx c{r.pools, r.ships, r.g, r.keys, host, r.rng};
    r.o().frame = 15;
    r.keys.key_down(kVkRight, false);
    turn_input(c);
    CHECK(r.o().frame == 0);      // wraps above 15
    CHECK(r.g.turned == 1);       // DGROUP:0x9138
    CHECK(host.sprites == 1);
    r.keys.key_up(kVkRight);
    r.keys.key_down(kVkLeft, false);
    turn_input(c);
    CHECK(r.o().frame == 15);     // wraps below 0
    r.keys.key_down(kVkRight, false);
    turn_input(c);
    CHECK(r.o().frame == 15);     // both: +1 then -1
    r.keys.key_up(kVkRight);
    r.keys.key_up(kVkLeft);
    r.keys.key_down(kVkRight, false);  // a tap between frames: down, then up
    r.keys.key_up(kVkRight);
    turn_input(c);
    CHECK(r.o().frame == 15);     // mask 0x8000 alone: no turn
    r.s().lockout.set(r.g.now);   // lock-out not before now
    r.keys.key_down(kVkRight, false);
    turn_input(c);
    CHECK(r.o().frame == 15);
}

TEST_CASE("flight: Alt spin and the turn cadence") {
    Rig r;
    CountingHost host;
    FlightCtx c{r.pools, r.ships, r.g, r.keys, host, r.rng};
    r.o().frame = 3;
    r.keys.key_down(kVkMenu, false);
    r.keys.key_down(kVkRight, false);
    turn_input(c);
    CHECK(r.s().spin == 1);
    CHECK(r.o().frame == 3);  // Alt: no step, the spin does it
    turn_input(c);
    CHECK(r.s().spin == 1);   // capped at 1
    spin(c, 0);
    CHECK(r.o().frame == 4);
    spin(c, 0);
    CHECK(r.o().frame == 5);
    // The cadence with turn word 21 on screen: 0 -> 21 (turn), 11, 1, 22 (turn)
    r.keys.reset();
    r.s().spin = 0;
    r.s().turn_acc = 0;
    r.keys.key_down(kVkRight, false);
    const std::int16_t f0 = r.o().frame;
    int steps = 0;
    std::int16_t prev = f0;
    for (int i = 0; i < 4; ++i) {
        move_ships(c);
        if (r.o().frame != prev) ++steps;
        prev = r.o().frame;
    }
    CHECK(steps == 2);
    CHECK(r.s().turn_acc == 22);
}

// The 0.3 s turn of wave-0001 (4 steps for Left, 3 for Right in the same run): the accumulator
// +0x16 runs whether a key is held or not, so a key held N frames turns 3 or 4 steps for N = 10
// ... 12 and 2 or 3 for N = 9, by +0x16 at key-down [BYTES seg13:0x61b seg13:0x654 seg13:0x688].
TEST_CASE("flight: a held turn's steps depend on the accumulator's phase") {
    for (int n = 9; n <= 12; ++n) {
        std::set<int> seen;
        for (int a0 = 0; a0 <= 30; ++a0) {
            Rig r;
            r.s().turn_acc = static_cast<std::int16_t>(a0);
            r.keys.key_down(kVkLeft, false);
            std::int16_t prev = r.o().frame;
            int steps = 0;
            for (int i = 0; i < n; ++i) {
                move_ships(r.c);
                if (r.o().frame != prev) ++steps;
                prev = r.o().frame;
            }
            seen.insert(steps);
        }
        if (n == 9)
            CHECK(seen == std::set<int>{2, 3});
        else
            CHECK(seen == std::set<int>{3, 4});
    }
}

TEST_CASE("flight: a tap of Up thrusts once through the pressed bit") {
    Rig r;
    r.keys.key_down(kVkUp, false);
    r.keys.key_up(kVkUp);
    thrust_step(r.c, 4, 0);
    CHECK(r.o().vy == -19);
    thrust_step(r.c, 4, 0);  // the read cleared it: drift now
    CHECK(r.o().vy >= -19);
    CHECK(r.s().thrusting == 0);
}

TEST_CASE("flight: the OverThruster") {
    Rig r;
    r.keys.key_down(kVkUp, false);
    r.keys.key_down(kVkControl, false);
    thrust_step(r.c, 4, 0);
    CHECK(r.s().thrust_mult == 5);
    CHECK(r.s().fuel == 575);
    CHECK(r.s().overthrust_cool == 8);
    CHECK(r.g.burst_count == 0);
    // k = 5: dvy = lo16(lo16(-16 * 60) * 5) / 50 = -4800 / 50
    CHECK(r.o().vy == -96);
    thrust_step(r.c, 4, 0);  // cool-down 8 >= 4: no burst; k decays to 4 first
    CHECK(r.s().fuel == 575);
    CHECK(r.s().thrust_mult == 4);
}

TEST_CASE("flight: limit_speed") {
    Pools pools;
    ObjectRecord& o = pools.rec(kPoolRocks, 0);
    o.vx = 30;
    o.vy = -15;
    limit_speed(pools, 0x28, pool_rec(kPoolRocks, 0));
    CHECK(o.vx == 29);
    CHECK(o.vy == -14);
    o.vx = 20;
    o.vy = 0;
    limit_speed(pools, 0x28, pool_rec(kPoolRocks, 0));
    CHECK(o.vx == 20);
}

TEST_CASE("flight: the design's thrust, drag, mass and turn word") {
    // SHP Thruster 12, Armor 40; body strength 30, +0x1c 10, +0x1d 7, flame
    const FlightDesign d = flight_design(12, 40, 30, 10, 7, 0);
    CHECK(d.thrust == 60);
    CHECK(d.drag == 10);
    CHECK(d.mass == 300 * (10 + 100) / 100);  // 330
    CHECK(d.has_flame == 1);
    CHECK(flight_design(12, 40, 30, 10, 7, 0x01).has_flame == 0);  // Body Thrust
    CHECK(flight_design(12, 200, 30, 10, 7, 0).mass == 300 * (-14 + 100) / 100);  // signed armor byte
    // Ship byte +0xe3 = design +0x44 AND 1 (seg14:0x21d6): TRANSFORMER (SHP flags 0x15) 0, the
    // yellow shots of core-0002; SKY TALON (0x1c) 1, the cyan shots of core-0006 (M4 s5 fixer).
    CHECK(flight_design(12, 40, 30, 10, 7, 0, 0x15).shot_row == 0);
    CHECK(flight_design(12, 40, 30, 10, 7, 0, 0x1c).shot_row == 1);
    CHECK(flight_design(12, 40, 30, 10, 7, 0, 0x21).shot_row == 0);  // 0x20 leaves bit 0 alone
    {
        ShipRecord t{};
        t.roster_index = -1;
        ObjectRecord u{};
        apply_flight_design(t, u, flight_design(12, 40, 30, 10, 7, 0, 0x1c), false, 0, 0);
        CHECK(t.be3 == 1);
        apply_flight_design(t, u, flight_design(12, 40, 30, 10, 7, 0, 0x15), false, 0, 0);
        CHECK(t.be3 == 0);
    }
    ShipRecord s{};
    s.roster_index = -1;
    ObjectRecord o{};
    apply_flight_design(s, o, d, false, 1234, 0);
    CHECK(s.bec == 1);
    CHECK(s.thrust == 60);
    CHECK(s.thrust_mult == 1);
    CHECK(s.lockout.get() == 1234);
    CHECK(s.turn == 1200 * 7 / 60);  // 140
    CHECK(o.mass == 330);
    apply_flight_design(s, o, d, true, 1234, 0);
    CHECK(s.turn == 105);  // 3/4 for the player while DGROUP:0x95bc != 0
    s.roster_index = 1;    // T'' = (lo16((5 + 10) * 10) * 60) / 100 = 90
    apply_flight_design(s, o, d, false, 1234, 5);
    CHECK(s.turn == 1200 * 7 / 90);
}
