// The launched weapons (docs/re/core.md "Two fire paths", "The weapon system", "Launch",
// "Pool-6 mover"): the weapon system seg11:0xac86 that Z and Enter reach, the launcher
// seg11:0x7c12, the in-use recount seg7:0x2080, the pool-6 mover seg12:0x0 with the retarget
// seg12:0x586, the steer seg12:0x7c0, the turn seg13:0xd6c and the clamp seg8:0x1cbe, and the
// generic on_damage seg7:0x2652. M4 stage 2, track MISSILE: weapon 8, RADAR-GUIDED MISSILES.
//
// Sources: work/analysis/m4/s2-missile.md and its verification s2-missile-verify.md (77
// confirmed; claim 7 CORRECTED: DGROUP:0x9432 += 3 only when W(w)+0x46 = 0 and ship word +0x32
// != 9; claim 45 CORRECTED: the mover draws only when W(id)-2 != -1, else writes +0x00/+0x02
// from seg8:0x29ea), s2-display-verify.md claim 22 as corrected (the launcher's slot search),
// and the listings of every routine named. Where core.md differs, the analyses are followed.
// All arithmetic keeps the original's 16-bit widths (ADR 0013 decision 7): every `cdq; idiv`
// of the listings is a 16-bit `cwd; idiv`.
//
// The missile entries DGROUP:0x66e0 are Pools::missiles (records.h), the weapon words
// DGROUP:0x2f54 are MissileState::words. Calls into routines other parts of the port own go
// through MissileHost, whose defaults do nothing; each names its address. M4 stage 7, track
// LAUNCHED (work/analysis/m4/s7-launched.md with s7-launched-verify.md: claims 2, 7, 13, 21, 28
// and 43 as corrected, Omissions 1-13) ports the other launched weapons (6, 9-12, 14, 16-22):
// the rear launch, the kind-5 aim, the dodge, the fire from a missile, the ends, and names the
// weapon system's other routines as MissileHost virtuals whose defaults log their address
// through MissileHost::unported. What stays unported (the Noble weapons' particles and shock,
// M6; seg13:0x2688's relation, M5) reaches `unported` or a default with its address.
//
// Portable, no Win32.
#pragma once

#include <cstdint>
#include <functional>

#include "shell/rng.h"
#include "world/collide.h"
#include "world/flight.h"
#include "world/records.h"
#include "world/sprite.h"
#include "world/update.h"
#include "world/visibility.h"
#include "world/world.h"

namespace is::world {

// ---- the weapon words seg11:0xac86, seg11:0x7c12 and seg12:0x0 read (s2-missile.md 0.2;
// docs/re/structs.md "Weapon table"). W(w)+off is WeaponWords::word(w, off); the offsets are wofs::k*.
namespace wofs {
constexpr int kName = 0x00;       // the ENG_LANG name string
constexpr int kLaunch = 0x04;     // 1 front, 2 rear
constexpr int kAnim = 0x06;       // 1 spin, 2 the steer turns `frame`, 4 frames by age, 5 frame 0
constexpr int kMaxDamage = 0x08;
constexpr int kKind = 0x0a;       // 0 not launched; 2 the ship's target; 3 anything; 5 aimed
constexpr int kThrust = 0x0c;
constexpr int kClamp = 0x0e;      // the per-axis speed clamp
constexpr int kMass = 0x10;
constexpr int kSpin = 0x12;       // the mode-1 period
constexpr int kW14 = 0x14;        // seg12:0x64aa when != 0; 1 skips pool DGROUP:0x9328 in the retarget;
                                  // seg12:0x64aa's action: 1 FIREBALL, 2 SNOWBALL, 3 lightning, 4 a gun shot [BYTES seg12:0x668b]
constexpr int kFireRule = 0x16;   // seg12:0x64aa's when: 1 always, 2 one in three, 3 facing a ship, 4 facing the target [BYTES seg12:0x64d8]
constexpr int kSound = 0x18;      // the launch sound
constexpr int kRecolourA = 0x1a, kRecolourB = 0x1c;
constexpr int kLifetime = 0x1e;
constexpr int kFuse = 0x20;       // the proximity fuse in cells
constexpr int kBlast = 0x22;      // blast_size
constexpr int kEnd = 0x24;        // 1 explode, 4 / 6 seg7:0x2e02, 5 seg12:0x6a60, 6 boomerang
constexpr int kFrames = 0x26;     // the mode-4 frame count
constexpr int kFuel = 0x2e;       // 0: the slot count is the ammunition, else the fuel it costs
constexpr int kW38 = 0x38;        // seg4:0x5514 every 4 passes when != 0
constexpr int kOffscreen = 0x3a;  // 0: an off-screen missile expires after 20 passes
constexpr int kRetarget = 0x3c;   // != 0: retarget every 13 passes
constexpr int kCloak = 0x3e;      // 0: a cloaked ship target jitters
constexpr int kDrag = 0x40;
constexpr int kAggression = 0x46; // 0: DGROUP:0x9432 += 3 per use
constexpr int kCell = -8;         // the cell side (raw)
constexpr int kGap = -6;          // the raw gap, read by seg10:0x533a
constexpr int kSlot = -4;         // the image slot, written by seg10:0x533a
constexpr int kGraphic = -2;      // the sheet's IGRAPHIC id, -1 none
}  // namespace wofs

constexpr int kPoolMissiles = 6;            // DGROUP:0x1e10, counts DGROUP:0x92f8 / DGROUP:0x9412
constexpr int kWeaponRadarMissiles = 8;     // RADAR-GUIDED MISSILES [BYTES ENG_LANG.DLL STRING 13] id 194
constexpr int kMissileCap = 10;             // the launcher runs while DGROUP:0x9412 < 10 [BYTES seg11:0x7c2b]
constexpr int kRetargetBest = 20000;        // seg12:0x586's first best [BYTES seg12:0x5a0]

// The wave's DGROUP words of this track that the World does not hold. DGROUP:0x9432 is
// World::w9432, DGROUP:0x9412 / 0x92f8 Pools' in-use / allocated counts of pool 6, DGROUP:0x9498
// Pools::bad_index_count.
struct MissileState {
    WeaponWords words;           // DGROUP:0x2f54 with the run-time image slots W(w)-4
    // DGROUP:0x8e28 = now at every use is World::w8e28 [BYTES seg11:0xacfa].
};
// The program's one MissileState (DGROUP lives once). The program fills `words` from
// hud::WeaponTable at the load (block(w) = table.block(w)) [DESIGN].
MissileState& missile_state();

// The calls out of the routines.
class MissileHost {
public:
    virtual ~MissileHost() = default;
    // seg10:0x754a(id, pool, rec, mode).
    virtual void sound(int id, int pool, int rec, int mode) { (void)id, (void)pool, (void)rec, (void)mode; }
    // seg10:0x744c(0x69): seg10:0x754a(0x69, 0, 0, 0) alone, the effect-priority reset before
    // the player's launch sound (s2-missile.md 3.2) [BYTES seg11:0x89e4].
    virtual void sound_reset() { sound(0x69, 0, 0, 0); }
    // seg10:0x7944(id, a, b, mode, far DGROUP:0x1118 ""): for a pool-6 source it plays as
    // seg10:0x754a does when id < 0x6a (no sequence switch is reached, s2-missile.md 3.3, claims
    // 33-36) and the 0x69 pre-call needs (a, b) = (ships, player) [DESIGN: through sound()].
    virtual void sound_7944(int id, int pool, int rec, int mode) { sound(id, pool, rec, mode); }
    // seg13:0x4eec(ship, v), the capture track's routine on ship word +0x82 [BYTES seg11:0xacea].
    virtual void capture_count(int ship, int v) { (void)ship, (void)v; }
    // PostMessage(DGROUP:0xa7e, WM_PAINT, 0, 0L): the weapon display repaints (DISPLAY's
    // hud::Info::invalidate(kInfoWeapon)), on every launch [BYTES seg11:0x7e9a-0x7ea6].
    virtual void paint_weapons() {}
    // seg3:0xf4(ship, dword ship +0x32), the target choice (DISPLAY's), kind-2 weapons only
    // [BYTES seg11:0x7f0e-0x7f2f].
    virtual void choose_target(int ship, std::uint32_t arg) { (void)ship, (void)arg; }
    // seg12:0x274a(pool, rec), the generic destroy (effects.h object_destroyed).
    virtual void destroy(int pool, int rec) { (void)pool, (void)rec; }
    // The far call through a record's `on_destroy` (+0x66) from seg7:0x2652 [BYTES seg7:0x26b1].
    virtual void on_destroy(int pool, int rec) { (void)pool, (void)rec; }
    // seg8:0x29ea(x, y), a world point to the screen (the W(id)-2 = -1 branch of the mover).
    virtual Point16 to_screen(std::int16_t x, std::int16_t y) { return Point16{x, y}; }
    // seg13:0x16ac(ship, pool·256 + rec), the touch (the capture track's).
    virtual void touch(int ship, std::int16_t pr) { (void)ship, (void)pr; }
    // seg13:0x6716(ship) before a real ship's one-step turn (flight.h ship_touched).
    virtual void ship_touched(int ship) { (void)ship; }
    // The design word +0x38 (DGROUP:0x970a + 0xad·n), the team seg12:0x586 compares; the
    // default reads collide.h design_from_world [OPEN: the word's writer].
    virtual std::int16_t team(const World& w, int design) { return design_from_world(w, design).team; }
    // A branch of a weapon this stage does not port, by its address (M4-later or M6).
    virtual void unported(const char* address, int a, int b) { (void)address, (void)a, (void)b; }

    // ---- M4 s7 LAUNCHED: the weapon system's switch [BYTES seg11:0xad38-0xadb5]. Each default
    // logs once through `unported` with the routine's address; the other tracks' ports are bound
    // by the program (flight_hooks.cpp bind_missile_links).
    // seg11:0x8a42(pool, rec), RING OF DEATH, cdecl; the switch passes (DGROUP:0x9324,
    // DGROUP:0x9646) [BYTES seg11:0xad5a-0xad66].
    virtual void ring_of_death(int pool, int rec) { unported("seg11:0x8a42", pool, rec); }
    // seg11:0x947a(pool, rec, w), cdecl: w 2 GRAPPLING HOOK, 0xf HANDCUFFS [BYTES seg11:0xad6b-0xad80].
    virtual void tether_use(int pool, int rec, int w) { (void)rec, unported("seg11:0x947a", pool, w); }
    // seg11:0x9fdc(pool, rec), LIGHTNING, cdecl: the switch's case 2 (DGROUP:0x9324,
    // DGROUP:0x9646) [BYTES seg11:0xad82-0xad8e] and BATTERIES' action seg12:0x66e8 (6, m).
    virtual void lightning(int pool, int rec) { unported("seg11:0x9fdc", pool, rec); }
    // seg13:0x1318(pool, rec, life, kind), Pascal: kind 4 SNOWBALLS, 5 FIREBALLS, life 0x1e
    // [BYTES seg11:0xad38-0xad58].
    virtual void fuel_ball(int pool, int rec, int life, int kind) {
        (void)rec, (void)life, unported("seg13:0x1318", pool, kind);
    }
    // seg11:0x9078(pool, rec), WILDCARD DEFENSE, cdecl [BYTES seg11:0xad9e-0xadaa].
    virtual void wildcard(int pool, int rec) { unported("seg11:0x9078", pool, rec); }
    // ---- M4 s7 LAUNCHED: the fire from a missile seg12:0x64aa [BYTES seg12:0x6681-0x66f7]
    // seg12:0x2db6(pool, rec, heading, life, kind), Pascal `retf 0xa`: a pool-5 ball from an
    // object (the FUEL track's spawn of seg12:0x2db6) [BYTES seg12:0x6691-0x66c1 seg12:0x3145].
    virtual void pool5_shot(int pool, int rec, int heading, int life, int kind) {
        (void)rec, (void)heading, (void)life, unported("seg12:0x2db6", pool, kind);
    }
    // seg6:0x84fc(a0, 0, 0, 0, 0, 0, 0, 0, a8, a9), the particle entry (M6) of ENLIGHTENMENT's
    // launch, cdecl [BYTES seg11:0x80aa-0x80da]: a0 = (r % 2 + 1)·10, a8 = W+0x1e - 1, a9 = m.
    // The default logs. Its port must draw seg6:0x8569's rand() only after it found a free
    // entry (verify claim 7 as corrected: three early exits come first) [OPEN M6].
    virtual void particle(int kind, int life, int m) { (void)life, unported("seg6:0x84fc", kind, m); }
    // seg13:0x2688(s, rec), the ships' relation the kind-5 aim compares with 150, cdecl with s
    // pushed last (M5; the default logs once through `unported` and returns 0, which never
    // passes) [BYTES seg11:0x82c9-0x82dd].
    virtual std::int16_t relation(int s, int rec) {
        unported("seg13:0x2688", s, rec);
        return 0;
    }
    // The visibility list DGROUP:0x507c / 0x50e0 with its count DGROUP:0x9146 (visibility.h), for
    // the kind-5 aim; the default is the program's last_visibility() (null: an empty list).
    virtual const VisibilityGlobals* visible() { return last_visibility(); }
    // The rear muzzle entry (x, y) = DGROUP:0x7ba0 / 0x7ba2 + 4·(64n + f), entry 16 + f of design
    // n (shots.h ShotState::mx / my); the default reads shot_state() [BYTES seg11:0x8856 seg11:0x88e3].
    virtual Point16 muzzle(int n, int k);
};

// What the routines touch.
struct MissileCtx {
    World& w;
    WaveGlobals& g;        // now, the frame counter DGROUP:0x1e68, the roles, the player, Size
    MissileState& st;
    MissileHost& h;
    shell::Rng& rng;
};

// seg11:0xac86, the weapon system [BYTES seg11:0xac86-0xade8] (claims 6-9, 7 as corrected): it
// returns for the player's ship byte +0xea or +0xe8 set or its object byte +0x2e set; then
// DGROUP:0x9432 += 3 when W(w)+0x46 = 0 and ship word +0x32 != 9, seg13:0x4eec(player, 4) and
// DGROUP:0x8e28 = now, whether or not anything is launched; W(w)+0x0a != 0 calls the launcher
// seg11:0x7c12(player, DGROUP:0x9324, w); the 23-entry switch's cases go to `unported`.
// The caller is the key path: WM_KEYUP 0x0d / 'Z' when `PlayMode` = 0 [BYTES seg11:0x6e11
// seg11:0x6f6e seg11:0x6f74], and WM_KEYDOWN 0x0d / 'Z' for weapon 13 (WILDCARD) [BYTES
// seg11:0x6f58-0x6f6e].
void weapon_system(MissileCtx& c);

// seg11:0x7c12(s, pool, w), the launcher [BYTES seg11:0x7c12-0x8a3d] (claims 10-32; display
// claim 22 as corrected): the record walk (grow pool 6 at the allocated count with the
// template; free a walked record with byte +0x51 = 0 whose `state` != 0, or `state` 0 with age
// 0, or age >= lifetime with W+0x24 != 6), the ammunition slot search from slot 1 for a ship
// launcher, the refusal while a missile of the same launcher has 0 < age < 15 (returns before
// seg7:0x2080), the spend, the weapon display's repaint, the entry and record fields, the
// front launch, the sounds; then seg7:0x2080(6, -1) and DGROUP:0x9498. The cap DGROUP:0x9412 >=
// 10 returns at once.
void launch(MissileCtx& c, int s, int pool, int w);

// seg7:0x2080(pool, rec) [BYTES seg7:0x2080-0x2127] (claim 37): for pools 1, 2, 4 ... 11 and 19,
// the in-use count becomes one more than the highest record index below the allocated count
// whose `state` is 0 or whose byte +0x51 is not 0, or 0; the record argument is never read.
void recount_in_use(Pools& p, int pool);

// seg8:0x1cbe(L, pool·256 + rec) [BYTES seg8:0x1ccc-0x1d46] (claim 65): vx and vy clamped
// separately to at most L, then at least -L, signed 16-bit.
void clamp_velocity(Pools& p, std::int16_t limit, std::int16_t pool_rec);

// seg13:0xd6c's arguments in the stack order [bp+6] ... [bp+0x18] (Pascal, `retf 0x14`).
struct TurnArgs {
    std::int16_t a0 = 0, a1 = -1, a2 = -1;
    std::int16_t ship = -1;   // [bp+0xc]: a ship, -1 (a heading to turn) or -100 (a direction)
    std::int16_t mode = 2;    // [bp+0xe]
    std::int16_t cur = 0;     // [bp+0x10]
    std::int16_t tx = 0, ty = 0, mx = 0, my = 0;
};
// seg13:0xd6c [BYTES seg13:0xd6c-0x1007] (claims 62-64): the points shifted right by 3, the
// world wrap of dx and dy outside a race wave, h = direction16 by the mode (2: (dy, dx); 3, 4, 5
// and 0x1e as the analysis), the current heading for h outside 0 ... 15, h for mode 0x1e or
// ship -100 or h = cur, else seg13:0x6716(ship) and one step the short way (a gap of exactly 8
// steps the "else" way). Mode 2 inside a race wave or with a0 != 0 (seg8:0x2658) is [OPEN M6]
// and returns cur. For ship -1 the original's seg13:0x6716(-1) writes now - 1 to DGROUP:0x708c,
// four bytes below the ship table; the port skips it [DESIGN ADR 0016 decision 3] (claim 64,
// precision note).
std::int16_t turn_toward(const TurnArgs& a, std::int16_t wave_kind, const ShipTable& ships,
                         const std::function<void(int ship)>& touched);

// seg12:0x586(R, m), the retarget [BYTES seg12:0x586-0x7bd] (claims 50-56): the nearest
// candidate of pools 0 ... DGROUP:0x9324 (not 2; not DGROUP:0x9328 when W+0x14 = 1) in play
// with `mass` > 30 (and byte +0x2f != 0 unless kind 3), at d = seg8:0x285a cells, d << 2 for
// the launcher, lo16(7 d) for a ship of the launcher's team when both are ships outside Demon's
// Gate, then lo16(2 d) / 3 for a ship (kinds other than 4), all 16-bit: a far own ship wraps
// negative and wins. Kept when d < R and d < the best (20000); adopted with the lock unless it
// is the launcher at age <= 60.
void retarget(MissileCtx& c, std::int16_t range, int m);

// seg12:0x7c0(m), the steer [BYTES seg12:0x7c0-0xa23] (claims 57-61): the aim from the
// missile's centre to the target's, two rand() draws (x then y) on every call, the cloak
// jitter, the turn (W+0x06 = 2 turns `frame`), the proximity fuse (blast_size doubled,
// seg12:0x274a and seg7:0x2080, then on), the thrust and the clamp seg8:0x1cbe. The fuse
// measures from the missile's centre to the locked target's corner + `size` / 2 (in world
// units), and the rock and icon movers run before seg12:0x0 in a frame, so a weapon-8 missile
// closing on its rock from the left or above fuses (0x45 alone) while one from the right or
// below touches first (0x06, 0x45, 0x08) (s5-weapons-verify.md claims 1-6, 12 as corrected;
// tests/test_world_missile_end.cpp) [ORACLE core-0002] [ORACLE core-0006].
void steer(MissileCtx& c, int m);

// seg12:0x0, the pool-6 mover, once per wave frame at seg7:0xcfb [BYTES seg12:0x0-0x57c]
// (claims 38-49; 45 as corrected): for every m below DGROUP:0x9412 that runs (age below the
// lifetime or W+0x24 = 6, a weapon, `state` 0) the animation, the retarget every 13 passes,
// the unlocked half-life (age = lifetime - 5), the off-screen expiry, the steer, the drag, the
// integrator seg8:0x1b54(1), the cell, the draw seg8:0xb76(0x600 | m, 3, DGROUP:0x39e8[W-4], 0)
// (through u.host.draw_sprite) or the screen point, the collision (seg8:0x50aa, seg8:0x546c
// with `extent`, seg8:0x5df2(6, m, ...)) on screen after age 3, the age and the end. `u` gives
// the integrator, the sprite routine and the collision words of the frame.
void move_pool6(MissileCtx& c, UpdateCtx& u);

// seg7:0x2652(pool, rec, a), the generic on_damage [BYTES seg7:0x2652-0x26b8] (claim 67): 0 for
// pool 15; 0 without adding when `damage` > `max_damage` already outside pool 9; else damage
// += a and, when it then exceeds max_damage, the far call through `on_destroy` and 1. For the
// collisions' CollideHost::other_damage when the handler is kDamageGeneric.
std::int16_t generic_on_damage(Pools& p, int pool, int rec, std::int16_t a, MissileHost& h);

// seg12:0x274a's pool-6 part [BYTES seg12:0x2aec-0x2afd] (claim 79): entry age = W(id)+0x1e,
// then entry weapon = -1 (effects.cpp object_destroyed calls it).
void missile_destroyed(Pools& p, int m, const WeaponWords& words);

// ---- M4 s7 LAUNCHED (work/analysis/m4/s7-launched.md with s7-launched-verify.md)

// seg8:0x348e(P, R, TP, TR, V), cdecl [BYTES seg8:0x348e-0x35cf] (verified claims 17, 18): record
// (P, R) gets the velocity that aims it at the centre of record (TP, TR) at speed V, plus the
// target's velocity. Both corners go through seg8:0x29ea (`to_screen`) and are shifted left by
// DGROUP:0x9150 (`shift`, 16-bit `shl`); only the target point gets + T.extent / 2 (toward
// zero); dx = T - obj, dy alike (16-bit); d = seg8:0x35d0(dx, dy) (>= 1); vx = T.vx +
// lo16((sext V · sext d) / sext dx) (32-bit `imul`, `cdq`, `idiv dword`: the divisor is dx, not
// d) and vy = T.vy + lo16(seg5:0x110(sext dy · sext V, sext d)), both stored at the end
// [BYTES seg8:0x3585-0x35c3]. The original has no test of dx and faults on dx = 0 (the kind-5
// self-aim reaches it when M.extent / 2 is 0 after the shift, the hook's aim when the target is
// straight above or below); the port skips the division and both stores, leaves the record's
// velocity unchanged, logs once through guard_todo and returns false [DESIGN ADR 0016 decision 4] (ruling
// R1). True when the velocity was stored.
using AimToScreen = std::function<Point16(std::int16_t x, std::int16_t y)>;
bool aim_at(World& w, const AimToScreen& to_screen, int shift, int P, int R, int TP, int TR,
            std::int16_t V);
// The same on the missile context: seg8:0x29ea is c.h.to_screen and DGROUP:0x9150 = 4 - Size
// (c.g.size) [BYTES seg9:0x2408].
bool aim_at(MissileCtx& c, int P, int R, int TP, int TR, std::int16_t V);

// seg13:0x1076(pool, rec), Pascal `retf 4` [BYTES seg13:0x1076-0x1315] (verified claims 22-25):
// "the most urgent object on a collision course" of object (pool, rec). The goal is the entry
// target (+0x00/+0x02) with goal mode 2 for pool 6, ship words +0x32/+0x34 with mode +0x38 for
// the ship pool (M5 caller seg4:0x4fc8); any other pool leaves both uninitialised in the
// original, where the port takes no goal [DESIGN ADR 0016 decision 1]. The scan runs i <
// DGROUP:0x9318 + DGROUP:0x9146: the visibility list, then pool-12 record 2(i - DGROUP:0x9146)
// + 1. Skips: the object itself, pools 2, 9 and 4, and with goal mode 2 the goal and pool 0.
// d = seg8:0x285a(self, cand) (cand - self, cells), 0 < d < 300; dv = self.v - cand.v (16-bit);
// q = (sext dx · sext dvx + sext dy · sext dvy) / sext d (32-bit) > 0; score = d -
// lo16(seg5:0x110(3q, 2)); kept below the best (from 20000) when < 60, or for pool 8 with rec <
// 2·DGROUP:0x9318 and score < 120; a kind-6 wave with byte DGROUP:0x9eb5 = 0 rejects pool 8.
// seg8:0x35d0(dvx, dvy) is computed and never read (no state, not called). Returns (pool << 8)
// | (rec & 0xff), or 0xffff. No rand().
std::uint16_t threat(MissileCtx& c, const VisibilityGlobals* vis, int pool, int rec);

// seg4:0x5514(m), Pascal `retf 2` [BYTES seg4:0x5514-0x5650] (verified claim 21 as corrected):
// the dodge of a weapon with W+0x38 != 0 (6 TARGETTING MISSILES, 9 DEATH HEADS), called by the
// mover every 4 passes. Mode 2 off screen or with no threat; else the threat goes to entry
// +0x10/+0x12 (stored before the launcher test), mode 2 when it is the launcher, else mode 4 for
// dx·dvy - dy·dvx > 0 (32-bit, target minus missile) and mode 3 otherwise.
void dodge(MissileCtx& c, const VisibilityGlobals* vis, int m);

// seg12:0x64aa(m, id), Pascal `retf 4` [BYTES seg12:0x64aa-0x6701] (verified claims 27-29, 28
// as corrected): the fire from a missile at ages 16, 20, ... (age & 3 = 0, age >= 13). W+0x16
// decides: 1 always, 2 rand() % 3 = 0 (the one rand()), 3 the on-screen missile's frame within
// one of the heading to an on-screen ship k <= DGROUP:0x9316 in state 0 (signed range, no wrap),
// 4 the same for the entry target (no table weapon). W+0x14 acts: 1 pool5_shot(6, m, frame, 0xb,
// 5), 2 (…, 4), 3 lightning(6, m) when signed byte +0x50 > 0x19, 4 shots.h object_shot(6, m)
// through `gun` (null: nothing).
void missile_fire(MissileCtx& c, int m, int id, const std::function<void(int pool, int rec)>& gun);

// ---- the program's links (the glue, src/world/flight_hooks.cpp, fills them; an empty slot does
// nothing)
struct MissileLinks {
    std::function<void(int id, int pool, int rec, int mode)> sound;  // seg10:0x754a
    std::function<void(int ship, int v)> capture_count;              // seg13:0x4eec
    std::function<void()> paint_weapons;                             // WM_PAINT to DGROUP:0xa7e
    std::function<void(int ship, std::uint32_t arg)> choose_target;  // seg3:0xf4
    std::function<void(int pool, int rec)> destroy;                  // seg12:0x274a
    std::function<void(int pool, int rec)> on_destroy;               // far call through +0x66
    std::function<Point16(std::int16_t x, std::int16_t y)> to_screen; // seg8:0x29ea
    std::function<void(int ship, std::int16_t pr)> touch;            // seg13:0x16ac
    std::function<void(int ship)> ship_touched;                      // seg13:0x6716
    std::function<void(const char* address, int a, int b)> unported; // a TODO log
    // M4 s7 LAUNCHED: the other tracks' routines (empty: MissileHost's logging default).
    std::function<void(int pool, int rec)> ring_of_death;             // seg11:0x8a42
    std::function<void(int pool, int rec, int w)> tether_use;        // seg11:0x947a
    std::function<void(int pool, int rec)> lightning;                 // seg11:0x9fdc
    std::function<void(int pool, int rec, int life, int kind)> fuel_ball;  // seg13:0x1318
    std::function<void(int pool, int rec)> wildcard;                  // seg11:0x9078
    std::function<void(int pool, int rec, int heading, int life, int kind)> pool5_shot;  // seg12:0x2db6
    std::function<void(int kind, int life, int m)> particle;          // seg6:0x84fc (M6)
    std::function<std::int16_t(int s, int rec)> relation;             // seg13:0x2688 (M5)
};
MissileLinks& missile_links();
// The MissileHost over missile_links().
MissileHost& linked_missile_host();
// seg11:0xac86 on the program's state: what the key path calls for WM_KEYUP 0x0d / 'Z' with
// `PlayMode` = 0 (DISPLAY's src/app routing; the caller tests PlayMode, seg11:0x6e11).
void fire_weapon_system(World& w, WaveGlobals& g, shell::Rng& rng);
// seg12:0x0 on the program's state, for UpdateHost::mover(Mover::k12_0) (CAPTURE's update).
void move_missiles(World& w, UpdateCtx& u);

}  // namespace is::world
