// The effect table's routines (src/world/effects.h; docs/re/core.md "Death and the explosion";
// work/analysis/m4/s1-death.md sections 1-3, verified in s1-death-verify.md): the frame
// routine seg8:0x3fbe (the fireballs of slot 10 at DGROUP:0xb70's offsets, 8 frames and the
// blackout, the waits and KABOOM of type 1, DGROUP:0x9430 and DGROUP:0x9434), the blast
// seg8:0x5ab0 ([OPEN]: unverified), a ship's death seg16:0x8e4 with its rand() order, the
// generic destroy seg12:0x274a with its fragment counts and rand() order, and seg10:0x5102.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "world/death.h"
#include "world/effects.h"

using namespace is::world;

namespace {

struct Call {
    std::string what;
    int a = 0, b = 0, c = 0, d = 0, e = 0;
    bool operator==(const Call& o) const {
        return what == o.what && a == o.a && b == o.b && c == o.c && d == o.d && e == o.e;
    }
};

struct Host : EffectHost {
    std::vector<Call> calls;
    std::int16_t kind = 0;
    void sound(int id, int pool, int rec, int mode) override { calls.push_back({"sound", id, pool, rec, mode}); }
    void player_sprite() override { calls.push_back({"player"}); }
    void hit(int p, int r, int sp, int sr, std::int16_t amount) override { calls.push_back({"hit", p, r, sp, sr, amount}); }
    void pieces(int k) override { calls.push_back({"pieces", k}); }
    void seg11_aece() override { calls.push_back({"aece"}); }
    std::int16_t design_kind(int) override { return kind; }
    void destroy(int p, int r) override { calls.push_back({"destroy", p, r}); }
    void seg7_2080(int p, int a) override { calls.push_back({"2080", p, a}); }
    int count(const std::string& w) const {
        int n = 0;
        for (const Call& c : calls) n += c.what == w;
        return n;
    }
};

struct Fixture {
    World w;
    EffectState st;
    EffectWords in;
    Host host;
    is::shell::Rng rng;
    RecordingBlitter out;
    ImageSlotTable images{};
    std::int16_t d943a = 0, d943c = 0;
    EffectCtx c{w, st, in, host, rng, &out, &images,
                [](std::int16_t x, std::int16_t y) {
                    return Point16{static_cast<std::int16_t>(x >> 3), static_cast<std::int16_t>(y >> 3)};
                },
                &d943a, &d943c};

    Fixture() {
        clear_effects(w.effects);
        w.W = 1024;
        w.H = 683;
        w.size = 1;
        w.detail = 3;
        w.player = 0;
        w.last_ship = 1;
        w.player_game = 1;
        w.pools.set_in_use(kPoolShips, 2);
        images[kSlotExplosion] = 0x50;
        in.now = 100000;
        rng.srand(1234);
    }
};

// A type-t entry k with 10 frames on (pool, rec), centre (cx, cy).
void entry(Fixture& f, int k, std::int16_t type, int pool, int rec, std::int16_t cx, std::int16_t cy) {
    EffectEntry& e = f.w.effects[static_cast<std::size_t>(k)];
    e.frame = 1;
    e.end = 10;
    e.type = type;
    e.pool = static_cast<std::int16_t>(pool);
    e.rec = static_cast<std::int16_t>(rec);
    e.start.set(f.in.now);
    f.st.sides.centre[static_cast<std::size_t>(k)] = EffectPoint{cx, cy};
}

}  // namespace

TEST_CASE("effects: DGROUP:0xb70 and the fireball geometry of seg8:0x3fbe") {
    CHECK(kExplosionX == std::array<std::int16_t, 8>{0, 10, 26, 48, 76, 110, 150, 196});
    // The offsets are the running sums of the sides 6f + 10 (verified claim 8).
    for (int f = 1; f < 8; ++f) CHECK(kExplosionX[f] - kExplosionX[f - 1] == 6 * (f - 1) + 10);
    CHECK(in_play_area(-50, -50, 640, 480));
    CHECK_FALSE(in_play_area(-51, 0, 640, 480));
    CHECK(in_play_area(689, 529, 640, 480));
    CHECK_FALSE(in_play_area(690, 0, 640, 480));
}

TEST_CASE("effects: a 10-frame entry draws 8 fireballs, blacks out at 9 and is freed") {
    Fixture f;
    f.w.pools.rec(3, 2).size = 22;
    f.w.pools.rec(3, 2).x = 800;
    f.w.pools.rec(3, 2).y = 1600;
    f.in.dg_1318 = 7;  // no blast here
    entry(f, 3, 4, 3, 2, 200, 150);
    f.w.w9430 = 1;
    f.w.w9434 = 3;
    for (int call = 1; call <= 8; ++call) {
        CAPTURE(call);
        f.out.calls.clear();
        effect_frame(f.c, 3);
        const int side = 6 * (call - 1) + 10;
        REQUIRE(f.out.calls.size() == 1);
        CHECK(f.out.calls[0] == bit_blt_call(kDcWindow, 200 - side / 2, 150 - side / 2, side, side, 0x50,
                                             kExplosionX[static_cast<std::size_t>(call - 1)], 0, kSrcCopy));
        CHECK(f.w.effects[3].frame == call + 1);
    }
    f.out.calls.clear();
    f.host.calls.clear();
    effect_frame(f.c, 3);  // +0x00 = 9: the last frame
    REQUIRE(f.out.calls.size() == 2);
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 200 - 26, 150 - 26, 52, 52, kBlackness));
    CHECK(f.out.calls[1] == pat_blt_call(kDcWindow, 100, 200, 22, 22, kBlackness));  // the source
    CHECK(f.w.effects[3].frame == kEffectFree);
    CHECK(f.w.w9434 == 0);
    CHECK(f.w.w9430 == 0);
    // The player (ship 0) is in play and alive: seg7:0x1208 (verified claim 11, extra).
    f.w.ships[0].bec = 1;
    entry(f, 3, 4, 3, 2, 200, 150);
    f.w.effects[3].frame = 9;
    f.host.calls.clear();
    effect_frame(f.c, 3);
    CHECK(f.host.count("player") == 1);
    // Off the play area nothing is drawn, and the frame still counts.
    entry(f, 3, 4, 3, 2, 2000, 150);
    f.out.calls.clear();
    effect_frame(f.c, 3);
    CHECK(f.out.calls.empty());
    CHECK(f.w.effects[3].frame == 2);
}

TEST_CASE("effects: the rescan keeps the highest busy index and DGROUP:0x9430 floors at 0") {
    Fixture f;
    f.in.dg_1318 = 7;
    entry(f, 2, 4, 3, 0, 10, 10);
    entry(f, 5, 4, 3, 0, 10, 10);
    f.w.effects[2].frame = 9;
    f.w.w9430 = 0;
    effect_frame(f.c, 2);
    CHECK(f.w.w9434 == 5);
    CHECK(f.w.w9430 == 0);
    // A type-0xff06 entry's last frame draws nothing; types > 5 go to the pieces hook.
    entry(f, 7, kEffectTypeFF06, 3, 0, 10, 10);
    f.w.effects[7].frame = 9;
    f.out.calls.clear();
    effect_frame(f.c, 7);
    CHECK(f.out.calls.empty());
    entry(f, 8, 6, 3, 0, 10, 10);
    f.host.calls.clear();
    effect_frame(f.c, 8);
    CHECK(f.host.count("pieces") == 1);
    CHECK(f.w.effects[8].frame == 2);
}

TEST_CASE("effects: type 1 waits for its start, then plays KABOOM at its world point") {
    Fixture f;
    entry(f, 0, 1, 3, 0, 10, 10);
    f.w.effects[0].start.set(f.in.now + 20);
    f.st.sides.point[0] = EffectPoint{1234, 5678};
    effect_frame(f.c, 0);
    CHECK(f.w.effects[0].frame == 1);  // no frame while waiting (verified claim 5)
    CHECK(f.host.calls.empty());
    f.in.now += 19;
    effect_frame(f.c, 0);
    CHECK(f.w.effects[0].frame == 1);  // start still above now (unsigned)
    f.in.now += 1;  // start = now: `jbe` goes on [BYTES seg8:0x400a]
    effect_frame(f.c, 0);
    REQUIRE(f.host.calls.size() == 1);
    CHECK(f.host.calls[0] == Call{"sound", 0x45, 0, 0, 3});
    CHECK(f.st.dg_5144 == 1234);
    CHECK(f.st.dg_5146 == 5678);
    CHECK(f.w.effects[0].frame == 2);
    // Type 1 has no blast at frame 6 and no 0x6a or source blackout on its last frame.
    f.w.effects[0].frame = 6;
    f.host.calls.clear();
    effect_frame(f.c, 0);
    CHECK(f.host.count("hit") == 0);
    f.w.effects[0].frame = 9;
    f.out.calls.clear();
    effect_frame(f.c, 0);
    CHECK(f.out.calls.size() == 1);
}

TEST_CASE("effects: seg16:0x8e4, the ship's 8 entries, its state and the player's deadline") {
    Fixture f;
    ObjectRecord& o = f.w.pools.rec(kPoolShips, 0);
    o.scr_x = 300;
    o.scr_y = 200;
    o.x = 4000;
    o.y = 5000;
    f.w.ships[0].bec = 1;
    f.w.rating = 1000;
    f.w.pools.rec(11, 0).state = 1;  // a free pod record
    f.w.pools.set_allocated(11, 10);
    is::shell::Rng expect = f.rng;
    ship_destroyed(f.c, kPoolShips, 0);
    for (int k = 0; k < 8; ++k) {
        CAPTURE(k);
        const EffectEntry& e = f.w.effects[static_cast<std::size_t>(k)];
        CHECK(e.frame == 1);
        CHECK(e.end == 10);
        CHECK(e.type == (k == 0 ? 5 : 1));
        CHECK(e.pool == kPoolShips);
        CHECK(e.rec == 0);
        CHECK(e.start.get() == f.in.now + 20u * static_cast<unsigned>(k));
        int ox = 16, oy = 16;
        if (k != 0) {
            ox = expect.rand() % 60 - 12;
            oy = expect.rand() % 60 - 12;
        }
        CHECK(f.st.sides.centre[static_cast<std::size_t>(k)].x == 300 + ox);
        CHECK(f.st.sides.centre[static_cast<std::size_t>(k)].y == 200 + oy);
        CHECK(f.st.sides.point[static_cast<std::size_t>(k)].x == 4000 + 0x80);
        CHECK(f.st.sides.point[static_cast<std::size_t>(k)].y == 5000 + 0x80);
    }
    CHECK(f.rng.state() == expect.state());
    CHECK(f.w.w9430 == 8);
    CHECK(f.w.w9434 == 0);  // not written by seg16:0x8e4 (verified claim 4)
    CHECK(f.w.ships[0].be2 == 1);
    CHECK(f.w.ships[0].bec == 0);
    CHECK(o.angle == 1);
    CHECK(f.w.ships[0].lockout.get() == f.in.now + 6000);
    CHECK(f.w.w1e84 == f.in.now + 8000);
    CHECK(f.w.rating == 800);
    // The sounds: 0x6a from seg13:0x4710, then KABOOM [ORACLE core-0001] (0x45 at the death).
    std::vector<int> sounds;
    for (const Call& c : f.host.calls)
        if (c.what == "sound") sounds.push_back(c.a);
    CHECK(sounds == std::vector<int>{0x6a, 0x45});
    // The pod took record 0 of pool 11.
    CHECK(f.w.pools.rec(11, 0).state == 0);
    CHECK(f.w.pools.rec(11, 0).extent == 0xb8);
}

TEST_CASE("effects: only entry 0 animates while DGROUP:0x9434 stays 0; the rest start together") {
    Fixture f;
    f.in.dg_1318 = 7;
    f.w.pools.set_allocated(11, 0);
    f.w.ships[0].bec = 1;
    ship_destroyed(f.c, kPoolShips, 0);
    int frames = 0;
    // The pass's loop k = 0 ... DGROUP:0x9434 while DGROUP:0x9430 > 0 (seg7:0x979-0x991).
    while (f.w.effects[0].frame != kEffectFree && frames < 20) {
        if (f.w.w9430 > 0)
            for (int k = 0; k <= f.w.w9434; ++k) effect_frame(f.c, k);
        f.in.now += 28;
        ++frames;
    }
    CHECK(frames == 9);  // 9 wave frames, 250 ms at 36 frames per second
    // The loop re-reads DGROUP:0x9434 [BYTES seg7:0x98b]: in the pass that frees entry 0 the
    // rescan raises it to 7, and entries 1 ... 7, long past their starts, all play KABOOM and
    // take their first frame together.
    for (int k = 1; k < 8; ++k) CHECK(f.w.effects[static_cast<std::size_t>(k)].frame == 2);
    int kabooms = 0;
    for (const Call& c : f.host.calls) kabooms += c == Call{"sound", 0x45, 0, 0, 3};
    CHECK(kabooms == 7);
    CHECK(f.w.w9434 == 7);
    CHECK(f.w.w9430 == 7);
}

TEST_CASE("effects: the demo player's death and the duel's DGROUP:0x668a") {
    Fixture f;
    f.w.player_game = 0;
    f.w.rating = 1001;
    f.w.ships[0].bec = 1;
    f.w.pools.set_allocated(11, 0);
    ship_destroyed(f.c, kPoolShips, 0);
    CHECK(f.w.w1e84 == 0);
    CHECK(f.w.w1e80 == f.in.now + 8000);
    CHECK(f.st.dg_95a8 == 1);
    CHECK(f.w.rating == 500);
    CHECK(f.host.count("aece") == 1);
    // Byte +0x147 = 1 outside the seg18 mode: nothing.
    Fixture g;
    g.w.ships[1].radar_hide = 1;
    ship_destroyed(g.c, kPoolShips, 1);
    CHECK(g.host.calls.empty());
    CHECK(g.w.w9430 == 0);
    // A duel: a computer ship dies while the player lives in play: no -1.
    Fixture d;
    d.w.kind = 7;
    d.w.pools.set_allocated(11, 0);
    d.w.ships[0].bec = 1;
    d.w.ships[1].bec = 1;
    d.w.gate[0].w12 = 1;
    ship_destroyed(d.c, kPoolShips, 1);
    // The loop leaves the player out, so with ship 1 dead no live ship clears the flag.
    CHECK(d.w.gate[0].w12 == -1);
    // With a third live ship in play the flag drops.
    Fixture e;
    e.w.kind = 7;
    e.w.last_ship = 2;
    e.w.pools.set_allocated(11, 0);
    e.w.ships[1].bec = 1;
    e.w.ships[2].bec = 1;
    e.w.gate[0].w12 = 1;
    ship_destroyed(e.c, kPoolShips, 1);
    CHECK(e.w.gate[0].w12 == 1);
}

TEST_CASE("effects: seg12:0x274a's fragment count") {
    CHECK(fragment_count(1, 3, 0) == 1);
    CHECK(fragment_count(10, 0, 0) == 3);
    CHECK(fragment_count(10, 1, 0) == 6);
    CHECK(fragment_count(10, 2, 0) == 10);
    CHECK(fragment_count(10, 3, 0) == 13);
    CHECK(fragment_count(3, 0, 0) == 2);  // at least 2
    CHECK(fragment_count(10, 3, 11) == 12);  // (35 - 11)·13/25
    CHECK(fragment_count(1, 3, 34) == 1);    // at least 1
    CHECK(fragment_count(10, 3, 40) == 1);
}

TEST_CASE("effects: seg12:0x274a, the fragments' rand() order, the player's 1500 ms") {
    Fixture f;
    ObjectRecord& o = f.w.pools.rec(kPoolShips, 0);
    o.blast_size = 9;
    o.extent = 168;
    o.x = 3000;
    o.y = 2000;
    o.scr_x = 40;
    o.scr_y = 50;
    o.size = 22;
    f.w.detail = 1;  // n = 9·60/100 = 5
    is::shell::Rng expect = f.rng;
    object_destroyed(f.c, kPoolShips, 0);
    const int n = 5;
    for (int j = 0; j < n; ++j) {
        CAPTURE(j);
        const EffectEntry& e = f.w.effects[static_cast<std::size_t>(j)];
        CHECK(e.type == (j == 0 ? 4 : 1));
        const int r = expect.rand() % 110;
        unsigned start = f.in.now + static_cast<unsigned>(r + 30 * j);
        int end = 10;
        if (j > 1 / 2 + 2 && expect.rand() % 10 <= 6) {
            end -= expect.rand() % 4 + 4;
            start += 60;
        }
        CHECK(e.end == end);
        CHECK(e.start.get() == start);
        int px = 3000, py = 2000;
        if (j > 0) {
            px = 84 + (expect.rand() % (2 * (n + 40)) - n - 40) * 8 + 3000;
            py = (expect.rand() % (2 * (n + 40)) - n - 40) * 8 + 84 + 2000;
        }
        CHECK(f.st.sides.point[static_cast<std::size_t>(j)].x == px);
        CHECK(f.st.sides.point[static_cast<std::size_t>(j)].y == py);
        CHECK(f.st.sides.centre[static_cast<std::size_t>(j)].x == px >> 3);
    }
    CHECK(f.rng.state() == expect.state());
    CHECK(f.w.w9430 == n);
    CHECK(f.w.w9434 == n - 1);
    CHECK(o.state == 1);
    CHECK(f.w.w1e84 == f.in.now + 1500);
    CHECK(f.host.calls.back() == Call{"sound", 0x45, kPoolShips, 0, 1});
    REQUIRE(f.out.calls.size() == 1);
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 40, 50, 22, 22, kBlackness));
}

TEST_CASE("effects: the blast seg8:0x5ab0 (unverified reading) [OPEN]") {
    Fixture f;
    // Source: a dying ship (pool 14, record 0) with blast_size 9 at the point (8000, 8000).
    f.w.pools.rec(kPoolShips, 0).blast_size = 9;
    entry(f, 0, 5, kPoolShips, 0, 0, 0);
    f.w.effects[0].frame = 6;
    f.st.sides.point[0] = EffectPoint{8000, 8000};
    // A rock 64 cells (512 units) right: d = 64, (2·64) >> 1 = 64 -> 100.
    f.w.pools.set_in_use(3, 1);
    ObjectRecord& r = f.w.pools.rec(3, 0);
    r.x = 8000 + 512;
    r.y = 8000;
    r.mass = 350;
    // The source itself (mass 0) is skipped.
    blast(f.c, 0);
    REQUIRE(f.host.count("hit") == 1);
    // A = ((9·600 / 100)·15) / ((350/25 + 25) >> 1) = (54·15)/19 = 42.
    CHECK(f.host.calls[0] == Call{"hit", 3, 0, kPoolShips, 0, -42});
    CHECK(r.flame == 4);  // +2, +2 more under 150
    CHECK(f.d943a == kPoolShips);
    CHECK(f.st.dg_b0c == 1);
    // v = ((9·250)/(100/10)) / ((350 + 100)/100) = 225 / 4 = 56; vx += 56·64/100 = 35.
    CHECK(r.vx == 35);
    CHECK(r.vy == 0);
    // Out of reach: (min(9, 7) + 8)·15 = 225 <= d.
    r.x = 8000 + 8 * 230;
    r.vx = 0;
    f.host.calls.clear();
    blast(f.c, 0);
    CHECK(f.host.calls.empty());
    // DGROUP:0x1318 > 6: nothing.
    r.x = 8000 + 512;
    f.in.dg_1318 = 7;
    blast(f.c, 0);
    CHECK(f.host.calls.empty());
}

TEST_CASE("effects: seg8:0x3fbe calls the blast at frame 6 of types 4 and 5") {
    Fixture f;
    f.w.pools.rec(kPoolShips, 0).blast_size = 9;
    entry(f, 0, 5, kPoolShips, 0, 0, 0);
    f.st.sides.point[0] = EffectPoint{8000, 8000};
    f.w.pools.set_in_use(3, 1);
    ObjectRecord& r = f.w.pools.rec(3, 0);
    r.x = 8000 + 512;
    r.y = 8000;
    r.mass = 350;
    for (int i = 0; i < 9; ++i) effect_frame(f.c, 0);
    CHECK(f.host.count("hit") == 1);
}

TEST_CASE("effects: seg10:0x5102 loads #170 and #175 into empty slots 10 and 11") {
    RecordingBlitter out;
    ImageSlotTable images{};
    build_effect_images(out, images, 1);
    REQUIRE(out.images.size() == 2);
    CHECK(out.images[0].graphic == 170);
    CHECK(out.images[1].graphic == 175);
    CHECK(images[kSlotExplosion] == out.images[0].id);
    CHECK(images[kSlotSnowball] == out.images[1].id);
    build_effect_images(out, images, 1);
    CHECK(out.images.size() == 2);  // both set: returns at once
    images[kSlotSnowball] = 0;
    build_effect_images(out, images, 0);
    REQUIRE(out.images.size() == 3);
    CHECK(out.images[2].graphic == 175);
}
