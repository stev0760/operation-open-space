// The wing debris (docs/re/core.md "Wings torn off"; work/analysis/m4/s1-death.md section 6 with
// s1-death-verify.md claims 42-47): the 3-entry table DGROUP:0x915c that seg8:0x2cd8 fills when a
// wing is torn off, its mover seg13:0x447a (called at the end of every wave frame), its draw
// seg8:0x8a6, and the smoke puffs seg7:0x5d6a it starts. The debris is not a pool object and calls
// no pair finder: it does not collide. M4 stage 1, track DEATH.
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "shell/rng.h"
#include "world/records.h"
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

// The 20 x 0x14 smoke table DGROUP:0x58c4 that seg7:0x5d6a fills (its mover seg7:0x5e04 runs
// while DGROUP:0x9456 > 0, not ported here) [BYTES seg7:0x5dcc-0x5df9].
struct SmokeEntry {
    std::int16_t x = 0, y = 0;   // DGROUP:0x58c4, 0x58c6
    std::int16_t a = 0, b = 0;   // DGROUP:0x58c8, 0x58ca: the call's 4th and 5th arguments
    std::int16_t used = -1;      // DGROUP:0x58cc: -1 free (the placement writes -1), 0 at the start
    std::array<std::int16_t, 5> rest{};  // +0x0a ... +0x13 [OPEN]
};
static_assert(sizeof(SmokeEntry) == 0x14, "smoke entry 0x14");
using SmokeTable = std::array<SmokeEntry, 20>;

// The cell side p of the debris: (32 << Size) >> 1, or (64 Size + 32) / 3 in the seg18 mode
// [BYTES seg8:0x8e3-0x905 seg13:0x45da-0x45fd].
std::int16_t debris_cell(std::int16_t size, std::int16_t dg_9130);

// seg7:0x5d6a(x, y, arg, a, b) [BYTES seg7:0x5d6a-0x5e03] (verified claim 47): for Detail 0, 1,
// 2 a rand() % 100 below 20, 45, 70 takes nothing; Detail 3 draws no rand(). Otherwise the first
// entry with `used` = -1 takes x, y, a, b and `used` 0, and DGROUP:0x9456 + 1. `arg` is not
// stored. A Detail outside 0 ... 3 reads an unset threshold in the original; the port takes the
// slot [DESIGN]. Returns the entry's index, -1 for none.
int smoke_start(SmokeTable& t, std::int16_t& dg_9456, std::int16_t detail, shell::Rng& rng,
                std::int16_t x, std::int16_t y, std::int16_t arg, std::int16_t a, std::int16_t b);

// The head of seg8:0x2cd8's debris part [BYTES seg8:0x2f19-0x2f8c] (verified claim 45): entry k
// gets the counter 1 and end 70 (dword 0x00460001), +0x1e the ship's `extent`, the ship's world
// position, the screen position (-1000, -1000), and the ship's velocity plus half the heading
// vector of (heading + 4) % 16 (truncating). The rest of seg8:0x2cd8's debris part (the position
// offsets from the wing's side, the strip image of seg10:0x7370 in five cells and its mask, the
// sound 0x6a) is the wing tear's, not read here: DebrisEntry::dc and ::mask stay for the caller
// to set [OPEN]. The free entry is the one whose counter is 0 (core.md "Wings torn off").
// Returns k, -1 when the table is full.
int debris_start(DebrisTable& t, const ObjectRecord& ship_obj);

// Everything the mover and the draw touch.
struct DebrisCtx {
    DebrisTable& table;
    SmokeTable& smoke;
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
