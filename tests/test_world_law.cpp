// The law (src/world/law.h): the crime routine seg13:0xb6f2 and the warrant seg13:0xb93a,
// against numbers worked from the listing (docs/re/law.md "The crime routine", "The warrant";
// work/analysis/m5/s4-crime.md sections 3 and 5 with s4-crime-verify.md claims 17-24, 58-61) and
// the oracle's counters (oracle/digests/ships-0003.md, ships-0004.md) (M5 stage 4 part B, track
// LAW).
#include <array>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/callin.h"
#include "world/cheats.h"
#include "world/law.h"
#include "world/pilot.h"
#include "world/relations.h"
#include "world/world.h"

using namespace is::world;

namespace {

using Msg = std::tuple<int, int, int, int>;
using Call = std::tuple<int, int, int>;

// Records the calls out; the call-in answers `accept` and, when `grow` is set, enters a new ship
// at DGROUP:0x9316 + 1 as seg13:0xae68 does. It notes DGROUP:0xa163 at the call (the race gate
// reads it).
class Host : public LawHost {
public:
    World* w = nullptr;
    std::uint8_t* a163 = nullptr;
    std::int16_t accept = 1;
    bool grow = false;
    std::vector<Call> calls;
    std::vector<int> a163_at_call;
    std::vector<Msg> msgs;
    std::int16_t call_in(int ship, int kind, int msg) override {
        calls.emplace_back(ship, kind, msg);
        a163_at_call.push_back(a163 ? *a163 : -1);
        if (accept != 0 && grow && w) w->last_ship = add16(w->last_ship, 1);
        return accept;
    }
    void message(int r, int p, int rec, int code) override { msgs.emplace_back(r, p, rec, code); }
};

// The player 0 (team 0) and ship 1 (team 3); Enforcers are added with enforcer(). Every ship of
// design i, in play (`state` 0), alive; all at the origin unless placed.
struct Rig {
    World w;
    Host h;
    std::int16_t w9648 = 0;
    std::uint8_t a163 = 0;
    std::int16_t w104a = 0;
    LawCtx c{w, h, w9648, a163, w104a};

    Rig() {
        h.w = &w;
        h.a163 = &a163;
        w.player = 0;
        w.last_ship = 1;
        w.kind = 0;
        w.pools.resize(kPoolShips, 8);
        w.pools.set_in_use(kPoolShips, 2);
        for (int i = 0; i < kShipCount; ++i) {
            w.ships[static_cast<std::size_t>(i)].design_index = static_cast<std::int16_t>(i);
            w.ships[static_cast<std::size_t>(i)].bec = 1;
            w.pools.rec(kPoolShips, i).state = 0;
        }
        w.design_ratings[1].team = 3;
    }
    // Ship i an Enforcer of class cls, with DGROUP:0x9316 raised to cover it.
    void enforcer(int i, int cls) {
        w.design_ratings[static_cast<std::size_t>(i)].team = 7;
        set_ship_sb(w.ships[static_cast<std::size_t>(i)], pw::kClass, static_cast<std::int8_t>(cls));
        if (w.last_ship < i) w.last_ship = static_cast<std::int16_t>(i);
    }
    ObjectRecord& o(int i) { return w.pools.rec(kPoolShips, i); }
    ShipRecord& s(int i) { return w.ships[static_cast<std::size_t>(i)]; }
    void forced(int code) {
        w104a = 1;  // F6: DGROUP:0x104a = 1 [BYTES seg11:0x733b]
        crime_report(c, w.player, code);
    }
};

}  // namespace

TEST_CASE("seg13:0xb6f2's warning per crime, the table seg13:0xb761") {
    const std::array<int, 10> want{0, 0x45, 0x46, 0x5d, 0x65, 0x72, 0x86, 0, 0xb9, 0xc0};
    for (int c = 0; c <= 9; ++c) {
        bool written = false;
        CHECK(crime_message(c, written) == want[static_cast<std::size_t>(c)]);
        CHECK(written == (c != 0 && c != 7));
    }
    bool written = true;
    CHECK(crime_message(10, written) == 0);  // `dec ax; cmp ax, 8; ja` [BYTES seg13:0xb753-0xb757]
    CHECK_FALSE(written);
    CHECK(crime_message(-1, written) == 0);  // unsigned: 0xfffe > 8
    CHECK_FALSE(written);
}

TEST_CASE("seg13:0xb6f2: nothing in a duel, nor crime 5 while DGROUP:0xa163 = 5") {
    Rig r;
    r.w.kind = 7;
    r.forced(2);
    CHECK(r.w.w964a == 0);
    CHECK(r.w9648 == 0);
    CHECK(r.w104a == 1);  // the return comes before the forced path's clear [BYTES seg13:0xb705-0xb70a]
    CHECK(r.h.calls.empty());
    r.w.kind = 0;
    r.a163 = 5;
    r.forced(5);
    CHECK(r.w.w964a == 0);
    CHECK(r.h.calls.empty());
    CHECK(r.w.ba14d[5] == 0);
}

TEST_CASE("seg13:0xb6f2's forced path: the counters of ships-0003 and ships-0004") {
    // Each crime of F6 from a fresh wave: the player's +1 (not for 9), +1 for 6 or +3, +4 for 2,
    // +2 forced: 0x0006 after crimes 1, 3, 5, 8, 0x000a after 2, 0x0005 after 9 [ORACLE ships-0004].
    const std::array<std::int16_t, 10> after{6, 6, 10, 6, 6, 6, 4, 6, 6, 5};
    for (int code = 0; code <= 9; ++code) {
        CAPTURE(code);
        Rig r;
        r.forced(code);
        CHECK(r.w.w964a == after[static_cast<std::size_t>(code)]);
        CHECK(r.w9648 == after[static_cast<std::size_t>(code)]);
        CHECK(r.a163 == code);
        CHECK(r.w.ba14d[static_cast<std::size_t>(code)] == 1);
        CHECK(r.w104a == 0);
        // The Interceptor: kind 0x15 with no message whatever the crime [BYTES seg13:0xb7d0-0xb7d8].
        REQUIRE(r.h.calls.size() == 1);
        CHECK(r.h.calls[0] == Call{0, kCallInInterceptor, -1});
        CHECK(r.h.a163_at_call[0] == code);  // DGROUP:0xa163 is stored before the call [BYTES seg13:0xb7cd]
        bool written = false;
        REQUIRE(r.h.msgs.size() == 1);
        CHECK(r.h.msgs[0] == Msg{0, 14, 1, crime_message(code, written)});
    }
    // ships-0004 a4 m1: three F6 drawing 4, 4, 9: DGROUP:0x9648 = 0x11 and the counts [0 0 0 0 2 0
    // 0 0 0 1]; m2: five F6 drawing 3, 8, 8, 4, 8: 0x1e [ORACLE ships-0004].
    Rig m1;
    for (int code : {4, 4, 9}) m1.forced(code);
    CHECK(m1.w9648 == 0x11);
    CHECK(m1.w.ba14d == std::array<std::uint8_t, 10>{0, 0, 0, 0, 2, 0, 0, 0, 0, 1});
    Rig m2;
    for (int code : {3, 8, 8, 4, 8}) m2.forced(code);
    CHECK(m2.w9648 == 0x1e);
    CHECK(m2.w.ba14d == std::array<std::uint8_t, 10>{0, 0, 0, 1, 1, 0, 0, 0, 3, 0});
}

TEST_CASE("seg13:0xb6f2: the forced warning comes from DGROUP:0x9316 after the call-in") {
    // ships-0003 k1: F6 drew 2; the panel shows "INTERCEPTOR ... THAT WAS EVIDENCE!" from the
    // new ship 3 (DGROUP:0x9316 2 -> 3) [ORACLE ships-0003].
    Rig r;
    r.w.last_ship = 2;
    r.h.grow = true;
    r.forced(2);
    REQUIRE(r.h.msgs.size() == 1);
    CHECK(r.h.msgs[0] == Msg{0, 14, 3, 0x46});
    CHECK(r.w.w964a == 10);
    // A refused call-in (a Grand Prix with crime 7, ships-0004 a5 s5: no ship) still sends the
    // forced line from the last ship, here with crime 7's unwritten word read as 0 [DESIGN ADR
    // 0016 decision 1] (the panel without a text line of ships-0003 k5).
    Rig q;
    q.h.accept = 0;
    q.forced(7);
    REQUIRE(q.h.msgs.size() == 1);
    CHECK(q.h.msgs[0] == Msg{0, 14, 1, 0});
    CHECK(q.w.ba14d[7] == 1);
    CHECK(q.a163 == 7);
}

TEST_CASE("seg13:0xb6f2: crime 8 always takes the forced path; a computer ship's crime") {
    Rig r;
    crime_report(r.c, 1, 8);  // ship 1, not the player, without F6
    CHECK(r.w.w964a == 5);    // +3 +2, no player +1
    CHECK(r.a163 == 8);
    CHECK(r.w.ba14d[8] == 0);  // the bytes count the player's crimes only
    REQUIRE(r.h.calls.size() == 1);
    CHECK(r.h.calls[0] == Call{1, kCallInInterceptor, -1});
    REQUIRE(r.h.msgs.size() == 1);
    CHECK(r.h.msgs[0] == Msg{1, 14, 1, 0xb9});  // to ship 1: seg3:0x6ec shows only the player's
}

TEST_CASE("seg13:0xb6f2's witness: the first Enforcer in state 0, not arresting, near s") {
    Rig r;
    r.enforcer(2, 2);  // a PATROLMAN
    r.enforcer(3, 1);
    // A witness at the far corner of the 3200-cell world: 1600 cells on each axis is 2266 cells
    // by seg8:0x285a, inside 0x960 [BYTES seg13:0xb8b0-0xb8b3].
    r.o(2).x = 12800;
    r.o(2).y = 12800;
    crime_report(r.c, 0, 1);
    REQUIRE(r.h.calls.size() == 1);
    CHECK(r.h.calls[0] == Call{0, kCallInInterceptor, -1});
    REQUIRE(r.h.msgs.size() == 1);
    CHECK(r.h.msgs[0] == Msg{0, 14, 2, 0x45});  // 570 from the witness
    CHECK(r.a163 == 1);
    CHECK(r.w.ba14d[1] == 1);
    CHECK(r.w.w964a == 4);
    // An arresting (0x16) or terminating (0x17) Enforcer does not witness; one out of play
    // neither; the next one does, and a dead one in `state` 0 does (no `alive` test).
    Rig q;
    q.enforcer(2, 2);
    q.enforcer(3, 2);
    q.enforcer(4, 2);
    q.enforcer(5, 2);
    q.s(2).w52 = 0x16;
    q.s(3).w52 = 0x17;
    q.o(4).state = 1;
    q.s(5).bec = 0;
    crime_report(q.c, 0, 4);
    REQUIRE(q.h.msgs.size() == 1);
    CHECK(q.h.msgs[0] == Msg{0, 14, 5, 0x65});
    // An Enforcer is its own witness at distance 0.
    Rig e;
    e.enforcer(1, 2);
    crime_report(e.c, 1, 2);
    REQUIRE(e.h.msgs.size() == 1);
    CHECK(e.h.msgs[0] == Msg{1, 14, 1, 0x46});
}

TEST_CASE("seg13:0xb6f2: a refused call-in, crime 6's served test, nobody to see it") {
    Rig r;
    r.enforcer(2, 2);
    r.h.accept = 0;
    crime_report(r.c, 0, 3);
    REQUIRE(r.h.msgs.size() == 1);
    CHECK(r.h.msgs[0] == Msg{0, 14, 2, 0x44});  // 559, absent: the voice only
    CHECK(r.a163 == 0);
    CHECK(r.w.ba14d[3] == 0);
    CHECK(r.w.w964a == 4);  // the words rose before the call
    // Crime 6 with the player's count of 6 above the served one: the witness is passed over.
    Rig q;
    q.enforcer(2, 2);
    q.w.ba14d[6] = 2;
    q.w.ba157[6] = 1;
    crime_report(q.c, 0, 6);
    CHECK(q.h.calls.empty());
    CHECK(q.w9648 == 2);  // +1 player, +1 for 6
    q.w.ba157[6] = 2;     // equal: seen
    crime_report(q.c, 0, 6);
    CHECK(q.h.calls.size() == 1);
    CHECK(q.w.ba14d[6] == 3);
    // Nobody: line 707 to the player once DGROUP:0x9648 is above 4, then 1001 holds it off.
    Rig n;
    crime_report(n.c, 1, 1);  // a computer ship's crime: +3
    CHECK(n.w9648 == 3);
    CHECK(n.h.msgs.empty());
    crime_report(n.c, 1, 1);  // 6 > 4
    REQUIRE(n.h.msgs.size() == 1);
    CHECK(n.h.msgs[0] == Msg{0, 7, 0, 0x6e});  // to the player, whoever s is [BYTES seg13:0xb91a-0xb924]
    CHECK(n.w9648 == 1001);
    CHECK(n.w.w964a == 6);
    crime_report(n.c, 1, 1);
    CHECK(n.h.msgs.size() == 1);
    CHECK(n.w9648 == 1004);
}

TEST_CASE("seg13:0xb6f2: the 16-bit words and the 8-bit counts wrap") {
    Rig r;
    r.w.w964a = 0x7ffe;
    r.w.ba14d[2] = 0xff;
    r.forced(2);
    CHECK(r.w.w964a == static_cast<std::int16_t>(0x7ffe + 10 - 0x10000));
    CHECK(r.w.ba14d[2] == 0);
}

TEST_CASE("seg13:0xb6f2 with the call-in: the order of SIRENS, HYPER, the entry and the line") {
    // ships-0004: SIRENS (0x70) within 10 ms of F6, the entering ship's HYPER (0x27), then the
    // Interceptor's line on the panel [ORACLE ships-0004]; seg13:0xae68 builds an Interceptor
    // (class 1, team 7, order 0x16) for kind 0x15 [BYTES seg13:0xaecd].
    struct Ev {
        std::vector<std::string> log;
    } ev;
    class CallHost : public CallInHost {
    public:
        World* w = nullptr;
        Ev* ev = nullptr;
        is::shell::Rng r;
        is::shell::Rng& rng() override { return r; }
        std::int16_t team(int design) override { return w->design_ratings[static_cast<std::size_t>(design)].team; }
        void play(int id) override { ev->log.push_back("play " + std::to_string(id)); }
        void sound(int id, int, int rec, int) override {
            ev->log.push_back("sound " + std::to_string(id) + " " + std::to_string(rec));
        }
        void music(int t, int) override { ev->log.push_back("music " + std::to_string(t)); }
        void entered(int ship) override { ev->log.push_back("entered " + std::to_string(ship)); }
        std::int16_t called = 0;  // DGROUP:0x964c, the test's own
        std::int16_t& called_in() override { return called; }
        void set_callin_frame(std::uint32_t) override {}
        void build_roster(int mode) override {  // seg14:0x0(0x15): one Interceptor at DGROUP:0x9316 + 1
            ev->log.push_back("build " + std::to_string(mode));
            w->last_ship = add16(w->last_ship, 1);
            const int n = w->last_ship;
            w->ships[static_cast<std::size_t>(n)].design_index = static_cast<std::int16_t>(n);
            w->ships[static_cast<std::size_t>(n)].bec = 1;
            w->design_ratings[static_cast<std::size_t>(n)].team = 7;
            set_ship_sb(w->ships[static_cast<std::size_t>(n)], pw::kClass, 1);
            w->pools.rec(kPoolShips, n).state = 100;
        }
    };
    class Linked : public LawHost {
    public:
        World* w = nullptr;
        CallHost* ch = nullptr;
        Ev* ev = nullptr;
        std::uint8_t* a163 = nullptr;
        std::int16_t call_in(int ship, int kind, int msg) override {
            return ::is::world::call_in(*w, *ch, ship, kind, msg, nullptr, nullptr, *a163);
        }
        void message(int r, int, int rec, int code) override {
            ev->log.push_back("message " + std::to_string(r) + " " + std::to_string(rec) + " " + std::to_string(code));
        }
    };
    Rig r;
    r.w.last_ship = 2;
    r.w.detail = 3;
    r.w.pools.set_in_use(kPoolShips, 3);
    CallHost ch;
    ch.w = &r.w;
    ch.ev = &ev;
    Linked lh;
    lh.w = &r.w;
    lh.ch = &ch;
    lh.ev = &ev;
    lh.a163 = &r.a163;
    LawCtx c{r.w, lh, r.w9648, r.a163, r.w104a};
    r.w104a = 1;
    crime_report(c, 0, 2);
    const std::vector<std::string> want{"play 112", "build 21", "sound 39 3", "entered 3", "music 2", "message 0 3 70"};
    CHECK(ev.log == want);
    const Pilot I = pilot(r.w.ships, 3);
    CHECK(I.order() == 0x16);  // the arrest order
    CHECK(I.order_pool() == 14);
    CHECK(I.order_rec() == 0);
    CHECK(I.cls() == 1);       // the Interceptor class for every crime
    // In a race only crime 8 lets the Interceptor in (DGROUP:0xa163 = 8 at the call).
    ev.log.clear();
    r.w.kind = 6;
    r.w.last_ship = 2;
    r.w104a = 1;
    crime_report(c, 0, 7);
    CHECK(ev.log == std::vector<std::string>{"message 0 2 0"});
    ev.log.clear();
    crime_report(c, 0, 8);
    CHECK(ev.log.size() == 6);
    CHECK(ev.log.front() == "play 112");
}

TEST_CASE("seg13:0xb93a: the warrant from a Patrol-class Enforcer on screen") {
    Rig r;
    r.enforcer(2, 2);  // a PATROLMAN
    r.o(2).on_screen = 1;
    r.w.ba161 = 0xff;  // seg9:0xc06 after a start or a restore [BYTES seg9:0xd33]
    // ships-0004 a6 g1: the escape leaves [0 0 0 0 0 1 0 0 0 0] against all 0 [ORACLE ships-0004].
    r.w.ba14d[5] = 1;
    r.a163 = 5;
    warrant_check(r.c);
    REQUIRE(r.h.calls.size() == 1);
    CHECK(r.h.calls[0] == Call{0, kCallInInterceptor, -1});
    REQUIRE(r.h.msgs.size() == 1);
    CHECK(r.h.msgs[0] == Msg{0, 14, 2, 0xae});  // 1940 from the Patrolman
    CHECK(r.a163 == 0xff);                      // the last arrest's crime: "crime -1"
    // Refused: no line, DGROUP:0xa163 kept, and no later ship is tried.
    Rig q;
    q.enforcer(2, 2);
    q.enforcer(3, 3);
    q.o(2).on_screen = 1;
    q.o(3).on_screen = 1;
    q.w.ba14d[1] = 1;
    q.h.accept = 0;
    q.a163 = 4;
    warrant_check(q.c);
    CHECK(q.h.calls.size() == 1);
    CHECK(q.h.msgs.empty());
    CHECK(q.a163 == 4);
}

TEST_CASE("seg13:0xb93a: who can serve it and what counts") {
    const auto calls = [](auto setup) {
        Rig r;
        r.enforcer(2, 2);
        r.o(2).on_screen = 1;
        r.w.ba14d[0] = 1;
        setup(r);
        warrant_check(r.c);
        return r.h.calls.size();
    };
    CHECK(calls([](Rig&) {}) == 1);
    CHECK(calls([](Rig& r) { set_ship_sb(r.s(2), pw::kClass, 1); }) == 0);  // an Interceptor: `jle 1`
    CHECK(calls([](Rig& r) { r.o(2).on_screen = 0; }) == 0);               // off screen
    CHECK(calls([](Rig& r) { r.s(2).bec = 0; }) == 0);                     // dead
    CHECK(calls([](Rig& r) { r.o(2).state = 1; }) == 0);                   // out of play
    CHECK(calls([](Rig& r) { r.w.ba157[0] = 1; }) == 0);                   // served
    // Crime 9 is never a warrant: the sums leave c = 9 out [BYTES seg13:0xb950-0xb954].
    CHECK(calls([](Rig& r) {
              r.w.ba14d[0] = 0;
              r.w.ba14d[9] = 5;
          }) == 0);
    // The bytes are signed (`cbw`): a served count of 0xff is -1, below an unserved 0.
    CHECK(calls([](Rig& r) {
              r.w.ba14d[0] = 0;
              r.w.ba157[3] = 0xff;
          }) == 1);
    // 0x80 unserved is -128: below the served 0.
    CHECK(calls([](Rig& r) { r.w.ba14d[0] = 0x80; }) == 0);
}

TEST_CASE("crime_report_program: F6 on the program's words (cheat_state().w104a)") {
    World w;
    w.player = 0;
    w.last_ship = 0;
    w.detail = -4;  // Detail + 3 < DGROUP:0x9316: the call-in refuses before any draw [BYTES seg13:0xaeae-0xaeb8]
    contact_words().w9648 = 0;
    contact_words().ba163 = 0;
    cheat_state().w104a = 1;          // F6 [BYTES seg11:0x733b]
    crime_report_program(w, 0, 9);    // the message is unbound here and logs once
    CHECK(cheat_state().w104a == 0);  // [BYTES seg13:0xb801]
    CHECK(contact_words().w9648 == 5);
    CHECK(contact_words().ba163 == 9);
    CHECK(w.ba14d[9] == 1);
    CHECK(w.w964a == 5);
    contact_words().w9648 = 0;
    contact_words().ba163 = 0;
}
