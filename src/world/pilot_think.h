// What a computer pilot decides (docs/re/pilots.md "The pilot"; work/analysis/m5/s3-tick.md
// sections 2.2-2.11 as corrected by s3-tick-verify.md; work/world/m5s3/think-report.md "Readings
// no verifier has checked" for the routines part A left unread): the think seg4:0x78, the goal
// choice seg4:0x2f98, the greed level seg4:0x84de, the needs seg4:0x4d00, the hazard step
// seg4:0x4fc8 with the obstacle search seg13:0x1076, the follow-up seg4:0x485c, the weapon choice
// seg4:0x6e70 with its scorer seg4:0x6a78, and the player's activity estimate seg4:0x59bc with
// its scorer seg4:0x8626 (M5 stage 3, track THINK).
//
// Every product, sum, shift and compare is made in the original's width (ADR 0013 decision 7):
// 16-bit words through records.h's helpers, the 32-bit costs of the goal choice as int32 with
// the RTL's long multiply (seg5:0x314, low 32 bits), long division (seg5:0x110, truncating) and
// shifts (seg5:0x346 sar, seg5:0x8c0 shl). rand() is drawn from the frame's shell::Rng in the
// original's order (ruling R1).
//
// Calls into routines other tracks or later stages own go through PilotHost; every default logs
// once through world::guard_todo, names the address and returns the neutral value (ruling R3).
//
// Portable, no Win32.
#pragma once

#include <cstdint>
#include <string>

#include "shell/rng.h"
#include "world/flight.h"
#include "world/pilot.h"
#include "world/records.h"

namespace is::world {

class World;

// --- Calls out of the pilot -------------------------------------------------------------------
class PilotHost {
public:
    virtual ~PilotHost() = default;
    // The design record word DGROUP:0x96d2 + 0xad·n + off: +0x38 the team (DGROUP:0x970a), +0x1e
    // the tank (DGROUP:0x96f0), +0x42 "captures icons" (DGROUP:0x9714).
    virtual std::int16_t design_word(int n, int off);
    // seg13:0x2688(a, b), the attitude read (relations.cpp) [BYTES seg13:0x2688-0x2713].
    virtual std::int16_t relation(int a, int b);
    // seg4:0x5d9c(a, b), the strength ratio (relations.cpp).
    virtual std::int16_t strength(int a, int b);
    // T(x, y): the team table word DGROUP:0x3824 + 2·(8x + y).
    virtual std::int16_t team_word(int x, int y);
    // seg3:0x6ec(recipient, pool, rec, code), cdecl: the message display.
    virtual void message(int recipient, int pool, int rec, int code);
    // seg3:0xf4(ship, pool, rec), cdecl: the target words +0x3e/+0x40.
    virtual void target(int ship, int pool, int rec);
    // seg3:0x0(pool, rec), cdecl: the object's visibility test.
    virtual std::int16_t visible(int pool, int rec);
    // seg4:0x5654(i), Pascal: the weapon system decision; 1 when it fired.
    virtual std::int16_t weapon_system(int i);
    // seg4:0x584c(k, s, life), Pascal: the weapon use.
    virtual void weapon_use(int k, int s, int life);
    // seg11:0x5ab6(i, mode), Pascal: the computer ship's thrust step.
    virtual void thrust(int i, int mode);
    // seg11:0x4e0a(i), Pascal: the gun.
    virtual void gun(int i);
    // seg13:0xadbe(s, kind, msg), cdecl: a call-in; returns AX (0 = refused, pitfalls.md).
    virtual std::int16_t call_in(int s, int kind, int msg);
    // seg12:0x2288(pool, rec), cdecl: the object leaves through the gate.
    virtual void through_gate(int pool, int rec);
    // seg16:0xb4e(i), cdecl: the ambulance call; 0 when refused.
    virtual std::int16_t ambulance_call(int i);
    // seg13:0x6716(i): ship_touched (flight.h).
    virtual void touched(int i);
    // seg11:0x2f8c(): the player's weapon display after a choice.
    virtual void weapon_display();
    // DGROUP:0xa62 = 1: the target display's MANUAL flag (src/hud/info.h) [BYTES seg4:0x62b].
    virtual void set_manual_target();
    // The pod table word DGROUP:0x8560 + 0x34·rec + off (effects.h EffectState::pods).
    virtual std::int16_t pod_word(int rec, int off);
    // The player's hit tallies: words DGROUP:0x9680, 0x9682 and dwords DGROUP:0x8e00, 0x8e04 = 0
    // (the contact track's seg4:0x7d36 words) [BYTES seg4:0x41f9-0x4213].
    virtual void clear_tallies();
    // The weapon table word DGROUP:0x2f54 + 0x50·w + off (missile.h MissileState::words).
    virtual std::int16_t weapon_word(int w, int off);
    // The pool-2 defender table word DGROUP:0x52fe + 0x24·k + off (stage 4).
    virtual std::int16_t defender_word(int k, int off);
    // The visibility list (visibility.h): the count DGROUP:0x9146 and entry k, the bytes
    // DGROUP:0x507c + k (pool) and DGROUP:0x50e0 + k (record), each read through `cbw`.
    virtual int visible_count();
    virtual void visible_entry(int k, std::int8_t& pool, std::int8_t& rec);
    // The words DGROUP:0x6e54 (pool) and DGROUP:0x6e52 (record) seg4:0x8626 compares with
    // [BYTES seg4:0x8b55-0x8b63] [OPEN: their meaning].
    virtual void mark_6e52(std::int16_t& pool, std::int16_t& rec);

    // The tick calls' host side (pilot_tick.h).
    // seg4:0x6906-0x69af: the player taken as a rival: DGROUP:0x8630 = string 1413 + the name of
    // (pool, rec) (seg3:0x1542) + string 1414 + the name of ship s + DGROUP:0x13da, then
    // DGROUP:0x58c = 1 and DGROUP:0x5c0 = now + 1200.
    virtual void steal_text(int pool, int rec, int s);
    // seg13:0x6890(i), cdecl: the watch windows' refresh of ship i (IS_SMART rows, the right
    // and left displays) [BYTES seg13:0x689a-0x68f6].
    virtual void watch_ship(int i);
    // seg13:0x50b0(0, 0); seg13:0x58dc(0, 0) unless the left display's mark pool is the ship
    // pool; seg7:0x64ac(0, 0, -1) [BYTES seg13:0x67f2-0x6821] (hud/wave.h pictures_tick).
    virtual void watch_tail();
    // DGROUP:0x95fe (the IS_SMART window), DGROUP:0x8f74 (INI SmWnd), DGROUP:0x131c (INI ShipAnim).
    virtual bool smart_window();
    virtual std::int16_t smwnd();
    virtual std::int16_t ship_anim();
    // DGROUP:0x95fa = n, then PostMessage(DGROUP:0x16, 0x9b4, 0, 0L) when n != 0 (the All Ships
    // window, shell.md 0x9b4) [BYTES seg13:0x683e-0x6881].
    virtual void all_ships_notice(int n);

    // The orders' and reactions' calls out (pilot_orders.h).
    // seg13:0x9a18(i, pool, rec), cdecl [BYTES seg13:0x9a18-0x9ae6]: unless (pool, rec) is out
    // of range (rec < 0 or not below the pool's in-use count) or out of play, every defender k <
    // DGROUP:0x940a owned by ship i (entry +0x06 = 14, +0x08 = i) that is not already at (pool,
    // rec) in state 15 and whose pool-2 object is in play gets entry +0x00 = 15, +0x0a = pool,
    // +0x0c = rec and the word DGROUP:0x52f6 + 0x24k (entry -8) = 0; the last one plays
    // seg10:0x754a(0x15, 2, k, 1).
    virtual void send_defenders(int i, int pool, int rec);
    // seg11:0x4c72(i) and seg11:0x4b00(i) (input.h cloak_toggle, shield_toggle): 1 when it toggled.
    virtual std::int16_t cloak_toggle(int i);
    virtual std::int16_t shield_toggle(int i);
    // seg3:0x332e(ids), Pascal (input.h strategy_notice): the player's two-string notice.
    virtual void notice(std::uint32_t ids);
    // seg10:0x754a(id, pool, rec, mode), cdecl: a positional sound.
    virtual void sound(int id, int pool, int rec, int mode);
    // seg13:0x69ad-0x69e0: the player's activity text DGROUP:0x8630 = seg10:0xa332(1421, mode 1,
    // the player's +0x58, ...), DGROUP:0x58c = 1, DGROUP:0x5c0 = now + 700 (Strategy mode, M8).
    virtual void order_text(int i);
    // LoadString(DGROUP:0x9212, id, buf, 40) [BYTES seg4:0x71de], the pilot name DGROUP:0x380c
    // [BYTES seg4:0x713d] and the name +0x0a of MASTER.SHP record n (far DGROUP:0x6e20 + 0x41·n +
    // 0x0a, seg14:0x3eda's copy) [BYTES seg4:0x724e-0x7262].
    virtual std::string lang(int id);
    virtual std::string pilot_name();
    virtual std::string shp_name(int n);

    // Stage 4 and later (ruling R3): each logs once and does nothing.
    virtual void reactions(int i);                         // seg13:0x7ba2
    virtual void gate_goal(int i);                         // seg13:0x3678
    virtual void command_report();                         // seg13:0xa65a
    virtual void seg13_9ae8(int i);                        // seg13:0x9ae8
    virtual void seg13_9ce0(int i);                        // seg13:0x9ce0
    virtual void seg4_474c(int i, int pool, int rec, int kind);  // seg4:0x474c
    virtual void demons_gate(int i);                       // seg4:0x174-0x249 (M6)
    virtual void duel(int i);                              // seg4:0x24f-0x382, seg4:0x44a-0x497 (M6)
};

// Everything the pilot touches. The frame builds one per call (the integrator: the frame's
// FlightCtx pools and ships, its WaveGlobals, the World and the wave's PilotGlobals).
struct PilotCtx {
    Pools& pools;
    ShipTable& ships;
    WaveGlobals& g;  // now, player, ship_pool, last_ship, gates, play_mode, demo, wave_kind, detail, ...
    World& w;        // w8680, w8690, w9000, score_level, budget, w992, w964a, ...
    PilotGlobals& pg;
    shell::Rng& rng;
    PilotHost& host;

    Pilot p(int i) { return pilot(ships, i); }
    ShipRecord& s(int i) { return ships[static_cast<std::size_t>(i)]; }
    ObjectRecord& obj(int pool, int rec);  // a zeroed scratch record outside the storage (ADR 0016)
    ObjectRecord& ship_obj(int i) { return obj(g.ship_pool, i); }
    std::int16_t team(int i);              // design word +0x38 of ship i's design +0x4e
};

// --- The routines -------------------------------------------------------------------------------
// seg8:0x285a(&dx, &dy, first, second) as the original pushes the points: dx = second − first
// (cells, wrapped); returns the distance in cells [BYTES seg8:0x2872-0x2888].
std::int16_t cells_between(const ObjectRecord& first, std::int16_t sx, std::int16_t sy,
                           std::int16_t& dx, std::int16_t& dy);
// seg13:0x371c(pool, rec), cdecl: 0 for a pool other than 12, else 1 for an even record (the
// gate's beam record) and 2 for an odd one [BYTES seg13:0x3729-0x373e].
std::int16_t gate_kind(std::int16_t pool, std::int16_t rec);

// seg4:0x78(i), Pascal, the think [BYTES seg4:0x78-0xbdc]. Its callers: seg13:0x0 on the
// DGROUP:0x134e cadence (flight.cpp, FlightHost::seg4_78), the steer seg4:0xbde for an estimate
// pool of -1 (pilot_steer.cpp, SteerHost::think) and seg3:0x1d0a through replan_think.
void think(PilotCtx& c, int i);
// seg3:0x21fe-0x2226, the Strategy command line's re-plan (seg3:0x1d0a, M8): +0x94 = 0, +0x52 =
// -1, then seg4:0x78(i) unless i is the player while DGROUP:0x95bc != 0 [BYTES seg3:0x21fe-0x222b].
void replan_think(PilotCtx& c, int i);
// seg4:0x2f98(i, a), Pascal, the goal choice [BYTES seg4:0x2f98-0x4748]; `a` is not read.
void choose_goal(PilotCtx& c, int i);
// seg4:0x84de(), cdecl: the greed level DGROUP:0x967e [BYTES seg4:0x84de-0x8625].
void greed_level(PilotCtx& c);
// seg4:0x485c(i, a), Pascal [BYTES seg4:0x485c-0x4cfb].
void after_choice(PilotCtx& c, int i, std::int16_t a);
// seg13:0x78b8(i, j), cdecl: whether i takes an order to help j; a ship index (j's first
// attacker that i likes enough) or -1 to accept, -2 ... -6 to refuse [BYTES seg13:0x78b8-0x7b3f].
std::int16_t help_answer(PilotCtx& c, int i, int j);
// seg4:0x4d00(i), Pascal, the needs [BYTES seg4:0x4d00-0x4fc6].
void needs(PilotCtx& c, int i);
// seg4:0x4fc8(i), Pascal, the hazard step [BYTES seg4:0x4fc8-0x55e0].
void hazard_step(PilotCtx& c, int i);
// seg13:0x1076(pool, rec), Pascal: the obstacle ahead, pool·256 + rec or 0xffff [BYTES
// seg13:0x1076-0x125e].
std::int16_t obstacle_ahead(PilotCtx& c, int pool, int rec);
// seg4:0x6e70(i), cdecl, the weapon choice [BYTES seg4:0x6e70-0x702c].
void choose_weapon(PilotCtx& c, int i);
// seg4:0x6a78(i, k), cdecl: slot k's score [BYTES seg4:0x6a78-0x6e6e].
std::int16_t weapon_score(PilotCtx& c, int i, int k);
// seg4:0x59bc(), cdecl: the player's activity estimate; returns AX [BYTES seg4:0x59bc-0x5d9b].
std::int16_t player_estimate(PilotCtx& c);
// seg4:0x8626(pool, rec, a), cdecl: the estimate's score of one candidate [BYTES seg4:0x8626-0x8b80].
std::int16_t estimate_score(PilotCtx& c, int pool, int rec, int a);

}  // namespace is::world
