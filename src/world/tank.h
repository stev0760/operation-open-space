// Pool 4, the fuel tank (docs/re/core.md "Fuel", "Other touches"; work/analysis/m4/s3-fuel.md
// section 3 with s3-fuel-verify.md claims 19-37, 57, 58): the mover seg12:0xae4, the tank's
// on_damage seg12:0xdc4 ("volatile": the wobble and the spontaneous explosion), the pickup
// dissolve seg12:0xe5c and the tank image seg10:0x565a. M4 stage 3, track FUEL.
//
// The respawns (core.md "The fuel tank", Respawns; C080): the race respawn seg12:0x4f4e is called
// only by seg12:0x3e98, which returns at once unless the wave is kind 6 (a race) [BYTES
// seg12:0x3ea8-0x3eaf seg12:0x3ee5], and a kind-7 (duel) wave's frame calls seg12:0x515e instead
// [BYTES seg7:0xe71 seg7:0xe78], which brings an idle tank back while a ship runs below 40 % of
// its tank. Outside a race or a duel a collected or destroyed tank stays gone for the wave (the
// only other `state` = 0 writer is the placement seg9:0xdf6 [BYTES seg9:0xf55]). Both respawns
// are M6 (races and duels), not ported (M4 s4 fixer).
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <cstdint>
#include <functional>

#include "world/collide.h"
#include "world/sprite.h"
#include "world/world.h"

namespace is::world {

struct UpdateCtx;

// The image slot of DGROUP:0x39f0 in the table DGROUP:0x39e8: (0x39f0 - 0x39e8) / 2 [BYTES
// seg12:0xce4 seg10:0x572d].
constexpr int kSlotTank = 4;

// What a pickup dissolve draws with and calls (seg12:0xe5c, seg12:0x129e).
struct DissolveOut {
    Blitter* out = nullptr;            // null draws nothing; the counter still runs [DESIGN]
    const ImageSlotTable* images = nullptr;  // DGROUP:0x39e8
    DcId window_dc = kDcWindow;        // DGROUP:0x13dc
    std::function<void()> player_sprite;            // seg7:0x1208
    std::function<void(int pool, int rec)> in_use;  // seg7:0x2080(pool, rec)
};

// seg12:0xdc4(pool, rec, amount), cdecl [BYTES seg12:0xdc4-0xe5a] (verified claim 21): `damage`
// += amount; the wobble byte +0x2f = 1 when 0; byte +0x50 = lo8(max(1, damage / 10)) (signed
// `idiv`); above 5 (signed byte) the record's on_destroy (+0x66, through collide.h run_on_destroy)
// and seg7:0x2080(pool, rec), result 1; else 0. 60 damage destroys at once.
std::int16_t tank_damage(CollideCtx& x, int pool, int rec, std::int16_t amount);

// seg12:0xae4 [BYTES seg12:0xae4-0xc5c] (verified claims 22-29): every in-use record's `collided`
// +0x1c cleared first; per record below DGROUP:0x940e: a taken tank (`state` != 0) only continues
// its dissolve while +0x51 != 0; `fixed` stops it; off screen it moves every third frame (drag
// 0x28, three steps) and does not collide; on screen drag, one step, the cell (+0x26 / 5) of
// record 0's size and gap, the sprite from slot 4, and with no special time of day the wobble
// (+0x26 += +0x2f, bounced between 5 and 21 at speed +0x50) and the volatile roll: +0x50 >
// rand() % 120 and +0x50 > rand() % 20 explode it (seg12:0x274a, seg7:0x2080). Then for an
// on-screen tank not `collided`: the pair finder; a ship with no shield and no cloak touches it
// (seg13:0x16ac), anything else bounces it, 125 for a pool-5 hit or an icon with DGROUP:0x57aa
// != 0, and dispatches with the tank as T.
void move_tanks(UpdateCtx& c, World& w);

// seg12:0xe5c(i), cdecl [BYTES seg12:0xe5c-0x129c] (verified claims 30-37): one step of a taken
// tank's dissolve. The frame-clear flag (DGROUP:0x8ffe != 0 and +0x51 > 8, before the step), the
// step +0x51 -= DGROUP:0x8ffe (floor 0), the phase p = 9 - +0x51, COLORONCOLOR, the player's
// sprite on odd phases when the tank's +0x48 is the player and `Detail` > 1, nothing more off
// screen; phase 0 the cell shrunk, 1 ... 4 squeezed sideways (with the side strips when
// DGROUP:0x8ffe != 0), 5 ... 8 a shrinking vertical strip (strips likewise), 9 blacked with
// seg7:0x2080(4, -1) and the player's sprite; the frame of width u(3) when the flag is set. It
// adds no fuel and plays no sound. Returns the phase.
std::int16_t tank_dissolve(World& w, int i, const DissolveOut& d);

// seg10:0x565a [BYTES seg10:0x565a-0x58cd]: the image DGROUP:0x39f0 (slot 4) for DGROUP:0x9214 =
// 0, 1 or 2: s = 36, g = 6 (0) or s = 30, g = 4, graphic 270, 272 or 274; a (5s + 6g) x (2g + s)
// image blacked, then cell k = 0 ... 4 of the graphic (s·k, 0, s x s) at ((g + s)k + g, g). It
// returns while the slot holds an image made for the same DGROUP:0x9214 (DGROUP:0x17c0). Last,
// seg10:0x6308(4, -1, -1, -1) at `size_level` (DGROUP:0x9156, rescale.h rescale_image) [BYTES
// seg10:0x58bb-0x58c4]: the 216 x 48 can becomes 432 x 96 at Size 2 and 108 x 24 at Size 0 (M4 s8
// SIZE-SITES). The variants 3 and 4 (two graphics, seg10:0x6ea0(9)) are [OPEN] (the meals, M6).
// `made_for` stands for DGROUP:0x17c0.
void build_tank_image(Blitter& out, ImageSlotTable& images, std::int16_t w9214, std::int16_t& made_for,
                      int size_level = 1);

}  // namespace is::world
