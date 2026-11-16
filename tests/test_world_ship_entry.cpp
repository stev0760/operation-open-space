// The other ships of a wave (M5 stage 3 part B, track ENTRY): the call-in seg13:0xadbe /
// seg13:0xae68 (src/world/callin.h), the gate placement and the per-ship sheet (src/world/
// ship_entry.h), the image slots across waves (src/world/player_sheet.h) and seg7:0x1d0c's entry
// of a wave's waiting computer ships (src/world/update.h). The numbers are worked from the
// listings cited at each check and, for the entry, from ships-0001.
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/callin.h"
#include "world/player_sheet.h"
#include "world/ship_entry.h"
#include "world/update.h"
#include "world/world.h"

using namespace is::world;

namespace {

// A call-in host that records every call; team(n) reads World::design_ratings[n].team.
struct TestHost final : CallInHost {
    World* w = nullptr;
    is::shell::Rng r;
    std::vector<std::string> log;
    std::uint32_t t = 100000, fr = 77;
    std::int16_t called = 0;
    std::uint32_t frame_set = 0;
    std::function<void(int mode)> on_build;
    is::shell::Rng& rng() override { return r; }
    std::int16_t team(int design) override {
        return design >= 0 && design < kShipCount ? w->design_ratings[static_cast<std::size_t>(design)].team
                                                  : std::int16_t{0};
    }
    std::uint32_t now() override { return t; }
    std::uint32_t frames() override { return fr; }
    void play(int id) override { log.push_back("play " + std::to_string(id)); }
    void sound(int id, int pool, int rec, int mode) override {
        log.push_back("sound " + std::to_string(id) + " " + std::to_string(pool) + " " + std::to_string(rec) + " " +
                      std::to_string(mode));
    }
    void music(int track, int flag) override {
        log.push_back("music " + std::to_string(track) + " " + std::to_string(flag));
    }
    void message(int recipient, int pool, int rec, int code) override {
        log.push_back("message " + std::to_string(recipient) + " " + std::to_string(pool) + " " +
                      std::to_string(rec) + " " + std::to_string(code));
    }
    void purchases(int ship) override { log.push_back("purchases " + std::to_string(ship)); }
    void entered(int ship) override { log.push_back("entered " + std::to_string(ship)); }
    void designs_begin() override { log.push_back("designs_begin"); }
    void designs_end() override { log.push_back("designs_end"); }
    void build_roster(int mode) override {
        log.push_back("build_roster " + std::to_string(mode));
        if (on_build) on_build(mode);
    }
    void seg11_238e() override { log.push_back("seg11_238e"); }
    void seg4_8346(int ship, int pool, int rec, int code) override {
        log.push_back("seg4_8346 " + std::to_string(ship) + " " + std::to_string(pool) + " " + std::to_string(rec) +
                      " " + std::to_string(code));
    }
    Point16 screen_to_world(std::int16_t sx, std::int16_t sy) override {
        log.push_back("screen_to_world " + std::to_string(sx) + " " + std::to_string(sy));
        return Point16{static_cast<std::int16_t>(sx + 1000), static_cast<std::int16_t>(sy + 2000)};
    }
    std::int16_t& called_in() override { return called; }
    void set_callin_frame(std::uint32_t f) override { frame_set = f; }
};

// Two gates (posts 0, 1 and 2, 3 of pool 8), the player (ship 0, team 0) and ship 1 (team 0) in
// play, Detail 3; design index n = ship n.
void two_ship_wave(World& w) {
    w.detail = 3;
    w.gates = 2;
    w.player = 0;
    w.last_ship = 1;
    w.pools.rec(kPoolGatePosts, 0).x = 1000;
    w.pools.rec(kPoolGatePosts, 0).y = 2000;
    w.pools.rec(kPoolGatePosts, 0).size = 20;
    w.pools.rec(kPoolGatePosts, 2).x = 5000;
    w.pools.rec(kPoolGatePosts, 2).y = 6000;
    w.pools.rec(kPoolGatePosts, 2).size = 24;
    w.gate[0].width = 1200;
    w.gate[1].width = 1000;
    for (int i = 0; i < kShipCount; ++i) {
        w.ships[static_cast<std::size_t>(i)].design_index = static_cast<std::int16_t>(i);
        w.ships[static_cast<std::size_t>(i)].bec = 1;
        set_byte_at(w.ships[static_cast<std::size_t>(i)], 0xcd, 0xff);
        w.pools.rec(kPoolShips, i).size = 33;
        w.pools.rec(kPoolShips, i).gap = 6;
    }
}

// The gate the call-in's rand() % DGROUP:0x9318 picks after `before` draws from seed s.
int gate_after(std::uint32_t s, int before, int gates) {
    is::shell::Rng r;
    r.set_state(s);
    for (int k = 0; k < before; ++k) r.rand();
    return r.rand() % gates;
}

std::int16_t post_x(const World& w, int g) { return w.pools.rec(kPoolGatePosts, 2 * g).x; }
std::int16_t post_y(const World& w, int g) { return w.pools.rec(kPoolGatePosts, 2 * g).y; }
std::int16_t post_size(const World& w, int g) { return w.pools.rec(kPoolGatePosts, 2 * g).size; }

bool logged(const TestHost& h, const std::string& s) {
    for (const auto& l : h.log)
        if (l == s) return true;
    return false;
}

}  // namespace

TEST_CASE("call-in: the gate x of seg13:0xae68") {
    // post x + post size - size/2 + W/2 [BYTES seg13:0xb12c-0xb156]
    CHECK(callin_gate_x(7000, 20, 33, 1200) == 7000 + 20 - 16 + 600);
    // signed halvings toward zero: -3 / 2 = -1, 5 / 2 = 2
    CHECK(callin_gate_x(100, 0, -3, 5) == 103);
    // 16-bit wrap
    CHECK(callin_gate_x(32000, 1000, 0, 0) == static_cast<std::int16_t>(33000 - 65536));
}

TEST_CASE("call-in: kind 0x16 with no Patrol present builds a ship, enters it at a gate, music 2") {
    World w;
    two_ship_wave(w);
    TestHost h;
    h.w = &w;
    h.r.set_state(1234);
    std::uint32_t music_due = 0;
    // seg14:0x0(0x16) puts a PATROLMAN after the last ship: team 7, +0x5e = 2 (SHP +0x05).
    h.on_build = [&w](int mode) {
        CHECK(mode == kCallInPatrol);
        w.last_ship = 2;
        w.design_ratings[2].team = 7;
        set_byte_at(w.ships[2], 0x5e, 2);
        w.design_ratings[2].captures = 1;
    };
    w.w992 = 5;
    const std::int16_t r = call_in(w, h, 0, kCallInPatrol, 0x6d, &music_due);
    // [BYTES seg13:0xae53-0xae5d]: AX = lo16(now + 12000)
    CHECK(r == static_cast<std::int16_t>((100000u + 12000u) & 0xffffu));
    CHECK(music_due == 112000u);
    const int g = gate_after(1234, 0, 2);  // one rand(), after seg14:0x0 [BYTES seg13:0xb4b2]
    const ObjectRecord& o = w.pools.rec(kPoolShips, 2);
    CHECK(o.state == 0);
    CHECK(o.y == post_y(w, g));
    CHECK(o.x == callin_gate_x(post_x(w, g), post_size(w, g), 33, w.gate[static_cast<std::size_t>(g)].width));
    CHECK(o.src_y == 6);          // +0x12 = +0x24 [BYTES seg13:0xb47d]
    CHECK(o.owner_pool == -1);    // +0x32 [BYTES seg13:0xb498]
    CHECK(o.w36 == -1);
    const ShipRecord& s = w.ships[2];
    CHECK(s.w52 == 0x14);  // activity [BYTES seg13:0xb04d seg13:0xb5fd]
    CHECK(s.w54 == 14);
    CHECK(s.w56 == 0);
    CHECK(s.w32 == 14);
    CHECK(s.w34 == 0);
    CHECK(s.d94.get() == 100000u);
    CHECK(s.w58 == 2);  // [bp-6], the old DGROUP:0x9316 + 1
    CHECK(sbyte_at(s, 0xed) == 1);
    CHECK(s.w38 == 3);
    CHECK(sbyte_at(s, 0xcd) == 0);
    CHECK(static_cast<std::uint32_t>(static_cast<std::uint16_t>(word_at(s, 0x98))) +
              (static_cast<std::uint32_t>(static_cast<std::uint16_t>(word_at(s, 0x9a))) << 16) ==
          105000u);
    CHECK(w.design_ratings[2].captures == 0);  // design word +0x42 = 0 [BYTES seg13:0xb63a]
    CHECK(h.called == 1);                      // DGROUP:0x964c [BYTES seg13:0xb644]
    CHECK(h.frame_set == 77u);                 // DGROUP:0x8e2c [BYTES seg13:0xb6e0]
    CHECK(w.w992 == 15);                       // [BYTES seg13:0xb462]
    // The order of the calls [BYTES seg13:0xb3fa-0xb5f1 seg13:0xae4e]
    const std::vector<std::string> want = {"designs_begin", "message 0 7 0 109", "seg11_238e", "build_roster 22",
                                           "designs_end",   "sound 39 14 2 1",   "entered 2",  "music 2 0"};
    CHECK(h.log == want);
    CHECK(h.r.state() != 0);
}

TEST_CASE("call-in: a waiting Interceptor enters at once with the siren (kind 0x15)") {
    World w;
    two_ship_wave(w);
    w.last_ship = 2;
    w.design_ratings[2].team = 7;
    set_byte_at(w.ships[2], 0x5e, 1);
    w.pools.rec(kPoolShips, 2).state = 100;  // off-world
    TestHost h;
    h.w = &w;
    h.r.set_state(99);
    const std::int16_t r = call_in_request(w, h, 0, kCallInInterceptor, -1);
    CHECK(r == 1);
    const int g = gate_after(99, 0, 2);
    const ObjectRecord& o = w.pools.rec(kPoolShips, 2);
    CHECK(o.state == 0);
    CHECK(o.x == callin_gate_x(post_x(w, g), post_size(w, g), 33, w.gate[static_cast<std::size_t>(g)].width));
    CHECK(w.ships[2].w52 == 0x16);
    CHECK(w.ships[2].w58 == 2);  // the ship's own index on this path [BYTES seg13:0xb1cd]
    // SIRENS for class < 2, then seg16:0x3784(k, 1), HYPER and the effect [BYTES seg13:0xb0c4-0xb195]
    const std::vector<std::string> want = {"play 112", "purchases 2", "sound 39 14 2 1", "entered 2"};
    CHECK(h.log == want);
}

TEST_CASE("call-in: an Interceptor in play is enlisted, or refused when it already serves") {
    World w;
    two_ship_wave(w);
    w.last_ship = 2;
    w.design_ratings[2].team = 7;
    set_byte_at(w.ships[2], 0x5e, 1);
    w.ships[2].w56 = 1;
    TestHost h;
    h.w = &w;
    const std::uint32_t s0 = h.r.state();
    // Free (+0xcd = 0xff): message 0x3f to its old +0x56, the order words, 1 [BYTES seg13:0xb28d-0xb2da]
    CHECK(call_in_request(w, h, 0, kCallInInterceptor, -1) == 1);
    CHECK(h.r.state() == s0);  // no rand() on this path
    CHECK(logged(h, "message 1 14 2 63"));
    CHECK(w.ships[2].w52 == 0x16);
    CHECK(w.ships[2].w56 == 0);
    CHECK(sbyte_at(w.ships[2], 0xcd) == 0);
    // Now it serves the requester with +0x52 = 0x16 > 0x14: refused [BYTES seg13:0xb275-0xb289]
    h.log.clear();
    CHECK(call_in_request(w, h, 0, kCallInInterceptor, -1) == 0);
    CHECK(h.log.empty());
    // With +0x52 = 0x14 it is enlisted again (the message goes to +0x56 = 0, the requester).
    w.ships[2].w52 = 0x14;
    CHECK(call_in_request(w, h, 0, kCallInInterceptor, -1) == 1);
    CHECK(logged(h, "message 0 14 2 63"));
}

TEST_CASE("call-in: a dead or busy match builds no ship (correction 52)") {
    World w;
    two_ship_wave(w);
    w.last_ship = 2;
    w.design_ratings[2].team = 7;
    set_byte_at(w.ships[2], 0x5e, 2);
    w.ships[2].bec = 0;  // dead, in play [BYTES seg13:0xb251-0xb256]
    TestHost h;
    h.w = &w;
    bool built = false;
    h.on_build = [&built](int) { built = true; };
    CHECK(call_in_request(w, h, 0, kCallInPatrol, 0x6d) == 0);
    CHECK_FALSE(built);
    // Alive but called in for ship 1 [BYTES seg13:0xb258-0xb267]
    w.ships[2].bec = 1;
    set_byte_at(w.ships[2], 0xcd, 1);
    CHECK(call_in_request(w, h, 0, kCallInPatrol, 0x6d) == 0);
    CHECK_FALSE(built);
}

TEST_CASE("call-in: the refusals of seg13:0xae68") {
    World w;
    two_ship_wave(w);
    TestHost h;
    h.w = &w;
    const CallInRequest ok = callin_request(w, h, 0, kCallInPatrol, nullptr, 0);
    CHECK_FALSE(ok.refused);
    CHECK(ok.team == 7);
    CHECK(ok.cls == 2);
    CHECK(ok.activity == 0x14);
    // 2·Detail + 1 < DGROUP:0x964c refuses every kind but 0x15 [BYTES seg13:0xae9c-0xaeac]
    h.called = 8;
    CHECK(callin_request(w, h, 0, kCallInPatrol, nullptr, 0).refused);
    CHECK_FALSE(callin_request(w, h, 0, kCallInInterceptor, nullptr, 0).refused);
    h.called = 7;
    CHECK_FALSE(callin_request(w, h, 0, kCallInPatrol, nullptr, 0).refused);
    // Detail + 3 < DGROUP:0x9316 refuses [BYTES seg13:0xaeae-0xaeb8]
    w.detail = 0;
    w.last_ship = 4;
    CHECK(callin_request(w, h, 0, kCallInInterceptor, nullptr, 0).refused);
    w.last_ship = 3;
    CHECK_FALSE(callin_request(w, h, 0, kCallInInterceptor, nullptr, 0).refused);
    w.detail = 3;
    w.last_ship = 1;
    // A race lets only 0x15 with byte DGROUP:0xa163 = 8 [BYTES seg13:0xae7c-0xae95]
    w.kind = 6;
    CHECK(callin_request(w, h, 0, kCallInInterceptor, nullptr, 7).refused);
    CHECK_FALSE(callin_request(w, h, 0, kCallInInterceptor, nullptr, 8).refused);
    CHECK(callin_request(w, h, 0, kCallInPatrol, nullptr, 8).refused);
    w.kind = 0;
    // Demon's Gate only 0x13 [BYTES seg13:0xaeba-0xaec5]
    w.handover.demons_gate = 1;
    CHECK(callin_request(w, h, 0, kCallInPatrol, nullptr, 0).refused);
    CHECK_FALSE(callin_request(w, h, 0, kCallInDemon, nullptr, 0).refused);
}

TEST_CASE("call-in: kind 4's requests and refusals") {
    World w;
    two_ship_wave(w);
    w.last_ship = 3;
    TestHost h;
    h.w = &w;
    std::int16_t table[64] = {};
    w.design_ratings[1].team = 2;  // the requester is ship 1, team 2
    // [bp-0xe] = 4, then 0 for a team-2 ship with word DGROUP:0x3828 > 50 [BYTES seg13:0xaf14-0xaf2d]
    CallInRequest q = callin_request(w, h, 1, kCallInHelp, table, 0);
    CHECK_FALSE(q.refused);
    CHECK(q.team == 4);
    CHECK(q.activity == 1);
    table[2] = 51;
    q = callin_request(w, h, 1, kCallInHelp, table, 0);
    CHECK(q.team == 0);
    // word DGROUP:0x3824 + 2·(8 T + P) < 0 refuses: T = 0, P = 2 -> index 2 (51 here)
    table[2] = 50;
    table[8 * 4 + 2] = -1;
    CHECK(callin_request(w, h, 1, kCallInHelp, table, 0).refused);
    table[8 * 4 + 2] = 0;
    // A helper already: +0xcd >= 0 [BYTES seg13:0xaf00-0xaf05]
    set_byte_at(w.ships[1], 0xcd, 0);
    CHECK(callin_request(w, h, 1, kCallInHelp, table, 0).refused);
    set_byte_at(w.ships[1], 0xcd, 0xff);
    // A live team-4 ship j < DGROUP:0x9316 serving ship 1; the last ship is not checked
    w.design_ratings[3].team = 4;
    set_byte_at(w.ships[3], 0xcd, 1);
    CHECK_FALSE(callin_request(w, h, 1, kCallInHelp, table, 0).refused);  // j = 3 = DGROUP:0x9316
    w.design_ratings[2].team = 4;
    set_byte_at(w.ships[2], 0xcd, 1);
    CHECK(callin_request(w, h, 1, kCallInHelp, table, 0).refused);
    set_byte_at(w.ships[2], 0xcd, 0xff);
    // +0x38 = 1 with byte +0x146 in {3, 0xd, 8, 0xa} refuses team 4 [BYTES seg13:0xafab-0xafe4]
    w.ships[1].w38 = 1;
    set_byte_at(w.ships[1], 0x146, 0xa);
    CHECK(callin_request(w, h, 1, kCallInHelp, table, 0).refused);
    set_byte_at(w.ships[1], 0x146, 9);
    CHECK_FALSE(callin_request(w, h, 1, kCallInHelp, table, 0).refused);
    // A team-6 requester is refused [BYTES seg13:0xaf54-0xaf59]
    w.design_ratings[1].team = 6;
    CHECK(callin_request(w, h, 1, kCallInHelp, table, 0).refused);
}

TEST_CASE("call-in: kind 4 enters an off-world teammate of the requester, and its music rule") {
    World w;
    two_ship_wave(w);
    w.last_ship = 3;
    TestHost h;
    h.w = &w;
    h.r.set_state(7);
    w.design_ratings[1].team = 4;  // the requester's own team word (not T)
    w.design_ratings[3].team = 4;
    w.pools.rec(kPoolShips, 3).state = 100;
    std::uint32_t due = 0;
    // The player is not the requester and its +0x32 != 14: no music, AX = the player [BYTES seg13:0xade9-0xae01]
    const std::int16_t r = call_in(w, h, 1, kCallInHelp, 0x6a, &due);
    CHECK(r == 0);  // the player's index 0: every caller reads it as refused
    CHECK(due == 0u);
    CHECK(w.pools.rec(kPoolShips, 3).state == 0);
    CHECK(w.ships[3].w52 == 1);
    CHECK(w.ships[3].w34 == 1);
    CHECK(sbyte_at(w.ships[3], 0xcd) == 1);
    CHECK_FALSE(logged(h, "music 8 0"));
    // The player asks: t = 8 [BYTES seg13:0xadec-0xadf1]
    w.pools.rec(kPoolShips, 3).state = 100;
    set_byte_at(w.ships[0], 0xcd, 0xff);
    w.design_ratings[0].team = 4;
    h.log.clear();
    const std::int16_t r2 = call_in(w, h, 0, kCallInHelp, 0x6a, &due);
    CHECK(r2 == static_cast<std::int16_t>((100000u + 12000u) & 0xffffu));
    CHECK(logged(h, "music 8 0"));
    // The player hunts the requester (+0x32 = 14, +0x34 = 1, +0x38 = 1): t = 7
    w.pools.rec(kPoolShips, 3).state = 100;
    set_byte_at(w.ships[1], 0xcd, 0xff);
    w.ships[0].w32 = 14;
    w.ships[0].w34 = 1;
    w.ships[0].w38 = 1;
    h.log.clear();
    call_in(w, h, 1, kCallInHelp, 0x6a, &due);
    CHECK(logged(h, "music 7 0"));
}

TEST_CASE("call-in: the music track of seg13:0xadbe by kind") {
    World w;
    two_ship_wave(w);
    w.last_ship = 2;
    w.design_ratings[2].team = 7;
    TestHost h;
    h.w = &w;
    const auto track = [&](int kind, int cls) {
        set_byte_at(w.ships[2], 0x5e, cls);
        set_byte_at(w.ships[2], 0xcd, 0xff);
        w.ships[2].w52 = 0;
        h.log.clear();
        call_in(w, h, 0, kind, -1, nullptr);
        for (const auto& l : h.log)
            if (l.rfind("music ", 0) == 0) return l;
        return std::string();
    };
    CHECK(track(kCallInInterceptor, 1) == "music 2 0");
    CHECK(track(kCallInTerminator, 0) == "music 7 0");
    CHECK(track(kCallInPatrol, 2) == "music 2 0");
}

TEST_CASE("call-in: Demon's Gate draws three rand() and places the ship round the player") {
    World w;
    two_ship_wave(w);
    w.handover.demons_gate = 1;
    w.W = 1024;
    w.pools.rec(kPoolShips, 0).scr_x = 512;
    w.pools.rec(kPoolShips, 0).scr_y = 341;
    TestHost h;
    h.w = &w;
    h.r.set_state(4242);
    w.design_ratings[0].team = 0;
    h.on_build = [&w](int mode) {
        CHECK(mode == kCallInDemon);
        w.last_ship = 2;
    };
    CHECK(call_in_request(w, h, 0, kCallInDemon, 0xb4) == 2);
    is::shell::Rng r;
    r.set_state(4242);
    r.rand();  // the gate's
    const int d = r.rand() % (1024 / 5) + 1024 / 5;
    const int dir = r.rand() % 16;
    CHECK(h.r.state() == r.state());
    static const int hx[16] = {0, 6, 11, 15, 16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6};
    static const int hy[16] = {-16, -15, -11, -6, 0, 6, 11, 15, 16, 16, 11, 6, 0, -6, -11, -15};
    const auto q16 = [](int v) { return static_cast<std::int16_t>(v) / 16; };  // cwd, and 0xf, sar 4
    const int sx = q16(static_cast<std::int16_t>(hx[dir] * d)) + 512;
    const int sy = q16(static_cast<std::int16_t>(hy[dir] * d)) + 341;
    CHECK(logged(h, "screen_to_world " + std::to_string(sx) + " " + std::to_string(sy)));
    CHECK(w.pools.rec(kPoolShips, 2).x == sx + 1000);
    CHECK(w.pools.rec(kPoolShips, 2).y == sy + 2000);
    CHECK(w.ships[2].w52 == 4);
}

TEST_CASE("call-in: kind 3 sends message 0x67 and sets the -300 attitude") {
    World w;
    two_ship_wave(w);
    TestHost h;
    h.w = &w;
    w.design_ratings[0].team = 0;
    h.on_build = [&w](int mode) {
        CHECK(mode == kCallInPirates);
        w.last_ship = 2;
        w.design_ratings[2].team = 3;
    };
    CHECK(call_in_request(w, h, 0, kCallInPirates, 0x69) == 2);
    CHECK(sbyte_at(w.ships[2], 0xed) == 0);
    CHECK(w.ships[2].r62[0] == -300);  // word +0x62 + 2·requester [BYTES seg13:0xb666-0xb671]
    CHECK(logged(h, "message 0 14 2 103"));
    CHECK(logged(h, "seg4_8346 2 14 0 13"));
}

// ---- the image slots across waves (seg14:0x96c-0x9e3, seg10:0x1eb6)

TEST_CASE("ship slots: a kept ship's sheet moves with its words, the old slot is zeroed, the rest freed") {
    RecordingBlitter out;
    ImageSlotTable images{};
    clear_sheet_copies();
    SheetPixels a;
    a.w = 2;
    a.h = 1;
    a.index = {1, 2};
    images[kShipImageSlotBase + 1] = 0x501;
    images[kShipImageSlotBase + 2] = 0x502;
    images[kShipImageSlotBase + 3] = 0x503;
    set_sheet_copy(2, a);
    const ShipSlotSnapshot sn = snapshot_ship_slots(images);
    // The ship of old slot 2 is new slot 1: slot 1 takes slot 2's saved words [BYTES seg14:0x9a0-0x9c7],
    // slot 2's current DC is still its saved one, so its words are zeroed [BYTES seg14:0x9cb-0x9df].
    take_ship_slot(images, sn, 1, 2);
    CHECK(images[kShipImageSlotBase + 1] == 0x502);
    if (images[kShipImageSlotBase + 2] == sn.dc[2]) zero_ship_slot(images, 2);
    CHECK(images[kShipImageSlotBase + 2] == kDcNone);
    REQUIRE(sheet_copy(1) != nullptr);
    CHECK(sheet_copy(1)->index == a.index);
    CHECK(sheet_copy(2) == nullptr);
    // Slot 3 beyond the new count is freed [BYTES seg14:0xa07-0xa1d seg10:0x1eb6-0x1f27]
    free_ship_slot(out, images, 3);
    CHECK(images[kShipImageSlotBase + 3] == kDcNone);
    REQUIRE(out.deleted.size() == 1);
    CHECK(out.deleted[0] == 0x503);
    // An empty slot frees nothing [BYTES seg10:0x1ed1-0x1ed6]
    free_ship_slot(out, images, 3);
    CHECK(out.deleted.size() == 1);
    // A swap reads the saved words, not slot 2's present ones
    images[kShipImageSlotBase + 4] = 0x504;
    images[kShipImageSlotBase + 5] = 0x505;
    const ShipSlotSnapshot s2 = snapshot_ship_slots(images);
    take_ship_slot(images, s2, 4, 5);
    take_ship_slot(images, s2, 5, 4);
    CHECK(images[kShipImageSlotBase + 4] == 0x505);
    CHECK(images[kShipImageSlotBase + 5] == 0x504);
    clear_sheet_copies();
}

TEST_CASE("ship sheet: a computer ship's sheet in slot 0x28 + i with its +0x3a, +0x5b and roster bytes") {
    RecordingBlitter out;
    ImageSlotTable images{};
    clear_sheet_copies();
    clear_design_sheet_sources();
    World w;
    w.size = 1;
    w.player = 0;
    w.last_ship = 1;
    w.roster.resize(3);
    ShipRecord& s = w.ship(1);
    s.design_index = 1;
    s.roster_index = 2;
    s.has_flame = 1;
    w.design_ratings[1].special = -1;
    DesignSheetSource src;
    src.graphic_id = 4001;
    src.colour_a = 3;
    src.colour_b = 4;
    src.bod_19 = 5;
    set_design_sheet_source(1, src);
    int renders = 0;
    int last_slot = 0, last_graphic = 0;
    SheetRenderer r = [&](const SheetRequest& q, SheetPixels& px) {
        ++renders;
        last_slot = q.slot;
        last_graphic = q.graphic_id;
        const SheetGeometry g = sheet_geometry(66, 33, q.gap_word, q.mode, q.scale);
        px.w = g.w;
        px.h = g.h;
        px.index.assign(static_cast<std::size_t>(g.w) * g.h, 9);
        px.source_height = 33;
        return true;
    };
    WeaponWords words;
    PlaceSheetCtx c{out, images, w, words, r, {}, nullptr, 0, 3, 0};
    ObjectRecord& o = w.ship_object(1);
    o.size = 33;
    o.gap = 5;
    PlacedSheet p = place_ship_sheet(c, 1, true);
    CHECK(p.rebuilt);
    CHECK(renders == 1);
    CHECK(last_slot == kShipImageSlotBase + 1);
    CHECK(last_graphic == 4001);
    CHECK(images[kShipImageSlotBase + 1] != kDcNone);
    CHECK(o.w3a == static_cast<std::int16_t>(images[kShipImageSlotBase + 1]));  // [BYTES seg16:0x3645]
    CHECK(w.roster[2].size == 33);  // roster bytes +0x7a / +0x7b [BYTES seg14:0x131c seg14:0x1337]
    CHECK(w.roster[2].gap == 5);
    // +0x5b = (bmHeight - gap) / (size + gap) - 1: two frames, h = (33 + 5)·2 + 5 = 81 -> 1
    CHECK(s.anim_last == 1);
    CHECK(p.has_w04);
    CHECK(p.w04 == flame_radius_word(5, 33));
    REQUIRE(last_build(1) != nullptr);
    // A later wave without the flag keeps the sheet: no render, the same DC, the last build
    const DcId dc = images[kShipImageSlotBase + 1];
    o.w3a = 0;
    p = place_ship_sheet(c, 1, false);
    CHECK_FALSE(p.rebuilt);
    CHECK(renders == 1);
    CHECK(images[kShipImageSlotBase + 1] == dc);
    CHECK(o.w3a == static_cast<std::int16_t>(dc));
    CHECK(p.build.rendered);
    CHECK(p.build.side == 33);
    clear_sheet_copies();
    clear_design_sheet_sources();
}

// ---- seg7:0x1d0c with computer ships (update.h ship_entry, general in the ship index)

namespace {

struct EntryHost final : UpdateHost {
    World* w = nullptr;
    std::vector<int> entered;
    std::vector<int> sounds;
    std::int16_t design_kind(int design) override {
        return design >= 0 && design < kShipCount ? w->design_ratings[static_cast<std::size_t>(design)].team
                                                  : std::int16_t{0};
    }
    void ship_entered(int ship) override { entered.push_back(ship); }
    void sound(int id, int pool, int rec, int mode) override {
        (void)pool, (void)mode;
        sounds.push_back(id * 100 + rec);
    }
    World* frame_world(Pools&) override { return w; }
};

}  // namespace

TEST_CASE("ship entry: the waiting ships come in 5000 ms apart in slot order, an Interceptor waits") {
    World w;
    two_ship_wave(w);
    w.last_ship = 3;
    for (int i = 0; i <= 3; ++i) w.pools.rec(kPoolShips, i).state = 100;
    // seg9:0x63e9-0x6429: +0x88 = now + 500000 i, then the player's and ship 0's dwords swap
    const std::uint32_t t0 = 50000;
    for (int i = 0; i <= 3; ++i) w.ships[static_cast<std::size_t>(i)].d88.set(t0 + 500000u * static_cast<std::uint32_t>(i));
    w.design_ratings[3].team = 7;
    set_byte_at(w.ships[3], 0x5e, 1);  // an Interceptor [BYTES seg7:0x1d80-0x1d99]
    EntryHost h;
    h.w = &w;
    FrameGlobals f;
    ViewState v;
    v.play_w = 1024;
    v.play_h = 683;
    JoyState joy;
    JoyButtons jb;
    WaveGlobals g;
    g.last_ship = 3;
    g.gates = 2;
    KeyLatch keys;
    is::shell::Rng rng;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, h, rng}, f, v, h, joy, jb};
    std::vector<std::pair<int, std::uint32_t>> times;
    for (std::uint32_t now = t0 + 20; now < t0 + 30000; now += 20) {
        g.now = now;
        f.entry_pending = 1;
        const std::size_t before = h.entered.size();
        ship_entry(c);
        if (h.entered.size() != before) times.emplace_back(h.entered.back(), now);
    }
    REQUIRE(times.size() == 3);
    CHECK(times[0].first == 0);
    CHECK(times[0].second == t0 + 20);
    // ship 1 at the first frame + 5000 (ships-0001: 4.93 s after the start, the next 5.0 s later)
    CHECK(times[1].first == 1);
    CHECK(times[1].second == t0 + 20 + 5000 + 20);
    CHECK(times[2].first == 2);
    CHECK(times[2].second == times[1].second + 5000 + 20);
    CHECK(w.pools.rec(kPoolShips, 3).state == 100);  // the Interceptor still waits
    CHECK(w.pools.rec(kPoolShips, 1).state == 0);
}

// ---- RosterHost's image-slot side (player_sheet.h ShipSlotCache) in roster_build.cpp's order

TEST_CASE("ship slots: the cache moves a kept ship's copies with its DC, in seg14:0x0's call order") {
    RecordingBlitter out;
    ImageSlotTable images{};
    clear_sheet_copies();
    ShipSlotCache cache;
    SheetPixels a, b;
    a.w = 1;
    a.h = 1;
    a.index = {7};
    b.w = 1;
    b.h = 1;
    b.index = {8};
    images[kShipImageSlotBase + 0] = 0x600;  // the player's
    images[kShipImageSlotBase + 1] = 0x601;
    images[kShipImageSlotBase + 2] = 0x602;
    set_sheet_copy(1, a);
    set_sheet_copy(2, b);
    // seg14:0x1f9-0x223: the words of every old slot at the entry
    std::array<std::uint16_t, 3> saved{};
    for (int j = 0; j < 3; ++j) saved[static_cast<std::size_t>(j)] = cache.words(images, j);
    CHECK(saved[1] == 0x601);
    // Old slot 1's ship is not kept: seg10:0x1eb6(1, ...) frees it [BYTES seg14:0x8f5-0x963]
    cache.free(out, images, 1);
    CHECK(images[kShipImageSlotBase + 1] == kDcNone);
    REQUIRE(out.deleted.size() == 1);
    CHECK(out.deleted[0] == 0x601);
    // Old slot 2's ship is new slot 1: slot 1 takes saved[2], and slot 2 still holds that DC,
    // so its words are zeroed without a free [BYTES seg14:0x9a0-0x9df]
    cache.set_words(images, 1, saved[2]);
    if (saved[2] == cache.words(images, 2)) cache.set_words(images, 2, kDcNone);
    CHECK(images[kShipImageSlotBase + 1] == 0x602);
    CHECK(images[kShipImageSlotBase + 2] == kDcNone);
    REQUIRE(sheet_copy(1) != nullptr);
    CHECK(sheet_copy(1)->index == b.index);
    CHECK(sheet_copy(2) == nullptr);
    CHECK(out.deleted.size() == 1);
    // A swap of two kept ships reads the words saved at the entry
    images[kShipImageSlotBase + 3] = 0x603;
    images[kShipImageSlotBase + 4] = 0x604;
    set_sheet_copy(3, a);
    set_sheet_copy(4, b);
    const std::uint16_t s3 = cache.words(images, 3), s4 = cache.words(images, 4);
    cache.set_words(images, 3, s4);
    cache.set_words(images, 4, s3);
    CHECK(images[kShipImageSlotBase + 3] == 0x604);
    CHECK(images[kShipImageSlotBase + 4] == 0x603);
    REQUIRE(sheet_copy(3) != nullptr);
    REQUIRE(sheet_copy(4) != nullptr);
    CHECK(sheet_copy(3)->index == b.index);
    CHECK(sheet_copy(4)->index == a.index);
    clear_sheet_copies();
}

TEST_CASE("ship sheet: the build and the tail halves of seg14:0x0's per-ship loop") {
    RecordingBlitter out;
    ImageSlotTable images{};
    clear_sheet_copies();
    clear_design_sheet_sources();
    World w;
    w.size = 1;
    w.player = 0;
    w.last_ship = 2;
    ShipRecord& s = w.ship(2);
    s.design_index = 2;
    s.roster_index = -1;
    s.has_flame = 1;
    w.design_ratings[2].special = -1;
    DesignSheetSource src;
    src.graphic_id = 4002;
    src.bod_19 = 3;
    set_design_sheet_source(2, src);
    SheetRenderer r = [&](const SheetRequest& q, SheetPixels& px) {
        const SheetGeometry g = sheet_geometry(66, 33, q.gap_word, q.mode, q.scale);
        px.w = g.w;
        px.h = g.h;
        px.index.assign(static_cast<std::size_t>(g.w) * g.h, 9);
        px.source_height = 33;
        return true;
    };
    WeaponWords words;
    PlaceSheetCtx c{out, images, w, words, r, {}, nullptr, 0, 3, 0};
    ObjectRecord& o = w.ship_object(2);
    o.size = 33;
    o.gap = 5;
    // RosterHost::purchases: seg16:0x3468(2, 3) builds into slot 0x2a
    const PlacedSheet b = ship_sheet_build(c, 2, true);
    CHECK(b.rebuilt);
    REQUIRE(b.build.rendered);
    CHECK_FALSE(b.has_w04);
    CHECK(ship_sheet_height(2) == b.build.bm_height);
    CHECK(ship_sheet_height(5) == 0);
    // RosterHost::ship_tail: object +0x3a = the slot's DC and design word +0x04 [BYTES seg14:0xc42-0xc68]
    o.w3a = 0;
    const PlacedSheet t = ship_sheet_flame(c, 2);
    CHECK(o.w3a == static_cast<std::int16_t>(images[kShipImageSlotBase + 2]));
    CHECK(t.has_w04);
    CHECK(t.w04 == flame_radius_word(3, 33));
    // Size 0 with an odd raw side: one more [BYTES seg14:0xbb5-0xbc2]
    w.size = 0;
    CHECK(ship_sheet_flame(c, 2).w04 == flame_radius_word(3, 34));
    // A ship whose slot holds no sheet gets nothing
    CHECK_FALSE(ship_sheet_flame(c, 1).has_w04);
    clear_sheet_copies();
    clear_design_sheet_sources();
}

TEST_CASE("call-in: the program's host joins DGROUP:0x964c and DGROUP:0x8e2c through the links") {
    World w;
    two_ship_wave(w);
    CallInLinks saved = callin_links();
    std::int16_t word964c = 5;
    std::uint32_t word8e2c = 0;
    callin_links().called_in = [&] { return &word964c; };
    callin_links().callin_frame = [&] { return &word8e2c; };
    CallInHost& h = linked_callin_host(w);
    CHECK(h.called_in() == 5);
    h.called_in() = 6;
    CHECK(word964c == 6);
    h.set_callin_frame(1234);
    CHECK(word8e2c == 1234u);
    callin_links() = saved;
    // Unset, callin_words() holds them
    callin_words() = CallInWords{};
    linked_callin_host(w).called_in() = 2;
    CHECK(callin_words().called_in == 2);
    callin_words() = CallInWords{};
}
