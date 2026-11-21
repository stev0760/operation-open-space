// The cheat keys (src/world/cheats.h; work/analysis/m5/s4-crime.md section 1 with
// s4-crime-verify.md claims 1-16 and 69; docs/re/law.md "Cheat keys"): the gate seg11:0x6ee7 and
// every key of seg11:0x72e0 with the stores the listing makes, the key-up gate seg11:0x6d19 and
// the 10 s defender block of the pass.
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/cheats.h"
#include "world/input.h"
#include "world/update.h"
#include "world/world.h"

using namespace is::world;

namespace {

struct Call {
    std::string what;
    int a = 0, b = 0, c = 0, d = 0;
    bool operator==(const Call& o) const { return what == o.what && a == o.a && b == o.b && c == o.c && d == o.d; }
};

class RecHost : public CheatHost {
public:
    std::vector<Call> calls;
    is::shell::Rng r;
    is::shell::Rng& rng() override { return r; }
    std::int16_t call_in(int s, int k, int m) override { calls.push_back({"call_in", s, k, m}); return 1; }
    void play(int id) override { calls.push_back({"play", id}); }
    void sound(int id, int pool, int rec, int mode) override { calls.push_back({"sound", id, pool, rec, mode}); }
    void message(int rc, int pool, int rec, int code) override { calls.push_back({"message", rc, pool, rec, code}); }
    void race_finish() override { calls.push_back({"race"}); }
    void crime(int s, int c) override { calls.push_back({"crime", s, c}); }
    void load_weapon(int s, int w) override { calls.push_back({"weapon", s, w}); }
    void score_redraw() override { calls.push_back({"redraw"}); }
    bool has(const Call& c) const {
        for (const Call& x : calls)
            if (x == c) return true;
        return false;
    }
    int count(const std::string& what) const {
        int n = 0;
        for (const Call& x : calls) n += x.what == what;
        return n;
    }
};

struct Rig {
    World w;
    CheatState st;
    std::int16_t god = 0, dg_1048 = 1;
    RecHost h;
    Rig() {
        w.player = 2;
        w.player_roster = 1;
        w.roster.resize(3);
        w.last_ship = 3;
        w.w920c = 2;
        st.w951c = 1;
        w.ships[2].design_index = 5;
        w.design_ratings[5].tank = 600;
        w.ships[2].fuel = 100;
    }
    CheatCtx ctx() { return CheatCtx{w, st, god, dg_1048, h}; }
    void key(std::uint16_t vk) {
        CheatCtx c = ctx();
        cheat_key(c, vk);
    }
    int pool() const { return w.roles.ships; }
};

}  // namespace

TEST_CASE("cheats: the gate seg11:0x6ee7 needs the edition 2 or -1") {
    Rig r;
    r.st.w951c = 0;
    for (int ed : {0, 1, 3}) {
        r.w.w920c = static_cast<std::int16_t>(ed);
        CheatCtx c = r.ctx();
        CHECK_FALSE(cheat_gate(c));
        CHECK(r.st.w951c == 0);
        CHECK(r.h.calls.empty());
    }
    for (int ed : {2, -1}) {
        r.w.w920c = static_cast<std::int16_t>(ed);
        r.st.w951c = 0;
        r.h.calls.clear();
        CheatCtx c = r.ctx();
        CHECK(cheat_gate(c));
        CHECK(r.st.w951c == 1);
        CHECK(r.h.has({"play", 0x49}));  // sound 0x49 on the way in
        CHECK(r.h.has({"redraw"}));      // dword DGROUP:0x5018 = -1
    }
}

TEST_CASE("cheats: the gate turns off on any non-zero value and clears god mode") {
    Rig r;
    r.st.w951c = 5;  // `cmp 1; sbb; neg`: anything but 0 gives 0
    r.god = 1;
    CheatCtx c = r.ctx();
    CHECK(cheat_gate(c));
    CHECK(r.st.w951c == 0);
    CHECK(r.god == 0);
    CHECK(r.h.count("play") == 0);
    CHECK(r.h.count("redraw") == 1);
    // On again: god mode stays what it was (0), sound 0x49.
    CHECK(cheat_gate(c));
    CHECK(r.st.w951c == 1);
    CHECK(r.h.has({"play", 0x49}));
}

TEST_CASE("cheats: '1' is the race finish in a race only") {
    Rig r;
    r.key('1');
    CHECK(r.h.calls.empty());
    r.w.kind = 6;
    r.key('1');
    CHECK(r.h.count("race") == 1);
    CHECK(r.h.calls.size() == 1);
}

TEST_CASE("cheats: '2' sets gate 0's speed word in a duel only") {
    Rig r;
    r.w.gate[0].w12 = 7;
    r.key('2');
    CHECK(r.w.gate[0].w12 == 7);
    r.w.kind = 7;
    r.key('2');
    CHECK(r.w.gate[0].w12 == -1);
}

TEST_CASE("cheats: '3', '4' and F5 call a Knight, a Pirate and a Patrolman while DGROUP:0x9316 < 8") {
    Rig r;
    r.key('3');
    r.key('4');
    r.key(kVkF5);
    REQUIRE(r.h.calls.size() == 3);
    CHECK(r.h.calls[0] == Call{"call_in", 2, 4, 0x6a});
    CHECK(r.h.calls[1] == Call{"call_in", 2, 3, 0x69});
    CHECK(r.h.calls[2] == Call{"call_in", 2, 0x16, 0x6d});
    r.h.calls.clear();
    r.w.last_ship = 8;  // signed `jge 8`
    r.key('3');
    r.key('4');
    r.key(kVkF5);
    CHECK(r.h.calls.empty());
    r.w.last_ship = 7;
    r.key('3');
    CHECK(r.h.calls.size() == 1);
}

TEST_CASE("cheats: '5' sets Demon's Gate's words and nothing else") {
    Rig r;
    r.w.ship_object(2).b54 = 9;
    r.key('5');
    CHECK(r.w.w1318 == 0);  // not in Demon's Gate
    CHECK(r.w.ship_object(2).b54 == 9);
    CHECK(r.h.calls.empty());
    r.w.handover.demons_gate = 1;
    r.key('5');
    CHECK(r.w.w1318 == 1);
    CHECK(r.w.ship_object(2).b54 == 0);
    CHECK(r.w.w913e == 1);
    REQUIRE(r.h.calls.size() == 1);
    CHECK(r.h.calls[0] == Call{"message", 2, 7, 0, 0xb5});  // seg3:0x6ec(P, 7, 0, 0xb5)
}

TEST_CASE("cheats: '6' refuels by 250 up to the design's tank, signed") {
    Rig r;
    r.key('6');
    CHECK(r.w.ships[2].fuel == 350);
    CHECK(r.h.has({"sound", 0x71, r.pool(), 2, 1}));
    r.w.ships[2].fuel = 500;
    r.key('6');
    CHECK(r.w.ships[2].fuel == 600);  // min(600, 750)
    r.w.ships[2].fuel = 600;
    r.key('6');
    CHECK(r.w.ships[2].fuel == 600);
    // fuel + 250 wraps in 16 bits: 32600 + 250 = -32686, below the tank, so the sum is stored.
    r.w.ships[2].fuel = 32600;
    r.key('6');
    CHECK(r.w.ships[2].fuel == static_cast<std::int16_t>(32600 + 250));
}

TEST_CASE("cheats: '7' toggles DGROUP:0x104e") {
    Rig r;
    r.key('7');
    CHECK(r.st.w104e == 1);
    r.st.w104e = 4;
    r.key('7');
    CHECK(r.st.w104e == 0);
    CHECK(r.h.calls.empty());
}

TEST_CASE("cheats: '8' adds one icon of resources") {
    Rig r;
    r.w.ships[2].set_resources(50);
    r.key('8');
    CHECK(r.w.ships[2].resources() == 150);
    CHECK(r.h.has({"sound", 0x23, r.pool(), 2, 1}));
}

TEST_CASE("cheats: '9' and F3 set the THRUSTERS and GUNS powerups to 6") {
    Rig r;
    r.key('9');
    CHECK(r.w.roster[1].b33[0] == 6);
    CHECK(r.w.roster[1].b32 == 0);
    CHECK(r.h.has({"sound", 0x20, r.pool(), 2, 1}));
    r.h.calls.clear();
    r.key(kVkF3);
    CHECK(r.w.roster[1].b32 == 6);
    CHECK(r.h.has({"sound", 0x20, r.pool(), 2, 1}));
    CHECK(r.w.roster[0].b32 == 0);
    CHECK(r.w.roster[2].b33[0] == 0);
}

TEST_CASE("cheats: a roster index outside the roster changes nothing but the sound") {
    Rig r;
    r.w.player_roster = 9;
    r.key('9');
    for (const auto& rr : r.w.roster) CHECK(rr.b33[0] == 0);
    CHECK(r.h.count("sound") == 1);
}

TEST_CASE("cheats: F4 sets DGROUP:0x913e = 2") {
    Rig r;
    r.key(kVkF4);
    CHECK(r.w.w913e == 2);
    CHECK(r.h.calls.empty());
}

TEST_CASE("cheats: F6 forces a crime of rand() % 10 and sets the force flag") {
    Rig r;
    // The draw is seg5:0xdc's first output of the seed; the crime code is it % 10.
    is::shell::Rng probe = r.h.r;
    const int want = probe.rand() % 10;
    r.key(kVkF6);
    CHECK(r.st.w104a == 1);
    REQUIRE(r.h.calls.size() == 1);
    CHECK(r.h.calls[0] == Call{"crime", 2, want});
    CHECK(r.h.r.state() == probe.state());  // one draw, no more
    for (int i = 0; i < 40; ++i) {
        r.h.calls.clear();
        r.key(kVkF6);
        REQUIRE(r.h.calls.size() == 1);
        CHECK(r.h.calls[0].b >= 0);
        CHECK(r.h.calls[0].b <= 9);
    }
}

TEST_CASE("cheats: F7 loads weapons 22, 23 and 26 in that order, then plays sound 0x25") {
    Rig r;
    r.key(kVkF7);
    REQUIRE(r.h.calls.size() == 4);
    CHECK(r.h.calls[0] == Call{"weapon", 2, 22});
    CHECK(r.h.calls[1] == Call{"weapon", 2, 23});
    CHECK(r.h.calls[2] == Call{"weapon", 2, 26});
    CHECK(r.h.calls[3] == Call{"play", 0x25});
}

TEST_CASE("cheats: F8 toggles god mode and redraws the score; F9 stops the 10 s launches") {
    Rig r;
    r.key(kVkF8);
    CHECK(r.god == 1);
    CHECK(r.h.count("redraw") == 1);
    r.key(kVkF8);
    CHECK(r.god == 0);
    CHECK(r.h.count("redraw") == 2);
    CHECK(r.dg_1048 == 1);
    r.key(kVkF9);
    CHECK(r.dg_1048 == 0);
}

TEST_CASE("cheats: a virtual key outside '1' ... '9' and F3 ... F9 does nothing") {
    Rig r;
    for (std::uint16_t vk : std::vector<std::uint16_t>{0x30, 0x3a, 0x41, 0x71, 0x79, 0x2e, 0x0d, 0x14})
        r.key(vk);
    CHECK(r.h.calls.empty());
    CHECK(r.god == 0);
    CHECK(r.dg_1048 == 1);
    CHECK(r.st.w104e == 0);
}

TEST_CASE("cheats: the player outside the ship pool leaves the stores alone") {
    Rig r;
    r.w.player = 200;
    r.w.handover.demons_gate = 1;
    r.key('5');
    r.key('6');
    r.key('8');
    CHECK(r.w.w913e == 0);
    CHECK(r.w.w1318 == 0);
    CHECK(r.h.calls.empty());
}

// ---- the key-up gate seg11:0x6d19 through key_message

namespace {

class Acts : public KeyActions {
public:
    std::vector<std::uint16_t> unported_keys;
    void unported(std::uint32_t, std::uint16_t key, const char*) override { unported_keys.push_back(key); }
};

struct LinkGuard {
    KeyLinks saved;
    LinkGuard() : saved(key_links()) { key_links() = KeyLinks{}; }
    ~LinkGuard() {
        key_links() = saved;
        cheat_state() = CheatState{};
    }
};

}  // namespace

TEST_CASE("cheats: no cheat key acts while DGROUP:0x951c is 0") {
    LinkGuard lg;
    std::vector<std::uint16_t> seen;
    key_links().cheat_key = [&](std::uint16_t vk) { seen.push_back(vk); };
    WaveGlobals g;
    g.demo = 1;
    g.now = 1000;
    KeyState ks;
    cheat_state() = CheatState{};
    Acts a;
    std::vector<std::uint16_t> vks;
    for (std::uint16_t vk = 0x31; vk <= 0x39; ++vk) vks.push_back(vk);
    for (std::uint16_t vk = 0x72; vk <= 0x78; ++vk) vks.push_back(vk);
    for (std::uint16_t vk : vks)
        CHECK(key_message(kWmKeyUp, vk, g, 0, 0, false, false, ks, a) == KeyResult::kHandled);
    CHECK(seen.empty());
    cheat_state().w951c = 1;
    for (std::uint16_t vk : vks)
        CHECK(key_message(kWmKeyUp, vk, g, 0, 0, false, false, ks, a) == KeyResult::kHandled);
    CHECK(seen == vks);
}

TEST_CASE("cheats: the key-up gate: the demo, the lock-out and the player's state") {
    LinkGuard lg;
    int n = 0;
    key_links().cheat_key = [&](std::uint16_t) { ++n; };
    WaveGlobals g;
    g.demo = 1;
    g.now = 1000;
    KeyState ks;
    cheat_state().w951c = 1;
    Acts a;
    CHECK(key_message(kWmKeyUp, '6', g, 2000, 0, false, false, ks, a) == KeyResult::kGated);  // +0x84 > now
    CHECK(key_message(kWmKeyUp, '6', g, 0, 1, false, false, ks, a) == KeyResult::kGated);     // state != 0
    CHECK(key_message(kWmKeyUp, '6', g, 1000, 0, false, false, ks, a) == KeyResult::kHandled); // not above now
    CHECK(n == 1);
    g.demo = 0;
    CHECK(key_message(kWmKeyUp, '6', g, 0, 0, false, false, ks, a) == KeyResult::kHandled);
    CHECK(n == 1);  // seg11:0x6d19: nothing without a player game
}

TEST_CASE("cheats: other key-ups are the router's, not the cheat switch's") {
    LinkGuard lg;
    WaveGlobals g;
    g.demo = 1;
    KeyState ks;
    cheat_state().w951c = 1;
    Acts a;
    for (std::uint16_t vk : std::vector<std::uint16_t>{0x0d, 0x1b, 'P', 'Z', 0x14, 0x7b, 0x30, 0x3a, 0x71, 0x79})
        CHECK(key_message(kWmKeyUp, vk, g, 0, 0, false, false, ks, a) == KeyResult::kNotMine);
    // A locked-out player does not make them the switch's either.
    CHECK(key_message(kWmKeyUp, 0x0d, g, 5000, 1, false, false, ks, a) == KeyResult::kNotMine);
}

TEST_CASE("cheats: DELETE on WM_KEYDOWN, 0x99b and 0x99e reaches the gate") {
    LinkGuard lg;
    int n = 0;
    key_links().cheat_gate = [&] { ++n; };
    WaveGlobals g;
    g.demo = 1;
    KeyState ks;
    Acts a;
    CHECK(key_message(kWmKeyDown, kVkDelete, g, 0, 0, false, false, ks, a) == KeyResult::kHandled);
    CHECK(key_message(kMsgAltKey, kVkDelete, g, 0, 0, false, false, ks, a) == KeyResult::kHandled);
    CHECK(key_message(kMsgCtrlKey, kVkDelete, g, 0, 0, false, false, ks, a) == KeyResult::kHandled);
    CHECK(n == 3);
    // Not in the demo (DGROUP:0x95bc = 0), and not past the player's lock-out.
    g.demo = 0;
    CHECK(key_message(kWmKeyDown, kVkDelete, g, 0, 0, false, false, ks, a) == KeyResult::kHandled);
    CHECK(n == 3);
    g.demo = 1;
    CHECK(key_message(kWmKeyDown, kVkDelete, g, 0, 1, false, false, ks, a) == KeyResult::kGated);
    CHECK(n == 3);
}

// ---- the 10 s defender block of the pass (seg7:0x886-0x8d0)

namespace {

class Counter : public UpdateHost {
public:
    int launches = 0, warnings = 0;
    void seg8_9464() override { ++launches; }
    void seg8_9564() override { ++warnings; }
};

}  // namespace

TEST_CASE("cheats: DGROUP:0x1048 is 1 in the image and F9's 0 stops the 10 s defender block") {
    FrameGlobals fresh;
    CHECK(fresh.dg_1048 == 1);

    Pools pools;
    ShipTable ships{};
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    Counter host;
    is::shell::Rng rng;
    JoyState joy;
    JoyButtons jb;
    UpdateCtx c{FlightCtx{pools, ships, g, keys, host, rng}, f, view, host, joy, jb};
    g.now = 50000;
    view.play_w = 1024;
    view.play_h = 683;
    view.shift = 3;
    ObjectRecord& o = pools.rec(kPoolShips, 0);
    o.mass = 50, o.x = 12800, o.y = 12800, o.extent = 256;
    ships[0].roster_index = -1;
    f.dg_92f0 = 1;     // pool 2 has records
    f.dg_1e6c = 40000; // due
    wave_pass(c, false);
    CHECK(host.launches == 1);
    CHECK(f.dg_1e6c == 60000u);  // now + 10000
    CHECK(f.dg_914e == 0);
    // F9: DGROUP:0x1048 = 0 skips the whole block.
    f.dg_1048 = 0;
    f.dg_1e6c = 40000;
    wave_pass(c, false);
    CHECK(host.launches == 1);
    CHECK(host.warnings == 0);
    CHECK(f.dg_1e6c == 40000u);
}

namespace {

class WorldCounter : public Counter {
public:
    World* fw = nullptr;
    World* frame_world(Pools&) override { return fw; }
};

}  // namespace

TEST_CASE("cheats: the 10 s defender block tests pool 2's allocated count DGROUP:0x92f0, not its storage") {
    // `cmp word [0x92f0], 0` at seg7:0x898 reads DGROUP:0x92ec + 2·2 (review L1); the storage
    // holds max(allocated, 1) records.
    World w;
    w.pools.resize(2, 0);
    REQUIRE(w.pools.records(2) >= 1);
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    WorldCounter host;
    host.fw = &w;
    is::shell::Rng rng;
    JoyState joy;
    JoyButtons jb;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    g.now = 50000;
    view.play_w = 1024;
    view.play_h = 683;
    view.shift = 3;
    ObjectRecord& o = w.pools.rec(kPoolShips, 0);
    o.mass = 50, o.x = 12800, o.y = 12800, o.extent = 256;
    w.ships[0].roster_index = -1;
    w.w1e6c = 40000;  // due
    wave_pass(c, false);
    CHECK(host.launches == 0);
    CHECK(w.w1e6c == 40000u);
    w.pools.set_allocated(2, 1);
    wave_pass(c, false);
    CHECK(host.launches == 1);
    CHECK(w.w1e6c == 60000u);
}
