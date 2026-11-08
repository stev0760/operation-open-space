// ISPACE.DLL ordinal 15, the wing cut (src/gfx/wing_cut.h). 16-bit arithmetic as the
// original's (ADR 0013 decision 7).
#include "gfx/wing_cut.h"

#include <algorithm>

namespace is::gfx {

const std::array<std::int16_t, 5> kWingT1 = {-128, -118, -91, -50, 0};  // [BYTES ISPACE.DLL DGROUP:0x1a]
const std::array<std::int16_t, 5> kWingT2 = {0, 50, 91, 118, 128};      // [BYTES ISPACE.DLL DGROUP:0x10]

// Cell 3's left edges for rows 21 ... 67 as Wine's GDI draws Polygon (270,68)(270,89)(359,89)
// (359,21) with WHITE_PEN and WHITE_BRUSH into a 1-bpp bitmap [DESIGN: recorded from Wine 11.0
// and 11.20; tests/test_gfx_wing_cut_win32.cpp checks it] [BYTES ISPACE.DLL seg5:0x3821-0x3851].
const std::array<std::int16_t, 47> kWingCell3Left = {
    359, 357, 355, 353, 351, 349, 347, 345, 343, 342, 340, 338, 336, 334, 332, 330,
    328, 326, 324, 323, 321, 319, 317, 315, 313, 311, 309, 307, 306, 304, 302, 300,
    298, 296, 294, 292, 290, 288, 287, 285, 283, 281, 279, 277, 275, 273, 271};

namespace {

std::int16_t lo16(long v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xffff)); }

// cwd; sub ax, dx; sar ax, 1: a 16-bit value / 2 toward zero.
std::int16_t half16(std::int16_t v) { return static_cast<std::int16_t>((v - (v < 0 ? -1 : 0)) >> 1); }

WingMask build_mask() {
    WingMask m;
    m.bit.assign(static_cast<std::size_t>(kWingMaskW) * kWingMaskH, 0);  // BLACKNESS
    const auto set = [&m](int x, int y) {
        if (x >= 0 && y >= 0 && x < kWingMaskW && y < kWingMaskH)
            m.bit[static_cast<std::size_t>(y) * kWingMaskW + x] = 1;
    };
    for (int y = 0; y < 90; ++y)
        for (int x = 45; x < 45 + 46; ++x) set(x, y);  // [BYTES ISPACE.DLL seg5:0x3767-0x3778]
    for (int y = 45; y < 45 + 46; ++y)
        for (int x = 360; x < 360 + 90; ++x) set(x, y);  // row 90 clipped [BYTES ISPACE.DLL seg5:0x377d-0x378f]
    for (int y = 0; y < kWingMaskH; ++y) {
        // Polygon (112,89)(179,89)(179,0)(157,0) [BYTES ISPACE.DLL seg5:0x37ed-0x381c] [DESIGN Wine GDI]
        for (int x = 157 - (y + 1) / 2; x <= 179; ++x) set(x, y);
        // Polygon (180,89)(269,89)(269,0) [BYTES ISPACE.DLL seg5:0x37bc-0x37e8] [DESIGN Wine GDI]
        for (int x = 269 - y; x <= 269; ++x) set(x, y);
        // Polygon (270,68)(270,89)(359,89)(359,21) [BYTES ISPACE.DLL seg5:0x3821-0x3851] [DESIGN Wine GDI]
        if (y >= 21) {
            const int left = y <= 67 ? kWingCell3Left[static_cast<std::size_t>(y - 21)] : 270;
            for (int x = left; x <= 359; ++x) set(x, y);
        }
    }
    return m;
}

// The destination pixel (dx, dy) of a cell at (x0, y0) for source offset (i, j) of the window.
void place(WingOrient o, int s, int i, int j, int& dx, int& dy) {
    const bool mx = o == WingOrient::kMirrorX || o == WingOrient::kTurn;
    const bool my = o == WingOrient::kMirrorY || o == WingOrient::kTurn;
    dx = mx ? s - 1 - i : i;
    dy = my ? s - 1 - j : j;
}

// For each window pixel the cell-local destination and the mask bit; outside the mask the
// source is clipped and the destination left as it is [DESIGN, as the flame strip's clipped
// source in world/player_sheet.cpp].
template <typename F>
void each_window_pixel(int s, int D, int f, bool cut, const F& fn) {
    const int k = wing_cell(f);
    if (k < 0 || s <= 0) return;
    const WingWindow win = wing_window(s, D, k);
    const WingOrient o = wing_orient(f, cut);
    const WingMask& m = wing_mask();
    for (int j = 0; j < win.h; ++j)
        for (int i = 0; i < win.w; ++i) {
            const int mx = win.sx + i, my = win.sy + j;
            if (mx < 0 || my < 0 || mx >= kWingMaskW || my >= kWingMaskH) continue;
            int dx = 0, dy = 0;
            place(o, s, i, j, dx, dy);
            if (dx < 0 || dy < 0 || dx >= s || dy >= s) continue;
            fn(dx, dy, m.at(mx, my));
        }
}

}  // namespace

const WingMask& wing_mask() {
    static const WingMask m = build_mask();  // built once, while DGROUP:0x186 = 0 [BYTES ISPACE.DLL seg5:0x371a]
    return m;
}

WingWindow wing_window(int s, int D, int k) {
    WingWindow w;
    if (k < 0 || k > 4) return w;
    const std::int16_t o = lo16(45 - half16(static_cast<std::int16_t>(s)));             // [BYTES ISPACE.DLL seg5:0x11b3-0x11c0]
    const std::int16_t A = static_cast<std::int16_t>(lo16(static_cast<long>(kWingT1[static_cast<std::size_t>(k)]) * D) >> 7);  // [BYTES ISPACE.DLL seg5:0x11cd-0x11dd]
    const std::int16_t B = static_cast<std::int16_t>(lo16(static_cast<long>(kWingT2[static_cast<std::size_t>(k)]) * D) >> 7);  // [BYTES ISPACE.DLL seg5:0x11e0-0x11ea]
    w.sx = lo16(lo16(90L * k) - A + o);  // [BYTES ISPACE.DLL seg5:0x12e2-0x12f2]
    w.sy = lo16(B + o);                  // [BYTES ISPACE.DLL seg5:0x12f5-0x12fb]
    const std::int16_t ww = lo16(A - o + 0x59);  // [BYTES ISPACE.DLL seg5:0x12fe-0x130f]
    const std::int16_t hh = lo16(0x59 - w.sy);   // [BYTES ISPACE.DLL seg5:0x1312-0x1321]
    w.w = ww <= s ? ww : s;  // jle [BYTES ISPACE.DLL seg5:0x1307]
    w.h = hh <= s ? hh : s;  // jle [BYTES ISPACE.DLL seg5:0x1319]
    return w;
}

int wing_cell(int f) {
    static constexpr std::array<int, 16> k = {0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1};  // [BYTES ISPACE.DLL seg5:0x12b0]
    return f >= 0 && f < 16 ? k[static_cast<std::size_t>(f)] : -1;
}

WingOrient wing_orient(int f, bool cut) {
    if (cut) {
        if (f < 5) return WingOrient::kIdentity;   // [BYTES ISPACE.DLL seg5:0x14fe seg5:0x133e]
        if (f <= 8) return WingOrient::kMirrorX;   // [BYTES ISPACE.DLL seg5:0x140b seg5:0x150c seg5:0x1385]
        if (f < 12) return WingOrient::kTurn;      // [BYTES ISPACE.DLL seg5:0x144d seg5:0x139d]
        return WingOrient::kMirrorY;               // [BYTES ISPACE.DLL seg5:0x1483 seg5:0x13b3 seg5:0x1351]
    }
    if (f < 5) return WingOrient::kTurn;           // [BYTES ISPACE.DLL seg5:0x175c seg5:0x15fd]
    if (f < 8) return WingOrient::kMirrorY;        // [BYTES ISPACE.DLL seg5:0x165a]
    if (f <= 12) return WingOrient::kIdentity;     // [BYTES ISPACE.DLL seg5:0x168f seg5:0x176a seg5:0x1610 seg5:0x15b4]
    return WingOrient::kMirrorX;                   // [BYTES ISPACE.DLL seg5:0x16cb]
}

std::vector<std::uint8_t> wing_keep(int s, int D, int f, bool cut) {
    std::vector<std::uint8_t> keep(s > 0 ? static_cast<std::size_t>(s) * s : 0, 1);
    each_window_pixel(s, D, f, cut, [&](int x, int y, bool white) {
        if (!white) keep[static_cast<std::size_t>(y) * s + x] = 0;  // SRCAND with black
    });
    return keep;
}

void wing_cut_sheet(std::vector<std::uint8_t>& px, int width, int height, int s, int g, int D, bool cut) {
    if (width <= 0 || height <= 0 || px.size() < static_cast<std::size_t>(width) * height) return;
    for (int f = 0; f < 16; ++f) {
        const int x0 = lo16(static_cast<long>(lo16(g + s)) * f + g);  // [BYTES ISPACE.DLL seg5:0x14d6-0x14fe]
        const int y0 = g;
        each_window_pixel(s, D, f, cut, [&](int x, int y, bool white) {
            const int tx = x0 + x, ty = y0 + y;
            if (white || tx < 0 || ty < 0 || tx >= width || ty >= height) return;
            px[static_cast<std::size_t>(ty) * width + tx] = 0;  // 0x008800C6
        });
    }
}

std::vector<std::uint8_t> wing_debris_cell(const std::vector<std::uint8_t>& px, int width, int height, int s,
                                           int g, int D, int f, bool cut) {
    std::vector<std::uint8_t> c(s > 0 ? static_cast<std::size_t>(s) * s : 0, 0);  // BLACKNESS [BYTES ISPACE.DLL seg5:0x124c]
    if (s <= 0) return c;
    // BitBlt(dc, 0, 0, s, s, sheet, (g + s) f + g, g, SRCCOPY) [BYTES ISPACE.DLL seg5:0x1251-0x1276]
    const int sx = lo16(static_cast<long>(lo16(g + s)) * f + g);
    const bool ok = width > 0 && height > 0 && px.size() >= static_cast<std::size_t>(width) * height;
    for (int y = 0; y < s && ok; ++y)
        for (int x = 0; x < s; ++x) {
            const int ux = sx + x, uy = g + y;
            if (ux < 0 || uy < 0 || ux >= width || uy >= height) continue;
            c[static_cast<std::size_t>(y) * s + x] = px[static_cast<std::size_t>(uy) * width + ux];
        }
    each_window_pixel(s, D, f, cut, [&](int x, int y, bool white) {
        if (white) c[static_cast<std::size_t>(y) * s + x] = 0;  // 0x00220326 DSna
    });
    return c;
}

}  // namespace is::gfx
