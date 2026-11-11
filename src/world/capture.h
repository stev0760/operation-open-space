// Capture (docs/re/core.md "Capture", "Scoring"): the touch routine seg13:0x16ac with its icon
// capture, fuel-tank, pod and pool-6 branches, the push-out seg8:0x7d70, the rivals seg13:0x3008,
// the ship word +0x82 seg13:0x4eec, the counters seg7:0x5ae8 / seg7:0x5a46 and the dissolve
// step of seg6:0xdaa.
//
// Sources: the listings (isre asm) read with work/analysis/m4/s2-capture.md and its verification
// s2-capture-verify.md (claims 30 and 33 as the verifier corrected them; the precision notes on
// claims 6, 10, 14-16, 18, 26 and 32 followed). Where the analysis corrects core.md, the analysis
// is followed: the touch is reached from the icon mover (icon_mover.h), not from the ship mover,
// for an icon; the pool-6 fuel and tumble writes all lie behind W(w)+0x42; the pool-6 sounds
// pass DGROUP:0x9324, not 14. All arithmetic keeps the original's widths (ADR 0013 decision 7):
// every `idiv` is a 16-bit signed divide (the listing prints `cwd` as cdq).
//
// The routines and tables other parts own are reached through CaptureHost, whose defaults do
// nothing (each names its address); the collision words and the damage routine seg8:0x45e0 come
// from the CollideCtx of the caller, whose DGROUP:0x943a / 0x943c decide the dead-ship rule.
//
// Portable, no Win32.
#pragma once

#include <cstdint>
#include <string>

#include "world/collide.h"
#include "world/flight.h"
#include "world/weapon_load.h"
#include "world/world.h"

namespace is::world {

// The calls out of the touch routine.
class CaptureHost {
public:
    virtual ~CaptureHost() = default;
    // PatBlt(DGROUP:0x13dc, x, y, w, h, 0x00420042 BLACKNESS) [BYTES seg13:0x1756-0x1782].
    virtual void erase(int x, int y, int w, int h) { (void)x, (void)y, (void)w, (void)h; }
    // seg7:0x3a42(rec): the icon dissolve's first step (draw.h dissolve_icon) [BYTES seg13:0x18b7].
    virtual void dissolve(int rec) { (void)rec; }
    // seg10:0x754a(id, pool, rec, mode) and seg10:0x744c(id) (src/audio).
    virtual void sound(int id, int pool, int rec, int mode) { (void)id, (void)pool, (void)rec, (void)mode; }
    virtual void play(int id) { (void)id; }
    // seg3:0x6ec(recipient, pool, rec, code): the message display (src/hud messages).
    virtual void message(int ship, int pool, int rec, int code) { (void)ship, (void)pool, (void)rec, (void)code; }
    // seg2:0x170e: the menu state after the defenders change hands [OPEN: not read].
    virtual void menu_refresh() {}
    // ENG_LANG string id (LoadString of DGROUP:0x9212, 0x28 bytes: at most 39 characters).
    virtual std::string lang(int id) { return (void)id, std::string{}; }
    // W(w)+off, word DGROUP:0x2f54 + 0x50·w + off (the weapon table's owner); 0 by default.
    virtual std::int16_t weapon_word(int w, int off) { return (void)w, (void)off, 0; }
    // The design word DGROUP:0x970a + 0xad·n, the team (ship_entry's design_kind).
    virtual std::int16_t team(int design) { return (void)design, 0; }
    // DGROUP:0x3854 + 2·team, the team table seg13:0x3008 divides by -100 [OPEN: its contents].
    virtual std::int16_t team_3854(int team) { return (void)team, 0; }
    // The computer pilots and the other movers' routines (M5, M6) [OPEN]; each names its address.
    virtual void seg13_b6f2(int ship, int code) { (void)ship, (void)code; }
    virtual void seg13_2714(int a, int b, int c, int d, int e, int f) {
        (void)a, (void)b, (void)c, (void)d, (void)e, (void)f;
    }
    virtual void seg4_5e8e(int a, int b, int c, int d) { (void)a, (void)b, (void)c, (void)d; }
    virtual std::int16_t seg4_8d3a() { return -1; }  // the race's leading ship [OPEN M6]
    virtual void seg13_adbe(int ship, int a, int b) { (void)ship, (void)a, (void)b; }
    // seg12:0xe5c(rec): the fuel tank's own dissolve step, ported by tank.h tank_dissolve
    // [BYTES seg12:0xe5c-0x129c]; the program's host (update.h CaptureForward) reaches it through
    // UpdateHost::tank_dissolve, the default does nothing (s2-capture.md 4a, verified claim 32).
    virtual void seg12_e5c(int rec) { (void)rec; }
    // seg12:0x129e(pool, rec): the pod's and the pool-6 record's dissolve, ported by pods.h
    // pod_dissolve [BYTES seg12:0x129e-0x1774]; the program's host (update.h CaptureForward)
    // reaches it through UpdateHost::pod_dissolve, the default does nothing.
    virtual void seg12_129e(int pool, int rec) { (void)pool, (void)rec; }
    // seg12:0x6d64(ship, weapon): a pod's weapon into a new slot, weapon_load.h load_weapon on
    // the program's weapon words and seg10:0x533a hook (weapon_load.h load_weapon_program)
    // [BYTES seg13:0x1e4e seg12:0x6d64-0x6de9] (M4 s5 WEAPONS).
    virtual void seg12_6d64(World& w, int ship, int weapon) { load_weapon_program(w, ship, weapon); }
};

// What the touch routine touches: the caller's collision context (the World, the wave words,
// DGROUP:0x943a / 0x943c, the CollideHost for seg8:0x45e0), the frame's flight context (for
// seg13:0x6716) and the host.
struct CaptureCtx {
    CollideCtx& x;
    FlightCtx& f;
    CaptureHost& h;
};

// seg13:0x16ac(ship, pool·256 + rec), cdecl [BYTES seg13:0x16ac-0x1fbf].
void touch(CaptureCtx& c, int ship, std::int16_t pool_rec);

// seg8:0x7d70, `retf 0x24`: the touched object pushed off the ship; only the object's +0x00,
// +0x02, +0x0c and +0x0e are written (verified claims 12-16) [BYTES seg8:0x7d70-0x8046].
void push_out(const ObjectRecord& ship, ObjectRecord& obj);

// The icon's value DGROUP:0x9604 before the "< 0 -> 50" fallback: (damage · -90) / 210 + 100,
// the 16-bit 3-operand `imul`, then a signed 16-bit `idiv` [BYTES seg13:0x1ae7-0x1af2].
constexpr std::int16_t capture_value(std::int16_t damage) {
    return add16(idiv16(mul16(damage, -90), 210), 100);
}

// The capture sound of seg13:0x1bba-0x1c04: 0x23 when (no defenders and the icon's `fixed`
// +0x54 = 0) or the wave is kind 6; else 0x26 below 2 defenders; else 0x66.
constexpr int capture_sound(std::int16_t defenders, std::uint8_t fixed, std::int16_t wave_kind) {
    if ((defenders == 0 && fixed == 0) || wave_kind == 6) return 0x23;
    return defenders < 2 ? 0x26 : 0x66;
}

// seg13:0x4eec(ship, v): ship word +0x82 [BYTES seg13:0x4ef7-0x4f41].
void bump_w82(ShipRecord& s, std::int16_t v);

// seg13:0x3008(pool·256 + rec, ship): the rivals of a capture [BYTES seg13:0x3008-0x32ff].
void rivals(CaptureCtx& c, std::int16_t pool_rec, int ship);

// seg7:0x5a46: DGROUP:0x91fc (World::icons_b) and the directory record's taken bits
// [BYTES seg7:0x5a46-0x5ae7].
void count_icons(World& w);
// seg7:0x5ae8(rec, captured): the counters [BYTES seg7:0x5ae8-0x5b67]; `ship_pool` is
// DGROUP:0x9324.
void icon_counted(World& w, int rec, int captured, std::int16_t ship_pool);

// seg6:0xdaa's switch on `Detail` (DGROUP:0x9208) for DGROUP:0x8ffe: 0 -> 3, 1 -> 2, 2 and 3 -> 1;
// any other value writes nothing, so `current` stays (verified claim 26) [BYTES seg6:0xe36
// seg6:0xe41 seg6:0xe4f seg6:0xe5d seg6:0xe6b-0xe78].
constexpr std::int16_t dissolve_step_for(std::int16_t detail, std::int16_t current) {
    switch (detail) {
    case 0: return 3;
    case 1: return 2;
    case 2:
    case 3: return 1;
    default: return current;
    }
}

}  // namespace is::world
