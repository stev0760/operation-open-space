// LIGHTNING (src/world/lightning.h; work/analysis/m4/s7-ring.md section 2 with s7-ring-verify.md:
// claims 10-28, 23 and 28 as corrected, Omissions O5-O14): the search seg11:0x9fdc (gate, range,
// pick, fee, the pool-6 launcher zap), the strike seg11:0xa234 (the amount, the dispatch, the
// charges, team 7, the virus's infection and its divide-by-zero guard of ruling R1), the bolt
// seg11:0xa4a6 (srand, pens, DGROUP:0x1016) and its three frame calls seg11:0xa9ea, and the
// visibility pass's discharge gates seg8:0x4ffb-0x50a0. The flash colour 0x00ffff7f is the one
// core-0009's frames show (127, 255, 255) [ORACLE core-0009].
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "doctest.h"
#include "shell/bolt.h"
#include "shell/rng.h"
#include "world/lightning.h"
#include "world/shots.h"
#include "world/visibility.h"

using namespace is::world;

namespace {

constexpr int kPoolBattery = 6;  // pool 6, the launched weapons (missile.h kPoolMissiles)

struct Dispatch {
    int tp, tr, xp, xr;
    std::int16_t amount;
    std::int16_t a943, c943;  // DGROUP:0x943a / 0x943c at the call
};

struct Host : LightningHost {
    std::vector<std::string> log;  // sounds, overlays, dispatches in order
    std::vector<Dispatch> hits;
    std::vector<int> bk;
    std::uint32_t time = 0x4321;
    std::int16_t team_word = 0;
    std::int16_t* a943 = nullptr;
    std::int16_t* c943 = nullptr;
    World* w = nullptr;
    std::int8_t set_target_charge = -128;  // a value the "on_damage" writes during the dispatch
    void sound(int id, int a, int b, int mode) override {
        log.push_back("sound " + std::to_string(id) + " " + std::to_string(a) + " " + std::to_string(b) + " " +
                      std::to_string(mode));
    }
    std::int16_t dispatch(int tp, int tr, int xp, int xr, std::int16_t amount) override {
        hits.push_back({tp, tr, xp, xr, amount, *a943, *c943});
        log.push_back("dispatch");
        if (set_target_charge != -128 && w) w->pools.rec(tp, tr).b50 = static_cast<std::uint8_t>(set_target_charge);
        return 0;
    }
    void icon_overlay(int icon, int k) override {
        log.push_back("overlay " + std::to_string(icon) + " " + std::to_string(k));
    }
    // A view: screen = ((x - 7000) >> 3, (y - 6000) >> 3).
    Point16 to_screen(std::int16_t x, std::int16_t y) override {
        return Point16{static_cast<std::int16_t>((x - 7000) >> 3), static_cast<std::int16_t>((y - 6000) >> 3)};
    }
    std::uint32_t current_time(std::uint32_t) override { return time; }
    void bk_mode(int mode) override { bk.push_back(mode); }
    std::int16_t team(const World&, int) override { return team_word; }
};

// The player (pool 14 record 0) carrying LIGHTNING in slot 1 at (8000, 8000), heading 0 (up),
// with charge 108; its one-barrel muzzle 16 units above its top-left.
struct Rig {
    World w;
    BoltState bolt;
    Host host;
    is::shell::Rng rng;
    WeaponWords words;
    ShotState shots;
    RecordingBlitter out;
    std::int16_t a943 = 0, c943 = 0;
    LightningCtx c{w, bolt, host, rng, words};
    Rig() {
        c.out = &out;
        c.dg_943a = &a943;
        c.dg_943c = &c943;
        c.shots = &shots;
        host.a943 = &a943;
        host.c943 = &c943;
        host.w = &w;
        words.set_word(kWeaponLightning, kWordLightningCost, 20);  // W(3)+0x2e [BYTES seg10:0x20bf]
        w.W = 640;
        w.H = 400;
        w.player = 0;
        ShipRecord& s = w.ships[0];
        s.slots_cb() = 2;
        s.weapon[1] = kWeaponLightning;
        s.fuel = 1000;
        s.design_index = 0;
        ObjectRecord& u = put(kPoolShips, 0, 8000, 8000, 325);
        u.frame = 0;
        u.b50 = 108;
        shots.mx(0, 0) = 0;
        shots.my(0, 0) = -2;
    }
    ObjectRecord& put(int pool, int rec, int x, int y, int mass) {
        ObjectRecord& o = w.pools.rec(pool, rec);
        o.x = static_cast<std::int16_t>(x);
        o.y = static_cast<std::int16_t>(y);
        o.mass = static_cast<std::int16_t>(mass);
        o.state = 0;
        if (w.pools.in_use(pool) <= rec) w.pools.set_in_use(pool, static_cast<std::int16_t>(rec + 1));
        return o;
    }
    ObjectRecord& player() { return w.pools.rec(kPoolShips, 0); }
};

int count_op(const RecordingBlitter& out, BlitCall::Op op) {
    int n = 0;
    for (const BlitCall& b : out.calls) n += b.op == op;
    return n;
}
std::vector<BlitCall> pens_created(const RecordingBlitter& out) {
    std::vector<BlitCall> v;
    for (const BlitCall& b : out.calls)
        if (b.op == BlitCall::kCreatePen) v.push_back(b);
    return v;
}

}  // namespace

TEST_CASE("lightning: the range R = min(4c, W / 2), at least 150 for the player (claim 11)") {
    CHECK(lightning_range(108, 640, true) == 320);
    CHECK(lightning_range(108, 641, true) == 320);
    CHECK(lightning_range(0, 640, true) == 150);  // O10: a player at charge 0 still reaches 150
    CHECK(lightning_range(64, 640, false) == 256);
    CHECK(lightning_range(64, 100, false) == 50);
    CHECK(lightning_range(-3, 640, false) == -12);
    CHECK(lightning_range(30, 640, true) == 150);
}

TEST_CASE("lightning: the pick is the heaviest object in range and in the frame's cone (claims 12, 13)") {
    Rig r;
    r.put(kPoolRocks, 0, 8000, 7200, 350);    // straight up, 100 cells
    r.put(kPoolRocks, 1, 8000, 6400, 1400);   // straight up, 200 cells
    r.put(kPoolIcons, 0, 8000, 7600, 160);    // straight up, 50 cells
    r.put(kPool5, 0, 8000, 7520, 5000);       // pool 5: skipped
    r.put(kPoolGateCentres, 0, 8000, 7440, 5000);  // pool 12: skipped
    r.put(kPoolRocks, 2, 8800, 8000, 3000);   // right: sector 4, out of the cone
    r.put(kPoolGatePosts, 0, 8000, 7000, 3500).state = 1;  // not in play
    r.put(kPoolRocks, 3, 8000, 4800, 3500);   // 400 cells: out of R = 320
    const SearchResult s = lightning_search(r.c, kPoolShips, 0);
    CHECK(s.acted);
    CHECK(s.range == 320);
    CHECK(s.pool == kPoolRocks);
    CHECK(s.rec == 1);
    CHECK(s.paid);
    CHECK(r.w.ships[0].fuel == 980);
    REQUIRE(r.host.hits.size() == 1);
    CHECK(r.host.hits[0].tp == kPoolRocks);
    CHECK(r.host.hits[0].tr == 1);
    // A tie keeps the first in scan order: an icon of 1400 (pool 0 comes first) now wins.
    Rig t;
    t.put(kPoolRocks, 1, 8000, 6400, 1400);
    t.put(kPoolIcons, 2, 8000, 7000, 1400);
    const SearchResult u = lightning_search(t.c, kPoolShips, 0);
    CHECK(u.pool == kPoolIcons);
    CHECK(u.rec == 2);
}

TEST_CASE("lightning: the cone h - 1 ... h + 1 has no wrap at 0 / 15 (claim 12)") {
    {
        Rig r;  // frame 0; a target in sector 15 (dx -30 cells, dy -100): out
        r.put(kPoolRocks, 0, 8000 - 240, 7200, 350);
        CHECK(direction16(-100, -30) == 15);
        CHECK(lightning_search(r.c, kPoolShips, 0).pool == -1);
        CHECK(r.w.ships[0].fuel == 1000);  // no pick: no fee, no strike, no sound
        CHECK(r.host.log.empty());
    }
    {
        Rig r;  // sector 1: in
        r.put(kPoolRocks, 0, 8000 + 240, 7200, 350);
        CHECK(direction16(-100, 30) == 1);
        CHECK(lightning_search(r.c, kPoolShips, 0).pool == kPoolRocks);
    }
    {
        Rig r;  // frame 15, a target straight up (sector 0): out
        r.player().frame = 15;
        r.put(kPoolRocks, 0, 8000, 7200, 350);
        CHECK(lightning_search(r.c, kPoolShips, 0).pool == -1);
    }
    {
        Rig r;  // a rock's spin frame 17 finds nothing
        r.player().frame = 17;
        r.put(kPoolRocks, 0, 8000, 7200, 350);
        CHECK(lightning_search(r.c, kPoolShips, 0).pool == -1);
    }
}

TEST_CASE("lightning: the gate and the fee (claims 10, 14; O9)") {
    {
        // Fuel 20 is not above W(3)+0x2e: the slot holding 3 lets the search go on, none other.
        Rig r;
        r.w.ships[0].fuel = 20;
        r.put(kPoolRocks, 0, 8000, 7200, 350);
        CHECK_FALSE(lightning_search(r.c, kPoolShips, 0).acted);
        CHECK(r.host.hits.empty());
    }
    {
        Rig r;
        r.w.ships[0].fuel = 21;
        r.put(kPoolRocks, 0, 8000, 7200, 350);
        CHECK(lightning_search(r.c, kPoolShips, 0).acted);
        CHECK(r.w.ships[0].fuel == 1);
    }
    {
        // No slot holding 3: nothing.
        Rig r;
        r.w.ships[0].weapon[1] = 1;
        r.put(kPoolRocks, 0, 8000, 7200, 350);
        CHECK_FALSE(lightning_search(r.c, kPoolShips, 0).acted);
    }
    {
        // A bolt pending (DGROUP:0x1016 != 0): the fee is paid before seg11:0xa234 refuses (O9).
        Rig r;
        r.bolt.pending = 4;
        r.put(kPoolRocks, 0, 8000, 7200, 350);
        const SearchResult s = lightning_search(r.c, kPoolShips, 0);
        CHECK(s.paid);
        CHECK(r.w.ships[0].fuel == 980);
        CHECK(r.host.log.empty());
        CHECK(r.bolt.pending == 4);
    }
    {
        // The fee is the literal pool 14's, floored at 0 (`jns`): with DGROUP:0x9324 = 20 (the
        // seg18 roles) the pool-14 user passes no gate and pays from fuel 5 down to 0.
        Rig r;
        r.w.roles.ships = 20;
        r.w.ships[0].fuel = 5;
        r.put(kPoolRocks, 0, 8000, 7200, 350);
        const SearchResult s = lightning_search(r.c, kPoolShips, 0);
        CHECK(s.acted);
        CHECK(s.range == 320);  // not the player's floor: the user is not in DGROUP:0x9324
        CHECK(s.paid);
        CHECK(r.w.ships[0].fuel == 0);
    }
}

TEST_CASE("lightning: a battery missile's launcher zap (claim 15)") {
    Rig r;
    r.words.set_word(14, 0x1e, 300);  // W(14)+0x1e, the lifetime
    ObjectRecord& m = r.put(kPoolBattery, 0, 8000, 9600, 20);  // 200 cells below the player
    m.b50 = 30;   // R = 120... too short: the player is 200 cells away
    m.frame = 0;  // facing up, toward the player
    MissileSlot& e = r.w.pools.missile(0);
    e.weapon = 14;
    e.launcher_pool = kPoolShips;
    e.launcher_rec = 0;
    e.age = 40;
    CHECK(lightning_search(r.c, kPoolBattery, 0).pool == -1);
    CHECK(e.age == 40);
    m.b50 = 60;  // R = 240: the player, its launcher, is the pick
    const SearchResult s = lightning_search(r.c, kPoolBattery, 0);
    CHECK(s.pool == kPoolShips);
    CHECK(s.rec == 0);
    CHECK_FALSE(s.paid);
    CHECK(r.w.ships[0].fuel == 1000);
    CHECK(e.age == 299);  // W(id)+0x1e - 1
    REQUIRE(r.host.hits.size() == 1);
    CHECK(r.host.hits[0].tp == kPoolShips);
    CHECK(r.host.hits[0].amount == 2030);
}

TEST_CASE("lightning: the player's strike: sound, bolt, dispatch and the charges (claims 16-18, 23, 24)") {
    Rig r;
    ObjectRecord& rock = r.put(kPoolRocks, 0, 8000, 6400, 1400);
    rock.extent = 10;
    const StrikeOutcome o = lightning_strike(r.c, kPoolShips, 0, kPoolRocks, 0);
    CHECK(o == StrikeOutcome::kStruck);
    CHECK(r.bolt.src_pool == kPoolShips);
    CHECK(r.bolt.src_rec == 0);
    REQUIRE(r.host.log.size() == 2);
    CHECK(r.host.log[0] == "sound 34 14 0 1");  // 0x22 at the source
    CHECK(r.host.log[1] == "dispatch");
    REQUIRE(r.host.hits.size() == 1);
    const Dispatch& d = r.host.hits[0];
    CHECK(d.tp == kPoolRocks);
    CHECK(d.tr == 0);
    CHECK(d.xp == kPoolShips);
    CHECK(d.xr == 0);
    CHECK(d.amount == 2054);  // 108 / 2 + 2000: the dispatcher deals 54 with its flag
    CHECK(d.a943 == kPoolShips);
    CHECK(d.c943 == 0);
    CHECK(rock.b50 == 64);           // lo8(3·108 / 5)
    CHECK(r.player().b50 == 72);     // 108 - 108 / 3
    // The bolt: from the muzzle (8000, 7984) to the rock's centre (8005, 6405), on the screen
    // (125, 248) to (125, 50).
    REQUIRE(!r.bolt.shape.main.empty());
    CHECK(r.bolt.shape.main.front() == is::shell::BoltPoint{125, 248});
    CHECK(r.bolt.shape.main.back() == is::shell::BoltPoint{125, 50});
    const int n = static_cast<int>(r.bolt.shape.main.size()) - 1;
    CHECK(r.bolt.pending == n + 1);
    // The pens: 0x00ffffc3 at widths 2 then 1, each selected out and deleted.
    const auto pens = pens_created(r.out);
    REQUIRE(pens.size() == 2);
    CHECK(pens[0] == create_pen_call(kPsSolid, 2, kBoltStrikePen, pens[0].pen));
    CHECK(pens[1] == create_pen_call(kPsSolid, 1, kBoltStrikePen, pens[1].pen));
    CHECK(count_op(r.out, BlitCall::kDeletePen) == 2);
    REQUIRE(r.out.calls.size() >= 2);
    CHECK(r.out.calls[1] == select_pen_call(kDcWindow, pens[0].pen));
    CHECK(count_op(r.out, BlitCall::kLineTo) >= n);  // the main line and the branches inside the clip
}

TEST_CASE("lightning: the amount mapping and a rock's full discharge (claims 21, 23, 24; O10)") {
    struct Case {
        int charge;
        std::int16_t amount;
    };
    for (const Case k : {Case{108, 2054}, Case{3, 2001}, Case{1, 2000}, Case{0, 2000}, Case{-1, 2000}, Case{-4, 1998}}) {
        CAPTURE(k.charge);
        Rig r;
        r.player().b50 = static_cast<std::uint8_t>(k.charge);
        r.put(kPoolRocks, 0, 8000, 7200, 350);
        lightning_strike(r.c, kPoolShips, 0, kPoolRocks, 0);
        REQUIRE(r.host.hits.size() == 1);
        CHECK(r.host.hits[0].amount == k.amount);
    }
    // A charged rock strikes: A = 64, all of it leaves the rock; the target takes 38.
    Rig r;
    ObjectRecord& src = r.put(kPoolRocks, 0, 8000, 7200, 1400);
    src.b50 = 64;
    src.extent = 6;
    ObjectRecord& dst = r.put(kPoolRocks, 1, 8000, 6400, 350);
    lightning_strike(r.c, kPoolRocks, 0, kPoolRocks, 1);
    REQUIRE(r.host.hits.size() == 1);
    CHECK(r.host.hits[0].amount == 2032);
    CHECK(dst.b50 == 38);
    CHECK(src.b50 == 0);
    // Its bolt starts at the rock's centre (x + extent / 2): (8003, 7203) on the screen (125, 150).
    CHECK(r.bolt.shape.main.front() == is::shell::BoltPoint{125, 150});
    // The rock chain of s7-ring.md: 64 -> 38 -> 22 -> 13 -> 7 -> 4 -> 2 -> 1 -> 0.
    std::int16_t c = 64;
    const std::int16_t chain[8] = {38, 22, 13, 7, 4, 2, 1, 0};
    for (std::int16_t next : chain) {
        c = static_cast<std::int16_t>((3 * c) / 5);
        CHECK(c == next);
    }
}

TEST_CASE("lightning: a ship target keeps its charge; a tank's is rewritten after the dispatch (claim 24, O11)") {
    {
        Rig r;
        ObjectRecord& src = r.put(kPoolRocks, 0, 8000, 7200, 1400);
        src.b50 = 50;
        r.player().b50 = 9;
        lightning_strike(r.c, kPoolRocks, 0, kPoolShips, 0);
        CHECK(r.player().b50 == 9);
        CHECK(src.b50 == 0);
    }
    {
        Rig r;
        ObjectRecord& src = r.put(kPoolRocks, 0, 8000, 7200, 1400);
        src.b50 = 50;
        ObjectRecord& tank = r.put(kPool4, 0, 8000, 6400, 100);
        r.host.set_target_charge = 5;  // the tank's on_damage: max(1, damage / 10)
        lightning_strike(r.c, kPoolRocks, 0, kPool4, 0);
        CHECK(tank.b50 == 30);  // lo8(3·50 / 5), written after the dispatch
    }
}

TEST_CASE("lightning: a team-7 ship source clears both charges (claim 25)") {
    Rig r;
    r.host.team_word = 7;
    ObjectRecord& rock = r.put(kPoolRocks, 0, 8000, 7200, 350);
    lightning_strike(r.c, kPoolShips, 0, kPoolRocks, 0);
    CHECK(rock.b50 == 0);
    CHECK(r.player().b50 == 0);
    // A ship target too.
    Rig s;
    s.host.team_word = 7;
    ObjectRecord& other = s.put(kPoolShips, 1, 8000, 7200, 300);
    other.b50 = 40;
    s.w.ships[1].design_index = 1;
    lightning_strike(s.c, kPoolShips, 0, kPoolShips, 1);
    CHECK(other.b50 == 0);
    CHECK(s.player().b50 == 0);
}

TEST_CASE("lightning: refusals: a pending bolt and a bad record (claims 16, 17)") {
    Rig r;
    r.put(kPoolRocks, 0, 8000, 7200, 350);
    r.bolt.pending = -3;
    r.bolt.src_pool = 2;
    r.bolt.src_rec = 5;
    CHECK(lightning_strike(r.c, kPoolShips, 0, kPoolRocks, 0) == StrikeOutcome::kPending);
    CHECK(r.bolt.src_pool == 2);
    CHECK(r.bolt.src_rec == 5);
    CHECK(r.host.log.empty());
    r.bolt.pending = 0;
    const std::int16_t bad = r.w.pools.bad_index_count;
    CHECK(lightning_strike(r.c, kPoolRocks, 4, kPoolRocks, 0) == StrikeOutcome::kBadRecord);
    CHECK(r.w.pools.bad_index_count == bad + 1);
    CHECK(r.bolt.src_pool == kPoolRocks);  // DGROUP:0x951e / 0x9520 are written before the test
    CHECK(r.bolt.src_rec == 4);
    CHECK(r.host.log.empty());
}

TEST_CASE("lightning: a virus bolt: its rand() after the bolt's, the icon's infection, the amount") {
    Rig r;
    ObjectRecord& virus = r.put(kPool9, 0, 8000, 8000, 50);
    virus.b50 = 50;
    virus.extent = 0;
    r.put(kPoolIcons, 0, 8000, 7200, 160);
    lightning_strike(r.c, kPool9, 0, kPoolIcons, 0);
    // The same draws from a fresh generator: srand(time), the bolt from (125, 250) to (125, 150),
    // then the virus's rand().
    is::shell::Rng ref;
    ref.srand(static_cast<std::uint16_t>(r.host.time & 0xffff));
    is::shell::BasicBolt<is::shell::BoltPoint> b;
    REQUIRE(is::shell::make_bolt(ref, 125, 250, 125, 150, &b));
    const int rnd = ref.rand();
    const std::int16_t A = static_cast<std::int16_t>(rnd % 25 + 16);  // r % (50 / 2) + 50 / 3
    CHECK(r.rng.state() == ref.state());
    REQUIRE(r.host.log.size() == 3);
    CHECK(r.host.log[0] == "sound 34 9 0 1");
    CHECK(r.host.log[1] == "overlay 0 0");  // seg7:0x2512(icon, 0) before the dispatch
    CHECK(r.host.log[2] == "dispatch");
    CHECK(r.host.hits[0].amount == A / 2 + 2000);
    CHECK(r.w.pools.rec(kPoolIcons, 0).b50 == static_cast<std::uint8_t>((3 * A) / 5));
    CHECK(virus.b50 == static_cast<std::uint8_t>(50 - A));
    // Green pens for a pool-9 source.
    for (const BlitCall& p : pens_created(r.out)) CHECK(p.rop == kBoltVirusPen);
    // A virus at charge 2: A = r % 1 + 0 = 0, a plain 2000, the charge stays 2 (O8).
    Rig t;
    t.put(kPool9, 0, 8000, 8000, 50).b50 = 2;
    t.put(kPoolRocks, 0, 8000, 7200, 350);
    lightning_strike(t.c, kPool9, 0, kPoolRocks, 0);
    REQUIRE(t.host.hits.size() == 1);
    CHECK(t.host.hits[0].amount == 2000);
    CHECK(t.w.pools.rec(kPool9, 0).b50 == 2);
}

TEST_CASE("lightning: a virus with charge -1, 0 or 1 skips the faulting division (ruling R1, O8)") {
    std::vector<std::string> logged;
    set_guard_todo([&](const std::string& what, const char* address) { logged.push_back(what + " " + address); });
    for (int ch : {1, 0, -1}) {
        CAPTURE(ch);
        Rig r;
        ObjectRecord& virus = r.put(kPool9, 0, 8000, 8000, 50);
        virus.b50 = static_cast<std::uint8_t>(ch);
        ObjectRecord& icon = r.put(kPoolIcons, 0, 8000, 7200, 160);
        icon.b50 = 7;
        CHECK(lightning_strike(r.c, kPool9, 0, kPoolIcons, 0) == StrikeOutcome::kGuarded);
        CHECK(r.host.hits.empty());           // no dispatch
        CHECK(icon.b50 == 7);                 // the target's charge unchanged
        CHECK(virus.b50 == static_cast<std::uint8_t>(ch));  // the source's unchanged
        REQUIRE(r.host.log.size() == 2);
        CHECK(r.host.log[1] == "overlay 0 0");  // the infection takes no A
        CHECK(r.a943 == kPool9);              // DGROUP:0x943a / 0x943c take no A either
        CHECK(r.bolt.pending > 0);            // the bolt was drawn before
        // The virus's rand() was drawn: one past the bolt's.
        is::shell::Rng ref;
        ref.srand(static_cast<std::uint16_t>(r.host.time & 0xffff));
        is::shell::BasicBolt<is::shell::BoltPoint> b;
        is::shell::make_bolt(ref, 125, 250, 125, 150, &b);
        ref.rand();
        CHECK(r.rng.state() == ref.state());
    }
    // Logged once for the site.
    CHECK(logged.size() == 1);
    if (!logged.empty()) CHECK(logged[0].find("seg11:0xa3a3") != std::string::npos);
    set_guard_todo(nullptr);
    bool ok = true;
    CHECK(strike_amount(kPool9, 1, 1234, &ok) == 0);
    CHECK_FALSE(ok);
    CHECK(strike_amount(kPool9, -128, 100, &ok) == 100 % -64 + -42);
    CHECK(ok);
    CHECK(strike_amount(kPoolRocks, -5, 0, &ok) == -5);
}

TEST_CASE("lightning: the bolt's three frame calls (claim 28 as corrected, O14)") {
    Rig r;
    r.put(kPoolRocks, 0, 8000, 6400, 1400);
    lightning_strike(r.c, kPoolShips, 0, kPoolRocks, 0);
    const std::int16_t D = r.bolt.pending;
    REQUIRE(D > 1);
    const int n = D - 1;
    // Call 1: black erase, the branches again with the stock BLACK_PEN, D negated.
    r.out.calls.clear();
    r.host.log.clear();
    bolt_frame(r.c, 1);
    CHECK(r.bolt.pending == -D);
    CHECK(r.bolt.flashed == 0);
    CHECK(r.host.log.empty());
    auto pens = pens_created(r.out);
    REQUIRE(pens.size() == 2);
    CHECK(pens[0].w == 2);
    CHECK(pens[0].rop == kBoltErasePen);
    CHECK(pens[1].w == 1);
    CHECK(pens[1].rop == kBoltErasePen);
    // GetStockObject(7) selected (even with no branch to draw), not deleted.
    bool stock = false;
    for (const BlitCall& b : r.out.calls) stock |= b == select_pen_call(kDcWindow, kPenBlack);
    CHECK(stock);
    CHECK(count_op(r.out, BlitCall::kDeletePen) == 2);
    // Call 2: the second 0x22 at the source (DGROUP:0x1318 <= 1), the flash 0x00ffff7f, F = 1.
    r.out.calls.clear();
    bolt_frame(r.c, 1);
    REQUIRE(r.host.log.size() == 1);
    CHECK(r.host.log[0] == "sound 34 14 0 1");
    CHECK(r.bolt.pending == D);
    CHECK(r.bolt.flashed == 1);
    pens = pens_created(r.out);
    REQUIRE(pens.size() == 2);
    CHECK(pens[0].rop == kBoltFlashPen);
    CHECK(pens[1].rop == kBoltFlashPen);
    CHECK(count_op(r.out, BlitCall::kLineTo) >= n);
    // Call 3: black again, both words cleared.
    r.out.calls.clear();
    bolt_frame(r.c, 1);
    CHECK(r.bolt.pending == 0);
    CHECK(r.bolt.flashed == 0);
    CHECK(pens_created(r.out)[0].rop == kBoltErasePen);
    CHECK(r.host.log.size() == 1);
    CHECK(r.host.bk == std::vector<int>{2, 2, 2});  // SetBkMode(DGROUP:0x13dc, 2) each call
    // A new strike is accepted again.
    CHECK(lightning_strike(r.c, kPoolShips, 0, kPoolRocks, 0) == StrikeOutcome::kStruck);
}

TEST_CASE("lightning: the second thunder at the bolt's first point when DGROUP:0x1318 > 1; a virus's green flash; arg 0") {
    {
        Rig r;
        r.w.w1318 = 2;
        r.put(kPoolRocks, 0, 8000, 6400, 1400);
        lightning_strike(r.c, kPoolShips, 0, kPoolRocks, 0);
        r.host.log.clear();
        bolt_frame(r.c, 1);
        bolt_frame(r.c, 1);
        REQUIRE(r.host.log.size() == 1);
        CHECK(r.host.log[0] == "sound 34 125 248 4");  // dword DGROUP:0x6b5c, mode 4
    }
    {
        Rig r;
        r.put(kPool9, 0, 8000, 8000, 50).b50 = 50;
        r.put(kPoolRocks, 0, 8000, 7200, 350);
        lightning_strike(r.c, kPool9, 0, kPoolRocks, 0);
        bolt_frame(r.c, 1);
        r.out.calls.clear();
        bolt_frame(r.c, 1);
        CHECK(pens_created(r.out)[0].rop == kBoltVirusPen);
    }
    {
        // arg 0 (the intro's call): no sound, the flash colour even for a pool-9 source.
        Rig r;
        r.bolt.pending = -2;
        r.bolt.src_pool = kPool9;
        r.bolt.shape.main = {{10, 10}, {20, 20}};
        bolt_frame(r.c, 0);
        CHECK(r.host.log.empty());
        CHECK(pens_created(r.out)[0].rop == kBoltFlashPen);
        CHECK(r.bolt.flashed == 1);
        CHECK(r.bolt.pending == 2);
    }
}

TEST_CASE("lightning: DGROUP:0x1036 left at 1 ends the next bolt after one call (O13)") {
    Rig r;
    r.bolt.flashed = 1;  // a bolt cut between its second and third call
    r.put(kPoolRocks, 0, 8000, 6400, 1400);
    lightning_strike(r.c, kPoolShips, 0, kPoolRocks, 0);
    REQUIRE(r.bolt.pending > 0);
    r.host.log.clear();
    r.out.calls.clear();
    bolt_frame(r.c, 1);
    CHECK(r.bolt.pending == 0);
    CHECK(r.bolt.flashed == 0);
    CHECK(r.host.log.empty());  // no flash, no second thunder
    for (const BlitCall& p : pens_created(r.out)) CHECK(p.rop == kBoltErasePen);
}

TEST_CASE("lightning: srand on every path; a bolt too long draws nothing and the strike goes on (claims 19, 20)") {
    Rig r;
    r.rng.srand(999);
    r.rng.rand();
    // The target 1200 px below the muzzle on the screen (248 -> 1448): L = 1200, L / 50 > 22.
    r.put(kPoolRocks, 0, 8000, 6000 + 8 * 1448, 350);
    CHECK(lightning_strike(r.c, kPoolShips, 0, kPoolRocks, 0) == StrikeOutcome::kStruck);
    CHECK(r.bolt.pending == 0);
    CHECK(r.out.calls.empty());
    CHECK(r.rng.state() == (r.host.time & 0xffffu));  // reseeded, no draw before the exit
    REQUIRE(r.host.hits.size() == 1);  // the dispatch happens with no picture
    // The same time gives the same bolt.
    Rig a, b;
    a.put(kPoolRocks, 0, 8000, 6400, 1400);
    b.put(kPoolRocks, 0, 8000, 6400, 1400);
    b.rng.srand(77);
    lightning_strike(a.c, kPoolShips, 0, kPoolRocks, 0);
    lightning_strike(b.c, kPoolShips, 0, kPoolRocks, 0);
    CHECK(a.bolt.shape.main == b.bolt.shape.main);
    CHECK(a.rng.state() == b.rng.state());
}

TEST_CASE("lightning: the visibility pass's discharge gates (claim 27, O7)") {
    // A seed whose first rand() % 9 is 0, and one whose is not.
    std::uint16_t yes = 0, no = 0;
    for (std::uint16_t s = 1; s < 1000 && (yes == 0 || no == 0); ++s) {
        is::shell::Rng t;
        t.srand(s);
        if (t.rand() % 9 == 0) {
            if (yes == 0) yes = s;
        } else if (no == 0) {
            no = s;
        }
    }
    REQUIRE(yes != 0);
    REQUIRE(no != 0);
    auto run = [](Pools& p, std::uint16_t seed, std::vector<std::pair<int, int>>& calls) {
        ViewState v;
        v.active[0] = ViewRect{0, 0, 25600, 25600};
        v.active_count = 1;
        VisibilityGlobals g;
        VisibilityHost h;
        h.seg11_9fdc = [&](int pool, int rec) { calls.push_back({pool, rec}); };
        is::shell::Rng rng;
        rng.srand(seed);
        visibility(p, v, g, rng, h);
    };
    auto pools = [] {
        Pools p;
        for (int i = 0; i < 3; ++i) {
            ObjectRecord& o = p.rec(kPoolRocks, i);
            o.x = static_cast<std::int16_t>(1000 + 100 * i);
            o.y = 1000;
            o.mass = 350;
        }
        p.set_in_use(kPoolRocks, 3);
        p.rec(kPoolRocks, 1).b50 = 20;  // a struck rock
        p.rec(kPoolShips, 0).x = 1500;
        p.rec(kPoolShips, 0).y = 1500;
        p.rec(kPoolShips, 0).mass = 325;
        p.rec(kPoolShips, 0).b50 = 108;  // the ship pool never discharges here
        p.set_in_use(kPoolShips, 1);
        return p;
    };
    {
        Pools p = pools();
        std::vector<std::pair<int, int>> calls;
        run(p, yes, calls);
        REQUIRE(calls.size() == 1);
        CHECK(calls[0] == std::make_pair(static_cast<int>(kPoolRocks), 1));
    }
    {
        Pools p = pools();
        std::vector<std::pair<int, int>> calls;
        run(p, no, calls);
        CHECK(calls.empty());
    }
    {
        // A tank's wobble rate counts as charge: any kind discharges.
        Pools p = pools();
        p.rec(kPool4, 0).x = 900;
        p.rec(kPool4, 0).y = 900;
        p.rec(kPool4, 0).mass = 100;
        p.rec(kPool4, 0).b50 = 3;
        p.set_in_use(kPool4, 1);
        std::vector<std::pair<int, int>> calls;
        run(p, yes, calls);
        REQUIRE(calls.size() == 2);
        CHECK(calls[0] == std::make_pair(static_cast<int>(kPoolRocks), 1));
        CHECK(calls[1] == std::make_pair(static_cast<int>(kPool4), 0));
    }
}
