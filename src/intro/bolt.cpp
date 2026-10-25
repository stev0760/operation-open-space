#include "intro/bolt.h"

#include <cstdlib>

namespace is::intro {

int newton_root(long g, long q) {
    if (g < 3) g = 3;
    return static_cast<int>((q / g + g) >> 1);
}

namespace {
// The RTL's long division seg5:0x110: truncation toward zero.
int ldiv_(long a, long b) { return b == 0 ? 0 : static_cast<int>(a / b); }
}  // namespace

bool make_bolt(shell::Rng& rng, int x1, int y1, int x2, int y2, Bolt* out) {
    const int dx = x2 - x1, dy = y2 - y1, ndy = -dy;
    const long q = static_cast<long>(dy) * dy + static_cast<long>(dx) * dx;
    const int l = newton_root(static_cast<long>(std::abs(dx)) + std::abs(dy), q);
    int seg = 30;
    if (!(l / 30 < 23)) {
        seg = 50;
        if (!(l / 50 < 23)) return false;
    }
    const int n = l / seg;
    Bolt b;
    b.main.resize(static_cast<std::size_t>(n) + 1);
    for (int i = 0; i < n; ++i) {
        Point& p = b.main[static_cast<std::size_t>(i)];
        if (i < 1) {
            p = {x1, y1};
        } else {
            const int along = (i + 1) * seg;
            const int r1 = rng.rand() % 10;
            const int ax = ldiv_(static_cast<long>(r1 + along - 5) * dx, l);
            const int r2 = rng.rand() % 26;
            const int px = ldiv_(static_cast<long>(r2 - 13) * ndy, l);
            p.x = px + ax + x1;
            const int r3 = rng.rand() % 10;
            const int ay = ldiv_(static_cast<long>(r3 + along - 5) * dy, l);
            const int r4 = rng.rand() % 26;
            const int py = ldiv_(static_cast<long>(r4 - 13) * dx, l);
            p.y = ay + py + y1;
        }
        if (rng.rand() % 3 != 0) continue;
        // A branch from P_i: 5 points [BYTES seg11:0xa585-0xa752].
        std::array<Point, 5> br{};
        br[0] = p;
        for (int k = 1; k < 5; ++k) {
            const int off = ((i % 2) * 32 - 16) * k;
            const int r1 = rng.rand() % 24;
            const int perp_x = ldiv_(static_cast<long>(off + r1 - 12) * ndy, l);
            const int r2 = rng.rand() % 15;
            const int along_x = ldiv_(static_cast<long>(r2 + k * 15) * dx, l);
            br[static_cast<std::size_t>(k)].x = along_x + perp_x + p.x;
            const int r3 = rng.rand() % 24;
            const int perp_y = ldiv_(static_cast<long>(r3 + off - 12) * dx, l);
            const int r4 = rng.rand() % 15;
            const int along_y = ldiv_(static_cast<long>(r4 + k * 15) * dy, l);
            br[static_cast<std::size_t>(k)].y = perp_y + along_y + p.y;
        }
        b.branches.push_back(br);
        if (b.branches.size() > 16) return false;  // seg11:0xa75c
        if (rng.rand() % 5 >= 4) continue;
        // A sub-branch from the branch's point 2: 3 points [BYTES seg11:0xa7a9-0xa7d1].
        std::array<Point, 3> sb{};
        sb[0] = br[2];
        for (int m = 1; m < 3; ++m) {
            const int off = ((m + 2) * (i % 2) * 2 - m) * 12;
            const int r1 = rng.rand() % 16;
            const int perp_x = ldiv_(static_cast<long>(off + r1 - 32) * ndy, l);
            const int base = (m + 2) * 15;
            const int r2 = rng.rand() % 15;
            const int along_x = ldiv_(static_cast<long>(r2 + base) * dx, l);
            sb[static_cast<std::size_t>(m)].x = along_x + perp_x + sb[0].x;
            const int r3 = rng.rand() % 16;
            const int perp_y = ldiv_(static_cast<long>(r3 + off - 32) * dx, l);
            const int r4 = rng.rand() % 15;
            const int along_y = ldiv_(static_cast<long>(r4 + base) * dy, l);
            sb[static_cast<std::size_t>(m)].y = perp_y + along_y + sb[0].y;
        }
        b.subs.push_back(sb);
    }
    b.main[static_cast<std::size_t>(n)] = {x2, y2};
    *out = std::move(b);
    return true;
}

bool line_visible(Point a, Point b, int w, int play_h) {
    if (a.y < 0 && b.y < 0) return false;
    if (a.y > play_h && b.y > play_h) return false;
    if (a.x < 0 && b.x < 0) return false;
    if (a.x > w && b.x > w) return false;
    return true;
}

}  // namespace is::intro
