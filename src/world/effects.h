// The effect table's routines (docs/re/core.md "Death and the explosion", "Rocks"; work/analysis/
// m4/s1-death.md sections 1-3 with s1-death-verify.md): the frame routine seg8:0x3fbe that
// animates the explosions, the blast seg8:0x5ab0, a ship's death seg16:0x8e4 (its `on_destroy`),
// the generic destroy handler seg12:0x274a (the fragments), and the explosion images of
// seg10:0x5102. M4 stage 1, track DEATH. M4 stage 5, track EFFECTS (work/analysis/m4/
// s5-effects.md section 1 with s5-effects-verify.md): the 5 x 5 pieces of effect types 6 (the
// break-up seg7:0x2e02) and 7 (the gun's spark seg8:0x804a, the defender burst seg8:0x92a0), the
// piece table DGROUP:0x2414, their source DCs DGROUP:0x5e98, the 20 brushes DGROUP:0x576c and
// their colour offset DGROUP:0xb6a.
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
#include <functional>

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
    Dword age;                   // DGROUP:0x8564: low word the age in frames, high word (DGROUP:0x8566) the lifetime; 0x01f40000 at the fill (pods.h pod_age, pod_lifetime) [BYTES seg13:0x4bb3 seg12:0x6c74 seg12:0x6b80]
    std::int16_t pool = 0;       // DGROUP:0x8568: the ship pool
    std::int16_t rec = 0;        // DGROUP:0x856a: the ship
    std::array<std::int16_t, 10> weapon{};  // DGROUP:0x856c + 2j: ship byte +0xb8 + j, -1 unused
    std::array<std::int16_t, 10> count{};   // DGROUP:0x8580 + 2j: ship byte +0xae + j, -1 unused
};
static_assert(sizeof(PodRecord) == 0x34, "pod record 0x34");
// The port keeps 16 pod records; the table's real bound is pool 11's allocated count
// DGROUP:0x9302, which the set-up sets [OPEN].
constexpr int kPodRecords = 16;

// ---- the pieces of types 6 and 7 (s5-effects.md 1.4)

// Piece P(e, i, j) at DGROUP:0x2414 + 0x1c·((5e + i)·5 + j), e < 4 (the index of every user)
// [BYTES seg8:0x430e-0x431b seg8:0x7e0-0x7ed seg7:0x346e-0x347b seg8:0x96ef-0x96fc] (verified
// claim 6). Only seg7:0x342e, seg8:0x9694, seg8:0x7d2 and seg8:0x3fbe touch the table, and
// nothing resets it at the placement or the load (verified claim 23).
struct EffectPiece {
    std::int16_t sx = 0, sy = 0;          // +0x00/+0x02 DGROUP:0x2414: this frame's screen point (seg8:0x7d2)
    std::int16_t old_x = 0, old_y = 0;    // +0x04/+0x06 DGROUP:0x2418: the previous frame's, blacked after the draw
    std::int16_t prev_x = 0, prev_y = 0;  // +0x08/+0x0a DGROUP:0x241c: the copy of +0x00/+0x02, the next old
    std::int16_t vx = 0, vy = 0;          // +0x0c/+0x0e DGROUP:0x2420: velocity per frame
    std::int16_t ax = 0, ay = 0;          // +0x10/+0x12 DGROUP:0x2424: the acceleration coefficients
    std::int16_t w14 = 0, w16 = 0;        // +0x14/+0x16 DGROUP:0x2428: no reference (verified claim 23)
    std::int16_t wx = 0, wy = 0;          // +0x18/+0x1a DGROUP:0x242c: world point
};
static_assert(sizeof(EffectPiece) == 0x1c, "piece record 0x1c");
constexpr int kPieceEntries = 4;   // types >= 6 live in entries 0 ... 3 (s5-effects.md 1.2)
constexpr int kPieceGrid = 5;      // the 5 x 5 loops [BYTES seg8:0x452a seg8:0x4480]
constexpr int kPieceCount = kPieceEntries * kPieceGrid * kPieceGrid;  // 100: DGROUP:0x2414 ... 0x2f03
constexpr std::uint16_t kDgPieceTable = 0x2414;
constexpr int kEffectBrushes = 20;  // DGROUP:0x576c + 2n, n = 0 ... 19 [BYTES seg10:0x4c0b-0x4d16]
// The pool word of the gun's spark: shot slot `rec` (seg8:0x804a) [BYTES seg8:0x96a3 seg8:0x8187].
constexpr std::int16_t kPieceShotPool = 0x1e;
// The 20 COLORREFs seg10:0x4b2e passes to CreateSolidBrush for DGROUP:0x576c (plain RGB
// 0x00bbggrr, not PALETTERGB) [BYTES seg10:0x4c0b-0x4d16] (verified claim 31): greys 0 ... 5,
// greens 6 ... 9, blues 10 ... 14 (the player's spark), reds to orange 15 ... 19 (other shots').
extern const std::array<std::uint32_t, kEffectBrushes> kEffectBrushColours;

// The break-up's source DC for DGROUP:0x5e98 + 2e: `owned` when the port made it for this entry
// (the pool-1 case, whose DC seg7:0x2e02 creates and seg8:0x3fbe deletes) [BYTES seg7:0x303f-0x306e
// seg8:0x45ab-0x45c3].
struct BreakSource {
    DcId dc = kDcNone;
    bool owned = false;
};

struct EffectState {
    EffectSides sides;              // DGROUP:0x5170, DGROUP:0x5224
    std::int16_t dg_5144 = 0;       // DGROUP:0x5144/0x5146: the point a mode-3 sound (KABOOM) plays at
    std::int16_t dg_5146 = 0;
    // DGROUP:0xb0c is CollideGlobals::w0b0c, which the blast reaches through EffectCtx::dg_b0c
    // (M4 s4 fixer, C006, C084).
    std::int16_t dg_95a8 = 0;       // DGROUP:0x95a8 = 1 at the demo player's death [BYTES seg16:0xaea]
    // DGROUP:0x698c (now + 10000 when a pool-4 record is destroyed) is World::w698c [BYTES seg12:0x279a].
    std::array<PodRecord, kPodRecords> pods{};  // DGROUP:0x8560
    // ---- M4 s5 EFFECTS
    std::array<EffectPiece, kPieceCount> pieces{};  // DGROUP:0x2414
    // DGROUP:0x5e98 + 2e, e < 4: the type-6 entry's source DC; DGROUP:0x5ea0 + 2e (the bitmap
    // the pool-1 DC replaced) is the port's `owned` flag [BYTES seg7:0x3015-0x306e].
    std::array<DcId, kPieceEntries> piece_src{};
    std::array<bool, kPieceEntries> piece_src_owned{};
    std::array<BrushId, kEffectBrushes> brushes{};  // DGROUP:0x576c (create_effect_brushes)
    bool brushes_made = false;                     // create_effect_brushes ran [DESIGN]
    // The colour offset DGROUP:0xb6a, its step DGROUP:0xb6c (0 and 1 in the image, no other
    // writer: verified claim 33) and the cycle's last time DGROUP:0x579c [BYTES DGROUP:0xb6a].
    std::int16_t dg_b6a = 0;
    std::int16_t dg_b6c = 1;
    std::uint32_t dg_579c = 0;
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
struct ShotState;

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
    // DGROUP:0xb0c, the dispatcher's word (CollideGlobals::w0b0c): the blast sets it to 0 around
    // its dispatcher call when the entry's frame counter is not 0, so the dispatcher skips the
    // defender block and messages 0x75 / 0x76 for blast hits [BYTES seg8:0x5b88 seg8:0x5b92
    // seg8:0x5b99 seg8:0x5bb8 seg8:0x616a seg8:0x6589] (M4 s4 fixer, C006, C084). Null writes
    // nothing.
    std::int16_t* dg_b0c = nullptr;
    // The shot slots DGROUP:0x6098 whose per-sub-step velocity +0x70/+0x72 seg8:0x9694 reads for
    // the gun's spark (shots.h); null reads the program's shot_state() (M4 s5 EFFECTS).
    ShotState* shots = nullptr;
};

// The program's calls of seg7:0x2e02 for the break-up's source (src/world/flight_hooks.cpp sets
// them; empty ones give no source and release nothing) (M4 s5 EFFECTS):
//  - break_source(pool, rec): seg7:0x303f-0x306e and seg7:0x2e5c-0x2ec2: pool 1 a new DC with the
//    fragment's bitmap DGROUP:0x4e94 + 2·rec (owned), pool 8 image slot DGROUP:0x39e8[word
//    DGROUP:0x3d6e + 0x34·rec], pool 6 slot [W(w)-4] (DGROUP:0x2f50 + 0x50·w, w = DGROUP:0x66e4 +
//    0x1c·rec), pool 4 slot 4 (DGROUP:0x39f0), any other pool none;
//  - release(dc): SelectObject back and DeleteDC of an owned source [BYTES seg7:0x3015-0x3039];
//  - fragment_dc(r): the DC of pool-1 record r's bitmap DGROUP:0x4e94 + 2r (the HUD's
//    g_fragment_dc, src/hud/wave_win32.cpp), kDcNone for none.
struct EffectLinks {
    std::function<BreakSource(int pool, int rec)> break_source;
    std::function<void(DcId dc)> release;
    std::function<DcId(int r)> fragment_dc;
};
EffectLinks& effect_links();
// The wave teardown seg10:0x22e's release of the break-up sources [BYTES seg10:0x2cb-0x301]: for
// e = 0 ... 3 with DGROUP:0x5e98 + 2e != 0, SelectObject(DC, DGROUP:0x5ea0 + 2e), DeleteDC, then 0.
// The original deletes the source of any pool; the port deletes only the DCs it made (`owned`,
// the pool-1 case, through EffectLinks::release) and leaves another pool's slot DC to the image
// table, as seg7:0x2e02's release does [DESIGN] (M4 s5 fixer, review L17).
void release_piece_sources(EffectState& st);

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
//  - types >= 6 run the piece branch seg8:0x4256-0x452a (piece_frame) for entries 0 ... 3
//    (the only ones a creator gives such a type; another index skips it [DESIGN]), and at the
//    end of a type-6 entry < 4 its source DGROUP:0x5e98 is released (pool 1) and cleared;
//  - +0x00 += 1; at +0x02 the entry is freed, DGROUP:0x9434 becomes the highest busy index
//    (0 when none) and DGROUP:0x9430 drops by 1, floor 0.
void effect_frame(EffectCtx& c, int k);

// seg8:0x5ab0(k), the blast of a type 4 or 5 entry at its frame 6: area damage and push for
// every object of pools DGROUP:0x9326 ... DGROUP:0x9324 near the entry's world point (core.md
// "Death and the explosion", The blast, steps 1-7, C001-C007) [BYTES seg8:0x5ab0-0x5de6]. The
// seg18 doubling of d, dx and dy (DGROUP:0x9130 != 0 and DGROUP:0x90f4 = 6) [BYTES seg8:0x5d47
// seg8:0x5d55] is the seg18 mode's (M6), not ported.
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

struct CollideCtx;

// seg7:0x2e02(pool, rec), cdecl: the `on_destroy` of pool 1 (the icon fragments) and of the
// time-of-day tanks [BYTES seg7:0x2e02-0x3099] (s3-fuel-verify.md claims 43-45 with 44's
// additions; M4 s3 FUEL). Only while `state` = 0: `state` = 1, the square blacked; the first of
// effect entries 0 ... 3 that is free or of type <= 5 becomes the break-up: the sound (0x1a for
// pools 1 and 8 and for pool 6 with W(w)+0x24 = 6; 0x1f for pool 4 and the other pool-6 records;
// none for any other pool), an entry still in use (+0x00 > -1, signed) first copied into the first
// free entry of 4 ... 44 (every word but the step +0x04, and both side points), then +0x00 = 1,
// +0x02 = 20, type 6, side = size / 5, step 2 when the entry's index is above `Detail` else 1,
// pool, rec, the screen centre (+0x00, +0x02) and the world point (+0x04, +0x06), DGROUP:0x9430 +
// 1 (DGROUP:0x9434 is not touched). No entry of 0 ... 3 qualifying does nothing more. Pool 1
// always ends with seg7:0x65aa(0, DGROUP:0x6038 + 2·rec), the icon's column row. Once e is
// chosen the entry's old source DGROUP:0x5e98 + 2e, when not 0, is released and cleared (the
// original's SelectObject + DeleteDC for any pool; the port releases only a source it owns, as a
// shared slot DC must survive [DESIGN]), then the new source is latched from
// effect_links().break_source (the fragment's record may be reused while the 19 frames run)
// [BYTES seg7:0x3015-0x306e seg7:0x2e5c-0x2ec2] (verified claim 30) (M4 s5 EFFECTS).
void pickup_destroyed(CollideCtx& x, int pool, int rec);

// ---- the piece branch of seg8:0x3fbe (M4 s5 EFFECTS; s5-effects.md 1.3-1.7)

// The piece record of entry e, row i, column j: (5e + i)·5 + j, 16-bit [BYTES seg8:0x430e-0x431b];
// -1 outside entries 0 ... 3 [DESIGN].
int piece_index(int e, int i, int j);

// The eight acceleration cases (ax, ay) of seg7:0x342e and seg8:0x9694, whose switch bodies are
// the same instruction sequences [BYTES seg7:0x3460-0x3671 seg8:0x96e1-0x98f2] (verified claims
// 18-20; the other five read from the listing, M4 s5 EFFECTS): 0 (2(i + 5) - 7j, -4(j + 1) + 6i),
// 1 (7(2 - j) - 2(i - 2)², -4(j - 2)² + 6(i - 2)), 2 (7(2 - j) - 2(i - 2)², 4(j - 2)² + 6(2 - i)),
// 3 (2(i - 2) + 7(2 - j)(5 - i), 6(i - 2)(j - 5) + 4(j - 2)), 4 ((2 - j)(i - 2), 9(i - 2)(j - 2)),
// 5 (6((i - 2)³ + j - 2), 5(i - 2) - 2(j - 2)²), 6 (6((j - 2)³ - (i - 2)³), 7i(i - 2)² -
// 2(j - 2)²), 7 (7j - 3i - 8, 6(i - 2)³). A case above 7 writes nothing (`ja`).
struct PieceAccel {
    std::int16_t ax = 0, ay = 0;
};
PieceAccel piece_accel(int i, int j, int c);

// seg7:0x342e(e, pool, rec), cdecl [BYTES seg7:0x343d-0x3759] (verified claims 9, 16-18): s =
// object `size` / 5; for i = 0 ... 4 (outer), j = 0 ... 4: world point (lo16(s·j) << 3 + x,
// lo16(s·i) << 3 + y), velocity (vx / 10 + (j - 2)·8, vy / 10 + (2 - i)·7), case 0's
// accelerations, then one rand() % 8 picks the case. Exactly 25 rand().
void pieces_setup_break(EffectCtx& c, int e, int pool, int rec);
// seg8:0x9694(e, pool, rec), cdecl [BYTES seg8:0x96a3-0x99cc] (verified claims 17, 21, 22): V =
// shot slot rec's words +0x70/+0x72 (DGROUP:0x6108/0x610a) for pool 0x1e, else the object's
// velocity; world point ((j << 4) + DGROUP:0x5224[e].x + 8, (i << 4) + .y + 8), velocity
// ((j - 2)·8 + V.x, (2 - i)·7 + V.y), case 0, then (rand() % 32) % 8 picks the case (the % 32
// value lands in the pool argument's slot, never read again). Exactly 25 rand().
void pieces_setup_burst(EffectCtx& c, int e, int pool, int rec);
// seg8:0x7d2(e, i, j), Pascal `retf 6` [BYTES seg8:0x7f0-0x82f] (verified claim 7): +0x00/+0x02
// = seg8:0x29ea(+0x18, +0x1a), +0x04/+0x06 = +0x08/+0x0a, +0x08/+0x0a = +0x00/+0x02.
void piece_screen(EffectCtx& c, int e, int i, int j);
// seg8:0x4256-0x452a(k) [BYTES seg8:0x4256-0x4538] (verified claims 2-14): at frame 1 the set-up
// (type 6 seg7:0x342e, else seg8:0x9694) and, for a pool not above DGROUP:0x9324 (signed), the
// source object's square blacked; at frame 5 of type 6 the blast seg8:0x5ab0; then for i, j = 0,
// step, ... < 5 (the step +0x04 re-read on each pass): seg8:0x7d2, the draw (type 6: BitBlt
// SRCCOPY side x side from DGROUP:0x5e98[k] at (side·i + gap, side·j + gap), gap = the object's
// +0x24 read again, so the first frame shows the picture transposed; type 7: SelectObject of
// brush DGROUP:0x576c[+0x0e + DGROUP:0xb6a], not restored, and PatBlt PATCOPY), PatBlt BLACKNESS
// at the old point after the draw, on the last frame (+0x02 - +0x00 - 1 = 0) PatBlt BLACKNESS at
// the new point and seg7:0x1208 while DGROUP:0x951a = 0, and the motion: w += lo16(lo16(lo16(a·f)·f)
// ·9) / lo16(end·end) + v (16-bit, f the frame before its increment), x above DGROUP:0x5a0 << 3
// becomes 0 and below 0 becomes DGROUP:0x5a0 << 3, y likewise. A step <= 0 (no creator writes
// one) ends the loops [DESIGN]; so does a zero divisor [DESIGN].
void piece_frame(EffectCtx& c, int k);

// seg8:0x82c6-0x82f4, the part of the pool-2 mover seg8:0x8256 that runs on every wave frame
// before any test of pool 2 (verified claims 32-34): once now - 60 is above DGROUP:0x579c (32-bit
// unsigned) DGROUP:0x579c = now, DGROUP:0xb6a += DGROUP:0xb6c, and DGROUP:0xb6c is negated when
// DGROUP:0xb6a > 3 or < 1 (signed): from (0, 1) the offset runs 1, 2, 3, 4, 3, 2, 1, 0, 1, ...
void effect_colour_cycle(EffectState& st, std::uint32_t now);

// seg10:0x4c0b-0x4d16: the 20 CreateSolidBrush calls of seg10:0x4b2e into DGROUP:0x576c
// (kEffectBrushColours); colours reach the 8-bit surface through ADR 0007. The original deletes
// and recreates them on each seg10:0x4b2e (seg10:0x4be5-0x4c09); the port makes them once for
// the program's Blitter, at the wave's load beside build_effect_images [DESIGN].
void create_effect_brushes(Blitter& out, EffectState& st);

// seg12:0x274a(pool, rec), the generic destroy handler [BYTES seg12:0x274a-0x2b1d] (verified
// claims 24-26; core.md "Fragments"): the index test, DGROUP:0x698c for pool 4, the square
// blacked in the play area (not for a pool-8 record of kind 1), the fragments (rand() % 110 for
// the start, % 10 and % 4 for the short ones, x then y around the object), `state` = 1, the
// player's DGROUP:0x1e84 / DGROUP:0x1e80 = now + 1500, the pool-6 slot and KABOOM.
void object_destroyed(EffectCtx& c, int pool, int rec);

}  // namespace is::world
