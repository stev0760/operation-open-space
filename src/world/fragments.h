// Pool 1, the icon fragments (work/analysis/m4/s3-fuel.md section 4 with s3-fuel-verify.md claims
// 38-46): the mover seg7:0x32d0 and its off-screen step seg7:0x3400. A fragment lives (`state` 0)
// until its damage passes `max_damage` 14 through the generic on_damage seg7:0x2652, whose
// on_destroy seg7:0x2e02 is effects.h pickup_destroyed; there is no timer. M4 stage 3, track FUEL.
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <cstdint>

#include "world/world.h"

namespace is::world {

struct UpdateCtx;

// seg7:0x3400(i), Pascal [BYTES seg7:0x3400-0x342b] (verified claim 40): seg8:0x1d54(h, 0x28) and
// seg8:0x1b54(h, 3).
void fragment_offscreen_step(UpdateCtx& c, int i);

// seg7:0x32d0 [BYTES seg7:0x32d0-0x33f0] (verified claims 39-42): per record below DGROUP:0x9408
// in `state` 0: off screen or without its bitmap DGROUP:0x4e94 + 2i it takes seg7:0x3400 every
// third frame (DGROUP:0x9142 % 3 = 0); else `src_x` = u(6), drag 0x28 and one step, the draw
// kind 3 from its own bitmap selected into DGROUP:0x9288, and when not `collided` the pair finder
// with DGROUP:0x943a = 1 and on a hit the bounce and the dispatch with the fragment as T. No
// ship-touch branch and no 125 case.
void move_fragments(UpdateCtx& c, World& w);

}  // namespace is::world
