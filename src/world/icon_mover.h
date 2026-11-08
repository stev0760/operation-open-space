// The icon mover seg7:0x309c and its hit handler seg7:0x4a38 (work/analysis/m4/s2-capture.md 1b,
// verified claims 1-3, from the listings work/analysis/m4/s2-capture-packs/seg7_309c.lst and
// seg7_4a38.lst). The pair finder seg8:0x50aa hands a pair to the lower pool's mover only
// (core.md "Who tests whom"), so a ship meets an icon here, not in the ship mover: this is where
// the player captures (s2-capture.md 0).
//
// The helpers seg7:0x477e, 0x4d86, 0x4ffe, 0x5660 and the pool-5 cases seg7:0x4a8a-0x4c4f
// (with seg7:0x56ba and 0x2512 under them) are icon_effects.h's (M4 s5 ICONS); the smoke
// seg7:0x5b68 they call is UpdateHost::puff. Portable, no Win32.
#pragma once

#include "world/update.h"

namespace is::world {

// The image slot of the icon strip DGROUP:0x3a02 in the table DGROUP:0x39e8: (0x3a02 - 0x39e8) / 2,
// and the first of the per-icon slots DGROUP:0x3a04 + 2i [BYTES seg7:0x30d3 seg7:0x3282].
constexpr int kIconStripSlot = 13;
constexpr int kIconOwnSlot = 14;

// seg7:0x309c [BYTES seg7:0x309c-0x3293]: nothing in Demon's Gate; per `state`-0 icon i below
// DGROUP:0x91fa: seg7:0x4d86 while DGROUP:0x57a8 != 0, the source cell, the speed limit 0x28, the
// step (not for a `fixed` icon), the sprite, and for an on-screen icon not `collided` the pair
// finder with DGROUP:0x943a = 0 and DGROUP:0x943c = i and seg7:0x4a38 on a hit; an off-screen icon
// takes seg7:0x3294 every third frame; every 30th frame a negative heat +0x2f rises toward 0.
void move_icons(UpdateCtx& c, World& w);
// seg7:0x3294(i): seg8:0x1d54(0x1e, i), then seg8:0x1b54(3, i) unless `fixed` [BYTES seg7:0x3294-0x32cd].
void icon_offscreen_step(UpdateCtx& c, int i);
// seg7:0x4a38(P, R, icon), cdecl [BYTES seg7:0x4a38-0x4d83].
void icon_hit(UpdateCtx& c, World& w, int pool, int rec, int icon);

}  // namespace is::world
