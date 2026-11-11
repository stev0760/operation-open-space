// The lightning bolt of the credits (docs/re/shell.md "Credits", Bolt shape): the point rules of
// seg11:0xa4a6 and the trivial rejection of the clipped line seg12:0x7256 on the intro's Point.
// The code lives in src/shell/bolt.h, shared with the wave's LIGHTNING (ruling R6 of M4 stage
// 7) [DESIGN]; these are the intro's names for it. Portable.
#pragma once

#include "intro/texts.h"
#include "shell/bolt.h"
#include "shell/rng.h"

namespace is::intro {

// P0 ... PN (DGROUP:0x6b5c), the branches (DGROUP:0x6bbc + 20 b, count DGROUP:0x9526) and the
// sub-branches (DGROUP:0x6cfc + 12 s, count DGROUP:0x9528) on the intro's Point.
using Bolt = shell::BasicBolt<Point>;

// seg8:0x1290(g, q): ((q / max(g, 3)) + max(g, 3)) / 2, one Newton step of the square root
// [BYTES seg8:0x1290].
inline int newton_root(long g, long q) { return shell::bolt_newton(g, q); }

// seg11:0xa4a6's points (shell/bolt.h make_bolt); the caller seeds `rng` first, as seg11:0xa4bb
// does.
inline bool make_bolt(shell::Rng& rng, int x1, int y1, int x2, int y2, Bolt* out) {
    return shell::make_bolt(rng, x1, y1, x2, y2, out);
}

// seg12:0x7256's rejection (shell/bolt.h bolt_line_visible) [BYTES seg12:0x7256].
inline bool line_visible(Point a, Point b, int w, int play_h) {
    return shell::bolt_line_visible(a.x, a.y, b.x, b.y, w, play_h);
}

}  // namespace is::intro
