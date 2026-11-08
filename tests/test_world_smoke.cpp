// The smoke (src/world/smoke.h; work/analysis/m4/s5-effects.md section 2 with
// s5-effects-verify.md claims 35-53, 56, 57, A1, A3): the records DGROUP:0x58bc, the puff
// seg7:0x5b68 (the Detail gate, the object and ship branches, the sound, DGROUP:0x9456), the debris
// puff seg7:0x5d6a, the mover seg7:0x5e04 (the six sizes and three cells from slot 12, the Detail
// gates, the 55-call life, the end square, DGROUP:0x58a), seg8:0x124a, the placement's reset and
// the strip seg10:0x5202; and the five (band, cell) pairs of core-0006's tf/wing-c0142 ... c0146.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <tuple>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "formats/dib.h"
#include "world/flight.h"
#include "world/shots.h"
#include "world/smoke.h"

using namespace is::world;

namespace {

struct Sound {
    int id, pool, rec, mode;
    bool operator==(const Sound& o) const { return id == o.id && pool == o.pool && rec == o.rec && mode == o.mode; }
};

struct Fixture {
    SmokeRecords table{};
    std::int16_t n9456 = 0;
    Pools pools;
    ShipTable ships{};
    ShotState shots;
    is::shell::Rng rng;
    RecordingBlitter out;
    ImageSlotTable images{};
    VisibilityGlobals vis;
    std::int16_t d58a = 0;
    std::vector<Sound> sounds;
    std::vector<const char*> unported;
    SmokeCtx c{table, n9456, pools, rng};
    Fixture() {
        smoke_place(table);
        c.ships = &ships;
        c.shots = &shots;
        c.out = &out;
        c.images = &images;
        c.visible = &vis;
        c.dg_58a = &d58a;
        c.to_screen = [](std::int16_t x, std::int16_t y) {
            return Point16{static_cast<std::int16_t>(x >> 3), static_cast<std::int16_t>(y >> 3)};
        };
        c.sound = [this](int id, int pool, int rec, int mode) { sounds.push_back({id, pool, rec, mode}); };
        c.unported = [this](const char* a) { unported.push_back(a); };
        images[kSlotSmoke] = 0x60;
    }
    // A record k at counter `counter`, world (x, y), velocity (vx, vy).
    SmokeRecord& put(int k, std::int16_t counter, std::int16_t x, std::int16_t y, std::int16_t vx = 0,
                     std::int16_t vy = 0) {
        SmokeRecord& s = table[static_cast<std::size_t>(k)];
        s.counter = counter;
        s.x = x, s.y = y, s.vx = vx, s.vy = vy;
        return s;
    }
};

// A state whose first rand() % 100 is `want`.
std::uint32_t state_for(int want) {
    for (std::uint32_t s = 1;; ++s) {
        is::shell::Rng r;
        r.set_state(s);
        if (r.rand() % 100 == want) return s;
    }
}

}  // namespace

TEST_CASE("smoke: the record DGROUP:0x58bc and the placement's reset [BYTES seg9:0x6747-0x675d]") {
    CHECK(sizeof(SmokeRecord) == 0x14);
    CHECK(kSmokeRecords == 20);
    CHECK(kDgSmokeTable + offsetof(SmokeRecord, x) == 0x58c4);
    CHECK(kDgSmokeTable + offsetof(SmokeRecord, vx) == 0x58c8);
    CHECK(kDgSmokeTable + offsetof(SmokeRecord, counter) == 0x58cc);
    CHECK(kDgSmokeTable + offsetof(SmokeRecord, drawn) == 0x58ce);
    SmokeRecords t{};
    for (SmokeRecord& s : t) {
        s.counter = 7;
        s.sx = 11;
        s.drawn = 1;
        s.x = 99;
    }
    smoke_place(t);
    for (const SmokeRecord& s : t) {
        CHECK(s.counter == -1);
        CHECK(s.sx == 11);  // no other field
        CHECK(s.drawn == 1);
        CHECK(s.x == 99);
    }
}

TEST_CASE("smoke: seg7:0x5b68 on an object, the s5-effects 2.2 vector") {
    Fixture f;
    f.c.detail = 3;
    f.pools.set_in_use(3, 1);
    ObjectRecord& o = f.pools.rec(3, 0);
    o.x = 4000, o.y = 7000, o.extent = 344, o.vx = 12, o.vy = -6;
    f.rng.set_state(0xabcdef01u);
    is::shell::Rng expect = f.rng;
    CHECK(expect.rand() == 32060);
    CHECK(expect.rand() == 16249);
    CHECK(expect.rand() == 5914);
    CHECK(smoke_puff(f.c, 3, 0, 0) == 0);
    CHECK(f.rng.state() == expect.state());  // Detail 3: no gate draw, then x, y, d
    const SmokeRecord& s = f.table[0];
    CHECK(s.counter == 0);
    CHECK(s.x == 4068);
    CHECK(s.y == 7081);
    CHECK(s.vx == 1);  // H[10] = (-11, 11): -11 / 2 + 12 / 2
    CHECK(s.vy == 2);  // 11 / 2 + -6 / 2
    CHECK(f.n9456 == 1);
    CHECK(f.sounds.empty());  // arg 0: no sound
    // arg 1 plays seg10:0x754a(1, pool, rec, 1); the next free record is taken.
    CHECK(smoke_puff(f.c, 3, 0, 1) == 1);
    CHECK(f.sounds == std::vector<Sound>{{1, 3, 0, 1}});
    CHECK(f.n9456 == 2);
}

TEST_CASE("smoke: seg7:0x5b68 on a ship takes muzzle entry 16 + heading and no rand()") {
    Fixture f;
    f.c.detail = 3;
    f.c.ship_pool = kPoolShips;
    f.pools.set_in_use(kPoolShips, 3);
    ObjectRecord& o = f.pools.rec(kPoolShips, 2);
    o.x = 8000, o.y = 9000, o.vx = 40, o.vy = -20, o.frame = 3;  // H[3] = (15, -6)
    f.ships[2].design_index = 5;
    f.shots.mx(5, 16 + 3) = 20;
    f.shots.my(5, 16 + 3) = 30;
    const std::uint32_t before = f.rng.state();
    CHECK(smoke_puff(f.c, kPoolShips, 2, 1) == 0);
    CHECK(f.rng.state() == before);
    const SmokeRecord& s = f.table[0];
    CHECK(s.x == 8000 + (20 - 3 - 4) * 8);     // 15 / 4 = 3
    CHECK(s.y == 9000 + (30 - (-1) - 4) * 8);  // -6 / 4 = -1, toward zero
    CHECK(s.vx == 40 - 7);                     // 15 / 2 = 7
    CHECK(s.vy == -20 + 3);                    // -6 / 2 = -3
    CHECK(f.sounds == std::vector<Sound>{{1, kPoolShips, 2, 1}});
    // At Detail 1 the ship's puff draws only the gate's rand().
    f.c.detail = 1;
    f.rng.set_state(state_for(45));
    is::shell::Rng e = f.rng;
    e.rand();
    CHECK(smoke_puff(f.c, kPoolShips, 2, 0) == 1);
    CHECK(f.rng.state() == e.state());
}

TEST_CASE("smoke: seg7:0x5b68's Detail gate, r <= 25, 45, 65 starts; Detail 3 always") {
    const int thr[3] = {25, 45, 65};
    for (int d = 0; d < 3; ++d) {
        CAPTURE(d);
        Fixture f;
        f.c.detail = static_cast<std::int16_t>(d);
        f.pools.set_in_use(3, 1);
        f.pools.rec(3, 0).extent = 100;
        f.rng.set_state(state_for(thr[d]));
        CHECK(smoke_puff(f.c, 3, 0, 1) == 0);  // r = thr: `jle` proceeds
        f.rng.set_state(state_for(thr[d] + 1));
        const std::uint32_t s = f.rng.state();
        is::shell::Rng e;
        e.set_state(s);
        e.rand();
        CHECK(smoke_puff(f.c, 3, 0, 1) == -1);  // thr + 1 returns
        CHECK(f.rng.state() == e.state());      // one draw only
        CHECK(f.sounds.size() == 1);
        CHECK(f.n9456 == 1);
    }
    // A Detail outside 0 ... 3 draws the gate's rand() and takes the record [DESIGN].
    Fixture f;
    f.c.detail = 5;
    f.pools.set_in_use(3, 1);
    f.pools.rec(3, 0).extent = 100;
    f.rng.set_state(state_for(99));
    CHECK(smoke_puff(f.c, 3, 0, 0) == 0);
}

TEST_CASE("smoke: all 20 busy: no rand() beyond the gate, no sound, no count [BYTES seg7:0x5c84]") {
    Fixture f;
    f.c.detail = 3;
    for (int k = 0; k < kSmokeRecords; ++k) f.put(k, 4, 0, 0);
    f.n9456 = 20;
    f.pools.set_in_use(3, 1);
    f.pools.rec(3, 0).extent = 100;
    const std::uint32_t before = f.rng.state();
    CHECK(smoke_puff(f.c, 3, 0, 1) == -1);
    CHECK(f.rng.state() == before);
    CHECK(f.sounds.empty());
    CHECK(f.n9456 == 20);
}

TEST_CASE("smoke: seg7:0x5d6a takes a record by Detail (moved from debris.h)") {
    SmokeRecords t{};
    smoke_place(t);
    std::int16_t n = 0;
    is::shell::Rng rng;
    is::shell::Rng expect = rng;
    CHECK(smoke_start(t, n, 3, rng, 10, 20, 1, 5, 6) == 0);
    CHECK(rng.state() == expect.state());  // Detail 3 draws nothing
    CHECK(t[0].x == 10);
    CHECK(t[0].y == 20);
    CHECK(t[0].vx == 5);
    CHECK(t[0].vy == 6);
    CHECK(t[0].counter == 0);
    CHECK(n == 1);
    // Detail 0: a draw below 20 takes nothing (`jge` takes: the opposite sense to seg7:0x5b68's).
    int taken = 0;
    is::shell::Rng r2;
    is::shell::Rng e2 = r2;
    for (int i = 0; i < 18; ++i) {
        const bool want = e2.rand() % 100 >= 20;
        const int k = smoke_start(t, n, 0, r2, 0, 0, 1, 0, 0);
        CHECK((k >= 0) == want);
        taken += k >= 0;
    }
    CHECK(n == 1 + taken);
}

TEST_CASE("smoke: seg7:0x5e04's cells, sides and offsets at Size 1 (verified claim 44)") {
    // Source x for c = 0, 3, 6, 9, 17, 18, 27, 36, 45, 53 (s5-effects.md 2.3).
    const std::array<std::pair<int, int>, 10> want = {
        {{0, 0}, {3, 10}, {6, 20}, {9, 30}, {17, 54}, {18, 66}, {27, 108}, {36, 156}, {45, 210}, {53, 250}}};
    for (const auto& [cc, sx] : want) {
        CAPTURE(cc);
        const PuffCell p = puff_cell(static_cast<std::int16_t>(cc), 1);
        CHECK(p.src_x == sx);
        CHECK(p.p == 2 * (cc / 9 + 5));  // 10, 12, ... 20
        CHECK(p.h == cc / 9);            // centred growth
        CHECK(p.src_x_defined);
    }
    // Counter 54 reads T[6], past the table (A1).
    CHECK_FALSE(puff_cell(54, 1).src_x_defined);
    CHECK(puff_cell(54, 1).p == 22);
    // The cell picks seg7:0x5e04 makes land on the bands seg10:0x5202 copies: band q at x = 3 y_q
    // (0, 30, 66, 108, 156, 210), cell (c / 3) % 3 of side p within it.
    RecordingBlitter out;
    ImageSlotTable images{};
    build_smoke_image(out, images);
    std::vector<int> band_x;
    for (const BlitCall& b : out.calls)
        if (b.op == BlitCall::kBitBlt) band_x.push_back(b.x);
    REQUIRE(band_x.size() == 6);
    for (int cc = 0; cc < 54; ++cc) {
        const PuffCell p = puff_cell(static_cast<std::int16_t>(cc), 1);
        CHECK(p.src_x == band_x[static_cast<std::size_t>(cc / 9)] + p.p * ((cc / 3) % 3));
    }
    // core-0006 tf/wing-c0142 ... c0146: counters 3, 10, 17, 24, 31 are band 0 cell 1, band 1
    // cell 0, band 1 cell 2, band 2 cell 2, band 3 cell 1 (verified claim 56).
    const std::array<std::tuple<int, int, int>, 5> wing = {{{3, 0, 1}, {10, 1, 0}, {17, 1, 2}, {24, 2, 2}, {31, 3, 1}}};
    for (const auto& [cc, band, cell] : wing) {
        CAPTURE(cc);
        const PuffCell p = puff_cell(static_cast<std::int16_t>(cc), 1);
        CHECK(p.q == band);
        CHECK(p.src_x == band_x[static_cast<std::size_t>(band)] + p.p * cell);
    }
    // Size 0 and 2 scale the side, the offset and the table.
    CHECK(puff_cell(9, 0).p == 6);
    CHECK(puff_cell(9, 0).h == 0);
    CHECK(puff_cell(9, 0).src_x == 15);
    CHECK(puff_cell(9, 2).p == 24);
    CHECK(puff_cell(9, 2).h == 2);
    CHECK(puff_cell(9, 2).src_x == 60);
}

TEST_CASE("smoke: seg10:0x5202 builds slot 12 from #4000 200 [BYTES seg10:0x5210-0x532b]") {
    RecordingBlitter out;
    ImageSlotTable images{};
    build_smoke_image(out, images);
    REQUIRE(out.images.size() == 2);
    CHECK(out.images[0].graphic == 200);
    const DcId src = out.images[0].id;
    const DcId strip = images[kSlotSmoke];
    CHECK(strip == out.images[1].id);
    CHECK(out.images[1].w == 270);
    CHECK(out.images[1].h == 22);
    REQUIRE(out.calls.size() == 7);
    CHECK(out.calls[0] == pat_blt_call(strip, 0, 0, 270, 22, kBlackness));
    int y = 0;
    for (int k = 0; k < 6; ++k) {
        CAPTURE(k);
        CHECK(out.calls[static_cast<std::size_t>(k + 1)] ==
              bit_blt_call(strip, 3 * y, 0, 6 * (k + 5), 2 * (k + 5), src, 0, y, kSrcCopy));
        y += 2 * (k + 5);
    }
    CHECK(y == 90);
    CHECK(out.deleted == std::vector<DcId>{src});
    // Only while slot 12 is empty.
    build_smoke_image(out, images);
    CHECK(out.calls.size() == 7);
}

TEST_CASE("smoke: seg7:0x5e04, the first draw, the old square and the move") {
    Fixture f;
    f.c.detail = 3;
    f.n9456 = 1;
    f.put(0, 0, 800, 1600, 16, -8);
    move_smoke(f.c);
    const SmokeRecord& s = f.table[0];
    // The screen point of the world point before the move: (100, 200).
    REQUIRE(f.out.calls.size() == 1);
    CHECK(f.out.calls[0] == bit_blt_call(kDcWindow, 100, 200, 10, 10, 0x60, 0, 0, kSrcCopy));
    CHECK(s.sx == 100);
    CHECK(s.x == 816);
    CHECK(s.y == 1592);
    CHECK(s.drawn == 1);
    CHECK(s.counter == 1);
    f.out.calls.clear();
    move_smoke(f.c);  // c = 1: (102, 199), the old square (100, 200) blacked with this call's p, h
    REQUIRE(f.out.calls.size() == 2);
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 100, 200, 10, 10, kBlackness));
    CHECK(f.out.calls[1] == bit_blt_call(kDcWindow, 102, 199, 10, 10, 0x60, 0, 0, kSrcCopy));
    CHECK(s.old_x == 100);
    // A record at counter 9 grows: p 12, h 1, the old square at old - 1 with side 12.
    SmokeRecord& t = f.put(1, 9, 80, 80);
    t.sx = 10, t.sy = 10, t.drawn = 1;
    f.out.calls.clear();
    f.table[0].counter = -1;
    move_smoke(f.c);
    REQUIRE(f.out.calls.size() == 2);
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 9, 9, 12, 12, kBlackness));
    CHECK(f.out.calls[1] == bit_blt_call(kDcWindow, 9, 9, 12, 12, 0x60, 30, 0, kSrcCopy));
}

TEST_CASE("smoke: the 55-call life and the end square [BYTES seg7:0x6037-0x608b]") {
    Fixture f;
    f.c.detail = 3;
    f.n9456 = 1;
    f.put(0, 0, 800, 1600);
    for (int call = 0; call < 54; ++call) move_smoke(f.c);
    CHECK(f.table[0].counter == 54);
    CHECK(f.n9456 == 1);
    f.out.calls.clear();
    move_smoke(f.c);  // the 55th call: counter 54 (no BitBlt from the undefined T[6]), then the end
    const SmokeRecord& s = f.table[0];
    CHECK(s.counter == -1);
    CHECK(f.n9456 == 0);
    CHECK(s.drawn == 1);  // as if drawn [DESIGN]
    REQUIRE(f.out.calls.size() == 2);
    // c = 54: the old square with this call's h 6 and p 22, no BitBlt, then the end square.
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 100 - 6, 200 - 6, 22, 22, kBlackness));
    CHECK(f.out.calls[1] == pat_blt_call(kDcWindow, 100 - 6, 200 - 6, 24, 24, kBlackness));
    // A free record is left alone.
    f.out.calls.clear();
    move_smoke(f.c);
    CHECK(f.out.calls.empty());
    CHECK(f.n9456 == 0);
}

TEST_CASE("smoke: Detail 0 freezes every fourth call; Detail 1 and above never [BYTES seg7:0x60bb-0x60cc]") {
    Fixture f;
    f.c.detail = 0;
    f.n9456 = 1;
    f.put(0, 3, 800, 1600, 8, 8);
    move_smoke(f.c);  // 3 % 4 = 3 > 2: count only
    CHECK(f.out.calls.empty());
    CHECK(f.table[0].x == 800);
    CHECK(f.table[0].counter == 4);
    move_smoke(f.c);
    CHECK(f.table[0].x == 808);
    CHECK(f.out.calls.size() == 1);  // drawn was 0: no old square
    f.c.detail = 1;
    f.table[0].counter = 7;
    f.out.calls.clear();
    move_smoke(f.c);
    CHECK(f.table[0].x == 816);
    CHECK(f.out.calls.size() == 2);
}

TEST_CASE("smoke: at Detail >= 2 a listed object over the puff suppresses the draw, not the blackout") {
    Fixture f;
    f.c.detail = 2;
    f.n9456 = 1;
    SmokeRecord& s = f.put(0, 1, 800, 1600);
    s.sx = 100, s.sy = 200, s.drawn = 1;
    f.pools.set_in_use(3, 1);
    ObjectRecord& o = f.pools.rec(3, 0);
    o.scr_x = 105, o.scr_y = 205, o.size = 20;
    f.vis.count = 1;
    f.vis.pool[0] = 3;
    f.vis.rec[0] = 0;
    move_smoke(f.c);
    REQUIRE(f.out.calls.size() == 1);
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 100, 200, 10, 10, kBlackness));
    CHECK(s.drawn == 0);
    // The puff's square (100 ... 110) ends where the object's (110 ...) begins: no overlap.
    o.scr_x = 110;
    move_smoke(f.c);
    CHECK(s.drawn == 1);
    // Detail 1 never tests the list.
    f.c.detail = 1;
    o.scr_x = 100;
    move_smoke(f.c);
    CHECK(s.drawn == 1);
}

TEST_CASE("smoke: DGROUP:0x58a when the puff is near the player [BYTES seg7:0x5fcf-0x6025]") {
    Fixture f;
    f.c.detail = 3;
    f.c.ship_pool = kPoolShips;
    f.c.player = 0;
    f.pools.set_in_use(kPoolShips, 1);
    ObjectRecord& p = f.pools.rec(kPoolShips, 0);
    p.scr_x = 300, p.scr_y = 300, p.size = 33;
    f.n9456 = 1;
    f.put(0, 0, 8 * 290, 8 * 290);  // (290, 290): inside (300 - 10, 300 - 10, 300 + 33 + 10, ...)
    move_smoke(f.c);
    CHECK(f.d58a == 1);
    f.d58a = 0;
    f.put(0, 0, 8 * 289, 8 * 300);  // one pixel left of the grown square
    move_smoke(f.c);
    CHECK(f.d58a == 0);
    f.put(0, 0, 8 * 342, 8 * 342);
    move_smoke(f.c);
    CHECK(f.d58a == 1);
    f.d58a = 0;
    f.put(0, 0, 8 * 343, 8 * 300);  // the right edge 343 is exclusive
    move_smoke(f.c);
    CHECK(f.d58a == 0);
}

TEST_CASE("smoke: the seg18 mode's blocks are reported, not run") {
    Fixture f;
    f.c.dg_9130 = 1;
    f.n9456 = 1;
    f.put(0, 54, 800, 1600);
    move_smoke(f.c);
    CHECK(f.unported.size() == 2);
}

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

}  // namespace

TEST_CASE("smoke: #4000 200's cells for counters 3, 10, 17, 24, 31 against core-0006 tf/wing-c0142 ... c0146 (golden)") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::fmt::Dib dib = store.sprite(static_cast<std::uint16_t>(kGraphicSmoke));
    REQUIRE(dib.width == 60);
    REQUIRE(dib.height == 90);
    const std::vector<std::uint32_t> px = is::fmt::to_bgra(dib);
    // The strip as seg10:0x5202 builds it: band k at (0, y_k) of 6(k + 5) x 2(k + 5) to (3 y_k, 0).
    std::vector<std::uint32_t> strip(270 * 22, 0);
    int y = 0;
    for (int k = 0; k < 6; ++k) {
        const int w = 6 * (k + 5), h = 2 * (k + 5);
        for (int r = 0; r < h; ++r)
            for (int x = 0; x < w; ++x)
                strip[static_cast<std::size_t>(r * 270 + 3 * y + x)] =
                    px[static_cast<std::size_t>((y + r) * 60 + x)] & 0xffffffu;
        y += h;
    }
    // core-0006 tf/wing-c0142 ... c0146 (verified claim 56): non-black pixels and their boxes.
    struct Want {
        int counter, lit, bw, bh;
    };
    const std::array<Want, 5> want = {{{3, 35, 8, 8}, {10, 62, 11, 10}, {17, 91, 12, 12}, {24, 135, 14, 14}, {31, 174, 16, 16}}};
    for (const Want& wv : want) {
        CAPTURE(wv.counter);
        const PuffCell p = puff_cell(static_cast<std::int16_t>(wv.counter), 1);
        int lit = 0, x0 = 1000, y0 = 1000, x1 = -1, y1 = -1, red = 0;
        for (int r = 0; r < p.p; ++r)
            for (int x = 0; x < p.p; ++x) {
                const std::uint32_t v = strip[static_cast<std::size_t>(r * 270 + p.src_x + x)];
                if (v == 0) continue;
                ++lit;
                if (v == 0xc00000u) ++red;  // (192, 0, 0) as 0x00rrggbb
                x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1;
                y0 = r < y0 ? r : y0, y1 = r > y1 ? r : y1;
            }
        CHECK(lit == wv.lit);
        CHECK(x1 - x0 + 1 == wv.bw);
        CHECK(y1 - y0 + 1 == wv.bh);
        if (wv.counter == 3) CHECK(red == 3);  // band 0 cell 1: the three red pixels (claim 57)
    }
}
