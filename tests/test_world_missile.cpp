// The launched weapons (src/world/missile.h): weapon 8, RADAR-GUIDED MISSILES, from the Z key's
// weapon system seg11:0xac86 through the launcher seg11:0x7c12 to the pool-6 mover seg12:0x0
// with the retarget seg12:0x586, the steer seg12:0x7c0, the turn seg13:0xd6c and the clamp
// seg8:0x1cbe (work/analysis/m4/s2-missile.md with s2-missile-verify.md). The launch reproduces
// core-0002's watched words: ammunition 20 -> 19, DGROUP:0x9412 0 -> 1, DGROUP:0x9432 + 3, the
// sound 0x1c [ORACLE core-0002].
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "doctest.h"
#include "hud/weapons.h"
#include "shell/rng.h"
#include "world/missile.h"
#include "world/player_sheet.h"
#include "world/rock_images.h"
#include "world/update.h"

using namespace is::world;

namespace {

struct Sound {
    int id, pool, rec, mode;
};

class Host : public MissileHost {
public:
    World* w = nullptr;
    const WeaponWords* words = nullptr;
    std::vector<Sound> sounds;
    int paints = 0;
    std::vector<std::pair<int, int>> captures, destroyed, on_destroys;
    std::vector<const char*> unported_calls;
    void sound(int id, int pool, int rec, int mode) override { sounds.push_back({id, pool, rec, mode}); }
    void capture_count(int ship, int v) override { captures.push_back({ship, v}); }
    void paint_weapons() override { ++paints; }
    // seg12:0x274a's parts the mover depends on: `state` 1 and the pool-6 entry words.
    void destroy(int pool, int rec) override {
        destroyed.push_back({pool, rec});
        w->pools.rec(pool, rec).state = 1;
        if (pool == 6) missile_destroyed(w->pools, rec, *words);
    }
    void on_destroy(int pool, int rec) override { on_destroys.push_back({pool, rec}); }
    void unported(const char* a, int, int) override { unported_calls.push_back(a); }
};

class Frame : public UpdateHost {
public:
    struct Draw {
        std::int16_t pr;
        int kind, slot, masked;
    };
    std::vector<Draw> drawn;
    void draw_sprite(std::int16_t pr, int kind, int slot, int masked) override {
        drawn.push_back({pr, kind, slot, masked});
    }
};

// The oracle's state at the first Z (core-0002): the player, ship 0, with weapon 8 in slot 1
// (20 missiles), pool 6 as the placement left it (one record, `state` 1, entry age 0x191).
struct Rig {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    Frame uh;
    is::shell::Rng rng;
    JoyState joy;
    JoyButtons jb;
    UpdateCtx u{FlightCtx{w.pools, w.ships, g, keys, uh, rng}, f, view, uh, joy, jb};
    MissileState st;
    Host h;
    MissileCtx c{w, g, st, h, rng};

    Rig() {
        const is::hud::WeaponTable t(0);
        for (int k = 0; k < kWeaponTableBlocks; ++k) st.words.block(k) = t.block(k);
        h.w = &w;
        h.words = &st.words;
        g.now = 100000;
        g.player = 0;
        g.ship_pool = kPoolShips;
        g.size = 1;
        g.game_frames = 500;
        g.demo = 1;
        w.player = 0;
        view.play_w = 1024;
        view.play_h = 683;
        view.shift = 3;
        for (int p = 0; p < kPoolCount; ++p) w.pools.set_in_use(p, 0);
        w.pools.set_in_use(kPoolShips, 1);
        w.pools.set_allocated(6, 1);
        ObjectRecord& r0 = w.pools.rec(6, 0);
        r0.state = 1;
        r0.mass = 20;
        r0.hit_sound = 6;
        r0.owner_pool = -1;
        w.pools.missile(0).age = 0x191;
        ShipRecord& s = w.ships[0];
        s.roster_index = -1;
        s.weapon_slot = 1;
        s.slots_cb() = 2;
        set_byte_at(s, 0xb8 + 1, kWeaponRadarMissiles);
        set_byte_at(s, 0xae + 1, 20);
        ObjectRecord& o = ship();
        o.x = 12800;
        o.y = 12800;
        o.extent = 264;
        o.size = 33;
        o.frame = 0;
        o.vx = 30;
        o.vy = 0;
        o.mass = 160;
        o.owner_pool = -1;
    }
    ObjectRecord& ship() { return w.pools.rec(kPoolShips, 0); }
    ObjectRecord& missile(int m = 0) { return w.pools.rec(6, m); }
    int count() { return sbyte_at(w.ships[0], 0xae + 1); }
};

}  // namespace

TEST_CASE("missile: weapon 8's table words [BYTES seg10:0x22b4-0x236b]") {
    // Claims 76 and 77 (s2-missile.md 0.2), read through the wave's copy of the HUD's table.
    Rig r;
    const WeaponWords& W = r.st.words;
    const int w = kWeaponRadarMissiles;
    CHECK(W.word(w, wofs::kName) == 194);
    CHECK(W.word(w, wofs::kLaunch) == 1);
    CHECK(W.word(w, wofs::kAnim) == 2);
    CHECK(W.word(w, wofs::kMaxDamage) == 1);
    CHECK(W.word(w, wofs::kKind) == 3);
    CHECK(W.word(w, wofs::kThrust) == 10);
    CHECK(W.word(w, wofs::kClamp) == 120);
    CHECK(W.word(w, wofs::kMass) == 20);
    CHECK(W.word(w, wofs::kSpin) == 0);
    CHECK(W.word(w, wofs::kW14) == 0);
    CHECK(W.word(w, wofs::kSound) == 0x1c);
    CHECK(W.word(w, wofs::kRecolourA) == 3);
    CHECK(W.word(w, wofs::kRecolourB) == 2);
    CHECK(W.word(w, wofs::kLifetime) == 200);
    CHECK(W.word(w, wofs::kFuse) == 40);
    CHECK(W.word(w, wofs::kBlast) == 9);
    CHECK(W.word(w, wofs::kEnd) == 1);
    CHECK(W.word(w, wofs::kFrames) == 0);
    CHECK(W.word(w, wofs::kFuel) == 0);
    CHECK(W.word(w, wofs::kW38) == 0);
    CHECK(W.word(w, wofs::kOffscreen) == 1);
    CHECK(W.word(w, wofs::kRetarget) == 1);
    CHECK(W.word(w, wofs::kCloak) == 0);
    CHECK(W.word(w, wofs::kDrag) == 0);
    CHECK(W.word(w, wofs::kAggression) == 0);
    CHECK(W.word(w, wofs::kCell) == 23);
    CHECK(W.word(w, wofs::kGap) == 1);
    CHECK(W.word(w, wofs::kSlot) == -1);
    CHECK(W.word(w, wofs::kGraphic) == 370);
    // Out of the table: 0 [DESIGN].
    CHECK(W.word(-1, wofs::kLifetime) == 0);
}

TEST_CASE("missile: the first Z launches from the oracle's state [ORACLE core-0002]") {
    Rig r;
    weapon_system(r.c);
    // The watched words: the count 20 -> 19, DGROUP:0x9412 0 -> 1, DGROUP:0x9432 + 3.
    CHECK(r.count() == 19);
    CHECK(r.w.pools.in_use(6) == 1);
    CHECK(r.w.w9432 == 3);
    CHECK(r.w.w8e28 == 100000u);
    REQUIRE(r.h.captures.size() == 1);
    CHECK(r.h.captures[0] == std::make_pair(0, 4));  // seg13:0x4eec(player, 4)
    CHECK(r.h.paints == 1);
    // seg10:0x744c(0x69) then seg10:0x7944(0x1c, 6, 0, 1) [BYTES seg11:0x89d6-0x8a02].
    REQUIRE(r.h.sounds.size() == 2);
    CHECK(r.h.sounds[0].id == 0x69);
    CHECK(r.h.sounds[1].id == 0x1c);
    CHECK(r.h.sounds[1].pool == 6);
    CHECK(r.h.sounds[1].rec == 0);
    CHECK(r.h.sounds[1].mode == 1);
    // The record: the cell 23 rounds up to 24 with gap 4 at Size 1 [BYTES seg11:0x7f78-0x7fe7].
    const ObjectRecord& m = r.missile();
    CHECK(m.state == 0);
    CHECK(m.size == 24);
    CHECK(m.gap == 4);
    CHECK(m.src_y == 4);
    CHECK(m.extent == 184);
    CHECK(m.mass == 20);
    CHECK(m.max_damage == 1);
    CHECK(m.blast_size == 9);
    CHECK(m.damage == 0);
    CHECK(m.frame == 0);
    CHECK(m.hit_sound == 6);  // record 0 keeps the wave template's CLUNK (s2-missile.md 3.2)
    CHECK(m.on_destroy.get() == kDestroyGeneric);
    CHECK(m.scr_x == -1000);
    CHECK(m.old_scr_y == -1000);
    // The front launch at heading 0, h = (0, -16), E = 264, e = 184: x + 40, y + 40 + (224 *
    // -16) / 15 = y - 198; v = 4 h + the ship's (30, 0) [BYTES seg11:0x86d4-0x87df seg11:0x8987].
    CHECK(m.x == 12840);
    CHECK(m.y == 12602);
    CHECK(m.vx == 30);
    CHECK(m.vy == -64);
    // The entry [BYTES seg11:0x7eb1-0x7ec4 seg11:0x7f57 seg11:0x89cb seg11:0x89d2].
    const MissileSlot& e = r.w.pools.missile(0);
    CHECK(e.weapon == 8);
    CHECK(e.age == 0);
    CHECK(e.mode == 2);
    CHECK(e.target_pool == -1);
    CHECK(e.lock == 0);
    CHECK(e.launcher_pool == kPoolShips);
    CHECK(e.launcher_rec == 0);
    // The gun's last-shot frame: frame# + 36 [BYTES seg11:0x87e3-0x8803].
    CHECK(ship_dword(r.w.ships[0], 0xa0) == 536u);
    CHECK(r.w.pools.bad_index_count == 0);
}

TEST_CASE("missile: DGROUP:0x9432 += 3 needs W+0x46 = 0 and ship word +0x32 != 9 (claim 7 corrected)") {
    Rig r;
    r.w.ships[0].w32 = 9;
    weapon_system(r.c);
    CHECK(r.w.w9432 == 0);
    CHECK(r.count() == 19);  // the launch itself goes on
    CHECK(r.h.captures.size() == 1);
}

TEST_CASE("missile: no use for ship byte +0xea / +0xe8 or the object's byte +0x2e [BYTES seg11:0xac97-0xacb5]") {
    Rig r;
    r.w.ships[0].be8 = 1;
    weapon_system(r.c);
    r.w.ships[0].be8 = 0;
    r.ship().b2e = 1;
    weapon_system(r.c);
    CHECK(r.count() == 20);
    CHECK(r.w.w9432 == 0);
    CHECK(r.h.captures.empty());
}

TEST_CASE("missile: the refusal while the launcher's missile is 1 ... 14 passes old [BYTES seg11:0x7e20-0x7e5d]") {
    Rig r;
    weapon_system(r.c);
    REQUIRE(r.count() == 19);
    r.w.pools.missile(0).age = 5;
    weapon_system(r.c);
    // DGROUP:0x9432 and seg13:0x4eec run on every Z; the count stays, no repaint.
    CHECK(r.w.w9432 == 6);
    CHECK(r.h.captures.size() == 2);
    CHECK(r.count() == 19);
    CHECK(r.h.paints == 1);
    // The walk grew record 1 (in_use 2) and the refusal skipped seg7:0x2080, which would have
    // counted it out (`state` 1) [BYTES seg11:0x7d41 seg11:0x8a35].
    CHECK(r.w.pools.allocated(6) == 2);
    CHECK(r.w.pools.in_use(6) == 2);
    CHECK(r.missile(1).state == 1);
    CHECK(r.missile(1).hit_sound == 100);  // the grown template [BYTES seg11:0x7c90]
    CHECK(r.missile(1).on_damage.get() == kDamageGeneric);
    CHECK(r.w.pools.missile(1).weapon == -1);
    // At age 15 the next Z launches into record 1.
    r.w.pools.missile(0).age = 15;
    weapon_system(r.c);
    CHECK(r.count() == 18);
    CHECK(r.w.pools.missile(1).weapon == 8);
    CHECK(r.w.pools.in_use(6) == 2);
}

TEST_CASE("missile: a same-frame second launch frees the age-0 missile and reuses it [BYTES seg11:0x7d6d-0x7d8b]") {
    Rig r;
    weapon_system(r.c);
    weapon_system(r.c);  // no mover pass between: entry 0's age is 0
    CHECK(r.count() == 18);
    CHECK(r.w.pools.allocated(6) == 1);
    CHECK(r.w.pools.missile(0).weapon == 8);
    CHECK(r.h.paints == 2);
}

TEST_CASE("missile: the cap DGROUP:0x9412 >= 10 and an empty magazine [BYTES seg11:0x7c2b seg11:0x8a0f]") {
    Rig r;
    r.w.pools.set_in_use(6, 10);
    weapon_system(r.c);
    CHECK(r.count() == 20);
    CHECK(r.w.pools.in_use(6) == 10);  // returned before seg7:0x2080
    CHECK(r.w.w9432 == 3);
    Rig e;
    set_byte_at(e.w.ships[0], 0xae + 1, 0);
    weapon_system(e.c);
    CHECK(e.count() == 0);
    CHECK(e.h.paints == 0);
    CHECK(e.h.sounds.empty());
    CHECK(e.w.pools.in_use(6) == 0);  // seg7:0x2080 ran
    CHECK(e.w.w9432 == 3);
    // Weapon 0 (slot 0) reaches the switch and returns (claim 9).
    Rig z;
    z.w.ships[0].weapon_slot = 0;
    weapon_system(z.c);
    CHECK(z.w.w9432 == 3);
    CHECK(z.h.unported_calls.empty());
}

TEST_CASE("missile: seg7:0x2080 recounts the in-use records [BYTES seg7:0x2080-0x2127]") {
    Pools p;
    p.resize(6, 3);
    p.rec(6, 0).state = 1;
    p.rec(6, 1).state = 0;
    p.rec(6, 2).state = 1;
    recount_in_use(p, 6);
    CHECK(p.in_use(6) == 2);
    p.rec(6, 1).state = 1;
    recount_in_use(p, 6);
    CHECK(p.in_use(6) == 0);
    p.rec(6, 2).race_status = 1;
    recount_in_use(p, 6);
    CHECK(p.in_use(6) == 3);
    // Pool 3 is not one of seg7:0x2080's pools.
    p.set_in_use(3, 7);
    recount_in_use(p, 3);
    CHECK(p.in_use(3) == 7);
}

TEST_CASE("missile: seg8:0x1cbe clamps each axis [BYTES seg8:0x1ccc-0x1d46]") {
    Pools p;
    p.rec(6, 0).vx = 200;
    p.rec(6, 0).vy = -300;
    clamp_velocity(p, 120, pool_rec(6, 0));
    CHECK(p.rec(6, 0).vx == 120);
    CHECK(p.rec(6, 0).vy == -120);
    p.rec(6, 0).vx = 50;
    clamp_velocity(p, 120, pool_rec(6, 0));
    CHECK(p.rec(6, 0).vx == 50);
}

TEST_CASE("missile: seg13:0xd6c turns one step the short way [BYTES seg13:0xf87-0xffd]") {
    ShipTable ships{};
    int touched = 0;
    const auto touch = [&touched](int) { ++touched; };
    // A target along heading k from the missile's point: the 16-sector direction is k.
    const auto at = [](int k, int cur) {
        TurnArgs a;
        a.cur = static_cast<std::int16_t>(cur);
        a.mx = 8000;
        a.my = 8000;
        a.tx = static_cast<std::int16_t>(8000 + heading_dx(k) * 400);
        a.ty = static_cast<std::int16_t>(8000 + heading_dy(k) * 400);
        return a;
    };
    for (int k = 0; k < 16; ++k) {
        TurnArgs a = at(k, 0);
        a.ship = -100;  // a direction, no turn
        CHECK(turn_toward(a, 0, ships, touch) == k);
    }
    CHECK(turn_toward(at(4, 0), 0, ships, touch) == 1);
    // A gap of exactly 8 steps up (h - cur = 8 < 9) [BYTES seg13:0xfe6 seg13:0xfe9 seg13:0xfeb];
    // s2-missile.md 6.3's "turn(0, 8) = 15" contradicts the listing and claim 63.
    CHECK(turn_toward(at(8, 0), 0, ships, touch) == 1);
    CHECK(turn_toward(at(9, 0), 0, ships, touch) == 15);   // h - cur = 9: down, -1 -> 15
    CHECK(turn_toward(at(0, 15), 0, ships, touch) == 0);   // cur - h = 15: up, 16 -> 0
    CHECK(turn_toward(at(7, 15), 0, ships, touch) == 14);  // cur - h = 8: down
    CHECK(turn_toward(at(3, 3), 0, ships, touch) == 3);    // h = cur
    CHECK(touched == 0);  // ship -1: seg13:0x6716's write is skipped [DESIGN]
    // A ship turns through seg13:0x6716.
    TurnArgs s = at(4, 0);
    s.ship = 2;
    CHECK(turn_toward(s, 0, ships, touch) == 1);
    CHECK(touched == 1);
    ships[2].beb = 1;
    CHECK(turn_toward(s, 0, ships, touch) == 0);
}

namespace {

// A launched missile at (8000, 8000), at rest.
void launched(Rig& r) {
    weapon_system(r.c);
    ObjectRecord& m = r.missile();
    m.x = 8000;
    m.y = 8000;
    m.vx = 0;
    m.vy = 0;
}

ObjectRecord& add_rock(Rig& r, std::int16_t x, std::int16_t y) {
    r.w.pools.set_in_use(kPoolRocks, 1);
    ObjectRecord& k = r.w.pools.rec(kPoolRocks, 0);
    k.state = 0;
    k.mass = 1400;
    k.size = 44;
    k.extent = 352;
    k.x = x;
    k.y = y;
    k.owner_pool = -1;
    return k;
}

}  // namespace

TEST_CASE("missile: the retarget's 16-bit scoring locks the own ship after age 60 [BYTES seg12:0x6bb-0x7b1] [ORACLE core-0002]") {
    // The own ship at 600 cells: 600 << 2 = 2400, lo16(7 x 2400) = 16800, lo16(2 x 16800) =
    // -31936, / 3 = -10645: below every rock's distance (s2-missile.md 4.2 "16-bit wrap").
    Rig r;
    launched(r);
    add_rock(r, 8320, 8000);  // 40 cells
    r.ship().x = 12800;
    r.ship().y = 8000;
    MissileSlot& e = r.w.pools.missile(0);
    e.age = 52;
    retarget(r.c, shl16(e.age, 3), 0);
    // Age <= 60: the best is the launcher, so nothing is adopted, not even the rock.
    CHECK(e.lock == 0);
    CHECK(e.target_pool == -1);
    e.age = 65;
    retarget(r.c, shl16(e.age, 3), 0);
    CHECK(e.lock == 1);
    CHECK(e.target_pool == kPoolShips);
    CHECK(e.target_rec == 0);
    // At 300 cells the own ship's score is +5600, beyond R = 520: the rock wins.
    r.ship().x = 10400;
    retarget(r.c, shl16(e.age, 3), 0);
    CHECK(e.target_pool == kPoolRocks);
    CHECK(e.target_rec == 0);
    // Nothing within R: the target and the lock stay (claim 56).
    r.w.pools.rec(kPoolRocks, 0).x = 20000;
    r.ship().x = 8000 + 8 * 60;  // 60 cells: 60 * 4 * 7 * 2 / 3 = 1120 > 520
    retarget(r.c, shl16(e.age, 3), 0);
    CHECK(e.target_pool == kPoolRocks);
    CHECK(e.lock == 1);
}

TEST_CASE("missile: the steer draws two rand() and turns one heading step [BYTES seg12:0x7c0-0xa23]") {
    Rig r;
    launched(r);
    MissileSlot& e = r.w.pools.missile(0);
    // A rock straight to the right of the missile's centre (8092, 8092): h = 4.
    add_rock(r, 16000, 8092 - 22);
    e.target_pool = kPoolRocks;
    e.target_rec = 0;
    e.lock = 1;
    is::shell::Rng ref = r.rng;
    ref.rand();
    ref.rand();
    steer(r.c, 0);
    CHECK(r.rng.state() == ref.state());
    const ObjectRecord& m = r.missile();
    CHECK(m.frame == 1);
    // Thrust 10 h(1) = (60, -150), the clamp at 120 per axis.
    CHECK(m.vx == 60);
    CHECK(m.vy == -120);
    CHECK(r.h.destroyed.empty());
    // Within the fuse's 40 cells: blast_size doubled, seg12:0x274a, and the thrust still runs.
    r.w.pools.rec(kPoolRocks, 0).x = 8092 + 8 * 20 - 22;
    steer(r.c, 0);
    REQUIRE(r.h.destroyed.size() == 1);
    CHECK(r.missile().blast_size == 18);
    CHECK(r.missile().state == 1);
    CHECK(r.w.pools.missile(0).weapon == -1);
    CHECK(r.w.pools.in_use(6) == 0);  // seg7:0x2080(6, m)
}

TEST_CASE("missile: an unlocked missile ends after 105 passes, a locked one after 200 [BYTES seg12:0xe8-0x10b seg12:0x445-0x462]") {
    {
        Rig r;
        launched(r);
        r.w.pools.set_in_use(kPoolShips, 0);  // no candidate at all
        int n = 0;
        while (r.missile().state == 0 && n < 400) {
            move_pool6(r.c, r.u);
            ++n;
        }
        CHECK(n == 105);
        REQUIRE(r.h.destroyed.size() == 1);
        CHECK(r.h.destroyed[0] == std::make_pair(6, 0));
        CHECK(r.w.pools.missile(0).weapon == -1);
        CHECK(r.w.pools.missile(0).age == 200);
        CHECK(r.w.pools.in_use(6) == 0);
        // Every pass drew the sheet seg8:0xb76(0x600 | m, 3, DGROUP:0x39e8[W-4], 0).
        REQUIRE(!r.uh.drawn.empty());
        CHECK(r.uh.drawn[0].pr == 0x600);
        CHECK(r.uh.drawn[0].kind == 3);
        CHECK(r.uh.drawn[0].slot == -1);  // W(8)-4 before seg10:0x533a
        CHECK(r.uh.drawn[0].masked == 0);
    }
    {
        // Locked on a far rock, the fuse word cleared so that it never detonates; off screen it
        // lives its full time (W(8)+0x3a = 1).
        Rig r;
        launched(r);
        r.w.pools.set_in_use(kPoolShips, 0);
        r.st.words.set_word(kWeaponRadarMissiles, wofs::kFuse, 0);
        add_rock(r, 20000, 20000);
        MissileSlot& e = r.w.pools.missile(0);
        e.target_pool = kPoolRocks;
        e.target_rec = 0;
        e.lock = 1;
        int n = 0;
        while (r.missile().state == 0 && n < 400) {
            move_pool6(r.c, r.u);
            ++n;
        }
        CHECK(n == 200);
        CHECK(r.h.destroyed.size() == 1);
    }
}

TEST_CASE("missile: the mover's W-2 = -1 branch writes the screen point and draws nothing (claim 45 corrected)") {
    Rig r;
    launched(r);
    r.st.words.set_word(kWeaponRadarMissiles, wofs::kGraphic, -1);
    move_pool6(r.c, r.u);
    CHECK(r.uh.drawn.empty());
    CHECK(r.missile().scr_x == 8000);  // the default to_screen returns the point
    CHECK(r.missile().scr_y == 8000);
}

TEST_CASE("missile: seg7:0x2652, the generic on_damage [BYTES seg7:0x2652-0x26b8]") {
    Rig r;
    launched(r);
    CHECK(generic_on_damage(r.w.pools, 6, 0, 1, r.h) == 0);  // damage 1, not above max_damage 1
    CHECK(r.missile().damage == 1);
    CHECK(generic_on_damage(r.w.pools, 6, 0, 1, r.h) == 1);
    REQUIRE(r.h.on_destroys.size() == 1);
    CHECK(r.h.on_destroys[0] == std::make_pair(6, 0));
    CHECK(generic_on_damage(r.w.pools, 6, 0, 5, r.h) == 0);  // already above: nothing added
    CHECK(r.missile().damage == 2);
    CHECK(generic_on_damage(r.w.pools, 15, 0, 5, r.h) == 0);
}

TEST_CASE("missile: seg10:0x533a picks weapon 8's image slot [BYTES seg10:0x533a-0x5464]") {
    Rig r;
    ImageSlotTable images{};
    WeaponSlotChoice c = weapon_image_slot(r.st.words, images, r.w.pools, kWeaponRadarMissiles);
    CHECK(c.slot == 0x30);
    CHECK(c.load);
    CHECK(r.st.words.word(kWeaponRadarMissiles, wofs::kSlot) == 0x30);
    CHECK(r.missile().size == 23);  // raw W-8 and W-6 into pool-6 record 0
    CHECK(r.missile().gap == 1);
    images[0x30] = 7;
    // Weapon 7 (kind 4, graphic 370 too) shares the slot; nothing else is written.
    r.missile().size = 0;
    c = weapon_image_slot(r.st.words, images, r.w.pools, 7);
    CHECK(c.slot == 0x30);
    CHECK_FALSE(c.load);
    CHECK(r.st.words.word(7, wofs::kSlot) == 0x30);
    CHECK(r.missile().size == 0);
    // Weapon 9 (graphic 150) takes the next empty slot.
    c = weapon_image_slot(r.st.words, images, r.w.pools, 9);
    CHECK(c.slot == 0x31);
    CHECK(c.load);
    // Weapons 22 and 27 take none.
    CHECK(weapon_image_slot(r.st.words, images, r.w.pools, 22).slot == -1);
    CHECK(weapon_image_slot(r.st.words, images, r.w.pools, 27).slot == -1);
}

TEST_CASE("missile: seg14:0x11a8's tail loads IGRAPHIC 370 for weapon 8 [BYTES seg14:0x13f3-0x1425 seg10:0x53b3-0x540c]") {
    Rig r;
    RecordingBlitter out;
    ImageSlotTable images{};
    std::vector<SheetRequest> asked;
    const SheetRenderer render = [&asked](const SheetRequest& q, SheetPixels& px) {
        asked.push_back(q);
        px.w = 16 * 23 + 17 * 5;
        px.h = 23 + 2 * 5;
        px.index.assign(static_cast<std::size_t>(px.w * px.h), 0);
        px.source_height = 23;
        return true;
    };
    const int n = build_weapon_sheets(out, images, r.w.pools, r.st.words, r.w.ships[0],
                                      weapon_sheet_sel(0, 1, 500), 1, render);
    CHECK(n == 1);
    REQUIRE(asked.size() == 1);
    CHECK(asked[0].slot == 0x30);
    CHECK(asked[0].graphic_id == 370);
    CHECK(asked[0].w1 == 2);
    CHECK(asked[0].gap_word == 0x63);
    CHECK(asked[0].recolour_a == 3);
    CHECK(asked[0].recolour_b == 2);
    CHECK(asked[0].colour_a == 12);
    CHECK(asked[0].colour_b == 12);
    CHECK(asked[0].mode == 0);
    CHECK(asked[0].sel == 0);
    CHECK(images[0x30] != 0);
    // Built once: the next build reuses the slot.
    CHECK(build_weapon_sheets(out, images, r.w.pools, r.st.words, r.w.ships[0], 0, 1, render) == 0);
    CHECK(weapon_sheet_sel(2, 0, 0) == 1);
    CHECK(weapon_sheet_sel(1, 1, 1) == 1);
    CHECK(weapon_sheet_sel(1, 1, 0) == 0);
}
