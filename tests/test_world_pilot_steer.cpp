// How a computer ship flies and fires (src/world/pilot_steer.h, src/world/flight.h computer_cap,
// escort_slowdown, move_ships): numbers worked from the listing (seg4:0xbde, seg13:0xd6c,
// seg4:0x5654, seg4:0x584c, seg11:0x5cd7-0x5f11, seg11:0x62ed) and the 20 ms series of
// oracle/digests/ships-0001.md (work/oracle/ships-0001/g1-ship{1,2}.csv; those tests skip without
// work/oracle beside IS_ORIGINAL_DIR) (M5 stage 3, track STEER).
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/ambulance.h"
#include "world/flight.h"
#include "world/input.h"
#include "world/missile.h"
#include "world/pilot.h"
#include "world/pilot_steer.h"
#include "world/world.h"

using namespace is::world;

namespace {

// The flight host: the team by design index, the special places, the cloak and shield calls.
class Host : public FlightHost {
public:
    std::map<int, std::int16_t> team;
    std::vector<RingPlace> places;
    int cloak_calls = 0, shield_calls = 0, thinks = 0, steers = 0, choices = 0;
    std::vector<std::string> order;
    std::int16_t design_kind(int design) override {
        const auto it = team.find(design);
        return it == team.end() ? 0 : it->second;
    }
    std::int16_t special_places() override { return static_cast<std::int16_t>(places.size()); }
    RingPlace special_place(int k) override { return places[static_cast<std::size_t>(k)]; }
    std::int16_t seg11_4c72(int) override { return ++cloak_calls, 0; }
    std::int16_t seg11_4b00(int) override { return ++shield_calls, 0; }
    void seg4_78(int ship) override { ++thinks, order.push_back("think " + std::to_string(ship)); }
    void seg4_bde(int ship) override { ++steers, order.push_back("steer " + std::to_string(ship)); }
    void seg4_6e70(int ship) override { ++choices, order.push_back("choice " + std::to_string(ship)); }
};

// The steer host: records every call out; the weapon words come from a local table.
class Calls : public SteerHost {
public:
    std::vector<std::string> log;
    Point16 screen{500, 300};
    std::map<std::pair<int, int>, std::int16_t> words;  // (w, off) -> W(w)+off
    void think(int s) override { log.push_back("think " + std::to_string(s)); }
    void hazard_step(int s) override { log.push_back("hazard " + std::to_string(s)); }
    void weapon_choice(int s) override { log.push_back("choice " + std::to_string(s)); }
    void touch(int s, std::int16_t pr) override {
        log.push_back("touch " + std::to_string(s) + " " + std::to_string(static_cast<std::uint16_t>(pr)));
    }
    Point16 to_screen(std::int16_t, std::int16_t) override { return screen; }
    void capture_count(int s, int v) override {
        log.push_back("count " + std::to_string(s) + " " + std::to_string(v));
    }
    void launch(World&, WaveGlobals&, is::shell::Rng&, int s, int pool, int w) override {
        log.push_back("launch " + std::to_string(s) + " " + std::to_string(pool) + " " + std::to_string(w));
    }
    void fuel_ball(int pool, int rec, int life, int kind) override {
        log.push_back("ball " + std::to_string(pool) + " " + std::to_string(rec) + " " +
                      std::to_string(life) + " " + std::to_string(kind));
    }
    void ring_of_death(int pool, int rec) override {
        log.push_back("ring " + std::to_string(pool) + " " + std::to_string(rec));
    }
    void tether_use(int pool, int rec, int w) override {
        log.push_back("tether " + std::to_string(pool) + " " + std::to_string(rec) + " " + std::to_string(w));
    }
    void lightning(int pool, int rec) override {
        log.push_back("lightning " + std::to_string(pool) + " " + std::to_string(rec));
    }
    void shock(int pool, int rec) override { log.push_back("shock " + std::to_string(pool) + " " + std::to_string(rec)); }
    void wildcard(int pool, int rec) override {
        log.push_back("wildcard " + std::to_string(pool) + " " + std::to_string(rec));
    }
    void weapon_display() override { log.push_back("display"); }
    std::int16_t weapon_word(int w, int off) override {
        const auto it = words.find({w, off});
        return it == words.end() ? 0 : it->second;
    }
    bool has(const std::string& s) const {
        for (const auto& l : log)
            if (l == s) return true;
        return false;
    }
};

// Ship 0 the player in action mode, ship 1 a computer ship with the PLATYPUS of ships-0001
// (MASTER.SHP record 88: thruster 27 -> +0x0c = 135; body 36 PLATYPUS: drag +0x0e = 20, turn
// base 4 -> turn 1200·4/135 = 35; mass 212 + full tank 1160 / 20 = 270) [ORACLE ships-0001], of
// team 2 (FUZZY ONES), off screen at the world's middle.
struct Rig {
    World w;
    WaveGlobals g;
    KeyLatch keys;
    Host host;
    is::shell::Rng rng;
    FlightCtx f{w.pools, w.ships, g, keys, host, rng};
    Calls calls;
    SteerGlobals sg;
    SteerCtx c{w, f, calls, sg};

    Rig() {
        g.now = 100000;
        g.player = 0;
        g.last_ship = 2;
        g.detail = 3;
        g.size = 1;
        g.dg_94e2 = 38;  // probe-0001's wave [ORACLE probe-0001]
        g.dg_952e = -1;
        host.team[1] = 2;
        for (int i = 0; i < 3; ++i) {
            ShipRecord& s = w.ships[static_cast<std::size_t>(i)];
            s.thrust = 135;
            s.drag = 20;
            s.fuel = 1160;
            s.w2a = 100;
            s.w26 = 100;
            s.thrust_mult = 1;
            s.roster_index = -1;
            s.has_flame = 0;
            s.bec = 1;
            s.turn = 35;
            s.design_index = static_cast<std::int16_t>(i);
            ObjectRecord& o = w.pools.rec(kPoolShips, i);
            o.mass = 270;
            o.x = 12800;
            o.y = 12800;
            o.extent = 160;
        }
        w.design_ratings[1].tank = 1160;
        w.pools.set_in_use(kPoolShips, 3);
    }
    ShipRecord& s(int i = 1) { return w.ships[static_cast<std::size_t>(i)]; }
    ObjectRecord& o(int i = 1) { return w.pools.rec(kPoolShips, i); }
    Pilot p(int i = 1) { return Pilot(s(i)); }
};

}  // namespace

// ------------------------------------------------------------------ seg11:0x5cd7-0x5f11

TEST_CASE("computer cap: ships-0001's PLATYPUS brakes at |vx| + |vy| = 91, then thrusts") {
    // g1-ship2.csv 08:38:22.566 (25, -66) heading 3 -> 22.586 (19, -48) -> 22.606 (25, -48) ->
    // 22.667 (30, -48) [ORACLE ships-0001]. The cap: 90 (team 2), DGROUP:0x94e2 = 38 leaves it,
    // x 3/4 = 67, Size 1, turn 35 > 30: 67·35/45 = 52; sp = seg8:0x35d0(25, -66) = 70 > 52 and the
    // heading 3 is not seg8:0x19b0(-66, 25), so mode 5: B = 270/30 + 135/8 + 20 + 30 = 75,
    // dv = (-(75·25)/270, -(75·-66)/270) = (-6, 18).
    Rig r;
    r.s().thrusting = 1;
    r.s().flame_until.set(r.g.now + 100u);  // inside the 250 ms flame window: the flame stays
    r.o().frame = 3;
    r.o().vx = 25;
    r.o().vy = -66;
    thrust_step(r.f, 3, 1);
    CHECK(r.s().laps == 52);
    CHECK(r.o().vx == 19);
    CHECK(r.o().vy == -48);
    // (19, -48): 67 < 90, the thrust at heading 3: ((15·135 - 20·19)/270, (-6·135 + 20·48)/270)
    // = (6, 0) [BYTES seg11:0x66b3-0x6713]
    CHECK(r.s().thrusting == 1);
    thrust_step(r.f, 3, 1);
    CHECK(r.s().laps == 90);
    CHECK(r.o().vx == 25);
    CHECK(r.o().vy == -48);
    thrust_step(r.f, 3, 1);
    CHECK(r.o().vx == 30);
    CHECK(r.o().vy == -48);
}

TEST_CASE("computer cap: the scalings, the team-5 cap and the unreachable thrust block") {
    Rig r;
    std::int16_t mode = 3;
    bool no_thrust = false;
    // Team 5 (SPEED DEMONS): 120; below it nothing else [BYTES seg11:0x5ce2-0x5cf6]
    r.host.team[1] = 5;
    r.o().vx = 50;
    r.o().vy = 50;
    computer_cap(r.f, 1, mode, no_thrust);
    CHECK(r.s().laps == 120);
    CHECK(mode == 3);
    // Team 2, DGROUP:0x94e2 = 20 (< 30): 90·5/4 = 112, x 3/4 = 84 (quarters `cwd; and dx, 3`),
    // turn 20 <= 30; sp = 70 <= 84: no brake [BYTES seg11:0x5d6c-0x5d78 seg11:0x5e1f-0x5e3f]
    r.host.team[1] = 2;
    r.g.dg_94e2 = 20;
    r.s().turn = 20;
    computer_cap(r.f, 1, mode, no_thrust);
    CHECK(r.s().laps == 84);
    CHECK(mode == 3);
    CHECK_FALSE(no_thrust);
    // 70 < 80 < 100: 90·4/5 = 72, x 3/4 = 54, Size 2: 216/5 = 43, turn 40: 43·35/50 = 30;
    // heading 0 is not the direction of (50, 50): mode 5 [BYTES seg11:0x5dec-0x5e0c
    // seg11:0x5e43-0x5e53 seg11:0x5e73-0x5e8e seg11:0x5f11]
    r.g.dg_94e2 = 80;
    r.g.size = 2;
    r.s().turn = 40;
    computer_cap(r.f, 1, mode, no_thrust);
    CHECK(r.s().laps == 30);
    CHECK(mode == 5);
    // >= 100: 90·3/4 = 67, x 3/4 = 50, Size 0: 250/4 = 62, turn 30 (not above 30)
    mode = 3;
    r.g.dg_94e2 = 100;
    r.g.size = 0;
    r.s().turn = 30;
    computer_cap(r.f, 1, mode, no_thrust);
    CHECK(r.s().laps == 62);
    CHECK(mode == 5);  // 62 < sp 70, heading 0 is not seg8:0x19b0(50, 50)
    // The block of seg11:0x5efb: only for sp >= 10923, where 3·sp wraps in 16 bits; (12000, 0)
    // heads 4 (east): 3·12000 = lo16 -29536, halved -14768 < the cap: no thrust, mode kept
    // (s3-tick-verify.md claim 42) [BYTES seg11:0x5efb-0x5f0f]
    mode = 3;
    no_thrust = false;
    r.o().vx = 12000;
    r.o().vy = 0;
    r.o().frame = 4;
    computer_cap(r.f, 1, mode, no_thrust);
    CHECK(no_thrust);
    CHECK(mode == 3);
}

TEST_CASE("computer cap: the special places' rings at Detail > 1") {
    // A ring of radius 800 units (100 cells, DGROUP:0x4f30 >> 3) around (12800, 12800): at the
    // centre (d = 1) the cap is 90·2/3 = 60, on the ring (d = 100, 60 < d < 140) 90·7/3 = 210;
    // beyond r + 100 = 200 cells, or at Detail 1, it stays 90 [BYTES seg11:0x5cfc-0x5de9].
    Rig r;
    std::int16_t mode = 3;
    bool no_thrust = false;
    r.host.places.push_back(RingPlace{800, 12800, 12800});
    computer_cap(r.f, 1, mode, no_thrust);
    CHECK(r.s().laps == 60);
    r.o().x = 12800 + 800;
    computer_cap(r.f, 1, mode, no_thrust);
    CHECK(r.s().laps == 210);
    r.o().x = 12800 + 8 * 200;  // d = 200: not below r + 100
    computer_cap(r.f, 1, mode, no_thrust);
    CHECK(r.s().laps == 90);
    // Only the first place in range counts: a second ring at the ship does not scale again
    r.o().x = 12800 + 800;
    r.host.places.push_back(RingPlace{800, 12800 + 800, 12800});
    computer_cap(r.f, 1, mode, no_thrust);
    CHECK(r.s().laps == 210);
    r.g.detail = 1;
    computer_cap(r.f, 1, mode, no_thrust);
    CHECK(r.s().laps == 90);
}

// ------------------------------------------------------------------ seg11:0x62ed

TEST_CASE("escort slow-down: 3/4 toward zero within 400 cells of the goal") {
    // ships-0001 g1-ship1.csv: (16, 0) -> (12, 0) and (9, -9) -> (6, -6) near the goal
    // [ORACLE ships-0001]; quarter(3·-9 = -27) = (-27 + 3) sar 2 = -6 [BYTES seg11:0x6379-0x63a4]
    Rig r;
    ObjectRecord& rock = r.w.pools.rec(kPoolRocks, 0);
    rock.x = 12800 + 8 * 399;
    rock.y = 12800;
    r.s().w32 = kPoolRocks;
    r.s().w34 = 0;
    r.s().w38 = 3;
    r.s().w3a = 2;
    r.s().w52 = 0;
    r.o().vx = 16;
    r.o().vy = 0;
    escort_slowdown(r.f, 1);
    CHECK(r.o().vx == 12);
    r.o().vx = 9;
    r.o().vy = -9;
    escort_slowdown(r.f, 1);
    CHECK(r.o().vx == 6);
    CHECK(r.o().vy == -6);
    // +0x52 = 2 still slows; 1 does not; 400 cells away does not; the action player does not
    r.s().w52 = 2;
    escort_slowdown(r.f, 1);
    CHECK(r.o().vx == 4);
    r.s().w52 = 1;
    escort_slowdown(r.f, 1);
    CHECK(r.o().vx == 4);
    r.s().w52 = 0;
    rock.x = 12800 + 8 * 400;
    escort_slowdown(r.f, 1);
    CHECK(r.o().vx == 4);
    rock.x = 12800;
    r.s(0) = r.s(1);
    r.o(0).vx = 40;
    escort_slowdown(r.f, 0);
    CHECK(r.o(0).vx == 40);
    // The tail runs after the brake path too (seg11:0x5faf jumps to seg11:0x62ed): brake
    // (40, 0) -> 40 - 3000/270 = 29, then 3/4 -> 21
    r.o().vx = 40;
    r.o().vy = 0;
    thrust_step(r.f, 5, 1);
    CHECK(r.o().vx == 21);
}

// ------------------------------------------------------------------ seg13:0xd6c

TEST_CASE("aim heading: the shared modes agree with the missile's turn (missile.cpp)") {
    Rig r;
    int n = 0;
    for (std::int16_t mode : {2, 3, 4, 5, 0x1e}) {
        for (int cur = 0; cur < 16; ++cur) {
            for (int dxs = -3; dxs <= 3; ++dxs) {
                for (int dys = -3; dys <= 3; ++dys) {
                    AimArgs a;
                    a.ship = 1;
                    a.mode = mode;
                    a.cur = static_cast<std::int16_t>(cur);
                    a.mx = 25000;
                    a.my = 300;
                    a.tx = lo16(25000 + dxs * 7000);  // across the wrap at 25600 too
                    a.ty = lo16(300 + dys * 5000);
                    TurnArgs t;
                    t.ship = 1;
                    t.mode = mode;
                    t.cur = a.cur;
                    t.tx = a.tx, t.ty = a.ty, t.mx = a.mx, t.my = a.my;
                    const std::int16_t want = turn_toward(t, 0, r.w.ships, {});
                    CHECK(aim_heading(r.c, a) == want);
                    ++n;
                }
            }
        }
    }
    CHECK(n == 5 * 16 * 49);
}

TEST_CASE("aim heading: a ship's hold, the led heading and the race bias") {
    Rig r;
    AimArgs a;
    a.ship = 1;
    a.mode = 2;
    a.cur = 4;
    a.mx = 12800, a.my = 12800;
    a.tx = 12800, a.ty = 11200;  // 200 cells north: h = 0, one step back from 4 [BYTES seg13:0xfb7-0xfdd]
    CHECK(aim_heading(r.c, a) == 3);
    CHECK(r.s().due156.get() == r.g.now - 1u);  // seg13:0x6716(S) [BYTES seg13:0xfaf]
    r.s().beb = 1;  // [BYTES seg13:0xd85]
    CHECK(aim_heading(r.c, a) == 4);
    r.s().beb = 0;
    r.s().type = 3;  // the shield's steps 1 ... 6 [BYTES seg13:0xd92-0xda3]
    CHECK(aim_heading(r.c, a) == 4);
    r.s().type = 7;
    CHECK(aim_heading(r.c, a) == 3);
    r.s().type = 0;
    // Mode 6 is not in the switch: the heading is kept [DESIGN ADR 0016 decision 1]
    a.mode = 6;
    CHECK(aim_heading(r.c, a) == 4);
    // a0 != 0: seg8:0x2658(DGROUP:0x9324, S, est pool, est rec), the led heading
    // (s3-tick-verify.md claim 40) [BYTES seg13:0xef4-0xf11]
    ObjectRecord& tank = r.w.pools.rec(kPool4, 0);
    tank.x = 14000;
    tank.y = 9000;
    tank.vx = -20;
    tank.vy = 5;
    a.mode = 2;
    a.a0 = 1;
    a.est_pool = kPool4;
    a.est_rec = 0;
    a.tx = tank.x, a.ty = tank.y;
    const std::int16_t led = intercept_heading(r.w.pools, kPool4, 0, kPoolShips, 1);
    a.cur = led;
    CHECK(aim_heading(r.c, a) == led);
    a.cur = static_cast<std::int16_t>((led + 2) & 15);
    CHECK(aim_heading(r.c, a) == ((led + 1) & 15));
    // The race (kind 6), mode 2, a pool other than 4: seg8:0x19b0 then byte +0x149 added (pool-8
    // record +0x148 + 2·gates lit) and +0x61 from the heading's gap [BYTES seg13:0xe48-0xef1]
    r.g.wave_kind = 6;
    a.a0 = 0;
    a.est_pool = 8;
    a.tx = 12800, a.ty = 11200;  // h = 0
    r.s().w32 = 8;
    set_ship_sb(r.s(), pw::kClaimPool, 0);
    set_ship_sb(r.s(), pw::kClaimRec, 2);
    r.g.gates = 1;
    r.w.pools.rec(8, 2).b52 = 1;
    a.cur = 0;
    r.o().frame = 0;
    CHECK(aim_heading(r.c, a) == 1);  // h = 2: one step
    CHECK(r.p().no_cap() == 1);       // |0 - 2| < 6
    r.w.pools.rec(8, 2).b52 = 0;      // h = (0 - 2) & 15 = 14
    r.o().frame = 6;
    CHECK(aim_heading(r.c, a) == 15);
    CHECK(r.p().no_cap() == 0);       // |6 - 14| = 8
}

// ------------------------------------------------------------------ seg4:0xbde

TEST_CASE("steer: the count, the served ship and the forced think") {
    Rig r;
    r.p().est_pool(kPoolRocks);
    steer(r.c, 1);
    CHECK(r.sg.steers[1] == 1);  // DGROUP:0x1cb8 + 2 [BYTES seg4:0xc0a]
    r.g.dg_952e = 1;
    r.g.dg_9e49 = 3;
    steer(r.c, 1);
    CHECK(r.sg.steers[1] == 1);  // [BYTES seg4:0xbed-0xc01]
    r.g.dg_952e = -1;
    r.p().est_pool(-1);
    steer(r.c, 1);
    CHECK(r.calls.has("think 1"));  // seg4:0x78(i) [BYTES seg4:0xcd2]
    CHECK(r.sg.steers[1] == 2);
}

TEST_CASE("steer: hunted by its own target, the ship brakes and drifts") {
    // Ship 1 attacks ship 2 (estimate (14, 2), d = 50 cells), whose estimate is (14, 1); |vx| =
    // 40 > 34: seg11:0x5ab6(1, 5): 40 - 75·40/270 = 29 [BYTES seg4:0xe6d-0xebb]; the brake
    // order stops the thrust, so +0x3a = 2 drifts (mode 6; Detail 2: -(29·20)/(8·270) = 0)
    // [BYTES seg4:0x1126-0x112b].
    Rig r;
    r.g.detail = 2;
    r.o(2).y = 12800 - 8 * 50;
    Pilot P = r.p();
    P.est_pool(kPoolShips);
    P.est_rec(2);
    P.est_act(1);
    P.est_app(2);
    r.p(2).est_pool(kPoolShips);
    r.p(2).est_rec(1);
    r.s().thrusting = 1;
    r.o().vx = 40;
    r.o().vy = 0;
    steer(r.c, 1);
    CHECK(r.o().vx == 29);
    CHECK(r.o().vy == 0);
    CHECK(r.s().thrusting == 0);  // the drift puts the flame out [BYTES seg11:0x6022-0x6034]
}

TEST_CASE("steer: closing fast brakes; order 0x14 passes mode 3 once") {
    // Aim 300 cells north (dx 0, dy -300); vy = +60: (60·-300)/300 = -60 < -50: the brake
    // [BYTES seg4:0xec0-0xf1e]; vy = +40 gives -40: no brake.
    Rig r;
    r.g.detail = 2;  // the drift after the brake order is then -(44·20)/(8·270) = 0
    r.o(2).y = 12800 - 8 * 300;
    Pilot P = r.p();
    P.est_pool(kPoolShips);
    P.est_rec(2);
    P.est_act(1);
    P.est_app(2);
    r.o().vy = 60;
    steer(r.c, 1);
    CHECK(r.o().vy == 60 - (75 * 60) / 270);  // 44
    Rig q;
    q.o(2).y = 12800 - 8 * 300;
    Pilot Q = q.p();
    Q.est_pool(kPoolShips);
    Q.est_rec(2);
    Q.est_act(1);
    Q.est_app(2);
    q.o().vy = 40;
    q.s().flame_until.set(q.g.now + 1000u);
    q.s().thrusting = 1;
    steer(q.c, 1);
    CHECK(q.s().thrusting == 1);  // heading 0 held: mode 1, no brake
    CHECK(q.o().vy == 40);        // mode 1 changes no velocity (world.md "Flame length")
    // Order 0x14 within 220 cells: +0x3a = 7, which passes 3 and is rewritten 2 [BYTES
    // seg4:0xe36-0xe4d seg4:0xf23-0xf39]
    Rig t;
    t.o(2).y = 12800 - 8 * 100;
    Pilot T = t.p();
    T.est_pool(kPoolShips);
    T.est_rec(2);
    T.est_act(3);
    T.est_app(2);
    T.order(0x14);
    AimArgs a;
    a.ship = 1;
    a.mode = 3;
    a.cur = 0;
    a.tx = 12800, a.ty = t.o(2).y, a.mx = 12800, a.my = 12800;
    Rig u;  // the same turn on a fresh rig
    const std::int16_t want = aim_heading(u.c, a);
    steer(t.c, 1);
    CHECK(T.est_app() == 2);
    CHECK(t.o().frame == want);
}

TEST_CASE("steer: the veer, the away burst and the turn counter") {
    // +0x3a = 3: seg11:0x5ab6(i, 1) with +0x61 = 1, and +0x16 = 35/3 = 11 [BYTES
    // seg4:0x10fd-0x111f seg4:0x1172-0x1194]
    Rig r;
    r.o(2).y = 12800 - 8 * 300;
    Pilot P = r.p();
    P.est_pool(kPoolShips);
    P.est_rec(2);
    P.est_act(3);
    P.est_app(3);
    P.goal_act(5);  // +0x38 = 5 holds the first thrust branch back
    steer(r.c, 1);
    CHECK(r.s().thrusting == 1);
    CHECK(P.no_cap() == 1);
    CHECK(r.s().turn_acc == 11);
    // +0x3a = 5 within 70 cells: after the turn's thrust, seg11:0x5ab6(i, 2), the OverThruster
    // (fuel - 25, multiplier 5) [BYTES seg4:0x1198-0x11b5 seg11:0x64b6-0x65e2]
    Rig q;
    q.o(2).y = 12800 - 8 * 60;
    Pilot Q = q.p();
    Q.est_pool(kPoolShips);
    Q.est_rec(2);
    Q.est_act(3);
    Q.est_app(5);
    steer(q.c, 1);
    CHECK(q.s().fuel == 1160 - 25);
    CHECK(q.s().thrust_mult == 5);
}

TEST_CASE("steer: the far capture burst draws one rand() % 18") {
    // +0x3a = +0x36 = 2, the estimate the goal (a0 = 1, the led heading), d = 1000 > 800, Engine
    // +0x26 = 100 > 50, fuel 1160 > 200, heading held: rand() % 18 = 0 bursts; +0x61 = 1 either
    // way (s3-tick-verify.md claim 38) [BYTES seg4:0x1207-0x12bc].
    int hit = -1, miss = -1;
    for (int seed = 1; seed < 2000 && (hit < 0 || miss < 0); ++seed) {
        is::shell::Rng p;
        p.srand(static_cast<std::uint16_t>(seed));
        ((p.rand() % 18) == 0 ? hit : miss) = seed;
    }
    REQUIRE(hit > 0);
    REQUIRE(miss > 0);
    for (int seed : {hit, miss}) {
        Rig r;
        r.rng.srand(static_cast<std::uint16_t>(seed));
        is::shell::Rng probe = r.rng;
        probe.rand();
        ObjectRecord& tank = r.w.pools.rec(kPool4, 0);
        tank.x = 12800;
        tank.y = 12800 - 8 * 1000;
        Pilot P = r.p();
        P.est_pool(kPool4), P.goal_pool(kPool4);
        P.est_rec(0), P.goal_rec(0);
        P.est_act(2), P.goal_act(2);
        P.est_app(2), P.goal_app(2);
        r.o().frame = intercept_heading(r.w.pools, kPool4, 0, kPoolShips, 1);
        steer(r.c, 1);
        CHECK(r.rng.state() == probe.state());  // exactly one draw
        CHECK(P.no_cap() == 1);
        CHECK(r.s().fuel == (seed == hit ? 1160 - 25 : 1160));
    }
}

TEST_CASE("steer: the tether grab of a fuel tank between 80 and 400 cells") {
    // A capturable tank (+0x53 = 1) at 200 cells, slot 1 holding weapon 2, the tether idle
    // (+0x36 = -1), fuel 1000 <= tank 1160 - 50: seg4:0x584c(1, i, 0) -> seg11:0x947a(14, i, 2)
    // [BYTES seg4:0x12bf-0x1384]; at fuel 1120 > 1110 nothing.
    for (std::int16_t fuel : {1000, 1120}) {
        Rig r;
        ObjectRecord& tank = r.w.pools.rec(kPool4, 0);
        tank.x = 12800;
        tank.y = 12800 - 8 * 200;
        tank.b53 = 1;
        r.o().w36 = -1;
        r.s().fuel = fuel;
        r.s().slots_cb() = 2;
        set_ship_b(r.s(), 0xb8 + 1, 2);
        Pilot P = r.p();
        P.est_pool(kPool4), P.goal_pool(kPool4);
        P.est_rec(0), P.goal_rec(0);
        P.est_act(2), P.goal_act(2);
        P.est_app(2), P.goal_app(2);
        r.o().frame = intercept_heading(r.w.pools, kPool4, 0, kPoolShips, 1);
        steer(r.c, 1);
        CHECK(r.calls.has("tether 14 1 2") == (fuel == 1000));
        CHECK(r.s().weapon_slot == (fuel == 1000 ? 1 : 0));
    }
}

TEST_CASE("steer: the touch of an unseen ship and its box") {
    // The box (x - 144, y - 144, x + 384, y + 384) around the estimate object, the ship's square
    // (x, y, x + extent, y + extent), off screen: seg13:0x16ac(i, 0·256 + 3) (s3-tick-verify.md
    // claim 37) [BYTES seg4:0x139d-0x143e].
    for (int dx : {300, 383, 384}) {
        Rig r;
        ObjectRecord& icon = r.w.pools.rec(0, 3);
        icon.x = 12800 - static_cast<std::int16_t>(dx);
        icon.y = 12800;
        Pilot P = r.p();
        P.est_pool(0), P.goal_pool(0);
        P.est_rec(3), P.goal_rec(3);
        P.est_act(2), P.goal_act(2);
        P.est_app(2), P.goal_app(2);
        steer(r.c, 1);
        CHECK(r.calls.has("touch 1 3") == (dx < 384));
    }
    Rig r;  // on screen: no touch
    r.o().on_screen = 1;
    Pilot P = r.p();
    P.est_pool(0), P.goal_pool(0);
    P.est_rec(3), P.goal_rec(3);
    P.est_act(2), P.goal_act(2);
    P.est_app(2), P.goal_app(2);
    r.w.pools.rec(0, 3).x = 12800 - 300;
    r.w.pools.rec(0, 3).y = 12800;
    steer(r.c, 1);
    CHECK_FALSE(r.calls.has("touch 1 3"));
}

TEST_CASE("steer: the gate detour aims beside and above an even pool-12 record") {
    // Pool-12 record 0 at (10000, 9000), its pool-8 record on screen above the ship: the aim is
    // (10000 ± 1400, 8600), + when the ship's screen x (word +0) exceeds seg8:0x29ea's x of the
    // record [BYTES seg4:0xcf8-0xdd9]: the heading turns toward NE (2) or NW (14).
    for (std::int16_t sx : {600, 400}) {
        Rig r;
        r.o().x = 10000, r.o().y = 10000;
        r.o().on_screen = 1;
        r.o().scr_x = sx;
        r.o().scr_y = 300;
        ObjectRecord& mid = r.w.pools.rec(12, 0);
        mid.x = 10000, mid.y = 9000;
        r.w.pools.rec(8, 0).on_screen = 1;
        r.w.pools.rec(8, 0).scr_y = 100;
        Pilot P = r.p();
        P.est_pool(12);
        P.est_rec(0);
        P.est_act(1);  // a0 = 0: the plain heading to the aim point
        P.est_app(2);
        steer(r.c, 1);
        CHECK(r.o().frame == (sx == 600 ? 1 : 15));
    }
}

// ------------------------------------------------------------------ seg4:0x5654, seg4:0x584c

TEST_CASE("weapon decision: the direction test and the cloak") {
    // Ship 1 at (12800, 12800) on screen (500, 300) facing 0; the goal ship 2 north of it on
    // screen (500, 100): seg8:0x19b0(-200, 0) = 0 [BYTES seg4:0x5747-0x5759].
    auto setup = [](Rig& r, std::uint8_t cls, std::int16_t heading) {
        r.o().on_screen = 1, r.o().scr_x = 500, r.o().scr_y = 300;
        r.o(2).on_screen = 1, r.o(2).scr_x = 500, r.o(2).scr_y = 100;
        r.o().frame = heading;
        r.s().weapon_slot = 1;
        set_ship_b(r.s(), 0xb8 + 1, 5);  // FIREBALLS: seg13:0x1318(14, s, 25, 5)
        Pilot P = r.p();
        P.goal_pool(kPoolShips);
        P.goal_rec(2);
        P.weapon_class(cls);
    };
    {
        Rig r;
        setup(r, 1, 0);
        CHECK(weapon_decision(r.c, 1) == 1);
        CHECK(r.calls.has("ball 14 1 25 5"));  // seg4:0x584c(+0x18, i, 25) [BYTES seg4:0x582b-0x583d]
    }
    {
        Rig r;
        setup(r, 2, 8);  // the opposite heading for class 2
        CHECK(weapon_decision(r.c, 1) == 1);
    }
    {
        Rig r;
        setup(r, 1, 3);  // off: [bp-8] unwritten, read as 0 [DESIGN ADR 0016 decision 1]
        CHECK(weapon_decision(r.c, 1) == 0);
        Rig q;
        setup(q, 0, 3);  // class 0 equals the 0
        CHECK(weapon_decision(q.c, 1) == 1);
        Rig t;
        setup(t, 3, 5);  // class 3 skips the test [BYTES seg4:0x56a5-0x56aa]
        CHECK(weapon_decision(t.c, 1) == 1);
    }
    {
        Rig r;  // cloaked within 3000 ms of DGROUP:0x8e28: no fire, the cloak (0xfe) and shield (7) go down
        setup(r, 1, 0);
        r.s().be8 = 1;
        r.s().type = 7;
        r.o().b2e = 0xfe;
        r.w.w8e28 = r.g.now - 2999u;
        CHECK(weapon_decision(r.c, 1) == 0);
        CHECK(r.host.cloak_calls == 1);
        CHECK(r.host.shield_calls == 1);
        r.w.w8e28 = r.g.now - 3000u;  // `jbe`: 3000 ms after fires [BYTES seg4:0x57b7-0x57c6]
        r.o().b2e = 0;
        CHECK(weapon_decision(r.c, 1) == 1);
    }
    {
        Rig r;  // a tumbling ship goal with W(slot 1)+0x42 != 0 calls the weapon choice first
        setup(r, 1, 0);
        r.calls.words[{1, 0x42}] = 1;  // indexed by the slot number (claim 51 (b))
        r.s(2).be2 = 1;
        weapon_decision(r.c, 1);
        CHECK(r.calls.log.front() == "choice 1");
    }
    {
        Rig r;  // no system selected
        setup(r, 1, 0);
        r.s().weapon_slot = 0;
        CHECK(weapon_decision(r.c, 1) == 0);
        CHECK(r.calls.log.empty());
    }
}

TEST_CASE("weapon use: the waits, the dispatch and the slot") {
    Rig r;
    ShipRecord& S = r.s();
    S.slots_cb() = 8;
    const int weapons[] = {0, 7, 5, 4, 1, 2, 15, 3};
    for (int k = 1; k < 8; ++k) set_ship_b(S, 0xb8 + k, static_cast<std::uint8_t>(weapons[k]));
    // W(7)+0x0a != 0: the launcher seg11:0x7c12(7, 14, s) [BYTES seg4:0x58d1-0x58e9]
    r.calls.words[{7, 0x0a}] = 1;
    // W(7)+0x42 = 0: +0xca = (class << 2) + low byte of W+0x28, 8-bit: (0x50 << 2) + 0xd0 = 0x10
    // [BYTES seg4:0x589c-0x58a7]
    set_ship_b(S, pw::kClass, 0x50);
    r.calls.words[{7, 0x28}] = 0x01d0;
    use_weapon(r.c, 1, 1, 0);
    CHECK(r.calls.has("count 1 5"));  // seg13:0x4eec(s, 5) [BYTES seg4:0x58c4-0x58ce]
    CHECK(r.calls.has("launch 1 14 7"));
    CHECK(r.p().weapon_wait() == 0x10);
    CHECK(S.weapon_slot == 1);
    // W(5)+0x42 != 0: +0xca = 2, +0xd4 = 3·W+0x28 [BYTES seg4:0x58ad-0x58c0]
    r.calls.words[{5, 0x42}] = 1;
    r.calls.words[{5, 0x28}] = 40;
    use_weapon(r.c, 2, 1, 25);
    CHECK(r.calls.has("ball 14 1 25 5"));
    CHECK(r.p().weapon_wait() == 2);
    CHECK(r.p().wait2() == 120);
    use_weapon(r.c, 3, 1, 0);
    CHECK(r.calls.has("ball 14 1 0 4"));
    use_weapon(r.c, 4, 1, 0);
    CHECK(r.calls.has("ring 14 1"));
    use_weapon(r.c, 5, 1, 0);
    CHECK(r.calls.has("tether 14 1 2"));
    use_weapon(r.c, 6, 1, 0);
    CHECK(r.calls.has("tether 14 1 15"));
    use_weapon(r.c, 7, 1, 0);
    CHECK(r.calls.has("lightning 14 1"));
    CHECK(S.weapon_slot == 7);
    // A ship whose object byte +0x2e is set does nothing [BYTES seg4:0x585a-0x5867]
    r.calls.log.clear();
    r.o().b2e = 1;
    use_weapon(r.c, 1, 1, 0);
    CHECK(r.calls.log.empty());
    CHECK(S.weapon_slot == 7);
    // The Action-mode player stamps DGROUP:0x8e28; the Strategy player with a new slot gets
    // DGROUP:0x9524 and seg11:0x2f8c [BYTES seg4:0x586b-0x587e seg4:0x597b-0x599f]
    Rig q;
    q.s(0).slots_cb() = 2;
    set_ship_b(q.s(0), 0xb8 + 1, 3);
    use_weapon(q.c, 1, 0, 0);
    CHECK(q.w.w8e28 == q.g.now);
    q.g.play_mode = 1;
    q.s(0).weapon_slot = 0;
    use_weapon(q.c, 1, 0, 0);
    CHECK(q.sg.w9524 == 1);
    CHECK(q.calls.has("display"));
}

// ------------------------------------------------------------------ seg13:0x0

TEST_CASE("ship mover: a computer ship's think, weapon choice and steer, in order") {
    // Detail 3: the think on DGROUP:0x134e % 4 = 0 on screen or % 6 = 0; the weapon choice on
    // the 9000 ms frame for +0x38 = 1; the steer while +0x16 < 10, adding the turn word (x 3 off
    // screen) [BYTES seg13:0x3e3-0x4d8 seg13:0x4dd-0x501 seg13:0x616-0x688].
    Rig r;
    r.g.last_ship = 1;
    r.s().w38 = 1;
    r.g.dg_134e = 5;  // the mover's increment makes it 6
    r.g.dg_1350 = 0;  // due: the 9000 ms flag
    move_ships(r.f);
    REQUIRE(r.host.order.size() == 3);
    CHECK(r.host.order[0] == "think 1");
    CHECK(r.host.order[1] == "choice 1");
    CHECK(r.host.order[2] == "steer 1");
    CHECK(r.s().turn_acc == 105);  // 35 x 3 off screen
    // The next ten frames only count the accumulator down; the steer comes back once it is
    // below 10: off screen every (3·35 + 10)/10 = 11.5 frames, the 11/12 gaps of ships-0001
    r.host.order.clear();
    std::vector<int> calls;
    for (int frame = 1; frame <= 50; ++frame) {
        const int before = r.host.steers;
        move_ships(r.f);
        if (r.host.steers != before) calls.push_back(frame);
    }
    REQUIRE(calls.size() >= 4);
    for (std::size_t k = 1; k < calls.size(); ++k) {
        const int gap = calls[k] - calls[k - 1];
        CHECK((gap == 11 || gap == 12));
    }
    // Detail 5 makes the first divisor 0 (an INT 0 in the original); the port skips that test
    // and keeps the second (18 - 20 = -2) [DESIGN ADR 0016 decision 4]
    r.g.detail = 5;
    r.host.thinks = 0;
    for (int frame = 0; frame < 101; ++frame) move_ships(r.f);
    CHECK(r.host.thinks == 51);  // the even values of the 0 ... 100 cycle
}

// ------------------------------------------------------------------ ships-0001's series

namespace {

std::filesystem::path oracle_dir(const char* run) {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    std::filesystem::path base = v;
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / run;
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

struct Sample {
    std::int16_t vx = 0, vy = 0, state = 0, onscr = 0, frame = 0;
};

std::vector<Sample> read_series(const std::filesystem::path& p) {
    std::vector<Sample> out;
    std::ifstream in(p);
    std::string line;
    std::getline(in, line);  // time,x,y,vx,vy,state,onscr,frame
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string f[8];
        for (auto& s : f) std::getline(ss, s, ',');
        Sample s;
        s.vx = static_cast<std::int16_t>(std::stoi(f[3]));
        s.vy = static_cast<std::int16_t>(std::stoi(f[4]));
        s.state = static_cast<std::int16_t>(std::stoi(f[5]));
        s.onscr = static_cast<std::int16_t>(std::stoi(f[6]));
        s.frame = static_cast<std::int16_t>(std::stoi(f[7]));
        out.push_back(s);
    }
    return out;
}

std::int16_t quarter3(std::int16_t v) {  // seg11:0x6379: (3v + (sign & 3)) sar 2
    const std::int16_t t = mul16(v, 3);
    return sar16(add16(t, t < 0 ? 3 : 0), 2);
}

// Whether one sample-to-sample velocity change is a step of the port's flight model for a ship
// with thrust word t0, drag d and a mass m in [m0, m1] (the design mass plus fuel / 20): a
// thrust at either heading with multiplier 1 ... 5 (the OverThruster's decay), the brake of
// seg11:0x6874, the Detail-3 drift of seg11:0x6117, each with or without the escort's 3/4.
bool explained(Rig& r, const Sample& a, const Sample& b, std::int16_t t0, std::int16_t d, int m0, int m1) {
    for (int m = m0; m <= m1; ++m) {
        std::vector<Dv> v;
        for (std::int16_t h : {a.frame, b.frame}) {
            for (std::int16_t k = 1; k <= 5; ++k) {
                const Dv t = thrust_dv(h, t0, k, d, static_cast<std::int16_t>(m), a.vx, a.vy);
                v.push_back(Dv{add16(a.vx, t.dx), add16(a.vy, t.dy)});
            }
            const Dv dr = drift_dv(h, d, static_cast<std::int16_t>(m), a.vx, a.vy, false);
            v.push_back(Dv{add16(a.vx, dr.dx), add16(a.vy, dr.dy)});
        }
        r.s().thrust = t0;
        r.s().drag = d;
        r.s().flame_until.set(r.g.now + 1000u);
        r.o().mass = static_cast<std::int16_t>(m);
        r.o().vx = a.vx;
        r.o().vy = a.vy;
        brake(r.f, 1);
        v.push_back(Dv{r.o().vx, r.o().vy});
        const std::size_t n = v.size();
        for (std::size_t k = 0; k < n; ++k) v.push_back(Dv{quarter3(v[k].dx), quarter3(v[k].dy)});
        v.push_back(Dv{quarter3(a.vx), quarter3(a.vy)});
        for (const Dv& e : v)
            if (e.dx == b.vx && e.dy == b.vy) return true;
    }
    return false;
}

void check_series(const char* file, std::int16_t t0, std::int16_t d, int m0, int m1) {
    const auto dir = oracle_dir("ships-0001");
    if (dir.empty() || !std::filesystem::exists(dir / file)) {
        MESSAGE("skipped: work/oracle/ships-0001/" << file << " not available");
        return;
    }
    const auto s = read_series(dir / file);
    REQUIRE(s.size() > 1000);
    Rig r;
    int changed = 0, ok = 0, gaps = 0, gaps_ok = 0, rows = 0, last_turn = -1, turns = 0, unit = 0;
    for (std::size_t k = 1; k < s.size(); ++k) {
        const Sample& a = s[k - 1];
        const Sample& b = s[k];
        if (a.state != 0 || b.state != 0) continue;
        ++rows;
        // The turn: one heading step per steer call; off screen the calls come every 11 or 12
        // frames for the turn word 35 (move_ships), so the gaps between unit steps are 11, 12
        // or a sum of such gaps (calls that kept the heading).
        if (a.frame != b.frame) {
            // A sample can hold two frames (the 20 ms poll against frames of about 23 ms), so a
            // rare two-step change is two steer calls.
            const int step = (b.frame - a.frame + 16) % 16;
            ++turns;
            if (step == 1 || step == 15) ++unit;
            if (last_turn >= 0 && a.onscr == 0 && b.onscr == 0) {
                const int g = rows - last_turn;
                ++gaps;
                if (g == 11 || g == 12 || g == 23 || g == 34 || g == 35 || g == 46) ++gaps_ok;
            }
            last_turn = rows;
        }
        if (a.vx == b.vx && a.vy == b.vy) continue;
        ++changed;
        if (explained(r, a, b, t0, d, m0, m1)) ++ok;
    }
    MESSAGE(std::string(file) << ": " << ok << " of " << changed << " velocity changes are port steps; "
                              << unit << " of " << turns << " heading changes are one step; " << gaps_ok << " of "
                              << gaps << " off-screen turn gaps are multiples of the 11.5-frame cadence");
    CHECK(changed > 1000);
    CHECK(ok * 100 >= changed * 95);
    CHECK(unit * 100 >= turns * 99);
    CHECK(gaps > 100);
    CHECK(gaps_ok * 100 >= gaps * 90);
}

}  // namespace

TEST_CASE("ships-0001: the PLATYPUS's series is made of the port's flight steps") {
    // MASTER.SHP 88 PLATYPUS: thrust 5·27 = 135, body 36: drag 20, mass 10·20·(25/4 + 100)/100 =
    // 212 plus fuel / 20 (tank 40·29 = 1160) [BYTES seg14:0x1c8c-0x1caa seg14:0x1e35-0x1e8f
    // seg11:0x1cfe-0x1d26] [ORACLE ships-0001].
    check_series("g1-ship2.csv", 135, 20, 212, 270);
}

TEST_CASE("ships-0001: the GOLDEN VIPER's series is made of the port's flight steps") {
    // MASTER.SHP 44 GOLDEN VIPER: thrust 5·27 = 135, body 25 VIPER: drag 18, mass
    // 10·22·(27/4 + 100)/100 = 233 plus fuel / 20 (tank 40·25 = 1000) [ORACLE ships-0001].
    check_series("g1-ship1.csv", 135, 18, 233, 283);
}
