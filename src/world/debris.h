// The wing debris (docs/re/core.md "Wings torn off"; work/analysis/m4/s1-death.md section 6 with
// s1-death-verify.md claims 42-47): the 3-entry table DGROUP:0x915c that seg8:0x2cd8 fills when a
// wing is torn off, its mover seg13:0x447a (called at the end of every wave frame), its draw
// seg8:0x8a6, and the smoke puffs seg7:0x5d6a it starts. The debris is not a pool object and calls
// no pair finder: it does not collide. M4 stage 1, track DEATH; the placement over the torn wing,
// its strip and the picture part of seg8:0x2cd8 (wing_picture) are M4 stage 5, track SHEET
// (work/analysis/m4/s5-sheet.md 1.2-1.4 and 3, verified in s5-sheet-verify.md claims 2, 6-10).
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "shell/rng.h"
#include "world/records.h"
// The smoke table DGROUP:0x58bc and seg7:0x5d6a (smoke_start) are smoke.h's (M4 s5 EFFECTS).
#include "world/smoke.h"
#include "world/sprite.h"
#include "world/visibility.h"

namespace is::world {

// Entry k of DGROUP:0x915c + 0x22k (s1-death.md section 6) [BYTES seg8:0x2f1d-0x2f8c
// seg13:0x463d-0x4670 seg8:0x8c0].
struct DebrisEntry {
    std::int16_t x = 0, y = 0;     // +0x00/+0x02 DGROUP:0x915c: world position
    std::int16_t sx = 0, sy = 0;   // +0x04/+0x06 DGROUP:0x9160: screen position, -1000 at creation
    std::int16_t vx = 0, vy = 0;   // +0x08/+0x0a DGROUP:0x9164: velocity
    DcId dc = kDcNone;             // +0x0c DGROUP:0x9168: the 5-cell strip
    DcId mask = kDcNone;           // +0x0e DGROUP:0x916a: its mask
    std::array<std::uint16_t, 5> gdi{};  // +0x10 ... +0x18: the bitmaps and old objects (GDI handles)
    std::int16_t counter = 0;      // +0x1a DGROUP:0x9176: the frame counter, 0 = free
    std::int16_t end = 0;          // +0x1c DGROUP:0x9178: the frame that frees it (70)
    std::int16_t extent = 0;       // +0x1e DGROUP:0x917a: the ship's `extent`
    std::int16_t w20 = 0;          // +0x20 [OPEN]
};
static_assert(sizeof(DebrisEntry) == 0x22, "debris entry 0x22");
constexpr int kDebrisCount = 3;
using DebrisTable = std::array<DebrisEntry, kDebrisCount>;

// The cell side p of the debris: (32 << Size) >> 1, or (64 Size + 32) / 3 in the seg18 mode
// [BYTES seg8:0x8e3-0x905 seg13:0x45da-0x45fd].
std::int16_t debris_cell(std::int16_t size, std::int16_t dg_9130);

// The head of seg8:0x2cd8's debris part [BYTES seg8:0x2f19-0x2f8c] (verified claim 45): entry k
// gets the counter 1 and end 70 (dword 0x00460001), +0x1e the ship's `extent`, the ship's world
// position, the screen position (-1000, -1000), and the ship's velocity plus half the heading
// vector of (heading + 4) % 16 (truncating). The free entry is the first whose counter is 0
// [BYTES seg8:0x33d7-0x33e9] (core.md "Wings torn off"). Returns k, -1 when the table is full.
// debris_place, the strip (debris_strip_plan) and sound 0x6a follow in wing_picture.
int debris_start(DebrisTable& t, const ObjectRecord& ship_obj);

// seg8:0x2cd8's distance R from the ship's centre to the torn wing's in world units, non-seg18
// path: R = 8 trunc(D / 2) + trunc((E - 8 D) / 4), E = the ship's `extent` +0x20, D = design
// +0x3c, 16-bit [BYTES seg8:0x3405-0x343a seg8:0x2f0b-0x2f16] (verified claim 6). TRANSFORMER at
// Size 1 (E 264, D 5): 72.
std::int16_t debris_reach(std::int16_t D, std::int16_t extent);

// The window of the torn wing's picture and the square's offsets [BYTES seg8:0x2f90-0x3096]
// (verified claims 7, 8, 45), non-seg18 path (cell 256 world units, c = 128):
//   m = cut ? (heading + 12) % 16 : (heading + 4) % 16 (signed idiv),
//   P = trunc(E / 2) + trunc(lo16(T128[m] R) / 128) per axis (`cwd; and dx, 0x7f; add; sar 7`),
//   x1 = min(c + Px, E), x0 = max(Px - c, 0), y1 = min(c + Py, E), y0 = max(Py - c, 0),
//   ox = max(c - Px, 0), oy = max(c - Py, 0),
// with T128 = DGROUP:0x66e (kDebrisT128), E = entry +0x1e. The entry's position gains (Px - c,
// Py - c), so the 256-unit square is centred on the wing R units to the port side (cut 1) or the
// starboard side (cut 0); the velocity push of debris_start is toward heading + 4 for both.
struct DebrisWindow {
    std::int16_t R = 0;                           // [bp-0xe]
    std::int16_t px = 0, py = 0;                  // [bp-0x22], [bp-0x20]
    std::int16_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // [bp-0x2a], [bp-0x28], [bp-0x26], [bp-0x24]
    std::int16_t ox = 0, oy = 0;                  // [bp-0x1e], [bp-0x1c]
};
extern const std::array<std::int16_t, 32> kDebrisT128;  // DGROUP:0x66e: (x, y) per heading
DebrisWindow debris_place(DebrisEntry& e, std::int16_t heading, std::int16_t D, bool cut);

// The five SRCCOPY BitBlts of seg8:0x3183-0x31de from ordinal 15's s x s wing bitmap into the
// black 5p x p strip, j = 0 ... 4: to ((256 j + ox) sar s, oy sar s), size ((x1 - x0) sar s,
// (y1 - y0) sar s), from (x0 sar s, y0 sar s), s = DGROUP:0x9150 [BYTES seg8:0x3183-0x31de]
// (verified claim 9). The seg18 mode's blits (seg8:0x31e0-0x3340, verified claim 44) are not
// ported [OPEN M6].
struct DebrisBlit {
    int x = 0, y = 0, w = 0, h = 0, sx = 0, sy = 0;
};
std::array<DebrisBlit, 5> debris_strip_plan(const DebrisWindow& win, int shift);

class World;

// seg8:0x2cd8's picture part on the World (CollideHost::wing_picture of the program): `cut` 1
// when wing +0xac goes. With `make_debris` and a free entry: debris_start, debris_place, ordinal
// 15 with its flag on the ship's sheet (player_sheet.h wing_cut_ship: the debris cell taken,
// then the cut), the strip: a 5p x p image made black (CreateCompatibleBitmap + PatBlt
// BLACKNESS), the five blits from the wing cell, and the strip's ordinal-6 mask (seg10:0x6ec6)
// [BYTES seg8:0x309a-0x33cd]; true, and seg8:0x2cd8 plays 0x6a. Otherwise ordinal 15 with flag 0
// (the cut alone) and false [BYTES seg8:0x343d-0x3482]. `out` null or no sheet: the entry
// without a picture [DESIGN]. The seg18 mode's geometry (DGROUP:0x9130 != 0, seg8:0x2e67-0x2f16
// and seg8:0x31e0-0x3340) is not ported: the port takes the normal one and logs [OPEN M6].
bool wing_picture(World& w, DebrisTable& t, Blitter* out, const ImageSlotTable* images, int ship, int cut,
                  bool make_debris);

// Everything the mover and the draw touch.
struct DebrisCtx {
    DebrisTable& table;
    SmokeRecords& smoke;              // DGROUP:0x58bc (World::smoke, smoke.h)
    std::int16_t& dg_9456;            // DGROUP:0x9456, the smoke count
    Pools& pools;
    shell::Rng& rng;
    std::int16_t size = 1;            // DGROUP:0x9156
    std::int16_t dg_9130 = 0;         // DGROUP:0x9130
    std::int16_t W = 0, H = 0;        // DGROUP:0x90ea, 0x90ec
    std::int16_t detail = 3;          // DGROUP:0x9208
    Blitter* out = nullptr;           // the window's Blitter; null draws nothing
    ToScreen to_screen;               // seg8:0x29ea; empty gives (0, 0)
    const VisibilityGlobals* visible = nullptr;  // DGROUP:0x507c/0x50e0, count DGROUP:0x9146
    std::function<void()> player_sprite;         // seg7:0x1208
};

// seg8:0x8a6(k) [BYTES seg8:0x8a6-0xb71] (verified claim 46): below counter 2 only the screen
// position; else, when the old square (sx, sy, sx + p, sy + p) meets any visibility-list object's
// (scr_x, scr_y, + size), the old square is cleared through the mask (SRCAND from the mask at
// (0, 0)) and the new one drawn masked (SRCAND from the mask at (0, 0), SRCPAINT from the strip
// at frame ((counter / 2) % 5)·p); otherwise BLACKNESS at the old square and SRCCOPY at the new.
// A square is drawn only inside (-40, -40) ... (W + 40, H + 40), exclusive.
void draw_debris(DebrisCtx& c, int k);

// seg13:0x447a [BYTES seg13:0x447a-0x470f] (verified claims 42-44): for each entry with counter
// > 0: the draw, position += velocity, velocity -= velocity >> 4 (sar); at counter 1 the flames
// #230 burnt into the strip (SRCAND from their mask, SRCPAINT, DSna from the strip's own mask,
// 5p x p at (0, 0)) and the player's sprite; at counters 50, 56, 61, 63, 65, 66 a smoke puff
// seg7:0x5d6a(x + 0x80, y + 0x80, 1, r2, r1) with r1, r2 = rand() % 60 - 30 drawn in that
// order; then counter + 1, and at the end word the entry is freed, its square blacked and its
// DCs released (seg10:0x37d0(k), whose other work is [OPEN]).
void move_debris(DebrisCtx& c);

}  // namespace is::world
