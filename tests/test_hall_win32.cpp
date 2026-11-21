// seg15:0x28b2, the dead ship's marks (src/hall/hall_gdi.h dead_ship_marks), on memory DCs: the
// thousand rand() draws in the original's order, the marks inside the square, the zero divisor.
#ifdef _WIN32
#include <windows.h>

#include <doctest.h>

#include "hall/hall_gdi.h"
#include "shell/rng.h"

using namespace is;

namespace {
struct Dc {
    HDC dc = nullptr;
    HBITMAP bm = nullptr;
    HGDIOBJ old = nullptr;
    Dc(int w, int h) {
        dc = CreateCompatibleDC(nullptr);
        bm = CreateCompatibleBitmap(dc, w, h);
        old = SelectObject(dc, bm);
        RECT r{0, 0, w, h};
        FillRect(dc, &r, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    }
    ~Dc() {
        SelectObject(dc, old);
        DeleteObject(bm);
        DeleteDC(dc);
    }
};
}  // namespace

TEST_CASE("hall: seg15:0x28b2 draws 1000 rand() and marks both squares") {
    const int k = 2, sz = 30;  // a 60 x 60 square
    Dc q(k * sz, k * sz), q2(k * sz, k * sz);
    shell::Rng rng;
    rng.srand(1234);
    shell::Rng ref;
    ref.srand(1234);
    // The first mark as the listing computes it: x = rand() % 57 + 3, y = rand() % 44.
    const int x0 = ref.rand() % (k * sz - 3) + 3;
    const int y0 = ref.rand() % ((sz - 8) * k);
    for (int i = 2; i < 1000; ++i) ref.rand();
    CHECK(hall::dead_ship_marks(q.dc, q2.dc, k, sz, rng, {}) == 1000);
    CHECK(rng.state() == ref.state());
    CHECK(x0 >= 3);
    CHECK(y0 < 44);
    // Nothing below row (sz - 8)·k + 5 is touched.
    int dark = 0, below = 0, differ = 0;
    for (int y = 0; y < k * sz; ++y)
        for (int x = 0; x < k * sz; ++x) {
            const bool d = GetPixel(q.dc, x, y) != RGB(255, 255, 255);
            if (d) ++dark;
            if (d && y >= (sz - 8) * k + 6) ++below;
            if (GetPixel(q.dc, x, y) != GetPixel(q2.dc, x, y)) ++differ;
        }
    CHECK(dark > 0);
    CHECK(below == 0);
    CHECK(differ == 0);
}

TEST_CASE("hall: seg15:0x28b2 with a zero divisor draws nothing") {
    Dc q(16, 16), q2(16, 16);
    shell::Rng rng;
    rng.srand(5);
    const std::uint32_t s0 = rng.state();
    CHECK(hall::dead_ship_marks(q.dc, q2.dc, 2, 8, rng, {}) == 0);  // (sz - 8)·k = 0
    CHECK(rng.state() == s0);
}
#endif
