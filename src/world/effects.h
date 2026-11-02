// The effect table's routines (docs/re/core.md "Death and the explosion", "Rocks"; work/analysis/
// m4/s1-death.md sections 1-3 with s1-death-verify.md): the frame routine seg8:0x3fbe that
// animates the explosions, the blast seg8:0x5ab0, a ship's death seg16:0x8e4 (its `on_destroy`),
// the generic destroy handler seg12:0x274a (the fragments), and the explosion images of
// seg10:0x5102. M4 stage 1, track DEATH.
//
// The routines work on the wave's World (world.h): its pools, ship records, the effect table
// DGROUP:0x1e90 and the words DGROUP:0x9430 (World::w9430), DGROUP:0x9434 (World::w9434),
// DGROUP:0x1e80, DGROUP:0x1e84, the rating DGROUP:0x5078 and DGROUP:0x668a (World::gate[0].w12).
// The words no other part of the port holds live in EffectState. Calls into routines other
// tracks own go through EffectHost, whose defaults do nothing; each names its address.
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>

#include "shell/rng.h"
#include "world/records.h"
#include "world/sprite.h"
#include "world/world.h"

namespace is::world {

// ---- the images (seg10:0x5102)

constexpr int kSlotExplosion = 10;  // DGROUP:0x39fc: IGRAPHIC.DLL #4000 170 [BYTES seg10:0x51de seg10:0x51e3]
constexpr int kSlotSnowball = 11;   // DGROUP:0x39fe: #4000 175 [BYTES seg10:0x513a seg10:0x513f]
constexpr int kGraphicExplosion = 0xaa;  // 170: 248 x 52, eight fireballs of side 10, 16, ... 52
constexpr int kGraphicSnowball = 0xaf;   // 175
constexpr int kGraphicFlames = 0xe6;     // 230: 160 x 32, five 32-px flame cells (seg13:0x4710, seg13:0x447a)
// DGROUP:0xb70: the source x of fireball f (the running sums of the sides 10, 16, 22, ...)
// [BYTES DGROUP:0xb70] (verified claim 8).
constexpr std::array<std::int16_t, 8> kExplosionX = {0, 10, 26, 48, 76, 110, 150, 196};
// The fireball side of frame f = +0x00 - 1 at Size 1: 6f + 10 [BYTES seg8:0x4057 seg8:0x405a].
constexpr int kExplosionFrames = 8;

// seg10:0x5102 [BYTES seg10:0x5110-0x51f6]: returns at once while both slots 10 and 11 hold a
// DC; otherwise slot 10 (when empty) takes seg10:0x6c02(170, 2 when `Enhance` DGROUP:0x8fe8 != 0
// else 0, 1) and slot 11 (when empty) seg10:0x6c02(175, 0, -1). Slot 11's seg9:0x27da(50) (the
// load's progress step while DGROUP:0x1e68 = 0) and the rescale seg10:0x6308(slot, -1, -1, -1)
// at Size != 1 are not ported [OPEN]. The wave's load calls it beside seg10:0x3a2e (shell.md
// "Wave set-up", the graphics chain), through build_wave_images.
void build_effect_images(Blitter& out, ImageSlotTable& images, std::int16_t enhance);

// seg8:0x7ba8(x, y) [BYTES seg8:0x7ba8]: PtInRect of (-50, -50, W + 50, H + 50) (verified claim 14).
bool in_play_area(std::int16_t x, std::int16_t y, std::int16_t W, std::int16_t H);

// The first free entry (+0x00 = 0xffff) of the table, -1 when none [BYTES seg16:0x9d8-0x9e7
// seg12:0x2a55-0x2a64].
int find_free_effect(const EffectTable& t);

// ---- the words of the effect routines that no other part holds

// The pod records DGROUP:0x8560 + 0x34·rec, one per pool-11 record (src/world/death.h fill_pod)
// [BYTES seg13:0x4bb3-0x4cbe].
struct PodRecord {
    std::int16_t kind = 0;       // DGROUP:0x8560: 1 player or outside Demon's Gate, 2 Demon's Gate, 3 flag 0
    std::int16_t resources = 0;  // DGROUP:0x8562: the ship's `resources` +0xce
    Dword age;                   // DGROUP:0x8564: 0x01f40000 at the fill; the word +0x04 is the pick key [OPEN its role]
    std::int16_t pool = 0;       // DGROUP:0x8568: the ship pool
    std::int16_t rec = 0;        // DGROUP:0x856a: the ship
    std::array<std::int16_t, 10> weapon{};  // DGROUP:0x856c + 2j: ship byte +0xb8 + j, -1 unused
    std::array<std::int16_t, 10> count{};   // DGROUP:0x8580 + 2j: ship byte +0xae + j, -1 unused
};
static_assert(sizeof(PodRecord) == 0x34, "pod record 0x34");
// The port keeps 16 pod records; the table's real bound is pool 11's allocated count
// DGROUP:0x9302, which the set-up sets [OPEN].
constexpr int kPodRecords = 16;

struct EffectState {
    EffectSides sides;              // DGROUP:0x5170, DGROUP:0x5224
    std::int16_t dg_5144 = 0;       // DGROUP:0x5144/0x5146: the point a mode-3 sound (KABOOM) plays at
    std::int16_t dg_5146 = 0;
    std::int16_t dg_b0c = 1;        // DGROUP:0xb0c, 1 in the image, 0 while the blast's dispatcher call runs [BYTES DGROUP:0xb0c]
    std::int16_t dg_95a8 = 0;       // DGROUP:0x95a8 = 1 at the demo player's death [BYTES seg16:0xaea]
    // DGROUP:0x698c (now + 10000 when a pool-4 record is destroyed) is World::w698c [BYTES seg12:0x279a].
    std::array<PodRecord, kPodRecords> pods{};  // DGROUP:0x8560
};
// The program's one EffectState (DGROUP lives once).
EffectState& effect_state();

// The DGROUP words the routines read that the World does not hold.
struct EffectWords {
    std::uint32_t now = 0;        // DGROUP:0x1e50
    std::int16_t autopilot = 0;   // DGROUP:0x951a
    std::int16_t dg_1318 = 0;     // DGROUP:0x1318: the blast runs only while it is <= 6 [OPEN its meaning]
};

// The calls the effect routines make into other parts of the port; the defaults do nothing.
class EffectHost {
public:
    virtual ~EffectHost() = default;
    // seg10:0x754a(id, pool, rec, mode): a sound. Mode 3 plays at DGROUP:0x5144/0x5146
    // (EffectState::dg_5144), which the routine writes first.
    virtual void sound(int id, int pool, int rec, int mode) { (void)id, (void)pool, (void)rec, (void)mode; }
    // seg7:0x1208, the player's sprite.
    virtual void player_sprite() {}
    // seg8:0x5df2(pool, rec, src_pool, src_rec, amount), the hit dispatcher (COLLIDE's).
    virtual void hit(int pool, int rec, int src_pool, int src_rec, std::int16_t amount) {
        (void)pool, (void)rec, (void)src_pool, (void)src_rec, (void)amount;
    }
    // seg8:0x4256-0x452a: the 5 x 5 piece effects of types >= 6 (seg7:0x342e, seg8:0x9694,
    // DGROUP:0x2414) [OPEN: not ported, not a death effect].
    virtual void pieces(int k) { (void)k; }
    // seg8:0x458e-0x45cd: entry k < 4 of type 6 freed: its DC DGROUP:0x5e98 + 2k [OPEN].
    virtual void piece_free(int k) { (void)k; }
    // seg18:0x5350(x, y) after an entry ends in the seg18 mode [OPEN M6].
    virtual void seg18_5350(std::int16_t x, std::int16_t y) { (void)x, (void)y; }
    // seg11:0xaece at the demo player's death with DGROUP:0x951a = 0 [OPEN].
    virtual void seg11_aece() {}
    // The design record's word +0x18 (DGROUP:0x970a + 0xad n) [OPEN: the design table].
    virtual std::int16_t design_kind(int design) { return (void)design, 0; }
    // The far call of a record's `on_destroy` (+0x66), cdecl (pool, rec) [BYTES seg13:0x4a1d].
    virtual void destroy(int pool, int rec) { (void)pool, (void)rec; }
    // seg7:0x2080(pool, arg) [OPEN].
    virtual void seg7_2080(int pool, int arg) { (void)pool, (void)arg; }
    // seg12:0x2aec-0x2afd: a destroyed pool-6 record's weapon slot DGROUP:0x66e4 + 0x1c·rec
    // (DGROUP:0x66ec = W(w)+0x1e, DGROUP:0x66e4 = -1) [BYTES seg12:0x2aec-0x2afd] (weapons, M4).
    // object_destroyed writes the two words in Pools::missiles itself (missile.h
    // missile_destroyed) and then calls this, for a copy the program keeps (M4 s2 MISSILE).
    virtual void pool6_destroyed(int rec) { (void)rec; }
};

// Everything the routines touch.
struct EffectCtx {
    World& w;
    EffectState& st;
    const EffectWords& in;
    EffectHost& host;
    shell::Rng& rng;
    Blitter* out = nullptr;                  // the window's Blitter; null draws nothing
    const ImageSlotTable* images = nullptr;  // DGROUP:0x39e8
    ToScreen to_screen;                      // seg8:0x29ea; empty gives (0, 0)
    std::int16_t* dg_943a = nullptr;         // DGROUP:0x943a/0x943c: the colliding party the
    std::int16_t* dg_943c = nullptr;         //   blast sets before its dispatcher call
    // The weapon words DGROUP:0x2f54 seg12:0x274a reads for a pool-6 record; null reads the
    // program's (missile.h missile_state) (M4 s2 MISSILE).
    const WeaponWords* weapons = nullptr;
};

// seg8:0x3fbe(k) [BYTES seg8:0x3fbe-0x45db] (verified claims 1, 4-13):
//  - entry k with +0x00 < 0 or +0x02 < +0x00 goes to the end test without a frame;
//  - types 1 and < 0 wait (no frame) while +0x10 > now; at +0x00 = 1 they play KABOOM
//    seg10:0x754a(0x45, 0, 0, 3) at the world point;
//  - types <= 5 draw fireball min(+0x00 - 1, 7) of slot 10, side ((6 (+0x00 - 1) + 10) << Size)
//    >> 1, SRCCOPY on the window centred on the entry's screen centre, for type >= 0 inside the
//    play area while +0x00 < +0x02 - 1, and call the blast at +0x00 % 6 = 0 for type != 1; on
//    the last frame (+0x00 >= +0x02 - 1, type != 0xff06) they black a (52 << Size) >> 1 square,
//    redraw the player (seg7:0x1208) while it is in play, DGROUP:0x951a = 0 and alive, and for
//    type != 1 play 0x6a on a ship source in play and black the source's square;
//  - types >= 6 go to EffectHost::pieces;
//  - +0x00 += 1; at +0x02 the entry is freed, DGROUP:0x9434 becomes the highest busy index
//    (0 when none) and DGROUP:0x9430 drops by 1, floor 0.
void effect_frame(EffectCtx& c, int k);

// seg8:0x5ab0(k), the blast of a type 4 or 5 entry at its frame 6: area damage and push for
// every object of pools DGROUP:0x9326 ... DGROUP:0x9324 near the entry's world point. Ported
// from the analyst's reading (s1-death.md section 2, claims 15-22), which no verifier has
// checked [OPEN]; the seg18 doubling (DGROUP:0x90f4 = 6) is not ported [OPEN].
void blast(EffectCtx& c, int k);

// seg16:0x8e4(pool, ship), a ship's `on_destroy` [BYTES seg16:0x8e4-0xb4c] (verified claims 2-4,
// 23; core.md "Death and the explosion"): the 8 death entries (rand() for entries 1 ... 7 with a
// free slot, x then y), `tumbling` +0xe2 = 1, object +0x26 = 1, the lock-out +0x84 = now + 6000,
// the burnt sheet seg13:0x4710, `alive` +0xec = 0, the duel's DGROUP:0x668a, KABOOM, the
// player's deadlines and rating, and the pod seg13:0x498e(ship, 1) outside the seg18 mode.
void ship_destroyed(EffectCtx& c, int pool, int ship);

// The fragment count of seg12:0x274a for `blast_size` b [BYTES seg12:0x2816-0x2880]: 1 for b =
// 1; else b·30/100, b·60/100, b, b·130/100 for Detail 0 ... 3, at least 2 (another Detail leaves
// the count unset in the original: the port takes 2 [DESIGN]); then with DGROUP:0x9430 > 10,
// (35 - DGROUP:0x9430)·n/25, at least 1. All 16-bit.
std::int16_t fragment_count(std::int16_t blast_size, std::int16_t detail, std::int16_t in_use);

// seg12:0x274a(pool, rec), the generic destroy handler [BYTES seg12:0x274a-0x2b1d] (verified
// claims 24-26; core.md "Fragments"): the index test, DGROUP:0x698c for pool 4, the square
// blacked in the play area (not for a pool-8 record of kind 1), the fragments (rand() % 110 for
// the start, % 10 and % 4 for the short ones, x then y around the object), `state` = 1, the
// player's DGROUP:0x1e84 / DGROUP:0x1e80 = now + 1500, the pool-6 slot and KABOOM.
void object_destroyed(EffectCtx& c, int pool, int rec);

}  // namespace is::world
