// M4 stage 5, track WEAPONS: a missile's end by its fuse or by a contact (triage M-7 = S2-1;
// work/analysis/m4/s5-weapons.md 1 and 9.4, verified claims 1-6, 12 as corrected, 13). The
// rock mover runs before the pool-6 mover in a frame (seg7:0xcd8 before seg7:0xcfb), so a
// weapon-8 missile that touches its locked rock is dispatched by the rock's pair first (0x06
// from the record's template hit sound, 0x45 from seg12:0x274a, then the rock's 0x08), while one
// within 40 cells of the fuse point and not touching fuses in the steer (0x45 alone). No code
// change came with it: the port's order is the bytes' order.
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "hud/weapons.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/missile.h"
#include "world/update.h"
#include "world/visibility.h"

using namespace is::world;

TEST_CASE("missile end: seg8:0x285a's cells for the three fuse vectors [BYTES seg12:0x988 seg8:0x285a]") {
    std::int16_t dx = 0, dy = 0;
    // The fuse point of a big rock at (8000, 8000): + size / 2 = (8022, 8022); the missile's
    // centre: x + extent / 2 = x + 92, y 8080 + 92 = 8172.
    CHECK(distance_cells(8022, 8022, 7772, 8172, dx, dy) == 36);  // x 7680: < 40, the fuse
    CHECK(distance_cells(8022, 8022, 8476, 8172, dx, dy) == 60);  // x 8384: neither
    CHECK(distance_cells(8022, 8022, 8412, 8172, dx, dy) == 52);  // x 8320: the squares meet
}

namespace {

// The frame's routines as the program runs them, with seg12:0x274a's parts the order depends
// on: 0x45 at seg12:0x2b0d, `state` 1, the entry's age and weapon [BYTES seg12:0x2a97
// seg12:0x2af5-0x2b0d].
class EndHost : public UpdateHost {
public:
    World* world = nullptr;
    VisibilityGlobals* vis = nullptr;
    std::vector<int> sounds;
    class Collide final : public CollideHost {
    public:
        EndHost* h = nullptr;
        void sound(int id, int, int, int) override { h->sounds.push_back(id); }
        void destroy(int pool, int rec) override { h->destroyed(pool, rec); }
        void on_destroy(int pool, int rec) override { h->destroyed(pool, rec); }
    } collide;
    EndHost() { collide.h = this; }
    void destroyed(int pool, int rec) {
        sounds.push_back(0x45);
        world->pools.rec(pool, rec).state = 1;
        if (pool == kPoolMissiles) missile_destroyed(world->pools, rec, missile_state().words);
    }
    World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
    const VisibilityGlobals* visible_list() override { return vis; }
    Blitter* blitter() override { return nullptr; }
    CollideHost& collide_host() override { return collide; }
    void sound(int id, int, int, int) override { sounds.push_back(id); }
};

struct Scene {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    VisibilityGlobals vis;
    is::shell::Rng rng;
    EndHost host;
    WeaponWords saved = missile_state().words;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};

    explicit Scene(std::int16_t missile_x) {
        const is::hud::WeaponTable t(0);
        for (int k = 0; k < kWeaponTableBlocks; ++k) missile_state().words.block(k) = t.block(k);
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
        g.player = 0;
        w.player = 0;
        view.shift = 3;
        view.play_w = 1024;
        view.play_h = 683;
        for (int p = 0; p < kPoolCount; ++p) w.pools.set_in_use(p, 0);
        // The player 300 px right of the rock, still
        w.pools.set_in_use(kPoolShips, 1);
        ObjectRecord& p = w.pools.rec(kPoolShips, 0);
        p.size = 33, p.extent = 264, p.mass = 160, p.x = 8000 + 2400, p.y = 8000, p.owner_pool = -1;
        w.ships[0].bec = 1;
        w.ships[0].roster_index = -1;
        // The big rock at (8000, 8000), still
        w.pools.set_in_use(kPoolRocks, 1);
        ObjectRecord& r = w.pools.rec(kPoolRocks, 0);
        r.size = 44, r.extent = 352, r.mass = 1400, r.x = 8000, r.y = 8000;
        r.owner_pool = -1;
        r.max_damage = 1000;
        r.on_damage.set(kDamageRock);
        r.hit_sound = 8;
        // The weapon-8 missile, pool 6 record 0 as a launch leaves it, locked on the rock at age
        // 20 (no retarget this pass), both velocities 0
        w.pools.set_allocated(kPoolMissiles, 1);
        w.pools.set_in_use(kPoolMissiles, 1);
        ObjectRecord& m = w.pools.rec(kPoolMissiles, 0);
        m.state = 0, m.size = 24, m.gap = 4, m.src_y = 4, m.extent = 184, m.mass = 20;
        m.max_damage = 1, m.blast_size = 9, m.hit_sound = 6, m.owner_pool = -1;
        m.on_damage.set(kDamageGeneric);
        m.on_destroy.set(kDestroyGeneric);
        m.x = missile_x, m.y = 8080;
        MissileSlot& e = w.pools.missile(0);
        e.weapon = kWeaponRadarMissiles, e.age = 20, e.mode = 2;
        e.target_pool = kPoolRocks, e.target_rec = 0, e.lock = 1;
        e.launcher_pool = kPoolShips, e.launcher_rec = 0;
        // The view on the player, the list, and the screen points of the last draw
        ViewInputs vi;
        vi.player_x = p.x;
        vi.player_y = p.y;
        recentre(view, vi);
        for (ObjectRecord* o : {&p, &r, &m}) {
            const ScreenPoint s = world_to_screen(view, o->x, o->y, 0, 0, 0);
            o->scr_x = s.x, o->scr_y = s.y;
            o->on_screen = 1;
        }
        vis.count = 0;
        visibility(w.pools, view, vis, rng, VisibilityHost{});
    }
    ~Scene() { missile_state().words = saved; }
    // seg7:0x3bae (seg7:0xcd8), then seg12:0x0 (seg7:0xcfb)
    void frame() {
        host.frame_ctx = &c;
        move_rocks(c);
        move_missiles(w, c);
        host.frame_ctx = nullptr;
    }
};

}  // namespace

TEST_CASE("missile end: from the left the fuse fires before any contact (one 0x45) [ORACLE core-0002]") {
    Scene s(7680);
    std::vector<int> fuse_sounds;
    MissileLinks saved = missile_links();
    missile_links().destroy = [&](int pool, int rec) { s.host.destroyed(pool, rec); };
    missile_links().sound = [&](int id, int, int, int) { fuse_sounds.push_back(id); };
    s.frame();
    missile_links() = saved;
    CHECK(s.host.sounds == std::vector<int>{0x45});
    CHECK(s.w.pools.rec(kPoolMissiles, 0).state == 1);
    CHECK(s.w.pools.rec(kPoolMissiles, 0).blast_size == 18);  // `blast_size` <<= 1 [BYTES seg12:0x99e]
    CHECK(s.w.pools.missile(0).weapon == -1);
}

TEST_CASE("missile end: from the right the contact comes first (0x06, 0x45, 0x08) [ORACLE core-0006]") {
    Scene s(8320);
    MissileLinks saved = missile_links();
    int steer_destroys = 0;
    missile_links().destroy = [&](int, int) { ++steer_destroys; };
    s.frame();
    missile_links() = saved;
    CHECK(s.host.sounds == std::vector<int>{0x06, 0x45, 0x08});
    CHECK(s.w.pools.rec(kPoolMissiles, 0).state == 1);
    CHECK(steer_destroys == 0);  // seg12:0x0 skips the record (`state` 1, weapon -1)
}
