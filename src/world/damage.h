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
// seg8:0x2cd8(ship, side): a wing torn off (side 1: byte +0xad, side 0: +0xac): dword +0x156 =
// now - 10000, the byte cleared, ordinal 15's side [bp-4] = 1 for side 0, design +0x3c recomputed
// (design_words.h design_band_recompute), the taunts while DGROUP:0x92e4 = 0, then one
// CollideHost::wing_picture (the debris entry with ordinal 15's flag, or the cut alone) and
// sound 0x6a after a debris entry [BYTES seg8:0x2cd8-0x2d96 seg8:0x2e5d seg8:0x33c2-0x33cd
// seg8:0x343d-0x3482] (work/analysis/m4/s5-sheet.md 1.1-1.2, verified claims 1, 2, 4).
void tear_wing(CollideCtx& x, int ship, int side);
// seg8:0x2cd8's head, shared by tear_wing and the restore re-cut (player_sheet.h
// recut_lost_wings): dword +0x156 = now - 10000 (DGROUP:0x1e50), the wing byte cleared (+0xac for
// side 0, +0xad for side 1) and, while DGROUP:0x95a6 = 0, design +0x3c recomputed. Returns
// ordinal 15's cut flag [bp-4]: 1 for side 0 [BYTES seg8:0x2cea-0x2d27] (M4 s5 fixer, review L13).
int wing_tear_head(ShipRecord& S, int side, std::uint32_t now, std::int16_t dg_95a6, int size_level);
// seg7:0x5090(pool, rec, d): the rock's and the gate post's on_damage, 1 when a rock was
// destroyed or split [BYTES seg7:0x5090-0x565d].
std::int16_t rock_damage(CollideCtx& x, int pool, int rec, std::int16_t d);

// seg7:0x233c(pool, rec, d): the icon's on_damage (core.md "Shooting an icon"), always on pool 0
// (it reads DGROUP:0x1df8, not its pool argument): no damage while icon word DGROUP:0x57a8 +
// 0x23·rec != 0 or heat byte +0x2f is -100 or -2; else `damage` += 3d capped at 211, copied to
// directory word +0x1d2 + 2·rec; the crack overlay (CollideHost::icon_crack) while `damage` > 0
// and +0x2f >= 0, with sound 0x12 for d != 0 and DGROUP:0x57aa + 0x23·rec = 0; the shatter and 1
// for d != 0 with `damage` > 208 or +0x2f < 0; else the column row seg7:0x65aa(0, rec) and 0
// [BYTES seg7:0x233c-0x2510].
std::int16_t icon_damage(CollideCtx& x, int pool, int rec, std::int16_t d);
// seg7:0x26c4(rec): the shatter: `damage` and the directory word -1, sound 0x1f, the fragments
// seg7:0x2768, `state` 1, the column row seg7:0x65aa(0, rec), the black (32 << Size) >> 1
// square at the screen position, seg7:0x5ae8(rec, 0) [BYTES seg7:0x26c4-0x2766].
void icon_shatter(CollideCtx& x, int rec);
// seg7:0x2768(rec): four pool-1 "Icon Fragment" records (s2-gun-verify.md 59, 60): the first
// record of `state` != 0 is reused (`state` and `damage` 0), else one is appended with every
// field of claim 60; each gets a bitmap and its quadrant picture (CollideHost), and the icon's
// position plus (0, 0), (176, 0), (0, 176), (176, 176) with its velocity plus (-10, -10),
// (20, -10), (-10, 20), (10, 10) [BYTES seg7:0x2768-0x2e00]. The pool-1 mover seg7:0x32d0 is
// [OPEN] (core.md "Pools 1, 4 and 11"): the port's fragments do not move until it is ported.
void icon_fragments(CollideCtx& x, int rec);

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
