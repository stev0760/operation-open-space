// The wave's update (docs/re/world.md "Update loop"; docs/re/timing.md "Wave passes", "Game
// time"): the work of a wave pass and of a wave frame in the original's address order, the ship
// entry seg7:0x1d0c, the rock mover seg7:0x3bae and the calls of the movers this stage does not
// port, each a named stub that cites its address (ADR 0013 decision 8).
//
// ADR 0001: the port runs one pass before every frame and no pass machinery; the pass's
// frame-gated work runs on that pass, the adaptive thresholds (seg7:0x9ed-0xaf7) are not
// ported, and game time counts frames (DGROUP:0x1e68, DGROUP:0x5a8). [DESIGN ADR 0001]
//
// Portable, no Win32. Calls into other parts of the port go through UpdateHost, whose defaults
// do nothing.
#pragma once

#include <cstdint>

#include "world/draw.h"
#include "world/flight.h"
#include "world/input.h"
#include "world/sprite.h"
#include "world/view.h"
#include "world/visibility.h"
#include "world/world.h"

namespace is::world {

// The movers of seg7:0xcd3-0xd2e that other stages port, by address.
enum class Mover : std::uint8_t {
    k12_532c,  // pool 9
    k12_633e,  // pool 8 (gates and hazards)
    k8_8256,   // pool 2
    k7_309c,
    k7_32d0,
    k7_40c4,   // pool DGROUP:0x9328 = 5
    k12_0,     // pool 6
    k6_73f6,
    k6_7c14,
    k7_5e04,   // while DGROUP:0x9456 > 0
    k12_3148,  // pool 10
    k12_6b1e,  // pool 11
    k12_ae4,   // pool 4, while DGROUP:0x9600 != 0
    k12_3e98,  // seg12:0x3e98(0)
};
const char* mover_address(Mover m);

// The DGROUP words of the pass and the frame beyond WaveGlobals.
struct FrameGlobals {
    std::int16_t dg_17b6 = 0;           // DGROUP:0x17b6, the 8-frame counter
    std::uint32_t info_due = 0;         // DGROUP:0x17b8, the information displays' deadline
    std::uint32_t joy_due = 0;          // DGROUP:0x2388, the joystick read's deadline
    std::int16_t dg_9142 = 0;           // DGROUP:0x9142, the 0 ... 54 frame counter
    std::int16_t effects_on = 0;        // DGROUP:0x9430
    std::int16_t last_effect = 0;       // DGROUP:0x9434
    std::int16_t beam_a = 3;            // DGROUP:0x594 (seg11:0x4e19 sets 7)
    std::int16_t beam_b = 2;            // DGROUP:0x598, no writer
    std::uint32_t beam_passes = 0;      // DGROUP:0x5cc
    std::int16_t beam_result = 0;       // DGROUP:0x9136
    std::int16_t entry_pending = 0;     // DGROUP:0x9140: a ship waits off-world (state 100)
    std::int16_t dg_9146 = 0;           // DGROUP:0x9146
    std::int16_t size_reread = 0;       // DGROUP:0x103c
    std::uint32_t speed_shown = 0;      // DGROUP:0x870c, the end of the speed display
    std::int16_t dg_9608 = 0;           // DGROUP:0x9608
    std::int16_t info_mode = 2;         // DGROUP:0xa76: 2 target, 3 message, 4 frame rate
    std::int16_t dg_95fc = 0;           // DGROUP:0x95fc
    std::uint32_t dg_6da8 = 0;          // DGROUP:0x6da8
    std::int16_t dg_5d2 = 0;            // DGROUP:0x5d2, toggled each frame
    std::int16_t dg_1016 = 0;           // DGROUP:0x1016
    std::int16_t dg_9456 = 0;           // DGROUP:0x9456
    std::int16_t dg_9600 = 0;           // DGROUP:0x9600
    std::uint32_t music_due = 0;        // DGROUP:0x2384
    std::int16_t dg_950 = 0;            // DGROUP:0x950
    std::int16_t race = 0;              // DGROUP:0x94e, the race flag
    std::int16_t dg_58a = 0;            // DGROUP:0x58a
    std::int16_t dg_588 = 0;            // DGROUP:0x588
    std::int16_t dg_9148 = 0;           // DGROUP:0x9148
    std::int16_t dg_1048 = 0;           // DGROUP:0x1048
    std::uint32_t dg_1e6c = 0;          // DGROUP:0x1e6c
    std::int16_t dg_92f0 = 0;           // DGROUP:0x92f0
    std::int16_t dg_914e = 0;           // DGROUP:0x914e
    std::uint32_t dg_5a4 = 0;           // DGROUP:0x5a4
    std::uint32_t dg_5b8 = 0;           // DGROUP:0x5b8
    std::int16_t dg_943a = 0, dg_943c = 0;  // DGROUP:0x943a, 0x943c: the colliding pool, record
    std::int16_t dg_9454 = 0;           // DGROUP:0x9454
};

class UpdateHost : public FlightHost {
public:
    // ---- the pass
    virtual void fuel_mass() {}                        // seg11:0x1bcc
    virtual void turn_followup() {}                    // seg11:0x894 [OPEN]
    virtual void radar_refresh() {}                    // seg11:0x3396
    virtual void select_clip() {}                      // SelectClipRgn(DGROUP:0x13dc, DGROUP:0x12)
    virtual void fire(int ship) { (void)ship; }        // seg11:0x4e0a (weapons, M4)
    virtual void seg8_9464() {}                        // [OPEN]
    virtual void seg8_9564() {}                        // [OPEN]
    virtual void seg7_2128() {}                        // [OPEN]
    virtual std::int16_t beam(int i, int j) { return (void)i, (void)j, 0; }  // seg8:0x3aec (M4)
    virtual void effect(int k) { (void)k; }            // seg8:0x3fbe
    virtual void burst() {}                            // seg7:0x375a, the particle burst
    // ---- the frame
    // ISPACE.DLL ordinal 41 as seg11:0x72b2 calls it: fills far DGROUP:0x1e4c and returns the
    // read's result, 0xa7 (JOYERR_UNPLUGGED) for no stick [BYTES seg11:0x72c9 seg11:0x72ce].
    virtual std::uint16_t joy_read() { return 0; }
    // PostMessage(main DGROUP:0x16, msg, key, 0) of a joystick button (seg11:0x7588).
    virtual void post_key(std::uint32_t msg, std::uint16_t key) { (void)msg, (void)key; }
    virtual void seg18_frame() {}                      // seg16:0x64fa in the seg18 mode
    virtual void size_reread() {}                      // seg10:0x7f88
    virtual void stars(int arg) { (void)arg; }         // seg8:0x158a(0)
    virtual void visibility() {}                       // seg8:0x4e14 [OPEN]
    virtual void speed_display_end_3_6ec() {}          // seg3:0x6ec(player, 14, +0x58, 2)
    virtual void speed_display_end_7_119c() {}         // seg7:0x119c(2, 0)
    virtual void lines() {}                            // seg12:0x3778
    virtual void dissolve(int i) { (void)i; }          // seg7:0x3a42(i)
    virtual void direction_mark(int d) { (void)d; }    // seg13:0x24aa(d)
    virtual void seg16_a2() {}
    virtual bool seg16_33a2() { return false; }
    virtual void seg16_3306() {}
    virtual void seg11_a9ea(int arg) { (void)arg; }
    virtual void sound_frame() {}                      // seg10:0x7b48 (M7's audio::frame)
    virtual void seg12_4d8e() {}
    virtual void seg18_d94() {}
    virtual void mover(Mover m) { (void)m; }           // the movers this stage does not port
    virtual void music_due() {}                        // seg13:0xba04
    virtual void seg13_b93a() {}
    virtual void player_tick() {}                      // seg7:0xdaf-0x117d, every 1500 ms [OPEN]
    virtual void seg7_158e() {}
    virtual void seg13_447a() {}
    // ---- the ship entry seg7:0x1d0c
    virtual void ship_entered(int ship) { (void)ship; }  // seg12:0x2556 and seg7:0x13b2(2, 14, i)
    virtual void seg13_563c() {}
    virtual void set_design_word_22(int design, std::int16_t v) { (void)design, (void)v; }
    // ---- the rock mover seg7:0x3bae and the gate posts seg12:0x1776
    // SelectObject(DGROUP:0x13dc, DGROUP:0x3b74[b]) and back: the heated rock's or post's brush.
    // The defaults select wave_graphics().heat[b] on blitter()'s window DC (M3 s4 DRAW).
    virtual std::uint16_t select_rock_brush(int b);
    virtual void restore_brush(std::uint16_t old);
    // seg8:0x50aa(pool * 256 + rec): the collision test; 0xFFFF for none (world.md "Collisions").
    virtual std::uint16_t collide(std::int16_t pool_rec) { return (void)pool_rec, 0xffffu; }
    // seg8:0x546c(...) then seg8:0x5df2(...): the collision's response [OPEN] M4.
    virtual void rock_hit(int rock, std::uint16_t hit) { (void)rock, (void)hit; }

    // ---- the drawing the frame does itself (M3 s4 DRAW)
    // The World whose pools the frame moves: the gates' records, the stars, the link slots and
    // the pool-8 table live there, not in FlightCtx. The default is the World set_frame_world
    // registered (the program's, src/world/hooks.cpp) when its pools are `pools`, else null;
    // with null the frame calls stars(), lines() and mover(k12_633e) instead [DESIGN].
    virtual World* frame_world(Pools& pools);
    // The window's Blitter for the draws beside seg8:0xb76 (the beam, the stars); the default
    // is wave_blitter(). With null the frame calls stars() and draws no beam.
    virtual Blitter* blitter() { return wave_blitter(); }
    // The list of the frame's visibility pass (DGROUP:0x507c / 0x50e0, count DGROUP:0x9146);
    // the default is last_visibility().
    virtual const VisibilityGlobals* visible_list() { return last_visibility(); }
    // The routines seg12:0x633e calls for the hazards (M5): seg12:0x390e(i) (kind 1),
    // seg12:0x5636(i) (kind 2), seg12:0x7006 (every other frame). The M5 port of seg12:0x5636
    // must clear World::w949e (DGROUP:0x949e) as it does at seg12:0x5ef8, which gates the
    // hazard marks' erase [BYTES seg12:0x5ef8 seg12:0x6408] (M3 s4 fixer, review L12).
    virtual void hazard(int kind, int i) { (void)kind, (void)i; }
    virtual void seg12_7006() {}
    // seg8:0x546c(...) then seg8:0x5df2(8, r, pool, rec, ...): a post's collision response (M4).
    virtual void post_hit(int rec, std::uint16_t hit) { (void)rec, (void)hit; }
    // seg12:0x2288(pool, rec): a ship passes the gate, the gate exit [OPEN: not read].
    virtual void gate_passed(int pool, int rec) { (void)pool, (void)rec; }
    // seg13:0x2f30 after seg12:0x1f7a's AI words for ship k (M5).
    virtual void gate_ai(int ship) { (void)ship; }
    // seg12:0x3778's erase and its seg7:0x2080 call (draw.h LinkHooks); null draws nothing.
    virtual LinkHooks* link_hooks() { return nullptr; }
    // seg8:0x1320, the Bright starfield [OPEN].
    virtual void bright_stars(int arg) { (void)arg; }
};

// The program's World for UpdateHost::frame_world (null: none) (M3 s4 DRAW).
void set_frame_world(World* w);

// What the pass and the frame touch.
struct UpdateCtx {
    FlightCtx flight;
    FrameGlobals& f;
    ViewState& view;
    UpdateHost& host;
    JoyState& joy;
    JoyButtons& joy_buttons;
};

// The wave pass (seg7:0x7fe-0x993) before a due frame. `max_rate` is FrameRateSet above 1000,
// where the original's threshold is 0 and the turn follow-up waits for frames % 3 = 0.
void wave_pass(UpdateCtx& c, bool max_rate);
// The wave frame (seg7:0x9ce-0x118d).
void wave_frame(UpdateCtx& c);
// seg7:0x1d0c: the first off-world ship whose time has come enters at a gate.
void ship_entry(UpdateCtx& c);
// seg7:0x3bae: the rock mover.
void move_rocks(UpdateCtx& c);

// ---- the pool-8 mover seg12:0x633e (work/analysis/m3/s4-draw.md 1) (M3 s4 DRAW)

// seg12:0x633e [BYTES seg12:0x633e-0x649f]: DGROUP:0x94a0, seg12:0x7006 every other frame, the
// oscillator DGROUP:0x0d08, the kind dispatch of the in-use pool-8 records with `state` 0 (kind
// 1 and 2 go to the host, kind 3 runs seg12:0x1776 for the pair and skips the second post), the
// hazard marks' erase and the two deadlines.
void move_pool8(UpdateCtx& c, World& w);
// seg12:0x1776(p) [BYTES seg12:0x1776-0x1f75]: gate p's posts 2p, 2p + 1: the beam brush
// index, the moving gate, the posts through seg8:0xb76 from slot 1 (heat <= 2, SRCCOPY) or
// slot 2 under DGROUP:0x3b74 (MERGECOPY), the beam (r1 only: the erase of the old position,
// then PATCOPY with DGROUP:0x3b88[index]), the OUT sign flash, the collisions and the passage.
void move_gate(UpdateCtx& c, World& w, int p);
// seg12:0x1f7a(p) [BYTES seg12:0x1f7a-0x2285]: the first listed object of `state` 0 whose square
// (x, y, x + extent, y + extent) meets gate p's beam rectangle, not pool 7, mass not 0xdac, a
// ship only after its dword +0x88 + 300 (the player) or + 3000, and byte +0x2e != 1 unless a
// ship with word +0x2e = 2: pool << 8 | rec, else 0xffff. A computer ship on its way to the
// gate stops and gets its AI words on the way.
std::uint16_t gate_test(UpdateCtx& c, World& w, int p);

}  // namespace is::world
