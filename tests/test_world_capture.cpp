// The touch routine seg13:0x16ac and its helpers (src/world/capture.h) and the icon mover
// seg7:0x309c's touch path (src/world/icon_mover.h) (M4 s2 CAPTURE), against the oracle's four
// captures of core-0002 and the listings' arithmetic (work/analysis/m4/s2-capture.md with
// s2-capture-verify.md).
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/capture.h"
#include "world/collide.h"
#include "world/effects.h"
#include "world/icon_mover.h"
#include "world/update.h"

using namespace is::world;

namespace {

struct Sound {
    int id, pool, rec, mode;
    bool operator==(const Sound& o) const {
        return id == o.id && pool == o.pool && rec == o.rec && mode == o.mode;
    }
};
struct Msg {
    int ship, pool, rec, code;
    bool operator==(const Msg& o) const {
        return ship == o.ship && pool == o.pool && rec == o.rec && code == o.code;
    }
};

class RecCaptureHost : public CaptureHost {
public:
    std::vector<Sound> sounds;
    std::vector<int> plays;
    std::vector<Msg> msgs;
    std::vector<int> dissolves;
    int erases = 0;
    std::int16_t w42 = 0, w24 = 0;
    void erase(int, int, int, int) override { ++erases; }
    void dissolve(int rec) override { dissolves.push_back(rec); }
    void sound(int id, int pool, int rec, int mode) override { sounds.push_back({id, pool, rec, mode}); }
    void play(int id) override { plays.push_back(id); }
    void message(int ship, int pool, int rec, int code) override { msgs.push_back({ship, pool, rec, code}); }
    std::string lang(int id) override {
        return id == 1700 ? "Out Of Control. That" : id == 1701 ? "Icon Didn't Taste So Good!" : "";
    }
    std::int16_t weapon_word(int, int off) override { return off == 0x42 ? w42 : off == 0x24 ? w24 : 0; }
};

class RecCollideHost : public CollideHost {
public:
    std::vector<std::pair<int, int>> destroyed;
    void destroy(int pool, int rec) override { destroyed.emplace_back(pool, rec); }
};

class NullFlightHost : public FlightHost {
public:
    int sprites = 0;
    void player_sprite() override { ++sprites; }
};

// A wave with the player (ship 0, a capturing design) and eight icons; icon 1 is the target.
struct Rig {
    World w;
    WaveGlobals g;
    CollideGlobals cg;
    RecCollideHost ch;
    ViewState view;
    is::shell::Rng rng;
    std::int16_t a = 0, b = 0, h = 0;
    KeyLatch keys;
    NullFlightHost fh;
    RecCaptureHost cap;
    CollideCtx x{w, g, cg, ch, view, nullptr, rng, a, b, h};
    FlightCtx f{w.pools, w.ships, g, keys, fh, rng};
    CaptureCtx c{x, f, cap};

    Rig() {
        g.now = 100000;
        g.player = 0;
        g.ship_pool = kPoolShips;
        g.demo = 1;
        w.player_game = 1;
        w.pools.set_in_use(kPoolShips, 1);
        w.pools.set_in_use(kPoolIcons, 8);
        w.icons_a = 8;
        w.design_ratings[0].captures = 1;
        w.design_ratings[0].tank = 1000;
        ShipRecord& s = w.ships[0];
        s.bec = 1;
        ObjectRecord& p = w.pools.rec(kPoolShips, 0);
        p.size = 33, p.scr_x = 500, p.scr_y = 300;
        for (int i = 0; i < 8; ++i) {
            ObjectRecord& o = w.pools.rec(kPoolIcons, i);
            o.size = 32, o.scr_x = 520, o.scr_y = 300, o.b53 = 1;
            w.icons[static_cast<std::size_t>(i)].number = static_cast<std::int16_t>(10 + i);
        }
        count_icons(w);
    }
    ObjectRecord& icon(int i) { return w.pools.rec(kPoolIcons, i); }
};

}  // namespace

TEST_CASE("capture: the icon's value from its damage, 16-bit [BYTES seg13:0x1ae7-0x1af2]") {
    CHECK(capture_value(0) == 100);
    // core-0002's 79 and 34: the damage moves in steps of 3 (seg7:0x2375), and only 51 and 156
    // among the multiples of 3 give them [ORACLE core-0002].
    CHECK(capture_value(48) == 80);
    CHECK(capture_value(51) == 79);
    CHECK(capture_value(54) == 77);
    CHECK(capture_value(153) == 35);
    CHECK(capture_value(156) == 34);
    CHECK(capture_value(159) == 32);
    CHECK(capture_value(211) == 10);  // the cap of seg7:0x2386
}

TEST_CASE("capture: core-0002's four captures [ORACLE core-0002]") {
    // (damage, resources before, captured before, score before) -> value, as the watch logs read.
    const std::vector<std::tuple<int, int, int, unsigned, int>> runs = {
        {0, 0, 0, 0u, 100},       // capture cap2
        {51, 0, 0, 0u, 79},       // capture-b cap1
        {0, 79, 1, 4000u, 100},   // capture-b cap2
        {156, 0, 0, 1800u, 34},   // gun2
    };
    for (const auto& [damage, res, cap, score, value] : runs) {
        Rig r;
        ShipRecord& s = r.w.ships[0];
        s.set_resources(static_cast<std::int16_t>(res));
        set_word_at(s, 0xd2, cap);
        r.w.score = score;
        r.w.rating = score;
        r.icon(1).damage = static_cast<std::int16_t>(damage);
        touch(r.c, 0, pool_rec(kPoolIcons, 1));
        CHECK(r.w.w9604 == value);
        CHECK(s.resources() == res + value);
        CHECK(word_at(s, 0xd2) == cap + 1);
        CHECK(r.w.score == score + 4000u);
        CHECK(r.w.rating == score + 4000u);
        // One sound, 0x23 from the icon; the message 0x55 to the capturer from the ambulance.
        CHECK(r.cap.sounds == std::vector<Sound>{{0x23, 0, 1, 1}});
        CHECK(r.cap.plays.empty());
        CHECK(r.cap.msgs == std::vector<Msg>{{0, 7, 0, 0x55}});
        // The icon: state 1, dissolve counter 11, +0x48 = the capturing ship, the first step at once.
        CHECK(r.icon(1).state == 1);
        CHECK(r.icon(1).race_status == 11);
        CHECK(r.icon(1).blast_size == 0);
        CHECK(r.cap.dissolves == std::vector<int>{1});
        CHECK(r.cap.erases == 1);
        CHECK(r.w.icons_b == 7);
        CHECK(s.w82 == 35);  // seg13:0x4eec(ship, 35) from 0
    }
}

TEST_CASE("capture: the sound choice [BYTES seg13:0x1bba-0x1c04]") {
    CHECK(capture_sound(0, 0, 0) == 0x23);
    CHECK(capture_sound(0, 1, 0) == 0x26);  // a `fixed` icon without defenders
    CHECK(capture_sound(1, 0, 0) == 0x26);
    CHECK(capture_sound(2, 0, 0) == 0x66);
    CHECK(capture_sound(5, 1, 6) == 0x23);  // a race
    CHECK(capture_sound(0, 1, 6) == 0x23);
}

TEST_CASE("capture: defenders change hands, sound 0x69 first, score (n + 4)·1000 [BYTES seg13:0x1b5c-0x1c3e]") {
    Rig r;
    r.w.pools.set_in_use(2, 3);
    r.cg.defenders.resize(3);
    r.cg.defenders[0] = DefenderEntry{0, 0, 1, 0, 0};  // the icon's
    r.cg.defenders[1] = DefenderEntry{0, 0, 2, 0, 0};  // another icon's
    r.cg.defenders[2] = DefenderEntry{0, 0, 1, 0, 0};
    touch(r.c, 0, pool_rec(kPoolIcons, 1));
    CHECK(r.w.w9602 == 2);
    CHECK(r.cg.defenders[0].owner_pool == kPoolShips);
    CHECK(r.cg.defenders[1].owner_pool == 0);
    CHECK(r.cg.defenders[2].owner_rec == 0);
    CHECK(r.cap.plays == std::vector<int>{0x69});
    CHECK(r.cap.sounds == std::vector<Sound>{{0x66, 0, 1, 1}});
    CHECK(r.w.score == 6000u);
}

TEST_CASE("capture: the counters, DGROUP:0x91fc and the directory record [BYTES seg7:0x5a46 seg7:0x5ae8]") {
    Rig r;
    CHECK(r.w.icons_b == 8);
    touch(r.c, 0, pool_rec(kPoolIcons, 3));
    // The capture recounts while the icon dissolves: 7 live icons.
    CHECK(r.w.icons_b == 7);
    CHECK(r.w.w945c == 1);
    CHECK(r.w.dir().word(0x150) == 1);
    CHECK(r.w.dir().icon_taken(13));
    CHECK_FALSE(r.w.dir().icon_taken(12));
    CHECK(r.w.dir().byte(0x1d0) == 0);
    // The other seven: the last one completes the record.
    for (int i : {0, 1, 2, 4, 5, 6, 7}) touch(r.c, 0, pool_rec(kPoolIcons, i));
    CHECK(r.w.icons_b == 0);
    CHECK(r.w.dir().word(0x150) == 8);
    CHECK(r.w.dir().byte(0x1d0) == 2);
    // A destroyed icon counts to its killer ship.
    Rig d;
    d.w.icons[2].b0a = kPoolShips;
    d.w.icons[2].b0b = 0;
    d.icon(2).state = 1;
    icon_counted(d.w, 2, 0, kPoolShips);
    CHECK(d.w.w945e == 1);
    CHECK(d.w.dir().word(0x14e) == 1);
    CHECK(word_at(d.w.ships[0], 0xd0) == 1);
}

TEST_CASE("capture: the dissolve step and its length per Detail [BYTES seg6:0xdaa seg7:0x3a63]") {
    CHECK(dissolve_step_for(0, 0) == 3);
    CHECK(dissolve_step_for(1, 0) == 2);
    CHECK(dissolve_step_for(2, 0) == 1);
    CHECK(dissolve_step_for(3, 0) == 1);
    CHECK(dissolve_step_for(4, 2) == 2);  // no write for other values
    // seg7:0x3a42's calls from 11 (the touch's first, then one per frame while +0x51 != 0): the
    // byte minus the step, floored at 0 (s2-capture.md 2e).
    const int expected[4] = {4, 6, 11, 11};
    for (int detail = 0; detail < 4; ++detail) {
        const std::uint8_t step = static_cast<std::uint8_t>(dissolve_step_for(static_cast<std::int16_t>(detail), 0));
        std::uint8_t b = 11;
        int calls = 0;
        do {
            ++calls;
            b = static_cast<std::uint8_t>(b - step);
            if (static_cast<std::int8_t>(b) < 0) b = 0;
        } while (b != 0);
        CHECK(calls == expected[detail]);
    }
}

TEST_CASE("capture: a dead ship's touch explodes it only when the caller is not itself [BYTES seg13:0x16fc-0x1726 seg8:0x461b]") {
    // From the ship mover: DGROUP:0x943a / 0x943c = the ship itself, no explosion.
    {
        Rig r;
        r.w.ships[0].bec = 0;
        r.w.ships[0].lockout.set(r.g.now);  // past +0x84 - 5500
        r.a = kPoolShips;
        r.b = 0;
        touch(r.c, 0, pool_rec(kPoolIcons, 1));
        CHECK(r.ch.destroyed.empty());
        CHECK(r.icon(1).state == 0);
        CHECK(r.cap.erases == 0);
    }
    // From the icon mover: DGROUP:0x943a = 0, the dead ship explodes.
    {
        Rig r;
        r.w.ships[0].bec = 0;
        r.w.ships[0].lockout.set(r.g.now);
        r.a = 0;
        r.b = 1;
        touch(r.c, 0, pool_rec(kPoolIcons, 1));
        CHECK(r.ch.destroyed == std::vector<std::pair<int, int>>{{kPoolShips, 0}});
        CHECK(r.icon(1).state == 0);
    }
}

TEST_CASE("capture: the early exits [BYTES seg13:0x16d7-0x1752]") {
    Rig r;
    r.icon(1).b53 = 0;
    touch(r.c, 0, pool_rec(kPoolIcons, 1));
    r.icon(1).b53 = 1;
    r.w.ships[0].bea = 1;
    touch(r.c, 0, pool_rec(kPoolIcons, 1));
    r.w.ships[0].bea = 0;
    r.icon(1).flame = 0xfe;  // a frozen icon
    touch(r.c, 0, pool_rec(kPoolIcons, 1));
    CHECK(r.cap.erases == 0);
    CHECK(r.icon(1).state == 0);
    // A non-capturing design: the PatBlt, the push and the player's +0x156, but no capture.
    r.icon(1).flame = 0;
    r.w.design_ratings[0].captures = 0;
    touch(r.c, 0, pool_rec(kPoolIcons, 1));
    CHECK(r.cap.erases == 1);
    CHECK(r.icon(1).state == 0);
    CHECK(r.fh.sprites == 1);  // seg13:0x6716 for the player
    CHECK(r.w.ships[0].due156.get() == r.g.now - 1u);
}

TEST_CASE("capture: a virused icon [BYTES seg13:0x18d7-0x1ad6]") {
    Rig r;
    r.w.ships[0].set_resources(150);
    r.icon(1).b53 = 2;
    r.g.play_mode = 0;
    // A gate post's beam (pool 8, side kind 2) is the nearest goal.
    r.w.pools.set_in_use(8, 1);
    r.w.pool8[0].at(0x1c) = 2;
    r.w.pools.rec(8, 0).x = 800;
    touch(r.c, 0, pool_rec(kPoolIcons, 1));
    CHECK(r.w.ships[0].resources() == 50);
    CHECK(r.cap.msgs == std::vector<Msg>{{0, 7, 0, 0x48}});
    CHECK(r.cap.sounds.empty());
    CHECK(r.w.score == 0u);
    CHECK(r.g.play_mode == 2);
    CHECK(r.g.demo == 0);
    CHECK(r.w.w8630.empty());  // PlayMode 2: no strings
    CHECK(r.w.ships[0].w32 == 8);
    CHECK(r.w.ships[0].w34 == 0);
    CHECK(r.w.ships[0].w3c == 2);
    CHECK(ship_dword(r.w.ships[0], 0x94) == r.g.now + 15000u);
    CHECK(r.w.w8690 == r.g.now + 7500u);
    // From PlayMode 1: 3, with strings 1700 + 1701.
    Rig q;
    q.icon(1).b53 = 2;
    q.g.play_mode = 1;
    touch(q.c, 0, pool_rec(kPoolIcons, 1));
    CHECK(q.g.play_mode == 3);
    CHECK(q.w.w8630 == "Out Of Control. ThatIcon Didn't Taste So Good!");
    CHECK(q.w.w058c == 1);
    CHECK(q.w.w05c0 == q.g.now + 1200u);
    CHECK(q.w.ships[0].resources() == 0);
    // The tail is skipped: +0x52 stays.
    CHECK(q.w.ships[0].w52 == 0);
}

TEST_CASE("capture: the tail sets +0x52 = 7 for the player when PlayMode != 0 [BYTES seg13:0x1f98-0x1fb2]") {
    Rig r;
    r.g.play_mode = 1;
    touch(r.c, 0, pool_rec(kPoolIcons, 1));
    CHECK(r.w.ships[0].w52 == 7);
    Rig q;
    touch(q.c, 0, pool_rec(kPoolIcons, 1));
    CHECK(q.w.ships[0].w52 == 0);
}

TEST_CASE("capture: the fuel tank [BYTES seg13:0x1c42-0x1ce8]") {
    Rig r;
    r.w.pools.set_in_use(4, 1);
    ObjectRecord& t = r.w.pools.rec(4, 0);
    t.b53 = 1;
    t.size = 20;
    r.w.ships[0].fuel = 900;
    touch(r.c, 0, pool_rec(4, 0));
    CHECK(r.w.ships[0].fuel == 1000);  // min(tank, fuel + 250)
    CHECK(t.state == 1);
    CHECK(t.race_status == 10);
    CHECK(t.blast_size == 0);
    CHECK(r.cap.sounds == std::vector<Sound>{{0x71, kPoolShips, 0, 1}});
    CHECK(r.w.w698c == r.g.now + 10000u);
    CHECK(r.w.ships[0].w82 == 30);
}

TEST_CASE("capture: a pool-6 record, the writes behind W(w)+0x42, the sound with DGROUP:0x9324 [BYTES seg13:0x1e8d-0x1f32]") {
    Rig r;
    r.g.ship_pool = 20;  // the seg18 roles: the sounds pass DGROUP:0x9324 (verified claim 30)
    r.w.pools.resize(20, 1);
    r.w.pools.set_in_use(20, 1);
    r.w.pools.rec(20, 0) = r.w.pools.rec(kPoolShips, 0);
    r.w.pools.set_in_use(6, 1);
    ObjectRecord& m = r.w.pools.rec(6, 0);
    m.b53 = 1;
    r.w.pools.missile(0).weapon = 4;
    r.cap.w24 = 6;
    r.w.ships[0].fuel = 100;
    touch(r.c, 0, pool_rec(6, 0));
    CHECK(r.cap.sounds == std::vector<Sound>{{0x23, 20, 0, 1}});
    CHECK(r.w.ships[0].fuel == 100);  // W(w)+0x42 = 0: none of the three writes
    CHECK(r.w.ships[0].be2 == 0);
    CHECK(m.state == 1);
    CHECK(m.race_status == 10);
    CHECK(r.w.ships[0].w82 == 50);
    Rig q;
    q.w.pools.set_in_use(6, 1);
    q.w.pools.rec(6, 0).b53 = 1;
    q.cap.w42 = 1;
    q.w.ships[0].fuel = 100;
    touch(q.c, 0, pool_rec(6, 0));
    CHECK(q.w.ships[0].fuel == 120);
    CHECK(q.w.ships[0].be2 == 1);
    CHECK(q.w.ships[0].lockout.get() == q.g.now + 1200u);
    CHECK(q.w.pools.rec(kPoolShips, 0).angle == 1);
    CHECK(q.cap.sounds == std::vector<Sound>{{0x71, kPoolShips, 0, 1}});
}

TEST_CASE("capture: seg8:0x7d70 pushes the object off the ship [BYTES seg8:0x7d70-0x8046]") {
    ObjectRecord s{}, o{};
    s.size = 32, s.scr_x = 100, s.scr_y = 100;
    o.size = 32, o.scr_x = 120, o.scr_y = 100;
    // Slow pair: v = ship v - 15·dx/d = -15; then px = (-dx + R)·100/|ux| = 12 beats py = 3200.
    push_out(s, o);
    CHECK(o.vx == -15);
    CHECK(o.vy == 0);
    CHECK(o.scr_x == 132);
    CHECK(o.scr_y == 100);
    CHECK(s.scr_x == 100);
    // Moving away fast: q = 100 > 15, nothing moves.
    ObjectRecord p{};
    p.size = 32, p.scr_x = 120, p.scr_y = 100, p.vx = 20;
    push_out(s, p);
    CHECK(p.vx == 20);
    CHECK(p.scr_x == 120);
}

TEST_CASE("capture: seg13:0x4eec, signed products, ldiv [BYTES seg13:0x4ef7-0x4f41]") {
    ShipRecord s{};
    s.w82 = 500;
    bump_w82(s, 35);
    CHECK(s.w82 == 517);
    s.w82 = 100;
    bump_w82(s, -10);
    CHECK(s.w82 == 90);
}

namespace {

// The icon mover with a pair finder that answers the player for icon 1.
class IconHost : public UpdateHost {
public:
    World* world = nullptr;
    std::vector<std::int16_t> asked;
    std::vector<std::tuple<std::int16_t, int>> drawn;
    std::vector<Sound> sounds;
    RecCaptureHost cap;
    World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
    std::uint16_t collide(std::int16_t pr) override {
        asked.push_back(pr);
        return pr == pool_rec(kPoolIcons, 1) ? static_cast<std::uint16_t>(pool_rec(kPoolShips, 0)) : 0xffffu;
    }
    void draw_sprite(std::int16_t pr, int, int slot, int) override { drawn.emplace_back(pr, slot); }
    CaptureHost& capture_host() override { return cap; }
    Blitter* blitter() override { return nullptr; }
};

}  // namespace

TEST_CASE("icon mover: the player captures through seg7:0x309c and seg7:0x4a38 [BYTES seg7:0x3143-0x3177 seg7:0x4a7f]") {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    is::shell::Rng rng;
    IconHost host;
    host.world = &w;
    g.now = 50000;
    g.player = 0;
    w.pools.set_in_use(kPoolShips, 1);
    w.pools.set_in_use(kPoolIcons, 3);
    w.icons_a = 3;
    w.design_ratings[0].captures = 1;
    w.ships[0].bec = 1;
    w.pools.rec(kPoolShips, 0).size = 33;
    w.pools.rec(kPoolShips, 0).mass = 160;
    for (int i = 0; i < 3; ++i) {
        ObjectRecord& o = w.pools.rec(kPoolIcons, i);
        o.size = 32, o.gap = 6, o.b53 = 1, o.on_screen = 1, o.mass = 100;
    }
    w.pools.rec(kPoolIcons, 2).on_screen = 0;
    // seg7:0x5a46 counts records 0 ... 7 whatever the in-use count: the unused ones are state 1.
    for (int i = 3; i < 8; ++i) w.pools.rec(kPoolIcons, i).state = 1;
    count_icons(w);
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    move_icons(c, w);
    // Icons 0 and 1 drawn from the strip (slot 13) at their cells; icon 2 off screen is not drawn.
    CHECK(host.drawn.size() == 2);
    CHECK(std::get<1>(host.drawn[0]) == kIconStripSlot);
    CHECK(w.pools.rec(kPoolIcons, 1).src_x == 44);  // (6 + 32)·1 + 6
    CHECK(host.asked == std::vector<std::int16_t>{pool_rec(kPoolIcons, 0), pool_rec(kPoolIcons, 1)});
    CHECK(f.dg_943a == 0);
    CHECK(f.dg_943c == 1);
    // The capture: state 1, the score, the value and the counters.
    CHECK(w.pools.rec(kPoolIcons, 1).state == 1);
    CHECK(w.pools.rec(kPoolIcons, 1).race_status == 11);
    CHECK(w.score == 4000u);
    CHECK(w.ships[0].resources() == 100);
    CHECK(w.icons_b == 2);
    CHECK(host.cap.sounds == std::vector<Sound>{{0x23, 0, 1, 1}});
    // A ship whose design does not capture only bounces.
    World w2;
    w2.pools.set_in_use(kPoolShips, 1);
    w2.pools.set_in_use(kPoolIcons, 2);
    w2.icons_a = 2;
    w2.ships[0].bec = 1;
    for (int i = 0; i < 2; ++i) {
        ObjectRecord& o = w2.pools.rec(kPoolIcons, i);
        o.size = 32, o.b53 = 1, o.on_screen = 1, o.mass = 100;
    }
    IconHost h2;
    h2.world = &w2;
    UpdateCtx c2{FlightCtx{w2.pools, w2.ships, g, keys, h2, rng}, f, view, h2, joy, jb};
    move_icons(c2, w2);
    CHECK(w2.pools.rec(kPoolIcons, 1).state == 0);
    CHECK(w2.score == 0u);
}

TEST_CASE("icon mover: an off-screen icon steps every third frame, a negative heat rises every 30th [BYTES seg7:0x317f seg7:0x3193-0x31c0]") {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    is::shell::Rng rng;
    IconHost host;
    host.world = &w;
    w.pools.set_in_use(kPoolIcons, 1);
    w.icons_a = 1;
    ObjectRecord& o = w.pools.rec(kPoolIcons, 0);
    o.vx = 4;
    o.x = 1000;
    o.flame = 0xff;  // heat -1
    o.b53 = 0;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    f.dg_9142 = 30;  // % 3 = 0 and % 30 = 0
    move_icons(c, w);
    CHECK(o.x == 1012);  // seg8:0x1b54(3, i)
    CHECK(o.flame == 0);
    CHECK(o.b53 == 1);
    f.dg_9142 = 31;
    move_icons(c, w);
    CHECK(o.x == 1012);
}

namespace {

class TailHost : public UpdateHost {
public:
    World* world = nullptr;
    int n551a = 0, n59ec = 0;
    World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
    std::uint16_t collide(std::int16_t) override { return 0xffffu; }
    void seg18_551a(int, int) override { ++n551a; }
    void seg18_59ec(int, int) override { ++n59ec; }
};

}  // namespace

TEST_CASE("ship mover: the weapon tail runs without a hit; an off-screen ship skips it [BYTES seg13:0xb69 seg13:0xc95-0xcf6]") {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    is::shell::Rng rng;
    TailHost host;
    host.world = &w;
    w.pools.set_in_use(kPoolShips, 1);
    w.ships[0].weapon_slot = 1;
    w.ships[0].weapon[1] = 0x1a;
    w.pools.rec(kPoolShips, 0).on_screen = 1;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    host.frame_ctx = &c;
    g.game_frames = 8;
    host.ship_collide(0);
    CHECK(host.n551a == 1);
    CHECK(host.n59ec == 0);  // no key down
    keys.key_down('Z', false);
    host.ship_collide(0);
    CHECK(host.n59ec == 1);
    g.game_frames = 9;
    host.ship_collide(0);
    CHECK(host.n59ec == 1);
    w.pools.rec(kPoolShips, 0).on_screen = 0;
    host.ship_collide(0);
    CHECK(host.n551a == 3);
    host.frame_ctx = nullptr;
}
