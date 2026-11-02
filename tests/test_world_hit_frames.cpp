// A rock hit through the frame's movers (M4 s1 fixer): the ship mover seg13:0x0 and the rock
// mover seg7:0x3bae over 120 frames with the visibility pass seg8:0x4e14 between them, a big rock
// drifting into the resting player. The pair bounces once and parts, as core-0001's hits do
// [ORACLE core-0001]; the comparison's "glued rock" (work/world/m4s1/compare/notes.md) does not
// come from the response.
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/damage.h"
#include "world/update.h"
#include "world/visibility.h"

using namespace is::world;

namespace {

class SimHost : public UpdateHost {
public:
    World* world = nullptr;
    VisibilityGlobals* vis = nullptr;
    ViewState* view = nullptr;
    WaveGlobals* g = nullptr;
    int contacts = 0;
    World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
    const VisibilityGlobals* visible_list() override { return vis; }
    Blitter* blitter() override { return nullptr; }
    void sound(int id, int, int, int) override {
        if (id == 8) ++contacts;
    }
    void draw_sprite(std::int16_t pr, int, int, int) override {
        ObjectRecord& o = world->pools.rec(pool_of(pr), rec_of(pr));
        const ScreenPoint p = world_to_screen(*view, o.x, o.y, 0, 0, 0);
        o.scr_x = p.x;
        o.scr_y = p.y;
    }
};

}  // namespace

TEST_CASE("collide: a rock and the resting player bounce once and part, through the movers") {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    VisibilityGlobals vis;
    is::shell::Rng rng;
    SimHost host;
    host.world = &w;
    host.vis = &vis;
    host.view = &view;
    host.g = &g;
    w.size = 1;
    w.shift = 3;
    g.size = 1;
    g.ship_pool = kPoolShips;
    g.rock_pool = kPoolRocks;
    g.pool5 = 5;
    g.now = 100000;
    view.shift = 3;
    view.play_w = 1024;
    view.play_h = 683;
    w.pools.set_in_use(kPoolShips, 1);
    w.pools.set_in_use(kPoolRocks, 1);
    w.design_ratings[0].armor4a = 8, w.design_ratings[0].armor5a = 6;
    w.design_ratings[0].armor4b = 8, w.design_ratings[0].armor5b = 6;
    w.roster.assign(1, RosterRecord{});
    ShipRecord& s = w.ships[0];
    s.bec = 1;
    s.drag = 10;
    s.thrust = 60;
    s.fuel = 600;
    s.w26 = 82, s.w28 = 81, s.w2a = 82, s.w2c = 81;
    s.has_flame = 1;
    s.turn_acc = 100;
    s.thrust_mult = 1;
    ObjectRecord& p = w.pools.rec(kPoolShips, 0);
    p.size = 33, p.extent = 264, p.mass = 160, p.x = 12800, p.y = 12800, p.vx = 0;
    p.owner_pool = -1;
    p.on_damage.set(kDamageShip);
    ObjectRecord& r = w.pools.rec(kPoolRocks, 0);
    r.size = 44, r.extent = 352, r.mass = 1400, r.x = 12800 + 600, r.y = 12800 + 40, r.vx = -20;
    r.owner_pool = -1;
    r.max_damage = 1000;
    r.on_damage.set(kDamageRock);
    r.hit_sound = 8;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    for (int frame = 0; frame < 120; ++frame) {
        g.now += 28;
        ViewInputs vi;
        vi.player_x = p.x;
        vi.player_y = p.y;
        recentre(view, vi);
        vis.count = 0;
        visibility(w.pools, view, vis, rng, VisibilityHost{});
        p.scr_x = static_cast<std::int16_t>(view.play_w / 2);
        p.scr_y = static_cast<std::int16_t>(view.play_h / 2);
        host.frame_ctx = &c;
        move_ships(c.flight);
        move_rocks(c);
        host.frame_ctx = nullptr;
    }
    // One dispatched contact (the rock's sound 0x08), the ship pushed left and, 100 frames on,
    // the rock's left edge more than 200 px right of the ship's (35 px at the contact).
    CHECK(host.contacts == 1);
    CHECK(p.vx < 0);
    CHECK(r.x - p.x > 8 * 200);
}

namespace {

class EffectHostRec : public UpdateHost {
public:
    World* world = nullptr;
    std::vector<int> calls;
    World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
    void effect(int k) override {
        calls.push_back(k);
        // An entry that ends and starts the others raises DGROUP:0x9434 inside the loop
        if (k == 0) world->w9434 = 3;
    }
};

}  // namespace

TEST_CASE("update: the pass's effect loop reads DGROUP:0x9430 once and DGROUP:0x9434 each time [BYTES seg7:0x972 seg7:0x98b]") {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    is::shell::Rng rng;
    EffectHostRec host;
    host.world = &w;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    wave_pass(c, false);
    CHECK(host.calls.empty());  // DGROUP:0x9430 = 0
    w.w9430 = 1;
    w.w9434 = 0;
    wave_pass(c, false);
    CHECK(host.calls == std::vector<int>{0, 1, 2, 3});
}
