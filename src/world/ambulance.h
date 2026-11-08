// The ambulance's call and flight (docs/re/core.md "The ambulance"; work/analysis/m4/
// s3-ambflight.md with the verifier's s3-ambflight-verify.md, whose CORRECTED verdicts 29 and 52
// are followed): the H key seg11:0x6f7a-0x6fe8, the call seg16:0xb4e with its placement, the
// mover seg16:0xa2 (states 0 ... 9), the intercept heading seg8:0x2658 with its square root
// seg8:0x1290, the park seg16:0x8a0, seg16:0x872, the drop-off of a computer ship seg16:0x32a0,
// the re-entry test seg16:0x33a2 and the re-entry seg16:0x3306, and the served ship's part
// seg13:0x3748 (core.md "The ship's part", verified in M4 stage 4: C036-C047).
//
// A = the ambulance, pool 7 record 0 (far DGROUP:0x1e14). S = the served ship, pool DGROUP:0x9324
// record DGROUP:0x952e (World::w952e). DGROUP:0x9e49 (the ambulance's state) is World::w9e49 and
// DGROUP:0x1e88 World::w1e88; the other words are AmbulanceState's. 16-bit arithmetic as the
// original's (ADR 0013 decision 7): every `idiv r16` is signed 16-bit, the listing's `cdq` after a
// 16-bit `imul` is `cwd` (lo16 of the product).
//
// Portable, no Win32. Everything outside the World goes through AmbulanceHost; the frame's
// routines (the integrator, the brake, the turn, the collisions) come from the UpdateCtx.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "world/flight.h"
#include "world/purchases.h"
#include "world/world.h"

namespace is::world {

struct UpdateCtx;
struct FrameGlobals;

// The ambulance's DGROUP words beside World::w9e49 / w952e / w1e88. All BSS (0 at load) unless
// stated [BYTES DGROUP:0x9e41-0x9e5d]. The visit (seg16:0xcfe, the VISIT track) writes the lift
// words at its phase 10. The price words DGROUP:0x9e55 / 0x9e57 are the Shop's (purchases.h),
// and so are DGROUP:0x9e5b / 0x9e5d: `served_alive` and `sold` are references to Shop::alive
// and Shop::sold_total, the program's Shop unless a test gives its own, so the pickup, the
// visit, the Options dialog and seg16:0x3784's dword write reach one store (triage D-25)
// [DESIGN].
struct AmbulanceState {
    explicit AmbulanceState(Shop& s = shop()) : served_alive(s.alive), sold(s.sold_total) {}

    std::int16_t lift_step = 0;    // DGROUP:0x9e41: +1 / 0 / -1 per frame on the lift
    std::int16_t spin = 0;         // DGROUP:0x9e43: 0 ... 130, the logo spin phase
    std::int16_t frame = 0;        // DGROUP:0x9e45: the logo-strip cell drawn (0 ... 13)
    std::int16_t lift = 0;         // DGROUP:0x9e47: 0 ... 15 (seg8:0xd04 reads it: SpriteGlobals::dg_9e47)
    std::int16_t dist = 0;         // DGROUP:0x9e4b: the distance to the target, cells
    std::int16_t cooldown = 0;     // DGROUP:0x9e4d: frames without steering after a hit
    std::int16_t spin_step = 0;    // DGROUP:0x9e4f: clamp(vx / 8); read only at seg16:0x210
    std::int16_t tick = 0;         // DGROUP:0x9e51: the 0 ... 7 frame counter
    std::int16_t pickup_dist = 0;  // DGROUP:0x9e53: DGROUP:0x9e4b at the 2 -> 3 step
    std::int16_t& served_alive;    // DGROUP:0x9e5b: the served ship's `alive` at the pickup (Shop::alive)
    std::int16_t& sold;            // DGROUP:0x9e5d: the visit's sold total (Shop::sold_total)
    std::int16_t prev_heading = 0; // DGROUP:0x1c42: S's heading last frame, -1 at the stop
    std::int16_t busy = 0;         // DGROUP:0x111e: 1 in states 1 ... 5; no reader in INSPACE.EXE (claim 58)
    std::int16_t w9596 = 0;        // DGROUP:0x9596 = 1 at the 2 -> 3 step; no reader (claim 58) [OPEN]
    std::int16_t pickup_size = 0;  // DGROUP:0x959e: `Size` DGROUP:0x9156 at the pickup (the visit reads it)
    std::uint32_t due = 0;         // dword DGROUP:0x6e10: the mover's deadline
    // dword DGROUP:0x6da8: the re-entry deadline (the visit's phase 64 writes now + 3000; the park
    // and seg16:0x3306 clear it). The frame's FrameGlobals::dg_6da8 stands for the same word: the
    // program syncs them around the frame's step 20 [DESIGN] (see ambulance_frame).
    std::uint32_t reentry_due = 0;

    // The dword store at seg16:0x3b07 / seg16:0x3784's end: DGROUP:0x9e5b = 1 with DGROUP:0x9e5d = 0.
    void set_served_alive_dword(std::uint32_t v) {
        served_alive = static_cast<std::int16_t>(v & 0xffffu);
        sold = static_cast<std::int16_t>(v >> 16);
    }
};

// The program's AmbulanceState (DGROUP), one for the whole run.
AmbulanceState& ambulance_state();

// seg9:0x6865-0x6870 in the wave placement seg9:0x6624: DGROUP:0x9e5b = 0, DGROUP:0x9e4b = 0,
// DGROUP:0x9e45 = 0 (before World::w9e49 = 10 and A.state = 1, which setup.cpp writes)
// [BYTES seg9:0x6865 seg9:0x6867 seg9:0x686a seg9:0x686d] (verified claim 55).
void ambulance_placement(AmbulanceState& a);
// The briefing entry seg9:0x5cca's part: DGROUP:0x9e5b = 1 and dword DGROUP:0x1e88 = 0 (beside
// A.state = 1, DGROUP:0x9e49 = 10 and DGROUP:0x952e = -1, which setup.cpp writes)
// [BYTES seg9:0x5d35 seg9:0x5d38 seg9:0x5d58] (verified claim 55).
void ambulance_briefing_entry(AmbulanceState& a, World& w);

// The calls the ambulance makes outside the World; the defaults do nothing.
class AmbulanceHost {
public:
    virtual ~AmbulanceHost() = default;
    // seg10:0x754a(id, pool, rec, arg).
    virtual void sound(int id, int pool, int rec, int arg) { (void)id, (void)pool, (void)rec, (void)arg; }
    // seg10:0x744c(id) (0x6e stops the sounds).
    virtual void play(int id) { (void)id; }
    // seg12:0x2556(pool, rec), seg12:0x26a4(pool, rec), seg7:0x13b2(mode, pool, rec): the
    // object's private buffer and the materialise effect (entry_effect.h).
    virtual void buffer(int pool, int rec) { (void)pool, (void)rec; }
    virtual void buffer_free(int pool, int rec) { (void)pool, (void)rec; }
    virtual void effect(int mode, int pool, int rec) { (void)mode, (void)pool, (void)rec; }
    // seg3:0x6ec(recipient, pool, rec, code), cdecl.
    virtual void message(int recipient, int pool, int rec, int code) {
        (void)recipient, (void)pool, (void)rec, (void)code;
    }
    // seg16:0x3784(ship, mode): the purchases (the VISIT track's) [BYTES seg16:0x87e].
    virtual void purchases(int ship, int mode) { (void)ship, (void)mode; }
    // seg11:0x2f8c, the weapon picture (src/hud) [BYTES seg16:0x3356].
    virtual void weapon_picture() {}
    // seg8:0xb76(pool_rec, kind, DGROUP:0x39e8[slot], masked) on the window DC.
    virtual void draw_sprite(std::int16_t pool_rec, int kind, int slot, int masked) {
        (void)pool_rec, (void)kind, (void)slot, (void)masked;
    }
    // seg13:0x3d40(den, num, o_rec, o_pool, s_rec, s_pool), the pick-up composite (draw.h
    // pickup_composite; core.md "The ship's part", seg13:0x3d40, C044-C047).
    virtual void composite(std::int16_t den, std::int16_t num, int o_rec, int o_pool, int s_rec, int s_pool) {
        (void)den, (void)num, (void)o_rec, (void)o_pool, (void)s_rec, (void)s_pool;
    }
    // Ctrl+H seg13:0x81c2(player) and Alt+H seg13:0x9ae8(player): Command mode (M5) [OPEN].
    virtual void ctrl_h(int ship) { (void)ship; }
    virtual void alt_h(int ship) { (void)ship; }
    // A routine the port does not carry: a TODO log.
    virtual void todo(const std::string& what, const char* address) { (void)what, (void)address; }
};

struct AmbulanceCtx {
    World& w;
    AmbulanceState& a;
    std::uint32_t now;  // DGROUP:0x1e50
    AmbulanceHost& host;
};

// ---- the call

// seg16:0xb4e(ship) [BYTES seg16:0xb4e-0xcf4] (verified claims 4-8; claim 4's extra test): 0 when
// refused; 1 when dispatched. A parked ambulance (`state` != 0) is placed at the gate nearest the
// ship; one in flight (`state` 0) only switches its target (claim 5).
std::int16_t ambulance_call(AmbulanceCtx& c, int ship);
// The reply seg11:0x6fb6-0x6fd1: 7 when seg16:0xb4e returned non-zero; else 132 when the 16-bit
// sum lo16(credit) + resources < -300 (signed `jge`), else 8 (verified claim 1).
int ambulance_reply_code(const World& w, int ship, std::int16_t called);
// WM_KEYDOWN 'H' (seg11:0x6f7a-0x6fe8): Ctrl seg13:0x81c2(player); Alt seg13:0x9ae8(player) and
// return; Ctrl returns; the demo (DGROUP:0x95bc = 0) returns; else seg16:0xb4e(player) and
// seg3:0x6ec(player, 7, 0, code). Returns the code shown, 0 when none.
// Routing (the FUEL track's src/world/input.cpp key switch, the fixer's wiring): call
// ambulance_key_program(ctrl, alt) for WM_KEYDOWN 'H'; the key is already exempt from the
// state gate (input.cpp, [BYTES seg11:0x6c75-0x6cd1]).
int ambulance_key(AmbulanceCtx& c, bool ctrl, bool alt);

// ---- the mover (seg16:0xa2), called by the frame while A's `state` is 0 (update.cpp step 20)

// seg16:0xa2 [BYTES seg16:0xa2-0x84f] (verified claims 9-33; claim 29 as corrected): the head
// (the tick, DGROUP:0x111e, the lift, the cell, the draw, the spin, the served-ship-gone test),
// the state switch (0, 1, 2, 3/4, 6, 9), the abort, the steering, the distance, the integrator,
// the cooldown and the collision. `u` gives the frame's integrator, brake, turn and collisions.
void ambulance_move(UpdateCtx& u, AmbulanceCtx& c);

// seg8:0x1290(N, g) (Pascal, `retf 8`): one Newton step, g < 3 (signed) -> 3, then
// (N / g + g) >> 1 (32-bit `idiv`, `sar`); the loop at seg8:0x12c6 is unreachable (verified claim
// 36) [BYTES seg8:0x129d-0x12c4]. Exported for seg13:0xf0c's mode (missile.h) too.
std::int32_t newton_root(std::int32_t n, std::int32_t g);
// seg8:0x2658(a_rec, a_pool, b_rec, b_pool) (Pascal, `retf 8`), the intercept heading of chaser
// b towards target a (verified claims 34, 35 with the widths of 35) [BYTES seg8:0x2658-0x2850].
// The missile's seg13:0xd6c mode with a0 != 0 (seg13:0xf0c) calls the same routine.
std::int16_t intercept_heading(const Pools& p, int a_pool, int a_rec, int b_pool, int b_rec);

// seg16:0x8a0, the park [BYTES seg16:0x8aa-0x8d3]: seg12:0x2556(7, 0), seg7:0x13b2(1, 7, 0),
// DGROUP:0x9e49 = 10, DGROUP:0x952e = -1, DGROUP:0x111e = 0, dword DGROUP:0x6da8 = 0.
void ambulance_park(AmbulanceCtx& c);
// seg16:0x872 [BYTES seg16:0x87c-0x893]: seg16:0x3784(DGROUP:0x952e, 0), then
// seg3:0x6ec(DGROUP:0x952e, 7, 0, 0x54).
void ambulance_repairs_done(AmbulanceCtx& c);

// ---- the tick's deadline DGROUP:0x1e88 (wave_end.cpp) and the re-entry (update.cpp step 20)

// seg16:0x32a0 [BYTES seg16:0x32a0-0x32fb] (verify claim 52 as corrected): every case of the
// due DGROUP:0x1e88 but the player's visit: the lift words, seg16:0x872, A.state = 0, with A
// byte +0x2e = 0xff seg12:0x2556(7, 0), seg7:0x13b2(2, 7, 0) and DGROUP:0x9e41 = 1; then
// DGROUP:0x9e49 = 6 and DGROUP:0x111e = 0.
void ambulance_drop(AmbulanceCtx& c);
// seg16:0x33a2 [BYTES seg16:0x33b1-0x345f] (verified claim 49): 1 when no `state`-0 record below
// the in-use count of pools 0 ... DGROUP:0x9324 (not pool 7, not the served ship) has its
// (x, y) in SetRect(A.x - 0x230, A.y - 0x280, A.x + 0x640, A.y + 0x280) (USER PtInRect).
bool ambulance_reentry_clear(const AmbulanceCtx& c);
// seg16:0x3306 [BYTES seg16:0x3311-0x3393] (verified claims 50, 51): S.state = A.state = 0,
// DGROUP:0x6da8 = 0, seg12:0x26a4 / seg12:0x2556 on S, seg11:0x2f8c; with A byte +0x2e = 0xff
// seg12:0x2556(7, 0), seg7:0x13b2(2, 7, 0), lift and DGROUP:0x9e41 = 1 and A.mass = 800; then
// DGROUP:0x9e49 = 6.
void ambulance_reentry(AmbulanceCtx& c);

// ---- the served ship's part

// seg13:0x3748(i) [BYTES seg13:0x3748-0x3b7c] (core.md "The ship's part", C036-C043): the ship mover's
// call for ship DGROUP:0x952e while 1 <= DGROUP:0x9e49 <= 6 (flight.cpp, FlightHost::seg13_3748).
// State 3 pulls the ship into A at 70 a frame and, within 5 cells, sets state 4 with a 1100 ms
// deadline; state 6 (A byte +0x2e = 0) pushes it out at 50 a frame with the lift at 15, plays
// CAPTURED at lift 2 and MISSILE at lift 14, and sets state 8 once it is out.
void served_ship_move(FlightCtx& f, AmbulanceCtx& c, int i);

// ---- the program's links (src/world/flight_hooks.cpp and src/hud fill them; an empty slot logs)

struct AmbulanceLinks {
    std::function<void(int id, int pool, int rec, int arg)> sound;   // seg10:0x754a
    std::function<void(int id)> play;                                // seg10:0x744c
    std::function<void(int pool, int rec)> buffer;                   // seg12:0x2556
    std::function<void(int pool, int rec)> buffer_free;              // seg12:0x26a4
    std::function<void(int mode, int pool, int rec)> effect;         // seg7:0x13b2
    std::function<void(int recipient, int pool, int rec, int code)> message;  // seg3:0x6ec
    std::function<void(int ship, int mode)> purchases;               // seg16:0x3784 (VISIT)
    std::function<void()> weapon_picture;                            // seg11:0x2f8c
    std::function<void(std::int16_t pr, int kind, int slot, int masked)> draw_sprite;  // seg8:0xb76
    std::function<void(std::int16_t den, std::int16_t num, int o_rec, int o_pool, int s_rec, int s_pool)>
        composite;                                                   // seg13:0x3d40 (draw.h)
    std::function<void(const std::string& what, const char* address)> todo;  // a TODO log
};
AmbulanceLinks& ambulance_links();
// The AmbulanceHost over ambulance_links(): an empty slot logs once through `todo` (or does
// nothing without one).
AmbulanceHost& linked_ambulance_host();

// The program's entry points on its World with ambulance_state() and linked_ambulance_host():
// the H key (input.cpp's 'H' case), the frame's seg16:0xa2 (UpdateHost::seg16_a2),
// seg16:0x33a2 / seg16:0x3306 (UpdateHost::seg16_33a2 / seg16_3306) and seg13:0x3748
// (FlightHost::seg13_3748). The frame keeps copies of DGROUP:0x9e49, 0x952e, 0x9e5b
// (WaveGlobals) and 0x6da8 (FrameGlobals): these entry points write them back after the call
// [DESIGN]; the frame's sync_in must copy World::w952e, ambulance_state().served_alive and
// ambulance_state().reentry_due in as it copies w9e49 (the fixer's request).
int ambulance_key_program(World& w, std::uint32_t now, bool ctrl, bool alt);
void ambulance_frame(UpdateCtx& u, World& w);
bool ambulance_reentry_clear_program(World& w, std::uint32_t now);
void ambulance_reentry_program(World& w, WaveGlobals& g, FrameGlobals& f);
void served_ship_move_program(FlightCtx& f, World& w, int i);

}  // namespace is::world
