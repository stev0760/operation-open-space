// A dead ship's sheet and pod (src/world/death.h; work/analysis/m4/s1-death.md section 3,
// verified claims 28-29 and 33-35): seg13:0x4710's mask and flame blits with the cell geometry
// of claim 29, seg13:0x4ba0's weapons, resources and the pod's place and speed, and
// seg13:0x498e's record choice and hunter.
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "world/death.h"
#include "world/flight.h"

using namespace is::world;

namespace {

struct Host : EffectHost {
    std::vector<std::string> log;
    std::int16_t kind = 0;
    void sound(int id, int, int, int) override { log.push_back("sound " + std::to_string(id)); }
    std::int16_t design_kind(int design) override { return design == 5 ? kind : 0; }
    void destroy(int p, int r) override { log.push_back("destroy " + std::to_string(p) + " " + std::to_string(r)); }
    void seg7_2080(int p, int a) override { log.push_back("2080 " + std::to_string(p) + " " + std::to_string(a)); }
};

struct Fixture {
    World w;
    EffectState st;
    EffectWords in;
    Host host;
    is::shell::Rng rng;
    RecordingBlitter out;
    ImageSlotTable images{};
    EffectCtx c{w, st, in, host, rng, &out, &images, nullptr, nullptr, nullptr};
    Fixture() {
        w.size = 1;
        w.last_ship = 2;
        w.player = 0;
        images[kShipImageSlotBase + 1] = 0x77;
    }
};

}  // namespace

TEST_CASE("death: seg13:0x4710 burns the flames into the sheet within the raised silhouette") {
    Fixture f;
    ObjectRecord& o = f.w.pools.rec(kPoolShips, 1);
    o.size = 33;
    o.gap = 5;
    burn_ship_sheet(f.c, 1);
    REQUIRE(f.host.log.size() == 1);
    CHECK(f.host.log[0] == "sound 106");  // 0x6a
    // #230, its mask, the sheet's mask.
    REQUIRE(f.out.images.size() == 3);
    CHECK(f.out.images[0].graphic == kGraphicFlames);
    CHECK(f.out.images[1].name == "mask");
    CHECK(f.out.images[1].graphic == f.out.images[0].id);
    CHECK(f.out.images[2].name == "mask");
    CHECK(f.out.images[2].graphic == 0x77);
    const DcId g = f.out.images[0].id, gm = f.out.images[1].id, sm = f.out.images[2].id;
    REQUIRE(f.out.calls.size() == 1 + 16 * 3);
    // The silhouette and its copy 9 px higher: 16·33 + 17·5 x 33 + 10 [BYTES seg13:0x4870].
    CHECK(f.out.calls[0] == bit_blt_call(sm, 0, -9, 16 * 33 + 17 * 5, 33 + 10, sm, 0, 0, kSrcAnd));
    // p = 32 at Size 1: q = min(33, 32) = 32 and o = max(0, (33 - 32) / 2) = 0 (claim 29).
    for (int h = 0; h < 16; ++h) {
        CAPTURE(h);
        const int x = 38 * h + 5;
        CHECK(f.out.calls[1 + 3 * h] == bit_blt_call(0x77, x, 5, 32, 32, gm, (h % 5) * 32, 0, kSrcAnd));
        CHECK(f.out.calls[2 + 3 * h] == bit_blt_call(0x77, x, 5, 32, 32, g, (h % 5) * 32, 0, kSrcPaint));
        CHECK(f.out.calls[3 + 3 * h] == bit_blt_call(0x77, x, 5, 33, 33, sm, x, 5, kDsna));
    }
    CHECK(f.out.deleted.size() == 3);
    // A larger cell centres the 32-px flame: size 44, o = 6.
    Fixture b;
    b.w.pools.rec(kPoolShips, 1).size = 44;
    b.w.pools.rec(kPoolShips, 1).gap = 4;
    burn_ship_sheet(b.c, 1);
    CHECK(b.out.calls[1] == bit_blt_call(0x77, 4 + 6, 4 + 6, 32, 32, b.out.images[1].id, 0, 0, kSrcAnd));
    // No sheet DC: the sound only.
    Fixture n;
    burn_ship_sheet(n.c, 2);
    CHECK(n.out.calls.empty());
    CHECK(n.host.log.size() == 1);
}

TEST_CASE("death: seg13:0x4ba0 moves the weapons and resources into the pod") {
    Fixture f;
    ShipRecord& s = f.w.ships[1];
    s.slots_cb() = 3;  // slots 1 and 2 in use
    for (int j = 0; j < 10; ++j) {
        s.weapon[static_cast<std::size_t>(j)] = static_cast<std::uint8_t>(20 + j);
        s.weapon_count[static_cast<std::size_t>(j)] = static_cast<std::uint8_t>(j);
    }
    s.set_resources(777);
    ObjectRecord& so = f.w.pools.rec(kPoolShips, 1);
    so.x = 5000;
    so.y = 25500;
    so.extent = 264;
    so.frame = 12;  // (12 + 8) & 15 = 4, the vector (16, 0)
    so.vx = 3;
    so.vy = -4;
    fill_pod(f.c, 1, 2, 1);
    const PodRecord& pd = f.st.pods[2];
    CHECK(pd.kind == 1);
    CHECK(pd.resources == 777);
    CHECK(s.resources() == 0);
    CHECK(s.slots_cb() == 1);
    CHECK(pd.weapon[1] == 21);
    CHECK(pd.count[2] == 2);
    CHECK(pd.weapon[3] == -1);
    CHECK(pd.count[9] == -1);
    for (int j = 1; j < 10; ++j) {
        CHECK(s.weapon[static_cast<std::size_t>(j)] == 0);
        CHECK(s.weapon_count[static_cast<std::size_t>(j)] == 0);
    }
    CHECK(s.weapon[0] == 20);  // slot 0 stays
    CHECK(pd.age.get() == 0x01f40000u);
    CHECK(pd.pool == kPoolShips);
    CHECK(pd.rec == 1);
    const ObjectRecord& po = f.w.pools.rec(11, 2);
    CHECK(po.extent == 0xb8);
    CHECK(po.size == 24);
    CHECK(po.gap == 4);
    CHECK(po.state == 0);
    CHECK(po.mass == 0x65);
    CHECK(po.w4a == 0x1d);
    // x: 132 + 5000 - 92 + 16·264/16 = 5304; y: 132 + 25500 - 92 + 0 = 25540 % 25600.
    CHECK(po.x == 5304);
    CHECK(po.y == 25540);
    CHECK(po.vx == 3 + 5 * 16);
    CHECK(po.vy == -4);
    CHECK(f.host.log.back() == "2080 11 -1");
    // flag 0: kind 3 and at most 400.
    Fixture g;
    g.w.ships[1].set_resources(900);
    fill_pod(g.c, 1, 0, 0);
    CHECK(g.st.pods[0].kind == 3);
    CHECK(g.st.pods[0].resources == 400);
    CHECK(g.w.ships[1].resources() == 500);
    // A Demon's Gate computer ship: kind 2, nothing moves.
    Fixture d;
    d.w.handover.demons_gate = 1;
    d.w.ships[1].set_resources(900);
    fill_pod(d.c, 1, 0, 1);
    CHECK(d.st.pods[0].kind == 2);
    CHECK(d.w.ships[1].resources() == 900);
}

TEST_CASE("death: seg13:0x498e picks the first free pool-11 record and sends a hunter") {
    Fixture f;
    for (int i = 0; i < 10; ++i) f.w.pools.rec(11, i).state = 0;
    f.w.pools.rec(11, 3).state = 1;
    f.w.pools.rec(11, 5).state = 1;
    f.w.ships[2].bec = 1;
    f.w.ships[2].design_index = 5;
    f.host.kind = 3;
    drop_pod(f.c, 1, 1);
    CHECK(f.w.pools.rec(11, 3).state == 0);
    CHECK(f.w.pools.rec(11, 5).state == 1);
    const ShipRecord& h = f.w.ships[2];
    CHECK(h.w32 == 11);
    CHECK(h.w06 == 11);
    CHECK(h.w34 == 3);
    CHECK(h.w08 == 3);
    CHECK(h.w38 == 2);
    CHECK(h.w3c == 2);
    // None free: the oldest (largest DGROUP:0x8564 word) is destroyed and refilled.
    Fixture g;
    for (int i = 0; i < 10; ++i) g.w.pools.rec(11, i).state = 0;
    g.st.pods[4].age.lo = 7;
    g.st.pods[6].age.lo = 9;
    drop_pod(g.c, 1, 0);
    REQUIRE(g.host.log.size() == 3);
    CHECK(g.host.log[0] == "destroy 11 6");
    CHECK(g.host.log[2] == "2080 11 -1");
    CHECK(g.st.pods[6].age.get() == 0x01f40000u);
    // A kind-7 ship needs byte +0x5e > 1.
    Fixture k;
    k.w.pools.rec(11, 0).state = 1;
    k.w.ships[2].bec = 1;
    k.w.ships[2].design_index = 5;
    k.host.kind = 7;
    k.w.ships[2].w5e = 1;
    drop_pod(k.c, 1, 1);
    CHECK(k.w.ships[2].w32 == 0);
    k.w.pools.rec(11, 1).state = 1;
    k.w.ships[2].w5e = 2;
    drop_pod(k.c, 1, 1);
    CHECK(k.w.ships[2].w32 == 11);
    CHECK(k.w.ships[2].w34 == 1);
}

TEST_CASE("death: seg13:0x100c steps the tumble's heading with the <= 0 -> 15 rule") {
    ShipRecord s{};
    ObjectRecord o{};
    o.angle = 1;
    CHECK(turn_step(s, o, 0) == 1);
    CHECK(turn_step(s, o, 14) == 15);
    CHECK(turn_step(s, o, 15) == 0);
    o.angle = -1;
    CHECK(turn_step(s, o, 1) == 15);  // the test on the sum: never 0 (verified claim 31)
    CHECK(turn_step(s, o, 5) == 4);
    s.type = 3;
    CHECK(turn_step(s, o, 5) == 5);
    s.type = 7;
    CHECK(turn_step(s, o, 5) == 4);
    s.beb = 1;
    CHECK(turn_step(s, o, 5) == 5);
    ShipRecord t{};
    t.lockout.set(1000);
    CHECK_FALSE(lockout_passed(t, 1000));
    CHECK(lockout_passed(t, 1001));
}
