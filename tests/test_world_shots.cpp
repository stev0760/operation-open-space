// The gun and its shots (src/world/shots.h): the gun seg11:0x4e0a with core-0002's slot words,
// the fire rate and the slot scan's refusals, the muzzle tables, the brushes, the shot update
// seg8:0x3aec, the power of seg8:0x6f3c, a gun hit on a rock through rock_damage's pool-30
// guard, the fire poll seg11:0x4db0 [ORACLE core-0002].
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/damage.h"
#include "world/input.h"
#include "world/shots.h"

using namespace is::world;

namespace {

class Calls : public CollideHost {
public:
    std::vector<std::pair<int, int>> sounds;  // (id, pool)
    void sound(int id, int pool, int, int) override { sounds.push_back({id, pool}); }
};

class Draws : public ShotHost {
public:
    std::vector<int> resets, brushes, thrusts;
    struct Blt {
        std::int16_t x, y, w, h;
        std::uint32_t rop;
    };
    std::vector<Blt> blts;
    void sound_744c(int id) override { resets.push_back(id); }
    void select_brush(int n) override { brushes.push_back(n); }
    void patblt(std::int16_t x, std::int16_t y, std::int16_t w, std::int16_t h, std::uint32_t rop) override {
        blts.push_back({x, y, w, h, rop});
    }
    void thrust_step(int mode, int ship) override { thrusts.push_back(mode * 100 + ship); }
};

struct Rig {
    World w;
    WaveGlobals g;
    CollideGlobals c;
    Calls h;
    ViewState view;
    is::shell::Rng rng;
    VisibilityGlobals vis;
    std::int16_t dg_943a = 0, dg_943c = 0, dg_9454 = 0;
    CollideCtx x{w, g, c, h, view, &vis, rng, dg_943a, dg_943c, dg_9454};
    ShotState s;
    Draws d;
    EffectState fx;
    ShotCtx sc{x, s, d, fx};

    Rig() {
        w.W = 1024;  // lifetime W / 10 = 102 [ORACLE core-0002]
        w.H = 700;
        w.size = 1;
        g.size = 1;
        g.detail = 3;
        g.dg_9006 = 7;
        g.now = 50000;
        g.game_frames = 100;
        g.player = 0;
        g.dg_952e = -1;
        w.player = 0;
        w.last_ship = 1;
        w.roles = wave_roles(false);
        view.play_w = 1024;
        view.play_h = 700;
        view.shift = 3;
        view.x0 = view.y0 = 8000;
        view.x0l = view.y0l = 8000;
        w.pools.set_in_use(kPoolShips, 2);
        for (int i = 0; i < 2; ++i) {
            ShipRecord& S = w.ships[static_cast<std::size_t>(i)];
            S.w28 = 81;  // the gun's health
            S.w24 = 1;   // TRANSFORMER: one barrel [ORACLE core-0002]
            S.d8c.set(1);
            S.roster_index = -1;
            S.bec = 1;
            set_byte_at(S, 0xe0, 1);
            ObjectRecord& O = w.pools.rec(kPoolShips, i);
            O.x = 12000;
            O.y = 12000;
            O.frame = 1;  // heading 1: DGROUP:0x1050 entry (6, -15)
            O.on_screen = 1;
            O.mass = 250;
        }
        place_shot_slots(s);
    }
    void frame() {
        run_shots(sc);
        ++g.game_frames;
    }
};

}  // namespace

TEST_CASE("shots: core-0002's slot words of a new shot") {
    Rig r;
    // The velocity (18, -45) is 3·(6, -15), heading 1 of DGROUP:0x1050, plus
    // ((v / 10)·10) / DGROUP:0x9006 = 0 for a ship slower than 10 per step.
    CHECK(heading_dx(1) * 3 == 18);
    CHECK(heading_dy(1) * 3 == -45);
    r.w.pools.rec(kPoolShips, 0).vx = 9;
    r.w.pools.rec(kPoolShips, 0).vy = -9;
    fire(r.sc, 0);
    CHECK(r.s.dg_594 == 7);
    CHECK(r.s.word(0, shot::kVx) == 18);
    CHECK(r.s.word(0, shot::kVy) == -45);
    CHECK(r.s.word(0, shot::kLife) == 102);
    CHECK(r.s.word(0, shot::kAge) == 0);
    CHECK(r.s.word(0, shot::kBarrels) == 1);
    CHECK(r.s.word(0, shot::kOwnerPool) == 14);
    CHECK(r.s.word(0, shot::kOwnerRec) == 0);
    CHECK(r.s.word(0, shot::kHits) == 0);
    // Ship byte +0xcc 7, the frame stamp +0xa0, DGROUP:0x9432 + 1 [ORACLE core-0002]
    CHECK(sbyte_at(r.w.ships[0], 0xcc) == 7);
    CHECK(ship_dword(r.w.ships[0], 0xa0) == 100u);
    CHECK(r.w.w9432 == 1);
    // The reset 0x69, then 0x03 at the shot (pool 0x1e, slot 0)
    CHECK(r.d.resets == std::vector<int>{0x69});
    REQUIRE(r.h.sounds.size() == 1);
    CHECK(r.h.sounds[0] == std::make_pair(3, 0x1e));
    // The player's start goes back by the ship's velocity; the muzzle table is empty here
    CHECK(r.s.word(0, shot::kWorldX) == 12000 - 9);
    CHECK(r.s.word(0, shot::kWorldY) == 12000 + 9);
    // A faster ship adds ((v / 10)·10) / 7: 75 -> 70 / 7 = 10
    Rig q;
    q.w.pools.rec(kPoolShips, 0).vx = 75;
    fire(q.sc, 0);
    CHECK(q.s.word(0, shot::kVx) == 28);
}

TEST_CASE("shots: one shot per 5 frames, slots 0 ... 4 in turn, age 15 behind a shot in flight") {
    Rig r;
    std::set<int> used;
    int shots = 0;
    for (int f = 0; f < 60; ++f) {
        const std::uint32_t before = ship_dword(r.w.ships[0], 0xa0);
        std::array<std::int16_t, kShotSlots> ages{};
        for (int j = 0; j < kShotSlots; ++j) ages[static_cast<std::size_t>(j)] = r.s.word(j, shot::kAge);
        fire(r.sc, 0);
        if (ship_dword(r.w.ships[0], 0xa0) != before || f == 0) {
            ++shots;
            for (int j = 0; j < kShotSlots; ++j)
                if (ages[static_cast<std::size_t>(j)] == -1 && r.s.word(j, shot::kAge) != -1) {
                    used.insert(j);
                    // The first shot starts at 0, every later one at 15 (a shot of the ship flies)
                    CHECK(r.s.word(j, shot::kAge) == (f == 0 ? 0 : 15));
                }
        }
        r.frame();
        // Age + 5 per frame (+1 per sub-step) [ORACLE core-0002]
    }
    CHECK(shots == 12);
    CHECK(used == std::set<int>{0, 1, 2, 3, 4});
    CHECK(r.s.dg_5cc == 300u);
}

TEST_CASE("shots: the slot scan's refusals and the bookkeeping without a slot") {
    SUBCASE("a busy own slot of age <= W / 50 = 20 refuses, before the bookkeeping") {
        Rig r;
        r.s.word(3, shot::kAge) = 20;
        r.s.word(3, shot::kOwnerPool) = 14;
        r.s.word(3, shot::kOwnerRec) = 0;
        fire(r.sc, 0);
        CHECK(ship_dword(r.w.ships[0], 0xa0) == 0u);
        CHECK(r.s.word(0, shot::kAge) == -1);
        r.s.word(3, shot::kAge) = 21;
        fire(r.sc, 0);
        CHECK(r.s.word(0, shot::kAge) == 15);
    }
    SUBCASE("a computer ship needs two free slots, or one whose stale owner is the player") {
        Rig r;
        r.g.play_mode = 0;
        // Ship 1 aims at nothing in particular: its goal (0, 0) on screen at its own point
        // gives dir 4 or 12; face it so the aim test passes.
        ObjectRecord& O = r.w.pools.rec(kPoolShips, 1);
        r.w.ships[1].w32 = 14;
        r.w.ships[1].w34 = 0;
        r.w.pools.rec(kPoolShips, 0).scr_x = 700;
        r.w.pools.rec(kPoolShips, 0).scr_y = 500;
        O.scr_x = 500;
        O.scr_y = 500;
        O.frame = 4;  // (16, 0): the player lies to the right
        for (int j = 0; j < 6; ++j) {
            r.s.word(j, shot::kAge) = 30;
            r.s.word(j, shot::kOwnerPool) = 14;
            r.s.word(j, shot::kOwnerRec) = 1;
        }
        r.s.word(6, shot::kOwnerPool) = 14;
        r.s.word(6, shot::kOwnerRec) = 1;  // the one free slot, last used by ship 1
        fire(r.sc, 1);
        CHECK(r.s.word(6, shot::kAge) == -1);  // one free slot, not the player's: no shot
        CHECK(r.d.thrusts == std::vector<int>{501});  // seg11:0x5ab6(5, 1) ran
        r.s.word(6, shot::kOwnerRec) = 0;  // the free slot's stale owner is the player
        fire(r.sc, 1);
        CHECK(r.s.word(6, shot::kAge) == 15);
        CHECK(r.s.word(6, shot::kOwnerRec) == 1);
    }
    SUBCASE("all seven busy: the player's bookkeeping runs, no slot is filled") {
        Rig r;
        for (int j = 0; j < 7; ++j) {
            r.s.word(j, shot::kAge) = 50;
            r.s.word(j, shot::kOwnerPool) = 14;
            r.s.word(j, shot::kOwnerRec) = 1;
        }
        fire(r.sc, 0);
        CHECK(ship_dword(r.w.ships[0], 0xa0) == 100u);
        CHECK(r.w.w8e28 == 50000u);
        CHECK(r.h.sounds.empty());
        CHECK(r.s.word(7, shot::kAge) == -1);  // DGROUP:0x594 = 7: slots 7 and 8 are not the gun's
    }
    SUBCASE("the intervals: +0xa0 + 5 and +0xa0 + +0x8c") {
        Rig r;
        set_ship_dword(r.w.ships[0], 0xa0, 96);
        fire(r.sc, 0);
        CHECK(r.s.word(0, shot::kAge) == -1);  // 96 + 5 > 100
        set_ship_dword(r.w.ships[0], 0xa0, 95);
        r.w.ships[0].d8c.set(6);
        fire(r.sc, 0);
        CHECK(r.s.word(0, shot::kAge) == -1);  // 95 + 6 > 100
        r.w.ships[0].d8c.set(5);
        fire(r.sc, 0);
        CHECK(r.s.word(0, shot::kAge) == 0);
    }
}

TEST_CASE("shots: the muzzle tables from hand-computed vectors") {
    CHECK(sin128(90) == 128);
    CHECK(cos128(0) == 128);
    ShotState s;
    DesignWords d;
    d.w00 = 9;   // r = 12
    d.w2a = 32;  // S / 2 = 16
    d.w68 = 10, d.w6a = -5, d.w6c = -10, d.w6e = -5;
    fill_muzzles(s, 2, d);
    // seg10:0x1460: k = 0 at 450 (up): (16, 16 - 12); k = 4 at 360 (right): (28, 16)
    CHECK(s.mx(2, 0) == 16);
    CHECK(s.my(2, 0) == 4);
    CHECK(s.mx(2, 4) == 28);
    CHECK(s.my(2, 4) == 16);
    // k = 1 at 427: cos = sin(23) = 51, sin(67) = 119: 51·12 / 128 = 4; 119·12 / 128 = 11
    CHECK(s.mx(2, 1) == 16 + 4);
    CHECK(s.my(2, 1) == 16 - 11);
    // seg10:0x1502 barrel 0 at a = 0: (16 + 10, 16 - 5); at a = 90: (16 + 5, 16 + 10)
    CHECK(s.mx(2, 32) == 26);
    CHECK(s.my(2, 32) == 11);
    CHECK(s.mx(2, 36) == 21);
    CHECK(s.my(2, 36) == 26);
    // barrel 1 at a = 0: (16 - 10, 16 - 5)
    CHECK(s.mx(2, 48) == 6);
    CHECK(s.my(2, 48) == 11);
    // The /128 on the difference (s2-gun-verify.md 14): a = 23, ox = 10, oy = -5: cos 119,
    // sin 51: (1190 + 255) / 128 = 11, not 9 + 1 = 10 per product
    CHECK(s.mx(2, 33) == 16 + 11);
}

TEST_CASE("shots: the one-barrel muzzle and the wrap") {
    Rig r;
    r.s.mx(0, 1) = 20;
    r.s.my(0, 1) = 3;
    r.w.pools.rec(kPoolShips, 0).x = 25590;
    fire(r.sc, 0);
    // (25590 + 160 + 25600) % 25600 = 150
    CHECK(r.s.word(0, shot::kWorldX) == 150);
    CHECK(r.s.word(0, shot::kWorldY) == 12024);
}

TEST_CASE("shots: the brush index per age, 20 brushes for both Bright settings") {
    CHECK(shot_brush_index(0, 102, true, 0) == 0);
    CHECK(shot_brush_index(12, 102, true, 0) == 0);
    CHECK(shot_brush_index(13, 102, true, 0) == 1);
    CHECK(shot_brush_index(51, 102, true, 0) == 4);
    CHECK(shot_brush_index(101, 102, true, 0) == 7);
    CHECK(shot_brush_index(51, 102, true, 1) == 14);
    CHECK(shot_brush_index(51, 102, false, 0) == 14);
    // core-0002's dots: a new trail #FFFF00, the oldest #691900 (brush 5) [ORACLE core-0002]
    CHECK(kShotBrushes[0] == 0x0200FFFFu);
    CHECK(kShotBrushes[5] == 0x02001969u);
    CHECK(kShotBrushes[10] == 0x02FFFF00u);
    CHECK(kShotBrushesBright[1] == 0x0200EBEBu);
    CHECK(kShotBrushesBright[19] == 0x02826400u);
}

TEST_CASE("shots: the shot update draws 2 x 2 heads, blacks point T, expires at the lifetime") {
    Rig r;
    fire(r.sc, 0);
    r.d.blts.clear();
    r.d.brushes.clear();
    CHECK(shot_update(r.sc, 0, 0) == 1);
    // One head (PATCOPY, 2 x 2 at Size 1) and the erase of point T = 5 (BLACKNESS)
    REQUIRE(r.d.blts.size() == 2);
    CHECK(r.d.blts[0].rop == kRopPatCopy);
    CHECK(r.d.blts[0].w == 2);
    CHECK(r.d.blts[1].rop == kRopBlackness);
    CHECK(r.d.brushes == std::vector<int>{0});
    CHECK(r.s.word(0, shot::kAge) == 1);
    // The head moved by the velocity
    CHECK(r.s.word(0, shot::kWorldX) == 12000 + 18);
    CHECK(r.s.word(0, shot::kWorldY) == 12000 - 45);
    // The trail: point 1 holds the drawn head
    CHECK(r.s.word(0, shot::kScrX + 2) == r.d.blts[0].x);
    // To the lifetime: the expiry blacks points 5 ... 0
    r.s.word(0, shot::kAge) = 101;
    r.d.blts.clear();
    CHECK(shot_update(r.sc, 0, 0) == 0);  // expired, and DGROUP:0x9136 is 0
    CHECK(r.s.word(0, shot::kAge) == -1);
    int blacks = 0;
    for (const auto& b : r.d.blts) blacks += b.rop == kRopBlackness;
    CHECK(blacks == 1 + 6);
    // A free slot returns 0 while DGROUP:0x9136 is 0, 1 once it is set
    r.s.dg_9136 = 0;
    CHECK(shot_update(r.sc, 0, 0) == 0);
    r.s.dg_9136 = 1;
    CHECK(shot_update(r.sc, 0, 0) == 1);
}

TEST_CASE("shots: the power at ages 0, 20, 50 and 101 for the player") {
    Rig r;
    fire(r.sc, 0);
    // p = 14 - age·14 / 102, 23 below age 20; x 3 / 2 for the player; x mass / 250 (250 here)
    const std::pair<int, int> cases[] = {{0, 34}, {20, 18}, {50, 12}, {101, 1}};
    for (const auto& [age, p] : cases) {
        r.s.word(0, shot::kAge) = static_cast<std::int16_t>(age);
        CHECK(shot_power(r.sc, 0) == p);
    }
    // TRANSFORMER's mass 160: 34·160 / 250 = 21
    r.w.pools.rec(kPoolShips, 0).mass = 160;
    r.s.word(0, shot::kAge) = 0;
    CHECK(shot_power(r.sc, 0) == 21);
    // Demon's Gate triples another ship's shot only (the owner record, claim 39)
    r.g.demons_gate = 1;
    CHECK(shot_power(r.sc, 0) == 21);
}

TEST_CASE("shots: a rock hit from slot 1 reaches rock_damage through the pool-30 guard") {
    // The guard reads DGROUP:0x9406 + 2·30 = DGROUP:0x9442, the collision word eA
    Rig r;
    ObjectRecord& rock = r.w.pools.rec(kPoolRocks, 0);
    rock.size = 44;
    rock.mass = 1400;
    rock.max_damage = 1000;
    rock.extent = 0x160;
    rock.on_damage.set(kDamageRock);
    r.w.pools.set_in_use(kPoolRocks, 1);
    r.c.b_pool = 0x1e;
    r.c.b_rec = 1;
    r.c.eA = 0;
    CHECK(rock_damage(r.x, kPoolRocks, 0, 10) == 0);
    CHECK(rock.damage == 0);  // eA 0 < slot 1: refused
    r.c.eA = 100;
    CHECK(rock_damage(r.x, kPoolRocks, 0, 10) == 0);
    CHECK(rock.damage == 15 + 15);  // 3·10 / 2, twice (not destroyed)
    rock.damage = 0;
    // A shot in slot 1 flying into the rock's square
    rock.x = 12400;
    rock.y = 12000;
    r.s.word(1, shot::kAge) = 30;
    r.s.word(1, shot::kLife) = 102;
    r.s.word(1, shot::kBarrels) = 1;
    r.s.word(1, shot::kAlive) = 1;
    r.s.word(1, shot::kOwnerPool) = 14;
    r.s.word(1, shot::kOwnerRec) = 0;
    r.s.word(1, shot::kWorldX) = 12400 + 8 * 30;  // past the centre: approaching
    r.s.word(1, shot::kWorldY) = 12000 + 8 * 30;
    r.s.word(1, shot::kVx) = -40;
    r.s.word(1, shot::kVy) = -40;
    r.vis.count = 1;
    r.vis.pool[0] = kPoolRocks;
    r.vis.rec[0] = 0;
    const std::int16_t res = shot_hit_test(r.sc, 1);
    CHECK(res == 1);
    CHECK(r.c.b_pool == 0x1e);
    CHECK(r.c.b_rec == 1);
    CHECK(r.dg_943a == 0x1e);
    CHECK(r.c.a_pool == kPoolRocks);
    CHECK(r.c.dA > 1);
    CHECK(rock.damage > 0);
    // The impact sound 0x11 at the shot, the kill scorer's rating 2·d for a damaged rock
    CHECK(r.h.sounds.back() == std::make_pair(0x11, 0x1e));
    CHECK(r.w.rating == static_cast<std::uint32_t>(2 * r.c.dA));
    CHECK(r.c.hit_pool == kPoolRocks);
}

TEST_CASE("shots: shot_view backs CollideHost::shot") {
    ShotState s;
    s.word(2, shot::kVx) = 7;
    s.word(2, shot::kWorldY) = 99;
    s.word(2, shot::kOwnerPool) = 14;
    set_shot_source(&s);
    CollideHost h;
    const ShotSlotView v = h.shot(2);
    CHECK(v.vx == 7);
    CHECK(v.y == 99);
    CHECK(v.owner_pool == 14);
    CHECK(h.shot(9).owner_pool == -1);
    set_shot_source(nullptr);
    set_weapon_words([](int w, int off) { return static_cast<std::int16_t>(w * 1000 + off); });
    CHECK(h.weapon_kind(8) == 8000 + 0x24);  // W(8)+0x24
    set_weapon_words(nullptr);
    CHECK(h.weapon_kind(8) == 0);
}

TEST_CASE("shots: the fire poll seg11:0x4db0") {
    WaveGlobals g;
    KeyLatch keys;
    g.now = 1000;
    CHECK_FALSE(fire_poll(g, keys, 0));
    keys.key_down(kVkShift, false);
    CHECK(fire_poll(g, keys, 0));
    CHECK_FALSE(fire_poll(g, keys, 1000));  // the lock-out has not passed
    g.play_mode = 1;
    CHECK_FALSE(fire_poll(g, keys, 0));
    g.play_mode = 0;
    keys.reset();
    keys.key_down(kVkSpace, false);
    keys.key_up(kVkSpace);
    CHECK(fire_poll(g, keys, 0));   // a tap between passes (0x8001) [DESIGN ADR 0013]
    CHECK_FALSE(fire_poll(g, keys, 0));
    g.joy_fire = 1;
    CHECK(fire_poll(g, keys, 0));
}
