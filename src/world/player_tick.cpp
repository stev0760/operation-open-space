// The player's 1500 ms tick seg7:0xdaf-0x117d (src/world/player_tick.h). The readings are
// work/world/m4s8/tick-report.md's "Readings no verifier has checked" [BYTES seg7:0xdaf-0x117d
// seg13:0x2f30-0x3005]. 16-bit arithmetic as the original's (ADR 0013 decision 7).
#include "world/player_tick.h"

#include "world/ambulance.h"
#include "world/flight.h"
#include "world/update.h"
#include "world/world.h"

namespace is::world {

namespace {

PlayerTickLinks g_links;
PlayerTickState g_state;

// The one-time logs: one flag per site.
struct Once {
    bool player = false, ship = false, object = false, pool = false;
    bool seg18_5cec = false, seg12_515e = false, mirror = false, seg4_59bc = false;
    bool seg13_7b40 = false, seg13_2714 = false, seg4_6700 = false, seg8_9ab8 = false;
    bool seg13_3302 = false, seg4_7e2a = false, seg4_83ae = false, seg8_9b4e = false;
    bool text = false;
};
Once g_once;

bool ship_ok(int i) { return i >= 0 && i < kShipCount; }

// A ship record through an index from a record word: outside the table it reads a zeroed scratch
// record [DESIGN ADR 0016 decision 2].
const ShipRecord& ship_at_or_zero(UpdateCtx& c, int i, const char* address) {
    if (ship_ok(i)) return c.flight.ships[static_cast<std::size_t>(i)];
    static ShipRecord zero;
    zero = ShipRecord{};
    guard_todo(g_once.ship, "a ship index outside the ship table", address);
    return zero;
}

// Record i of pool 14 through the pool pointer DGROUP:0x1e30; outside the pool's storage a zeroed
// scratch record [DESIGN ADR 0016 decision 2].
const ObjectRecord& pool14_or_zero(UpdateCtx& c, int i, const char* address) {
    Pools& p = c.flight.pools;
    if (i >= 0 && i < p.records(kPoolShips)) return p.rec(kPoolShips, i);
    static ObjectRecord zero;
    zero = ObjectRecord{};
    guard_todo(g_once.object, "a pool-14 record outside the pool", address);
    return zero;
}

// The ambulance's re-entry watch [BYTES seg7:0xdfb-0xe32]: with dword DGROUP:0x6da8 = 0, a served
// ship (DGROUP:0x952e >= 0) and DGROUP:0x9e49 in 0 ... 8 (signed), DGROUP:0x6da8 = 1 when the
// state is 6 ... 8, or when the visit deadline dword DGROUP:0x1e88 is 0 while the ambulance (pool 7
// record 0, far DGROUP:0x1e14) has `state` != 0; the frame's step 20 then runs seg16:0x33a2 and
// seg16:0x3306 (update.cpp) [BYTES seg7:0xc7c-0xc8f].
void reentry_watch(UpdateCtx& c, const World* w) {
    WaveGlobals& g = c.flight.g;
    if (c.f.dg_6da8 != 0) return;  // [BYTES seg7:0xdfb seg7:0xe01]
    if (g.dg_952e < 0) return;     // [BYTES seg7:0xe03 seg7:0xe08]
    if (g.dg_9e49 < 0 || g.dg_9e49 >= 9) return;  // [BYTES seg7:0xe0a-0xe16]
    if (g.dg_9e49 < 6) {  // [BYTES seg7:0xe18 seg7:0xe1d]
        // Without a frame World (a test) DGROUP:0x1e88 reads 0 [DESIGN].
        const std::uint32_t visit_due = w ? w->w1e88 : 0u;
        if (visit_due != 0) return;  // [BYTES seg7:0xe1f seg7:0xe25]
        Pools& p = c.flight.pools;
        const std::int16_t a_state = p.records(kPool7) > 0 ? p.rec(kPool7, 0).state : std::int16_t{0};
        if (a_state == 0) return;  // [BYTES seg7:0xe27-0xe30]
    }
    // [BYTES seg7:0xe32]. The frame's FrameGlobals::dg_6da8 and AmbulanceState::reentry_due stand
    // for the one word; the next frame's sync copies the store in, so both are written (ambulance.h)
    // [DESIGN].
    c.f.dg_6da8 = 1;
    ambulance_state().reentry_due = 1;
}

// The Action-mode block seg7:0xf07-0x1079 (PlayMode = 0).
void action_goal(UpdateCtx& c, int p) {
    WaveGlobals& g = c.flight.g;
    ShipRecord& P = c.flight.ships[static_cast<std::size_t>(p)];
    // seg4:0x59bc, the player's goal estimate; its AX goes to [bp-8], which nothing reads
    // [BYTES seg7:0xf07 seg7:0xf0c].
    if (g_links.seg4_59bc)
        (void)g_links.seg4_59bc();
    else
        guard_todo(g_once.seg4_59bc, "the player's goal estimate (M5)", "seg4:0x59bc");
    // The goal words from the estimate's words [BYTES seg7:0xf0f-0xf31].
    P.w32 = P.w06;
    P.w34 = P.w08;
    P.w38 = P.w36;
    P.w3c = P.w3a;
    // A pool-9 goal with +0x38 = 1 stamps dword DGROUP:0x8714 = frame# [BYTES seg7:0xf35-0xf4b].
    if (P.w32 == 9 && P.w38 == 1) g_state.dg_8714 = g.game_frames;
    // [BYTES seg7:0xf4f-0xf84]: +0x38 = 1, word +0x52 = 1 or 5, and pool-14 record +0x56 in play.
    const bool hunt = P.w38 == 1 && (P.w52 == 1 || P.w52 == 5) &&
                      pool14_or_zero(c, P.w56, "seg7:0xf7f").state == 0;
    if (hunt) {
        // A ship goal q whose own goal is ship +0x56 with its +0x38 = 1: seg13:0x7b40(+0x56)
        // [BYTES seg7:0xf88-0xfb4].
        if (P.w32 == 0xe) {
            const ShipRecord& Q = ship_at_or_zero(c, P.w34, "seg7:0xf91");
            if (Q.w38 == 1 && Q.w32 == 0xe && Q.w34 == P.w56) {
                if (g_links.seg13_7b40)
                    g_links.seg13_7b40(P.w56);
                else
                    guard_todo(g_once.seg13_7b40, "the goal ship's report (M5)", "seg13:0x7b40");
            }
        }
    } else {
        // [BYTES seg7:0xfb8-0xfe7]: the goal is ship q (+0x38 = 1) and q's goal is a ship other
        // than the player (q's +0x38 = 1).
        bool chase = P.w32 == 0xe && P.w38 == 1;
        const ShipRecord* Q = nullptr;
        if (chase) {
            Q = &ship_at_or_zero(c, P.w34, "seg7:0xfcc");
            chase = Q->w38 == 1 && Q->w32 == 0xe && Q->w34 != p;
        }
        if (chase) {
            // DGROUP:0x596 + 1, and above 1 the call with b = min(10, q's signed byte +0xfd + 9p)
            // [BYTES seg7:0xfe9-0x1026]: seg13:0x2714(q's +0x34, player, 2(10 - b), 5(10 - b),
            // 0, 7), then DGROUP:0x596 = -4.
            g_state.dg_596 = add16(g_state.dg_596, 1);
            if (g_state.dg_596 > 1) {
                std::int8_t b = sbyte_at(*Q, 0xfd + 9 * p);  // [BYTES seg7:0xfff-0x1009]
                if (b > 10) b = 10;
                const std::int16_t a = sub16(10, b);  // `sub ax, 0xa; neg ax` [BYTES seg7:0x100a-0x100d]
                if (g_links.seg13_2714)
                    g_links.seg13_2714(Q->w34, p, add16(a, a), mul16(a, 5), 0, 7);  // [BYTES seg7:0x100f-0x101e]
                else
                    guard_todo(g_once.seg13_2714, "the attitude change (M5)", "seg13:0x2714");
                g_state.dg_596 = -4;  // [BYTES seg7:0x1026]
            }
        } else if (P.w32 != 0xe || P.w38 == 1) {
            g_state.dg_596 = 0;  // [BYTES seg7:0x102e-0x1042]
        }
    }
    // A goal of pool 0, 4 or 11 with +0x38 = 2: seg4:0x6700(player) [BYTES seg7:0x1048-0x1079].
    if ((P.w32 == 0 || P.w32 == 4 || P.w32 == 0xb) && P.w38 == 2) {
        if (g_links.seg4_6700)
            g_links.seg4_6700(p);
        else
            guard_todo(g_once.seg4_6700, "the player's goal object (M5)", "seg4:0x6700");
    }
}

// The ship loop seg7:0x10b9-0x1166 over i = 0 ... DGROUP:0x9316 (signed `jle`).
void ship_loop(UpdateCtx& c, int p) {
    WaveGlobals& g = c.flight.g;
    Pools& pools = c.flight.pools;
    ShipRecord& P = c.flight.ships[static_cast<std::size_t>(p)];
    for (int i = 0; i <= g.last_ship; ++i) {
        if (!ship_ok(i)) {  // [DESIGN ADR 0016 decision 2] a ship count past the table
            guard_todo(g_once.ship, "a ship index outside the ship table", "seg7:0x10c9");
            break;
        }
        ShipRecord& S = c.flight.ships[static_cast<std::size_t>(i)];
        if (i != p) {  // [BYTES seg7:0x10c1-0x10c7]
            // Ship i's signed byte +0xc2 + player in 1 ... 19 falls by 1 once frame# / 10 (seg5:0x2b4,
            // unsigned 32-bit) minus ship i's sign-extended word +0x44 is not 0 ... 60 (32-bit,
            // `or bx, bx; jne` then `cmp cx, 0x3c; jbe`), and both ships' bytes +0x13e + the other
            // go to 0 [BYTES seg7:0x10d2-0x111b].
            const std::int8_t r = sbyte_at(S, 0xc2 + p);
            if (r > 0 && r < 0x14) {
                const std::uint32_t t = g.game_frames / 10u;
                const std::uint32_t d = t - static_cast<std::uint32_t>(static_cast<std::int32_t>(S.w44));
                if (d > 60u) {
                    set_byte_at(S, 0xc2 + p, r - 1);  // [BYTES seg7:0x1108]
                    set_byte_at(S, 0x13e + p, 0);     // [BYTES seg7:0x110e]
                    set_byte_at(P, 0x13e + i, 0);     // [BYTES seg7:0x111b]
                }
            }
        }
        // A team-7 ship (design word DGROUP:0x970a = 7) destroyed (`state` 1) with byte +0x5e = 1
        // goes back off-world, `state` 100, with `alive` +0xec = 1; the record is pool DGROUP:0x9324's
        // through the pool table DGROUP:0x1df8 [BYTES seg7:0x111f-0x1158].
        if (i < pools.records(g.ship_pool)) {
            ObjectRecord& o = pools.rec(g.ship_pool, i);
            if (o.state == 1 && c.flight.host.design_kind(S.design_index) == 7 && (S.w5e & 0xff) == 1) {
                o.state = 100;  // [BYTES seg7:0x1152]
                S.bec = 1;      // [BYTES seg7:0x1158]
            }
        } else {
            guard_todo(g_once.object, "a ship-pool record outside the pool", "seg7:0x112e");
        }
    }
}

}  // namespace

PlayerTickState& player_tick_state() { return g_state; }
PlayerTickLinks& player_tick_links() { return g_links; }

void player_tick(UpdateCtx& c) {
    WaveGlobals& g = c.flight.g;
    Pools& pools = c.flight.pools;
    const int p = g.player;
    if (!ship_ok(p)) {  // [DESIGN ADR 0016 decision 2]
        guard_todo(g_once.player, "the player index outside the ship table", "seg7:0xdb6");
        return;
    }
    ShipRecord& P = c.flight.ships[static_cast<std::size_t>(p)];
    // Due once dword +0x156 is at or below now: `ja` past the tick on the high word, then on the
    // low word, unsigned [BYTES seg7:0xdaf-0xdca].
    if (P.due156.get() > g.now) return;
    // DGROUP:0x951a != 0 jumps to seg7:0x1179: seg13:0x2f30 alone, +0x156 not re-armed, so it
    // runs on every frame of the autopilot [BYTES seg7:0xdce seg7:0xdd3 seg7:0x1179 seg7:0x117d].
    if (g.autopilot != 0) {
        ship_goal_text(c, p);
        return;
    }
    // +0x156 = now + 1500, `add 0x5dc; adc 0` [BYTES seg7:0xdd7-0xde1], then seg13:0x6716(player)
    // (flight.h ship_touched): seg7:0x1208 without a shake, else +0x156 = now - 1, so a shaking
    // screen makes the tick due again on the next frame [BYTES seg7:0xde5-0xdee seg13:0x6720-0x6753].
    // The cloaked player's ghost is that seg7:0x1208: object byte +0x2e = 0xfe takes seg8:0xb76's
    // mask (sprite.cpp) [BYTES seg8:0x11ea-0x1231] [ORACLE core-0010].
    P.due156.set(g.now + 1500u);
    ship_touched(c.flight, p);
    World* w = c.host.frame_world(pools);
    // DGROUP:0x913e != 0 (the wave start's chance block, or a kind-2 pod): seg18:0x5cec
    // [BYTES seg7:0xdef-0xdf6]. Without a frame World it reads 0 [DESIGN].
    if (w && w->w913e != 0) {
        if (g_links.seg18_5cec)
            g_links.seg18_5cec();
        else
            guard_todo(g_once.seg18_5cec, "the wave's chance event", "seg18:0x5cec");
    }
    reentry_watch(c, w);
    // A ship whose byte +0xeb is still set while its object byte +0x2e is 0 (pool 14 through
    // DGROUP:0x1e30, i = 0 ... DGROUP:0x9316) gets +0xeb = 0 [BYTES seg7:0xe3b-0xe6f]. For the
    // player the frame's branch keeps the tick off while +0xeb is set (update.cpp), so only other
    // ships meet it.
    for (int i = 0; i <= g.last_ship; ++i) {
        if (!ship_ok(i)) {  // [DESIGN ADR 0016 decision 2]
            guard_todo(g_once.ship, "a ship index outside the ship table", "seg7:0xe42");
            break;
        }
        ShipRecord& S = c.flight.ships[static_cast<std::size_t>(i)];
        if (S.beb != 0 && pool14_or_zero(c, i, "seg7:0xe58").b2e == 0) S.beb = 0;  // [BYTES seg7:0xe61]
    }
    // A kind-7 wave: seg12:0x515e [BYTES seg7:0xe71-0xe78].
    if (g.wave_kind == 7) {
        if (g_links.seg12_515e)
            g_links.seg12_515e();
        else
            guard_todo(g_once.seg12_515e, "the kind-7 respawn (M6)", "seg12:0x515e");
    }
    // Demon's Gate (DGROUP:0x92e8) in a player game (DGROUP:0x95bc != 0): PlayMode = 0; with
    // DGROUP:0x94b2 != 0, the player alone (DGROUP:0x9316 = 0) and frame# > 100 (unsigned), the
    // mirror [BYTES seg7:0xe7d-0xefb]. Without a frame World DGROUP:0x94b2 reads 0 [DESIGN].
    if (g.demons_gate != 0 && g.demo != 0) {
        g.play_mode = 0;  // [BYTES seg7:0xe8b]
        if (w && w->w94b2 != 0 && g.last_ship == 0 && g.game_frames > 100u) {
            if (g_links.demons_gate_mirror)
                g_links.demons_gate_mirror(p);
            else
                guard_todo(g_once.mirror, "Demon's Gate's mirror (M6)", "seg7:0xea7");
        }
    }
    if (g.play_mode == 0) action_goal(c, p);  // [BYTES seg7:0xefe seg7:0xf03]
    if (g_links.seg8_9ab8)  // [BYTES seg7:0x107a]
        g_links.seg8_9ab8();
    else
        guard_todo(g_once.seg8_9ab8, "pool 2 at its gate (M5)", "seg8:0x9ab8");
    if (g_links.seg13_3302)  // [BYTES seg7:0x107f]
        g_links.seg13_3302();
    else
        guard_todo(g_once.seg13_3302, "the goal reports, message 0xb8 (M5)", "seg13:0x3302");
    // The in-use clamp of pools 0 ... DGROUP:0x9324 (records.h clamp_in_use) [BYTES seg7:0x1084-0x10b7].
    if (g.ship_pool >= 0 && g.ship_pool < kPoolCount) {
        pools.clamp_in_use(g.ship_pool);
    } else {  // [DESIGN ADR 0016 decision 2]
        guard_todo(g_once.pool, "a ship pool outside the pool table", "seg7:0x10b1");
        return;
    }
    ship_loop(c, p);
    if (g_links.seg4_7e2a)  // [BYTES seg7:0x116a]
        g_links.seg4_7e2a();
    else
        guard_todo(g_once.seg4_7e2a, "the computer ships' tick (M5)", "seg4:0x7e2a");
    if (g_links.seg4_83ae)  // [BYTES seg7:0x116f]
        g_links.seg4_83ae();
    else
        guard_todo(g_once.seg4_83ae, "the hunters' fix on a cloaked ship (M5)", "seg4:0x83ae");
    if (g_links.seg8_9b4e)  // [BYTES seg7:0x1174]
        g_links.seg8_9b4e();
    else
        guard_todo(g_once.seg8_9b4e, "pool 2's goals (M5)", "seg8:0x9b4e");
    ship_goal_text(c, p);  // [BYTES seg7:0x1179 seg7:0x117d]
}

void ship_goal_text(UpdateCtx& c, int ship) {
    WaveGlobals& g = c.flight.g;
    if (ship != g.player) return;  // [BYTES seg13:0x2f44-0x2f4a]
    if (!ship_ok(ship)) {          // [DESIGN ADR 0016 decision 2]
        guard_todo(g_once.player, "the player index outside the ship table", "seg13:0x2f4e");
        return;
    }
    ShipRecord& S = c.flight.ships[static_cast<std::size_t>(ship)];
    if (S.w06 < 0) {  // signed [BYTES seg13:0x2f53-0x2f60]
        S.w06 = 0;
        return;
    }
    // Object +0x1a != 0 or the ship is the player: always, here [BYTES seg13:0x2f81-0x2f8e]. Mode 1
    // for another ship or in Strategy mode, 0 for the player in Action mode [BYTES seg13:0x2f90-0x2fa7].
    const int mode = g.play_mode != 0 ? 1 : 0;
    World* w = c.host.frame_world(c.flight.pools);
    if (!g_links.activity_text || !w) {
        guard_todo(g_once.text, "the ship activity text, the Strategy-mode command line (M8)", "seg3:0x1d0a");
        return;
    }
    // The text before the call (lstrcpy to [bp-0xaa]), seg3:0x1d0a(100, ship, mode) into
    // DGROUP:0x8630, and DGROUP:0x58c = 1 when lstrcmp finds them different [BYTES seg13:0x2f63-0x2fd1].
    // The trace seg13:0x4eca (OutputDebugString while INI `Aux` is set) is not ported [DESIGN].
    const std::string before = w->w8630;
    w->w8630 = g_links.activity_text(100, ship, mode);
    if (w->w8630 != before) w->w058c = 1;
}

}  // namespace is::world
