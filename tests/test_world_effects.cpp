// The effect table's routines (src/world/effects.h; docs/re/core.md "Death and the explosion";
// work/analysis/m4/s1-death.md sections 1-3, verified in s1-death-verify.md): the frame
// routine seg8:0x3fbe (the fireballs of slot 10 at DGROUP:0xb70's offsets, 8 frames and the
// blackout, the waits and KABOOM of type 1, DGROUP:0x9430 and DGROUP:0x9434), the blast
// seg8:0x5ab0 (core.md The blast, C001-C007), a ship's death seg16:0x8e4 with its rand() order, the
// generic destroy seg12:0x274a with its fragment counts and rand() order, and seg10:0x5102.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "doctest.h"
#include "world/collide.h"
#include "world/death.h"
#include "world/effects.h"
#include "world/shots.h"

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
    void hit(int p, int r, int sp, int sr, std::int16_t amount) override {
        calls.push_back({"hit", p, r, sp, sr, amount});
        b0c_seen.push_back(b0c ? *b0c : -99);
    }
    std::int16_t* b0c = nullptr;  // DGROUP:0xb0c as the dispatcher would read it
    std::vector<int> b0c_seen;
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
    std::int16_t b0c = 1;  // CollideGlobals::w0b0c, the dispatcher's DGROUP:0xb0c

    Fixture() {
        c.dg_b0c = &b0c;
        host.b0c = &b0c;
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
    // A type-0xff06 entry's last frame draws nothing; a type > 5 at an index >= 4 (no creator
    // makes one) skips the piece branch and still counts its frame [DESIGN].
    entry(f, 7, kEffectTypeFF06, 3, 0, 10, 10);
    f.w.effects[7].frame = 9;
    f.out.calls.clear();
    effect_frame(f.c, 7);
    CHECK(f.out.calls.empty());
    entry(f, 8, 6, 3, 0, 10, 10);
    f.host.calls.clear();
    f.out.calls.clear();
    const std::uint32_t before = f.rng.state();
    effect_frame(f.c, 8);
    CHECK(f.out.calls.empty());
    CHECK(f.rng.state() == before);
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

TEST_CASE("effects: the blast seg8:0x5ab0 (core.md The blast, C001-C007)") {
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
    // DGROUP:0xb0c: 0 during the dispatcher call when the frame counter is not 0, 1 after it;
    // the dispatcher reads the same word (M4 s4 fixer, C006, C084) [BYTES seg8:0x5b88-0x5bb8].
    REQUIRE(f.host.b0c_seen.size() == 1);
    CHECK(f.host.b0c_seen[0] == 0);
    CHECK(f.b0c == 1);
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
    // Frame counter 0: DGROUP:0xb0c stays 1 for the call.
    f.in.dg_1318 = 6;
    f.w.effects[0].frame = 0;
    f.host.b0c_seen.clear();
    blast(f.c, 0);
    REQUIRE(f.host.b0c_seen.size() == 1);
    CHECK(f.host.b0c_seen[0] == 1);
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

// ---- M4 s5 EFFECTS: the pieces of types 6 and 7 (work/analysis/m4/s5-effects.md 1.3-1.8 with
// s5-effects-verify.md claims 1-34, 54)

namespace {

// Entry k of type t on (pool, rec): frame 1, `end` frames, side, brush, step.
EffectEntry& piece_entry(Fixture& f, int k, std::int16_t type, int pool, int rec, std::int16_t end,
                         std::int16_t side, std::int16_t brush, std::int16_t step) {
    EffectEntry& e = f.w.effects[static_cast<std::size_t>(k)];
    e.frame = 1;
    e.end = end;
    e.type = type;
    e.pool = static_cast<std::int16_t>(pool);
    e.rec = static_cast<std::int16_t>(rec);
    e.side = side;
    e.brush = brush;
    e.step = step;
    return e;
}

const EffectPiece& piece(const Fixture& f, int e, int i, int j) {
    return f.st.pieces[static_cast<std::size_t>(piece_index(e, i, j))];
}

// The broken fragment of s5-effects.md 1.6's vector: pool 1 record 2 at (1000, 2000), size 20,
// velocity (35, -17), its screen square (125, 250), gap 6, `state` 1 as seg7:0x2e02 leaves it.
void fragment(Fixture& f) {
    f.w.pools.set_in_use(1, 3);
    ObjectRecord& o = f.w.pools.rec(1, 2);
    o.x = 1000, o.y = 2000, o.size = 20, o.vx = 35, o.vy = -17, o.scr_x = 125, o.scr_y = 250, o.gap = 6;
    o.state = 1;
}

}  // namespace

TEST_CASE("effects: the piece table DGROUP:0x2414 and its index (verified claims 6, 23)") {
    CHECK(sizeof(EffectPiece) == 0x1c);
    CHECK(kPieceCount == 100);
    CHECK(kDgPieceTable + kPieceCount * 0x1c == 0x2f04);
    CHECK(piece_index(0, 0, 0) == 0);
    CHECK(piece_index(1, 2, 3) == 25 + 10 + 3);
    CHECK(piece_index(3, 4, 4) == 99);
    CHECK(piece_index(4, 0, 0) == -1);
    CHECK(offsetof(EffectPiece, wx) == 0x18);
    CHECK(offsetof(EffectPiece, ax) == 0x10);
}

TEST_CASE("effects: the eight acceleration cases of seg7:0x342e / seg8:0x9694") {
    // Case 0 is also the default fill (verified claim 18).
    CHECK(piece_accel(0, 0, 0).ax == 10);
    CHECK(piece_accel(0, 0, 0).ay == -4);
    CHECK(piece_accel(4, 4, 0).ax == -10);
    CHECK(piece_accel(4, 4, 0).ay == 4);
    CHECK(piece_accel(0, 0, 1).ax == 6);     // 7·2 - 2·4
    CHECK(piece_accel(0, 0, 1).ay == -28);   // -4·4 + 6·-2
    CHECK(piece_accel(0, 0, 2).ay == 28);    // 4(j - 2)² + 6(2 - i) = 16 + 12
    CHECK(piece_accel(1, 4, 3).ax == -2 + 7 * -2 * 4);
    CHECK(piece_accel(1, 4, 3).ay == 6 * -1 * -1 + 4 * 2);
    CHECK(piece_accel(3, 0, 4).ax == 2);     // (2 - j)(i - 2) (verified claim 19)
    CHECK(piece_accel(3, 0, 4).ay == -18);   // 9(i - 2)(j - 2)
    CHECK(piece_accel(0, 2, 5).ax == -48);   // 6((i - 2)³ + j - 2)
    CHECK(piece_accel(0, 2, 5).ay == -10);
    CHECK(piece_accel(0, 4, 6).ax == 96);    // 6((j - 2)³ - (i - 2)³)
    CHECK(piece_accel(0, 4, 6).ay == -8);
    CHECK(piece_accel(3, 1, 6).ay == 21 - 2);  // 7i(i - 2)² - 2(j - 2)²
    CHECK(piece_accel(0, 1, 7).ax == -1);    // 7j - 3i - 8 (verified claim 20)
    CHECK(piece_accel(0, 1, 7).ay == -48);   // 6(i - 2)³
}

TEST_CASE("effects: seg7:0x342e, the s5-effects 1.6 vector, 25 rand() in i-outer j-inner order") {
    Fixture f;
    fragment(f);
    f.rng.set_state(0x12345678u);
    is::shell::Rng expect = f.rng;
    std::array<int, 25> draws{};
    for (int& d : draws) d = expect.rand();
    CHECK(draws[0] == 13289);
    CHECK(draws[1] == 23359);
    CHECK(draws[24] == 26088);
    pieces_setup_break(f.c, 0, 1, 2);
    CHECK(f.rng.state() == expect.state());  // exactly 25
    struct V {
        int i, j, wx, wy, vx, vy, ax, ay;
    };
    const std::array<V, 6> want = {{{0, 0, 1000, 2000, -13, 13, 6, -28},
                                    {0, 1, 1032, 2000, -5, 13, -1, -48},
                                    {0, 2, 1064, 2000, 3, 13, -48, -10},
                                    {0, 4, 1128, 2000, 19, 13, 96, -8},
                                    {2, 2, 1064, 2064, 3, -1, 0, 0},
                                    {4, 4, 1128, 2128, 19, -15, -10, 4}}};
    for (const V& v : want) {
        CAPTURE(v.i);
        CAPTURE(v.j);
        const EffectPiece& p = piece(f, 0, v.i, v.j);
        CHECK(p.wx == v.wx);
        CHECK(p.wy == v.wy);
        CHECK(p.vx == v.vx);
        CHECK(p.vy == v.vy);
        CHECK(p.ax == v.ax);
        CHECK(p.ay == v.ay);
    }
}

TEST_CASE("effects: seg8:0x9694 from the shot slot's velocity, the s5-effects 1.6 vector") {
    Fixture f;
    ShotState shots;
    f.c.shots = &shots;
    shots.word(3, shot::kVx) = 120;  // DGROUP:0x6108 + 0x94·3
    shots.word(3, shot::kVy) = -40;
    f.st.sides.point[1] = EffectPoint{5000, 6000};
    f.rng.set_state(1);
    is::shell::Rng expect = f.rng;
    for (int k = 0; k < 25; ++k) expect.rand();
    pieces_setup_burst(f.c, 1, kPieceShotPool, 3);
    CHECK(f.rng.state() == expect.state());
    struct V {
        int i, j, wx, wy, vx, vy, ax, ay;
    };
    const std::array<V, 4> want = {{{0, 0, 5008, 6008, 104, -26, 6, -28},
                                    {0, 2, 5040, 6008, 120, -26, 48, 0},
                                    {2, 2, 5040, 6040, 120, -40, 0, 0},
                                    {4, 4, 5072, 6072, 136, -54, -4, 36}}};
    for (const V& v : want) {
        CAPTURE(v.i);
        CAPTURE(v.j);
        const EffectPiece& p = piece(f, 1, v.i, v.j);
        CHECK(p.wx == v.wx);
        CHECK(p.wy == v.wy);
        CHECK(p.vx == v.vx);
        CHECK(p.vy == v.vy);
        CHECK(p.ax == v.ax);
        CHECK(p.ay == v.ay);
    }
    // Another pool reads the object's velocity (verified claim 22).
    f.w.pools.set_in_use(2, 1);
    f.w.pools.rec(2, 0).vx = 7;
    f.w.pools.rec(2, 0).vy = 9;
    pieces_setup_burst(f.c, 1, 2, 0);
    CHECK(piece(f, 1, 2, 2).vx == 7);
    CHECK(piece(f, 1, 2, 2).vy == 9);
}

TEST_CASE("effects: a type-6 piece's frames: draw, then black the old square; the transposed source") {
    Fixture f;
    fragment(f);
    f.rng.set_state(0x12345678u);
    piece_entry(f, 0, 6, 1, 2, 20, 4, 0, 1);
    f.st.piece_src[0] = 0x77;
    f.st.piece_src_owned[0] = true;
    f.st.sides.point[0] = EffectPoint{1000, 2000};
    effect_frame(f.c, 0);
    // Frame 1: the set-up, the source's square (pool 1 <= DGROUP:0x9324), then per piece the
    // BitBlt and the old square (0, 0 in a fresh table) (verified claims 3, 8, 11).
    REQUIRE(f.out.calls.size() == 1 + 2 * 25);
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 125, 250, 20, 20, kBlackness));
    CHECK(f.out.calls[1] == bit_blt_call(kDcWindow, 125, 250, 4, 4, 0x77, 6, 6, kSrcCopy));
    CHECK(f.out.calls[2] == pat_blt_call(kDcWindow, 0, 0, 4, 4, kBlackness));
    // Piece (0, 1) stands at column 1 (x 1032 / 8 = 129) and shows source column 0, row 1.
    CHECK(f.out.calls[3] == bit_blt_call(kDcWindow, 129, 250, 4, 4, 0x77, 6, 10, kSrcCopy));
    // Piece (1, 0) stands at row 1 and shows source column 1, row 0.
    CHECK(f.out.calls[11] == bit_blt_call(kDcWindow, 125, 254, 4, 4, 0x77, 10, 6, kSrcCopy));
    CHECK(f.w.effects[0].frame == 2);
    // The motion of piece (0, 0): (987, 2013), (974, 2024), (962, 2032) after frames 1, 2, 3.
    CHECK(piece(f, 0, 0, 0).wx == 987);
    CHECK(piece(f, 0, 0, 0).wy == 2013);
    f.out.calls.clear();
    effect_frame(f.c, 0);
    CHECK(piece(f, 0, 0, 0).wx == 974);
    CHECK(piece(f, 0, 0, 0).wy == 2024);
    // Frame 2 draws at the new point and blacks frame 1's (123, 251).
    CHECK(f.out.calls[0] == bit_blt_call(kDcWindow, 123, 251, 4, 4, 0x77, 6, 6, kSrcCopy));
    CHECK(f.out.calls[1] == pat_blt_call(kDcWindow, 125, 250, 4, 4, kBlackness));
    effect_frame(f.c, 0);
    CHECK(piece(f, 0, 0, 0).wx == 962);
    CHECK(piece(f, 0, 0, 0).wy == 2032);
    for (int k = 4; k < 19; ++k) effect_frame(f.c, 0);
    CHECK(f.w.effects[0].frame == 19);
    // Frame 19 is the last: per piece the draw, the old square, the new square and seg7:0x1208.
    f.out.calls.clear();
    f.host.calls.clear();
    effect_frame(f.c, 0);
    REQUIRE(f.out.calls.size() == 3 * 25);
    CHECK(f.out.calls[0].op == BlitCall::kBitBlt);
    CHECK(f.out.calls[1].op == BlitCall::kPatBlt);
    CHECK(f.out.calls[2] == pat_blt_call(kDcWindow, f.out.calls[0].x, f.out.calls[0].y, 4, 4, kBlackness));
    CHECK(f.host.count("player") == 25);
    CHECK(piece(f, 0, 0, 0).wx == 1077);
    CHECK(piece(f, 0, 0, 0).wy == 2006);
    // Freed at frame 20; the pool-1 source is released and cleared (verified claim 15).
    CHECK(f.w.effects[0].frame == kEffectFree);
    CHECK(f.out.deleted == std::vector<DcId>{0x77});
    CHECK(f.st.piece_src[0] == kDcNone);
    // With DGROUP:0x951a != 0 the last frame does not redraw the player.
    piece_entry(f, 0, 6, 1, 2, 20, 4, 0, 1);
    f.w.effects[0].frame = 19;
    f.in.autopilot = 1;
    f.host.calls.clear();
    effect_frame(f.c, 0);
    CHECK(f.host.count("player") == 0);
}

TEST_CASE("effects: the motion is 16-bit: ax 18, f 19, end 20 gives -17, not +146 (s5-effects 1.5)") {
    Fixture f;
    piece_entry(f, 0, 7, kPieceShotPool, 0, 20, 2, 10, 2);
    f.w.effects[0].frame = 18;
    EffectPiece& p = f.st.pieces[0];
    p.wx = 5000, p.wy = 5000, p.ax = 18, p.ay = 0, p.vx = 0, p.vy = 0;
    effect_frame(f.c, 0);  // f = 18: lo16(18·18·18·9) = 52488 -> -13048; / 400 = -32
    CHECK(p.wx == 5000 - 32);
    f.w.effects[0].frame = 19;
    p.wx = 5000;
    effect_frame(f.c, 0);  // lo16(6498·9) = -7054; / 400 = -17
    CHECK(p.wx == 5000 - 17);
    // The wraps at DGROUP:0x5a0 << 3 = 25600 (signed).
    piece_entry(f, 0, 7, kPieceShotPool, 0, 20, 2, 10, 2);
    f.w.effects[0].frame = 2;
    p.wx = 25600, p.vx = 1, p.ax = 0;
    p.wy = 0, p.vy = -1, p.ay = 0;
    effect_frame(f.c, 0);
    CHECK(p.wx == 0);
    CHECK(p.wy == 25600);
}

TEST_CASE("effects: a step-2 spark: nine PATCOPY squares of brush 15 + b, 4 px apart (core-0002 rock-c0064)") {
    Fixture f;
    ShotState shots;
    f.c.shots = &shots;
    for (int n = 0; n < kEffectBrushes; ++n) f.st.brushes[static_cast<std::size_t>(n)] = static_cast<BrushId>(0x300 + n);
    f.st.dg_b6a = 1;
    // The barrel's head at (4936, 1064): squares from (4936 + 8 + 16j) / 8 = 618 + 2j.
    piece_entry(f, 0, 7, kPieceShotPool, 4, 14, 2, 15, 2);
    f.st.sides.point[0] = EffectPoint{4936, 1064};
    const std::uint32_t s0 = f.rng.state();
    effect_frame(f.c, 0);
    is::shell::Rng e;
    e.set_state(s0);
    for (int k = 0; k < 25; ++k) e.rand();
    CHECK(f.rng.state() == e.state());  // the set-up draws 25 even when only 9 pieces show
    // No source square for the shot pool 0x1e (verified claim 3); per piece SelectObject, PATCOPY,
    // the old square.
    REQUIRE(f.out.calls.size() == 27);
    int n = 0;
    for (int i = 0; i < 5; i += 2)
        for (int j = 0; j < 5; j += 2) {
            CAPTURE(i);
            CAPTURE(j);
            const std::size_t at = static_cast<std::size_t>(3 * n++);
            CHECK(f.out.calls[at].op == BlitCall::kSelectBrush);
            CHECK(f.out.calls[at].brush == 0x300 + 16);  // brush 16 = 0x0000c4, (196, 0, 0)
            CHECK(f.out.calls[at + 1] == pat_blt_call(kDcWindow, 618 + 2 * j, 134 + 2 * i, 2, 2, kPatCopy));
            CHECK(f.out.calls[at + 2].rop == kBlackness);
        }
    CHECK(kEffectBrushColours[16] == 0x0000c4u);
    // Next frame, b = 2: brush 17.
    f.st.dg_b6a = 2;
    f.out.calls.clear();
    effect_frame(f.c, 0);
    CHECK(f.out.calls[0].brush == 0x300 + 17);
}

TEST_CASE("effects: the blast at frame 5 of type 6, never for type 7 (verified claim 4)") {
    for (const std::int16_t type : {std::int16_t{6}, std::int16_t{7}}) {
        CAPTURE(type);
        Fixture f;
        fragment(f);
        f.w.pools.rec(1, 2).blast_size = 0;  // seg7:0x2768 never writes +0x48
        piece_entry(f, 0, type, 1, 2, 20, 4, 10, 2);
        f.st.sides.point[0] = EffectPoint{8000, 8000};
        f.w.pools.set_in_use(3, 1);
        ObjectRecord& r = f.w.pools.rec(3, 0);
        r.x = 8000, r.y = 8000, r.mass = 350;
        for (int k = 1; k <= 19; ++k) {
            f.host.calls.clear();
            effect_frame(f.c, 0);
            CHECK(f.host.count("hit") == (type == 6 && k == 5 ? 1 : 0));
        }
    }
}

TEST_CASE("effects: seg8:0x82c6's colour cycle 1, 2, 3, 4, 3, 2, 1, 0, 1 at 61 ms (verified claim 32)") {
    EffectState st;
    std::uint32_t now = 0;
    std::vector<int> seen;
    for (int k = 0; k < 9; ++k) {
        now += 61;
        effect_colour_cycle(st, now);
        seen.push_back(st.dg_b6a);
    }
    CHECK(seen == std::vector<int>{1, 2, 3, 4, 3, 2, 1, 0, 1});
    // 60 ms is not enough (`jbe`).
    effect_colour_cycle(st, now + 60);
    CHECK(st.dg_b6a == 1);
    effect_colour_cycle(st, now + 61);
    CHECK(st.dg_b6a == 2);
}

TEST_CASE("effects: seg10:0x4b2e's 20 brushes DGROUP:0x576c (verified claim 31)") {
    RecordingBlitter out;
    EffectState st;
    create_effect_brushes(out, st);
    REQUIRE(out.brushes.size() == 20);
    for (int n = 0; n < 20; ++n) {
        CHECK(out.brushes[static_cast<std::size_t>(n)].colorref == kEffectBrushColours[static_cast<std::size_t>(n)]);
        CHECK(st.brushes[static_cast<std::size_t>(n)] == out.brushes[static_cast<std::size_t>(n)].id);
    }
    CHECK(kEffectBrushColours[10] == 0x800000u);
    CHECK(kEffectBrushColours[19] == 0x0080ffu);
    create_effect_brushes(out, st);
    CHECK(out.brushes.size() == 20);  // once [DESIGN]
}

TEST_CASE("effects: seg7:0x2e02 latches the break-up's source and releases an owned old one") {
    struct Host : CollideHost {
        void erase(int, int, int, int) override {}
    } h;
    World w;
    WaveGlobals g;
    CollideGlobals cg;
    ViewState view;
    is::shell::Rng rng;
    std::int16_t a = 0, b = 0, hh = 0;
    CollideCtx x{w, g, cg, h, view, nullptr, rng, a, b, hh};
    clear_effects(w.effects);
    w.pools.set_in_use(4, 1);
    w.pools.rec(4, 0).size = 30;
    EffectLinks saved = effect_links();
    std::vector<std::pair<int, int>> asked;
    std::vector<DcId> released;
    effect_links().break_source = [&asked](int pool, int rec) {
        asked.emplace_back(pool, rec);
        return BreakSource{0x55, pool == 1};
    };
    effect_links().release = [&released](DcId d) { released.push_back(d); };
    EffectState& st = effect_state();
    const EffectState before = st;
    st.piece_src[0] = 0x44;  // an owned source left by an entry that never ended
    st.piece_src_owned[0] = true;
    pickup_destroyed(x, 4, 0);
    CHECK(released == std::vector<DcId>{0x44});
    CHECK(asked == std::vector<std::pair<int, int>>{{4, 0}});
    CHECK(st.piece_src[0] == 0x55);
    CHECK_FALSE(st.piece_src_owned[0]);
    CHECK(w.effects[0].type == 6);
    CHECK(w.effects[0].side == 6);
    // A shared source is forgotten, not released [DESIGN].
    w.effects[0].frame = kEffectFree;
    w.pools.rec(4, 0).state = 0;
    released.clear();
    pickup_destroyed(x, 4, 0);
    CHECK(released.empty());
    effect_links() = saved;
    st = before;
}

TEST_CASE("effects: seg10:0x22e releases the break-up sources at the wave teardown (review L17)") {
    EffectState st;
    std::vector<DcId> released;
    EffectLinks saved = effect_links();
    effect_links().release = [&released](DcId dc) { released.push_back(dc); };
    st.piece_src = {0x31, kDcNone, 0x33, 0x34};
    st.piece_src_owned = {true, false, false, true};
    release_piece_sources(st);
    // Every entry back to 0 [BYTES seg10:0x2f4]; the port deletes only its own DCs [DESIGN].
    CHECK(released == std::vector<DcId>{0x31, 0x34});
    for (int e = 0; e < kPieceEntries; ++e) {
        CHECK(st.piece_src[static_cast<std::size_t>(e)] == kDcNone);
        CHECK_FALSE(st.piece_src_owned[static_cast<std::size_t>(e)]);
    }
    effect_links() = saved;
}
