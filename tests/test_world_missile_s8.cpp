// M4 stage 8, track TESTS-DOCS: what a launched weapon does when it meets an object, end to end
// (work/analysis/m4/s8-hits.md with s8-hits-verify.md, claims 1-24 as the verifier corrected
// them): the shock blast seg8:0x5ab0 run by the effect frame seg8:0x3fbe after the destroy
// handlers seg12:0x274a and seg7:0x2e02, the dispatcher seg8:0x5df2 and the ship's seg8:0x45e0 it
// reaches, the generic on_damage seg7:0x2652, the kill score, and the DOUGHNUT touch
// seg13:0x16ac that the pool-6 mover seg12:0x0 reaches. The routines are wired as the program
// wires them (src/world/flight_hooks.cpp): the effect host's hit() is the dispatcher, a missile's
// on_destroy is object_destroyed or pickup_destroyed, and the mover's touch is capture.h touch.
// The launches are the weapon system's own (the weapon table of src/hud/weapons.cpp).
#include <cstdint>
#include <utility>
#include <vector>

#include "doctest.h"
#include "hud/weapons.h"
#include "shell/rng.h"
#include "world/capture.h"
#include "world/collide.h"
#include "world/effects.h"
#include "world/missile.h"
#include "world/update.h"
#include "world/visibility.h"

using namespace is::world;

namespace {

struct Snd {
    int id, pool, rec;
};
struct Hit {
    int p, r, sp, sr;
    int amount;
};

struct Scene;

class ColHost final : public CollideHost {
public:
    Scene* s;
    explicit ColHost(Scene* sc) : s(sc) {}
    void sound(int id, int pool, int rec, int mode) override;
    void on_destroy(int pool, int rec) override;
    void destroy(int pool, int rec) override;
};

class EffHost final : public EffectHost {
public:
    Scene* s;
    explicit EffHost(Scene* sc) : s(sc) {}
    void sound(int id, int pool, int rec, int mode) override;
    void hit(int pool, int rec, int sp, int sr, std::int16_t amount) override;
};

class MisHost final : public MissileHost {
public:
    Scene* s;
    Point16 rear{10, 30};
    explicit MisHost(Scene* sc) : s(sc) {}
    void sound(int id, int pool, int rec, int mode) override;
    void destroy(int pool, int rec) override;
    void on_destroy(int pool, int rec) override;
    void touch(int ship, std::int16_t pr) override;
    Point16 muzzle(int, int) override { return rear; }
    const VisibilityGlobals* visible() override;
};

class CapHost final : public CaptureHost {
public:
    Scene* s;
    explicit CapHost(Scene* sc) : s(sc) {}
    void sound(int id, int pool, int rec, int mode) override;
    std::int16_t weapon_word(int w, int off) override;
    void seg12_129e(int pool, int rec) override;
};

class FrameHost final : public UpdateHost {
public:
    Scene* s;
    explicit FrameHost(Scene* sc) : s(sc) {}
    World* frame_world(Pools& p) override;
    const VisibilityGlobals* visible_list() override;
    Blitter* blitter() override { return nullptr; }
    CollideHost& collide_host() override;
    CollideGlobals& collide_globals() override;
};

// The player, ship 0 (TRANSFORMER's collision divisors, 100 in every system), alone in the wave
// with the pool-6 record 0 as the placement leaves it. The launcher's weapon is `weapon` in
// slot 1 (20 of them).
struct Scene {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    VisibilityGlobals vis;
    is::shell::Rng rng;
    CollideGlobals cg;
    EffectState& es = effect_state();  // seg7:0x2e02 (pickup_destroyed) reads the program's table
    EffectWords ew;
    MissileState st;
    WeaponWords saved = missile_state().words;
    std::vector<Snd> sounds;         // the collision, effect and capture sounds
    std::vector<Snd> launch_sounds;  // the weapon system's
    std::vector<Hit> hits;           // the blast's dispatcher calls, before the dispatch
    std::vector<std::pair<int, std::int16_t>> touches;
    std::vector<std::pair<int, int>> dissolves;
    ColHost col{this};
    EffHost eff{this};
    MisHost mis{this};
    CapHost cap{this};
    FrameHost fh{this};
    CollideCtx x{w, g, cg, col, view, &vis, rng, f.dg_943a, f.dg_943c, f.dg_9454};
    EffectCtx ec{w,
                 es,
                 ew,
                 eff,
                 rng,
                 nullptr,
                 nullptr,
                 [](std::int16_t px, std::int16_t py) {
                     return Point16{static_cast<std::int16_t>(px >> 3), static_cast<std::int16_t>(py >> 3)};
                 },
                 &f.dg_943a,
                 &f.dg_943c};
    UpdateCtx u{FlightCtx{w.pools, w.ships, g, keys, fh, rng}, f, view, fh, joy, jb};
    MissileCtx mc{w, g, st, mis, rng};

    explicit Scene(int weapon, int selected = -2) {
        es = EffectState{};
        const is::hud::WeaponTable t(0);
        for (int k = 0; k < kWeaponTableBlocks; ++k) st.words.block(k) = t.block(k);
        missile_state().words = st.words;  // collide.h weapon_word, effects.cpp seg7:0x2e02's reads
        ec.weapons = &st.words;
        ec.dg_b0c = &cg.w0b0c;
        clear_effects(w.effects);
        g.now = 100000;
        ew.now = g.now;
        g.player = 0;
        g.ship_pool = kPoolShips;
        g.rock_pool = kPoolRocks;
        g.pool5 = 5;
        g.size = 1;
        g.game_frames = 500;
        g.demo = 1;
        w.size = 1;
        w.shift = 3;
        w.detail = 2;
        w.W = 1024;
        w.H = 683;
        w.player = 0;
        w.last_ship = 0;
        w.player_game = 1;
        view.play_w = 1024;
        view.play_h = 683;
        view.shift = 3;
        for (int p = 0; p < kPoolCount; ++p) w.pools.set_in_use(p, 0);
        w.pools.set_in_use(kPoolShips, 1);
        w.pools.set_allocated(6, 1);
        DesignRating& d = w.design_ratings[0];  // as test_world_collide.cpp's transformer_design
        d.armor4a = 8, d.armor5a = 6, d.armor4b = 8, d.armor5b = 6;
        d.special = 3;
        d.repair = 1;
        d.captures = 1;
        d.tank = 1000;
        w.roster.assign(1, RosterRecord{});
        ObjectRecord& r0 = w.pools.rec(6, 0);
        r0.state = 1;
        r0.mass = 20;
        r0.owner_pool = -1;
        r0.on_damage.set(kDamageGeneric);  // the placement's template, seg7:0x2652 [BYTES seg9:0x6624]
        w.pools.missile(0).age = 0x191;
        ShipRecord& s = w.ships[0];
        s.bec = 1;
        s.roster_index = -1;
        s.design_index = 0;
        s.weapon_slot = 1;
        s.slots_cb() = 2;
        s.fuel = 900;
        s.w26 = s.w28 = s.w2a = s.w2c = 100;
        set_byte_at(s, 0xb8 + 1, weapon);
        set_byte_at(s, 0xae + 1, 20);
        ObjectRecord& o = ship();
        o.x = 12800;
        o.y = 12800;
        o.extent = 264;
        o.size = 33;
        o.mass = 160;
        o.owner_pool = -1;
        o.on_screen = 1;  // seg8:0x5ccd: the blast skips an off-screen DGROUP:0x952e
        o.hit_sound = 0x0b;
        o.on_damage.set(kDamageShip);
        weapon_system(mc);
        if (selected != -2) set_byte_at(s, 0xb8 + 1, selected);
    }
    ~Scene() {
        missile_state().words = saved;
        es = EffectState{};
    }

    ObjectRecord& ship() { return w.pools.rec(kPoolShips, 0); }
    ObjectRecord& missile(int m = 0) { return w.pools.rec(6, m); }
    MissileSlot& entry(int m = 0) { return w.pools.missile(m); }
    ShipRecord& S() { return w.ships[0]; }
    ObjectRecord& add_rock(int i, std::int16_t px, std::int16_t py, std::int16_t mass = 1400) {
        if (w.pools.in_use(kPoolRocks) <= i) w.pools.set_in_use(kPoolRocks, static_cast<std::int16_t>(i + 1));
        ObjectRecord& k = w.pools.rec(kPoolRocks, i);
        k.state = 0;
        k.mass = mass;
        k.size = 44;
        k.extent = 352;
        k.x = px;
        k.y = py;
        k.owner_pool = -1;
        k.max_damage = 1;
        k.hit_sound = 8;
        k.on_damage.set(kDamageRock);
        return k;
    }
    // The player's corner `cells` cells right of the missile's corner (seg8:0x285a's unit is 1/8).
    void ship_at_distance(int cells) {
        ship().x = static_cast<std::int16_t>(missile().x + 8 * cells);
        ship().y = missile().y;
    }
    // The passes until the record stops running.
    int run(int limit = 1000) {
        int n = 0;
        while (missile().state == 0 && n < limit) {
            move_pool6(mc, u);
            ++n;
        }
        return n;
    }
    // One wave frame's effect loop (seg7:0x972-0x991): the entries 0 ... DGROUP:0x9434.
    void effects_pass() {
        if (w.w9430 <= 0) return;
        for (int k = 0; k <= w.w9434 && k < kEffectCount; ++k) effect_frame(ec, k);
    }
    // The effect passes until the blast dispatches (or `limit`); the number of them.
    int until_blast(int limit = 12) {
        int n = 0;
        while (hits.empty() && n < limit) {
            effects_pass();
            ++n;
        }
        return n;
    }
    // A contact of damage `a` on pool-6 record m through the dispatcher's on_damage.
    std::int16_t contact(int m, std::int16_t a) { return call_on_damage(x, 6, m, a); }
    int sounds_of(int id) const {
        int n = 0;
        for (const Snd& s : sounds) n += s.id == id ? 1 : 0;
        return n;
    }
};

void ColHost::sound(int id, int pool, int rec, int) { s->sounds.push_back({id, pool, rec}); }
void ColHost::on_destroy(int pool, int rec) { object_destroyed(s->ec, pool, rec); }  // seg12:0x274a
void ColHost::destroy(int pool, int rec) { object_destroyed(s->ec, pool, rec); }
void EffHost::sound(int id, int pool, int rec, int) { s->sounds.push_back({id, pool, rec}); }
void EffHost::hit(int pool, int rec, int sp, int sr, std::int16_t amount) {
    s->hits.push_back({pool, rec, sp, sr, amount});
    dispatch(s->x, pool, rec, sp, sr, amount);  // seg8:0x5df2 as flight_hooks.cpp's ProgramEffectHost
}
void MisHost::sound(int id, int pool, int rec, int) { s->launch_sounds.push_back({id, pool, rec}); }
void MisHost::destroy(int pool, int rec) { object_destroyed(s->ec, pool, rec); }
void MisHost::on_destroy(int pool, int rec) { object_destroyed(s->ec, pool, rec); }
void MisHost::touch(int ship, std::int16_t pr) {
    s->touches.push_back({ship, pr});
    CaptureCtx cc{s->x, s->u.flight, s->cap};
    is::world::touch(cc, ship, pr);  // seg13:0x16ac
}
const VisibilityGlobals* MisHost::visible() { return &s->vis; }
void CapHost::sound(int id, int pool, int rec, int) { s->sounds.push_back({id, pool, rec}); }
std::int16_t CapHost::weapon_word(int w, int off) { return s->st.words.word(w, off); }
void CapHost::seg12_129e(int pool, int rec) { s->dissolves.push_back({pool, rec}); }
World* FrameHost::frame_world(Pools& p) { return &s->w.pools == &p ? &s->w : nullptr; }
const VisibilityGlobals* FrameHost::visible_list() { return &s->vis; }
CollideHost& FrameHost::collide_host() { return s->col; }
CollideGlobals& FrameHost::collide_globals() { return s->cg; }

// seg8:0x1290(mass, 25) = (mass / 25 + 25) >> 1, at least 1 [BYTES seg8:0x5da7-0x5dd0].
int mm_of(int mass) {
    const int m = (mass / 25 + 25) >> 1;
    return m < 1 ? 1 : m;
}
// The blast's amount at distance d for blast size B and target mass: lo16(15·lo16(600 B / d)) / Mm
// [BYTES seg8:0x5d9c-0x5da4 seg8:0x5b22].
int amount_of(int b, int d, int mass) { return (15 * (600 * b / d)) / mm_of(mass); }
// R = (min(B, 7) + 8)·15 [BYTES seg8:0x5d7d-0x5d90].
int range_of(int b) { return ((b < 7 ? b : 7) + 8) * 15; }

}  // namespace

TEST_CASE("hits: the launch gives the missile record the words the contact chain reads [BYTES seg11:0x7ff5-0x8031 seg11:0x80dd-0x8113]") {
    struct Case {
        int w, max_damage, blast, on_destroy_pickup;
    };
    for (const Case k : {Case{11, 1, 8, 0}, Case{18, 1, 4, 1}, Case{19, 1, 7, 1}, Case{20, 1, 5, 1},
                         Case{21, 3, 3, 0}, Case{16, 1, 6, 0}, Case{9, 1, 6, 0}}) {
        CAPTURE(k.w);
        Scene sc(k.w);
        REQUIRE(sc.w.pools.in_use(6) == 1);
        CHECK(sc.missile().max_damage == k.max_damage);
        CHECK(sc.missile().blast_size == k.blast);
        CHECK(sc.missile().mass == 20);
        CHECK((sc.missile().on_destroy.get() == kDestroyPickup) == (k.on_destroy_pickup != 0));
        CHECK(sc.entry().weapon == k.w);
        CHECK((sc.missile().b53 != 0) == (k.w == 16));  // `capturable` [BYTES seg11:0x8127]
    }
}

TEST_CASE("hits: the DUMB MISSILE's life-end explosion at pass 55: seg12:0x274a, then the blast 6 passes on (B 8, range 225) [BYTES seg12:0x2a66-0x2b0d seg8:0x4234-0x4250 seg8:0x5d7d]") {
    Scene sc(11);
    sc.missile().x = 4000;
    sc.missile().y = 4000;
    CHECK(sc.run() == 55);
    REQUIRE(sc.w.pools.in_use(6) == 0);
    CHECK(sc.entry().age == 100);  // W+0x1e [BYTES seg12:0x2aec]
    CHECK(sc.entry().weapon == -1);
    CHECK(sc.missile().state == 1);
    // The KABOOM at the missile; at Detail 2 eight fragments, fragment 0 a type-4 entry at the
    // missile's corner, the other seven type 1 [BYTES seg12:0x2a66 seg12:0x28ce seg12:0x296a].
    REQUIRE(sc.sounds.size() == 1);
    CHECK(sc.sounds[0].id == 0x45);
    CHECK(sc.sounds[0].pool == 6);
    CHECK(sc.sounds[0].rec == 0);
    CHECK(sc.w.w9430 == 8);
    CHECK(sc.w.effects[0].type == 4);
    CHECK(sc.w.effects[0].end == 10);
    CHECK(sc.w.effects[0].pool == 6);
    CHECK(sc.w.effects[0].rec == 0);
    CHECK(sc.es.sides.point[0].x == sc.missile().x);
    CHECK(sc.es.sides.point[0].y == sc.missile().y);
    for (int k = 1; k < 8; ++k) CHECK(sc.w.effects[static_cast<std::size_t>(k)].type == 1);
    // The player 100 cells away (the range's lower clamp), mass 391: Mm = (15 + 25) >> 1 = 20,
    // a = 15·lo16(4800 / 100) / 20 = 36.
    sc.ship_at_distance(100);
    sc.ship().mass = 391;
    const std::uint32_t score = sc.w.score;
    const std::int16_t w9432 = sc.w.w9432;
    for (int i = 1; i <= 5; ++i) {
        sc.effects_pass();
        CHECK(sc.hits.empty());  // no blast on effect calls 1 ... 5
    }
    sc.effects_pass();  // call 6: the frame counter 6
    REQUIRE(sc.hits.size() == 1);
    CHECK(sc.hits[0].p == kPoolShips);
    CHECK(sc.hits[0].sp == 6);
    CHECK(sc.hits[0].sr == 0);
    CHECK(sc.hits[0].amount == -36);
    CHECK(amount_of(8, 100, 391) == 36);
    CHECK(sc.ship().flame == 4);  // +2, +2 more under 150
    // The launcher's own ship takes the hit: its design sound, the tumble 600 ms (12 · 36 < 600).
    REQUIRE(sc.sounds.size() == 2);
    CHECK(sc.sounds[1].id == 0x0b);
    CHECK(sc.S().be2 == 1);
    CHECK(sc.S().lockout.get() == sc.g.now + 600u);
    CHECK(sc.S().w26 < 100);
    // The launcher is the credit: no score, no DGROUP:0x9432, no record of a player's hit.
    CHECK(sc.w.score == score);
    CHECK(sc.w.w9432 == w9432);
    CHECK(sc.cg.hit_time == 0u);
    // The push, after the dispatch: v = (lo16(250·8) / (100 / 10)) / ((391 + 100) / 100) = 200 / 4
    // = 50, vx += 50·100 / 100 [BYTES seg8:0x5bbe-0x5c98].
    CHECK(sc.ship().vx == 50);
    CHECK(sc.ship().vy == 0);
}

TEST_CASE("hits: the blast dispatches in pool order, a rock's sound before the ship's, and pushes each away from the centre [BYTES seg8:0x5af6 seg8:0x643b-0x645d seg8:0x5bbe-0x5c98]") {
    Scene sc(11);
    sc.missile().x = 4000;
    sc.missile().y = 4000;
    sc.contact(0, 5);
    ObjectRecord& rock = sc.add_rock(0, static_cast<std::int16_t>(sc.missile().x - 800), sc.missile().y);
    rock.max_damage = 1000;
    sc.ship_at_distance(100);
    sc.until_blast(8);
    REQUIRE(sc.hits.size() == 2);
    CHECK(sc.hits[0].p == kPoolRocks);
    CHECK(sc.hits[1].p == kPoolShips);
    int rock_at = -1, ship_at = -1, i = 0;
    for (const Snd& s : sc.sounds) {
        if (s.id == 8) rock_at = i;
        if (s.id == 0x0b) ship_at = i;
        ++i;
    }
    CHECK(rock_at >= 0);
    CHECK(rock_at < ship_at);
    // The rock: v = (2000 / 10) / ((1400 + 100) / 100) = 200 / 15 = 13, left of the centre.
    CHECK(rock.vx == -13);
    CHECK(sc.ship().vx > 0);
    // The credited rock hit counts for the player's record; the player's own ship does not.
    CHECK(sc.cg.hit_pool == 3);
}

TEST_CASE("hits: the blast's range is (min(B, 7) + 8)·15 and its amount lo16(15·(600 B / d)) / Mm; 12·a decides the tumble [BYTES seg8:0x5d69-0x5da4 seg8:0x68f0-0x6950]") {
    // d = 224 is inside the DUMB MISSILE's 225, 225 outside; d under 100 counts as 100.
    for (const int d : {224, 225, 60}) {
        CAPTURE(d);
        Scene sc(11);
        sc.missile().x = 4000;
        sc.missile().y = 4000;
        sc.contact(0, 5);
        sc.ship_at_distance(d);
        sc.until_blast(7);
        if (d == 225) {
            CHECK(sc.hits.empty());
            CHECK(sc.ship().flame == 0);
            CHECK(sc.ship().vx == 0);
        } else {
            REQUIRE(sc.hits.size() == 1);
            const int dd = d < 100 ? 100 : d;
            CHECK(sc.hits[0].amount == -amount_of(8, dd, 160));
            CHECK(sc.ship().flame == (dd < 150 ? 4 : 2));
        }
    }
    // ENLIGHTENMENT's 20 at d = 100 on a ship of mass 160: Mm = (6 + 25) >> 1 = 15, q = 120,
    // a = 15·120 / 15 = 120, so the lock-out is 12·120 = 1440 ms; the range clamps B at 7.
    Scene sc(22);
    CHECK(sc.missile().blast_size == 20);
    sc.missile().x = 4000;
    sc.missile().y = 4000;
    sc.contact(0, 5);
    sc.ship_at_distance(224);
    sc.until_blast(7);
    CHECK(sc.hits.size() == 1);
    Scene q(22);
    q.missile().x = 4000;
    q.missile().y = 4000;
    q.contact(0, 5);
    q.ship_at_distance(100);
    q.until_blast(7);
    REQUIRE(q.hits.size() == 1);
    CHECK(q.hits[0].amount == -120);
    CHECK(q.S().be2 == 1);
    CHECK(q.S().lockout.get() == q.g.now + 1440u);
}

TEST_CASE("hits: the heat is added before the dispatch, so a rock at heat 3 splits on the blast itself [BYTES seg8:0x5b49-0x5b77 seg8:0x5bb2 seg7:0x50df]") {
    for (const int heat : {3, 1}) {
        CAPTURE(heat);
        Scene sc(11);
        sc.missile().x = 4000;
        sc.missile().y = 4000;
        sc.contact(0, 5);
        ObjectRecord& rock = sc.add_rock(0, static_cast<std::int16_t>(sc.missile().x + 800), sc.missile().y);
        rock.flame = static_cast<std::uint8_t>(heat);
        sc.ship().x = 20000;  // out of the blast
        sc.until_blast(7);
        REQUIRE(!sc.hits.empty());  // a split rock's pieces are in the loop's range too
        CHECK(sc.hits[0].p == kPoolRocks);
        // heat + 4 is seen by seg7:0x5090's old-heat test (break only above 6): 7 splits, 5 does not.
        // A split big rock lands its first small rock in its own record (state 1 at the scan):
        // mass 0x15e, and more records join the pool; heat 1 + 4 = 5 only takes damage.
        const ObjectRecord& r0 = sc.w.pools.rec(kPoolRocks, 0);
        CHECK(r0.mass == (heat == 3 ? 0x15e : 1400));
        CHECK((sc.w.pools.in_use(kPoolRocks) > 1) == (heat == 3));
        CHECK(sc.sounds_of(0x45) == (heat == 3 ? 2 : 1));  // the missile's KABOOM and the split's
    }
}

TEST_CASE("hits: an amount of exactly -1 does nothing in the dispatcher, the heat and the push still apply [BYTES seg8:0x5e93-0x5e9b seg8:0x5b3b seg8:0x5bbe]") {
    // B 1 (PELLETS) at d = 100: q = 6, 15·6 = 90; a rock of mass 1700 has Mm = (68 + 25) >> 1 = 46,
    // a = 1, dispatched as -1. Mass 1400 has Mm 40 and a = 2, which is dispatched.
    for (const int mass : {1700, 1400}) {
        CAPTURE(mass);
        Scene sc(17);
        CHECK(sc.missile().blast_size == 1);
        sc.missile().x = 4000;
        sc.missile().y = 4000;
        sc.contact(0, 5);
        ObjectRecord& rock = sc.add_rock(0, static_cast<std::int16_t>(sc.missile().x + 800), sc.missile().y,
                                         static_cast<std::int16_t>(mass));
        rock.max_damage = 1000;
        sc.ship().x = 20000;
        sc.until_blast(7);
        REQUIRE(sc.hits.size() == 1);
        CHECK(sc.hits[0].amount == (mass == 1700 ? -1 : -2));
        CHECK(rock.flame == 4);  // the heat applies either way
        CHECK(rock.vx > 0);      // so does the push
        if (mass == 1700) {
            CHECK(sc.sounds_of(8) == 0);  // no hit sound
            CHECK(rock.damage == 0);      // no damage
        } else {
            CHECK(sc.sounds_of(8) == 1);
            CHECK(rock.damage != 0);
        }
    }
}

TEST_CASE("hits: every launched weapon blasts its range from the frame its destroy handler sets, the launcher included [BYTES seg8:0x5d77 seg12:0x274a seg7:0x2e02 seg8:0x4234 seg8:0x42d0]") {
    struct Case {
        int w, b, r, pass;  // weapon, B, R, the effect call that blasts (6 for type 4, 5 for type 6)
    };
    for (const Case k : {Case{6, 7, 225, 6}, Case{7, 7, 225, 6}, Case{8, 9, 225, 6}, Case{9, 6, 210, 6},
                         Case{10, 6, 210, 6}, Case{11, 8, 225, 6}, Case{12, 6, 210, 6}, Case{14, 6, 210, 6},
                         Case{16, 6, 210, 6}, Case{17, 1, 135, 6}, Case{18, 4, 180, 5}, Case{19, 7, 225, 5},
                         Case{20, 5, 195, 5}, Case{21, 3, 165, 6}, Case{22, 20, 225, 6}}) {
        CAPTURE(k.w);
        CHECK(range_of(k.b) == k.r);
        for (const int d : {k.r - 1, k.r}) {
            CAPTURE(d);
            Scene sc(k.w);
            REQUIRE(sc.missile().blast_size == k.b);
            sc.missile().x = 4000;
            sc.missile().y = 4000;
            // A contact of total damage 5 destroys every launched weapon (max_damage 1 or 3).
            CHECK(sc.contact(0, 5) == 1);
            CHECK(sc.missile().state == 1);
            sc.ship_at_distance(d);
            const int n = sc.until_blast(10);
            if (d == k.r) {
                CHECK(sc.hits.empty());
            } else {
                REQUIRE(sc.hits.size() == 1);
                CHECK(n == k.pass);
                CHECK(sc.hits[0].p == kPoolShips);  // the launcher is not spared
                CHECK(sc.hits[0].amount == -amount_of(k.b, d < 100 ? 100 : d, 160));
            }
        }
    }
}

TEST_CASE("hits: THROWING STARS die on a total damage of 4 (max_damage 3), not 3 [BYTES seg7:0x2652-0x26c3]") {
    Scene sc(21);
    REQUIRE(sc.missile().max_damage == 3);
    CHECK(sc.contact(0, 3) == 0);
    CHECK(sc.missile().damage == 3);
    CHECK(sc.missile().state == 0);
    CHECK(sc.w.w9430 == 0);
    CHECK(sc.contact(0, 1) == 1);  // 4 > 3
    CHECK(sc.missile().state == 1);
    CHECK(sc.entry().weapon == -1);
    CHECK(sc.w.w9430 > 0);
    // A missile that already exceeds its maximum is not hurt again and returns 0.
    Scene q(11);
    CHECK(q.contact(0, 2) == 1);  // 2 > 1
    q.missile().state = 0;
    CHECK(q.contact(0, 2) == 0);  // damage 2 > max 1: returns 0 without adding
    CHECK(q.missile().damage == 2);
}

TEST_CASE("hits: a BEER BOTTLE destroyed by a contact breaks up: SHATTER 0x1f, a type-6 entry of 20 frames, 25 rand(), the blast at frame 5 (B 4, range 180) [BYTES seg7:0x2e02-0x3079 seg7:0x3730 seg8:0x42cc-0x42e2]") {
    Scene sc(18);
    sc.missile().x = 4000;
    sc.missile().y = 4000;
    CHECK(sc.contact(0, 2) == 1);
    CHECK(sc.sounds_of(0x1f) == 1);
    CHECK(sc.sounds_of(0x45) == 0);
    CHECK(sc.entry().weapon == 18);  // a break-up keeps the entry [BYTES seg7:0x2e02]
    REQUIRE(sc.w.w9430 == 1);
    const EffectEntry& e = sc.w.effects[0];
    CHECK(e.type == 6);
    CHECK(e.frame == 1);
    CHECK(e.end == 20);
    CHECK(e.pool == 6);
    CHECK(e.rec == 0);
    CHECK(sc.es.sides.point[0].x == sc.missile().x);
    CHECK(sc.es.sides.point[0].y == sc.missile().y);
    // The entry index 0 is not above Detail (2): the 5 x 5 pieces of step 1; side size / 5.
    CHECK(e.step == 1);
    CHECK(e.side == sc.missile().size / 5);
    sc.ship_at_distance(179);
    is::shell::Rng expect = sc.rng;
    for (int i = 0; i < 25; ++i) expect.rand();
    sc.effects_pass();  // frame 1: seg7:0x342e draws 25 rand()
    CHECK(sc.rng.state() == expect.state());
    CHECK(sc.hits.empty());
    for (int i = 2; i <= 4; ++i) {
        sc.effects_pass();
        CHECK(sc.hits.empty());
    }
    sc.effects_pass();  // call 5
    REQUIRE(sc.hits.size() == 1);
    CHECK(sc.hits[0].sp == 6);
    CHECK(sc.hits[0].amount == -amount_of(4, 179, 160));
    // A step 2 for an entry index above Detail: at Detail 0 entry 1 (entry 0 busy with a break-up).
    Scene q(18);
    q.w.detail = 0;
    q.missile().x = 4000;
    q.missile().y = 4000;
    q.w.effects[0].frame = 3;
    q.w.effects[0].end = 20;
    q.w.effects[0].type = 6;
    q.w.w9430 = 1;
    q.contact(0, 2);
    CHECK(q.w.effects[1].type == 6);
    CHECK(q.w.effects[1].step == 2);
    CHECK(q.w.w9430 == 2);
}

TEST_CASE("hits: SPUD MISSILES and BOOMERANGS break up and blast at frame 5, the boomerang holder is not dispatched [BYTES seg7:0x2e24-0x3079 seg8:0x5b49-0x5b77 seg8:0x5bb2 seg8:0x5bbe-0x5c98]") {
    // SPUD MISSILES: SHATTER 0x1f, B 5, range 195.
    {
        Scene sc(20);
        sc.missile().x = 4000;
        sc.missile().y = 4000;
        CHECK(sc.contact(0, 2) == 1);
        CHECK(sc.sounds_of(0x1f) == 1);
        sc.ship_at_distance(194);
        CHECK(sc.until_blast(8) == 5);
        CHECK(sc.hits.size() == 1);
        CHECK(sc.S().be2 == 1);  // the launcher is tumbled by its own spud
    }
    // BOOMERANGS: FLANGE 0x1a; a ship whose selected weapon has end kind 6 is not dispatched, but
    // still gets the heat and the push; any other selection is.
    for (const int selected : {19, 8}) {
        CAPTURE(selected);
        Scene sc(19, selected);
        sc.missile().x = 4000;
        sc.missile().y = 4000;
        CHECK(sc.contact(0, 2) == 1);
        CHECK(sc.sounds_of(0x1a) == 1);
        sc.ship_at_distance(100);
        CHECK(sc.until_blast(8) == 5);
        REQUIRE(sc.hits.size() == 1);
        CHECK(sc.ship().flame == 4);
        CHECK(sc.ship().vx > 0);
        if (selected == 19) {
            CHECK(sc.S().be2 == 0);
            CHECK(sc.sounds_of(0x0b) == 0);
            CHECK(sc.S().w26 == 100);
            CHECK(sc.S().lockout.get() == 0u);
        } else {
            CHECK(sc.S().be2 == 1);
            CHECK(sc.sounds_of(0x0b) == 1);
            CHECK(sc.S().w26 < 100);
            CHECK(sc.S().lockout.get() == sc.g.now + 600u);
        }
    }
}

TEST_CASE("hits: a fragment's blast reads a freed entry, W(-1)+0x24 is never 6: the boomerang holder is dispatched [BYTES seg12:0x2afd seg8:0x6040 seg8:0x143a-0x1457]") {
    Scene sc(11, 19);  // a DUMB MISSILE's launcher with BOOMERANGS selected
    sc.missile().x = 4000;
    sc.missile().y = 4000;
    CHECK(sc.contact(0, 2) == 1);
    CHECK(sc.entry().weapon == -1);
    sc.ship_at_distance(100);
    sc.until_blast(8);
    REQUIRE(sc.hits.size() == 1);
    CHECK(sc.S().be2 == 1);
    CHECK(sc.sounds_of(0x0b) == 1);
}

TEST_CASE("hits: MINES and DEATH HEADS blast the launcher that laid them; the tumble is 600 ms, DGROUP:0xb0c stays 1 [ORACLE core-0009] [BYTES seg8:0x5b88-0x5bb8]") {
    for (const int w : {9, 10}) {
        CAPTURE(w);
        Scene sc(w);
        sc.missile().x = 4000;
        sc.missile().y = 4000;
        // The life end, not a contact: age to lifetime - 1 (a mine is 64, a head 200).
        const int life = sc.st.words.word(w, wofs::kLifetime);
        sc.entry().age = static_cast<std::int16_t>(life - 1);
        sc.entry().lock = 1;
        sc.run();
        CHECK(sc.missile().state == 1);
        CHECK(sc.entry().weapon == -1);
        CHECK(sc.entry().age == life);
        sc.ship_at_distance(110);
        const std::int16_t w9432 = sc.w.w9432;  // the launch's own += 3 (W+0x46 = 0)
        sc.until_blast(8);
        REQUIRE(sc.hits.size() == 1);
        CHECK(sc.hits[0].p == kPoolShips);
        // a@100·Mm = 540 at B 6; the player's Mm 15: 15·lo16(3600 / 110) / 15 = 32.
        CHECK(sc.hits[0].amount == -32);
        CHECK(sc.S().be2 == 1);
        CHECK(sc.S().lockout.get() == sc.g.now + 600u);
        CHECK(sc.cg.w0b0c == 1);  // set again after the dispatcher call
        CHECK(sc.w.w9432 == w9432);
    }
}

TEST_CASE("hits: a pool-6 record destroyed with the player as the credit scores 1000, P[6] [BYTES DGROUP:0xb1a seg8:0x6486-0x64ae seg8:0x7be8]") {
    CHECK(kill_points(6) == 1000);
    CHECK(kill_weight(6) == 0);
    Scene sc(19);
    // A second boomerang, another of the player's, as target; the first is the attacker.
    sc.w.pools.set_allocated(6, 2);
    sc.w.pools.set_in_use(6, 2);
    sc.w.pools.rec(6, 1) = sc.missile(0);
    sc.w.pools.missile(1) = sc.entry(0);
    sc.missile(0).state = 0;
    sc.missile(1).state = 0;
    sc.missile(0).x = 4000;
    sc.missile(0).y = 4000;
    sc.missile(1).x = 4100;
    sc.missile(1).y = 4000;
    sc.ship().x = 20000;
    const std::uint32_t score = sc.w.score;
    const std::uint32_t rating = sc.w.rating;
    // seg12:0x0's last branch with the first record as the finder: T = the object found, X = (6, m).
    const std::int16_t res = dispatch(sc.x, 6, 1, 6, 0, 5);
    CHECK(res == 1);
    CHECK(sc.w.score == score + 1000u);
    CHECK(sc.w.rating == rating + 1000u);
    CHECK(sc.missile(1).state == 1);
    CHECK(sc.sounds_of(0x1a) == 1);  // FLANGE of the break-up
    // A contact that does not destroy it scores the weight W[6] = 0 times the amount: nothing.
    Scene q(11);
    q.w.pools.set_allocated(6, 2);
    q.w.pools.set_in_use(6, 2);
    q.w.pools.rec(6, 1) = q.missile(0);
    q.w.pools.missile(1) = q.entry(0);
    q.missile(0).state = 0;
    q.missile(1).state = 0;
    q.missile(1).max_damage = 100;
    q.ship().x = 20000;
    const std::uint32_t qs = q.w.score;
    const std::uint32_t qr = q.w.rating;
    CHECK(dispatch(q.x, 6, 1, 6, 0, 5) == 0);
    CHECK(q.w.score == qs);
    CHECK(q.w.rating == qr);
}

namespace {

// A DOUGHNUT over the player's corner, homing at age `age`, on screen with the visibility list
// built, so the pool-6 mover's contact branch finds the ship (the catch scene of the s7 tests).
void doughnut_over_ship(Scene& sc, int age) {
    sc.ship().x = 8000;
    sc.ship().y = 8000;
    ObjectRecord& m = sc.missile();
    m.x = 8000 + 40;
    m.y = 8000 + 40;
    m.vx = m.vy = 0;
    sc.entry().age = static_cast<std::int16_t>(age);
    ViewInputs vi;
    vi.player_x = sc.ship().x;
    vi.player_y = sc.ship().y;
    recentre(sc.view, vi);
    for (ObjectRecord* o : {&sc.ship(), &m}) {
        const ScreenPoint p = world_to_screen(sc.view, o->x, o->y, 0, 0, 0);
        o->scr_x = p.x;
        o->scr_y = p.y;
        o->on_screen = 1;
    }
    sc.vis.count = 0;
    visibility(sc.w.pools, sc.view, sc.vis, sc.rng, VisibilityHost{});
}

void frame(Scene& sc) {
    sc.fh.frame_ctx = &sc.u;
    move_pool6(sc.mc, sc.u);
    sc.fh.frame_ctx = nullptr;
}

}  // namespace

TEST_CASE("hits: a DOUGHNUT eaten by the player: 1200 ms tumble, fuel + 20 up to the tank, 0x71, the dissolve [BYTES seg12:0x37e-0x3ab seg13:0x1e8d-0x1f5e]") {
    Scene sc(16);
    REQUIRE(sc.missile().b53 == 1);
    REQUIRE(sc.st.words.word(16, 0x42) != 0);
    doughnut_over_ship(sc, 120);
    sc.S().fuel = 900;
    frame(sc);
    REQUIRE(sc.touches.size() == 1);
    CHECK(sc.touches[0].first == 0);
    CHECK(sc.touches[0].second == 0x600);
    CHECK(sc.missile().race_status == 10);  // +0x51
    CHECK(sc.missile().state == 1);
    CHECK(sc.missile().blast_size == 0);  // +0x48 = the ship
    CHECK(sc.S().be2 == 1);
    CHECK(sc.S().lockout.get() == sc.g.now + 1200u);
    CHECK(sc.ship().angle == 1);
    CHECK(sc.S().fuel == 920);
    CHECK(sc.sounds_of(0x71) == 1);
    CHECK(sc.sounds[0].pool == kPoolShips);
    CHECK(sc.sounds[0].rec == 0);
    CHECK(sc.sounds_of(0x45) == 0);  // eaten: no explosion
    REQUIRE(sc.dissolves.size() == 1);
    CHECK(sc.dissolves[0] == std::pair<int, int>{6, 0});
    CHECK(sc.w.score == 0u);  // no score
    // The fuel is capped at the tank.
    Scene q(16);
    doughnut_over_ship(q, 120);
    q.S().fuel = 995;
    frame(q);
    CHECK(q.S().fuel == 1000);
    // A fuel above the tank drops to it (`cmp; jle`).
    Scene r(16);
    doughnut_over_ship(r, 120);
    r.S().fuel = 1100;
    frame(r);
    CHECK(r.S().fuel == 1000);
}

TEST_CASE("hits: a doughnut is not in play for its launcher until age 10 [BYTES seg8:0x6c67-0x6c97]") {
    for (const int age : {9, 10}) {
        CAPTURE(age);
        Scene sc(16);
        doughnut_over_ship(sc, age);
        frame(sc);
        CHECK(sc.touches.size() == (age == 10 ? 1u : 0u));
    }
}

TEST_CASE("hits: a cloaked ship gets nothing from a doughnut, a shielded one bounces it unharmed [BYTES seg12:0x382-0x3a0 seg13:0x172e-0x1740 seg8:0x6311-0x637d]") {
    // Cloaked (+0xe8): the touch branch is entered and the touch returns at exit 4.
    {
        Scene sc(16);
        doughnut_over_ship(sc, 120);
        sc.S().be8 = 1;
        sc.S().fuel = 900;
        frame(sc);
        CHECK(sc.touches.size() == 1);
        CHECK(sc.missile().state == 0);
        CHECK(sc.missile().race_status == 0);
        CHECK(sc.S().fuel == 900);
        CHECK(sc.sounds_of(0x71) == 0);
        CHECK(sc.dissolves.empty());
    }
    // Shielded (+0xea): no touch; the doughnut's own dispatch is gated off by its `capturable`.
    {
        Scene sc(16);
        doughnut_over_ship(sc, 120);
        sc.S().bea = 1;
        sc.S().fuel = 900;
        frame(sc);
        CHECK(sc.touches.empty());
        CHECK(sc.missile().state == 0);
        CHECK(sc.missile().damage == 0);
        CHECK(sc.S().fuel == 900);
        CHECK(sc.sounds_of(0x71) == 0);
        CHECK(sc.sounds_of(0x45) == 0);
    }
}

TEST_CASE("hits: any other object explodes a doughnut on a dispatched contact of damage 2, with B 6; unconsumed it ends at age 350 [BYTES seg10:0x1fb2 seg12:0x274a seg12:0x439-0x4fc]") {
    Scene sc(16);
    sc.missile().x = 4000;
    sc.missile().y = 4000;
    sc.add_rock(0, 20000, 20000);
    // The rock (pool 3) as the attacker: the gate of `capturable` names only pool 2 and a capturing ship.
    CHECK(dispatch(sc.x, 6, 0, kPoolRocks, 0, 2) == 1);
    CHECK(sc.missile().state == 1);
    CHECK(sc.entry().weapon == -1);
    CHECK(sc.entry().age == 350);
    CHECK(sc.sounds_of(0x45) == 1);
    CHECK(sc.w.effects[0].type == 4);
    CHECK(sc.missile().blast_size == 6);
    // A capturing ship as the attacker is gated off (a hit of damage 2 does nothing).
    Scene q(16);
    q.missile().x = 4000;
    q.missile().y = 4000;
    CHECK(dispatch(q.x, 6, 0, kPoolShips, 0, 2) == 0);
    CHECK(q.missile().state == 0);
    CHECK(q.missile().damage == 0);
    // Unconsumed, the record ends at age 350 after 180 passes (175 -> 345 -> 350), exploding.
    Scene r(16);
    r.missile().x = 2000;
    r.missile().y = 2000;
    CHECK(r.run() == 180);
    CHECK(r.entry().age == 350);
    CHECK(r.entry().weapon == -1);
    CHECK(r.sounds_of(0x45) == 1);
    CHECK(r.touches.empty());
}
