// M4 stage 5, track WEAPONS (src/world/shots.h): DOUBLE GUNS' two barrels from seg14:0x11a8's
// design tail and seg10:0x1502 (work/analysis/m4/s5-weapons.md 8, verified claims 42-48), and the
// seam-fix stack word of seg8:0x6f3c (sections 7 and 9.5, verified claims 32-40 and 49). The
// heading-3 entries differ by core-0006's (11, 27) between the two trails [ORACLE core-0006].
#include <cstdint>
#include <utility>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/design_words.h"
#include "world/shots.h"

using namespace is::world;

namespace {

struct Entry {
    int x, y;
};
Entry e(ShotState& s, int n, int k) { return {s.mx(n, k), s.my(n, k)}; }

void check_barrels(ShotState& s, int n, int k, Entry b0, Entry b1) {
    CAPTURE(k);
    CHECK(e(s, n, 32 + k).x == b0.x);
    CHECK(e(s, n, 32 + k).y == b0.y);
    CHECK(e(s, n, 48 + k).x == b1.x);
    CHECK(e(s, n, 48 + k).y == b1.y);
}

}  // namespace

namespace {
DesignWordSet barrels(std::int8_t b1e, std::int8_t b1f, std::int16_t side, int size_level) {
    DesignWordSet d;
    design_barrels(d, b1e, b1f, side, size_level);
    return d;
}
}  // namespace

TEST_CASE("shots: seg14:0x11a8's barrel offsets of SKY TALON and TRANSFORMER [BYTES MASTER.BOD]") {
    // TALON (body 5): +0x1e = 1, +0x1f = 16; TRANSFORMER (body 33): 4, 6; side 33, Size 1
    const DesignWordSet t = barrels(1, 16, 33, 1);
    CHECK(t.w68 == -16);
    CHECK(t.w6a == 0);
    CHECK(t.w6c == 15);
    CHECK(t.w6e == 0);
    const DesignWordSet f = barrels(4, 6, 33, 1);
    CHECK(f.w68 == -13);
    CHECK(f.w6a == -10);
    CHECK(f.w6c == 12);
    CHECK(f.w6e == -10);
}

TEST_CASE("shots: seg10:0x1502's two-barrel entries from the stored words [BYTES seg10:0x1511-0x1613]") {
    ShotState s;
    World w;
    DesignWordSet talon = barrels(1, 16, 33, 1);  // SKY TALON
    talon.w2a = 33;                               // the sheet's side (seg14:0xd94)
    set_design_words(3, talon);
    const DesignWords d = design_words_from_world(w, 3);
    CHECK(d.w2a == 33);
    CHECK(d.w68 == -16);
    CHECK(d.w6c == 15);
    fill_muzzles(s, 3, d);
    check_barrels(s, 3, 0, {0, 16}, {31, 16});
    check_barrels(s, 3, 3, {10, 2}, {21, 29});
    check_barrels(s, 3, 4, {16, 0}, {16, 31});
    check_barrels(s, 3, 8, {32, 16}, {1, 16});
    // core-0006's DOUBLE GUNS trails at heading 3: heads (572, 323) and (583, 350)
    CHECK(e(s, 3, 48 + 3).x - e(s, 3, 32 + 3).x == 11);
    CHECK(e(s, 3, 48 + 3).y - e(s, 3, 32 + 3).y == 27);

    DesignWordSet tf = barrels(4, 6, 33, 1);  // TRANSFORMER
    tf.w2a = 33;
    set_design_words(3, tf);
    fill_muzzles(s, 3, design_words_from_world(w, 3));
    check_barrels(s, 3, 0, {3, 6}, {28, 6});
    check_barrels(s, 3, 3, {20, 1}, {29, 23});
    check_barrels(s, 3, 4, {26, 3}, {26, 28});
    check_barrels(s, 3, 8, {29, 26}, {4, 26});
    set_design_words(3, DesignWordSet{});
}

TEST_CASE("shots: seg8:0x29ea's word at seg8:0x6f3c's [bp-0x1e] [BYTES seg8:0x2a02-0x2b4f]") {
    ViewState v;
    v.play_h = 683;
    v.shift = 3;      // Size 1: H << 2 = 2732
    v.y0l = 10000;    // the origin y, the player's y - (H << 3) / 2 at the recentre
    // The head level with the player's y at the recentre: 0, the seam fix skipped
    CHECK(seam_word(v, 12732, 0, 0, 0) == 0);
    CHECK(seam_word(v, 12733, 0, 0, 0) == 1);
    CHECK(seam_word(v, 12700, 0, 0, 0) == -32);
    // |off| > 12800: the y wrap's seg5:0x346 leaves its return IP 0x2b4f
    CHECK(seam_word(v, 12732 + 12800, 0, 0, 0) == 12800);
    CHECK(seam_word(v, 12732 + 12801, 0, 0, 0) == 0x2b4f);
    CHECK(seam_word(v, 12732 - 12801, 0, 0, 0) == 0x2b4f);
    // ... except in a kind-6 wave before DGROUP:0x9eb5 = 1
    CHECK(seam_word(v, 12732 + 12801, 6, 0, 0) == 12801);
    CHECK(seam_word(v, 12732 + 12801, 6, 1, 0) == 0x2b4f);
    // Size 4 (s = 0): the shift count byte 0xff is 31 on a 386, the 16-bit term 0
    v.shift = 0;
    CHECK(seam_word(v, 10005, 0, 0, 0) == 5);
    // The seg18 path keeps the port's 1 [OPEN M6]
    CHECK(seam_word(v, 10000, 0, 0, 1) == 1);
}

TEST_CASE("shots: seg8:0x3688 sets the word from its last barrel before the per-object tests") {
    World w;
    WaveGlobals g;
    CollideGlobals c;
    CollideHost h;
    ViewState view;
    view.play_h = 700;
    view.shift = 3;
    view.y0l = 8000;
    is::shell::Rng rng;
    std::int16_t a = 0, b = 0, d = 0;
    CollideCtx x{w, g, c, h, view, nullptr, rng, a, b, d};
    ShotState s;
    ShotHost sh;
    EffectState fx;
    ShotCtx sc{x, s, sh, fx};
    w.gates = 0;
    s.word(0, shot::kBarrels) = 2;
    s.word(0, shot::kOwnerPool) = 0;
    s.word(0, shot::kWorldY) = 9000;
    s.word(0, shot::kWorldY + 12) = 8000 + (700 << 2);  // barrel 1, the last: level with the player
    shot_hit_test(sc, 0);
    CHECK(s.stack_1e == 0);
    s.word(0, shot::kWorldY + 12) = 8000 + (700 << 2) + 3;
    shot_hit_test(sc, 0);
    CHECK(s.stack_1e == 3);
    // One barrel: barrel 0's y
    s.word(0, shot::kBarrels) = 1;
    shot_hit_test(sc, 0);
    CHECK(s.stack_1e == 9000 - 8000 - (700 << 2));
}

namespace {

class PushHost : public CollideHost {
public:
    std::vector<std::pair<int, int>> pushes;
    void seg8_81f8(int pool, int rec) override { pushes.push_back({pool, rec}); }
};

}  // namespace

TEST_CASE("shots: seg8:0x6f3c pushes the player's gun hits on non-ships into the hit history [BYTES seg8:0x7271-0x7290]") {
    // s5-weapons.md 4, verified claim 22: slot words +0x7a / +0x7c = (DGROUP:0x9324, DGROUP:0x9646)
    // and a target pool != DGROUP:0x9324, before the impact, so a hit whatever its result.
    const auto run = [](int owner_rec, int target_pool) {
        World w;
        WaveGlobals g;
        CollideGlobals c;
        PushHost h;
        ViewState view;
        view.play_w = 1024, view.play_h = 700, view.shift = 3;
        view.x0l = 8000 - 4096, view.y0l = 8000 - 2800;  // (8000, 8000) on screen at (512, 350)
        view.x0 = static_cast<std::int16_t>(view.x0l), view.y0 = static_cast<std::int16_t>(view.y0l);
        is::shell::Rng rng;
        std::int16_t a = 0, b = 0, d = 0;
        CollideCtx x{w, g, c, h, view, nullptr, rng, a, b, d};
        ShotState s;
        ShotHost sh;
        EffectState fx;
        ShotCtx sc{x, s, sh, fx};
        g.size = 1, g.ship_pool = kPoolShips, g.player = 0, g.rock_pool = kPoolRocks, g.now = 50000;
        w.size = 1, w.player = 0, w.last_ship = 1;
        w.pools.set_in_use(kPoolShips, 2);
        w.pools.set_in_use(kPoolRocks, 1);
        for (int i = 0; i < 2; ++i) {
            w.ships[static_cast<std::size_t>(i)].roster_index = -1;
            w.ships[static_cast<std::size_t>(i)].bec = 1;
            w.pools.rec(kPoolShips, i).mass = 250;
        }
        ObjectRecord& t = w.pools.rec(target_pool, target_pool == kPoolShips ? 1 : 0);
        t.x = 8000, t.y = 8000, t.size = 44, t.extent = 352, t.mass = 1400, t.owner_pool = -1;
        s.word(0, shot::kBarrels) = 1;
        s.word(0, shot::kAlive) = 1;
        s.word(0, shot::kOwnerPool) = kPoolShips;
        s.word(0, shot::kOwnerRec) = static_cast<std::int16_t>(owner_rec);
        s.word(0, shot::kAge) = 30;
        s.word(0, shot::kLife) = 102;
        s.word(0, shot::kWorldX) = 8000 + 64;
        s.word(0, shot::kWorldY) = 8000 + 80;
        s.heads[0] = 512 + 8, s.heads[1] = 350 + 10;  // inside the 44-px square
        shot_hit_object(sc, target_pool, target_pool == kPoolShips ? 1 : 0, 0);
        return h.pushes;
    };
    const auto rock = run(0, kPoolRocks);
    REQUIRE(rock.size() == 1);
    CHECK(rock[0] == std::make_pair(static_cast<int>(kPoolRocks), 0));
    CHECK(run(1, kPoolRocks).empty());   // a computer ship's gun
    CHECK(run(0, kPoolShips).empty());   // a ship target
}
