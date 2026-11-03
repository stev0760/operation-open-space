// M4 stage 3, track AMBFLIGHT: the ambulance's call and flight (src/world/ambulance.h;
// work/analysis/m4/s3-ambflight.md with s3-ambflight-verify.md): the refusal table and the
// reply codes of seg16:0xb4e / seg11:0x6fb6, the placement at the nearest gate, the approach's
// v/3 below 120 and the stop, the state sequence 1 -> 2 -> 3 -> 4 with its deadlines and
// sounds, the vanish's deadline DGROUP:0x1e88, the drop-off of a computer ship seg16:0x32a0,
// the re-entry seg16:0x33a2 / seg16:0x3306, the park, and the intercept heading seg8:0x2658 for
// pinned vectors.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/ambulance.h"
#include "world/update.h"
#include "world/wave_end.h"

using namespace is::world;

namespace {

struct Call {
    std::string what;
    int a = 0, b = 0, c = 0, d = 0;
    bool operator==(const Call& o) const { return what == o.what && a == o.a && b == o.b && c == o.c && d == o.d; }
};

class RecHost : public AmbulanceHost {
public:
    std::vector<Call> calls;
    std::vector<int> sounds;
    void sound(int id, int pool, int rec, int) override {
        sounds.push_back(id);
        calls.push_back({"sound", id, pool, rec, 0});
    }
    void play(int id) override { calls.push_back({"play", id}); }
    void buffer(int pool, int rec) override { calls.push_back({"buffer", pool, rec}); }
    void buffer_free(int pool, int rec) override { calls.push_back({"free", pool, rec}); }
    void effect(int mode, int pool, int rec) override { calls.push_back({"effect", mode, pool, rec}); }
    void message(int r, int pool, int rec, int code) override { calls.push_back({"message", r, pool, rec, code}); }
    void purchases(int ship, int mode) override { calls.push_back({"purchases", ship, mode}); }
    void weapon_picture() override { calls.push_back({"picture"}); }
    void draw_sprite(std::int16_t pr, int kind, int slot, int masked) override {
        calls.push_back({"sprite", pr, kind, slot, masked});
    }
    void composite(std::int16_t den, std::int16_t num, int, int, int, int) override {
        calls.push_back({"composite", den, num});
    }
    bool has(const Call& c) const {
        for (const Call& x : calls)
            if (x == c) return true;
        return false;
    }
};

class FrameHost : public UpdateHost {
public:
    World* w = nullptr;
    std::uint16_t hit = 0xffffu;
    World* frame_world(Pools&) override { return w; }
    std::uint16_t collide(std::int16_t) override { return hit; }
};

struct Rig {
    World w;
    AmbulanceState a;
    RecHost host;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    FrameHost fh;
    is::shell::Rng rng;
    JoyState joy;
    JoyButtons jb;
    UpdateCtx u{FlightCtx{w.pools, w.ships, g, keys, fh, rng}, f, view, fh, joy, jb};
    std::uint32_t now = 100000;

    Rig() {
        fh.w = &w;
        g.now = now;
        w.player = 0;
        w.player_game = 1;
        w.w934e = 3;
        w.w9e49 = 10;
        w.gates = 1;
        for (int p = 0; p < kStartupPools; ++p)
            for (int i = 0; i < w.pools.records(p); ++i) {
                ObjectRecord& r = w.pools.rec(p, i);
                r.state = 1;
                r.owner_pool = -1;
            }
        ObjectRecord& A = w.pools.rec(7, 0);
        A.size = 40, A.gap = 6, A.extent = 320, A.mass = 800, A.state = 1;  // setup.cpp's record
        ObjectRecord& S = w.pools.rec(kPoolShips, 0);
        S.state = 0, S.x = 10000, S.y = 10000, S.size = 32, S.gap = 6;
        w.ships[0].bec = 1;
        w.ships[0].wdc = 150;  // credit 1.50 (core.md "The words")
        // Gate 0: its centres (pool 12 records 0, 1) and post 0 (pool 8 record 0).
        w.pools.rec(kPoolGateCentres, 0).x = 6912, w.pools.rec(kPoolGateCentres, 0).y = 13770;
        w.pools.rec(kPoolGateCentres, 1).x = 6912, w.pools.rec(kPoolGateCentres, 1).y = 14200;
        w.pools.rec(kPoolGatePosts, 0).x = 6152, w.pools.rec(kPoolGatePosts, 0).y = 13920;
    }
    AmbulanceCtx ctx() { return AmbulanceCtx{w, a, now, host}; }
    ObjectRecord& A() { return w.pools.rec(7, 0); }
    ObjectRecord& S() { return w.pools.rec(kPoolShips, 0); }
    void step(std::uint32_t dt = 28) {
        now += dt;
        g.now = now;
        AmbulanceCtx c = ctx();
        ambulance_move(u, c);
        g.dg_9e49 = w.w9e49;
    }
};

}  // namespace

TEST_CASE("ambulance: seg16:0xb4e's refusals") {
    struct Row {
        const char* what;
        void (*set)(Rig&);
        int ship;
        std::int16_t want;
    };
    const Row rows[] = {
        {"passes", [](Rig&) {}, 0, 1},
        {"clock below 3 s", [](Rig& r) { r.w.w934e = 2; }, 0, 0},
        {"DGROUP:0x913a", [](Rig& r) { r.w.w913a = 1; }, 0, 0},
        {"DGROUP:0x913c", [](Rig& r) { r.w.w913c = 1; }, 0, 0},
        {"Demon's Gate without 0x94b2", [](Rig& r) { r.w.handover.demons_gate = 1; }, 0, 0},
        {"Demon's Gate with 0x94b2", [](Rig& r) { r.w.handover.demons_gate = 1, r.w.w94b2 = 1; }, 0, 1},
        {"race", [](Rig& r) { r.w.kind = 6; }, 0, 0},
        {"duel", [](Rig& r) { r.w.kind = 7; }, 0, 0},
        {"debt below -350, resources below 200",
         [](Rig& r) { r.w.ships[0].wdc = -500, r.w.ships[0].wde = -1, r.w.ships[0].set_resources(100); }, 0, 0},
        {"debt below -350, resources 200",
         [](Rig& r) { r.w.ships[0].wdc = -600, r.w.ships[0].wde = -1, r.w.ships[0].set_resources(200); }, 0, 1},
        {"another ship below 200", [](Rig& r) { r.w.ships[1].wdc = 199; }, 1, 0},
        {"another ship at 200", [](Rig& r) { r.w.ships[1].wdc = 150, r.w.ships[1].set_resources(50); }, 1, 1},
        {"vanishing", [](Rig& r) { r.A().state = 0, r.A().b2e = 1, r.w.w9e49 = 10; }, 0, 0},
        {"busy (state 8)", [](Rig& r) { r.w.w9e49 = 8; }, 0, 0},
        {"returning (state 9) is retargeted", [](Rig& r) { r.w.w9e49 = 9, r.A().state = 0; }, 0, 1},
    };
    for (const Row& row : rows) {
        Rig r;
        r.w.pools.rec(kPoolShips, 1).state = 0;
        row.set(r);
        AmbulanceCtx c = r.ctx();
        CHECK_MESSAGE(ambulance_call(c, row.ship) == row.want, row.what);
        if (row.want == 1) {
            CHECK(r.w.w9e49 == 0);
            CHECK(r.w.w952e == row.ship);
            CHECK(r.a.dist == 1400);
        }
    }
}

TEST_CASE("ambulance: the reply codes 7, 8 and 132 (seg11:0x6fb6-0x6fd1)") {
    Rig r;
    CHECK(ambulance_reply_code(r.w, 0, 1) == 7);
    CHECK(ambulance_reply_code(r.w, 0, 0) == 8);
    r.w.ships[0].wdc = -301;
    r.w.ships[0].wde = -1;
    CHECK(ambulance_reply_code(r.w, 0, 0) == 132);
    r.w.ships[0].wdc = -300;
    CHECK(ambulance_reply_code(r.w, 0, 0) == 8);
    // The 16-bit sum: resources lift it back over -300.
    r.w.ships[0].wdc = -400;
    r.w.ships[0].set_resources(100);
    CHECK(ambulance_reply_code(r.w, 0, 0) == 8);
}

TEST_CASE("ambulance: the H key calls seg16:0xb4e and shows the reply") {
    Rig r;
    AmbulanceCtx c = r.ctx();
    CHECK(ambulance_key(c, false, false) == 7);
    CHECK(r.host.has({"message", 0, 7, 0, 7}));
    // A second H while it flies: busy (state 0 < 9).
    CHECK(ambulance_key(c, false, false) == 8);
    // The demo and Ctrl/Alt do not call.
    Rig d;
    d.w.player_game = 0;
    AmbulanceCtx dc = d.ctx();
    CHECK(ambulance_key(dc, false, false) == 0);
    CHECK(d.host.calls.empty());
    Rig k;
    AmbulanceCtx kc = k.ctx();
    CHECK(ambulance_key(kc, true, false) == 0);
    CHECK(ambulance_key(kc, false, true) == 0);
    CHECK(k.w.w9e49 == 10);
}

TEST_CASE("ambulance: the placement at the nearest gate") {
    Rig r;
    r.w.gates = 2;
    // Gate 1 (pool 12 record 2, post pool 8 record 2) is nearer the ship.
    r.w.pools.rec(kPoolGateCentres, 2).x = 9000, r.w.pools.rec(kPoolGateCentres, 2).y = 9000;
    r.w.pools.rec(kPoolGatePosts, 2).x = 8240, r.w.pools.rec(kPoolGatePosts, 2).y = 9150;
    AmbulanceCtx c = r.ctx();
    REQUIRE(ambulance_call(c, 0) == 1);
    const ObjectRecord& A = r.A();
    CHECK(A.state == 0);
    CHECK(A.b2e == 0);
    CHECK(A.x == 8240 + 480);
    CHECK(A.y == 9150 - 320);
    CHECK(A.vx == -20);
    CHECK(A.vy == -70);
    CHECK(A.scr_x == -1000);
    CHECK(A.scr_y == -1000);
    CHECK(A.old_scr_x == 100);
    CHECK(A.old_scr_y == 100);
    CHECK(A.on_screen == 0);
    CHECK(r.w.pools.in_use(7) == 1);  // seg7:0x2080(7, -1)
    // With one gate it is post 0.
    Rig one;
    AmbulanceCtx c1 = one.ctx();
    REQUIRE(ambulance_call(c1, 0) == 1);
    CHECK(one.A().x == 6152 + 480);
    CHECK(one.A().y == 13920 - 320);
}

TEST_CASE("ambulance: the approach divides v by 3 below 120 cells and stops below 10") {
    Rig r;
    r.w.w9e49 = 0;
    r.w.w952e = 0;
    ObjectRecord& A = r.A();
    A.state = 0;
    A.x = r.S().x + 400, A.y = r.S().y;  // 50 cells away
    A.vx = 90, A.vy = -60;
    r.a.dist = 50;
    r.a.cooldown = 5;  // no steering this frame
    r.step();
    CHECK(A.vx == 30);
    CHECK(A.vy == -20);
    CHECK(r.w.w9e49 == 0);
    r.a.cooldown = 5;
    r.step();
    CHECK(A.vx == 10);  // 30 / 3: |10| is not below 10
    CHECK(A.vy == -6);
    CHECK(r.w.w9e49 == 0);
    r.a.cooldown = 5;
    r.step();
    // 10 / 3 = 3, -6 / 3 = -2: the stop
    CHECK(A.vx == 0);
    CHECK(A.vy == 0);
    CHECK(r.w.w9e49 == 1);
    CHECK(A.mass == 100);
    CHECK(r.S().b54 == 1);
    CHECK(r.a.frame == 5);
    CHECK(r.a.prev_heading == -1);
    // Above 120 nothing is divided.
    Rig f;
    f.w.w9e49 = 0;
    f.w.w952e = 0;
    f.A().state = 0;
    f.A().vx = 90;
    f.a.dist = 120;
    f.a.cooldown = 5;
    f.step();
    CHECK(f.A().vx == 90);
}

TEST_CASE("ambulance: the pickup, the lift and the deadlines of states 1 to 4") {
    Rig r;
    r.w.w9e49 = 1;
    r.w.w952e = 0;
    ObjectRecord& A = r.A();
    A.state = 0;
    A.mass = 100;
    A.x = r.S().x + 160, A.y = r.S().y;  // 20 cells: no abort
    r.a.dist = 20;
    r.a.tick = 7;            // the next frame is frame 0
    r.w.ships[0].bec = 0;    // a dead ship is taken without turning
    r.w.w1e84 = 5, r.w.w1e80 = 6;
    r.step();
    CHECK(r.w.w9e49 == 2);
    CHECK(r.a.served_alive == 0);
    CHECK(r.w.ships[0].bec == 1);
    CHECK(r.w.ships[0].radar_hide == 1);
    CHECK(r.w.w1e84 == 0);  // the player's death deadlines cancelled
    CHECK(r.w.w1e80 == 0);
    CHECK(r.a.due == r.now + 750u);
    CHECK(A.b54 == 1);
    CHECK(r.host.sounds.back() == 0x23);
    CHECK(r.a.lift == 1);
    // The lift rises to 15 and holds; 750 ms later "Let me guide you in." and state 3.
    const std::uint32_t due = r.a.due;
    while (r.now <= due) r.step();
    CHECK(r.a.lift == 15);
    CHECK(r.w.w9e49 == 3);
    CHECK(r.host.sounds.back() == 0x2f);
    CHECK(r.a.pickup_dist == r.a.dist);
    CHECK(r.host.has({"buffer", kPoolShips, 0}));
    CHECK(r.host.has({"buffer", 7, 0}));
    // State 3: no sprite from the mover (the composite draws it).
    r.host.calls.clear();
    r.step();
    CHECK_FALSE(r.host.has({"sprite", 0x700, 1, 5, 0}));
    // seg13:0x3748 pulls the ship in at 70 a frame and, within 5 cells, sets state 4.
    int frames = 0;
    AmbulanceCtx pull = r.ctx();
    while (r.w.w9e49 == 3 && frames < 100) {
        served_ship_move(r.u.flight, pull, 0);
        ++frames;
    }
    CHECK(r.w.w9e49 == 4);
    CHECK(r.S().x == A.x + 2);
    CHECK(r.a.due == r.now + 1100u);
    CHECK(r.a.lift_step == -1);
    // State 4: the lift comes down (15 frames), then 1100 ms after, the vanish and DGROUP:0x1e88.
    const std::uint32_t due4 = r.a.due;
    r.host.calls.clear();
    while (r.now <= due4 || r.a.lift != 0) r.step();
    CHECK(r.host.has({"effect", 1, 7, 0}));
    CHECK(r.w.w1e88 == r.now + 1000u);
    // The vanish runs (+0x2e = 1): the deadline is rewritten each frame, the vanish not restarted.
    A.b2e = 1;
    r.host.calls.clear();
    r.step();
    CHECK_FALSE(r.host.has({"effect", 1, 7, 0}));
    CHECK(r.w.w1e88 == r.now + 1000u);
    CHECK(r.w.w9e49 == 4);
}

TEST_CASE("ambulance: the abort sends states 1 and 2 back to the approach") {
    Rig r;
    r.w.w9e49 = 2;
    r.w.w952e = 0;
    r.A().state = 0;
    r.a.due = r.now + 100000u;
    r.a.dist = 141;
    r.step();
    CHECK(r.w.w9e49 == 0);
    CHECK(r.a.lift_step == -1);
    CHECK(r.a.spin == 50);
    CHECK(r.a.frame == 5);
}

TEST_CASE("ambulance: the served ship gone sends it home; the park") {
    Rig r;
    r.w.w9e49 = 0;
    r.w.w952e = 0;
    r.A().state = 0;
    r.S().state = 1;
    r.step();
    CHECK(r.w.w9e49 == 9);
    // The first state-9 frame steers to pool 12 record 1 and computes the distance; a near
    // distance parks it on the next frame.
    r.a.dist = 100;
    r.A().x = 6912, r.A().y = 14200;
    r.step();
    CHECK(r.w.w9e49 == 10);
    CHECK(r.w.w952e == -1);
    CHECK(r.host.has({"effect", 1, 7, 0}));
    CHECK(r.a.reentry_due == 0);
}

TEST_CASE("ambulance: seg16:0x32a0, a computer ship's drop-off") {
    Rig r;
    r.w.w952e = 1;
    r.A().state = 1;
    r.A().b2e = 0xff;
    AmbulanceCtx c = r.ctx();
    ambulance_drop(c);
    CHECK(r.host.calls[0] == Call{"purchases", 1, 0});
    CHECK(r.host.calls[1] == Call{"message", 1, 7, 0, 0x54});
    CHECK(r.host.calls[2] == Call{"buffer", 7, 0});
    CHECK(r.host.calls[3] == Call{"effect", 2, 7, 0});
    CHECK(r.A().state == 0);
    CHECK(r.w.w9e49 == 6);
    CHECK(r.a.lift == 15);
    CHECK(r.a.lift_step == 1);
    CHECK(r.a.frame == 5);
    CHECK(r.a.spin == 50);
}

TEST_CASE("ambulance: the tick's DGROUP:0x1e88 runs seg16:0x32a0 unless the player's visit") {
    Rig r;
    is::shell::GameState s;
    WaveEndHost weh;
    s.set(is::shell::kWave);
    s.pass(10000);  // DGROUP:0x1e50 = 10000
    r.w.w952e = 1;
    r.w.w1e88 = 1;
    WaveEndCtx c{r.w, s, weh, nullptr, &r.a, &r.host};
    wave_deadlines(c);
    CHECK(r.w.w1e88 == 0);
    CHECK(r.w.w9e49 == 6);
    CHECK(r.host.has({"purchases", 1, 0}));
}

TEST_CASE("ambulance: the re-entry box seg16:0x33a2 and seg16:0x3306") {
    Rig r;
    r.w.w952e = 0;
    ObjectRecord& A = r.A();
    A.x = 10000, A.y = 10000;
    A.b2e = 0xff;
    r.w.pools.set_in_use(3, 1);
    ObjectRecord& rock = r.w.pools.rec(3, 0);
    rock.state = 0;
    rock.x = 10000 + 0x640 - 1, rock.y = 10000;
    AmbulanceCtx c = r.ctx();
    CHECK_FALSE(ambulance_reentry_clear(c));
    rock.x = 10000 + 0x640;  // PtInRect leaves out the right edge
    CHECK(ambulance_reentry_clear(c));
    rock.x = 10000 - 0x230, rock.y = 10000 - 0x280;
    CHECK_FALSE(ambulance_reentry_clear(c));
    rock.state = 1;
    CHECK(ambulance_reentry_clear(c));
    // The served ship itself does not count.
    r.w.pools.set_in_use(kPoolShips, 1);
    r.S().x = 10000, r.S().y = 10000;
    CHECK(ambulance_reentry_clear(c));
    r.a.reentry_due = 5;
    ambulance_reentry(c);
    CHECK(r.S().state == 0);
    CHECK(A.state == 0);
    CHECK(r.a.reentry_due == 0);
    CHECK(r.a.lift == 1);
    CHECK(r.a.lift_step == 1);
    CHECK(A.mass == 800);
    CHECK(r.w.w9e49 == 6);
    CHECK(r.host.has({"effect", 2, 7, 0}));
    CHECK(r.host.has({"picture"}));
}

TEST_CASE("ambulance: the drop-off pushes the ship out at lift 15 and ends in state 8, then 9") {
    Rig r;
    r.w.w9e49 = 6;
    r.w.w952e = 0;
    ObjectRecord& A = r.A();
    A.state = 0;
    A.x = 10000, A.y = 10000;
    r.S().x = 10002, r.S().y = 10000;
    r.a.lift = 15;
    r.a.pickup_dist = 30;
    AmbulanceCtx c = r.ctx();
    served_ship_move(r.u.flight, c, 0);
    // dx = dy = 0 in cells: dx = -10, D = 10, pushed right at 50
    CHECK(r.S().vx == 50);
    CHECK(r.S().vy == 0);
    int frames = 0;
    while (r.w.w9e49 == 6 && frames < 100) {
        served_ship_move(r.u.flight, c, 0);
        ++frames;
    }
    CHECK(r.w.w9e49 == 8);
    CHECK(r.a.lift_step == -1);
    CHECK(r.w.ships[0].radar_hide == 0);
    // The lift comes down; at 0 state 9 and both `fixed` bytes cleared.
    A.b54 = 1;
    r.S().b54 = 1;
    r.a.dist = 1200;  // phase 64's distance: no park on the first state-9 frame
    for (int k = 0; k < 20 && r.w.w9e49 == 8; ++k) r.step();
    CHECK(r.w.w9e49 == 9);
    CHECK(A.b54 == 0);
    CHECK(r.S().b54 == 0);
    // The lift sounds of state 6: CAPTURED at 2, MISSILE at 14 with the stop for the player.
    Rig s;
    s.w.w9e49 = 6;
    s.w.w952e = 0;
    s.a.lift = 2;
    AmbulanceCtx sc = s.ctx();
    served_ship_move(s.u.flight, sc, 0);
    CHECK(s.host.sounds.back() == 0x23);
    s.a.lift = 14;
    served_ship_move(s.u.flight, sc, 0);
    CHECK(s.host.has({"play", 0x6e}));
    CHECK(s.host.sounds.back() == 0x1c);
}

TEST_CASE("ambulance: seg8:0x1290 and the intercept heading seg8:0x2658 for pinned vectors") {
    CHECK(newton_root(2500, 50) == 50);
    CHECK(newton_root(9, 1) == 3);   // g below 3 becomes 3
    CHECK(newton_root(1600, 20) == 50);
    Pools p;
    ObjectRecord& b = p.rec(7, 0);  // the chaser
    b.x = 8000, b.y = 8000, b.vx = 0, b.vy = 0;
    ObjectRecord& a = p.rec(kPoolShips, 0);  // the target, 100 cells up
    a.x = 8000, a.y = 7200, a.vx = 0, a.vy = 0;
    // Still target: T = 0, V = 50, W = 50, X = 0, Y = -50: straight up.
    CHECK(intercept_heading(p, kPoolShips, 0, 7, 0) == 0);
    // A target crossing left at 30: T = 30, U = 1600, g = 20, W = 50, X = -30, Y = -50.
    a.vx = -30;
    CHECK(intercept_heading(p, kPoolShips, 0, 7, 0) == direction16(-50, -30));
    CHECK(intercept_heading(p, kPoolShips, 0, 7, 0) == 15);
    // Faster than the chaser (T = 200 > V = 50): the plain direction (dy, dx).
    a.vx = -200;
    CHECK(intercept_heading(p, kPoolShips, 0, 7, 0) == direction16(-100, 0));
    // Crossing right at 30 leads the other way.
    a.vx = 30;
    CHECK(intercept_heading(p, kPoolShips, 0, 7, 0) == direction16(-50, 30));
    CHECK(intercept_heading(p, kPoolShips, 0, 7, 0) == 1);
}

TEST_CASE("ambulance: the steering adds (t·500 - v·40) / mass on frames 0 and 1 of 8") {
    Rig r;
    r.w.w9e49 = 0;
    r.w.w952e = 0;
    ObjectRecord& A = r.A();
    A.state = 0;
    A.x = 8000, A.y = 9600;  // 200 cells below the ship at (8000, 8000)
    r.S().x = 8000, r.S().y = 8000;
    r.w.pools.rec(kPoolGateCentres, 1).x = 20000;  // far from the gate
    A.vx = 0, A.vy = 0;
    r.a.dist = 1400;
    r.a.tick = 7;  // frame 0
    r.step();
    // Heading 0 (0, -16): dvy = (-16·500 - 0) / 800 = -10; the integrator moves it.
    CHECK(A.vx == 0);
    CHECK(A.vy == -10);
    CHECK(A.y == 9600 - 10);
    CHECK(r.a.dist == 200);  // measured before the integrator
    // Frame 2: no steering.
    r.a.tick = 1;
    r.step();
    CHECK(A.vy == -10);
}

TEST_CASE("ambulance: the set-up words") {
    AmbulanceState a;
    a.served_alive = 3, a.dist = 4, a.frame = 5;
    ambulance_placement(a);
    CHECK(a.served_alive == 0);
    CHECK(a.dist == 0);
    CHECK(a.frame == 0);
    World w;
    w.w1e88 = 9;
    ambulance_briefing_entry(a, w);
    CHECK(a.served_alive == 1);
    CHECK(w.w1e88 == 0);
    a.set_served_alive_dword(1);
    CHECK(a.sold == 0);
}
