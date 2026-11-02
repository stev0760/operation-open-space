// The icon's on_damage seg7:0x233c, its shatter seg7:0x26c4 and the four fragments seg7:0x2768
// (src/world/damage.h), and a shot on an icon: 0x11 then 0x12 [ORACLE core-0002].
#include <cstdint>
#include <utility>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/damage.h"
#include "world/shots.h"

using namespace is::world;

namespace {

class Calls : public CollideHost {
public:
    std::vector<int> sounds;
    std::vector<std::pair<int, int>> rows, gone;
    std::vector<CrackPlan> cracks;
    std::vector<FragmentPlan> pictures;
    std::vector<std::pair<int, int>> bitmaps;
    struct Erase {
        int x, y, w, h;
    };
    std::vector<Erase> erased;
    void sound(int id, int, int, int) override { sounds.push_back(id); }
    void column_row(int pool, int rec) override { rows.push_back({pool, rec}); }
    void icon_gone(int rec, int arg) override { gone.push_back({rec, arg}); }
    void icon_crack(const CrackPlan& p) override { cracks.push_back(p); }
    void fragment_bitmap(int r, std::int16_t side) override { bitmaps.push_back({r, side}); }
    void fragment_pictures(const FragmentPlan& p) override { pictures.push_back(p); }
    void erase(int x, int y, int w, int h) override { erased.push_back({x, y, w, h}); }
};

struct Rig {
    World w;
    WaveGlobals g;
    CollideGlobals c;
    Calls h;
    ViewState view;
    is::shell::Rng rng;
    std::int16_t dg_943a = 0, dg_943c = 0, dg_9454 = 0;
    CollideCtx x{w, g, c, h, view, nullptr, rng, dg_943a, dg_943c, dg_9454};

    Rig() {
        w.size = 1;
        g.size = 1;
        w.pools.resize(1, 0);
        w.pools.set_in_use(1, 0);
        ObjectRecord& I = w.pools.rec(0, 2);
        I.gap = 6;
        I.size = 32;
        I.x = 9000;
        I.y = 7000;
        I.vx = 30;
        I.vy = -20;
        I.scr_x = 300;
        I.scr_y = 200;
        I.b53 = 1;
        I.on_damage.set(kDamageIcon);
        w.pools.set_in_use(0, 3);
    }
};

}  // namespace

TEST_CASE("icon damage: 3 x amount, the directory word, the crack and 0x12") {
    Rig r;
    CHECK(icon_damage(r.x, 0, 2, 5) == 0);
    const ObjectRecord& I = r.w.pools.rec(0, 2);
    CHECK(I.damage == 15);
    CHECK(r.w.dir().word(0x1d2 + 4) == 15);
    REQUIRE(r.h.cracks.size() == 1);
    const CrackPlan& p = r.h.cracks[0];
    CHECK(p.scaled);
    CHECK(p.frame == 0);
    CHECK(p.dest_x == (6 + 32) * 2 + 6);
    CHECK(p.dest_y == 6);
    CHECK(p.src_merge_x == 3 * 32);
    CHECK(p.src_and_x == 0);
    CHECK(r.h.sounds == std::vector<int>{0x12});
    CHECK(r.h.rows == std::vector<std::pair<int, int>>{{0, 2}});
    // Damage 70 and on: crack frame 1
    r.w.pools.rec(0, 2).damage = 60;
    icon_damage(r.x, 0, 2, 4);
    CHECK(r.h.cracks.back().frame == 1);
    CHECK(r.h.cracks.back().src_merge_x == 4 * 32);
    // Amount 0 redraws the crack unscaled at gap 6, side 32, with no sound
    r.h.sounds.clear();
    icon_damage(r.x, 0, 2, 0);
    CHECK_FALSE(r.h.cracks.back().scaled);
    CHECK(r.h.sounds.empty());
    // DGROUP:0x57aa + 0x23·rec != 0 silences the crack
    r.w.icons[2].w02 = 1;
    icon_damage(r.x, 0, 2, 1);
    CHECK(r.h.sounds.empty());
}

TEST_CASE("icon damage: refused while DGROUP:0x57a8 is set or the heat byte is -100 or -2") {
    for (int k = 0; k < 3; ++k) {
        Rig r;
        if (k == 0) r.w.icons[2].w00 = 1;
        if (k == 1) r.w.pools.rec(0, 2).flame = static_cast<std::uint8_t>(-100);
        if (k == 2) r.w.pools.rec(0, 2).flame = static_cast<std::uint8_t>(-2);
        CHECK(icon_damage(r.x, 0, 2, 50) == 0);
        CHECK(r.w.pools.rec(0, 2).damage == 0);
        CHECK(r.h.rows.size() == 1);
        CHECK(r.h.cracks.empty());
    }
}

TEST_CASE("icon damage: capped at 211; the shatter above 208 with four fragments") {
    Rig r;
    r.w.pools.rec(0, 2).damage = 200;
    CHECK(icon_damage(r.x, 0, 2, 3) == 1);  // 209 > 208
    const ObjectRecord& I = r.w.pools.rec(0, 2);
    CHECK(I.damage == -1);
    CHECK(I.state == 1);
    CHECK(r.w.dir().word(0x1d2 + 4) == -1);
    // The crack's 0x12, then 0x1f SHATTER [ORACLE core-0002]
    CHECK(r.h.sounds == std::vector<int>{0x12, 0x1f});
    CHECK(r.h.rows == std::vector<std::pair<int, int>>{{0, 2}});
    CHECK(r.h.gone == std::vector<std::pair<int, int>>{{2, 0}});
    REQUIRE(r.h.erased.size() == 1);
    CHECK(r.h.erased[0].x == 300);
    CHECK(r.h.erased[0].w == 32);
    CHECK(r.w.icons[2].w19 == 1);
    // Four appended pool-1 records (s2-gun-verify.md 59, 60)
    CHECK(r.w.pools.allocated(1) == 4);
    CHECK(r.w.pools.in_use(1) == 4);
    const std::int16_t dx[4] = {0, 176, 0, 176}, dy[4] = {0, 0, 176, 176};
    const std::int16_t dvx[4] = {-10, 20, -10, 10}, dvy[4] = {-10, -10, 20, 10};
    for (int k = 0; k < 4; ++k) {
        const ObjectRecord& f = r.w.pools.rec(1, k);
        CHECK(f.x == 9000 + dx[k]);
        CHECK(f.y == 7000 + dy[k]);
        CHECK(f.vx == 30 + dvx[k]);
        CHECK(f.vy == -20 + dvy[k]);
        CHECK(f.extent == 160);
        CHECK(f.mass == 60);
        CHECK(f.max_damage == 14);
        CHECK(f.size == 20);
        CHECK(f.gap == 6);
        CHECK(f.src_y == 6);
        CHECK(f.hit_sound == 0x12);
        CHECK(f.radar_color.get() == 0x00808000u);
        CHECK(f.on_damage.get() == kDamageGeneric);
        CHECK(f.radar_draw.get() == kRadarSmall);
        CHECK(f.on_destroy.get() == kDestroyPickup);
        CHECK(f.owner_pool == -1);
        CHECK(f.state == 0);
        CHECK(r.w.icons[2].fragment[static_cast<std::size_t>(k)] == k);
        CHECK(r.w.fragment_icon[static_cast<std::size_t>(k)] == 2);
    }
    REQUIRE(r.h.bitmaps.size() == 4);
    CHECK(r.h.bitmaps[0].second == 2 * 6 + 20);
    REQUIRE(r.h.pictures.size() == 1);
    const FragmentPlan& p = r.h.pictures[0];
    CHECK(p.quad_at == 6 + 2);  // gap + ((2 << Size) >> 1)
    CHECK(p.quad == 20 - 4);    // piece - ((4 << Size) >> 1)
    CHECK(p.src_x[0] == (6 + 32) * 2 + 3 + 6);
    CHECK(p.src_x[1] == (6 + 32) * 2 + 16 + 6);
    CHECK(p.src_y[3] == 6 + 20);
    // The cap: 205 + 3·40 -> 211, which the crack sees as frame 211 / 70 = 3 (not 325 / 70 = 4)
    Rig t;
    t.w.pools.rec(0, 2).damage = 205;
    CHECK(icon_damage(t.x, 0, 2, 0) == 0);  // amount 0 never shatters
    CHECK(t.w.pools.rec(0, 2).damage == 205);
    CHECK(icon_damage(t.x, 0, 2, 40) == 1);
    CHECK(t.h.cracks.back().frame == 3);
}

TEST_CASE("icon fragments: a pool-1 record of state != 0 is reused") {
    Rig r;
    r.w.pools.resize(1, 2);
    r.w.pools.set_in_use(1, 2);
    r.w.pools.rec(1, 0).state = 0;  // a live fragment stays
    r.w.pools.rec(1, 1).state = 1;
    r.w.pools.rec(1, 1).damage = 9;
    r.w.pools.rec(1, 1).extent = 77;
    icon_fragments(r.x, 2);
    CHECK(r.w.icons[2].fragment[0] == 1);  // reused: only state and damage
    CHECK(r.w.pools.rec(1, 1).state == 0);
    CHECK(r.w.pools.rec(1, 1).damage == 0);
    CHECK(r.w.pools.rec(1, 1).extent == 77);
    CHECK(r.w.icons[2].fragment[1] == 2);  // then appended
    CHECK(r.w.icons[2].fragment[3] == 4);
    CHECK(r.w.pools.allocated(1) == 5);
    CHECK(r.w.pools.in_use(1) == 5);
}

TEST_CASE("icon damage: a shot on an icon plays 0x11 then 0x12 [ORACLE core-0002]") {
    Rig r;
    WaveGlobals& g = r.g;
    g.player = 0;
    g.detail = 3;
    r.w.W = 1024;
    r.w.H = 700;
    r.view.shift = 3;
    r.view.play_w = 1024;
    r.view.play_h = 700;
    r.view.x0l = r.view.y0l = 0;
    r.c.eA = 100;
    ObjectRecord& I = r.w.pools.rec(0, 2);
    I.extent = 0x100;
    I.mass = 300;
    r.w.pools.rec(kPoolShips, 0).mass = 250;
    r.w.ships[0].roster_index = -1;
    ShotState s;
    place_shot_slots(s);
    s.word(0, shot::kAge) = 30;
    s.word(0, shot::kLife) = 102;
    s.word(0, shot::kBarrels) = 1;
    s.word(0, shot::kAlive) = 1;
    s.word(0, shot::kOwnerPool) = 14;
    s.word(0, shot::kOwnerRec) = 0;
    s.word(0, shot::kWorldX) = 9000 + 8 * 28;  // past the icon's centre: approaching
    s.word(0, shot::kWorldY) = 7000 + 8 * 28;
    s.word(0, shot::kVx) = -40;
    s.word(0, shot::kVy) = -40;
    VisibilityGlobals vis;
    vis.count = 1;
    vis.pool[0] = 0;
    vis.rec[0] = 2;
    CollideCtx x{r.w, r.g, r.c, r.h, r.view, &vis, r.rng, r.dg_943a, r.dg_943c, r.dg_9454};
    ShotHost sh;
    EffectState fx;
    ShotCtx sc{x, s, sh, fx};
    CHECK(shot_hit_test(sc, 0) == 1);
    REQUIRE(r.h.sounds.size() >= 2);
    CHECK(r.h.sounds[0] == 0x11);
    CHECK(r.h.sounds[1] == 0x12);
    CHECK(r.w.pools.rec(0, 2).damage == 3 * r.c.dA);
    // The kill scorer: a damaged icon adds W[0]·d = d to the rating
    CHECK(r.w.rating == static_cast<std::uint32_t>(r.c.dA));
}
