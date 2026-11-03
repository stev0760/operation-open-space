// The icon fragments, pool 1 (M4 s3 FUEL): src/world/fragments.h's mover seg7:0x32d0 and the
// break-up seg7:0x2e02 (effects.h pickup_destroyed) against work/analysis/m4/s3-fuel.md section 4
// with s3-fuel-verify.md claims 38-45.
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

#include "doctest.h"
#include "hud/target.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/effects.h"
#include "world/fragments.h"
#include "world/update.h"

using namespace is::world;

namespace {

class FragHost : public UpdateHost {
public:
    World* world = nullptr;
    bool bitmap = true;
    std::vector<std::pair<std::int16_t, int>> drawn;
    World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
    std::uint16_t collide(std::int16_t) override { return 0xffffu; }
    bool fragment_has_bitmap(int) override { return bitmap; }
    void draw_fragment(std::int16_t pr, int r) override { drawn.emplace_back(pr, r); }
    Blitter* blitter() override { return nullptr; }
};

struct RecHost : CollideHost {
    std::vector<std::tuple<int, int, int>> sounds;
    std::vector<std::pair<int, int>> rows;
    int erases = 0;
    void sound(int id, int pool, int rec, int) override { sounds.emplace_back(id, pool, rec); }
    void column_row(int pool, int rec) override { rows.emplace_back(pool, rec); }
    void erase(int, int, int, int) override { ++erases; }
};

}  // namespace

TEST_CASE("fragments: on screen with a bitmap they drift and draw; off screen every third frame [BYTES seg7:0x32d0-0x33f0 seg7:0x3400]") {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    is::shell::Rng rng;
    FragHost host;
    host.world = &w;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    w.pools.set_in_use(1, 2);
    ObjectRecord& a = w.pools.rec(1, 0);
    a.on_screen = 1, a.x = 5000, a.y = 5000, a.vx = 6, a.vy = -2, a.gap = 6, a.size = 20;
    ObjectRecord& b = w.pools.rec(1, 1);
    b.on_screen = 0, b.x = 100, b.vx = 4;
    f.dg_9142 = 1;
    move_fragments(c, w);
    CHECK(a.x == 5006);
    CHECK(a.y == 4998);
    CHECK(a.src_x == 6);  // u(6)
    CHECK(host.drawn == std::vector<std::pair<std::int16_t, int>>{{pool_rec(1, 0), 0}});
    CHECK(b.x == 100);  // 1 % 3 != 0
    CHECK(f.dg_943a == 1);
    f.dg_9142 = 3;
    move_fragments(c, w);
    CHECK(b.x == 112);  // seg8:0x1b54(h, 3)
    // Without its bitmap an on-screen fragment takes the off-screen step too.
    host.bitmap = false;
    host.drawn.clear();
    move_fragments(c, w);
    CHECK(host.drawn.empty());
    CHECK(a.x == 5006 + 6 + 18);
    // A fragment in `state` 1 is skipped.
    a.state = 1;
    move_fragments(c, w);
    CHECK(a.x == 5006 + 6 + 18);
}

TEST_CASE("fragments: the end above 14 damage through seg7:0x2652 and seg7:0x2e02 [BYTES seg7:0x2e02-0x3099]") {
    World w;
    WaveGlobals g;
    CollideGlobals cg;
    RecHost h;
    ViewState view;
    is::shell::Rng rng;
    std::int16_t a = 0, b = 0, hh = 0;
    CollideCtx x{w, g, cg, h, view, nullptr, rng, a, b, hh};
    w.detail = 1;
    for (auto& e : w.effects) e.frame = kEffectFree;
    w.pools.set_in_use(1, 3);
    ObjectRecord& o = w.pools.rec(1, 2);
    o.size = 20, o.scr_x = 300, o.scr_y = 200, o.x = 4000, o.y = 4100;
    o.max_damage = 14;
    o.on_damage.set(kDamageGeneric);
    o.on_destroy.set(kDestroyPickup);
    w.fragment_icon[2] = 5;
    // Entry 0 holds an ordinary blast still running: it moves to entry 4 (no step copied).
    w.effects[0].frame = 3;
    w.effects[0].end = 9;
    w.effects[0].type = 1;
    w.effects[0].step = 7;
    effect_state().sides.centre[0] = EffectPoint{11, 12};
    CHECK(call_on_damage(x, 1, 2, 14) == 0);
    CHECK(o.state == 0);
    CHECK(call_on_damage(x, 1, 2, 1) == 1);  // 15 > 14
    CHECK(o.state == 1);
    CHECK(h.erases == 1);
    CHECK(h.sounds == std::vector<std::tuple<int, int, int>>{{0x1a, 1, 2}});
    CHECK(h.rows == std::vector<std::pair<int, int>>{{0, 5}});
    const EffectEntry& e = w.effects[0];
    CHECK(e.frame == 1);
    CHECK(e.end == 20);
    CHECK(e.type == 6);
    CHECK(e.side == 4);
    CHECK(e.step == 1);  // entry 0 <= Detail 1
    CHECK(e.pool == 1);
    CHECK(e.rec == 2);
    CHECK(effect_state().sides.centre[0].x == 300);
    CHECK(effect_state().sides.point[0].y == 4100);
    CHECK(w.effects[4].frame == 3);
    CHECK(w.effects[4].type == 1);
    CHECK(w.effects[4].step == 0);
    CHECK(effect_state().sides.centre[4].x == 11);
    CHECK(w.w9430 == 1);
    // Already broken: only the column row again.
    pickup_destroyed(x, 1, 2);
    CHECK(h.erases == 1);
    CHECK(h.rows.size() == 2);
    effect_state().sides = EffectSides{};
}

TEST_CASE("fragments: the target texts of pool 1 [BYTES seg3:0x2cf-0x2ed seg3:0x40b-0x428 seg3:0x16ab]") {
    const is::hud::FragmentTexts t = is::hud::fragment_target_texts("BRAVO.EXE", "Icon Fragment^N");
    CHECK(t.t9628 == "BRAVO.EXE");
    CHECK(t.t8718 == "Icon Fragment");
    CHECK(is::hud::kStringIconFragment == 892);
}
