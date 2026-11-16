// How a computer ship flies and fires (M5 stage 3 part B, track STEER): the steer seg4:0xbde,
// the turn seg13:0xd6c for a ship, the weapon system decision seg4:0x5654 and the weapon use
// seg4:0x584c (docs/re/pilots.md "The steer", "The weapon system decision", "The weapon use";
// work/analysis/m5/s3-tick.md 2.3, 2.5, 2.9, 2.10 as corrected by s3-tick-verify.md claims 34,
// 35, 37, 38, 49 and its Omissions on seg4:0xbde). The speed cap and the escort slow-down of
// seg11:0x5ab6 are flight.h's (thrust_step, computer_cap, escort_slowdown); the obstacle ahead
// seg13:0x1076, which only the hazard step seg4:0x4fc8 and the pool-6 mover call, is missile.h's
// `threat` (M4 s7, verified claims 22-25); the steer does not call it.
//
// Every routine keeps the original's 16/32-bit widths and its rand() draws in the order written
// (ruling R1). The calls into the think track (seg4:0x78, seg4:0x4fc8, seg4:0x6e70), the contact
// track (seg13:0x16ac) and the M4 launchers go through SteerHost, so the tracks build apart; the
// defaults log once and do nothing.
//
// Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>

#include "world/flight.h"
#include "world/sprite.h"
#include "world/world.h"

namespace is::world {

// --- The DGROUP words the steer keeps that World does not hold (BSS, 0 at load) -------------
// The program's instance is steer_state(): zero at program start and never reset, as in the
// original, whose only writer of DGROUP:0x1cb8 is the steer's own `inc` [BYTES seg4:0xc0a]
// (no other instruction of the listings names DGROUP:0x1cb8).
struct SteerGlobals {
    // DGROUP:0x1cb8 + 2i: ship i's steer count, +1 at every seg4:0xbde(i) that passes the
    // ambulance test; its only reader is the race path, which passes its low bit to
    // seg4:0x144c [BYTES seg4:0xc0a seg4:0xc39].
    std::array<std::int16_t, kShipCount> steers{};
    // DGROUP:0x8e3c + 2i: the race path's turn-counter switch after a drift (seg4:0x113c,
    // seg4:0x1159); the race routines of seg4:0x144c ... seg4:0x1ded read it too [OPEN: its
    // writer; M6] [BYTES seg4:0x1912 seg4:0x1ded].
    std::array<std::int16_t, kShipCount> w8e3c{};
    // DGROUP:0x9524: the Strategy player's weapon slot, set by seg4:0x584c when it changes
    // [BYTES seg4:0x5998] (Strategy mode, a later stage).
    std::int16_t w9524 = 0;
};
// The program's steer words.
SteerGlobals& steer_state();

// --- The calls out of the steer and the weapon use ---------------------------------------------
// Each names the original's routine. The think track's three calls and the races log once
// (world::guard_todo) and do nothing until the integrator binds them; the touch, the view and
// the weapon dispatch default to the routines M4 ported, through missile.h's
// linked_missile_host() (the MissileLinks the program binds in bind_missile_links), the same
// calls the player's weapon system seg11:0xac86 makes, so a computer ship launches through
// them unchanged (each takes the launcher's pool and record).
class SteerHost {
public:
    virtual ~SteerHost() = default;
    // ---- track THINK
    virtual void think(int ship);                  // seg4:0x78(i), Pascal `retf 2`
    virtual void hazard_step(int ship);            // seg4:0x4fc8(i), Pascal (the race path)
    virtual void weapon_choice(int ship);          // seg4:0x6e70(i), cdecl
    // ---- races (M6): seg4:0x144c(i, a), cdecl
    virtual void race_course(int ship, int arg);
    // ---- seg13:0x16ac(i, pool·256 + rec), cdecl: the touch of an unseen ship (capture.h touch,
    // bound as MissileLinks::touch)
    virtual void touch(int ship, std::int16_t pool_rec);
    // ---- seg8:0x29ea(x, y): a world point to the screen (MissileLinks::to_screen)
    virtual Point16 to_screen(std::int16_t x, std::int16_t y);
    // ---- seg4:0x584c's dispatch, the M4 launchers
    virtual void capture_count(int ship, int v);              // seg13:0x4eec(s, v), cdecl
    // seg11:0x7c12(w, 14, s), Pascal: missile.h launch on (world, g, missile_state(),
    // linked_missile_host(), rng).
    virtual void launch(World& world, WaveGlobals& g, shell::Rng& rng, int ship, int pool, int weapon);
    virtual void fuel_ball(int pool, int rec, int life, int kind);  // seg13:0x1318(14, s, life, w)
    virtual void ring_of_death(int pool, int rec);            // seg11:0x8a42(14, s), cdecl
    virtual void tether_use(int pool, int rec, int w);        // seg11:0x947a(14, s, w), cdecl
    virtual void lightning(int pool, int rec);                // seg11:0x9fdc(14, s), cdecl
    virtual void shock(int pool, int rec);                    // seg11:0x9daa(14, s), cdecl (M6, logs once)
    virtual void wildcard(int pool, int rec);                 // seg11:0x9078(14, s), cdecl
    // seg11:0x2f8c(): the Strategy player's weapon display after a slot change (logs once).
    virtual void weapon_display();
    // The weapon word W(w)+off, DGROUP:0x2f54 + 0x50·w + off (records.h WeaponWords); the default
    // reads missile.h missile_state().words, the program's table.
    virtual std::int16_t weapon_word(int w, int off);
};
// A SteerHost with the defaults above (the think track's calls unbound).
SteerHost& default_steer_host();

// Everything the routines touch. `f` is the flight model's context on the same World (its pools
// and ships are World's), so the thrust calls are flight.h's thrust_step.
struct SteerCtx {
    World& w;
    FlightCtx& f;
    SteerHost& h;
    SteerGlobals& sg;
};

// seg13:0xd6c's arguments in the stack order [bp+6] ... [bp+0x18] (Pascal, `retf 0x14`): the
// steer pushes own position, aim point, heading, mode, i, the estimate (pool, rec) and a0, so in
// declaration order seg13:0xd6c(own, aim, heading, mode, i, est pool, est rec, a0)
// (s3-tick-verify.md claim 34) [BYTES seg4:0x102d-0x104f seg13:0x1007].
struct AimArgs {
    std::int16_t a0 = 0;        // [bp+6]: 1 takes the led heading in mode 2
    std::int16_t est_rec = 0;   // [bp+8]
    std::int16_t est_pool = 0;  // [bp+0xa]
    std::int16_t ship = 0;      // [bp+0xc]: S
    std::int16_t mode = 2;      // [bp+0xe]
    std::int16_t cur = 0;       // [bp+0x10]: the heading
    std::int16_t tx = 0, ty = 0;  // [bp+0x12], [bp+0x14]: the aim point
    std::int16_t mx = 0, my = 0;  // [bp+0x16], [bp+0x18]: the own position
};

// seg13:0xd6c for a ship S > -1 [BYTES seg13:0xd6c-0x1007]: the heading held while byte +0xeb
// != 0 or word +0x10 is 1 ... 6; the points in cells (sar 3) with the world wrap by half of
// DGROUP:0x5a0 / 0x5a2 outside a race; the wanted heading h by the mode (2: seg8:0x19b0(dx, dy),
// or with a0 != 0, or in a race for an estimate of pool 4, the led heading seg8:0x2658(14, S,
// est pool, est rec); the race's checkpoint bias otherwise; 3, 4, 5 and 0x1e as missile.h's
// turn_toward); h outside 0 ... 15 keeps the heading; mode 0x1e returns h; h = heading returns
// it; else seg13:0x6716(S) and one step the short way. A mode the switch does not list leaves
// h unwritten: the heading is kept [DESIGN ADR 0016 decision 1] (the accepted D-4 rule of
// missile.cpp's turn_toward for the same site).
std::int16_t aim_heading(SteerCtx& c, const AimArgs& a);

// seg4:0xbde(i), cdecl, the steer [BYTES seg4:0xbde-0x1441] (pilots.md "The steer"): the
// ambulance test, the steer count, the race path, a0, the forced think for an estimate pool of
// -1, the aim point (a cloaked ship's guess +0x14a, the gate detour of an even pool-12
// estimate, else the estimate's position), the distance, the brake orders, the turn, the thrust
// (seg11:0x5ab6 modes 1, 2, 5, 6), the turn counter, the capture approach (the close burst, the
// far burst with one rand() % 18, the tether grab) and the touch box of an unseen ship.
void steer(SteerCtx& c, int i);

// seg4:0x5654(i), Pascal `retf 2`, the weapon system decision [BYTES seg4:0x5654-0x5848]:
// returns 1 after seg4:0x584c(+0x18, i, 25), else 0 (s3-tick-verify.md claim 51).
std::int16_t weapon_decision(SteerCtx& c, int i);

// seg4:0x584c(k, s, life), Pascal `retf 6`, the weapon use [BYTES seg4:0x584c-0x59b7]
// (s3-tick-verify.md claims 48-50): the waits +0xca / +0xd4, seg13:0x4eec(s, 5), the launcher
// by W+0x0a or the dispatch by weapon, and +0x18 = k.
void use_weapon(SteerCtx& c, int k, int s, int life);

// The frame's entry points for the integrator's bindings: each builds a SteerCtx over
// steer_state() for one call. `w` must be the World whose pools and ships `f` moves.
void steer_frame(World& w, FlightCtx& f, SteerHost& h, int i);                       // seg4:0xbde(i)
std::int16_t weapon_decision_frame(World& w, FlightCtx& f, SteerHost& h, int i);      // seg4:0x5654(i)
void use_weapon_frame(World& w, FlightCtx& f, SteerHost& h, int k, int s, int life);  // seg4:0x584c

}  // namespace is::world
