// The player's 1500 ms tick (src/world/player_tick.h; docs/re/world.md "The 1500 ms tick";
// work/world/m4s8/tick-report.md): the due test and the re-arm, seg13:0x6716's redraw that shows
// the cloaked ship's ghost through seg8:0xb76's 0xfe mask, the shake and the autopilot paths, the
// ambulance's re-entry watch, the +0xeb fix-up, the Action-mode goal words, the in-use clamp, the
// ship loop, Demon's Gate's PlayMode store, seg13:0x2f30 and the order of the calls out.
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/ambulance.h"
#include "world/draw.h"
#include "world/player_tick.h"
#include "world/update.h"

using namespace is::world;

namespace {

constexpr BrushId kMaskBrush = 0x77;
constexpr DcId kSheet = 0x55;

class TickHost : public UpdateHost {
public:
    World* world = nullptr;
    DrawContext* draw = nullptr;
    std::vector<std::string>* log = nullptr;
    int sprites = 0;
    std::int16_t kind = 0;
    World* frame_world(Pools&) override { return world; }
    void player_sprite() override {
        ++sprites;
        if (log) log->push_back("sprite");
        if (draw) draw_player_sprite(*draw);
    }
    std::int16_t design_kind(int) override { return kind; }
};

struct Rig {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    TickHost host;
    is::shell::Rng rng;
    JoyState joy;
    JoyButtons jb;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    EffectTable effects{};
    ImageSlotTable images{};
    DrawGlobals dg;
    RecordingBlitter out;
    DrawContext dc{w.pools, w.ships, effects, images, dg, out,
                   [](std::int16_t x, std::int16_t y) {
                       return Point16{static_cast<std::int16_t>(x >> 3), static_cast<std::int16_t>(y >> 3)};
                   },
                   nullptr};
    std::vector<std::string> calls;

    Rig() {
        player_tick_links() = PlayerTickLinks{};
        player_tick_state() = PlayerTickState{};
        ambulance_state().reentry_due = 0;
        host.world = &w;
        host.draw = &dc;
        host.log = &calls;
        g.now = 50000;
        g.player = 0;
        g.last_ship = 0;
        g.dg_952e = -1;
        clear_effects(effects);
        dg.state = 1;
        dg.dg_3a38 = 1;
        dg.sprite.player = 0;
        dg.sprite.player_mask_brush = kMaskBrush;
        ObjectRecord& o = w.pools.rec(kPoolShips, 0);
        o.on_screen = 1;
        o.scr_x = 496, o.scr_y = 325;
        o.size = 32, o.gap = 4;
        o.frame = 2;
        w.ships[0].bec = 1;
        w.ships[0].has_flame = 1;  // image slot 0x28 + 0
        images[kShipImageSlotBase] = kSheet;
    }
    // The links hold lambdas on this rig: the program's state goes back to empty with it.
    ~Rig() {
        player_tick_links() = PlayerTickLinks{};
        player_tick_state() = PlayerTickState{};
        ambulance_state().reentry_due = 0;
    }
    Rig(const Rig&) = delete;
    Rig& operator=(const Rig&) = delete;
    ShipRecord& player() { return w.ships[0]; }
    void tick() { player_tick(c); }
    // Every slot of player_tick_links() records its name.
    void record_links() {
        PlayerTickLinks& l = player_tick_links();
        auto rec = [this](const char* s) { return [this, s] { calls.push_back(s); }; };
        l.seg18_5cec = rec("seg18:0x5cec");
        l.seg12_515e = rec("seg12:0x515e");
        l.demons_gate_mirror = [this](int) { calls.push_back("mirror"); };
        l.seg4_59bc = [this] { return calls.push_back("seg4:0x59bc"), std::int16_t{0}; };
        l.seg13_7b40 = [this](int) { calls.push_back("seg13:0x7b40"); };
        l.seg13_2714 = [this](int, int, int, int, int, int) { calls.push_back("seg13:0x2714"); };
        l.seg4_6700 = [this](int) { calls.push_back("seg4:0x6700"); };
        l.seg8_9ab8 = rec("seg8:0x9ab8");
        l.seg13_3302 = rec("seg13:0x3302");
        l.seg4_7e2a = rec("seg4:0x7e2a");
        l.seg4_83ae = rec("seg4:0x83ae");
        l.seg8_9b4e = rec("seg8:0x9b4e");
        l.activity_text = [this](int, int, int) { return calls.push_back("seg3:0x1d0a"), std::string{}; };
    }
};

}  // namespace

TEST_CASE("player tick: due once +0x156 is at or below now, re-armed 1500 ms ahead") {
    Rig r;
    r.player().due156.set(50001);
    r.tick();
    CHECK(r.host.sprites == 0);
    CHECK(r.player().due156.get() == 50001u);
    r.player().due156.set(50000);  // equal is due [BYTES seg7:0xdc6 seg7:0xdca]
    r.tick();
    CHECK(r.host.sprites == 1);
    CHECK(r.player().due156.get() == 51500u);
    r.g.now = 51499;
    r.tick();
    CHECK(r.host.sprites == 1);
    r.g.now = 51500;
    r.tick();
    CHECK(r.host.sprites == 2);
    CHECK(r.player().due156.get() == 53000u);
    // Unsigned 32-bit, high word first: 0x10000 lies after 0xffff.
    r.g.now = 0xffffu;
    r.player().due156.set(0x10000u);
    r.tick();
    CHECK(r.host.sprites == 2);
    // The re-arm carries into the high word (`adc`).
    r.g.now = 0xfb00u;
    r.player().due156.set(0);
    r.tick();
    CHECK(r.player().due156.get() == 0x100dcu);
}

TEST_CASE("player tick: a cloaked ship at rest shows the dithered ghost") {
    Rig r;
    // The vanish's end left byte +0x2e = 0xfe and +0xeb = 0 (gate_exit.cpp vanish_end,
    // entry_effect.cpp); the tick falls due 0.22 s later, the phase core-0010 shows.
    ObjectRecord& o = r.w.pools.rec(kPoolShips, 0);
    o.b2e = 0xfe;
    r.player().due156.set(50220);
    r.tick();
    CHECK(r.out.calls.empty());
    r.g.now = 50220;
    r.tick();
    // seg7:0x1208 -> seg8:0xb76's pinned path: the cell SRCCOPY, then DPa with the mask brush.
    REQUIRE(r.out.calls.size() == 4);
    CHECK(r.out.calls[0] == bit_blt_call(kDcWindow, 496, 325, 32, 32, kSheet, 2 * 36 + 4, 0, kSrcCopy));
    CHECK(r.out.calls[1].op == BlitCall::kSelectBrush);
    CHECK(r.out.calls[1].brush == kMaskBrush);
    CHECK(r.out.calls[2] == pat_blt_call(kDcWindow, 496, 325, 32, 32, kDpa));
    CHECK(r.out.calls[3].op == BlitCall::kSelectBrush);
    CHECK(r.out.calls[3].brush == kBrushWhite);
    // The ghost stays: every later tick draws it again, 1500 ms apart.
    r.out.calls.clear();
    r.g.now = 51719;
    r.tick();
    CHECK(r.out.calls.empty());
    r.g.now = 51720;
    r.tick();
    CHECK(r.out.calls.size() == 4);
    // A ship hidden with 0xff (the gate exit, the vanish of mode 1 elsewhere) draws nothing.
    r.out.calls.clear();
    o.b2e = 0xff;
    r.g.now = 60000;
    r.tick();
    CHECK(r.host.sprites == 3);
    CHECK(r.out.calls.empty());
}

TEST_CASE("player tick: a shaking screen leaves the tick due on the next frame") {
    Rig r;
    r.g.dg_238c = 1;
    r.tick();
    CHECK(r.host.sprites == 0);
    CHECK(r.player().due156.get() == 49999u);  // seg13:0x6716: now - 1 [BYTES seg13:0x673d-0x6753]
}

TEST_CASE("player tick: the autopilot runs seg13:0x2f30 alone on every frame") {
    Rig r;
    r.record_links();
    r.g.autopilot = 1;
    r.player().due156.set(40000);
    r.tick();
    CHECK(r.calls == std::vector<std::string>{"seg3:0x1d0a"});
    CHECK(r.player().due156.get() == 40000u);  // not re-armed [BYTES seg7:0xdd3]
    r.tick();
    CHECK(r.calls.size() == 2);
    // Not due: nothing at all.
    r.player().due156.set(60000);
    r.tick();
    CHECK(r.calls.size() == 2);
}

TEST_CASE("player tick: seg13:0x2f30, the goal word and the command line text") {
    Rig r;
    std::vector<int> args;
    std::string text = "Capturing ROCK";
    player_tick_links().activity_text = [&](int a, int ship, int mode) {
        args = {a, ship, mode};
        return text;
    };
    r.w.w8630 = "old";
    ship_goal_text(r.c, 0);
    CHECK(args == std::vector<int>{100, 0, 0});
    CHECK(r.w.w8630 == "Capturing ROCK");
    CHECK(r.w.w058c == 1);  // lstrcmp != 0
    r.w.w058c = 0;
    ship_goal_text(r.c, 0);
    CHECK(r.w.w058c == 0);  // the same text
    r.g.play_mode = 1;      // Strategy mode: mode 1
    ship_goal_text(r.c, 0);
    CHECK(args == std::vector<int>{100, 0, 1});
    // A negative word +0x06 becomes 0 and ends the call [BYTES seg13:0x2f53-0x2f60].
    args.clear();
    r.player().w06 = -2;
    ship_goal_text(r.c, 0);
    CHECK(r.player().w06 == 0);
    CHECK(args.empty());
    // Another ship: nothing [BYTES seg13:0x2f4a].
    r.w.ships[1].w06 = -2;
    ship_goal_text(r.c, 1);
    CHECK(r.w.ships[1].w06 == -2);
    CHECK(args.empty());
}

TEST_CASE("player tick: the ambulance's re-entry watch") {
    Rig r;
    auto run = [&r](std::int16_t served, std::int16_t state, std::uint32_t visit_due, std::int16_t a_state) {
        r.f.dg_6da8 = 0;
        ambulance_state().reentry_due = 0;
        r.g.dg_952e = served;
        r.g.dg_9e49 = state;
        r.w.w1e88 = visit_due;
        r.w.pools.rec(kPool7, 0).state = a_state;
        r.player().due156.set(0);
        r.tick();
        CHECK(r.f.dg_6da8 == ambulance_state().reentry_due);
        return r.f.dg_6da8;
    };
    CHECK(run(-1, 6, 0, 1) == 0u);  // no served ship
    CHECK(run(0, 6, 5, 0) == 1u);   // states 6 ... 8 whatever the deadline and A
    CHECK(run(0, 8, 0, 0) == 1u);
    CHECK(run(0, 9, 0, 1) == 0u);
    CHECK(run(0, -1, 0, 1) == 0u);
    CHECK(run(0, 3, 0, 1) == 1u);   // A out of play, no visit pending
    CHECK(run(0, 3, 0, 0) == 0u);
    CHECK(run(0, 0, 7, 1) == 0u);
    // A deadline already set is left alone.
    r.f.dg_6da8 = 1234;
    r.g.dg_9e49 = 6;
    r.player().due156.set(0);
    r.tick();
    CHECK(r.f.dg_6da8 == 1234u);
}

TEST_CASE("player tick: a stuck +0xeb is cleared once the object's +0x2e is 0") {
    Rig r;
    r.g.last_ship = 2;
    r.w.ships[1].beb = 1;
    r.w.ships[2].beb = 1;
    r.w.pools.rec(kPoolShips, 2).b2e = 1;  // still vanishing
    r.tick();
    CHECK(r.w.ships[1].beb == 0);
    CHECK(r.w.ships[2].beb == 1);
}

TEST_CASE("player tick: the Action-mode goal words") {
    Rig r;
    r.record_links();
    ShipRecord& P = r.player();
    P.w06 = 9, P.w08 = 3, P.w36 = 1, P.w3a = 5;
    r.g.game_frames = 777;
    r.tick();
    CHECK(P.w32 == 9);
    CHECK(P.w34 == 3);
    CHECK(P.w38 == 1);
    CHECK(P.w3c == 5);
    CHECK(player_tick_state().dg_8714 == 777u);  // pool 9 with +0x38 = 1
    CHECK(r.calls.size() >= 2);
    CHECK(r.calls[1] == "seg4:0x59bc");  // after the sprite
    // Strategy mode keeps the goal words and skips seg4:0x59bc.
    r.calls.clear();
    r.g.play_mode = 1;
    P.w06 = 4;
    P.due156.set(0);
    r.tick();
    CHECK(P.w32 == 9);
    for (const std::string& s : r.calls) CHECK(s != "seg4:0x59bc");
    // A goal of pool 4 with +0x38 = 2 calls seg4:0x6700 [BYTES seg7:0x1048-0x1079].
    r.calls.clear();
    r.g.play_mode = 0;
    P.w36 = 2;
    P.due156.set(0);
    r.tick();
    CHECK(std::find(r.calls.begin(), r.calls.end(), "seg4:0x6700") != r.calls.end());
}

TEST_CASE("player tick: the goal ship's own goal, seg13:0x7b40 and the DGROUP:0x596 pacing") {
    Rig r;
    r.g.last_ship = 2;
    std::vector<std::vector<int>> att;
    std::vector<int> report;
    player_tick_links().seg13_2714 = [&](int a, int b, int c, int d, int e, int f) {
        att.push_back({a, b, c, d, e, f});
    };
    player_tick_links().seg13_7b40 = [&](int s) { report.push_back(s); };
    ShipRecord& P = r.player();
    ShipRecord& Q = r.w.ships[1];
    P.w06 = 0xe, P.w08 = 1, P.w36 = 1;  // the goal: ship 1, +0x38 = 1
    Q.w32 = 0xe, Q.w34 = 2, Q.w38 = 1;  // ship 1 goes for ship 2
    set_byte_at(Q, 0xfd + 9 * 0, 3);   // ship 1's byte +0xfd + 9 * player
    auto tick = [&r] {
        r.player().due156.set(0);
        r.tick();
    };
    tick();
    CHECK(player_tick_state().dg_596 == 1);
    CHECK(att.empty());
    tick();
    REQUIRE(att.size() == 1);
    CHECK(att[0] == std::vector<int>{2, 0, 14, 35, 0, 7});  // a = 10 - 3
    CHECK(player_tick_state().dg_596 == -4);
    for (int k = 0; k < 5; ++k) tick();  // -3 ... 1
    CHECK(att.size() == 1);
    set_byte_at(Q, 0xfd, 40);  // above 10: b = 10, a = 0
    tick();
    REQUIRE(att.size() == 2);
    CHECK(att[1] == std::vector<int>{2, 0, 0, 0, 0, 7});
    // Ship 1 going for the player instead: DGROUP:0x596 back to 0 [BYTES seg7:0x102e-0x1042].
    Q.w34 = 0;
    tick();
    CHECK(player_tick_state().dg_596 == 0);
    // +0x52 = 5 with pool-14 record +0x56 in play: seg13:0x7b40(+0x56) when ship 1 goes for it,
    // and DGROUP:0x596 is left alone.
    player_tick_state().dg_596 = 9;
    P.w52 = 5, P.w56 = 2;
    Q.w34 = 2;
    tick();
    CHECK(report == std::vector<int>{2});
    CHECK(player_tick_state().dg_596 == 9);
    CHECK(att.size() == 2);
}

TEST_CASE("player tick: a goal ship outside the table reads a zeroed record and logs once") {
    Rig r;
    std::vector<std::string> todo;
    struct TodoScope {
        explicit TodoScope(std::vector<std::string>& v) {
            set_guard_todo([&v](const std::string& what, const char* address) { v.push_back(what + " " + address); });
        }
        ~TodoScope() { set_guard_todo(nullptr); }
    } scope(todo);
    player_tick_state().dg_596 = 5;
    ShipRecord& P = r.player();
    P.w06 = 0xe, P.w08 = 99, P.w36 = 1;
    r.tick();
    // q's +0x38 reads 0: no call, and the goal ship with +0x38 = 1 resets DGROUP:0x596.
    CHECK(player_tick_state().dg_596 == 0);
    bool logged = false;
    for (const std::string& s : todo) logged = logged || s.find("seg7:0xfcc") != std::string::npos;
    CHECK(logged);
}

TEST_CASE("player tick: the in-use clamp of pools 0 ... DGROUP:0x9324") {
    Rig r;
    Pools& p = r.w.pools;
    p.set_in_use(5, static_cast<std::int16_t>(p.allocated(5) + 2));
    p.set_in_use(3, -1);
    p.set_in_use(2, 1);
    r.tick();
    CHECK(p.in_use(5) == p.allocated(5));
    CHECK(p.in_use(3) == p.allocated(3));
    CHECK(p.in_use(2) == 1);
}

TEST_CASE("player tick: the other ships' relation byte and the team-7 respawn") {
    Rig r;
    r.g.last_ship = 1;
    r.g.game_frames = 1000;  // frame# / 10 = 100
    ShipRecord& S = r.w.ships[1];
    set_byte_at(S, 0xc2 + 0, 5);
    set_byte_at(S, 0x13e + 0, 9);
    set_byte_at(r.player(), 0x13e + 1, 9);
    S.w44 = 50;  // 100 - 50 = 50 <= 60: kept
    r.tick();
    CHECK(sbyte_at(S, 0xc2) == 5);
    S.w44 = 39;  // 61: falls by 1, both bytes +0x13e cleared
    r.player().due156.set(0);
    r.tick();
    CHECK(sbyte_at(S, 0xc2) == 4);
    CHECK(sbyte_at(S, 0x13e) == 0);
    CHECK(sbyte_at(r.player(), 0x13e + 1) == 0);
    S.w44 = 200;  // negative difference, unsigned: falls too
    r.player().due156.set(0);
    r.tick();
    CHECK(sbyte_at(S, 0xc2) == 3);
    set_byte_at(S, 0xc2, 20);  // 20 is outside 1 ... 19
    r.player().due156.set(0);
    r.tick();
    CHECK(sbyte_at(S, 0xc2) == 20);
    // Team 7, destroyed, byte +0x5e = 1: off-world again and alive.
    ObjectRecord& o = r.w.pools.rec(kPoolShips, 1);
    o.state = 1;
    S.w5e = 0x0201;
    S.bec = 0;
    r.host.kind = 6;
    r.player().due156.set(0);
    r.tick();
    CHECK(o.state == 1);
    r.host.kind = 7;
    r.player().due156.set(0);
    r.tick();
    CHECK(o.state == 100);
    CHECK(S.bec == 1);
}

TEST_CASE("player tick: Demon's Gate sets Action mode and calls the mirror") {
    Rig r;
    r.record_links();
    r.g.demons_gate = 1;
    r.g.demo = 1;
    r.g.play_mode = 1;
    r.w.w94b2 = 1;
    r.g.game_frames = 100;
    r.tick();
    CHECK(r.g.play_mode == 0);
    CHECK(std::find(r.calls.begin(), r.calls.end(), "mirror") == r.calls.end());
    r.g.game_frames = 101;
    r.player().due156.set(0);
    r.tick();
    CHECK(std::find(r.calls.begin(), r.calls.end(), "mirror") != r.calls.end());
}

TEST_CASE("player tick: the calls out in address order") {
    Rig r;
    r.record_links();
    r.w.w913e = 1;
    r.g.wave_kind = 7;
    r.tick();
    const std::vector<std::string> want = {"sprite",       "seg18:0x5cec", "seg12:0x515e", "seg4:0x59bc",
                                           "seg8:0x9ab8",  "seg13:0x3302", "seg4:0x7e2a",  "seg4:0x83ae",
                                           "seg8:0x9b4e",  "seg3:0x1d0a"};
    CHECK(r.calls == want);
}
