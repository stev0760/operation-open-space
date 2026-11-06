// Collisions (src/world/collide.h): the pair finder seg8:0x50aa and its one-collision-per-frame
// rule, the pair test seg8:0x6afa and the orientation word (ADR 0015), the response seg8:0x546c
// with the bounce and its tails, the dispatcher seg8:0x5df2, and the three sites of
// src/world/update.cpp (s1-hit.md 4). The rock-against-player hit reproduces core-0001's hit 5
// losses [ORACLE core-0001].
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/damage.h"
#include "world/update.h"

using namespace is::world;

namespace {

struct Sound {
    int id, pool, rec;
};

class Calls : public CollideHost {
public:
    std::vector<Sound> sounds;
    std::vector<int> messages;
    std::vector<std::pair<int, int>> destroyed;
    void sound(int id, int pool, int rec, int) override { sounds.push_back({id, pool, rec}); }
    void message(int, int, int, int code) override { messages.push_back(code); }
    void destroy(int pool, int rec) override { destroyed.push_back({pool, rec}); }
    std::vector<std::string> book;  // the credit bookkeeping's calls
    void seg8_9028(int k, int code, int pool, int rec) override {
        book.push_back("9028 " + std::to_string(k) + " " + std::to_string(code) + " " + std::to_string(pool) + " " +
                       std::to_string(rec));
    }
    void seg8_81f8(int pool, int rec) override {
        book.push_back("81f8 " + std::to_string(pool) + " " + std::to_string(rec));
    }
};

// TRANSFORMER's collision divisors: SHP Armor 34 gives design words +0x10 ... +0x16 = 34 / 4,
// 34 / 5, 34 / 4, 34 / 5 = 8, 6, 8, 6 (rating.cpp design_rating) [BYTES seg14:0x1ce2-0x1d0e];
// its body has Pull-off Wings (MASTER.BOD flags 0x06: row 3 first) [BYTES seg14:0x1d8c].
void transformer_design(World& w) {
    DesignRating& d = w.design_ratings[0];
    d.armor4a = 8, d.armor5a = 6, d.armor4b = 8, d.armor5b = 6;
    d.special = 3;
    d.repair = 1;
    d.captures = 1;
    d.tank = 40 * 33;
}

// One player and one big rock on screen, the player moving right into the rock.
struct Rig {
    World w;
    WaveGlobals g;
    CollideGlobals c;
    Calls h;
    ViewState view;
    VisibilityGlobals vis;
    is::shell::Rng rng;
    std::int16_t dg_943a = 0, dg_943c = 0, dg_9454 = 0;
    CollideCtx x{w, g, c, h, view, &vis, rng, dg_943a, dg_943c, dg_9454};

    Rig() {
        w.size = 1;
        w.shift = 3;
        w.player = 0;
        w.last_ship = 0;
        w.w9000 = 0;  // Complex 0 [BYTES seg6:0x1214]
        w.player_game = 1;
        transformer_design(w);
        w.roster.assign(1, RosterRecord{});
        g.now = 100000;
        g.size = 1;
        view.shift = 3;
        view.play_w = 1024;
        view.play_h = 683;
        w.pools.set_in_use(kPoolShips, 1);
        w.pools.set_in_use(kPoolRocks, 1);
        ShipRecord& s = w.ships[0];
        s.bec = 1;
        s.roster_index = 0;
        s.design_index = 0;
        s.thrust_mult = 1;
        s.fuel = 0x528;
        s.w26 = 82, s.w28 = 81, s.w2a = 82, s.w2c = 81;
        ObjectRecord& p = player();
        p.scr_x = 500, p.scr_y = 400, p.size = 33, p.extent = 264, p.mass = 160;
        p.on_screen = 1, p.x = 4000, p.y = 3200, p.vx = 66, p.vy = 0;
        p.hit_sound = 7;
        p.owner_pool = -1;
        p.on_damage.set(kDamageShip);
        ObjectRecord& r = rock();
        r.scr_x = 520, r.scr_y = 405, r.size = 44, r.extent = 352, r.mass = 1400;
        r.on_screen = 1, r.x = 4160, r.y = 3240;
        r.hit_sound = 8;
        r.max_damage = 1000;
        r.owner_pool = -1;
        r.on_damage.set(kDamageRock);
        vis.count = 2;
        vis.pool[0] = kPoolRocks, vis.rec[0] = 0;
        vis.pool[1] = kPoolShips, vis.rec[1] = 0;
    }
    ObjectRecord& player() { return w.pools.rec(kPoolShips, 0); }
    ObjectRecord& rock(int i = 0) { return w.pools.rec(kPoolRocks, i); }
    // The rock mover's site seg7:0x3efb-0x4068 on rock i (update.cpp UpdateHost::rock_hit).
    std::uint16_t rock_site(int i) {
        dg_943a = kPoolRocks;
        dg_943c = static_cast<std::int16_t>(i);
        const std::uint16_t hit = find_pair(x, pool_rec(kPoolRocks, i));
        if (hit == 0xffffu) return hit;
        ObjectRecord& o = rock(i);
        dg_9454 = static_cast<std::int8_t>(o.flame);
        const std::int16_t amount = respond(x, o.mass, add16(o.extent, 2), &o.vy, &o.vx, hit);
        dispatch(x, kPoolRocks, i, hit >> 8, hit & 0xff, amount);
        dg_9454 = 0;
        return hit;
    }
};

}  // namespace

TEST_CASE("collide: a rock against the player, core-0001's hit 5 [ORACLE core-0001]") {
    Rig r;
    const std::uint16_t hit = r.rock_site(0);
    REQUIRE(hit == static_cast<std::uint16_t>(pool_rec(kPoolShips, 0)));
    // K = the player is party A (the first test of seg8:0x6afa), the rock B [BYTES seg8:0x6d0e]
    CHECK(r.c.orientation == 1);
    CHECK(r.c.a_pool == kPoolShips);
    CHECK(r.c.b_pool == kPoolRocks);
    // The bounce: eA = (264 + 2) >> 3 + 1 = 34, eB = (352 + 2) >> 3 + 1 = 45, mA 160, mB 1400:
    // dA = 56 for the player, dB = 6 for the rock; B (the rock) moved to (536, 405).
    CHECK(r.c.eA == 34);
    CHECK(r.c.eB == 45);
    CHECK(r.c.dA == 56);
    CHECK(r.c.dB == 6);
    // The amount 56 at seg8:0x45e0: 56·7/8 = 49, (0 + 6)·49/8 = 36, 36·3/4 = 27 (the attacker is
    // not a shot), 2·27 = 54 (no shield, no cloak); 54/8 = 6 for Engine and Thrust, 54/6 = 9 for
    // Gun and Shield. core-0001 hit 5: 82, 81, 82, 81 -> 76, 72, 76, 72.
    const ShipRecord& s = r.w.ships[0];
    CHECK(s.w26 == 76);
    CHECK(s.w28 == 72);
    CHECK(s.w2a == 76);
    CHECK(s.w2c == 72);
    CHECK(r.player().damage == 54);
    // Slot 0x07 (the design's hit sound, SHP +0x1f = 7) and then 0x08 (the rock's) [ORACLE core-0001]
    REQUIRE(r.h.sounds.size() == 2);
    CHECK(r.h.sounds[0].id == 7);
    CHECK(r.h.sounds[0].pool == kPoolShips);
    CHECK(r.h.sounds[1].id == 8);
    CHECK(r.h.sounds[1].pool == kPoolRocks);
    // The velocities from the slots, the caller's (the rock's) through its pointers
    CHECK(r.player().vx == -43);
    CHECK(r.player().vy == -25);
    CHECK(r.rock().vx == 12);
    CHECK(r.rock().vy == 3);
    // B (the rock) to the world from its moved screen position: 536 << 3, 405 << 3 [BYTES seg8:0x5837]
    CHECK(r.rock().x == 4288);
    CHECK(r.rock().y == 3240);
    // Both used up for the frame
    CHECK(r.player().w1c == 1);
    CHECK(r.rock().w1c == 1);
    // The tumble and the lock-out max(12·56, 600) (s1-hit-verify.md 22) [BYTES seg8:0x690f]
    CHECK(s.be2 == 1);
    CHECK(s.lockout.get() == 100000u + 672u);
    // The rock's dispatch: X = the player, credit the player: the last target and the kill
    // scorer's 2·6 [BYTES seg8:0x6283 seg8:0x7c84]; seg7:0x5090 adds 3·6/2 = 9 twice, heat 9/4
    CHECK(r.c.hit_pool == kPoolRocks);
    CHECK(r.c.hit_rec == 0);
    CHECK(r.w.rating == 12u);
    CHECK(r.rock().damage == 18);
    CHECK(static_cast<std::int8_t>(r.rock().flame) == 2);
}

TEST_CASE("collide: one collision per object per frame [BYTES seg8:0x50d7 seg8:0x52e3]") {
    Rig r;
    // A second rock on the player too
    r.w.pools.set_in_use(kPoolRocks, 2);
    ObjectRecord& r2 = r.rock(1);
    r2 = r.rock(0);
    r2.scr_x = 510;
    r.vis.count = 3;
    r.vis.pool[2] = kPoolRocks, r.vis.rec[2] = 1;
    CHECK(r.rock_site(0) == static_cast<std::uint16_t>(pool_rec(kPoolShips, 0)));
    const std::size_t n = r.h.sounds.size();
    // The player is `collided`: rock 1 finds nobody (rock 0 is collided too)
    CHECK(r.rock_site(1) == 0xffffu);
    CHECK(r.h.sounds.size() == n);
    // A caller that collided this frame returns at once
    CHECK(find_pair(r.x, pool_rec(kPoolRocks, 0)) == 0xffffu);
    // The visibility pass clears `collided` (world.md); the next frame meets again
    r.player().w1c = 0;
    r.rock(1).w1c = 0;
    CHECK(r.rock_site(1) == static_cast<std::uint16_t>(pool_rec(kPoolShips, 0)));
}

TEST_CASE("collide: the pool order and the search box of seg8:0x50aa") {
    Rig r;
    // The ship mover (pool 14) never finds the rock (pool 3 < 14) [BYTES seg8:0x52b6]
    r.dg_943a = kPoolShips;
    CHECK(find_pair(r.x, pool_rec(kPoolShips, 0)) == 0xffffu);
    // A candidate whose top-left lies outside the search box: h = (70 << 1) >> 1 = 70
    Rig f;
    f.player().scr_x = 520 - 71;
    f.player().size = 120;
    CHECK(find_pair(f.x, pool_rec(kPoolRocks, 0)) == 0xffffu);
    f.player().scr_x = 520 - 70;
    CHECK(find_pair(f.x, pool_rec(kPoolRocks, 0)) == static_cast<std::uint16_t>(pool_rec(kPoolShips, 0)));
}

TEST_CASE("collide: ADR 0015, an undecided pair keeps the last hit's orientation") {
    Rig r;
    // Rock 1 against rock 0: neither the player nor fixed
    r.w.pools.set_in_use(kPoolRocks, 2);
    ObjectRecord& r1 = r.rock(1);
    r1 = r.rock(0);
    r1.scr_x = 530;
    r.player().on_screen = 0;
    r.vis.count = 2;
    r.vis.pool[1] = kPoolRocks, r.vis.rec[1] = 1;
    r.c.orientation = 0;  // the wave start [DESIGN ADR 0015]
    r.dg_943a = kPoolRocks;
    CHECK(find_pair(r.x, pool_rec(kPoolRocks, 0)) == static_cast<std::uint16_t>(pool_rec(kPoolRocks, 1)));
    CHECK(r.c.orientation == 0);  // K = rock 1 is B
    CHECK(r.c.b_rec == 1);
    CHECK(r.c.a_rec == 0);
    r.rock(0).w1c = r.rock(1).w1c = 0;
    r.c.orientation = 1;  // as a hit with the player left it
    CHECK(find_pair(r.x, pool_rec(kPoolRocks, 0)) == static_cast<std::uint16_t>(pool_rec(kPoolRocks, 1)));
    CHECK(r.c.orientation == 1);  // K = rock 1 is A
    CHECK(r.c.a_rec == 1);
    reset_collide_wave(r.c);
    CHECK(r.c.orientation == 0);
}

TEST_CASE("collide: a gate post against the player, the post is A and the player is pushed") {
    Rig r;
    // Gate post 0 (seg9:0x10d4: fixed, mass 3500, extent 272, hit sound 8, on_damage seg7:0x5090)
    r.w.pools.set_in_use(kPoolRocks, 0);
    r.w.pools.set_in_use(kPoolGatePosts, 1);
    ObjectRecord& post = r.w.pools.rec(kPoolGatePosts, 0);
    post.scr_x = 520, post.scr_y = 405, post.size = 34, post.extent = 272, post.mass = 0xdac;
    post.on_screen = 1, post.x = 4160, post.y = 3240;
    post.b54 = 1;
    post.flame = 2;
    post.hit_sound = 8;
    post.owner_pool = -1;
    post.on_damage.set(kDamageRock);
    r.vis.count = 2;
    r.vis.pool[0] = kPoolGatePosts, r.vis.rec[0] = 0;
    r.dg_943a = kPoolGatePosts;
    r.dg_943c = 0;
    const std::uint16_t hit = find_pair(r.x, pool_rec(kPoolGatePosts, 0));
    REQUIRE(hit == static_cast<std::uint16_t>(pool_rec(kPoolShips, 0)));
    // K is the player -> 1, then C fixed -> 0: C = the post is A (s1-hit-verify.md 34)
    CHECK(r.c.orientation == 0);
    CHECK(r.c.a_pool == kPoolGatePosts);
    CHECK(r.c.b_pool == kPoolShips);
    const std::int16_t px = r.player().x;
    // The post site seg12:0x1d79-0x1dc3 (update.cpp UpdateHost::post_hit)
    const std::int16_t amount = respond(r.x, post.mass, add16(post.extent, 2), &post.vy, &post.vx, hit);
    // The caller is A: its mass 3500 becomes 20000 for a fixed party [BYTES seg8:0x590c]
    CHECK(r.c.mA == 20000);
    CHECK(amount == r.c.dA);
    // The player (B) was dispatched inside the response: sound 7, its lamps hit, pushed back
    REQUIRE(r.h.sounds.size() == 1);
    CHECK(r.h.sounds[0].id == 7);
    CHECK(r.player().x != px);
    CHECK(r.player().x == static_cast<std::int16_t>(r.c.Bx << 3));
    CHECK(r.player().vx == r.c.Bvx);
    CHECK(r.player().vx < 0);
    CHECK(r.w.ships[0].w26 < 82);
    dispatch(r.x, kPoolGatePosts, 0, hit >> 8, hit & 0xff, amount);
    REQUIRE(r.h.sounds.size() == 2);
    CHECK(r.h.sounds[1].id == 8);
    // seg7:0x5090 on a post adds the amount once and returns 0 (s1-hit-verify.md 25)
    CHECK(post.damage == amount);
    CHECK(post.state == 0);
    // The pool-8 deadline now + 140·amount [BYTES seg8:0x6965]
    const std::uint32_t due = static_cast<std::uint16_t>(r.w.pool8[0].at(0x04)) |
                              (static_cast<std::uint32_t>(static_cast<std::uint16_t>(r.w.pool8[0].at(0x06))) << 16);
    CHECK(due == 100000u + static_cast<std::uint32_t>(140 * amount));
    // The player's last target is the post, with no score change for a ship attacker [BYTES seg8:0x62a0]
    CHECK(r.c.hit_pool == kPoolGatePosts);
    CHECK(r.w.w9432 == 0);
}

TEST_CASE("collide: a separating pair is not dispatched and returns -1") {
    Rig r;
    r.player().vx = -6;  // moving away from the rock
    const std::uint16_t hit = r.rock_site(0);
    REQUIRE(hit == static_cast<std::uint16_t>(pool_rec(kPoolShips, 0)));
    CHECK(r.h.sounds.empty());
    CHECK(r.w.ships[0].w26 == 82);
    // seg8:0x698a(0): the player takes its (scaled) slot velocity and `collided`
    CHECK(r.player().w1c == 1);
    CHECK(r.player().vx == r.c.Avx);
    CHECK(dispatch(r.x, kPoolRocks, 0, kPoolShips, 0, -1) == 0);
}

TEST_CASE("collide: the dispatcher's amount rules, the OverThruster ram and the pass-through") {
    Rig r;
    // 2000 < a < 3000: a - 2000 and the 2000-range flag, the long lock-out max(25a, 900)
    r.dg_943a = kPoolRocks;
    dispatch(r.x, kPoolShips, 0, kPoolRocks, 0, 2010);
    CHECK(r.w.ships[0].lockout.get() == 100000u + 900u);
    // The ram: the player's thrust multiplier > 2 against a rock raises message 0xd [BYTES seg8:0x5edd]
    r.w.ships[0].thrust_mult = 5;
    dispatch(r.x, kPoolRocks, 0, kPoolShips, 0, 4);
    REQUIRE(!r.h.messages.empty());
    CHECK(r.h.messages.back() == 0xd);
    // The kill scorer's tables (globals.md "Kill scorer")
    CHECK(kill_points(8) == 3400);
    CHECK(kill_weight(14) == 8);
}

TEST_CASE("collide: seg8:0x2bbe, a screen point to the world") {
    ViewState v;
    v.shift = 3;
    v.x0 = 1000;
    v.y0 = -8;  // a negative origin moves into the world first: -8 + 25600
    const ScreenPoint p = screen_to_world(v, 10, 2, 0);
    CHECK(p.x == 1080);
    CHECK(p.y == 25608 - 25600);
    v.x0 = 25000;
    CHECK(screen_to_world(v, 100, 0, 0).x == 25000 + 800 - 25600);
}

namespace {

// The sites through the UpdateHost defaults (update.cpp): the frame's World and visibility list.
class SiteHost : public UpdateHost {
public:
    World* world = nullptr;
    VisibilityGlobals* vis = nullptr;
    std::vector<int> sounds;
    World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
    const VisibilityGlobals* visible_list() override { return vis; }
    Blitter* blitter() override { return nullptr; }
    void sound(int id, int, int, int) override { sounds.push_back(id); }
};

}  // namespace

TEST_CASE("collide: the rock mover's site through UpdateHost::collide and rock_hit") {
    Rig r;
    SiteHost host;
    host.world = &r.w;
    host.vis = &r.vis;
    WaveGlobals& g = r.g;
    g.rock_pool = kPoolRocks;
    FrameGlobals f;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    UpdateCtx c{FlightCtx{r.w.pools, r.w.ships, g, keys, host, r.rng}, f, r.view, host, joy, jb};
    r.rock().vx = 0;
    move_rocks(c);
    // The rock moved one step (vx 0) and met the player: 0x07 then 0x08, the hit-5 losses
    REQUIRE(host.sounds.size() == 2);
    CHECK(host.sounds[0] == 7);
    CHECK(host.sounds[1] == 8);
    CHECK(r.w.ships[0].w28 == 72);
    CHECK(f.dg_9454 == 0);
    CHECK(f.dg_943a == kPoolRocks);
}

TEST_CASE("collide: the credit bookkeeping's goal stamp is dword +0xa4 (M4 s4, C009)") {
    // core.md Hit dispatcher, Credit bookkeeping 1: the credited ship whose word +0x38 is 1 and
    // whose goal (+0x32, +0x34) is T gets dword +0xa4 = now [BYTES seg8:0x60fe-0x6106]; byte
    // +0x147 (`radar_hide`) is not written.
    Rig r;
    r.g.now = 0x12345678u;
    ShipRecord& s = r.w.ships[0];
    s.w38 = 1;
    s.w32 = kPoolRocks;
    s.w34 = 0;
    dispatch(r.x, kPoolRocks, 0, kPoolShips, 0, 4);
    CHECK(ship_dword(s, 0xa4) == 0x12345678u);
    CHECK(ship_dword(s, 0x144) == 0u);
    CHECK(s.radar_hide == 0);
}

TEST_CASE("collide: DGROUP:0xb0c = 0 skips the defender block and the hit history (M4 s4, C006)") {
    // core.md Credit bookkeeping 2: with T an icon, DGROUP:0xb0c != 0 and the player credited,
    // each defender owned by T gets seg8:0x9028(k, 10, cp, cr) and seg8:0x81f8(T) pushes the
    // hit; the blast clears DGROUP:0xb0c around its dispatcher call, which skips both [BYTES
    // seg8:0x6159 seg8:0x616a seg8:0x6259 seg8:0x5b92].
    Rig r;
    r.w.pools.set_in_use(0, 1);
    ObjectRecord& ic = r.w.pools.rec(0, 0);
    ic.owner_pool = -1;
    ic.mass = 100;
    DefenderEntry d;
    d.owner_pool = 0;
    d.owner_rec = 0;
    r.c.defenders.push_back(d);
    dispatch(r.x, 0, 0, kPoolShips, 0, 4);
    const auto join = [](const std::vector<std::string>& v) {
        std::string t;
        for (const std::string& e : v) t += e + ";";
        return t;
    };
    // The tail seg8:0x99ce's own seg8:0x9028(k, 15, ...) follows on both paths.
    const std::string tail = "9028 0 15 " + std::to_string(kPoolShips) + " 0;";
    CHECK(join(r.h.book) == "9028 0 10 " + std::to_string(kPoolShips) + " 0;81f8 0 0;" + tail);
    r.h.book.clear();
    r.c.w0b0c = 0;
    dispatch(r.x, 0, 0, kPoolShips, 0, 4);
    CHECK(join(r.h.book) == tail);
}
