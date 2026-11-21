// The arrest (src/world/pilot_orders.h enforcer_orders, order 0x16, seg13:0xa98a-0xac34) and
// the prisoner's gate passage (src/world/gate_exit.h, seg12:0x23fd-0x24b1), against numbers
// worked from the listing (docs/re/law.md "The Enforcer's executor" step 4 and "The HANDCUFFS,
// the escort and the gate"; work/analysis/m5/s4-crime.md section 4 with s4-crime-verify.md claims
// 41-49) and the oracle's arrest of ships-0003 k1 and ships-0004 (M5 stage 4 part B, track LAW).
#include <array>
#include <cstdint>
#include <tuple>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/flight.h"
#include "world/gate_exit.h"
#include "world/law.h"
#include "world/pilot.h"
#include "world/pilot_orders.h"
#include "world/pilot_think.h"
#include "world/relations.h"
#include "world/world.h"

using namespace is::world;

namespace {

using Msg = std::tuple<int, int, int, int>;

class Host : public PilotHost {
public:
    std::array<std::int16_t, 8> team{};  // by design index
    std::vector<Msg> msgs;
    std::vector<std::tuple<int, int, int>> throws, defenders, targets, calls;
    World* w = nullptr;
    std::int16_t hook_after_throw = 10;  // the pool-14 object +0x36 seg4:0x584c leaves (10: the hook flies)

    std::int16_t design_word(int n, int off) override {
        if (off == 0x38 && n >= 0 && n < 8) return team[static_cast<std::size_t>(n)];
        return 0;
    }
    void message(int to, int pool, int rec, int code) override { msgs.emplace_back(to, pool, rec, code); }
    void weapon_use(int k, int s, int life) override {
        throws.emplace_back(k, s, life);
        if (w) w->pools.rec(kPoolShips, s).w36 = hook_after_throw;
    }
    void send_defenders(int i, int p, int r) override { defenders.emplace_back(i, p, r); }
    void target(int s, int p, int r) override { targets.emplace_back(s, p, r); }
    std::int16_t call_in(int s, int kind, int msg) override {
        calls.emplace_back(s, kind, msg);
        return 1;
    }
};

// ships-0003 k1: the player 0 (FUZZY ONES, team 2), ship 1 (team 3) and the Interceptor 3
// (team 7, class 1) entered by F6 for crime 2, ordered 0x16 on (14, 0) with +0xed = 1; its
// HANDCUFFS in weapon slot 1 (+0xcb = 1) [ORACLE ships-0003].
struct Rig {
    World w;
    WaveGlobals g;
    PilotGlobals pg;
    is::shell::Rng rng;
    Host host;
    PilotCtx c{w.pools, w.ships, g, w, pg, rng, host};

    Rig() {
        host.w = &w;
        host.team = {2, 3, 0, 7, 0, 0, 0, 0};
        g.now = 1000000;
        g.player = 0;
        g.demo = 1;  // a player game (DGROUP:0x95bc != 0)
        g.last_ship = 3;
        g.game_frames = 1482;
        w.player = 0;
        w.last_ship = 3;
        w.wave = 3;
        for (int p = 0; p < kPoolCount; ++p)
            if (w.pools.exists(p)) w.pools.set_in_use(p, 0);
        w.pools.set_in_use(kPoolShips, 4);
        for (int i = 0; i < kShipCount; ++i) {
            w.ships[static_cast<std::size_t>(i)].design_index = static_cast<std::int16_t>(i);
            w.ships[static_cast<std::size_t>(i)].bec = 1;
            o(i).w36 = -1;
            o(i).owner_pool = -1;
        }
        Pilot E = p(3);
        set_ship_sb(s(3), pw::kClass, 1);
        E.order(0x16);
        E.order_pool(14);
        E.order_rec(0);
        E.order_by(3);
        E.one_shot(1);
        E.order_until(g.now + 5000u);
        E.slots(1);
        set_ship_b(s(3), 0xb8 + 1, 15);  // HANDCUFFS, weapon 15
        away();
        ContactWords& cw = contact_words();
        cw.ba163 = 2;
        cw.da147 = 0;
        cw.wa14b = 0;
        law_words().ba162 = 0;
    }
    ~Rig() {
        ContactWords& cw = contact_words();
        cw.ba163 = 0;
        cw.da147 = 0;
        cw.wa14b = 0;
        law_words().ba162 = 0;
    }
    ShipRecord& s(int i) { return w.ships[static_cast<std::size_t>(i)]; }
    Pilot p(int i) { return pilot(w.ships, i); }
    ObjectRecord& o(int i) { return w.pools.rec(kPoolShips, i); }
    // The Interceptor 2000 cells right of the player, then near it: cells x 8 world units.
    void away() { o(3).x = static_cast<std::int16_t>(o(0).x + 8 * 2000); }
    void at_cells(int cells) { o(3).x = static_cast<std::int16_t>(o(0).x + 8 * cells); }
    void replan() { enforcer_orders(c, 3); }
};

}  // namespace

TEST_CASE("seg13:0xa98a: the approach, line 552 with the arrest record, the throw (ships-0003 k1)") {
    Rig r;
    Pilot E = r.p(3);
    // Far off: follow (14, 0) with +0x36 = 3, +0x3a = 2; nothing said; +0xed stays 1.
    r.replan();
    CHECK(E.replan_at() == 1000750u);
    CHECK(E.est_pool() == 14);
    CHECK(E.est_rec() == 0);
    CHECK(E.est_act() == 3);
    CHECK(E.est_app() == 2);
    CHECK(E.one_shot() == 1);
    CHECK(r.host.msgs.empty());
    CHECK(r.host.throws.empty());
    // 250 cells is outside (`jge`), 249 inside [BYTES seg13:0xaacb-0xaace].
    r.at_cells(250);
    r.replan();
    CHECK(r.host.msgs.empty());
    r.at_cells(249);
    r.replan();
    // 552 "Follow Me Out." from the Interceptor to the player; the record: frame 1482, wave 3,
    // crime 2, ship 3 (k1: DGROUP:0xa147 = 1482, 0xa14b = 3, 0xa161 = 2, 0xa162 = 3).
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0] == Msg{0, 14, 3, 0x3d});
    CHECK(E.one_shot() == 0);
    CHECK(contact_words().da147 == 1482u);
    CHECK(contact_words().wa14b == 3);
    CHECK(r.w.ba161 == 2);
    CHECK(law_words().ba162 == 3);
    // The player in a player game does not answer (B3): its words +0x52 ... +0x58 stay 0, as the
    // player's record reads through the ten arrests of ships-0004 a3 [ORACLE ships-0004]; the
    // HANDCUFFS of slot 1 fly.
    CHECK(r.p(0).order() == 0);
    CHECK(r.p(0).order_pool() == 0);
    CHECK(r.p(0).order_rec() == 0);
    CHECK(r.p(0).order_by() == 0);
    REQUIRE(r.host.throws.size() == 1);
    CHECK(r.host.throws[0] == std::make_tuple(1, 3, 0));
    CHECK(r.o(3).w36 == 10);
    // While the hook flies: +0xed = 0 is the gate estimate (12, 0), and the tail keeps 0: the
    // goal flips (14, P) <-> (12, 0) with +0x36 3 <-> 2 [ORACLE ships-0003].
    r.replan();
    CHECK(E.est_pool() == 12);
    CHECK(E.est_rec() == 0);
    CHECK(E.est_act() == 2);
    CHECK(E.est_app() == 2);
    CHECK(E.one_shot() == 0);
    CHECK(r.host.throws.size() == 1);
    // A miss (the hook back to -1): the tail re-arms +0xed = 1; the next re-plan inside 250
    // cells says 552 again and rewrites the record (k1: 1482, 1676, 1866, ...).
    r.o(3).w36 = -1;
    r.replan();
    CHECK(E.one_shot() == 1);
    r.g.game_frames = 1676;
    r.replan();
    CHECK(r.host.msgs.size() == 2);
    CHECK(contact_words().da147 == 1676u);
    CHECK(r.host.throws.size() == 2);
    // The hold: the Interceptor's object (+0x36, +0x38) = (14, 0): to gate 0, +0xed stays 0.
    r.o(3).w36 = 14;
    r.o(3).w38 = 0;
    r.replan();
    CHECK(E.est_pool() == 12);
    CHECK(E.est_rec() == 0);
    CHECK(E.one_shot() == 0);
    r.replan();
    CHECK(E.one_shot() == 0);
    CHECK(r.host.msgs.size() == 2);
    CHECK(r.host.throws.size() == 2);
}

TEST_CASE("seg13:0xa9c6: the prisoner resists; crime 7; 553; the re-arrest and 578") {
    Rig r;
    Pilot E = r.p(3);
    Pilot P = r.p(0);
    r.at_cells(100);
    // The player attacks the Interceptor (goal (14, 3), +0x38 = 1) and has hit it twice; +0x98
    // has passed.
    P.goal_pool(14);
    P.goal_rec(3);
    P.goal_act(1);
    E.hit(0, 100);
    E.order_until(r.g.now);  // not below now: `jb` [BYTES seg13:0xa9e3-0xa9e8]
    r.replan();
    CHECK(E.order() == 0x16);  // not yet
    E.order_until(r.g.now - 1u);
    r.replan();
    CHECK(E.order() == 0x16);  // one stamp is not enough (`jle 1`)
    // (The first of the two re-plans above said 552 and threw; the second, with +0xed = 0 and the
    // hook flying, only set the gate estimate.)
    CHECK(r.host.msgs.size() == 1);
    CHECK(r.host.throws.size() == 1);
    r.host.msgs.clear();
    E.one_shot(0);
    E.hit(3, 7);
    E.order_until(r.g.now - 1u);
    r.w.ba14d[7] = 0;
    r.replan();
    CHECK(E.order() == 0x17);
    CHECK(E.one_shot() == 1);  // 0 -> 1; 2 or -1 -> 2
    CHECK(E.order_until() == r.g.now + 20000u);
    CHECK(E.replan_at() == r.g.now);
    CHECK(r.w.ba14d[7] == 1);  // DGROUP:0xa154 [BYTES seg13:0xaa22]
    CHECK(r.host.msgs.empty());
    // Terminate: 553 "Alright Tough Guy, I'll Resort To Force." to the player, attack.
    r.replan();
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0] == Msg{0, 14, 3, 0x3e});
    CHECK(E.one_shot() == 0);
    CHECK(E.est_pool() == 14);
    CHECK(E.est_rec() == 0);
    CHECK(E.est_act() == 1);
    // 20 s on: an Interceptor goes back to the arrest with +0xed = 2 [BYTES seg13:0xacf4-0xad36].
    r.g.now += 20001u;
    r.replan();
    CHECK(E.order() == 0x16);
    CHECK(E.one_shot() == 2);
    CHECK(E.order_until() == r.g.now + 5000u);
    // The player no longer attacks: within 250 cells +0xed = 2 says 578 and stores -1, and throws.
    P.goal_act(3);
    r.host.msgs.clear();
    r.host.throws.clear();
    r.replan();
    REQUIRE(r.host.msgs.size() == 1);
    CHECK(r.host.msgs[0] == Msg{0, 14, 3, 0x57});
    CHECK(E.one_shot() == -1);
    CHECK(r.host.throws.size() == 1);
    // Resisting again with +0xed = -1 keeps 2.
    P.goal_act(1);
    r.o(3).w36 = -1;
    E.one_shot(-1);
    E.order_until(r.g.now - 1u);
    r.replan();
    CHECK(E.order() == 0x17);
    CHECK(E.one_shot() == 2);
    CHECK(r.w.ba14d[7] == 2);
}

TEST_CASE("seg13:0xab28: a computer prisoner fights or leaves; the player in the demo answers") {
    const auto arrest = [](std::int16_t team, std::int8_t b5e, int t, std::int16_t demo) {
        Rig r;
        r.host.team[1] = team;
        r.g.demo = demo;
        set_ship_sb(r.s(t), pw::kClass, b5e);
        Pilot E = r.p(3);
        E.order_rec(static_cast<std::int16_t>(t));
        r.o(3).x = static_cast<std::int16_t>(r.o(t).x + 8 * 10);
        r.replan();
        return std::make_tuple(r.p(t).order(), r.p(t).order_pool(), r.p(t).order_rec(), r.p(t).order_by(),
                               r.p(t).order_until(), ship_sb(r.s(t), pw::kReaction), r.host.defenders.size(),
                               r.g.now);
    };
    // PIRATES (3), RENEGADES (6), PREDATORS (1): order 2 against the Interceptor, +0x58 = t,
    // seg4:0x8346(t, 14, e, 10) [BYTES seg13:0xab80-0xaba7].
    for (std::int16_t team : {3, 6, 1}) {
        CAPTURE(team);
        const auto [o, op, orec, by, until, react, defs, now] = arrest(team, 2, 1, 1);
        CHECK(o == 2);
        CHECK(op == 14);
        CHECK(orec == 3);
        CHECK(by == 1);
        CHECK(react == 10);
        CHECK(defs == 1u);
        (void)until, (void)now;
    }
    // Any other team leaves: order 10, +0x98 = now + 500, +0x58 = e [BYTES seg13:0xab62-0xab7a].
    {
        const auto [o, op, orec, by, until, react, defs, now] = arrest(0, 2, 1, 1);
        CHECK(o == 10);
        CHECK(by == 3);
        CHECK(until == now + 500u);
        CHECK(defs == 0u);
        (void)op, (void)orec, (void)react;
    }
    // Byte +0x5e <= 0 (signed): no answer.
    {
        const auto [o, op, orec, by, until, react, defs, now] = arrest(3, 0, 1, 1);
        CHECK(o == 0);
        (void)op, (void)orec, (void)by, (void)until, (void)react, (void)defs, (void)now;
    }
    // The player answers only in the demo (DGROUP:0x95bc = 0): the FUZZY ONES leave.
    {
        const auto [o, op, orec, by, until, react, defs, now] = arrest(3, 1, 0, 0);
        CHECK(o == 10);
        (void)op, (void)orec, (void)by, (void)until, (void)react, (void)defs, (void)now;
    }
    {
        const auto [o, op, orec, by, until, react, defs, now] = arrest(3, 1, 0, 1);
        CHECK(o == 0);
        (void)op, (void)orec, (void)by, (void)until, (void)react, (void)defs, (void)now;
    }
}

TEST_CASE("seg13:0xa9b3: the target gone; the tail's unwritten [bp-8]; the slot loop's bound") {
    Rig r;
    Pilot E = r.p(3);
    E.order_rec(1);
    E.one_shot(0);
    r.o(1).state = 1;  // ship 1 out of play
    r.o(3).w36 = 14;   // the cuffs hold ship 2
    r.o(3).w38 = 2;
    r.replan();
    CHECK(E.est_pool() == 12);
    CHECK(E.est_rec() == 0);
    CHECK(E.one_shot() == 1);  // [bp-8] read as 0 != 2 [DESIGN ADR 0016 decision 1]
    E.one_shot(0);
    r.o(3).w38 = 0;  // holding ship 0: equal to the 0 read
    r.replan();
    CHECK(E.one_shot() == 0);
    // Dead: the same.
    Rig d;
    Pilot D = d.p(3);
    D.order_rec(1);
    d.s(1).bec = 0;
    d.at_cells(10);
    d.replan();
    CHECK(D.est_pool() == 12);
    CHECK(d.host.msgs.empty());
    // The slots: j = 0 ... sbyte +0xcb inclusive [BYTES seg13:0xabb4-0xabc1].
    Rig s;
    s.p(3).slots(0);  // slot 1 is past the bound
    s.at_cells(10);
    s.replan();
    CHECK(s.host.msgs.size() == 1);
    CHECK(s.host.throws.empty());
    Rig t;
    t.p(3).slots(2);
    set_ship_b(t.s(3), 0xb8 + 1, 0);
    set_ship_b(t.s(3), 0xb8 + 2, 15);  // the byte at +0xcb's index is tested too
    t.at_cells(10);
    t.replan();
    REQUIRE(t.host.throws.size() == 1);
    CHECK(std::get<0>(t.host.throws[0]) == 2);
}

TEST_CASE("seg12:0x23fd-0x24b1: the prisoner's gate passage sets DGROUP:0x93a4") {
    // The k1 escort: the Interceptor 3 holding the player on the cuffs goes through the gate off
    // screen; the player follows, DGROUP:0x1e80 = now + 1000 and DGROUP:0x93a4 = 1 [ORACLE
    // ships-0003: state 8 at DGROUP:0x93a4's write + 1000 ms].
    const auto rig = [](World& w) {
        w.player = 0;
        w.last_ship = 3;
        for (int i = 0; i < 4; ++i) {
            w.ships[static_cast<std::size_t>(i)].design_index = static_cast<std::int16_t>(i);
            w.ships[static_cast<std::size_t>(i)].bec = 1;
            ObjectRecord& o = w.pools.rec(kPoolShips, i);
            o.state = 0;
            o.on_screen = 0;
            o.owner_pool = -1;
            o.w36 = -1;
        }
        w.design_ratings[3].team = 7;
        w.design_ratings[1].team = 3;
    };
    GateExitHost h;
    {
        World w;
        rig(w);
        ShipRecord& I = w.ships[3];
        I.w54 = 14;
        I.w56 = 0;
        w.pools.rec(kPoolShips, 3).w36 = 14;
        w.pools.rec(kPoolShips, 3).w38 = 0;
        GateExitCtx c{w, 500000u, h, nullptr};
        gate_exit(c, kPoolShips, 3);
        CHECK(w.pools.rec(kPoolShips, 3).state == 1);
        CHECK(w.pools.rec(kPoolShips, 0).state == 1);  // the prisoner with it
        CHECK(w.w1e80 == 501000u);
        CHECK(w.w93a4 == 1);
    }
    {
        // A holder of another team takes nothing through.
        World w;
        rig(w);
        w.ships[1].w54 = 14;
        w.ships[1].w56 = 0;
        w.pools.rec(kPoolShips, 1).w36 = 14;
        w.pools.rec(kPoolShips, 1).w38 = 0;
        GateExitCtx c{w, 500000u, h, nullptr};
        gate_exit(c, kPoolShips, 1);
        CHECK(w.pools.rec(kPoolShips, 0).state == 0);
        CHECK(w.w93a4 == 0);
    }
    {
        // The player passing on its own while held by an Enforcer: only the flag (k5).
        World w;
        rig(w);
        w.pools.rec(kPoolShips, 0).owner_pool = 14;
        w.pools.rec(kPoolShips, 0).owner_rec = 3;
        GateExitCtx c{w, 500000u, h, nullptr};
        gate_exit(c, kPoolShips, 0);
        CHECK(w.w93a4 == 1);
        CHECK(w.w1e80 == 0u);
        // Held by a PIRATE, or by a ship index outside the table: no flag.
        World v;
        rig(v);
        v.pools.rec(kPoolShips, 0).owner_pool = 14;
        v.pools.rec(kPoolShips, 0).owner_rec = 1;
        GateExitCtx cv{v, 500000u, h, nullptr};
        gate_exit(cv, kPoolShips, 0);
        CHECK(v.w93a4 == 0);
        World x;
        rig(x);
        x.pools.rec(kPoolShips, 0).owner_pool = 14;
        x.pools.rec(kPoolShips, 0).owner_rec = 9;
        GateExitCtx cx{x, 500000u, h, nullptr};
        gate_exit(cx, kPoolShips, 0);
        CHECK(x.w93a4 == 0);
    }
}
