// M4 stage 7, track LAUNCHED: the remaining launched weapons (src/world/missile.h, shots.h
// object_shot) from work/analysis/m4/s7-launched.md with s7-launched-verify.md: the weapon
// system's named calls, the aim seg8:0x348e with its zero-divisor guard (ruling R1), the
// kind-5 aim seg11:0x8193 with its self-aim, the rear launch seg11:0x881d, ENLIGHTENMENT's
// rand() (ruling R5), weapon 16's team-7 stores (ruling R4), the dodge seg4:0x5514 with
// seg13:0x1076, the fire from a missile seg12:0x64aa, the gun shot of an object seg13:0x1fc2,
// the dissolve, the ends, the boomerang's catch, and the lives and launch sounds of core-0009
// [ORACLE core-0009].
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "doctest.h"
#include "hud/weapons.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/missile.h"
#include "world/shots.h"
#include "world/update.h"
#include "world/visibility.h"

using namespace is::world;

namespace {

struct Call {
    const char* what;
    int a, b, c, d, e;
};

class Host : public MissileHost {
public:
    World* w = nullptr;
    const WeaponWords* words = nullptr;
    const VisibilityGlobals* vis = nullptr;
    std::vector<std::pair<int, int>> sounds;  // (id, rec) of seg10:0x7944 / 0x754a
    std::vector<Call> calls;
    std::vector<std::pair<int, std::int16_t>> touches;
    std::vector<std::pair<int, int>> destroyed;
    Point16 rear{10, 30};
    int paints = 0;
    void sound(int id, int, int rec, int) override { sounds.push_back({id, rec}); }
    void paint_weapons() override { ++paints; }
    void destroy(int pool, int rec) override {
        destroyed.push_back({pool, rec});
        w->pools.rec(pool, rec).state = 1;
        if (pool == kPoolMissiles) missile_destroyed(w->pools, rec, *words);
    }
    void touch(int ship, std::int16_t pr) override { touches.push_back({ship, pr}); }
    void ring_of_death(int pool, int rec) override { calls.push_back({"ring", pool, rec, 0, 0, 0}); }
    void tether_use(int pool, int rec, int x) override { calls.push_back({"tether", pool, rec, x, 0, 0}); }
    void lightning(int pool, int rec) override { calls.push_back({"lightning", pool, rec, 0, 0, 0}); }
    void fuel_ball(int pool, int rec, int life, int kind) override {
        calls.push_back({"fuel_ball", pool, rec, life, kind, 0});
    }
    void wildcard(int pool, int rec) override { calls.push_back({"wildcard", pool, rec, 0, 0, 0}); }
    void pool5_shot(int pool, int rec, int heading, int life, int kind) override {
        calls.push_back({"pool5_shot", pool, rec, heading, life, kind});
    }
    void particle(int kind, int life, int m) override { calls.push_back({"particle", kind, life, m, 0, 0}); }
    void unported(const char* a, int x, int y) override { calls.push_back({a, x, y, 0, 0, 0}); }
    std::int16_t team(const World&, int design) override { return design == 5 ? 7 : 0; }
    const VisibilityGlobals* visible() override { return vis; }
    Point16 muzzle(int, int) override { return rear; }
    int count(const char* what) const {
        int n = 0;
        for (const Call& c : calls) n += std::string(c.what) == what ? 1 : 0;
        return n;
    }
};

class Collide final : public CollideHost {
public:
    std::vector<int> sounds;
    void sound(int id, int, int, int) override { sounds.push_back(id); }
};

class Frame : public UpdateHost {
public:
    Collide collide;
    std::vector<std::pair<int, int>> dissolved;
    World* world = nullptr;
    CollideHost& collide_host() override { return collide; }
    void pod_dissolve(int pool, int rec) override { dissolved.push_back({pool, rec}); }
    void draw_sprite(std::int16_t, int, int, int) override {}
    const VisibilityGlobals* visible_list() override { return nullptr; }
};

// The player, ship 0, at rest at (12800, 12800) heading 0, with weapon w in slot 1 (20), the
// pool-6 record 0 as the placement leaves it (`state` 1, age 0x191), no other object.
struct Rig {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    Frame uh;
    is::shell::Rng rng;
    JoyState joy;
    JoyButtons jb;
    UpdateCtx u{FlightCtx{w.pools, w.ships, g, keys, uh, rng}, f, view, uh, joy, jb};
    MissileState st;
    Host h;
    MissileCtx c{w, g, st, h, rng};
    WeaponWords saved = missile_state().words;

    explicit Rig(int weapon) {
        const is::hud::WeaponTable t(0);
        for (int k = 0; k < kWeaponTableBlocks; ++k) st.words.block(k) = t.block(k);
        missile_state().words = st.words;  // collide.h weapon_word (seg7:0x2e02's W+0x24)
        h.w = &w;
        h.words = &st.words;
        uh.world = &w;
        g.now = 100000;
        g.player = 0;
        g.ship_pool = kPoolShips;
        g.pool5 = 5;
        g.size = 1;
        g.game_frames = 500;
        g.demo = 1;
        w.player = 0;
        w.W = 1024;
        w.H = 683;
        view.play_w = 1024;
        view.play_h = 683;
        view.shift = 3;
        for (int p = 0; p < kPoolCount; ++p) w.pools.set_in_use(p, 0);
        w.pools.set_in_use(kPoolShips, 1);
        w.pools.set_allocated(6, 1);
        ObjectRecord& r0 = w.pools.rec(6, 0);
        r0.state = 1;
        r0.mass = 20;
        r0.owner_pool = -1;
        w.pools.missile(0).age = 0x191;
        ShipRecord& s = w.ships[0];
        s.roster_index = -1;
        s.weapon_slot = 1;
        s.slots_cb() = 2;
        s.fuel = 1000;
        set_byte_at(s, 0xb8 + 1, weapon);
        set_byte_at(s, 0xae + 1, 20);
        ObjectRecord& o = ship();
        o.x = 12800;
        o.y = 12800;
        o.extent = 264;
        o.size = 33;
        o.frame = 0;
        o.mass = 160;
        o.owner_pool = -1;
    }
    ~Rig() { missile_state().words = saved; }
    ObjectRecord& ship() { return w.pools.rec(kPoolShips, 0); }
    ObjectRecord& missile(int m = 0) { return w.pools.rec(6, m); }
    MissileSlot& entry(int m = 0) { return w.pools.missile(m); }
    int count() { return sbyte_at(w.ships[0], 0xae + 1); }
    // The passes until the record stops running (or `limit`).
    int run(int limit = 1000) {
        int n = 0;
        while (missile().state == 0 && n < limit) {
            move_pool6(c, u);
            ++n;
        }
        return n;
    }
};

ObjectRecord& add_rock(World& w, int i, std::int16_t x, std::int16_t y) {
    if (w.pools.in_use(kPoolRocks) <= i) w.pools.set_in_use(kPoolRocks, static_cast<std::int16_t>(i + 1));
    ObjectRecord& k = w.pools.rec(kPoolRocks, i);
    k.state = 0;
    k.mass = 1400;
    k.size = 44;
    k.extent = 352;
    k.x = x;
    k.y = y;
    k.owner_pool = -1;
    return k;
}

}  // namespace

TEST_CASE("launched: the weapon system's switch names the other tracks' routines [BYTES seg11:0xad38-0xadb5]") {
    struct Case {
        int w;
        const char* what;
        int c;  // the third argument (tether w, fuel_ball life)
        int d;  // fuel_ball kind
    };
    for (const Case k : {Case{1, "ring", 0, 0}, Case{2, "tether", 2, 0}, Case{3, "lightning", 0, 0},
                         Case{4, "fuel_ball", 0x1e, 4}, Case{5, "fuel_ball", 0x1e, 5},
                         Case{13, "wildcard", 0, 0}, Case{15, "tether", 0xf, 0}}) {
        Rig r(k.w);
        weapon_system(r.c);
        REQUIRE(r.h.calls.size() == 1);
        const Call& c = r.h.calls[0];
        CHECK(std::string(c.what) == k.what);
        CHECK(c.a == kPoolShips);  // DGROUP:0x9324
        CHECK(c.b == 0);           // DGROUP:0x9646
        CHECK(c.c == k.c);
        CHECK(c.d == k.d);
        CHECK(r.w.pools.in_use(6) == 0);  // nothing launched
    }
    // Weapon 23, the shock, stays an M6 stub with its address (ruling R5).
    Rig r(23);
    weapon_system(r.c);
    REQUIRE(r.h.calls.size() == 1);
    CHECK(std::string(r.h.calls[0].what) == "seg11:0x9daa");
}

TEST_CASE("launched: the default hooks log their addresses once through unported") {
    class Log : public MissileHost {
    public:
        std::vector<std::string> seen;
        void unported(const char* a, int, int) override { seen.emplace_back(a); }
    } h;
    h.ring_of_death(14, 0);
    h.tether_use(14, 0, 2);
    h.lightning(6, 1);
    h.fuel_ball(14, 0, 0x1e, 4);
    h.wildcard(14, 0);
    h.pool5_shot(6, 1, 3, 0xb, 5);
    h.particle(10, 199, 0);
    CHECK(h.seen == std::vector<std::string>{"seg11:0x8a42", "seg11:0x947a", "seg11:0x9fdc", "seg13:0x1318",
                                             "seg11:0x9078", "seg12:0x2db6", "seg6:0x84fc"});
    CHECK(h.relation(0, 1) == 0);
}

TEST_CASE("launched: seg8:0x348e aims at the target's centre, dividing vx by dx [BYTES seg8:0x348e-0x35cf]") {
    World w;
    for (int p = 0; p < kPoolCount; ++p) w.pools.set_in_use(p, 0);
    const AimToScreen same = [](std::int16_t x, std::int16_t y) { return Point16{x, y}; };
    ObjectRecord& o = w.pools.rec(6, 0);
    ObjectRecord& t = w.pools.rec(6, 1);
    // The self-aim of a pellet (extent 72): both corners equal, dx = dy = 36, d = 51:
    // vx = 120·51/36 = 170, vy = 36·120/51 = 84, plus the target's (its own) velocity.
    o.x = 800, o.y = 800, o.extent = 72, o.vx = 30, o.vy = -5;
    CHECK(magnitude(36, 36) == 51);
    CHECK(aim_at(w, same, 0, 6, 0, 6, 0, 120));
    CHECK(o.vx == 30 + 170);
    CHECK(o.vy == -5 + 84);
    // A target straight to the right: (V, 0) plus its velocity.
    o.x = 0, o.y = 0, o.vx = 0, o.vy = 0;
    t.x = 400 - 36, t.y = -36, t.extent = 72, t.vx = 7, t.vy = 9;
    CHECK(aim_at(w, same, 0, 6, 0, 6, 1, 120));
    CHECK(o.vx == 7 + 120);
    CHECK(o.vy == 9 + 0);
    // Nearly straight below: dx = 8, dy = 400, d = magnitude(8, 400) = 400, vx = 120·400/8.
    t.x = 8 - 36, t.y = 400 - 36;
    CHECK(magnitude(8, 400) == 400);
    CHECK(aim_at(w, same, 0, 6, 0, 6, 1, 120));
    CHECK(o.vx == 7 + 6000);
    CHECK(o.vy == 9 + 120);
    // The shift DGROUP:0x9150 applies to both corners, not to the extent / 2.
    t.x = 50 - 4, t.y = -4, t.extent = 8;  // tx = 46 << 3 + 4 = 372, ax = 0
    o.vx = o.vy = 0;
    CHECK(aim_at(w, same, 3, 6, 0, 6, 1, 100));
    CHECK(o.vx == 7 + 100);  // dy = -32 + 4 = -28: d = magnitude(372, -28)
    // Straight below: dx = 0, a divide fault in the original. The port stores nothing and
    // returns false (ruling R1, [DESIGN ADR 0016]).
    t.x = -36, t.y = 400, t.extent = 72;
    o.vx = 11, o.vy = 12;
    CHECK_FALSE(aim_at(w, same, 0, 6, 0, 6, 1, 120));
    CHECK(o.vx == 11);
    CHECK(o.vy == 12);
}

TEST_CASE("launched: the first pellet of a used pool-6 slot aims at itself (verify Omission 12) [BYTES seg11:0x8240-0x82a5]") {
    // PROTECTIVE PELLETS from the ship at (12800, 12800) moving (30, 0): record 0 is in the
    // in-use range and its stale launcher words (0, 0) are not the ship's, so the search adopts
    // record 0 itself at d = 1. seg8:0x348e then sees dx = dy = 36 (both corners are M's).
    Rig r(17);
    r.ship().vx = 30;
    r.w.pools.set_in_use(6, 1);
    weapon_system(r.c);
    const ObjectRecord& m = r.missile();
    CHECK(r.entry().weapon == 17);
    CHECK(m.extent == 72);
    CHECK(m.vx == 30 + 170);
    CHECK(m.vy == 0 + 84);
    // The placement: L.x + 132 - 36, then lo16(5·lo16((36 + 33)·heading[h]))/20 with h the
    // direction of (170, 84) [BYTES seg11:0x81d7-0x8235 seg11:0x84bf-0x855a].
    const std::int16_t h = direction16(84, 170);
    CHECK(m.x == 12800 + 96 + idiv16(mul16(mul16(69, heading_dx(h)), 5), 20));
    CHECK(m.y == 12800 + 96 + idiv16(mul16(mul16(69, heading_dy(h)), 5), 20));
    // Kind 5: no launcher velocity added at step 11, no lock, no dword +0xa0 [BYTES seg11:0x898b].
    CHECK(r.entry().lock == 0);
    CHECK(ship_dword(r.w.ships[0], 0xa0) == 0u);
    CHECK(r.entry().launcher_pool == kPoolShips);
    REQUIRE(!r.h.sounds.empty());
    CHECK(r.h.sounds.back().first == 0x1c);  // MISSILE [ORACLE core-0009]
}

TEST_CASE("launched: a pellet with no target flies straight ahead at 8·heading [BYTES seg11:0x855e-0x86b8]") {
    Rig r(17);
    r.ship().vx = 30;
    r.ship().frame = 4;  // right: (16, 0)
    weapon_system(r.c);
    const ObjectRecord& m = r.missile();
    CHECK(m.vx == 30 + 128);
    CHECK(m.vy == 0);
    // h = direction of (128, 0) = 4: x + lo16((36 + 132)·16)/20 = + 134
    CHECK(direction16(0, 128) == 4);
    CHECK(m.x == 12800 + 96 + 134);
    CHECK(m.y == 12800 + 96);
}

TEST_CASE("launched: the kind-5 scan prefers a farther rock ahead by 2/3 (claim 13 as corrected) [BYTES seg11:0x82af-0x837b]") {
    // Rock A 120 cells behind (below) the ship, rock B 150 cells ahead (above): 150·2/3 = 100
    // < 120, so B is the target; turning the ship away (heading 8) makes A the target.
    for (const int heading : {0, 8}) {
        Rig r(21);
        VisibilityGlobals vis;
        vis.count = 2;
        vis.pool[0] = kPoolRocks, vis.rec[0] = 0;
        vis.pool[1] = kPoolRocks, vis.rec[1] = 1;
        r.h.vis = &vis;
        add_rock(r.w, 0, 12800, 12800 + 8 * 120);
        add_rock(r.w, 1, 12800, 12800 - 8 * 150);
        r.ship().frame = static_cast<std::int16_t>(heading);
        weapon_system(r.c);
        const ObjectRecord& m = r.missile();
        if (heading == 0) {
            CHECK(m.vy < 0);  // toward B, above
        } else {
            CHECK(m.vy > 0);  // toward A, below
        }
        CHECK(r.entry().weapon == 21);
        REQUIRE(!r.h.sounds.empty());
        CHECK(r.h.sounds.back().first == 0x02);  // ZOOM [ORACLE core-0009]
    }
}

TEST_CASE("launched: a kind-5 target at x distance 0 reaches the aim's guard; both placements run (ruling R1) [BYTES seg8:0x358e seg11:0x844d-0x86b8]") {
    // PROTECTIVE PELLETS (extent 72) from the ship at rest heading up: M is at 12800 + 132 - 36
    // = 12896 when seg8:0x348e runs. The one visible rock, extent 352, at x = 12896 - 22 puts its
    // corner (x << 3) + 176 on M's: dx = 0, a divide fault in the original; the port stores
    // nothing [DESIGN ADR 0016]. Step (c) still places M along the heading of (M.v - L.v) =
    // (0, 0), seg8:0x19b0(0, 0) = 12: + lo16(5·lo16((36 + 33)·-16)) / 20 = -276 in x. Step (d)
    // then finds M moving with L: v = 8·heading(0) = (0, -128), placed + 168·heading / 20.
    Rig r(17);
    VisibilityGlobals vis;
    vis.count = 1;
    vis.pool[0] = kPoolRocks, vis.rec[0] = 0;
    r.h.vis = &vis;
    add_rock(r.w, 0, 12896 - 22, 12800 - 8 * 150);
    weapon_system(r.c);
    const ObjectRecord& m = r.missile();
    CHECK(r.entry().weapon == 17);
    CHECK(m.extent == 72);
    CHECK(direction16(0, 0) == 12);
    CHECK(m.vx == shl16(heading_dx(0), 3));
    CHECK(m.vy == shl16(heading_dy(0), 3));
    const std::int16_t hd = direction16(m.vy, m.vx);
    CHECK(m.x == add16(add16(12896, idiv16(mul16(mul16(69, heading_dx(12)), 5), 20)),
                       idiv16(mul16(168, heading_dx(hd)), 20)));
    CHECK(m.y == add16(add16(12896, idiv16(mul16(mul16(69, heading_dy(12)), 5), 20)),
                       idiv16(mul16(168, heading_dy(hd)), 20)));
    SUBCASE("one cell to the side the aim stores and step (d) is not taken") {
        Rig q(17);
        q.h.vis = &vis;
        add_rock(q.w, 0, 12896 - 21, 12800 - 8 * 150);
        weapon_system(q.c);
        CHECK(q.missile().vx != shl16(heading_dx(0), 3));
    }
}

TEST_CASE("launched: the rear launch puts a mine behind the ship [BYTES seg11:0x881d-0x8979]") {
    // REAR-DEPLOYED MINES (extent 176) from the ship heading 0 with the rear muzzle entry
    // (10, 30): x + 80 + lo16(11·hx[8])/4 - 88, y + 240 + 44 - 88; v = -(heading[0] << 2) = (0, 64),
    // less the ship's velocity on the position, plus it on the velocity.
    Rig r(10);
    r.ship().vx = 30;
    r.ship().vy = -8;
    weapon_system(r.c);
    const ObjectRecord& m = r.missile();
    CHECK(m.extent == 176);
    CHECK(m.x == 12800 + 80 + 0 - 88 - 30);
    CHECK(m.y == 12800 + 240 + 44 - 88 + 8);
    CHECK(m.vx == 0 + 30);
    CHECK(m.vy == 64 - 8);
    CHECK(ship_dword(r.w.ships[0], 0xa0) == 0u);  // only the front launch writes it (claim 44)
    REQUIRE(r.h.sounds.size() == 2);
    CHECK(r.h.sounds[1].first == 0x18);  // NEW_DEF [ORACLE core-0009]
    // Heading 4: the muzzle's other side, (f + 8) % 16 = 12, heading[12] = (-16, 0).
    Rig q(10);
    q.ship().frame = 4;
    weapon_system(q.c);
    CHECK(q.missile().x == 12800 + 80 + idiv16(-176, 4) - 88);
    CHECK(q.missile().y == 12800 + 240 + 0 - 88);
    CHECK(q.missile().vx == -64);
    CHECK(q.missile().vy == 0);
}

TEST_CASE("launched: the default rear muzzle entry reads the shot state's table [BYTES seg11:0x8856]") {
    class Plain : public MissileHost {
    } h;
    ShotState& s = shot_state();
    const std::int16_t ox = s.mx(3, 21), oy = s.my(3, 21);
    s.mx(3, 21) = 17;
    s.my(3, 21) = -9;
    const Point16 p = h.muzzle(3, 21);
    CHECK(p.x == 17);
    CHECK(p.y == -9);
    s.mx(3, 21) = ox;
    s.my(3, 21) = oy;
}

TEST_CASE("launched: ENLIGHTENMENT draws one rand() and calls seg6:0x84fc (ruling R5) [BYTES seg11:0x8087-0x80da]") {
    Rig r(22);
    is::shell::Rng ref = r.rng;
    const std::int16_t r1 = static_cast<std::int16_t>(ref.rand());
    weapon_system(r.c);
    CHECK(r.rng.state() == ref.state());
    REQUIRE(r.h.count("particle") == 1);
    const Call* p = nullptr;
    for (const Call& c : r.h.calls)
        if (std::string(c.what) == "particle") p = &c;
    REQUIRE(p != nullptr);
    CHECK(p->a == (r1 % 2 + 1) * 10);
    CHECK(p->b == 199);  // W(22)+0x1e - 1
    CHECK(p->c == 0);    // m
    CHECK(r.entry().weapon == 22);
    CHECK(r.entry().age == 0);  // the Demon's Gate store is overwritten (claim 6)
    CHECK(r.w.ships[0].fuel == 1000 - 150);
}

TEST_CASE("launched: a doughnut points the team-7 ships at itself, the last ship left out (claim 9) [BYTES seg11:0x8119-0x8181]") {
    Rig r(16);
    r.g.last_ship = 2;
    r.w.ships[1].design_index = 5;  // team 7 through Host::team
    r.w.ships[2].design_index = 5;
    r.w.ships[0].design_index = 0;
    weapon_system(r.c);
    const ShipRecord& a = r.w.ships[1];
    CHECK(a.w06 == 6);
    CHECK(a.w08 == 0);
    CHECK(a.w3a == 2);
    CHECK(a.w36 == 2);
    CHECK(a.d94.get() == 100100u);
    const ShipRecord& b = r.w.ships[2];
    CHECK(b.w06 == 0);
    CHECK(b.d94.get() == 0u);
    CHECK(r.w.ships[0].w06 == 0);
    CHECK(r.missile().b53 == 1);
    // Any other weapon clears +0x53.
    Rig q(10);
    q.missile().b53 = 1;
    weapon_system(q.c);
    CHECK(q.missile().b53 == 0);
}

TEST_CASE("launched: the launch sounds and lives by age of core-0009 [ORACLE core-0009] [BYTES seg12:0xe8-0x10b seg12:0x427-0x4f6]") {
    // Unlocked, a record's age jumps from lifetime / 2 to lifetime - 5: PELLETS 35 passes
    // (0.94 - 0.96 s), MINES 37 (1.00 - 1.01 s), STARS 47 (1.26 - 1.28 s), DUMB 55 (1.47 -
    // 1.50 s), DOUGHNUTS 180 (5.6 s) at 26 - 31 ms a frame; the entry ends with age = lifetime.
    struct Case {
        int w, sound, life, passes;
    };
    for (const Case k : {Case{17, 0x1c, 60, 35}, Case{10, 0x18, 64, 37}, Case{21, 0x02, 85, 47},
                         Case{11, 0x1c, 100, 55}, Case{16, 0x18, 350, 180}}) {
        CAPTURE(k.w);
        Rig r(k.w);
        weapon_system(r.c);
        REQUIRE(r.h.sounds.size() == 2);
        CHECK(r.h.sounds[0].first == 0x69);
        CHECK(r.h.sounds[1].first == k.sound);
        CHECK(r.count() == 19);
        r.missile().x = 2000;  // far from the ship
        r.missile().y = 2000;
        CHECK(r.run() == k.passes);
        REQUIRE(r.h.destroyed.size() == 1);
        CHECK(r.entry().age == k.life);
        CHECK(r.entry().weapon == -1);
        CHECK(r.w.pools.in_use(6) == 0);
    }
    // The other launch sounds: 0x1c for 6 and 14, 0x02 for 9, 18, 19, 20, 0x4e for 12.
    for (const std::pair<int, int> k : {std::pair<int, int>{6, 0x1c}, {14, 0x1c}, {9, 0x02}, {18, 0x02},
                                        {19, 0x02}, {20, 0x02}, {12, 0x4e}}) {
        CAPTURE(k.first);
        Rig r(k.first);
        weapon_system(r.c);
        REQUIRE(r.h.sounds.size() == 2);
        CHECK(r.h.sounds[1].first == k.second);
        CHECK(r.entry().weapon == k.first);
    }
}

TEST_CASE("launched: a bottle's end smashes it through seg7:0x2e02 (SHATTER 0x1f) [BYTES seg12:0x464-0x46d seg7:0x2e9e-0x2ec2]") {
    for (const int w : {18, 20}) {
        CAPTURE(w);
        Rig r(w);
        weapon_system(r.c);
        r.missile().x = 2000;
        CHECK(r.run() == 58);  // 53 -> 101 -> 106 unlocked
        CHECK(r.missile().state == 1);
        CHECK(r.h.destroyed.empty());  // not seg12:0x274a
        REQUIRE(r.uh.collide.sounds.size() == 1);
        CHECK(r.uh.collide.sounds[0] == 0x1f);
        CHECK(r.w.pools.in_use(6) == 0);  // seg7:0x2080(6, m)
        CHECK(r.entry().weapon == w);      // seg7:0x2e02 does not free the entry
    }
}

TEST_CASE("launched: a boomerang turns home at its lifetime and breaks up when the launcher is gone (claim 39) [BYTES seg12:0x485-0x4ec]") {
    Rig r(19);
    weapon_system(r.c);
    r.missile().x = 2000;
    r.missile().y = 2000;
    CHECK(r.run(58) == 58);  // age 106 reached: no end, the record still runs
    CHECK(r.missile().state == 0);
    CHECK(r.entry().lock == 1);
    CHECK(r.entry().target_pool == kPoolShips);
    CHECK(r.entry().target_rec == 0);
    CHECK(r.entry().age == 106);
    // Locked, kind 1 steers once age > +0x0e (0): two rand() per pass.
    is::shell::Rng ref = r.rng;
    ref.rand();
    ref.rand();
    move_pool6(r.c, r.u);
    CHECK(r.rng.state() == ref.state());
    CHECK(r.missile().state == 0);
    CHECK(r.uh.collide.sounds.empty());
    // The launcher dies: the next end pass breaks the boomerang up with FLANGE 0x1a.
    r.ship().state = 1;
    move_pool6(r.c, r.u);
    CHECK(r.missile().state == 1);
    REQUIRE(r.uh.collide.sounds.size() == 1);
    CHECK(r.uh.collide.sounds[0] == 0x1a);
    CHECK(r.w.pools.in_use(6) == 0);
}

TEST_CASE("launched: entry +0x0e is never reset by a launch (claim 43 as corrected) [BYTES seg11:0x911a seg12:0x156]") {
    Rig r(19);
    r.entry().steer_delay = 50;  // as a WILDCARD fruit left the slot
    weapon_system(r.c);
    CHECK(r.entry().weapon == 19);
    CHECK(r.entry().steer_delay == 50);
    // Locked at age 30, kind 1 does not steer (no rand()) until age > 50.
    r.entry().lock = 1;
    r.entry().target_pool = kPoolShips;
    r.entry().target_rec = 0;
    r.entry().age = 30;
    r.missile().x = 2000;
    is::shell::Rng ref = r.rng;
    move_pool6(r.c, r.u);
    CHECK(r.rng.state() == ref.state());
    r.entry().age = 51;
    ref.rand();
    ref.rand();
    move_pool6(r.c, r.u);
    CHECK(r.rng.state() == ref.state());
}

TEST_CASE("launched: a record that does not run with +0x51 != 0 dissolves (claim 4) [BYTES seg12:0x4fe-0x516]") {
    Rig r(16);
    weapon_system(r.c);
    r.missile().state = 1;
    r.missile().race_status = 10;  // the touch's dissolve counter
    move_pool6(r.c, r.u);
    REQUIRE(r.uh.dissolved.size() == 1);
    CHECK(r.uh.dissolved[0] == std::make_pair(6, 0));
    r.missile().race_status = 0;
    move_pool6(r.c, r.u);
    CHECK(r.uh.dissolved.size() == 1);
}

TEST_CASE("launched: seg13:0x1076 ranks the closing objects; seg4:0x5514 picks the turn by the cross product [BYTES seg13:0x1076-0x1315 seg4:0x5514-0x5650]") {
    Rig r(6);
    weapon_system(r.c);
    ObjectRecord& m = r.missile();
    m.x = 8000, m.y = 8000, m.vx = 0, m.vy = 0, m.on_screen = 1;
    r.ship().x = 20000;  // the launcher far away
    VisibilityGlobals vis;
    vis.count = 1;
    vis.pool[0] = kPoolRocks, vis.rec[0] = 0;
    // A rock 100 cells right and 10 below, closing at 80: q = (100·80 + 10·0)/d > 0.
    ObjectRecord& k = add_rock(r.w, 0, 8000 + 800, 8000 + 80);
    k.vx = -80;
    CHECK(threat(r.c, &vis, 6, 0) == 0x0300u);
    dodge(r.c, &vis, 0);
    CHECK(r.entry().alt_pool == kPoolRocks);
    CHECK(r.entry().alt_rec == 0);
    CHECK(r.entry().mode == 4);  // 800·0 - 80·(-80) > 0
    // Above the line: mode 3.
    k.y = 8000 - 80;
    dodge(r.c, &vis, 0);
    CHECK(r.entry().mode == 3);
    // Moving away: q <= 0, no threat, mode 2.
    k.vx = 80;
    CHECK(threat(r.c, &vis, 6, 0) == 0xffffu);
    dodge(r.c, &vis, 0);
    CHECK(r.entry().mode == 2);
    // Off screen: mode 2 at once.
    k.vx = -80;
    m.on_screen = 0;
    dodge(r.c, &vis, 0);
    CHECK(r.entry().mode == 2);
    m.on_screen = 1;
    // The entry target is skipped (goal mode 2 for pool 6).
    r.entry().target_pool = kPoolRocks;
    r.entry().target_rec = 0;
    CHECK(threat(r.c, &vis, 6, 0) == 0xffffu);
    r.entry().target_pool = -1;
    // 300 cells or more: rejected.
    k.x = 8000 + 8 * 300, k.y = 8000;
    CHECK(threat(r.c, &vis, 6, 0) == 0xffffu);
    // A score of 60 or more passes only for pool 8 below 2·DGROUP:0x9318 and under 120:
    // d = 100, q = 20 (closing at 20), score = 100 - 30 = 70.
    k.x = 8000 + 800, k.vx = -20;
    CHECK(threat(r.c, &vis, 6, 0) == 0xffffu);
    r.g.gates = 1;
    r.w.pools.set_in_use(8, 3);
    ObjectRecord& post = r.w.pools.rec(8, 1);
    post = k;
    vis.pool[0] = 8, vis.rec[0] = 1;
    CHECK(threat(r.c, &vis, 6, 0) == 0x0801u);
    vis.rec[0] = 2;
    r.w.pools.rec(8, 2) = k;
    CHECK(threat(r.c, &vis, 6, 0) == 0xffffu);  // rec 2 >= 2·1
    // Pools 2, 9 and 4 are never threats.
    for (const int pool : {2, 4, 9}) {
        vis.pool[0] = static_cast<std::uint8_t>(pool), vis.rec[0] = 0;
        r.w.pools.set_in_use(pool, 1);
        ObjectRecord& o = r.w.pools.rec(pool, 0);
        o = k;
        o.vx = -80;
        CHECK(threat(r.c, &vis, 6, 0) == 0xffffu);
    }
}

TEST_CASE("launched: the dodge stores the threat before it finds the launcher (claim 21 as corrected) [BYTES seg4:0x5551-0x5573]") {
    Rig r(6);
    weapon_system(r.c);
    ObjectRecord& m = r.missile();
    m.x = 8000, m.y = 8000, m.vx = 0, m.vy = 0, m.on_screen = 1;
    r.ship().x = 8000 + 800, r.ship().y = 8000, r.ship().vx = -80;
    VisibilityGlobals vis;
    vis.count = 1;
    vis.pool[0] = kPoolShips, vis.rec[0] = 0;
    r.entry().mode = 4;
    dodge(r.c, &vis, 0);
    CHECK(r.entry().alt_pool == kPoolShips);
    CHECK(r.entry().alt_rec == 0);
    CHECK(r.entry().mode == 2);
}

TEST_CASE("launched: seg13:0xd6c's modes 3, 4 and 5 turn across and away from the threat [BYTES seg13:0xf2d-0xf7f]") {
    // The threat 100 cells to the right: mode 2 heads at it (4), mode 5 away (12), modes 4 and
    // 3 at right angles, a quarter away: up-left and down-left.
    TurnArgs a;
    a.ship = -100;
    a.mx = 8000, a.my = 8000, a.tx = 8800, a.ty = 8000;
    const ShipTable ships{};
    a.mode = 2;
    CHECK(turn_toward(a, 0, ships, {}) == 4);
    a.mode = 5;
    CHECK(turn_toward(a, 0, ships, {}) == 12);
    a.mode = 4;
    const std::int16_t h4 = turn_toward(a, 0, ships, {});
    CHECK(h4 == direction16(-100, -25));
    CHECK((h4 == 15 || h4 == 0));
    a.mode = 3;
    const std::int16_t h3 = turn_toward(a, 0, ships, {});
    CHECK(h3 == direction16(100, -25));
    CHECK((h3 == 8 || h3 == 9));
}

TEST_CASE("launched: seg12:0x64aa fires at ages 16, 20, ... by W+0x16 and acts by W+0x14 (claims 27-29) [BYTES seg12:0x64aa-0x6701]") {
    int gun_calls = 0;
    const std::function<void(int, int)> gun = [&](int pool, int rec) {
        CHECK(pool == 6);
        CHECK(rec == 0);
        ++gun_calls;
    };
    // BATTERIES (rule 1, action 3): lightning(6, m) when signed byte +0x50 > 0x19.
    {
        Rig r(14);
        weapon_system(r.c);
        r.missile().b50 = 0x1a;
        for (const int age : {12, 13, 14, 15, 17}) {
            r.entry().age = static_cast<std::int16_t>(age);
            missile_fire(r.c, 0, 14, gun);
        }
        CHECK(r.h.count("lightning") == 0);
        r.entry().age = 16;
        missile_fire(r.c, 0, 14, gun);
        CHECK(r.h.count("lightning") == 1);
        r.missile().b50 = 0x19;
        r.entry().age = 20;
        missile_fire(r.c, 0, 14, gun);
        CHECK(r.h.count("lightning") == 1);
        r.missile().b50 = 0x80;  // signed: -128
        missile_fire(r.c, 0, 14, gun);
        CHECK(r.h.count("lightning") == 1);
    }
    // FIRE DEMONS (rule 2, action 1): one rand() per qualifying pass, fire when r % 3 = 0.
    {
        Rig r(12);
        weapon_system(r.c);
        r.missile().frame = 5;
        is::shell::Rng ref = r.rng;
        int expected = 0;
        for (int age = 16; age < 16 + 4 * 30; age += 4) {
            const std::int16_t v = static_cast<std::int16_t>(ref.rand());
            if (v % 3 == 0) ++expected;
            r.entry().age = static_cast<std::int16_t>(age);
            missile_fire(r.c, 0, 12, gun);
        }
        CHECK(r.rng.state() == ref.state());
        CHECK(expected > 0);
        CHECK(r.h.count("pool5_shot") == expected);
        for (const Call& c : r.h.calls) {
            if (std::string(c.what) != "pool5_shot") continue;
            CHECK(c.a == 6);
            CHECK(c.b == 0);
            CHECK(c.c == 5);    // the missile's frame
            CHECK(c.d == 0xb);  // life
            CHECK(c.e == 5);    // FIREBALL
        }
        r.entry().age = 17;  // no rand() off the age gate
        missile_fire(r.c, 0, 12, gun);
        CHECK(r.rng.state() == ref.state());
    }
    // DEATH HEADS (rule 3, action 4): the on-screen head's frame within one of the heading to an
    // on-screen ship in state 0, ships 0 ... DGROUP:0x9316 inclusive, no wrap (claim 28).
    {
        Rig r(9);
        weapon_system(r.c);
        ObjectRecord& m = r.missile();
        m.on_screen = 1, m.scr_x = 100, m.scr_y = 100;
        ObjectRecord& s = r.ship();
        s.on_screen = 1, s.scr_x = 200, s.scr_y = 100;  // heading 4
        REQUIRE(direction16(0, 100) == 4);
        r.entry().age = 16;
        for (const int f : {3, 4, 5}) {
            m.frame = static_cast<std::int16_t>(f);
            missile_fire(r.c, 0, 9, gun);
        }
        CHECK(gun_calls == 3);
        m.frame = 6;
        missile_fire(r.c, 0, 9, gun);
        CHECK(gun_calls == 3);
        m.frame = 4;
        s.on_screen = 0;
        missile_fire(r.c, 0, 9, gun);
        CHECK(gun_calls == 3);
        s.on_screen = 1;
        m.on_screen = 0;
        missile_fire(r.c, 0, 9, gun);
        CHECK(gun_calls == 3);
        m.on_screen = 1;
        // The plain signed range: frame 0 does not take a ship at heading 15.
        s.scr_x = 100 - 60, s.scr_y = 100 - 150;
        REQUIRE(direction16(-150, -60) == 15);
        m.frame = 0;
        missile_fire(r.c, 0, 9, gun);
        CHECK(gun_calls == 3);
        m.frame = 14;
        missile_fire(r.c, 0, 9, gun);
        CHECK(gun_calls == 4);
        // Ship k = DGROUP:0x9316 counts; ship 0 dead does not.
        s.state = 1;
        r.g.last_ship = 1;
        r.w.pools.set_in_use(kPoolShips, 2);
        ObjectRecord& s1 = r.w.pools.rec(kPoolShips, 1);
        s1.on_screen = 1, s1.state = 0, s1.scr_x = 200, s1.scr_y = 100;
        m.frame = 4;
        missile_fire(r.c, 0, 9, gun);
        CHECK(gun_calls == 5);
        r.g.last_ship = 0;
        missile_fire(r.c, 0, 9, gun);
        CHECK(gun_calls == 5);
    }
}

TEST_CASE("launched: seg13:0x1fc2 fills a gun slot for an object (claims 30-33) [BYTES seg13:0x1fc2-0x2187]") {
    World w;
    WaveGlobals g;
    CollideGlobals cg;
    Collide ch;
    ViewState view;
    view.shift = 3;
    view.play_w = 1024;
    view.play_h = 683;
    is::shell::Rng rng;
    std::int16_t a = 0, b = 0, d = 0;
    CollideCtx x{w, g, cg, ch, view, nullptr, rng, a, b, d};
    ShotState s;
    class Shots : public ShotHost {
    public:
        std::vector<int> races;
        void race_5626(int j) override { races.push_back(j); }
    } sh;
    ShotCtx c{x, s, sh, effect_state()};
    place_shot_slots(s);
    g.ship_pool = kPoolShips;
    g.player = 0;
    g.detail = 3;
    w.W = 1024;
    w.H = 683;
    ObjectRecord& o = w.pools.rec(6, 0);
    o.x = 8000, o.y = 8000, o.extent = 168, o.frame = 4, o.vx = 99, o.vy = 99;
    // Stale words the routine does not write.
    s.word(0, shot::kRicoVx) = 77;
    s.set_dword(0, shot::kRicoAt, 0x12345u);
    s.word(0, shot::kWorldX + 4) = 555;  // trail point 2
    s.word(0, shot::kBarrels) = 2;
    object_shot(c, 6, 0);
    CHECK(s.word(0, shot::kAge) == 0);
    CHECK(s.word(0, shot::kWorldX) == 84 + 128 + 8000);
    CHECK(s.word(0, shot::kWorldY) == 84 + 0 + 8000);
    CHECK(s.word(0, shot::kWorldX + 2) == -100);  // trail point 1
    CHECK(s.word(0, shot::kWorldY + 2) == -100);
    CHECK(s.word(0, shot::kWorldX + 4) == 555);
    CHECK(s.word(0, shot::kOwnerPool) == 6);
    CHECK(s.word(0, shot::kOwnerRec) == 0);
    CHECK(s.word(0, shot::kBarrels) == 1);
    CHECK(s.word(0, shot::kAlive) == 1);
    CHECK(s.word(0, shot::kNoSkip) == 0);
    CHECK(s.word(0, shot::kVx) == 48);  // 3·16, no owner velocity
    CHECK(s.word(0, shot::kVy) == 0);
    CHECK(s.word(0, shot::kLife) == 20);  // 20·1024/1000
    CHECK(s.word(0, shot::kRicoVx) == 77);
    CHECK(s.dword(0, shot::kRicoAt) == 0x12345u);
    REQUIRE(ch.sounds.size() == 1);
    CHECK(ch.sounds[0] == 3);  // ONE_SHOT [ORACLE core-0009]
    CHECK(sh.races.empty());
    // The second shot takes slot 1; then one free slot and no player's slot: no shot.
    object_shot(c, 6, 0);
    CHECK(s.word(1, shot::kAge) == 0);
    object_shot(c, 6, 0);
    CHECK(s.word(2, shot::kAge) == -1);
    CHECK(ch.sounds.size() == 2);
    // A slot with the player's owner words (busy or free) lifts the gate.
    s.word(1, shot::kOwnerPool) = kPoolShips;
    s.word(1, shot::kOwnerRec) = 0;
    object_shot(c, 6, 0);
    CHECK(s.word(2, shot::kAge) == 0);
    // Detail 0: no gate; no free slot: nothing.
    g.detail = 0;
    object_shot(c, 6, 0);
    CHECK(ch.sounds.size() == 3);
    // A kind-6 wave: 0x24 and seg11:0x5626(j) before the scale.
    place_shot_slots(s);
    g.wave_kind = 6;
    object_shot(c, 6, 0);
    CHECK(s.word(0, shot::kLife) == 36);  // 36·1024/1000
    REQUIRE(sh.races.size() == 1);
    CHECK(sh.races[0] == 0);
}

namespace {

// The collision of a homing boomerang with its launcher, as the frame runs it (the rig of
// tests/test_world_missile_end.cpp).
struct CatchScene {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    VisibilityGlobals vis;
    is::shell::Rng rng;
    class FrameHost : public UpdateHost {
    public:
        World* world = nullptr;
        VisibilityGlobals* vis = nullptr;
        Collide collide;
        World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
        const VisibilityGlobals* visible_list() override { return vis; }
        Blitter* blitter() override { return nullptr; }
        CollideHost& collide_host() override { return collide; }
    } host;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    MissileState st;
    Host h;
    MissileCtx mc{w, g, st, h, rng};
    WeaponWords saved = missile_state().words;

    explicit CatchScene(int selected) {
        const is::hud::WeaponTable t(0);
        for (int k = 0; k < kWeaponTableBlocks; ++k) st.words.block(k) = t.block(k);
        missile_state().words = st.words;
        h.w = &w;
        h.words = &st.words;
        host.world = &w;
        host.vis = &vis;
        w.size = 1;
        w.shift = 3;
        g.size = 1;
        g.ship_pool = kPoolShips;
        g.rock_pool = kPoolRocks;
        g.pool5 = 5;
        g.now = 100000;
        g.game_frames = 500;
        w.player = 0;
        view.shift = 3;
        view.play_w = 1024;
        view.play_h = 683;
        for (int p = 0; p < kPoolCount; ++p) w.pools.set_in_use(p, 0);
        w.pools.set_in_use(kPoolShips, 1);
        ObjectRecord& p = w.pools.rec(kPoolShips, 0);
        p.size = 33, p.extent = 264, p.mass = 160, p.x = 8000, p.y = 8000, p.owner_pool = -1;
        ShipRecord& s = w.ships[0];
        s.bec = 1;
        s.roster_index = -1;
        s.weapon_slot = 1;
        s.slots_cb() = 2;
        set_byte_at(s, 0xb8 + 1, selected);
        set_byte_at(s, 0xae + 1, 17);
        // A boomerang (cell 27: size 28, extent 216) over the ship's corner, homing, age 120
        w.pools.set_allocated(kPoolMissiles, 1);
        w.pools.set_in_use(kPoolMissiles, 1);
        ObjectRecord& m = w.pools.rec(kPoolMissiles, 0);
        m.state = 0, m.size = 28, m.gap = 4, m.src_y = 4, m.extent = 216, m.mass = 20;
        m.max_damage = 1, m.blast_size = 7, m.hit_sound = 100, m.owner_pool = -1;
        m.on_damage.set(kDamageGeneric);
        m.on_destroy.set(kDestroyPickup);
        m.x = 8000 + 40, m.y = 8000 + 40;
        MissileSlot& e = w.pools.missile(0);
        e.weapon = 19, e.age = 120, e.mode = 2;
        e.target_pool = kPoolShips, e.target_rec = 0, e.lock = 1;
        e.launcher_pool = kPoolShips, e.launcher_rec = 0;
        ViewInputs vi;
        vi.player_x = p.x;
        vi.player_y = p.y;
        recentre(view, vi);
        for (ObjectRecord* o : {&p, &m}) {
            const ScreenPoint sp = world_to_screen(view, o->x, o->y, 0, 0, 0);
            o->scr_x = sp.x, o->scr_y = sp.y;
            o->on_screen = 1;
        }
        vis.count = 0;
        visibility(w.pools, view, vis, rng, VisibilityHost{});
    }
    ~CatchScene() { missile_state().words = saved; }
    void frame() {
        host.frame_ctx = &c;
        move_pool6(mc, c);
        host.frame_ctx = nullptr;
    }
};

}  // namespace

TEST_CASE("launched: a ship with BOOMERANGS selected catches a boomerang: +0x53, the touch, the count + 1 (claim 42) [BYTES seg12:0x3ad-0x40b]") {
    CatchScene s(19);
    s.frame();
    REQUIRE(s.h.touches.size() == 1);
    CHECK(s.h.touches[0].first == 0);
    CHECK(s.h.touches[0].second == 0x600);
    CHECK(s.w.pools.rec(kPoolMissiles, 0).b53 == 1);
    CHECK(sbyte_at(s.w.ships[0], 0xae + 1) == 18);
    CHECK(s.h.paints == 0);  // the display is not redrawn at a catch [ORACLE core-0009]
    CHECK(s.h.destroyed.empty());
    // With another weapon selected the boomerang is an ordinary hit: no touch.
    CatchScene q(10);
    q.frame();
    CHECK(q.h.touches.empty());
    CHECK(sbyte_at(q.w.ships[0], 0xae + 1) == 17);
}

TEST_CASE("launched: the table words the stage-7 branches read (verified claims 1-3) [BYTES seg10:0x1fb2-0x2ff8]") {
    MissileState st;
    const is::hud::WeaponTable t(0);
    for (int k = 0; k < kWeaponTableBlocks; ++k) st.words.block(k) = t.block(k);
    const WeaponWords& W = st.words;
    for (int w = 0; w < kWeaponTableBlocks; ++w) {
        CAPTURE(w);
        CHECK(W.word(w, wofs::kEnd) != 5);  // only WILDCARD's run-time store makes W(27)+0x24 = 5
        if (W.word(w, wofs::kKind) == 0) continue;
        CHECK(W.word(w, wofs::kOffscreen) == 1);
        CHECK(W.word(w, wofs::kMass) == 20);
    }
    // The dodge: W+0x38 = 1 for 6 and 9 only [BYTES seg10:0x221c seg10:0x2921].
    for (int w = 0; w < kWeaponTableBlocks; ++w) CHECK((W.word(w, wofs::kW38) != 0) == (w == 6 || w == 9));
    // seg12:0x64aa: (W+0x16, W+0x14) = (3, 4) DEATH HEADS, (2, 1) FIRE DEMONS, (1, 3) BATTERIES.
    CHECK(W.word(9, wofs::kFireRule) == 3);
    CHECK(W.word(9, wofs::kW14) == 4);
    CHECK(W.word(12, wofs::kFireRule) == 2);
    CHECK(W.word(12, wofs::kW14) == 1);
    CHECK(W.word(14, wofs::kFireRule) == 1);
    CHECK(W.word(14, wofs::kW14) == 3);
    // The kind-5 aim: W+0x46 = 1 and V = W+0x0e: 120 PELLETS, 95 STARS.
    CHECK(W.word(17, wofs::kKind) == 5);
    CHECK(W.word(17, wofs::kAggression) == 1);
    CHECK(W.word(17, wofs::kClamp) == 120);
    CHECK(W.word(21, wofs::kKind) == 5);
    CHECK(W.word(21, wofs::kAggression) == 1);
    CHECK(W.word(21, wofs::kClamp) == 95);
    // The rear launch: W+0x04 = 2 for 9, 10, 16.
    for (const int w : {9, 10, 16}) CHECK(W.word(w, wofs::kLaunch) == 2);
    // The ends: 4 for 18 and 20, 6 for 19; ENLIGHTENMENT's graphic -1.
    CHECK(W.word(18, wofs::kEnd) == 4);
    CHECK(W.word(20, wofs::kEnd) == 4);
    CHECK(W.word(19, wofs::kEnd) == 6);
    CHECK(W.word(22, wofs::kGraphic) == -1);
}
