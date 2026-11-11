// The tethers (src/world/tether.h): GRAPPLING HOOK and HANDCUFFS, seg11:0x947a (the fire and the
// release), seg12:0x3498 (the hook meets an object), seg12:0x3148 (the pool-10 mover), seg8:0x1e32
// (the held object's pull, swing, rope and breaks), seg13:0x6e5-0xa57 (the throw) and the hit
// dispatcher's tethered target (work/analysis/m4/s7-tether.md with s7-tether-verify.md). Every
// expected value is worked by hand from the listings; core-0009's numbers are the costs 25 and 20
// and the sounds 0x1d, 0x06 and 0x14 [ORACLE core-0009] (M4 s7 TETHER).
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "hud/weapons.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/line.h"
#include "world/tether.h"
#include "world/update.h"
#include "world/visibility.h"

using namespace is::world;

namespace {

struct Sound {
    int id, pool, rec, mode;
    bool operator==(const Sound& o) const { return id == o.id && pool == o.pool && rec == o.rec && mode == o.mode; }
};
struct Aim {
    int P, R, TP, TR, S, shift;
};

// to_screen: (x >> 3) - 1500, (y >> 3) - 1500, so the world point (12800, 12800) is (100, 100).
Point16 screen_of(std::int16_t x, std::int16_t y) {
    return Point16{static_cast<std::int16_t>(sar16(x, 3) - 1500), static_cast<std::int16_t>(sar16(y, 3) - 1500)};
}

class Host : public TetherHost {
public:
    const is::hud::WeaponTable table{0};
    std::vector<Sound> sounds;
    std::vector<int> commands;
    int sprites = 0;
    RecordingBlitter out;
    std::int16_t team_v = 0;
    std::array<std::int16_t, 2> muzzle_v{{0, 0}};
    std::vector<std::array<int, 3>> muzzles;  // (n, f, xy)
    std::vector<Aim> aims;
    bool aim_store = true;
    bool real_aim = false;  // the default seg8:0x348e (missile.h aim_at) with this host's to_screen
    std::int16_t aim_vx = 0, aim_vy = 0;

    void sound(int id, int pool, int rec, int mode) override { sounds.push_back({id, pool, rec, mode}); }
    void command(int id) override { commands.push_back(id); }
    Point16 to_screen(std::int16_t x, std::int16_t y) override { return screen_of(x, y); }
    Blitter* blitter() override { return &out; }
    void player_sprite() override { ++sprites; }
    std::int16_t weapon_word(int w, int off) override { return table.word(w, off); }
    std::int16_t team(const World&, int) override { return team_v; }
    std::int16_t muzzle(int n, int f, int xy) override {
        muzzles.push_back({n, f, xy});
        return muzzle_v[static_cast<std::size_t>(xy)];
    }
    bool aim(World& w, int P, int R, int TP, int TR, std::int16_t S, int shift) override {
        aims.push_back({P, R, TP, TR, S, shift});
        if (real_aim) return TetherHost::aim(w, P, R, TP, TR, S, shift);
        if (!aim_store) return false;
        obj(w, P, R).vx = aim_vx;
        obj(w, P, R).vy = aim_vy;
        return true;
    }
};

// The player (ship 0) on screen at (12800, 12800) with a GRAPPLING HOOK in slot 1 and 100 fuel;
// pool 10 as the load leaves it (three records, `state` 1, the template's size and gap at Size
// 1); every slot without ends; nothing else in play.
struct Rig {
    World w;
    WaveGlobals g;
    Host h;
    TetherState st;
    TetherCtx c{w, g, h, 640, 400, st};

    Rig() {
        g.now = 100000;
        g.player = 0;
        g.ship_pool = kPoolShips;
        g.size = 1;
        g.game_frames = 1;
        g.play_mode = 0;
        w.roles.radar_first = 0;
        for (int p = 0; p < kStartupPools; ++p) {
            w.pools.set_in_use(p, 0);
            for (int i = 0; i < w.pools.records(p); ++i) {
                w.pools.rec(p, i).owner_pool = -1;  // seg9:0x6624 [BYTES seg9:0x68df seg9:0x68e9]
                w.pools.rec(p, i).w36 = -1;
            }
        }
        w.pools.resize(kPoolHooks, 3);
        for (int k = 0; k < 3; ++k) {
            ObjectRecord& r = w.pools.rec(kPoolHooks, k);
            r = ObjectRecord{};
            r.size = 12, r.gap = 6, r.extent = 0x60, r.mass = 11, r.state = 1, r.hit_sound = 6;
            r.max_damage = -1, r.owner_pool = -1, r.w36 = -1;
            w.links[static_cast<std::size_t>(k)].pool_a = -1;
            w.links[static_cast<std::size_t>(k)].pool_b = -1;
        }
        w.pools.set_in_use(kPoolShips, 1);
        ShipRecord& s = w.ships[0];
        s.design_index = 0;
        s.slots_cb() = 2;
        set_byte_at(s, 0xb8 + 1, kWeaponHook);
        s.fuel = 100;
        s.bec = 1;
        ObjectRecord& o = ship();
        o.on_screen = 1, o.scr_x = 84, o.scr_y = 84, o.size = 33, o.extent = 264, o.mass = 200;
        o.x = 12800, o.y = 12800, o.vx = 10, o.vy = 0, o.frame = 4;
    }
    ObjectRecord& ship(int i = 0) { return w.pools.rec(kPoolShips, i); }
    ObjectRecord& hook(int k = 0) { return w.pools.rec(kPoolHooks, k); }
    ObjectRecord& rock(int i = 0) { return w.pools.rec(kPoolRocks, i); }
    LinkSlot& link(int k = 0) { return w.links[static_cast<std::size_t>(k)]; }
    // A rock in play at (x, y).
    ObjectRecord& add_rock(int i, std::int16_t x, std::int16_t y, std::int16_t mass) {
        if (w.pools.in_use(kPoolRocks) <= i) w.pools.set_in_use(kPoolRocks, static_cast<std::int16_t>(i + 1));
        ObjectRecord& r = rock(i);
        r.state = 0, r.x = x, r.y = y, r.mass = mass, r.owner_pool = -1, r.w36 = -1;
        return r;
    }
    // Object (pool, rec) held by the player's slot k, as seg12:0x3498's attach leaves it.
    void hold(int pool, int rec, int k, int weapon) {
        ObjectRecord& t = w.pools.rec(pool, rec);
        t.owner_pool = kPoolShips, t.owner_rec = 0;
        set_held_slot(t, k);
        ship().w36 = static_cast<std::int16_t>(pool), ship().w38 = static_cast<std::int16_t>(rec);
        set_own_slot(ship(), k);
        LinkSlot& l = link(k);
        l.pool_a = static_cast<std::int16_t>(pool), l.rec_a = static_cast<std::int16_t>(rec);
        l.pool_b = kPoolShips, l.rec_b = 0;
        l.weapon = static_cast<std::int16_t>(weapon);
    }
    std::vector<BlitCall> lines() const {
        std::vector<BlitCall> v;
        for (const BlitCall& b : h.out.calls)
            if (b.op == BlitCall::kMoveTo || b.op == BlitCall::kLineTo) v.push_back(b);
        return v;
    }
};

}  // namespace

TEST_CASE("tether: the weapon records W(2) and W(15) [BYTES seg10:0x213f-0x21ef]") {
    const is::hud::WeaponTable t(0);
    CHECK(t.word(kWeaponHook, 0x00) == 188);   // name
    CHECK(t.word(kWeaponHook, 0x02) == 190);   // picture
    CHECK(t.word(kWeaponHook, 0x0a) == 0);     // the switch's weapons
    CHECK(t.word(kWeaponHook, 0x2e) == 25);    // the fuel per Z [ORACLE core-0009]
    CHECK(t.word(kWeaponHook, 0x34) == 1000);  // price
    CHECK(t.word(kWeaponHook, 0x46) == 1);
    CHECK(t.word(kWeaponCuffs, 0x00) == 201);
    CHECK(t.word(kWeaponCuffs, 0x0a) == 0);
    CHECK(t.word(kWeaponCuffs, 0x2e) == 20);   // [ORACLE core-0009]
    CHECK(t.word(kWeaponCuffs, 0x34) == 750);
    CHECK(t.word(kWeaponCuffs, 0x46) == 0);
}

TEST_CASE("seg11:0x947a: the gates and the cost come first [BYTES seg11:0x9497-0x9515]") {
    SUBCASE("off screen: nothing, not even the cost") {
        Rig r;
        r.ship().on_screen = 0;
        tether_use(r.c, kPoolShips, 0, kWeaponHook);
        CHECK(r.w.ships[0].fuel == 100);
        CHECK(r.h.sounds.empty());
    }
    SUBCASE("more than the fuel refuses") {
        Rig r;
        r.w.ships[0].fuel = 24;
        tether_use(r.c, kPoolShips, 0, kWeaponHook);
        CHECK(r.w.ships[0].fuel == 24);
        CHECK(r.h.sounds.empty());
        CHECK(r.ship().w36 == -1);
    }
    SUBCASE("no hook in slots 1 ...: the cost is paid and nothing happens (slot 0 is not read)") {
        Rig r;
        r.w.ships[0].slots_cb() = 1;
        set_byte_at(r.w.ships[0], 0xb8, kWeaponHook);
        tether_use(r.c, kPoolShips, 0, kWeaponHook);
        CHECK(r.w.ships[0].fuel == 75);
        CHECK(r.h.sounds.empty());
        CHECK(r.ship().w36 == -1);
    }
    SUBCASE("either weapon in a slot passes; HANDCUFFS cost 20 [ORACLE core-0009]") {
        Rig r;
        set_byte_at(r.w.ships[0], 0xb8 + 1, kWeaponCuffs);
        tether_use(r.c, kPoolShips, 0, kWeaponCuffs);
        CHECK(r.w.ships[0].fuel == 80);
        CHECK(r.ship().w36 == kPoolHooks);
    }
}

TEST_CASE("seg11:0x947a: the straight shot with nothing in range [BYTES seg11:0x9530-0x95a8 seg11:0x9a6a-0x9c2d]") {
    Rig r;
    r.h.muzzle_v = {{6, -9}};
    tether_use(r.c, kPoolShips, 0, kWeaponHook);
    CHECK(r.w.ships[0].fuel == 75);                                       // [ORACLE core-0009]
    REQUIRE(r.h.sounds.size() == 1);
    CHECK(r.h.sounds[0] == Sound{0x1d, kPoolShips, 0, 1});                // SLING at the user
    const LinkSlot& l = r.link(0);
    CHECK(l.weapon == kWeaponHook);
    CHECK(l.pool_a == kPoolHooks);
    CHECK(l.rec_a == 0);
    CHECK(l.pool_b == kPoolShips);
    CHECK(l.rec_b == 0);
    CHECK(l.w08 == 0);
    CHECK(r.ship().w36 == kPoolHooks);
    CHECK(r.ship().w38 == 0);
    CHECK(own_slot(r.ship()) == 0);
    const ObjectRecord& hk = r.hook(0);
    CHECK(hk.state == 0);
    CHECK(hk.scr_x == -1000);
    CHECK(hk.scr_y == -1000);
    CHECK(hk.owner_pool == kPoolShips);
    CHECK(hk.owner_rec == 0);
    CHECK(hk.frame == 4);
    CHECK(hk.vx == 7 * heading_dx(4) + 10);
    CHECK(hk.vy == 7 * heading_dy(4));
    // The ship muzzle (lo16(m·4) / 3) << 3 of entry (n 0, f = the user's frame 4) [BYTES seg11:0x9b1c-0x9b53]
    CHECK(hk.x == 12800 + 64);
    CHECK(hk.y == 12800 - 96);
    REQUIRE(r.h.muzzles.size() == 2);
    CHECK(r.h.muzzles[0] == std::array<int, 3>{0, 4, 0});
    CHECK(r.w.pools.in_use(kPoolHooks) == 1);  // seg7:0x2080(10, -1)
    CHECK(r.h.aims.empty());
}

TEST_CASE("seg11:0x947a: the release on the second Z, silent and paid [BYTES seg11:0x9c30-0x9d9c]") {
    Rig r;
    tether_use(r.c, kPoolShips, 0, kWeaponHook);
    r.hook(0).scr_x = 50, r.hook(0).scr_y = 60;
    r.link(0).line = {1, 2, 3, 4, 5, 6, 7, 8};
    r.h.sounds.clear();
    r.h.out.calls.clear();
    tether_use(r.c, kPoolShips, 0, kWeaponHook);
    CHECK(r.w.ships[0].fuel == 50);  // the release costs the fire's 25 [ORACLE core-0009]
    CHECK(r.h.sounds.empty());
    CHECK(r.ship().w36 == -1);
    CHECK(r.hook(0).owner_pool == -1);
    CHECK(r.hook(0).state == 1);
    CHECK(r.link(0).pool_a == -1);
    CHECK(r.link(0).pool_b == -1);
    // BLACK_PEN, seg12:0x7256(+0x1c, +0x1e, +0x14, +0x16), the pen back, PatBlt BLACKNESS
    const std::vector<BlitCall>& b = r.h.out.calls;
    REQUIRE(b.size() == 5);
    CHECK(b[0] == select_pen_call(kDcWindow, kPenBlack));
    CHECK(b[1] == move_to_call(kDcWindow, 7, 8));
    CHECK(b[2] == line_to_call(kDcWindow, 3, 4));
    CHECK(b[3].op == BlitCall::kSelectPen);
    CHECK(b[4] == pat_blt_call(kDcWindow, 50, 60, 12, 12, kBlackness));
    CHECK(r.h.sprites == 1);  // the player while DGROUP:0x951a = 0
    CHECK(r.w.pools.in_use(kPoolHooks) == 0);
}

TEST_CASE("seg11:0x947a: releasing a held ship sets its tumble [BYTES seg11:0x9c43-0x9c8c]") {
    Rig r;
    r.w.pools.set_in_use(kPoolShips, 2);
    r.ship(1).state = 0;
    r.hold(kPoolShips, 1, 1, kWeaponCuffs);
    tether_use(r.c, kPoolShips, 0, kWeaponCuffs);
    CHECK(r.w.ships[0].fuel == 80);
    CHECK(r.w.ships[1].lockout.get() == r.g.now + 800u);
    CHECK(r.ship(1).angle == 1);
    CHECK(r.w.ships[1].be2 == 1);
    CHECK(r.ship(1).owner_pool == -1);
    CHECK(r.ship().w36 == -1);
    CHECK(r.hook(1).state == 1);
}

TEST_CASE("seg11:0x947a: the search scores and the aimed shot [BYTES seg11:0x96a7-0x989e seg11:0x9923-0x9a67]") {
    Rig r;
    // A: 100 cells east, mass 100, within one of the user's heading: 500 doubled, (500·1000)/600
    // = 833. B: 200 cells south, mass 800: 1200, (400·1200)/600 = 800. C: fixed, skipped.
    r.add_rock(0, 12800 + 800, 12800, 100);
    r.add_rock(1, 12800, 12800 + 1600, 800);
    r.add_rock(2, 12800 + 80, 12800, 5000).b54 = 1;
    std::int16_t dx = 0, dy = 0;
    distance_cells(12800 + 800, 12800, 12800, 12800, dx, dy);
    r.ship().frame = direction16(dy, dx);
    SUBCASE("the best target and the aim at 110") {
        r.h.aim_vx = 50, r.h.aim_vy = 3;
        tether_use(r.c, kPoolShips, 0, kWeaponHook);
        REQUIRE(r.h.aims.size() == 1);
        CHECK(r.h.aims[0].P == kPoolHooks);
        CHECK(r.h.aims[0].R == 0);
        CHECK(r.h.aims[0].TP == kPoolRocks);
        CHECK(r.h.aims[0].TR == 0);
        CHECK(r.h.aims[0].S == 110);
        CHECK(r.h.aims[0].shift == 3);
        CHECK(r.hook(0).vx == 50);
        CHECK(r.hook(0).frame == direction16(3, 50));  // seg8:0x19b0(num vy, den vx)
        CHECK(r.hook(0).state == 0);
        // The muzzle reads record 0's stale frame (0), not the user's [BYTES seg11:0x9661]
        REQUIRE(!r.h.muzzles.empty());
        CHECK(r.h.muzzles[0][1] == 0);
    }
    SUBCASE("an aim that stores nothing falls back to 7 x the heading (Omission E)") {
        r.h.aim_store = false;
        tether_use(r.c, kPoolShips, 0, kWeaponHook);
        CHECK(r.hook(0).vx == add16(mul16(heading_dx(r.ship().frame), 7), 10));
        CHECK(r.hook(0).vy == mul16(heading_dy(r.ship().frame), 7));
    }
    SUBCASE("Demon's Gate shoots straight") {
        r.g.demons_gate = 1;
        tether_use(r.c, kPoolShips, 0, kWeaponHook);
        CHECK(r.h.aims.empty());
    }
}

TEST_CASE("seg11:0x947a: a ship target is aimed at 160 [BYTES seg11:0x9923-0x9932]") {
    Rig r;
    r.w.pools.set_in_use(kPoolShips, 2);
    ObjectRecord& s1 = r.ship(1);
    s1.state = 0, s1.x = 12800 + 800, s1.y = 12800, s1.mass = 300, s1.owner_pool = -1, s1.w36 = -1;
    tether_use(r.c, kPoolShips, 0, kWeaponHook);
    REQUIRE(r.h.aims.size() == 1);
    CHECK(r.h.aims[0].TP == kPoolShips);
    CHECK(r.h.aims[0].TR == 1);
    CHECK(r.h.aims[0].S == 160);
}

TEST_CASE("seg11:0x947a: an aim across x distance 0 stores nothing; the hook falls back to 7 x the heading (ruling R1, Omission E) [BYTES seg8:0x358e seg11:0x994d-0x99d1]") {
    // The rock 100 cells straight below the ship, extent 0: the hook's corner (100, 100) and the
    // rock's (100, 200) give ex = (100 << 3) + 0 - (100 << 3) = 0, a divide fault in the
    // original's 32-bit `idiv`. The port's seg8:0x348e stores nothing [DESIGN ADR 0016], so the
    // hook still moves with the ship and gets 7·heading + the ship's velocity (10, 0).
    Rig r;
    r.h.real_aim = true;
    r.add_rock(0, 12800, 12800 + 800, 500);
    tether_use(r.c, kPoolShips, 0, kWeaponHook);
    REQUIRE(r.h.aims.size() == 1);
    CHECK(r.h.aims[0].TP == kPoolRocks);
    CHECK(r.h.aims[0].TR == 0);
    CHECK(r.h.aims[0].S == 110);
    CHECK(r.hook(0).vx == add16(mul16(heading_dx(4), 7), 10));
    CHECK(r.hook(0).vy == mul16(heading_dy(4), 7));
    CHECK(r.hook(0).frame == direction16(r.hook(0).vy, r.hook(0).vx));
    CHECK(r.hook(0).state == 0);
    CHECK(r.link(0).pool_a == kPoolHooks);
    CHECK(r.ship().w36 == kPoolHooks);
    SUBCASE("one pixel to the side the aim stores, vx = S·n / ex (verified claim 19)") {
        // ex = 8, ey = 800, n = seg8:0x35d0(8, 800): vx = lo16(110·n / 8), vy = 800·110 / n, on
        // top of the rock's velocity (0, 0); the fallback is not taken.
        Rig q;
        q.h.real_aim = true;
        q.add_rock(0, 12800 + 8, 12800 + 800, 500);
        tether_use(q.c, kPoolShips, 0, kWeaponHook);
        const std::int16_t n = magnitude(8, 800);
        CHECK(q.hook(0).vx == lo16((110 * static_cast<std::int32_t>(n)) / 8));
        CHECK(q.hook(0).vy == lo16((800 * 110) / static_cast<std::int32_t>(n)));
    }
}

TEST_CASE("seg11:0x947a: an Enforcer's cuffs take ship words +0x54/+0x56 (ruling R4) [BYTES seg11:0x95ab-0x95d9]") {
    Rig r;
    r.h.team_v = 7;
    set_byte_at(r.w.ships[0], 0xb8 + 1, kWeaponCuffs);
    r.add_rock(0, 12800 + 800, 12800, 100);  // would win a search
    r.w.ships[0].w54 = 9, r.w.ships[0].w56 = 2;
    tether_use(r.c, kPoolShips, 0, kWeaponCuffs);
    REQUIRE(r.h.aims.size() == 1);
    CHECK(r.h.aims[0].TP == 9);
    CHECK(r.h.aims[0].TR == 2);
    CHECK(r.link(0).weapon == kWeaponCuffs);
    SUBCASE("no arrest target: the straight shot") {
        Rig q;
        q.h.team_v = 7;
        set_byte_at(q.w.ships[0], 0xb8 + 1, kWeaponCuffs);
        q.add_rock(0, 12800 + 800, 12800, 100);
        q.w.ships[0].w54 = -1;
        tether_use(q.c, kPoolShips, 0, kWeaponCuffs);
        CHECK(q.h.aims.empty());
        CHECK(q.ship().w36 == kPoolHooks);
    }
    SUBCASE("the hook of an Enforcer searches") {
        Rig q;
        q.h.team_v = 7;
        q.add_rock(0, 12800 + 800, 12800, 100);
        q.w.ships[0].w54 = 9;
        tether_use(q.c, kPoolShips, 0, kWeaponHook);
        REQUIRE(q.h.aims.size() == 1);
        CHECK(q.h.aims[0].TP == kPoolRocks);
    }
}

TEST_CASE("seg11:0x947a: no free slot, no fire; a non-ship user needs no slot or fuel [BYTES seg11:0x9530-0x956d]") {
    Rig r;
    for (int k = 0; k < 3; ++k) r.link(k).pool_a = 3;
    tether_use(r.c, kPoolShips, 0, kWeaponHook);
    CHECK(r.w.ships[0].fuel == 75);
    CHECK(r.h.sounds.empty());
    Rig q;
    ObjectRecord& hz = q.w.pools.rec(8, 0);
    hz.on_screen = 1, hz.x = 1000, hz.y = 1000, hz.extent = 64, hz.frame = 0, hz.w36 = -1;
    tether_use(q.c, 8, 0, kWeaponHook);
    CHECK(q.link(0).pool_b == 8);
    // The non-ship muzzle: lo16((extent / 2)·DGROUP:0x1050[0]) / 16 + (DGROUP:0x10d0[0] << 3)
    // [BYTES seg11:0x9b55-0x9bd3] (Omission D)
    CHECK(q.hook(0).x == add16(1000, add16(idiv16(mul16(32, heading_dx(0)), 16), -14 * 8)));
    CHECK(q.hook(0).y == add16(1000, add16(idiv16(mul16(32, heading_dy(0)), 16), -23 * 8)));
}

TEST_CASE("seg12:0x3498: the hook meets its holder, another hook, a fixed object [BYTES seg12:0x34a7-0x35c1 seg12:0x3728-0x376f]") {
    Rig r;
    tether_use(r.c, kPoolShips, 0, kWeaponHook);
    r.hook(0).scr_x = 30, r.hook(0).scr_y = 40;
    r.h.out.calls.clear();
    r.h.sounds.clear();
    CHECK(hook_meets(r.c, kPoolShips, 0, 0) == 0);
    CHECK(hook_meets(r.c, kPoolHooks, 1, 0) == 0);
    CHECK(r.h.out.calls.empty());
    r.add_rock(0, 13000, 12800, 100).b54 = 1;
    r.link(0).w08 = 7;
    CHECK(hook_meets(r.c, kPoolRocks, 0, 0) == 1);
    CHECK(r.h.sounds.empty());  // the miss is silent
    CHECK(r.ship().w36 == -1);
    CHECK(r.link(0).pool_a == -1);
    CHECK(r.link(0).pool_b == -1);
    CHECK(r.link(0).w08 == 0);
    CHECK(r.hook(0).state == 1);
    CHECK(r.w.pools.in_use(kPoolHooks) == 0);
    REQUIRE(!r.h.out.calls.empty());
    CHECK(r.h.out.calls.back() == pat_blt_call(kDcWindow, 30, 40, 12, 12, kBlackness));
}

TEST_CASE("seg12:0x3498: the attach, CLUNK at the hook [BYTES seg12:0x35c4-0x3722]") {
    Rig r;
    tether_use(r.c, kPoolShips, 0, kWeaponHook);
    r.h.sounds.clear();
    ObjectRecord& t = r.add_rock(0, 13000, 12800, 100);
    CHECK(hook_meets(r.c, kPoolRocks, 0, 0) == 1);
    REQUIRE(r.h.sounds.size() == 1);
    CHECK(r.h.sounds[0] == Sound{6, kPoolHooks, 0, 1});  // CLUNK [ORACLE core-0009]
    CHECK(t.owner_pool == kPoolShips);
    CHECK(t.owner_rec == 0);
    CHECK(held_slot(t) == 0);
    CHECK(r.ship().w36 == kPoolRocks);
    CHECK(r.ship().w38 == 0);
    CHECK(r.link(0).pool_a == kPoolRocks);
    CHECK(r.link(0).rec_a == 0);
    CHECK(r.hook(0).state == 1);
    CHECK(r.w.pools.in_use(kPoolHooks) == 1);  // no seg7:0x2080 at the attach
    SUBCASE("a fuel tank attaches though `fixed`") {
        Rig q;
        tether_use(q.c, kPoolShips, 0, kWeaponHook);
        ObjectRecord& tank = q.w.pools.rec(4, 0);
        tank.b54 = 1, tank.owner_pool = -1;
        CHECK(hook_meets(q.c, 4, 0, 0) == 1);
        CHECK(tank.owner_pool == kPoolShips);
        CHECK(tank.b54 == 0);
    }
}

TEST_CASE("seg12:0x3498: the steal frees the old tether [BYTES seg12:0x35f8-0x36a4]") {
    Rig r;
    r.w.pools.set_in_use(kPoolShips, 2);
    ObjectRecord& t = r.add_rock(0, 13000, 12800, 100);
    // Ship 1 holds the rock with slot 2.
    t.owner_pool = kPoolShips, t.owner_rec = 1;
    set_held_slot(t, 2);
    r.ship(1).w36 = kPoolRocks, r.ship(1).w38 = 0;
    r.link(2).pool_a = kPoolRocks, r.link(2).pool_b = kPoolShips, r.link(2).rec_b = 1, r.link(2).w08 = 5;
    r.hook(2).state = 0;
    tether_use(r.c, kPoolShips, 0, kWeaponHook);  // slot 0
    CHECK(hook_meets(r.c, kPoolRocks, 0, 0) == 1);
    CHECK(r.ship(1).w36 == -1);
    CHECK(r.link(2).pool_a == -1);
    CHECK(r.link(2).pool_b == -1);
    CHECK(r.link(2).w08 == 0);
    CHECK(r.hook(2).state == 1);
    CHECK(t.owner_rec == 0);
    CHECK(held_slot(t) == 0);
}

TEST_CASE("seg12:0x3498: a ship's hook takes only its goal ship; a missile's age is quartered [BYTES seg12:0x355d-0x3599]") {
    Rig r;
    r.w.pools.set_in_use(kPoolShips, 2);
    r.ship(1).state = 0;
    tether_use(r.c, kPoolShips, 0, kWeaponHook);
    r.w.ships[0].w32 = 3;  // the goal is not ship 1
    CHECK(hook_meets(r.c, kPoolShips, 1, 0) == 1);
    CHECK(r.ship(1).owner_pool == -1);  // the miss
    CHECK(r.ship().w36 == -1);
    Rig q;
    tether_use(q.c, kPoolShips, 0, kWeaponHook);
    q.w.pools.missile(1).age = 23;
    ObjectRecord& m = q.w.pools.rec(6, 1);
    m.owner_pool = -1;
    CHECK(hook_meets(q.c, 6, 1, 0) == 1);
    CHECK(q.w.pools.missile(1).age == 5);
    CHECK(m.owner_pool == kPoolShips);
}

TEST_CASE("seg8:0x1e32: the hook swings a rock, pulls a capturable [BYTES seg8:0x23b4-0x24c8]") {
    // Holder: the player at (12800, 12800), extent 0; the rock 100 cells east, at rest: o1 = -100,
    // o2 = 0, d = 100. Swing 0: vx = -180·0 / 100 + (-100 << 3) / 100 = -8, vy = 180·-100 / 100 = -180.
    Rig r;
    r.ship().extent = 0;
    ObjectRecord& t = r.add_rock(0, 12800 + 800, 12800, 100);
    r.hold(kPoolRocks, 0, 0, kWeaponHook);
    r.st.pens = {{0x501, 0x502, 0x503, 0x504}};
    tether_follow(r.c, kPoolRocks, 0);
    CHECK(t.vx == -8);
    CHECK(t.vy == -180);
    CHECK(t.x == 12800 + 800 - 8);
    CHECK(t.y == 12800 - 180);
    CHECK(r.st.pen_phase == 1);
    CHECK(r.h.sprites == 1);
    // The erase of the last line, then the new one from the holder (100 + 16, 100 + 16) to the
    // rock's point + (v sar 3): (199 - 1, 77 - 23) in pen DGROUP:0x3b46
    const std::vector<BlitCall> lines = r.lines();
    REQUIRE(lines.size() == 4);
    CHECK(lines[2] == move_to_call(kDcWindow, 116, 116));
    CHECK(lines[3] == line_to_call(kDcWindow, 198, 54));
    bool pen_used = false;
    for (const BlitCall& b : r.h.out.calls) pen_used = pen_used || b == select_pen_call(kDcWindow, 0x502);
    CHECK(pen_used);
    CHECK(r.link(0).line[2] == 198);  // rolled
    CHECK(r.link(0).line[6] == 116);
    SUBCASE("swing 1 reverses the circle") {
        Rig q;
        q.ship().extent = 0;
        ObjectRecord& u = q.add_rock(0, 12800 + 800, 12800, 100);
        q.hold(kPoolRocks, 0, 0, kWeaponHook);
        q.link(0).swing = 1;
        tether_follow(q.c, kPoolRocks, 0);
        CHECK(u.vx == -8);
        CHECK(u.vy == 180);
    }
    SUBCASE("a capturable object is pulled: v += o sar 2") {
        Rig q;
        q.ship().extent = 0;
        ObjectRecord& u = q.add_rock(0, 12800 + 800, 12800, 100);
        u.b53 = 1;
        u.vx = 3;
        q.hold(kPoolRocks, 0, 0, kWeaponHook);
        tether_follow(q.c, kPoolRocks, 0);
        CHECK(u.vx == 3 - 25);
        CHECK(u.vy == 0);
    }
    SUBCASE("Bright draws in WHITE_PEN") {
        Rig q;
        q.w.bright = 1;
        q.ship().extent = 0;
        q.add_rock(0, 12800 + 800, 12800, 100);
        q.hold(kPoolRocks, 0, 0, kWeaponHook);
        tether_follow(q.c, kPoolRocks, 0);
        bool white = false;
        for (const BlitCall& b : q.h.out.calls) white = white || b == select_pen_call(kDcWindow, kPenWhite);
        CHECK(white);
    }
}

TEST_CASE("seg8:0x1e32: the pen phase runs 0 ... 3 [BYTES seg8:0x1e41-0x1e4c]") {
    Rig r;
    r.ship().extent = 0;
    r.add_rock(0, 12800 + 800, 12800, 100);
    r.hold(kPoolRocks, 0, 0, kWeaponHook);
    std::vector<int> phases;
    for (int i = 0; i < 5; ++i) {
        tether_follow(r.c, kPoolRocks, 0);
        phases.push_back(r.st.pen_phase);
    }
    CHECK(phases == std::vector<int>{1, 2, 3, 0, 1});
}

TEST_CASE("seg8:0x1e32: the HANDCUFFS rope beyond 300 cells [BYTES seg8:0x20d8-0x2393]") {
    // The rock 320 cells east of the holder, both at rest: v = newton_root(0, 0) = 1, e = (-320, 0)
    // px, dot 0, X = 20·50 = 1000: aT = 1000 / 100 = 10, T.vx += 10·-320 / 320 = -10; aH = 1000 / 200
    // = 5, H.vx -= 5·-320 / 320.
    Rig r;
    r.ship().extent = 0;
    r.ship().vx = 0;
    ObjectRecord& t = r.add_rock(0, 12800 + 2560, 12800, 100);
    r.hold(kPoolRocks, 0, 0, kWeaponCuffs);
    tether_follow(r.c, kPoolRocks, 0);
    CHECK(t.vx == -10);
    CHECK(t.vy == 0);
    CHECK(r.ship().vx == 5);
    CHECK(r.st.impulse_t == 10);
    CHECK(r.st.impulse_h == 5);
    CHECK(t.x == 12800 + 2560 - 10);
    SUBCASE("the next call carries a third of the impulses (Omission L)") {
        // dvx = -15, v = newton_root(225, 15) = 15, dot = 4800 >= 0: aT = 10, aH = 5;
        // T.vx += (10 / 3 + 10)·-320 / 320 = -13; H.vx -= (5 / 3 + 5)·-320 / 320 = -6.
        tether_follow(r.c, kPoolRocks, 0);
        CHECK(t.vx == -23);
        CHECK(r.ship().vx == 11);
    }
    SUBCASE("the object moving away: the inelastic stop by the mass ratio (Omission K)") {
        // T.v = (20, 0): v = 20, dot = -6400, c = 640000 / 6400 = 100, r = 2000 / 100 = 20;
        // aT = 2000 / (10000 / 200 + 100) = 13, aH = 7; + 10 and + 5.
        Rig q;
        q.ship().extent = 0;
        q.ship().vx = 0;
        ObjectRecord& u = q.add_rock(0, 12800 + 2560 + 20, 12800, 100);
        u.vx = 20;
        q.hold(kPoolRocks, 0, 0, kWeaponCuffs);
        tether_follow(q.c, kPoolRocks, 0);
        CHECK(u.vx == 20 - 23);
        CHECK(q.ship().vx == 12);
        CHECK(q.st.impulse_t == 23);
        CHECK(q.st.impulse_h == 12);
    }
    SUBCASE("slack up to 300 cells") {
        Rig q;
        q.ship().extent = 0;
        ObjectRecord& u = q.add_rock(0, 12800 + 2400, 12800, 100);
        q.hold(kPoolRocks, 0, 0, kWeaponCuffs);
        tether_follow(q.c, kPoolRocks, 0);
        CHECK(u.vx == 0);
        CHECK(q.ship().vx == 10);
    }
}

TEST_CASE("seg8:0x1e32: a zero mass skips the cuffs' divisions that fault in the original (ruling R1's rule) [BYTES seg8:0x22ca seg8:0x22dd seg8:0x22fb seg8:0x2346]") {
    // The Omission K rig above (T moving away at 20, r = 20, X / 200 = 5, X / 100 = 10), with one
    // mass 0. The original faults; the port skips the division and what depends on it
    // [DESIGN ADR 0016].
    SUBCASE("T's mass 0: aT = (100·r) / (0 / 200 + 100) = r, aH = 0; no X / mT") {
        Rig q;
        q.ship().extent = 0;
        q.ship().vx = 0;
        ObjectRecord& u = q.add_rock(0, 12800 + 2560 + 20, 12800, 0);
        u.vx = 20;
        q.hold(kPoolRocks, 0, 0, kWeaponCuffs);
        tether_follow(q.c, kPoolRocks, 0);
        CHECK(q.st.impulse_t == 20);
        CHECK(q.st.impulse_h == 5);
        CHECK(u.vx == 20 - 20);
        CHECK(q.ship().vx == 5);
    }
    SUBCASE("H's mass 0: no aT / aH (seg8:0x22ca), aT = X / 100, no X / mH") {
        Rig q;
        q.ship().extent = 0;
        q.ship().vx = 0;
        q.ship().mass = 0;
        ObjectRecord& u = q.add_rock(0, 12800 + 2560 + 20, 12800, 100);
        u.vx = 20;
        q.hold(kPoolRocks, 0, 0, kWeaponCuffs);
        tether_follow(q.c, kPoolRocks, 0);
        CHECK(q.st.impulse_t == 10);
        CHECK(q.st.impulse_h == 0);
        CHECK(u.vx == 20 - 10);
        CHECK(q.ship().vx == 0);
    }
}

TEST_CASE("seg8:0x1e32: the breaks, STATIC at the object, the forces still applied [BYTES seg8:0x1fda-0x20cd]") {
    SUBCASE("HANDCUFFS beyond 350 cells") {
        Rig r;
        r.ship().extent = 0;
        r.ship().vx = 0;
        ObjectRecord& t = r.add_rock(0, 12800 + 3200, 12800, 100);
        r.hold(kPoolRocks, 0, 0, kWeaponCuffs);
        r.w.pools.set_in_use(kPoolHooks, 1);
        tether_follow(r.c, kPoolRocks, 0);
        CHECK(t.owner_pool == -1);
        CHECK(r.ship().w36 == -1);
        CHECK(r.link(0).pool_a == -1);
        CHECK(r.link(0).pool_b == -1);
        CHECK(r.h.commands == std::vector<int>{0x69});
        REQUIRE(r.h.sounds.size() == 1);
        CHECK(r.h.sounds[0] == Sound{0x14, kPoolRocks, 0, 1});  // [ORACLE core-0009]
        CHECK(r.w.pools.in_use(kPoolHooks) == 0);
        // d = 400 still drives the rope: X = 60·50 = 3000, aT = 30, T.vx = 30·-400 / 400
        CHECK(t.vx == -30);
        CHECK(r.ship().vx == 15);
        // Both erases (the drawn line and the raw step-4 points), then the last line's erase, no
        // new line
        CHECK(r.lines().size() == 6);
    }
    SUBCASE("the hook at 450 holds, at 451 breaks") {
        Rig r;
        r.ship().extent = 0;
        r.add_rock(0, 12800 + 3600, 12800, 100);
        r.hold(kPoolRocks, 0, 0, kWeaponHook);
        tether_follow(r.c, kPoolRocks, 0);
        CHECK(r.rock().owner_pool == kPoolShips);
        Rig q;
        q.ship().extent = 0;
        q.add_rock(0, 12800 + 3608, 12800, 100);
        q.hold(kPoolRocks, 0, 0, kWeaponHook);
        tether_follow(q.c, kPoolRocks, 0);
        CHECK(q.rock().owner_pool == -1);
    }
    SUBCASE("the holding cost: 3 fuel every max(mass / -200 + 8, 3) frames; empty breaks") {
        Rig r;
        r.ship().extent = 0;
        r.add_rock(0, 12800 + 800, 12800, 100);
        r.hold(kPoolRocks, 0, 0, kWeaponHook);
        r.g.game_frames = 16;  // q = 8
        r.w.ships[0].fuel = 10;
        tether_follow(r.c, kPoolRocks, 0);
        CHECK(r.w.ships[0].fuel == 7);
        CHECK(r.rock().owner_pool == kPoolShips);
        r.g.game_frames = 17;
        tether_follow(r.c, kPoolRocks, 0);
        CHECK(r.w.ships[0].fuel == 7);
        r.g.game_frames = 24;
        r.w.ships[0].fuel = 3;
        tether_follow(r.c, kPoolRocks, 0);
        CHECK(r.w.ships[0].fuel == 0);
        CHECK(r.rock().owner_pool == -1);  // d = 1000
    }
    SUBCASE("the ambulance always breaks") {
        Rig r;
        r.ship().extent = 0;
        ObjectRecord& amb = r.w.pools.rec(7, 0);
        amb.x = 12800 + 80, amb.y = 12800, amb.owner_pool = -1;
        r.hold(7, 0, 0, kWeaponHook);
        tether_follow(r.c, 7, 0);
        CHECK(amb.owner_pool == -1);
    }
}

namespace {

class Frame : public UpdateHost {
public:
    struct Draw {
        std::int16_t pr;
        int kind, slot, masked;
    };
    std::vector<Draw> drawn;
    std::vector<Sound> sounds;
    RecordingBlitter out;
    std::uint16_t hit = 0xffffu;
    int sprites = 0;
    void draw_sprite(std::int16_t pr, int kind, int slot, int masked) override {
        drawn.push_back({pr, kind, slot, masked});
    }
    std::uint16_t collide(std::int16_t) override { return hit; }
    Blitter* blitter() override { return &out; }
    void player_sprite() override { ++sprites; }
    void sound(int id, int pool, int rec, int mode) override { sounds.push_back({id, pool, rec, mode}); }
};

struct FrameRig {
    Rig r;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    Frame uh;
    is::shell::Rng rng;
    JoyState joy;
    JoyButtons jb;
    UpdateCtx u{FlightCtx{r.w.pools, r.w.ships, r.g, keys, uh, rng}, f, view, uh, joy, jb};

    FrameRig() {
        view.play_w = 640;
        view.play_h = 400;
        view.shift = 3;
        rng.srand(1234);
        tether_state() = TetherState{};
        // The hook in flight from the player, 100 cells east after this pass's move
        r.ship().on_screen = 1, r.ship().scr_x = 100, r.ship().scr_y = 100;
        r.w.pools.set_in_use(kPoolHooks, 1);
        ObjectRecord& h = r.hook(0);
        h.state = 0, h.on_screen = 1, h.scr_x = 200, h.scr_y = 100;
        h.x = 12800 + 800 - 50, h.y = 12800, h.vx = 50, h.vy = 0;
        h.owner_pool = kPoolShips, h.owner_rec = 0;
        r.ship().w36 = kPoolHooks, r.ship().w38 = 0;
        LinkSlot& l = r.link(0);
        l.pool_a = kPoolHooks, l.rec_a = 0, l.pool_b = kPoolShips, l.rec_b = 0, l.weapon = kWeaponHook;
    }
};

}  // namespace

TEST_CASE("seg12:0x3148: one pass of a hook in flight [BYTES seg12:0x3379-0x3376]") {
    FrameRig fr;
    is::shell::Rng expect = fr.rng;
    const int r1 = expect.rand() % 128, r2 = expect.rand() % 128;
    move_hooks(fr.u, fr.r.w);
    const ObjectRecord& h = fr.r.hook(0);
    CHECK(h.x == 12800 + 800);  // seg8:0x1b54(0x0a00, 1)
    CHECK(h.frame == direction16(0, 0));  // the last pass's line (all 0 at the first pass, Omission I)
    CHECK(h.src_x == add16(mul16(18, h.frame), 6));
    REQUIRE(fr.uh.drawn.size() == 1);
    CHECK(fr.uh.drawn[0].pr == 0x0a00);
    CHECK(fr.uh.drawn[0].kind == 3);
    CHECK(fr.uh.drawn[0].slot == kHookImageSlot);
    // Two rand() for the pen RGB(r2, 0, 128 + r1)
    CHECK(fr.rng.rand() == expect.rand());
    const std::vector<BlitCall>& b = fr.uh.out.calls;
    REQUIRE(b.size() == 10);
    CHECK(b[0] == select_pen_call(kDcWindow, kPenBlack));
    CHECK(b[1] == move_to_call(kDcWindow, 0, 0));
    CHECK(b[2] == line_to_call(kDcWindow, 0, 0));
    CHECK(b[4] == create_pen_call(kPsSolid, 1, (static_cast<std::uint32_t>(128 + r1) << 16) | static_cast<std::uint32_t>(r2), 0x300));
    CHECK(b[5] == select_pen_call(kDcWindow, 0x300));
    CHECK(b[6] == move_to_call(kDcWindow, 116, 116));  // the holder's point + size / 2
    CHECK(b[7] == line_to_call(kDcWindow, 206, 106));  // the hook's
    CHECK(b[9] == delete_pen_call(0x300));
    const LinkSlot& l = fr.r.link(0);
    CHECK(l.w08 == 1);
    CHECK(l.line[2] == 206);  // rolled
    CHECK(l.line[6] == 116);
    CHECK(fr.f.dg_943a == kPoolHooks);
    CHECK(fr.f.dg_943c == 0);
    CHECK(fr.uh.sprites == 1);  // the player's hook
}

TEST_CASE("seg12:0x3148: the miss after 42 passes, sooner when far [BYTES seg12:0x3337-0x334f seg12:0x340c-0x345c]") {
    SUBCASE("the 42nd pass") {
        FrameRig fr;
        fr.r.link(0).w08 = 41;
        move_hooks(fr.u, fr.r.w);
        CHECK(fr.r.link(0).pool_a == -1);
        CHECK(fr.r.ship().w36 == -1);
        CHECK(fr.r.hook(0).state == 1);
        CHECK(fr.r.w.pools.in_use(kPoolHooks) == 0);
        CHECK(fr.uh.out.calls.back() == pat_blt_call(kDcWindow, 200, 100, 12, 12, kBlackness));
        CHECK(fr.uh.sprites == 0);  // the slot has no holder any more
    }
    SUBCASE("the 41st does not") {
        FrameRig fr;
        fr.r.link(0).w08 = 40;
        move_hooks(fr.u, fr.r.w);
        CHECK(fr.r.link(0).pool_a == kPoolHooks);
        CHECK(fr.r.link(0).w08 == 41);
    }
    SUBCASE("beyond (H << 3) / 2 cells: + 50") {
        FrameRig fr;
        fr.view.play_h = 20;  // 80 < 100
        move_hooks(fr.u, fr.r.w);
        CHECK(fr.r.link(0).pool_a == -1);
        CHECK(fr.r.w.pools.in_use(kPoolHooks) == 0);
    }
}

TEST_CASE("seg12:0x3148: the contact attaches through seg12:0x3498 [BYTES seg12:0x32f1-0x3334]") {
    FrameRig fr;
    ObjectRecord& t = fr.r.add_rock(0, 13700, 12800, 100);
    fr.uh.hit = static_cast<std::uint16_t>(pool_rec(kPoolRocks, 0));
    move_hooks(fr.u, fr.r.w);
    CHECK(t.owner_pool == kPoolShips);
    REQUIRE(fr.uh.sounds.size() == 1);
    CHECK(fr.uh.sounds[0] == Sound{6, kPoolHooks, 0, 1});
    CHECK(fr.r.ship().w36 == kPoolRocks);
    CHECK(fr.r.link(0).w08 == 0);
}

TEST_CASE("seg13:0x6e5-0xa57: the throw [BYTES seg13:0x6e5-0xa54]") {
    // The player in Strategy mode swings the goal rock (moving east at 50); a heavy rock 200 cells
    // east: t = -((0 + 50·-200) / 200) = 50, (50 << 7) / 50 = 128 > 125: the release.
    auto rig = [](Rig& r, std::int16_t vx) {
        r.g.play_mode = 1;
        r.g.detail = 3;
        r.w.ships[0].w38 = 1;
        r.w.ships[0].w32 = kPoolRocks, r.w.ships[0].w34 = 0;
        ObjectRecord& g = r.add_rock(0, 12800 + 800, 12800, 100);
        g.vx = vx;
        r.add_rock(1, 12800 + 800 + 1600, 12800, 600);
        r.hold(kPoolRocks, 0, 0, kWeaponHook);
    };
    VisibilityGlobals vis;
    vis.count = 2;
    vis.pool[0] = kPoolRocks, vis.rec[0] = 0;
    vis.pool[1] = kPoolRocks, vis.rec[1] = 1;
    {
        Rig r;
        rig(r, 50);
        tether_throw(r.c, &vis, 0);
        CHECK(r.rock().owner_pool == -1);
        CHECK(r.ship().w36 == -1);
        CHECK(r.w.ships[0].fuel == 75);
        CHECK(r.rock().vx == 50);  // the throw keeps the velocity
    }
    {
        Rig r;
        rig(r, -50);
        tether_throw(r.c, &vis, 0);
        CHECK(r.rock().owner_pool == kPoolShips);
    }
    {
        Rig r;
        rig(r, 50);
        r.g.play_mode = 0;  // the player in Action mode never throws
        tether_throw(r.c, &vis, 0);
        CHECK(r.rock().owner_pool == kPoolShips);
    }
    {
        Rig r;
        rig(r, 50);
        r.link(0).weapon = kWeaponCuffs;  // only the hook throws
        tether_throw(r.c, &vis, 0);
        CHECK(r.rock().owner_pool == kPoolShips);
    }
}

TEST_CASE("seg12:0x3778's hooks erase in BLACK_PEN and recount [BYTES seg12:0x38a6-0x38f3]") {
    RecordingBlitter out;
    Pools pools;
    pools.resize(kPoolHooks, 3);
    pools.set_in_use(kPoolHooks, 3);
    for (int k = 0; k < 3; ++k) pools.rec(kPoolHooks, k).state = 1;
    TetherLinkHooks h;
    h.out = &out;
    h.pools = &pools;
    h.play_w = 640, h.play_h = 400;
    h.line(kDcWindow, 1, 2, 3, 4);
    h.seg7_2080(kPoolHooks, -1);
    REQUIRE(out.calls.size() == 4);
    CHECK(out.calls[0] == select_pen_call(kDcWindow, kPenBlack));
    CHECK(out.calls[1] == move_to_call(kDcWindow, 1, 2));
    CHECK(out.calls[2] == line_to_call(kDcWindow, 3, 4));
    CHECK(pools.in_use(kPoolHooks) == 0);
}

TEST_CASE("the rope pens DGROUP:0x3b44 ... 0x3b4a [BYTES seg10:0x4d87-0x4df3]") {
    RecordingBlitter out;
    TetherState st;
    make_rope_pens(out, st);
    REQUIRE(out.pens.size() == 4);
    CHECK(out.pens[0].colorref == 0x000000ffu);
    CHECK(out.pens[1].colorref == 0x007f007fu);
    CHECK(out.pens[2].colorref == 0x007f00ffu);
    CHECK(out.pens[3].colorref == 0x00ff00ffu);
    CHECK(st.pens[0] == out.pens[0].id);
    make_rope_pens(out, st);  // the old ones deleted first
    int deleted = 0;
    for (const BlitCall& b : out.calls) deleted += b.op == BlitCall::kDeletePen ? 1 : 0;
    CHECK(deleted == 4);
}

namespace {

class CollideCalls : public CollideHost {
public:
    std::vector<std::array<int, 3>> releases;
    void seg11_947a(int pool, int rec, int arg) override { releases.push_back({pool, rec, arg}); }
};

}  // namespace

TEST_CASE("seg8:0x5df2: a tethered target toggles its slot's swing and draws one rand() [BYTES seg8:0x5f3e-0x5fe2]") {
    World w;
    WaveGlobals g;
    CollideGlobals cg;
    CollideCalls h;
    ViewState view;
    VisibilityGlobals vis;
    is::shell::Rng rng;
    std::int16_t a = 0, b = 0, d = 0;
    CollideCtx x{w, g, cg, h, view, &vis, rng, a, b, d};
    w.pools.set_in_use(kPoolShips, 1);
    ObjectRecord& s = w.pools.rec(kPoolShips, 0);
    s.mass = 160, s.owner_pool = -1;
    ObjectRecord& t = w.pools.rec(kPoolRocks, 0);
    t.mass = 50, t.max_damage = -1, t.owner_pool = kPoolShips, t.owner_rec = 0;
    set_held_slot(t, 1);
    w.links[1].swing = 0;
    is::shell::Rng expect = rng;
    expect.rand();
    dispatch(x, kPoolRocks, 0, kPoolShips, 0, 4);
    CHECK(w.links[1].swing == 1);  // 160 + 100 > 50
    CHECK(h.releases.empty());     // T is not a ship
    dispatch(x, kPoolRocks, 0, kPoolShips, 0, 4);
    CHECK(w.links[1].swing == 0);
    (void)expect;
}
