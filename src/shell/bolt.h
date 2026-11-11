// The lightning bolt seg11:0xa4a6 (docs/re/shell.md "Credits", Bolt shape), shared by the title's
// bolt (src/intro, seg11:0xa4a6(x1, y1, x2, y2, 0xe)) and the wave's LIGHTNING (src/world/
// lightning.h, seg11:0xa4a6 from the strike seg11:0xa234): the point rules, the Newton step
// seg8:0x1290 the length takes, and the trivial rejection of the clipped line seg12:0x7256 the
// intro draws with. It lives in the shell so that the wave uses it without including the intro;
// src/intro/bolt.h gives the intro its names for it (ruling R6 of M4 stage 7) [DESIGN]. Portable.
//
// The points are a template on the caller's point type P (any struct with int-assignable `x`,
// `y`): the intro keeps its own Point, the wave takes BoltPoint below [DESIGN].
#pragma once

#include <array>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>

#include "shell/rng.h"

namespace is::shell {

// The tables seg11:0xa4a6 fills: P0 ... PN (DGROUP:0x6b5c + 4i), the branches (DGROUP:0x6bbc +
// 20b, 5 points each, count DGROUP:0x9526) and the sub-branches (DGROUP:0x6cfc + 12s, 3 points
// each, count DGROUP:0x9528) [BYTES seg11:0xa897 seg11:0xa8dc seg11:0xa95a seg11:0xa99e].
template <class P>
struct BasicBolt {
    std::vector<P> main;
    std::vector<std::array<P, 5>> branches;
    std::vector<std::array<P, 3>> subs;
};

// A point of the wave's bolt (the screen point seg8:0x29ea returns).
struct BoltPoint {
    int x = 0, y = 0;
    bool operator==(const BoltPoint& o) const { return x == o.x && y == o.y; }
};

// seg8:0x1290(g, q) as the bolt calls it: ((q / max(g, 3)) + max(g, 3)) / 2, one Newton step of
// the square root [BYTES seg8:0x1290].
int bolt_newton(long g, long q);

namespace bolt_detail {
// The RTL's long division seg5:0x110: truncation toward zero. The bolt divides by its length
// L >= 1 only; 0 gives 0 [DESIGN].
inline int ldiv(long a, long b) { return b == 0 ? 0 : static_cast<int>(a / b); }
}  // namespace bolt_detail

// seg11:0xa4a6's points for a bolt from (x1, y1) to (x2, y2), with the draws from `rng` (the
// caller seeds it first, as seg11:0xa4bb does). Returns false when the routine draws nothing: a
// length L with L / 50 > 22, or a 17th branch; `out` is then left as it was [BYTES seg11:0xa529
// seg11:0xa537 seg11:0xa53c seg11:0xa54d seg11:0xa559 seg11:0xa561 seg11:0xa56e seg11:0xa585
// seg11:0xa5c6 seg11:0xa614 seg11:0xa66c seg11:0xa67b seg11:0xa684 seg11:0xa696 seg11:0xa6ac
// seg11:0xa752 seg11:0xa7a9 seg11:0xa7b8 seg11:0xa7d5 seg11:0xa83a seg11:0xa884 seg11:0xa897].
template <class P>
bool make_bolt(Rng& rng, int x1, int y1, int x2, int y2, BasicBolt<P>* out) {
    using bolt_detail::ldiv;
    const int dx = x2 - x1, dy = y2 - y1, ndy = -dy;
    const long q = static_cast<long>(dy) * dy + static_cast<long>(dx) * dx;
    const int l = bolt_newton(static_cast<long>(std::abs(dx)) + std::abs(dy), q);
    int seg = 30;
    if (!(l / 30 < 23)) {
        seg = 50;
        if (!(l / 50 < 23)) return false;
    }
    const int n = l / seg;
    BasicBolt<P> b;
    b.main.resize(static_cast<std::size_t>(n) + 1);
    for (int i = 0; i < n; ++i) {
        P& p = b.main[static_cast<std::size_t>(i)];
        if (i < 1) {
            p.x = x1;
            p.y = y1;
        } else {
            const int along = (i + 1) * seg;
            const int r1 = rng.rand() % 10;
            const int ax = ldiv(static_cast<long>(r1 + along - 5) * dx, l);
            const int r2 = rng.rand() % 26;
            const int px = ldiv(static_cast<long>(r2 - 13) * ndy, l);
            p.x = px + ax + x1;
            const int r3 = rng.rand() % 10;
            const int ay = ldiv(static_cast<long>(r3 + along - 5) * dy, l);
            const int r4 = rng.rand() % 26;
            const int py = ldiv(static_cast<long>(r4 - 13) * dx, l);
            p.y = ay + py + y1;
        }
        if (rng.rand() % 3 != 0) continue;
        // A branch from P_i: 5 points [BYTES seg11:0xa585-0xa752].
        std::array<P, 5> br{};
        br[0] = p;
        for (int k = 1; k < 5; ++k) {
            const int off = ((i % 2) * 32 - 16) * k;
            const int r1 = rng.rand() % 24;
            const int perp_x = ldiv(static_cast<long>(off + r1 - 12) * ndy, l);
            const int r2 = rng.rand() % 15;
            const int along_x = ldiv(static_cast<long>(r2 + k * 15) * dx, l);
            br[static_cast<std::size_t>(k)].x = along_x + perp_x + p.x;
            const int r3 = rng.rand() % 24;
            const int perp_y = ldiv(static_cast<long>(r3 + off - 12) * dx, l);
            const int r4 = rng.rand() % 15;
            const int along_y = ldiv(static_cast<long>(r4 + k * 15) * dy, l);
            br[static_cast<std::size_t>(k)].y = perp_y + along_y + p.y;
        }
        b.branches.push_back(br);
        if (b.branches.size() > 16) return false;  // seg11:0xa75c
        if (rng.rand() % 5 >= 4) continue;
        // A sub-branch from the branch's point 2: 3 points [BYTES seg11:0xa7a9-0xa7d1].
        std::array<P, 3> sb{};
        sb[0] = br[2];
        for (int m = 1; m < 3; ++m) {
            const int off = ((m + 2) * (i % 2) * 2 - m) * 12;
            const int r1 = rng.rand() % 16;
            const int perp_x = ldiv(static_cast<long>(off + r1 - 32) * ndy, l);
            const int base = (m + 2) * 15;
            const int r2 = rng.rand() % 15;
            const int along_x = ldiv(static_cast<long>(r2 + base) * dx, l);
            sb[static_cast<std::size_t>(m)].x = along_x + perp_x + sb[0].x;
            const int r3 = rng.rand() % 16;
            const int perp_y = ldiv(static_cast<long>(r3 + off - 32) * dx, l);
            const int r4 = rng.rand() % 15;
            const int along_y = ldiv(static_cast<long>(r4 + base) * dy, l);
            sb[static_cast<std::size_t>(m)].y = perp_y + along_y + sb[0].y;
        }
        b.subs.push_back(sb);
    }
    b.main[static_cast<std::size_t>(n)].x = x2;
    b.main[static_cast<std::size_t>(n)].y = y2;
    *out = std::move(b);
    return true;
}

// seg12:0x7256's rejection: false when both ends lie beyond the same edge of (0, 0)-(w,
// DGROUP:0x90ec) [BYTES seg12:0x7256]. The intro draws lines that cross an edge whole and lets
// GDI clip them [DESIGN: the intro does not port the routine's own edge intersection; the wave
// draws through src/world/line.h clip_line, which does].
bool bolt_line_visible(int ax, int ay, int bx, int by, int w, int play_h);

}  // namespace is::shell
