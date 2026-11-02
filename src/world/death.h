// A dead ship's own changes (docs/re/core.md "Death and the explosion"; work/analysis/m4/
// s1-death.md section 3 with s1-death-verify.md claims 28-29, 33-35): the flames burnt into its
// sprite sheet, seg13:0x4710, and the pod it drops, seg13:0x498e with its fill seg13:0x4ba0.
// seg16:0x8e4 (effects.h ship_destroyed) calls both. M4 stage 1, track DEATH.
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <cstdint>

#include "world/effects.h"

namespace is::world {

// seg13:0x4710(ship) [BYTES seg13:0x4710-0x498d] (verified claims 28, 29):
//  - seg10:0x754a(0x6a, DGROUP:0x9324, ship, 1);
//  - graphic #230 through seg10:0x6c02(0xe6, 0, -1) and its mask (seg10:0x6ec6); the rescale
//    seg10:0x6308(-1, -1, bitmap, -1) at Size != 1 is not ported [OPEN];
//  - the mask of the ship's sheet, image slot 0x28 + ship (DGROUP:0x3a38 + 2 ship), SRCAND with
//    itself shifted up by (9 << Size) >> 1 rows over 16 size + 17 gap x size + 2 gap: the
//    silhouette and its copy 9 px higher;
//  - for heading h = 0 ... 15 at (x, y) = ((gap + size) h + gap, gap): flame cell h % 5 (side
//    q = min(size, p), p = (32 << Size) >> 1, at offset o = max(0, (size - p) / 2)) with SRCAND
//    from the flames' mask then SRCPAINT, then DSna (0x220326) of the size x size cell from the
//    shifted sheet mask: everything outside the silhouette and its raised copy goes black.
// The sheet stays burnt for the rest of the wave. Nothing is drawn without a Blitter, images or
// a sheet DC [DESIGN]. The created images are deleted at the end, as the original's DCs and
// bitmaps are.
void burn_ship_sheet(EffectCtx& c, int ship);

// seg13:0x4ba0(ship, rec, flag) [BYTES seg13:0x4ba0-0x4ec9] (verified claims 34, 35): pod record
// rec (EffectState::pods) and pool-11 record rec from the ship: outside a Demon's Gate wave or for
// the player, flag != 0 moves the weapon slots 1 ... 9 below byte +0xcb (bytes +0xb8 + j and
// +0xae + j, -1 for unused, all cleared on the ship) and all `resources` +0xce (kind 1, byte +0xcb
// = 1), flag 0 moves min(+0xce, 400) (kind 3); a Demon's Gate computer ship gives kind 2 and
// nothing; object +0x4a = 0x1d either way. The record: `size` (24 << Size) >> 1, `gap`
// (4 << Size) >> 1, `extent` 0xb8, `state` 0, at the ship's centre minus 92 plus the heading
// vector of (heading + 8) & 15 times the ship's extent / 16, wrapped by % 25600 (signed), moving
// at the ship's velocity plus 5 times that vector, `mass` 0x65, bytes +0x53 and +0x51 0; then
// seg7:0x2080(11, -1).
void fill_pod(EffectCtx& c, int ship, int rec, int flag);

// seg13:0x498e(ship, flag) [BYTES seg13:0x498e-0x4ac3] (verified claim 33): the first pool-11
// record (DGROUP:0x1e24, allocated count DGROUP:0x9302) whose `state` != 0 gets the pod; when none,
// the record with the largest pod word DGROUP:0x8564 (> 0) has its `on_destroy` called, gets the
// pod and seg7:0x2080(11, -1) follows (with every word <= 0 the original uses an unset index: the
// port drops no pod then [DESIGN]). With flag != 0 the first live ship whose design word
// DGROUP:0x970a is 3, or 7 with ship byte +0x5e > 1, is sent after it: words +0x52 = 0, +0x32 =
// +0x06 = 11, +0x34 = +0x08 = rec, +0x38 = +0x3c = 2. The pool-11 mover seg12:0x6b1e and the
// pickup are not ported: the pod lies inert [OPEN M4 stage 2].
void drop_pod(EffectCtx& c, int ship, int flag);

}  // namespace is::world
