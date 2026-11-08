// The fuel tank (pool 4) and the pods (pool 11) (M4 s3 FUEL): src/world/tank.h and pods.h
// against work/analysis/m4/s3-fuel.md sections 3 and 5 with s3-fuel-verify.md and core-0003's
// pickup.
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/effects.h"
#include "world/pods.h"
#include "world/sprite.h"
#include "world/tank.h"
#include "world/update.h"

using namespace is::world;

namespace {

// The tank's phases from a touch (+0x51 = 10, the touch's own call) to the end, at a step.
std::vector<int> tank_phases(std::int16_t step, int& calls, std::vector<std::pair<int, int>>& in_use,
                             int& sprites) {
    World w;
    w.player = 0;
    w.detail = 3;
    w.w8ffe = step;
    w.pools.set_in_use(kPool4, 1);
    ObjectRecord& o = w.pools.rec(kPool4, 0);
    o.size = 36, o.gap = 6, o.on_screen = 1, o.state = 1, o.race_status = 10, o.blast_size = 0;
    DissolveOut d;
    d.in_use = [&in_use](int p, int r) { in_use.emplace_back(p, r); };
    d.player_sprite = [&sprites] { ++sprites; };
    std::vector<int> phases;
    calls = 0;
    // The touch's call, then the mover's while +0x51 != 0 (seg12:0xc38-0xc4b).
    do {
        phases.push_back(tank_dissolve(w, 0, d));
        ++calls;
    } while (o.race_status != 0);
    return phases;
}

class MoverHost : public UpdateHost {
public:
    World* world = nullptr;
    std::uint16_t answer = 0xffffu;  // the pair finder's answer for every caller
    std::vector<std::tuple<std::int16_t, int, int>> drawn;  // (pool_rec, kind, slot)
    std::vector<int> dissolves;
    std::vector<std::tuple<int, int, int>> puffs;
    std::vector<std::pair<int, int>> destroys;
    World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
    std::uint16_t collide(std::int16_t) override { return answer; }
    void draw_sprite(std::int16_t pr, int kind, int slot, int) override { drawn.emplace_back(pr, kind, slot); }
    void tank_dissolve(int rec) override { dissolves.push_back(rec); }
    void puff(int pool, int rec, int arg) override { puffs.emplace_back(pool, rec, arg); }
    void seg12_274a(int pool, int rec) override { destroys.emplace_back(pool, rec); }
    Blitter* blitter() override { return nullptr; }
};

struct MoverRig {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    is::shell::Rng rng;
    MoverHost host;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    MoverRig() {
        host.world = &w;
        g.now = 10000;
        g.player = 0;
    }
};

}  // namespace

TEST_CASE("tank dissolve: 4, 5 or 10 frames by Detail, the end at phase 9 [BYTES seg12:0xe85-0xeb9 seg12:0x1155-0x1185]") {
    int calls = 0, sprites = 0;
    std::vector<std::pair<int, int>> in_use;
    // DGROUP:0x8ffe = 3 (Detail 0): phases 2, 5, 8, 9.
    CHECK(tank_phases(3, calls, in_use, sprites) == std::vector<int>{2, 5, 8, 9});
    CHECK(calls == 4);
    CHECK(in_use == std::vector<std::pair<int, int>>{{kPool4, -1}});
    // 2 (Detail 1): 1, 3, 5, 7, 9; the player's sprite on every odd phase at Detail 3 and at 9.
    in_use.clear();
    sprites = 0;
    CHECK(tank_phases(2, calls, in_use, sprites) == std::vector<int>{1, 3, 5, 7, 9});
    CHECK(calls == 5);
    CHECK(sprites == 6);
    // 1 (Detail 2, 3): 0 ... 9, about 0.28 s at 36 frames a second (core-0003: gone 0.31 s after
    // the touch's screenshot) [ORACLE core-0003].
    in_use.clear();
    CHECK(tank_phases(1, calls, in_use, sprites) == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    CHECK(calls == 10);
    CHECK(in_use.size() == 1);
}

TEST_CASE("tank dissolve: off screen the counter runs and the end is skipped [BYTES seg12:0xf2a-0xf39]") {
    World w;
    w.w8ffe = 3;
    w.pools.set_in_use(kPool4, 1);
    ObjectRecord& o = w.pools.rec(kPool4, 0);
    o.state = 1, o.race_status = 1, o.on_screen = 0;
    int ends = 0;
    DissolveOut d;
    d.in_use = [&ends](int, int) { ++ends; };
    CHECK(tank_dissolve(w, 0, d) == 9);
    CHECK(o.race_status == 0);
    CHECK(ends == 0);
}

TEST_CASE("tank damage: the wobble below 60, the destroy at 60 [BYTES seg12:0xdc4-0xe5a]") {
    World w;
    WaveGlobals g;
    CollideGlobals cg;
    struct H : CollideHost {
        std::vector<std::pair<int, int>> destroyed, counted;
        void on_destroy(int p, int r) override { destroyed.emplace_back(p, r); }
        void seg7_2080(int p, int r) override { counted.emplace_back(p, r); }
    } h;
    ViewState view;
    is::shell::Rng rng;
    std::int16_t a = 0, b = 0, hh = 0;
    CollideCtx x{w, g, cg, h, view, nullptr, rng, a, b, hh};
    w.pools.set_in_use(kPool4, 2);
    ObjectRecord& o = w.pools.rec(kPool4, 1);
    o.on_damage.set(kDamagePool4);
    o.on_destroy.set(kDestroyGeneric);
    CHECK(call_on_damage(x, kPool4, 1, 30) == 0);
    CHECK(o.flame == 1);
    CHECK(o.b50 == 3);
    CHECK(call_on_damage(x, kPool4, 1, 5) == 0);
    CHECK(o.b50 == 3);  // 35 / 10
    CHECK(o.flame == 1);
    CHECK(call_on_damage(x, kPool4, 1, 25) == 1);  // 60: +0x50 = 6 > 5
    CHECK(h.destroyed == std::vector<std::pair<int, int>>{{kPool4, 1}});
    CHECK(h.counted == std::vector<std::pair<int, int>>{{kPool4, 1}});
    // A small hit on a fresh tank still gives speed 1.
    ObjectRecord& t = w.pools.rec(kPool4, 0);
    t.on_damage.set(kDamagePool4);
    CHECK(tank_damage(x, kPool4, 0, 4) == 0);
    CHECK(t.b50 == 1);
}

TEST_CASE("tank mover: the cell of record 0, the wobble between 5 and 21, a taken tank's dissolve [BYTES seg12:0xae4-0xd5e]") {
    MoverRig r;
    r.w.pools.set_in_use(kPool4, 2);
    for (int i = 0; i < 2; ++i) {
        ObjectRecord& o = r.w.pools.rec(kPool4, i);
        o.size = 36, o.gap = 6, o.on_screen = 1, o.w1c = 1;
    }
    ObjectRecord& o = r.w.pools.rec(kPool4, 0);
    o.flame = 2, o.b50 = 0, o.angle = 20;  // wobbling down with speed 0 (no roll can pass)
    r.w.pools.rec(kPool4, 1).state = 1;
    r.w.pools.rec(kPool4, 1).race_status = 4;
    move_tanks(r.c, r.w);
    // +0x26 = 22 -> 21, +0x2f = -+0x50 = 0; the cell 20 / 5 = 4 drawn before the step.
    CHECK(o.angle == 21);
    CHECK(o.flame == 0);
    CHECK(o.src_x == 4 * 42 + 6);
    CHECK(r.host.drawn == std::vector<std::tuple<std::int16_t, int, int>>{{pool_rec(kPool4, 0), 1, kSlotTank}});
    CHECK(r.host.dissolves == std::vector<int>{1});
    CHECK(o.w1c == 0);  // the mover clears `collided` first
    CHECK(r.host.destroys.empty());
    // With a special time of day nothing wobbles.
    r.w.w9214 = 1;
    o.flame = 3;
    move_tanks(r.c, r.w);
    CHECK(o.angle == 21);
}

TEST_CASE("tank mover: a pool-5 hit deals 125 and the tank is the target [BYTES seg12:0xbc5-0xc33]") {
    MoverRig r;
    r.w.pools.set_in_use(kPool4, 1);
    ObjectRecord& o = r.w.pools.rec(kPool4, 0);
    o.size = 36, o.gap = 6, o.on_screen = 1;
    o.on_damage.set(kDamagePool4);
    o.on_destroy.set(kDestroyGeneric);
    r.w.pools.set_in_use(kPool5, 1);
    r.host.answer = static_cast<std::uint16_t>(pool_rec(kPool5, 0));
    move_tanks(r.c, r.w);
    CHECK(r.f.dg_943a == kPool4);
    CHECK(o.damage == 125);
    CHECK(static_cast<std::int8_t>(o.b50) == 12);
}

TEST_CASE("pod mover: capturable at age 3, the expiry after 500 frames with five puffs [BYTES seg12:0x6bf8-0x6c09 seg12:0x6b80-0x6bad seg12:0x6a60]") {
    MoverRig r;
    r.w.pools.set_in_use(11, 1);
    ObjectRecord& o = r.w.pools.rec(11, 0);
    o.size = 24, o.gap = 4, o.w4a = kSlotPod, o.on_destroy.set(kDestroyPool11);
    PodRecord& P = effect_state().pods[0];
    P = PodRecord{};
    P.kind = 2;
    P.age.set(0x01f40000u);
    for (int k = 0; k < 3; ++k) move_pods(r.c, r.w);
    CHECK(o.b53 == 0);
    move_pods(r.c, r.w);
    CHECK(o.b53 == 1);
    CHECK(o.src_x == 1 * 28 + 4);  // kind 2: cell 1
    CHECK(std::get<1>(r.host.drawn.back()) == 3);
    CHECK(std::get<2>(r.host.drawn.back()) == kSlotPod);
    for (int k = 4; k < 500; ++k) move_pods(r.c, r.w);
    CHECK(pod_age(P) == 500);
    CHECK(o.state == 0);
    move_pods(r.c, r.w);  // age 501 > 500
    CHECK(o.state == 1);
    CHECK(r.host.puffs.size() == 5);
    // Gone: it no longer moves or ages.
    move_pods(r.c, r.w);
    CHECK(pod_age(P) == 501);
    P = PodRecord{};
}

TEST_CASE("pod mover: no expiry in Demon's Gate [BYTES seg12:0x6b8c]") {
    MoverRig r;
    r.g.demons_gate = 1;
    r.w.pools.set_in_use(11, 1);
    ObjectRecord& o = r.w.pools.rec(11, 0);
    o.on_destroy.set(kDestroyPool11);
    PodRecord& P = effect_state().pods[0];
    P = PodRecord{};
    P.kind = 1;
    P.age.set(0x01f40000u);
    for (int k = 0; k < 600; ++k) move_pods(r.c, r.w);
    CHECK(o.state == 0);
    CHECK(pod_age(P) == 600);
    P = PodRecord{};
}

TEST_CASE("pod dissolve: a slot outside 0 ... 100 takes weapon 16's word W(16)-4 = DGROUP:0x3450 [BYTES seg12:0x1378-0x1395 seg10:0x2da8] [DESIGN ADR 0016 decision 3]") {
    World w;
    w.pools.set_in_use(11, 1);
    ObjectRecord& o = w.pools.rec(11, 0);
    o.size = 24, o.gap = 4, o.on_screen = 1, o.race_status = 9, o.w4a = 200, o.blast_size = -1;
    ImageSlotTable images{};
    images[7] = 0x1234;
    RecordingBlitter out;
    DissolveOut d;
    d.out = &out;
    d.images = &images;
    std::vector<std::pair<int, int>> asked;
    const auto words = [&asked](int wpn, int off) {
        asked.emplace_back(wpn, off);
        return wpn == 16 && off == -4 ? std::int16_t{7} : std::int16_t{0};
    };
    CHECK(pod_dissolve(w, 11, 0, d, words) == 0);  // phase 9 - 9
    CHECK(w.pools.bad_index_count == 1);            // DGROUP:0x9498
    CHECK(w.pools.missile(0).weapon == 0x10);       // DGROUP:0x66e4 + 0x1c·0, for a pod too
    CHECK(asked == std::vector<std::pair<int, int>>{{16, -4}});
    // Phase 0's StretchBlt reads weapon 16's sheet, slot 7.
    int stretches = 0;
    for (const BlitCall& b : out.calls) {
        if (b.op != BlitCall::kStretchBlt) continue;
        CHECK(b.src == 0x1234);
        ++stretches;
    }
    CHECK(stretches > 0);
}
