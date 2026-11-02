// Collisions (docs/re/core.md "Collisions"): the pair finder seg8:0x50aa, the pair test
// seg8:0x6afa, the response seg8:0x546c with its tails seg8:0x698a / seg8:0x6a44 and the bounce
// (bounce.h), the hit dispatcher seg8:0x5df2 with the kill scorer seg8:0x7be8 and seg8:0x99ce,
// and screen to world seg8:0x2bbe. The handlers the dispatcher calls through a record's
// on_damage (+0x5e) are in damage.h.
//
// Sources: the listings (isre asm) of every routine named, read with work/analysis/m4/s1-hit.md
// and its verification s1-hit-verify.md (claims 22, 25 and 29 as the verifier corrected them;
// claims 37 and 38 unchecked and tagged [OPEN] at their sites) and s1-bounce.md. The pair
// orientation word follows ADR 0015 decision 1. All arithmetic keeps the original's widths
// (ADR 0013 decision 7). The routines and tables other parts of the port own are reached
// through CollideHost, whose defaults do nothing; each names its address.
//
// Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "shell/rng.h"
#include "world/flight.h"
#include "world/view.h"
#include "world/visibility.h"
#include "world/world.h"

namespace is::world {

// The ship pool's handlers that seg14's ship set-up writes into the record's far-pointer slots
// [BYTES seg14:0x2158 seg14:0x212a] (structs.md "Callbacks of the ship pool").
constexpr std::uint32_t kDamageShip = handler(8, 0x45e0);    // on_damage seg8:0x45e0
constexpr std::uint32_t kDestroyShip = handler(16, 0x8e4);   // on_destroy seg16:0x8e4

// The design-record words the collision and damage routines read, DGROUP:0x96d2 + 0xad·n +
// offset (structs.md "Design records"; n = ship word +0x4e).
struct DamageDesign {
    std::array<std::int16_t, 4> shot_div{};  // +0x08 ... +0x0e (DGROUP:0x96da): a shot's divisors [OPEN: their writer]
    std::array<std::int16_t, 4> hit_div{};   // +0x10 ... +0x16 (DGROUP:0x96e2): SHP +2 / 4, / 5, / 4, / 5 [BYTES seg14:0x1ce2-0x1d0e]
    std::array<std::int16_t, 4> lamp_row{{0, 1, 2, -1}};  // +0x22 ... +0x28 (DGROUP:0x96f4) [BYTES seg14:0x1d3b-0x1dd6]
    std::int16_t w2a = 0;        // +0x2a (DGROUP:0x96fc), the wings' +0x20 base [OPEN: its writer]
    std::int16_t hit_sound = 0;  // +0x2e (DGROUP:0x9700): SHP +0x1f [BYTES seg14:0x1e18 seg14:0x1e2d]
    std::int16_t team = 0;       // +0x38 (DGROUP:0x970a) [OPEN: its writer]
    std::int16_t captures = 0;   // +0x42 (DGROUP:0x9714): SHP +9 bit 0x10 [BYTES seg14:0x1eb4]
    std::int16_t repair = 0;     // +0x4c (DGROUP:0x971e): SHP +9 bit 0x04 [BYTES seg14:0x1ffd]
};
// The design words the World keeps (World::design_ratings, rating.h) as DamageDesign; the words
// it does not keep are 0 [DESIGN].
DamageDesign design_from_world(const World& w, int n);

// A pool-5 object's entry DGROUP:0x5a50 + 0x14·rec: its shooter and its last-hit object
// [BYTES seg8:0x6015 seg8:0x549d seg8:0x5519] (structs.md). The port keeps 32 [OPEN: the
// table's size and its owner, the pool-5 mover seg7:0x40c4].
struct Pool5Entry {
    std::int16_t shooter_pool = 0, shooter_rec = 0;  // DGROUP:0x5a50, 0x5a52
    std::int16_t hit_pool = 0, hit_rec = 0;          // DGROUP:0x5a54, 0x5a56
};
// A missile's entry DGROUP:0x66e4 + 0x1c·rec (pool 6) [BYTES seg8:0x602d seg8:0x6c71].
struct MissileEntry {
    std::int16_t weapon = 0;                          // DGROUP:0x66e4
    std::int16_t launcher_pool = -1, launcher_rec = 0;  // DGROUP:0x66e8, 0x66ea
    std::int16_t age = 0;                             // DGROUP:0x66ec
};
// A defender entry DGROUP:0x52fe + 0x24·k, k < DGROUP:0x940a (structs.md "Other wave tables").
struct DefenderEntry {
    std::int16_t w00 = 0;                       // +0x00 DGROUP:0x52fe
    std::int16_t owner_pool = -1, owner_rec = 0;  // +0x06, +0x08 DGROUP:0x5304, 0x5306
    std::int16_t killer_pool = 0, killer_rec = 0;  // +0x0a, +0x0c DGROUP:0x5308, 0x530a
};
// A shot slot of the 0x94-byte table DGROUP:0x6098 as seg8:0x45e0 reads it [BYTES seg8:0x47a7
// seg8:0x47ae seg8:0x47b5 seg8:0x47bc seg8:0x47cb seg8:0x47d4].
struct ShotSlotView {
    std::int16_t vx = 0, vy = 0;   // +0x70, +0x72 (DGROUP:0x6108, 0x610a)
    std::int16_t x = 0, y = 0;     // +0x30, +0x48 (DGROUP:0x60c8, 0x60e0)
    std::int16_t owner_pool = -1, owner_rec = 0;  // +0x7a, +0x7c (DGROUP:0x6112, 0x6114)
};

// The DGROUP words of the collision path. They are globals in the original and persist between
// collisions (s1-hit.md 2: the pool-5 path deals the damage words a previous bounce left).
struct CollideGlobals {
    // ADR 0015 decision 1: the pair orientation, the word that stands for seg8:0x6afa's [bp-0xc];
    // 0 at the wave start (seg9:0x60bc) [DESIGN ADR 0015].
    std::int16_t orientation = 0;
    std::int16_t a_pool = 0, b_pool = 0;  // DGROUP:0x944a, 0x944c
    std::int16_t a_rec = 0, b_rec = 0;    // DGROUP:0x944e, 0x9450
    std::int16_t eA = 0, eB = 0;          // DGROUP:0x9442, 0x9444
    std::int16_t mA = 0, mB = 0;          // DGROUP:0x9446, 0x9448
    std::int16_t Bvx = 0, Bvy = 0;        // DGROUP:0x52dc, 0x52de
    std::int16_t Avx = 0, Avy = 0;        // DGROUP:0x52e0, 0x52e2
    std::int16_t Ax = 0, Ay = 0;          // DGROUP:0x52e4, 0x52e6
    std::int16_t Bx = 0, By = 0;          // DGROUP:0x52e8, 0x52ea
    std::int16_t dB = 0, dA = 0;          // DGROUP:0x9436, 0x9438
    std::int16_t hit_pool = 0, hit_rec = 0;  // DGROUP:0x943e, 0x9440: the player's last target
    std::uint32_t hit_time = 0;           // dword DGROUP:0x52d8
    // DGROUP:0xb0a (0 in the image): non-zero makes seg8:0x45e0 play sound 7 in place of the
    // design's hit sound; no writer but the clear at seg8:0x4d43 [BYTES DGROUP:0xb0a seg8:0x4d43].
    std::int16_t w0b0a = 0;
    // DGROUP:0xb0c (1 in the image), written by seg8:0x5b88-0x5bb8 [BYTES DGROUP:0xb0c].
    std::int16_t w0b0c = 1;
    // DGROUP:0x104c: the player takes no damage while it is set (the HUD's score_alt_hi) [OPEN:
    // its writer]; DGROUP:0x95a6 (seg8:0x2cd8's design recompute runs while 0) [OPEN];
    // DGROUP:0x8e30, a Demon's Gate deadline [OPEN]; DGROUP:0x9530, seg18's state [OPEN].
    std::int16_t w104c = 0;
    std::int16_t w95a6 = 0;
    // The seg18 mode's lock-out scale (s1-hit-verify.md 22, CORRECTED) reads DGROUP:0x90f4, the
    // game state (globals.md), and the byte DGROUP:0xa198. The collisions run in state 1's frame,
    // so the port holds 1 for the state [DESIGN]; DGROUP:0xa198 is 0 in the image [BYTES
    // DGROUP:0xa198] and its writer is the seg18 mode's [OPEN M6].
    std::int16_t state_90f4 = 1;
    std::uint8_t ba198 = 0;
    std::uint32_t w8e30 = 0;
    std::int16_t w9530 = 0;
    Rect16 square;  // DGROUP:0x5160, the caller's square
    Rect16 box;     // DGROUP:0x5148, the search box
    Rect16 other;   // DGROUP:0x5150, the candidate's square
    Rect16 meet;    // DGROUP:0x5158, IntersectRect's output
    std::array<Pool5Entry, 32> pool5{};
    std::array<MissileEntry, 32> missiles{};
    std::vector<DefenderEntry> defenders;  // DGROUP:0x940a of them
    std::array<std::int16_t, kLinkSlots> link_65da{};  // DGROUP:0x65da + 0x20·s [OPEN: LinkSlot's word +0x0e]
    std::array<std::int16_t, 32> pool11_856a{};        // DGROUP:0x856a + 0x34·rec [OPEN]
};
// ADR 0015 decision 1: the wave start's reset of the pair orientation [DESIGN ADR 0015].
inline void reset_collide_wave(CollideGlobals& c) { c.orientation = 0; }

// The calls out of the collision and damage routines.
class CollideHost {
public:
    virtual ~CollideHost() = default;
    // seg10:0x754a(id, pool, rec, mode).
    virtual void sound(int id, int pool, int rec, int mode) { (void)id, (void)pool, (void)rec, (void)mode; }
    // seg10:0x7944(id, pool, rec, mode, far design + 0x93): a design's sound; its body is [OPEN]
    // (sound.md); the default plays a positive id through sound() [OPEN formats.md SHP +0x1f].
    virtual void design_sound(int id, int pool, int rec, int mode) {
        if (id > 0 && id != 0xffff) sound(id, pool, rec, mode);
    }
    // seg3:0x6ec(ship, pool, rec, code): a message (the HUD's message display).
    virtual void message(int ship, int pool, int rec, int code) { (void)ship, (void)pool, (void)rec, (void)code; }
    // The computer pilots' routines of M5 [OPEN].
    virtual void seg13_b6f2(int ship, int code) { (void)ship, (void)code; }
    virtual void seg13_2714(int a, int b, int c, int d, int e, int f) { (void)a, (void)b, (void)c, (void)d, (void)e, (void)f; }
    virtual std::int16_t seg13_2688(int a, int b) { return (void)a, (void)b, 0; }
    virtual void seg13_2e60(int a, int b, int c) { (void)a, (void)b, (void)c; }
    virtual void seg4_5e8e(int a, int b, int c, int d) { (void)a, (void)b, (void)c, (void)d; }
    virtual void seg4_7d36(int a, int b, int c) { (void)a, (void)b, (void)c; }
    virtual void seg4_7636(int rec, int cp, int cr, int kill) { (void)rec, (void)cp, (void)cr, (void)kill; }
    virtual void seg8_9028(int k, int code, int pool, int rec) { (void)k, (void)code, (void)pool, (void)rec; }
    virtual void seg8_81f8(int pool, int rec) { (void)pool, (void)rec; }
    virtual void seg8_7ce0(int pool, int rec, int a, int res) { (void)pool, (void)rec, (void)a, (void)res; }
    virtual void seg11_947a(int pool, int rec, int arg) { (void)pool, (void)rec, (void)arg; }
    virtual void seg12_3498(int tpool, int trec, int xrec) { (void)tpool, (void)trec, (void)xrec; }
    virtual void seg18_633c() {}
    // seg7:0x2080(pool, rec): the in-use counts (structs.md "In-use counts").
    virtual void seg7_2080(int pool, int rec) { (void)pool, (void)rec; }
    // seg10:0x8b1c and the BitBlt of seg8:0x6725-0x674a, the race marker (M6) [OPEN].
    virtual void race_marker(int rec) { (void)rec; }
    // seg12:0x274a(pool, rec), the generic destroy (the effects' owner).
    virtual void destroy(int pool, int rec) { (void)pool, (void)rec; }
    // seg16:0x8e4(pool, ship), the ship's on_destroy (the death's owner).
    virtual void ship_destroyed(int pool, int ship) { (void)pool, (void)ship; }
    // PatBlt(DGROUP:0x13dc, x, y, w, h, BLACKNESS) of seg7:0x51f9.
    virtual void erase(int x, int y, int w, int h) { (void)x, (void)y, (void)w, (void)h; }
    // seg8:0x2cd8's design recompute seg14:0x3eda / seg14:0x4074 (DGROUP:0x95a6 = 0) [OPEN].
    virtual void wing_design(int ship) { (void)ship; }
    // seg8:0x2cd8's taunts seg3:0x6ec(j, 14, ship, 0x19 ... 0x1e) (M5) [OPEN].
    virtual void wing_taunts(int ship, int side) { (void)ship, (void)side; }
    // seg8:0x2cd8's debris: the torn wing into a free entry of the table DGROUP:0x9176 (the
    // debris' owner); true when it found one, and seg8:0x2cd8 then plays sound 0x6a
    // [BYTES seg8:0x33c2 seg8:0x33cd].
    virtual bool wing_debris(int ship, int side) { return (void)ship, (void)side, false; }
    // seg13:0x6716(ship), the flame off after the thruster fails (flight.h ship_touched).
    virtual void flame_off(int ship) { (void)ship; }
    // An on_damage the port does not run here (seg7:0x233c, seg7:0x2652, seg16:0x85e, ...):
    // its result [OPEN].
    virtual std::int16_t other_damage(std::uint32_t handler_id, int pool, int rec, std::int16_t a) {
        return (void)handler_id, (void)pool, (void)rec, (void)a, 0;
    }
    // Shot slot k of DGROUP:0x6098 (the weapons' owner, M4 stage 2).
    virtual ShotSlotView shot(int k) { return (void)k, ShotSlotView{}; }
    // W(w)+0x24, the weapon kind DGROUP:0x2f78 + 0x50·w.
    virtual std::int16_t weapon_kind(int w) { return (void)w, 0; }
    // The design words of design n; the default reads the World's.
    virtual DamageDesign design(const World& w, int n) { return design_from_world(w, n); }
};

struct FrameGlobals;

// What the collision routines touch.
struct CollideCtx {
    World& w;
    WaveGlobals& g;        // now, the roles, the player, the modes (synced from World)
    CollideGlobals& c;
    CollideHost& h;
    const ViewState& view;
    const VisibilityGlobals* vis;  // DGROUP:0x507c / 0x50e0, null: an empty list
    shell::Rng& rng;
    std::int16_t& dg_943a;  // DGROUP:0x943a, the colliding mover's pool
    std::int16_t& dg_943c;  // DGROUP:0x943c, its record
    std::int16_t& dg_9454;  // DGROUP:0x9454, the moving rock's heat
};

// seg8:0x50aa(pool·256 + rec): the first object the caller collides with, pool·256 + rec, or
// 0xffff [BYTES seg8:0x50aa-0x5467].
std::uint16_t find_pair(CollideCtx& x, std::int16_t pr);
// seg8:0x6afa(C, K): 1 for a hit, with both `collided` words set and the parties stored
// [BYTES seg8:0x6afa-0x6f37].
int pair_test(CollideCtx& x, std::int16_t c, std::int16_t k);
// seg8:0x546c(mass, extent + 2, &vy, &vx, hit): the caller's damage, or -1 [BYTES
// seg8:0x546c-0x5aab].
std::int16_t respond(CollideCtx& x, std::int16_t mass, std::int16_t ext2, std::int16_t* vy,
                     std::int16_t* vx, std::uint16_t hit);
// seg8:0x698a(flag) (party A) and seg8:0x6a44(flag) (party B) [BYTES seg8:0x698a-0x6a43
// seg8:0x6a44-0x6af9].
void tail_a(CollideCtx& x, int flag);
void tail_b(CollideCtx& x, int flag);
// seg8:0x5df2(T pool, T rec, X pool, X rec, a): the hit dispatcher; on_damage's result
// [BYTES seg8:0x5df2-0x6989].
std::int16_t dispatch(CollideCtx& x, int tp, int tr, int xp, int xr, std::int16_t a);
// The far call through T's on_damage (+0x5e) of seg8:0x647c [BYTES seg8:0x647c].
std::int16_t call_on_damage(CollideCtx& x, int pool, int rec, std::int16_t a);
// seg8:0x7be8(pool, rec, a, res): the kill scorer [BYTES seg8:0x7be8-0x7cdf].
void kill_score(CollideCtx& x, int pool, int rec, std::int16_t a, std::int16_t res);
// seg8:0x99ce(T pool, T rec, credit pool, credit rec, res) [BYTES seg8:0x99ce-0x9ab6].
void seg8_99ce(CollideCtx& x, int tp, int tr, int cp, int cr, std::int16_t res);
// seg8:0x2bbe(x, y): a screen point to the world, the DGROUP:0x9130 = 0 path
// [BYTES seg8:0x2bcd-0x2c7b]; the seg18 path (seg8:0x2c7d) is [OPEN] and returns (x, y).
ScreenPoint screen_to_world(const ViewState& v, std::int16_t x, std::int16_t y, std::int16_t dg_9130);

// The two persistent kill-scorer tables (globals.md "Kill scorer") [BYTES DGROUP:0xb0e DGROUP:0xb38].
std::int16_t kill_points(int pool);
std::int16_t kill_weight(int pool);

// Record access with the port's guard: an index outside the storage reads and writes a scratch
// record, where the original reads past its block [DESIGN].
ObjectRecord& obj(World& w, int pool, int rec);
ShipRecord& ship_at(World& w, int i);
std::uint32_t ship_dword(const ShipRecord& s, int off);
void set_ship_dword(ShipRecord& s, int off, std::uint32_t v);

}  // namespace is::world
