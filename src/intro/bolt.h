// The lightning bolt of the credits (docs/re/shell.md "Credits", Bolt shape): the point rules of
// seg11:0xa4a6 and the trivial rejection of the clipped line routine seg12:0x7256. Portable.
#pragma once

#include <array>
#include <vector>

#include "intro/texts.h"
#include "shell/rng.h"

namespace is::intro {

struct Bolt {
    std::vector<Point> main;                  // P0 ... PN, DGROUP:0x6b5c
    std::vector<std::array<Point, 5>> branches;  // DGROUP:0x6bbc + 20 b, count DGROUP:0x9526
    std::vector<std::array<Point, 3>> subs;      // DGROUP:0x6cfc + 12 s, count DGROUP:0x9528
};

// seg8:0x1290(g, q): ((q / max(g, 3)) + max(g, 3)) / 2, one Newton step of the square root
// [BYTES seg8:0x1290].
int newton_root(long g, long q);

// seg11:0xa4a6's points for a bolt from (x1, y1) to (x2, y2), with the draws from `rng` (the
// caller seeds it first, as seg11:0xa4bb does). Returns false when the routine draws nothing: a
// length L with L / 50 > 22, or a 17th branch [BYTES seg11:0xa529 seg11:0xa537 seg11:0xa53c
// seg11:0xa54d seg11:0xa559 seg11:0xa561 seg11:0xa56e seg11:0xa585 seg11:0xa5c6 seg11:0xa614
// seg11:0xa66c seg11:0xa67b seg11:0xa684 seg11:0xa696 seg11:0xa6ac seg11:0xa752 seg11:0xa7a9
// seg11:0xa7b8 seg11:0xa7d5 seg11:0xa83a seg11:0xa884 seg11:0xa897].
bool make_bolt(shell::Rng& rng, int x1, int y1, int x2, int y2, Bolt* out);

// seg12:0x7256's rejection: false when both ends lie beyond the same edge of (0, 0)-(W,
// DGROUP:0x90ec) [BYTES seg12:0x7256]. Lines that cross an edge are drawn whole and clipped by
// GDI [DESIGN: the routine's own edge intersection is not ported].
bool line_visible(Point a, Point b, int w, int play_h);

}  // namespace is::intro
