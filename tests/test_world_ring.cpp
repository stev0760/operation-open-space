// RING OF DEATH (src/world/ring.h; work/analysis/m4/s7-ring.md section 1 with s7-ring-verify.md:
// claims 1-9, claim 6 as corrected, Omissions O1-O3): the gate, the fuel rule with no floor, the
// 24 ring entries and the two 0xff06 centre entries, the entry search, DGROUP:0x9430 and
// DGROUP:0x9434, and the effect frame seg8:0x3fbe and the blast seg8:0x5ab0 driving them (one
// KABOOM per entry, one blast per centre entry at its frame 6 that skips the user, the
// DGROUP:0x1318 gate). The WARBIRD's fuel steps 71, 70, 69, 68, 67, 66 are core-0009's
// [ORACLE core-0009].
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/effects.h"
#include "world/ring.h"

using namespace is::world;

namespace {

struct Hit {
    int pool, rec, sp, sr;
    std::int16_t amount;
};

struct Host : EffectHost {
    EffectState* st = nullptr;
    std::vector<std::string> sounds;  // "id mode @x,y" (the KABOOM point DGROUP:0x5144/0x5146)
    std::vector<Hit> hits;
    void sound(int id, int, int, int mode) override {
        sounds.push_back(std::to_string(id) + " " + std::to_string(mode) + " @" +
                         std::to_string(st ? st->dg_5144 : 0) + "," + std::to_string(st ? st->dg_5146 : 0));
    }
    void hit(int pool, int rec, int sp, int sr, std::int16_t amount) override {
        hits.push_back({pool, rec, sp, sr, amount});
    }
};

// The player's ship (pool 14 record 0) with RING OF DEATH in slot `slot`, slots 1 ... 3 in use.
struct Rig {
    World w;
    EffectState st;
    EffectWords in;
    Host host;
    is::shell::Rng rng;
    RecordingBlitter out;
    ImageSlotTable images{};
    EffectCtx c{w, st, in, host, rng, &out, &images, nullptr, nullptr, nullptr};
    explicit Rig(int slot = 1) {
        host.st = &st;
        w.size = 1;
        w.W = 640;
        w.H = 400;
        w.player = 0;
        w.last_ship = 0;
        clear_effects(w.effects);
        ShipRecord& s = w.ships[0];
        s.slots_cb() = 4;
        s.weapon[1] = 4;
        s.weapon[2] = 4;
        s.weapon[3] = 4;
        s.weapon[static_cast<std::size_t>(slot)] = kWeaponRingOfDeath;
        s.fuel = 1000;
        ObjectRecord& u = w.pools.rec(kPoolShips, 0);
        u.x = 1000;
        u.y = 2000;
        u.scr_x = 300;
        u.scr_y = 200;
        u.mass = 325;
        u.blast_size = 11;  // TRANSFORMER: design mass 259 / 22 [BYTES seg14:0x20fb-0x210a]
        w.pools.set_in_use(kPoolShips, 1);
    }
    ObjectRecord& user() { return w.pools.rec(kPoolShips, 0); }
};

// The verified table of claim 4: block k's signs, whether x takes a, and m = base + sign·j.
struct Block {
    int sx;
    bool xa;
    int sy;
    int base, sign;
};
const Block kBlocks[8] = {{+1, true, -1, 0, +1},  {+1, false, -1, 5, -1}, {+1, false, +1, 5, +1},
                          {+1, true, +1, 10, -1}, {-1, true, -1, 20, -1}, {-1, false, -1, 15, +1},
                          {-1, false, +1, 15, -1}, {-1, true, +1, 10, +1}};
const int kA[3] = {11, 32, 53};
const int kB[3] = {79, 73, 60};  // seg8:0x1290(6400 - a², 80 - a / 4)

}  // namespace

TEST_CASE("ring: the 26 entries of a player with RING OF DEATH in slot 1 (claims 3-7, 6 as corrected)") {
    Rig r;
    r.w.w9430 = 0;
    r.w.w9434 = 0;
    const RingResult res = ring_of_death(r.w, r.st.sides, 5000, kPoolShips, 0);
    CHECK(res.acted);
    CHECK(res.start == 1);
    CHECK(res.ring == 24);
    CHECK(res.centre == 2);
    // Entry 0 stays free: the searches start at the slot index l = 1.
    CHECK(r.w.effects[0].frame == kEffectFree);
    for (int j = 0; j < 3; ++j) {
        for (int k = 0; k < 8; ++k) {
            const int e = 1 + 8 * j + k;
            CAPTURE(e);
            const Block& b = kBlocks[k];
            const int u = b.xa ? kA[j] : kB[j], v = b.xa ? kB[j] : kA[j];
            const EffectEntry& E = r.w.effects[static_cast<std::size_t>(e)];
            CHECK(E.type == 1);
            CHECK(E.frame == 1);
            CHECK(E.end == 10);
            CHECK(E.pool == kPoolShips);
            CHECK(E.rec == 0);
            CHECK(E.start.get() == 5000u + static_cast<std::uint32_t>(30 * (b.base + b.sign * j)));
            CHECK(r.st.sides.point[static_cast<std::size_t>(e)].x == 1000 + b.sx * 8 * u);
            CHECK(r.st.sides.point[static_cast<std::size_t>(e)].y == 2000 + b.sy * 8 * v);
            CHECK(r.st.sides.centre[static_cast<std::size_t>(e)].x == 300 + b.sx * u);  // Size 1: (u << 1) >> 1
            CHECK(r.st.sides.centre[static_cast<std::size_t>(e)].y == 200 + b.sy * v);
        }
    }
    // Literal values of the first iteration (s7-ring.md "Ring values").
    CHECK(r.st.sides.point[1].x == 1088);
    CHECK(r.st.sides.point[1].y == 1368);
    CHECK(r.st.sides.centre[1].x == 311);
    CHECK(r.st.sides.centre[1].y == 121);
    CHECK(r.st.sides.point[2].x == 1632);  // block 2: X + 8b
    CHECK(r.st.sides.centre[24].x == 300 - 53);  // block 8 of j = 2: SX - a
    CHECK(r.st.sides.centre[24].y == 200 + 60);
    // The two centre entries at 25 and 26: the user's own points, 0xff06, count 13, now and now + 360.
    for (int e : {25, 26}) {
        const EffectEntry& E = r.w.effects[static_cast<std::size_t>(e)];
        CHECK(E.type == kEffectTypeFF06);
        CHECK(E.frame == 1);
        CHECK(E.end == 13);
        CHECK(r.st.sides.point[static_cast<std::size_t>(e)].x == 1000);
        CHECK(r.st.sides.point[static_cast<std::size_t>(e)].y == 2000);
        CHECK(r.st.sides.centre[static_cast<std::size_t>(e)].x == 300);
        CHECK(r.st.sides.centre[static_cast<std::size_t>(e)].y == 200);
    }
    CHECK(r.w.effects[25].start.get() == 5000u);
    CHECK(r.w.effects[26].start.get() == 5360u);
    CHECK(r.w.effects[27].frame == kEffectFree);
    CHECK(r.w.w9430 == 26);
    CHECK(r.w.w9434 == 26);
}

TEST_CASE("ring: clockwise from 12 o'clock over 0 ... 600 ms (claim 4)") {
    Rig r;
    ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
    // Sort the ring's screen offsets by start: each later point lies clockwise (screen y down),
    // so the cross product of successive offsets is never negative.
    struct P {
        std::uint32_t t;
        int x, y;
    };
    std::vector<P> v;
    for (int e = 1; e <= 24; ++e) {
        const auto& c = r.st.sides.centre[static_cast<std::size_t>(e)];
        v.push_back({r.w.effects[static_cast<std::size_t>(e)].start.get(), c.x - 300, c.y - 200});
    }
    std::stable_sort(v.begin(), v.end(), [](const P& a, const P& b) { return a.t < b.t; });
    CHECK(v.front().t == 0u);
    CHECK(v.front().x == 11);
    CHECK(v.front().y == -79);  // 12 o'clock
    CHECK(v.back().t == 600u);
    CHECK(v.back().x == -11);
    CHECK(v.back().y == -79);
    // (Two entries with one start appear together; their code order is not compared.)
    for (std::size_t i = 1; i < v.size(); ++i)
        if (v[i].t != v[i - 1].t) CHECK(v[i - 1].x * v[i].y - v[i - 1].y * v[i].x >= 0);
    // Three pairs share a start: 150, 300, 450 ms; 21 distinct times.
    int distinct = 1;
    for (std::size_t i = 1; i < v.size(); ++i) distinct += v[i].t != v[i - 1].t;
    CHECK(distinct == 21);
}

TEST_CASE("ring: the screen offsets at Size 0 and 2; the world offsets do not change") {
    for (int size : {0, 2}) {
        Rig r;
        r.w.size = static_cast<std::int16_t>(size);
        ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
        const int as[3][2] = {{5, 39}, {16, 36}, {26, 30}};       // Size 0: (u << 0) >> 1
        const int bs[3][2] = {{22, 158}, {64, 146}, {106, 120}};  // Size 2: (u << 2) >> 1
        for (int j = 0; j < 3; ++j) {
            CAPTURE(size);
            CAPTURE(j);
            const int e = 1 + 8 * j;  // block 1: (+a, -b)
            const int sa = size == 0 ? as[j][0] : bs[j][0], sb = size == 0 ? as[j][1] : bs[j][1];
            CHECK(r.st.sides.centre[static_cast<std::size_t>(e)].x == 300 + sa);
            CHECK(r.st.sides.centre[static_cast<std::size_t>(e)].y == 200 - sb);
            CHECK(r.st.sides.point[static_cast<std::size_t>(e)].x == 1000 + 8 * kA[j]);
            CHECK(r.st.sides.point[static_cast<std::size_t>(e)].y == 2000 - 8 * kB[j]);
        }
    }
}

TEST_CASE("ring: the gate and the fuel rule (claims 1, 2)") {
    // Fuel 70 refuses, 71 acts.
    {
        Rig r;
        r.w.ships[0].fuel = 70;
        const RingResult res = ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
        CHECK_FALSE(res.acted);
        CHECK(r.w.ships[0].fuel == 70);
        CHECK(r.w.w9430 == 0);
        CHECK(r.w.effects[1].frame == kEffectFree);
    }
    {
        // TRANSFORMER at full tank: mass 325, cost 70·325 / 250 = 91; from 71 the fuel goes to -20,
        // no floor.
        Rig r;
        r.w.ships[0].fuel = 71;
        const RingResult res = ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
        CHECK(res.acted);
        CHECK(res.cost == -91);
        CHECK(r.w.ships[0].fuel == -20);
        CHECK(res.ring == 24);
    }
    {
        // The first slot holding 1 decides: slot 2 here, so the searches start at index 2.
        Rig r(2);
        const RingResult res = ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
        CHECK(res.start == 2);
        CHECK(r.w.effects[1].frame == kEffectFree);
        CHECK(r.w.effects[2].type == 1);
        CHECK(r.w.effects[27].type == kEffectTypeFF06);
        CHECK(r.w.w9434 == 27);
    }
    {
        // A slot past +0xcb - 1 is not searched: no weapon 1 found, nothing.
        Rig r(3);
        r.w.ships[0].slots_cb() = 3;
        CHECK_FALSE(ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0).acted);
        CHECK(r.w.ships[0].fuel == 1000);
    }
    {
        // No slot at all (+0xcb = 1).
        Rig r;
        r.w.ships[0].slots_cb() = 1;
        CHECK_FALSE(ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0).acted);
    }
}

TEST_CASE("ring: the WARBIRD's fuel steps of core-0009 (71, 70, 69, 68, 67, 66)") {
    // WARBIRD: SHP armor 18, body 9 strength base 21: design mass 210·(18 / 4 + 100) / 100 = 218
    // [BYTES MASTER.SHP MASTER.BOD seg14:0x1e35-0x1e8f]; the object's mass is the design mass +
    // fuel / 20, updated by the 1000-ms tick between the presses [BYTES seg11:0x1d26]. Start fuel
    // 756 [ORACLE core-0009].
    Rig r;
    r.w.ships[0].fuel = 756;
    const std::int16_t expected[6] = {685, 615, 546, 478, 411, 345};
    for (int i = 0; i < 6; ++i) {
        CAPTURE(i);
        r.user().mass = static_cast<std::int16_t>(218 + r.w.ships[0].fuel / 20);
        clear_effects(r.w.effects);
        ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
        CHECK(r.w.ships[0].fuel == expected[i]);
    }
}

TEST_CASE("ring: a user outside the ship pool: no gate, no cost, entries from 0 (O4)") {
    Rig r;
    ObjectRecord& post = r.w.pools.rec(kPoolGatePosts, 3);
    post.x = 400;
    post.y = 800;
    post.scr_x = 50;
    post.scr_y = 60;
    r.w.ships[0].fuel = 10;
    const RingResult res = ring_of_death(r.w, r.st.sides, 1000, kPoolGatePosts, 3);
    CHECK(res.acted);
    CHECK(res.start == 0);
    CHECK(res.cost == 0);
    CHECK(r.w.ships[0].fuel == 10);
    CHECK(r.w.effects[0].type == 1);
    CHECK(r.w.effects[0].pool == kPoolGatePosts);
    CHECK(r.w.effects[0].rec == 3);
    CHECK(r.st.sides.point[0].x == 400 + 88);
    CHECK(r.w.effects[24].type == kEffectTypeFF06);
    CHECK(r.w.effects[25].start.get() == 1360u);
    CHECK(r.w.w9434 == 25);
}

TEST_CASE("ring: the searches over busy entries (claim 6 as corrected, O2)") {
    {
        // Holes: entries 1 ... 10 busy except 5. The search from 1 finds 5, the next from 6 finds
        // 11, then 12, ...: the ring is 5 and 11 ... 33, the centres 34 and 35.
        Rig r;
        for (int e = 1; e <= 10; ++e)
            if (e != 5) r.w.effects[static_cast<std::size_t>(e)].frame = 3;
        const RingResult res = ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
        CHECK(res.ring == 24);
        CHECK(r.w.effects[5].type == 1);
        CHECK(r.w.effects[11].type == 1);
        CHECK(r.w.effects[33].type == 1);
        CHECK(r.w.effects[34].type == kEffectTypeFF06);
        CHECK(r.w.effects[35].type == kEffectTypeFF06);
        CHECK(r.w.w9434 == 35);
    }
    {
        // Free 0 ... 10, busy 11 ... 44: ten ring entries at 1 ... 10; the 11th search stops at 45,
        // so the next starts at 46 and every later one fails: no more ring, no centre, and
        // entry 0 (before the start) stays free. DGROUP:0x9434 is not raised (O2).
        Rig r;
        for (int e = 11; e < kEffectCount; ++e) r.w.effects[static_cast<std::size_t>(e)].frame = 2;
        r.w.w9434 = 0;
        const RingResult res = ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
        CHECK(res.ring == 10);
        CHECK(res.centre == 0);
        CHECK(r.w.effects[0].frame == kEffectFree);
        CHECK(r.w.effects[10].type == 1);
        CHECK(r.w.w9430 == 10);
        CHECK(r.w.w9434 == 0);
    }
    {
        // A full table: nothing placed, the fuel still paid.
        Rig r;
        for (auto& e : r.w.effects) e.frame = 1;
        const RingResult res = ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
        CHECK(res.acted);
        CHECK(res.ring + res.centre == 0);
        CHECK(r.w.ships[0].fuel == 1000 - 91);
    }
}

TEST_CASE("ring: a 0xff06 entry through seg8:0x3fbe: KABOOM at frame 1, one blast at frame 6 skipping the user (claims 8, 9, O1)") {
    Rig r;
    ring_of_death(r.w, r.st.sides, 5000, kPoolShips, 0);
    // A big rock 80 world units right of the user's top-left (10 cells: d = 100 after the floor).
    ObjectRecord& rock = r.w.pools.rec(kPoolRocks, 0);
    rock.x = 1080;
    rock.y = 2000;
    rock.mass = 1400;
    r.w.pools.set_in_use(kPoolRocks, 1);
    r.in.now = 5000;
    // Entry 25 (start now): 12 calls free it; the KABOOM plays on the first, at the user's point.
    for (int call = 1; call <= 12; ++call) {
        CAPTURE(call);
        effect_frame(r.c, 25);
        if (call < 12) CHECK(r.w.effects[25].frame == call + 1);
    }
    CHECK(r.w.effects[25].frame == kEffectFree);
    REQUIRE(r.host.sounds.size() == 1);
    CHECK(r.host.sounds[0] == "69 3 @1000,2000");  // 0x45 mode 3 at DGROUP:0x5224[25]
    // One blast, at frame 6: the rock only (the user is skipped), TRANSFORMER's B = 11 on a big
    // rock at d <= 100: ((11·600 / 100)·15) / 40 = 24 (s7-ring.md 1.4).
    REQUIRE(r.host.hits.size() == 1);
    CHECK(r.host.hits[0].pool == kPoolRocks);
    CHECK(r.host.hits[0].rec == 0);
    CHECK(r.host.hits[0].sp == kPoolShips);
    CHECK(r.host.hits[0].amount == -24);
    // No fireball (type <= -1) and no blackout on the last frame (straight to the advance).
    CHECK(r.out.calls.empty());
    // Entry 26 (start now + 360) waits without a frame step until its start.
    effect_frame(r.c, 26);
    CHECK(r.w.effects[26].frame == 1);
    CHECK(r.host.sounds.size() == 1);
    r.in.now = 5360;
    effect_frame(r.c, 26);
    CHECK(r.w.effects[26].frame == 2);
    CHECK(r.host.sounds.size() == 2);
}

TEST_CASE("ring: the blast's DGROUP:0x1318 gate and the type-1 entries that never blast") {
    Rig r;
    ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
    ObjectRecord& rock = r.w.pools.rec(kPoolRocks, 0);
    rock.x = 1080;
    rock.y = 2000;
    rock.mass = 1400;
    r.w.pools.set_in_use(kPoolRocks, 1);
    r.in.now = 0;
    r.in.dg_1318 = 7;  // > 6: the blast does nothing [BYTES seg8:0x5aec]
    for (int call = 0; call < 12; ++call) effect_frame(r.c, 25);
    CHECK(r.host.hits.empty());
    // A type-1 ring entry: KABOOM at frame 1, fireballs, no blast over its 9 frames.
    r.in.dg_1318 = 0;
    r.host.sounds.clear();
    for (int call = 0; call < 9; ++call) effect_frame(r.c, 1);
    CHECK(r.w.effects[1].frame == kEffectFree);
    CHECK(r.host.hits.empty());
    REQUIRE(r.host.sounds.size() == 1);
    CHECK(r.host.sounds[0] == "69 3 @1088,1368");
    int blits = 0;
    for (const BlitCall& b : r.out.calls) blits += b.op == BlitCall::kBitBlt;
    CHECK(blits == 8);  // fireballs at frames 1 ... 8, centred on DGROUP:0x5170[1]
}

TEST_CASE("ring: the whole effect over 0.9 s: 26 KABOOMs and two blasts (O1)") {
    Rig r;
    ring_of_death(r.w, r.st.sides, 0, kPoolShips, 0);
    ObjectRecord& rock = r.w.pools.rec(kPoolRocks, 0);
    rock.x = 1000;
    rock.y = 1800;
    rock.mass = 350;
    r.w.pools.set_in_use(kPoolRocks, 1);
    // One frame every 30 ms, the frame loop k = 0 ... DGROUP:0x9434 (world.md).
    for (std::uint32_t t = 0; t <= 900; t += 30) {
        r.in.now = t;
        for (int k = 0; k <= r.w.w9434; ++k) effect_frame(r.c, k);
    }
    CHECK(r.host.sounds.size() == 26);
    CHECK(r.host.hits.size() == 2);
    for (const Hit& h : r.host.hits) CHECK(h.pool == kPoolRocks);
    for (int k = 0; k < kEffectCount; ++k) CHECK(r.w.effects[static_cast<std::size_t>(k)].frame == kEffectFree);
    CHECK(r.w.w9430 == 0);
}
