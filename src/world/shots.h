// The gun and its shots (docs/re/core.md "The gun", "Shot slots", "Shot update", "Shot hit
// test"): the nine 0x94-byte shot slots DGROUP:0x6098, the gun seg11:0x4e0a, the muzzle tables
// DGROUP:0x7b60 (seg10:0x1460, seg10:0x1620, seg10:0x1502), the trail routine seg8:0x764, the
// shot update seg8:0x3aec with its brushes (seg10:0x4dfe), the hit test seg8:0x3688 and the
// per-object test seg8:0x6f3c with its impact, damage and score tail, and the spark seg8:0x804a.
//
// Sources: the listings (isre asm) of every routine named, read with
// work/analysis/m4/s2-gun.md and its verification s2-gun-verify.md (59 confirmed, claim 12
// CORRECTED: sound id 4 only when ship bytes +0xe0 and +0xe1 are both non-zero; the precision
// notes on claims 10, 14, 23, 25, 28, 33, 38 and 60 followed). Claim 44's stack word [bp-0x1e]
// of seg8:0x6f3c is the last word seg8:0x29ea left at its depth (seam_word, M4 s5:
// work/analysis/m4/s5-weapons.md sections 7 and 9.5, verified claims 32-40 and 49).
// 16-bit arithmetic stays 16-bit (ADR 0013 decision 7): the listings print `cwd` as `cdq`, and
// every `idiv` there is a 16-bit signed divide.
//
// A slot is kept as raw words, the table as one flat array, and every index is the listing's
// own expression: the fill writes trail point 6 past its [2][6] arrays and seg8:0x764 writes
// screen point 6 of barrel 1 into barrel 0's words, and only a literal port gets the same words
// in every case (s2-gun.md 1.4). An index past the nine slots reads and writes a scratch word
// [DESIGN].
//
// Portable, no Win32: the PatBlt and SelectObject calls on DGROUP:0x13dc go through ShotHost.
#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "world/collide.h"
#include "world/effects.h"

namespace is::world {

// ---- the slot table DGROUP:0x6098

constexpr int kShotSlots = 9;            // DGROUP:0x594 (7) + DGROUP:0x598 (2) [BYTES seg11:0x4e19]
constexpr int kShotSlotBytes = 0x94;     // [BYTES seg11:0x500c]
constexpr int kShotSlotWords = kShotSlotBytes / 2;
constexpr std::uint16_t kDgShotSlots = 0x6098;

// Byte offsets of a slot's words (s2-gun.md 1.4).
namespace shot {
constexpr int kScrX = 0x00;      // + 12a + 2k: screen x of trail point k of barrel a
constexpr int kScrY = 0x18;      // + 12a + 2k: screen y
constexpr int kWorldX = 0x30;    // + 12a: world x of barrel a's head (+ 2k at the fill)
constexpr int kWorldY = 0x48;    // + 12a: world y
constexpr int kConvX = 0x68;     // the two-barrel convergence vector
constexpr int kConvY = 0x6a;
constexpr int kW6c = 0x6c;       // 0x1c from the fill's dword store; no reader [BYTES seg11:0x5572]
constexpr int kW6e = 0x6e;       // 0x3e; no reader
constexpr int kVx = 0x70;        // the velocity per sub-step
constexpr int kVy = 0x72;
constexpr int kLife = 0x74;      // lifetime in sub-steps
constexpr int kAge = 0x76;       // age, -1 = free
constexpr int kBarrels = 0x78;
constexpr int kOwnerPool = 0x7a;  // kept after the slot frees
constexpr int kOwnerRec = 0x7c;
constexpr int kHits = 0x7e;      // the player's shot's hits; no reader found
constexpr int kW80 = 0x80;
constexpr int kNoSkip = 0x82;    // != 0 disables the near-miss skip [BYTES seg8:0x6f87]
constexpr int kAlive = 0x84;     // + 2a: barrel a alive
constexpr int kRicoVx = 0x88;    // the race's ricochet velocity (seg11:0x5626 only)
constexpr int kRicoVy = 0x8a;
constexpr int kRicoAt = 0x90;    // dword: the ricochet sub-step
}  // namespace shot

// The DGROUP words of the gun and the slot loop.
struct ShotState {
    std::array<std::int16_t, kShotSlots * kShotSlotWords> words{};
    std::int16_t dg_594 = 3;          // DGROUP:0x594: 3 in the image, the gun sets 7 [BYTES seg11:0x4e19]
    std::int16_t dg_598 = 2;          // DGROUP:0x598: 2 in the image, no writer
    std::uint32_t dg_5cc = 0;         // DGROUP:0x5cc: the sub-step counter [BYTES seg7:0x965]
    std::int16_t dg_9136 = 0;         // DGROUP:0x9136: the last seg8:0x3aec result of the loop
    // DGROUP:0x8e28, the player's last fire time, is World::w8e28 [BYTES seg11:0x4ff1].
    // DGROUP:0x5168 ... 0x516e: the heads of barrels 0 and 1 seg8:0x3688 fills (screen, then
    // world); persistent, so a one-barrel slot's gate test reads barrel 1's stale entry
    // [BYTES seg8:0x3816 seg8:0x381a].
    std::array<std::int16_t, 4> heads{};
    // seg8:0x6f3c's [bp-0x1e], never written by seg8:0x6f3c (s2-gun-verify.md 42): the stack
    // word T - 44 below seg8:0x3688's body sp, which the head loop's last seg8:0x29ea call left
    // (seam_word); shot_hit_test sets it before its seg8:0x6f3c calls [BYTES seg8:0x3692
    // seg8:0x36c3 seg8:0x3722 seg8:0x6f40]. 1 until the first hit test
    // [DESIGN ADR 0016 decision 5].
    std::int16_t stack_1e = 1;
    // The muzzle tables DGROUP:0x7b60 + 4(64n + k), (x, y) per entry, n = 0 ... 7: entries
    // 0 ... 15 one barrel (seg10:0x1460), 16 ... 31 the rear launch (seg10:0x1620), 32 ... 63
    // two barrels (seg10:0x1502) [BYTES DGROUP:0x7b60].
    std::array<std::int16_t, kShipCount * 64 * 2> muzzle{};

    // Word at byte offset `off` of slot j: the flat word 0x4a·j + off / 2 [DESIGN: scratch
    // outside the table].
    std::int16_t& word(int j, int off);
    std::int16_t word(int j, int off) const;
    std::uint32_t dword(int j, int off) const;
    void set_dword(int j, int off, std::uint32_t v);
    std::int16_t& mx(int n, int k) { return muzzle_word(n, k, 0); }
    std::int16_t& my(int n, int k) { return muzzle_word(n, k, 1); }

private:
    std::int16_t& muzzle_word(int n, int k, int xy);
    std::int16_t scratch_ = 0;
};

// The program's one ShotState (DGROUP lives once).
ShotState& shot_state();
// The placement seg9:0x6624: +0x74 = 50 and +0x76 = -1 in all nine slots (world.md "Update
// loop" step 4) [BYTES seg9:0x6624]. The set-up's World::beams loop (setup.cpp) stands for it
// until the fixer routes it here.
void place_shot_slots(ShotState& s);
// The source CollideHost::shot(k) reads (collide.h shot_view); the default is shot_state().
void set_shot_source(const ShotState* s);

// ---- the design words the muzzle tables read (the port has no 0xad-byte design table)

// Design record n's words DGROUP:0x96d2 + 0xad·n + off for the gun: +0x00 and +0x02
// (seg14:0xc42's value s = trunc(side / 2), formats.md MASTER.BOD +0x19) [BYTES seg14:0xc50
// seg14:0xc54],
// +0x2a (the body's cell side; seg8:0x45e0 sets ship +0x20 = +0x2a - 14 at the second wing
// loss) and +0x68 ... +0x6e (the two barrels' offsets, seg14:0x11a8's design tail while
// DGROUP:0x95a6 != 0) (design_words.h, s5-weapons-verify.md claims 42, 45).
struct DesignWords {
    std::int16_t w00 = 0;  // DGROUP:0x96d2
    std::int16_t w02 = 0;  // DGROUP:0x96d4
    std::int16_t w2a = 0;  // DGROUP:0x96fc
    std::int16_t w68 = 0, w6a = 0, w6c = 0, w6e = 0;  // DGROUP:0x973a ... 0x9740
};
// The words the World holds: +0x00, +0x02, +0x2a and +0x68 ... +0x6e as design_words.h keeps them
// (the sheet build's design tail and seg14:0x0's stores write them, player_sheet.h
// placement_tail; 0 before any). With s = 16 (side 33) a one-barrel shot leaves 19 px from the
// cell's centre and the tail smoke and the rear launch 16 px behind it (M4 s5 fixer).
DesignWords design_words_from_world(const World& w, int n);
// seg10:0x1460(n): entries 0 ... 15 [BYTES seg10:0x146f-0x14f7].
void muzzle_1460(ShotState& s, int n, const DesignWords& d);
// seg10:0x1620(n): entries 16 ... 31, the rear launch (seg11:0x7c12's seg11:0x881d, seg7): r =
// design +0x02, a from 270, /127 and /-127 (verified, work/analysis/m4/s7-owed-verify.md S5-2)
// [BYTES seg10:0x162f-0x16ae].
void muzzle_1620(ShotState& s, int n, const DesignWords& d);
// seg10:0x1502(n): entries 32 ... 63, two barrels [BYTES seg10:0x1511-0x1613].
void muzzle_1502(ShotState& s, int n, const DesignWords& d);
// seg14:0x0's three calls for a ship of design n [BYTES seg14:0xcdc seg14:0xce6 seg14:0xcf0].
void fill_muzzles(ShotState& s, int n, const DesignWords& d);

// ---- the shot brushes DGROUP:0x3b4c (seg10:0x4dfe)

// The 20 COLORREFs (0x02bbggrr, PALETTERGB) of seg10:0x4dfe for `Bright` DGROUP:0x8ff2 = 0
// [BYTES seg10:0x4f52-0x5060] and != 0 [BYTES seg10:0x4e40-0x4f4f] (s2-gun.md 3.2, claims 29,
// 30). Brush n is DGROUP:0x3b4c + 2n; DGROUP:0x3b60 is n = 10.
extern const std::array<std::uint32_t, 20> kShotBrushes;
extern const std::array<std::uint32_t, 20> kShotBrushesBright;
// seg8:0x3aec's brush index for slot j: b + lo16(10 · signed ship byte +0xe3) for a ship's
// shot, else 10 + b, b = (age << 3) / lifetime, all 16-bit. +0xe3 is 0 or 1 (its only writer
// stores design +0x44 AND 1, flight.h FlightDesign::shot_row), so the index stays inside the 20
// brushes (core.md "Which row") [BYTES seg8:0x3c75-0x3caf seg14:0x21d4].
std::int16_t shot_brush_index(std::int16_t age, std::int16_t lifetime, bool ship_shot,
                              std::int8_t row_e3);

// ---- the calls out

class ShotHost {
public:
    virtual ~ShotHost() = default;
    // seg10:0x744c(id): with 0x69 only the reset seg10:0x754a(0x69, 0, 0, 0) (sound.md).
    virtual void sound_744c(int id) { (void)id; }
    // seg13:0x4eec(ship, 1) [OPEN: not read].
    virtual void seg13_4eec(int ship, int arg) { (void)ship, (void)arg; }
    // seg7:0x1208, the player's sprite (the gun calls it for the player outside action mode).
    virtual void player_sprite() {}
    // seg11:0x5ab6(mode, ship): the gun's brake or drift call for a computer ship (flight.h
    // thrust_step(c, 5, ship)).
    virtual void thrust_step(int mode, int ship) { (void)mode, (void)ship; }
    // seg11:0x5626(j): the race's ricochet; returns at once outside kind-6 waves (M6) [OPEN].
    virtual void race_5626(int j) { (void)j; }
    // seg18:0x49d8(j, n), seg18:0x4dc4(j), seg18:0x3f02(x, y, d): the seg18 mode's (M6) [OPEN].
    virtual void seg18_49d8(int j, int n) { (void)j, (void)n; }
    virtual void seg18_4dc4(int j) { (void)j; }
    virtual void seg18_3f02(std::int16_t x, std::int16_t y, std::int16_t d) { (void)x, (void)y, (void)d; }
    // SelectObject(DGROUP:0x13dc, DGROUP:0x3b4c[n]); the brush stays selected
    // [BYTES seg8:0x3cb3].
    virtual void select_brush(int n) { (void)n; }
    // PatBlt(DGROUP:0x13dc, x, y, w, h, rop): PATCOPY 0x00F00021 for a head, BLACKNESS 0x42 for
    // the erase [BYTES seg8:0x3d77 seg8:0x3dd4 seg8:0x3bd4 seg8:0x3f81].
    virtual void patblt(std::int16_t x, std::int16_t y, std::int16_t w, std::int16_t h, std::uint32_t rop) {
        (void)x, (void)y, (void)w, (void)h, (void)rop;
    }
    // The design word +0x32 (DGROUP:0x9704 + 0xad·n, the design mass); the default reads the
    // ship's pool record +0x44, which seg14:0x20bc sets from the same value: an equivalence
    // [BYTES seg14:0x1e68 seg14:0x20bc] (triage D-22).
    virtual std::int16_t design_mass(const World& w, int ship_pool, int ship) {
        return obj(const_cast<World&>(w), ship_pool, ship).mass;
    }
    // The signed roster byte +0x32 of roster record r (far DGROUP:0x6e24 + 0x7c·r); the port's
    // RosterRecord does not hold it [OPEN], the default 0.
    virtual std::int16_t roster_byte_32(int r) { return (void)r, 0; }
};

constexpr std::uint32_t kRopPatCopy = 0x00F00021;
constexpr std::uint32_t kRopBlackness = 0x00000042;

struct ShotCtx {
    CollideCtx& x;  // the World, the wave globals, the collision words and host, the view, the list
    ShotState& s;
    ShotHost& h;
    EffectState& fx;  // the effect table's side arrays DGROUP:0x5170 / 0x5224 (the spark)
};

// seg11:0x4e0a(s): the gun of ship s [BYTES seg11:0x4e0a-0x5622].
void fire(ShotCtx& c, int ship);
// seg8:0x764(k, j): screen point k of every barrel a < slot +0x78 from world point k
// [BYTES seg8:0x764-0x7c6].
void trail_point(ShotCtx& c, int k, int j);
// seg13:0x1fc2(pool, rec), Pascal `retf 4`: one gun shot from an object, DEATH HEADS' fire
// from seg12:0x64aa (work/analysis/m4/s7-launched.md 3.2; verified claims 30-33, Omission 8)
// [BYTES seg13:0x1fc2-0x2187]. With `Detail` DGROUP:0x9208 > 0 (signed) it returns unless some
// slot j < DGROUP:0x594, free or busy, has the player's owner words (DGROUP:0x9324,
// DGROUP:0x9646) or two slots are free. The first free slot j: +0x76 = +0x7e = +0x80 = 0; for a
// = 0, 1 trail point a of barrel 0 (+0x30 + 2a, +0x48 + 2a) = -100 and seg8:0x764(a, j), which
// still sees the slot's previous barrel count; head +0x30 = extent / 2 + ((hx[frame] << 3) +
// x), +0x48 alike (no wrap); owner (pool, rec); +0x78 = 1, +0x84 = 1, +0x82 = 0; velocity
// lo16(3·heading[frame]) without the owner's; +0x74 = 20 (in a kind-6 wave 0x24, then
// seg11:0x5626(j)), then seg5:0x110(+0x74 · max(W, H), 1000); seg10:0x754a(3, 0x1e, j, 2).
// Not written: +0x86, +0x88 / +0x8a, dword +0x90, +0x68 / +0x6a, the other trail points
// (verified claim 32). With no free slot, nothing.
void object_shot(ShotCtx& c, int pool, int rec);
// seg8:0x3aec(i, j): slot j's sub-step i; 0 when the slot is free and DGROUP:0x9136 is 0, else 1
// [BYTES seg8:0x3aec-0x3fba].
std::int16_t shot_update(ShotCtx& c, int i, int j);
// seg7:0x938-0x970: DGROUP:0x9136 = 0, then five sub-steps of every slot j < DGROUP:0x598 +
// DGROUP:0x594 (re-read each iteration), DGROUP:0x5cc + 1 after each sub-step [BYTES
// seg7:0x938-0x970]. The original runs it only when DGROUP:0x9144 >= DGROUP:0x8f76 and the
// pass's frame-skip flag [bp-0xa] is 0 (s2-gun-verify.md 33); the port's one pass per frame
// stands for both [DESIGN ADR 0001].
void run_shots(ShotCtx& c);
// seg8:0x3688(j): 0 none, 1 a hit the shot survives, 2 the shot ends [BYTES seg8:0x3688-0x3adf].
std::int16_t shot_hit_test(ShotCtx& c, int j);
// The word seg8:0x29ea(x, y) leaves at T - 44 (T = seg8:0x3688's body sp), which every
// seg8:0x6f3c call of that seg8:0x3688 reads as its [bp-0x1e] (verified claims 32-36, 49):
// on the DGROUP:0x9130 = 0 path the last write is CX pushed at seg8:0x2ade for seg9:0x88dc, the
// low word of off = sext32(y) - sext32(lo16(DGROUP:0x90ec << ((DGROUP:0x9150 - 1) & 31))) -
// dword DGROUP:0x2410 (claim 37; the byte shift count 0xff at DGROUP:0x9150 = 0 is 31 on a 386),
// unless the y wrap's far call to seg5:0x346 at seg8:0x2b4a runs and leaves its return IP 0x2b4f:
// |off| (32-bit, seg9:0x88dc) > sext32(lo16(DGROUP:0x5a2 << 2)), except in a kind-6 wave with
// byte DGROUP:0x9eb5 != 1 (claims 35, 38) [BYTES seg8:0x2a02-0x2b4f]. `y` is the head loop's last
// barrel's world y, slot word +0x48 + 12(+0x78 - 1) (claim 40). The word is 0, and the seam fix
// skipped, exactly when that y equals the player's y at the last recentre. The seg18 path
// (DGROUP:0x9130 != 0) leaves lo16(|dword DGROUP:0x88cc|) (claim 39); the port returns 1 there,
// so the fix runs, until seg18 mode is ported [OPEN M6] [DESIGN ADR 0016 decision 5, Alternatives:
// the held 1].
std::int16_t seam_word(const ViewState& v, std::int16_t y, std::int16_t wave_kind, std::uint8_t dg_9eb5,
                       std::int16_t dg_9130);
// seg8:0x6f3c(pool, rec, j, -): slot j against one object [BYTES seg8:0x6f3c-0x7ba3].
std::int16_t shot_hit_object(ShotCtx& c, int pool, int rec, int j);
// The power of slot j's shot at seg8:0x715d-0x724b (owner, age, lifetime, the design mass, the
// Demon's Gate x 3 and the roster scale).
std::int16_t shot_power(ShotCtx& c, int j);
// seg8:0x804a(j, v, a): the spark, effect type 7 [BYTES seg8:0x804a-0x81f6]; its drawing is
// effects.h piece_frame (seg8:0x3fbe's branch at seg8:0x4256) [BYTES seg8:0x4256-0x4538].
void spark(ShotCtx& c, int j, std::int16_t v, int a);

}  // namespace is::world
