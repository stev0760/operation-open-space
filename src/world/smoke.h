// The smoke (work/analysis/m4/s5-effects.md section 2 with s5-effects-verify.md, claims 35-53
// with 53 CORRECTED, A1, A3; M4 stage 5, track EFFECTS): the 20 records DGROUP:0x58bc
// (world.h SmokeRecord, World::smoke), the puff seg7:0x5b68 that every smoking object calls
// (the damaged ship's tail, cold and hit rocks, the gate posts and hazards, missile ends, pod
// expiry, the icon helpers), the debris puff seg7:0x5d6a, the mover seg7:0x5e04 with its
// screen step seg8:0x124a, the placement's reset and the smoke strip seg10:0x5202 (image slot
// 12, DGROUP:0x3a00).
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <cstdint>
#include <functional>

#include "shell/rng.h"
#include "world/records.h"
#include "world/sprite.h"
#include "world/visibility.h"
#include "world/world.h"

namespace is::world {

struct ShotState;

// ---- the strip seg10:0x5202

constexpr int kSlotSmoke = 12;          // DGROUP:0x3a00 [BYTES seg10:0x5275]
constexpr int kGraphicSmoke = 0xc8;     // IGRAPHIC.DLL #4000 200: 60 x 90, 4 bpp [BYTES seg10:0x5214]
constexpr int kSmokeStripW = 0x10e;     // 270 [BYTES seg10:0x525a]
constexpr int kSmokeStripH = 0x16;      // 22 [BYTES seg10:0x525d]
constexpr int kSmokeBands = 6;          // k = 0 ... 5 [BYTES seg10:0x52f0]
constexpr int kSmokeLife = 0x37;        // the counter that frees a record (55 calls) [BYTES seg7:0x6037]

// seg10:0x5202 [BYTES seg10:0x5210-0x532b] (verified claims 50, 51): #4000 200 through
// seg10:0x6c02(200, 0, -1) into a DC of its own; a 270 x 22 image in slot 12 (DGROUP:0x3a00,
// bitmap DGROUP:0x3ae8), PatBlt BLACKNESS; then for k = 0 ... 5 the band at (0, y_k) of
// 6(k + 5) x 2(k + 5) to (3 y_k, 0), y_0 = 0, y_{k+1} = y_k + 2(k + 5) (bands at x = 0, 30, 66,
// 108, 156, 210); the source freed. The palette DGROUP:0x920e selected into both DCs is the
// renderer's (ADR 0007). Last, seg10:0x6308(12, -1, -1, -1) at `size_level` (DGROUP:0x9156,
// rescale.h rescale_image) [BYTES seg10:0x5322-0x532e]: 540 x 44 at Size 2, 135 x 11 at Size 0
// (M4 s8 SIZE-SITES). The chain runs it only while DGROUP:0x3a00 is 0 [BYTES seg9:0x265c
// seg9:0x2696] (shell.md "Load"): the port tests slot 12 here.
void build_smoke_image(Blitter& out, ImageSlotTable& images, int size_level = 1);

// The placement seg9:0x6624's part: counter -1 for k < 20, no other field [BYTES
// seg9:0x6747-0x675d] (verified claim 36). The set-up calls it (setup.cpp); DGROUP:0x9456 = 0 is
// the set-up's own store.
void smoke_place(SmokeRecords& t);

// seg7:0x5d6a(x, y, arg, vx, vy) [BYTES seg7:0x5d6a-0x5e03] (s1-death-verify.md claim 47;
// s5-effects-verify.md claim 52 and A3): for Detail 0, 1, 2 a rand() % 100 below 20, 45, 70
// takes nothing (`jge` takes the record); Detail 3 draws no rand(). Otherwise the first record
// with counter -1 takes counter 0, the world point (x, y) and the velocity (vx, vy), and
// DGROUP:0x9456 + 1; `arg` is never read and no sound plays. A Detail outside 0 ... 3 reads an
// unset threshold in the original; the port takes the record [DESIGN ADR 0016 decision 1].
// Returns the record's index, -1 for none. Callers: the wing debris seg13:0x447a (debris.h) and
// hazard kind 1 at seg12:0x3b5f.
int smoke_start(SmokeRecords& t, std::int16_t& dg_9456, std::int16_t detail, shell::Rng& rng,
                std::int16_t x, std::int16_t y, std::int16_t arg, std::int16_t vx, std::int16_t vy);

// Everything seg7:0x5b68, seg7:0x5e04 and seg8:0x124a touch.
struct SmokeCtx {
    SmokeRecords& table;                  // DGROUP:0x58bc
    std::int16_t& dg_9456;                // DGROUP:0x9456, the records in use
    Pools& pools;                         // DGROUP:0x1df8
    shell::Rng& rng;                      // seg5:0xdc
    const ShipTable* ships = nullptr;     // DGROUP:0x7090: ship word +0x4e, the design (null reads 0)
    ShotState* shots = nullptr;           // the muzzle tables DGROUP:0x7b60 (null reads 0)
    std::int16_t detail = 3;              // DGROUP:0x9208 `Detail`
    std::int16_t size = 1;                // DGROUP:0x9156 `Size`
    std::int16_t ship_pool = kPoolShips;  // DGROUP:0x9324
    std::int16_t player = 0;              // DGROUP:0x9646
    std::int16_t dg_9130 = 0;             // DGROUP:0x9130, the seg18 mode
    Blitter* out = nullptr;               // the window's Blitter (DGROUP:0x13dc); null draws nothing
    const ImageSlotTable* images = nullptr;  // DGROUP:0x39e8: slot 12, the strip
    ToScreen to_screen{};                 // seg8:0x29ea; empty gives (0, 0)
    const VisibilityGlobals* visible = nullptr;  // DGROUP:0x507c / 0x50e0, count DGROUP:0x9146
    std::int16_t* dg_58a = nullptr;       // DGROUP:0x58a: the frame redraws the player (null: none)
    // seg10:0x754a(id, pool, rec, mode); empty plays nothing.
    std::function<void(int id, int pool, int rec, int mode)> sound{};
    // The seg18 mode's blocks of seg7:0x5e04 (the DGROUP:0x8860 rectangles, seg18:0x5350), not
    // ported [OPEN M6]; called with the block's address while DGROUP:0x9130 != 0.
    std::function<void(const char* address)> unported{};
};

// seg7:0x5b68(pool, rec, arg), cdecl [BYTES seg7:0x5b68-0x5d68] (verified claims 37-41):
//  - the Detail gate: threshold 25, 45, 65, 90 for Detail 0 ... 3; at Detail 3 no rand(),
//    otherwise rand() % 100 above the threshold returns (`jle` proceeds); a Detail outside
//    0 ... 3 leaves the threshold unset in the original: the port takes the record
//    [DESIGN ADR 0016 decision 1];
//  - the first record with counter -1 takes counter 0; with none: return, no sound, no count;
//  - a pool other than DGROUP:0x9324: x = rand() % extent + x, y = rand() % extent + y, then
//    d = rand() % 16 and v = H[d] / 2 + v / 2 (H = DGROUP:0x1050; signed 16-bit divides);
//  - the ship pool, no rand(): R = muzzle entry 16 + h of design n (ship word +0x4e, h = object
//    +0x28), x = (R.x - H[h].x / 4 - 4)·8 + x, likewise y, and v = v - H[h] / 2;
//  - seg10:0x754a(1, pool, rec, 1) (PUFF) when `arg` != 0; DGROUP:0x9456 + 1.
// Returns the record's index, -1 for none. A zero `extent` (the original's divide trap) and a
// heading outside 0 ... 15 take 0 for the term [DESIGN].
int smoke_puff(SmokeCtx& c, int pool, int rec, int arg);

// seg8:0x124a(k), Pascal `retf 2` [BYTES seg8:0x1258-0x1280] (verified claim 35): +0x04/+0x06 =
// +0x00/+0x02, then +0x00/+0x02 = seg8:0x29ea(+0x08, +0x0a).
void smoke_screen(SmokeCtx& c, int k);

// The puff's geometry at counter c and Size s (seg7:0x60ac-0x6123, verified claim 44): band q =
// c / 9, side p = ((q + 5) << (s + 1)) >> 1, offset h = (q << s) >> 1, source x =
// lo16(q·((30 << s) >> 1)) + ((T[q] << s) >> 1) + lo16(p·((c / 3) % 3)) with T = {0, 0, 6, 18,
// 36, 60} at [bp-0x1c] [BYTES seg7:0x5e13-0x5e28]. T[6] (counter 54) is the word [bp-0x10] past
// the table, which only the seg18 block writes (A1): `src_x_defined` is false then.
struct PuffCell {
    std::int16_t q = 0, p = 0, h = 0, src_x = 0;
    bool src_x_defined = true;
};
PuffCell puff_cell(std::int16_t counter, std::int16_t size);

// seg7:0x5e04 [BYTES seg7:0x5e04-0x6182] (verified claims 42-49), for k = 0 ... 19:
//  - a counter >= 55 or <= -1 goes to the end test only;
//  - c % 4 > Detail + 2 skips the move and the draw (Detail 0 freezes every fourth call) but
//    still counts;
//  - seg8:0x124a(k) first, so the puff stands where its world point was before the move; at
//    Detail >= 2 the square (scr - h, + p) meeting any listed object's (scr, + size) suppresses
//    the draw;
//  - world point += velocity (16-bit, no wrap); when the last call drew, PatBlt BLACKNESS at
//    old - h with this call's side p; the seg18 block (DGROUP:0x9130 != 0) is not ported;
//  - the draw: BitBlt SRCCOPY from slot 12 at (src x, 0), +0x12 = 1; else +0x12 = 0;
//  - DGROUP:0x58a = 1 when the screen point lies in the player's square grown by p;
//  - counter + 1; at 55: PatBlt BLACKNESS of (24 << Size) >> 1 at scr - ((6 << Size) >> 1),
//    counter -1, DGROUP:0x9456 - 1.
// At counter 54 the source x is undefined (A1): the port skips that one BitBlt and keeps the
// flag as if it drew; the end square of the same call covers the draw at every Size
// [DESIGN ADR 0016 decision 1].
void move_smoke(SmokeCtx& c);

}  // namespace is::world
