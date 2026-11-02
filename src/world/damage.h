// Damage (docs/re/core.md "Damage"): the ship pool's on_damage seg8:0x45e0 with the wings
// seg8:0x2cd8, the rock pool's (and the gate posts') on_damage seg7:0x5090 with the break-up
// into three small rocks, and the auto-repair seg11:0x1d7c that seg11:0x1bcc calls at its end.
//
// Ported from the listings (isre asm) with core.md, work/analysis/m4/s1-hit.md and
// s1-hit-verify.md (claim 25 CORRECTED: seg7:0x5090's guard on the last collision's B party)
// and s1-death.md section 5 (the small rocks' fields). The destroy routines are the effects'
// (seg12:0x274a, seg16:0x8e4): reached through CollideHost. 16-bit arithmetic stays 16-bit
// (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>

#include "world/collide.h"

namespace is::world {

// seg8:0x45e0(pool, ship, d): the ship's on_damage, 1 when the ship died, else 0
// [BYTES seg8:0x45e0-0x4d5d].
std::int16_t ship_damage(CollideCtx& x, int pool, int ship, std::int16_t d);
// seg8:0x2cd8(ship, side): a wing torn off (side 1: byte +0xad, side 0: +0xac), the head the
// port runs and the debris through CollideHost [BYTES seg8:0x2cd8-0x2d23 seg8:0x2d96 seg8:0x33cd].
void tear_wing(CollideCtx& x, int ship, int side);
// seg7:0x5090(pool, rec, d): the rock's and the gate post's on_damage, 1 when a rock was
// destroyed or split [BYTES seg7:0x5090-0x565d].
std::int16_t rock_damage(CollideCtx& x, int pool, int rec, std::int16_t d);

// The words seg11:0x1d7c keeps: the 181-frame deadline (dword DGROUP:0x5014, 0 in the image)
// and the per-ship countdowns (DGROUP:0x1858 + 2i, 0 in the image) [BYTES DGROUP:0x5014
// DGROUP:0x1858].
struct RepairState {
    std::uint32_t due = 0;
    std::array<std::int16_t, kShipCount> countdown{};
};
// seg11:0x1d7c on the World (frame# = World::frames, DGROUP:0x1e68), the design words through
// `h` [BYTES seg11:0x1d7c-0x1ef4].
void auto_repair(World& w, RepairState& r, CollideHost& h);

}  // namespace is::world
