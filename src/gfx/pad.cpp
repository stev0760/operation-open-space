// owner: M4 stage 6, track PAD.
// seg15:0x482e and seg15:0x4a2e (src/gfx/pad.h). 16-bit arithmetic as the original's (ADR 0013
// decision 7).
#include "gfx/pad.h"

#include <algorithm>
#include <cstdlib>

namespace is::gfx {

namespace {

constexpr std::int16_t to_i16(std::uint32_t v) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xffffu));
}
constexpr std::int16_t lo16(std::int32_t v) { return to_i16(static_cast<std::uint32_t>(v)); }
constexpr std::int16_t add16(std::int32_t a, std::int32_t b) { return lo16(a + b); }
constexpr std::int16_t sub16(std::int32_t a, std::int32_t b) { return lo16(a - b); }
// `idiv` of a 16-bit dividend (cwd) by a 16-bit divisor, toward zero; 0 for a zero divisor, where
// the original faults [DESIGN: ADR 0016 decision 4] (review L4).
constexpr std::int16_t idiv16(std::int32_t n, std::int32_t d) {
    return lo16(d) == 0 ? std::int16_t{0} : lo16(static_cast<std::int32_t>(lo16(n)) / lo16(d));
}
// cwd; sub ax, dx; sar ax, 1.
constexpr std::int16_t half16(std::int32_t v) {
    const std::int16_t w = lo16(v);
    return static_cast<std::int16_t>((w - (w < 0 ? -1 : 0)) >> 1);
}
// seg5:0x110, the C runtime's long division (toward zero); 0 for a zero divisor, where the original
// faults [DESIGN: ADR 0016 decision 4] (review L4).
constexpr std::int32_t ldiv32(std::int32_t n, std::int32_t d) { return d == 0 ? 0 : n / d; }

// ---- Wine's polygon raster (pad.h pad_mask) ------------------------------------------------------

// BRESINITPGON / BRESINCRPGON of Wine's region.c (the X11 scan converter) [REF Wine gdi32
// region.c].
struct Edge {
    int x = 0, d = 0, m = 0, m1 = 0, incr1 = 0, incr2 = 0;
};
Edge edge_init(int dy, int x1, int x2) {
    Edge e;
    e.x = x1;
    const int dx = x2 - x1;
    e.m = dx / dy;
    if (dx < 0) {
        e.m1 = e.m - 1;
        e.incr1 = -2 * dx + 2 * dy * e.m1;
        e.incr2 = -2 * dx + 2 * dy * e.m;
        e.d = 2 * e.m * dy - 2 * dx - 2 * dy;
    } else {
        e.m1 = e.m + 1;
        e.incr1 = 2 * dx - 2 * dy * e.m1;
        e.incr2 = 2 * dx - 2 * dy * e.m;
        e.d = -2 * e.m * dy + 2 * dx;
    }
    return e;
}
void edge_step(Edge& e) {
    const bool up = e.m1 > 0 ? e.d > 0 : e.d >= 0;
    if (up) {
        e.x += e.m1;
        e.d += e.incr1;
    } else {
        e.x += e.m;
        e.d += e.incr2;
    }
}

// dibdrv's get_octant_number [REF Wine gdi32 dibdrv objects.c].
int octant(int dx, int dy) {
    if (dy > 0) return dx > 0 ? (dx > dy ? 1 : 2) : (-dx > dy ? 4 : 3);
    return dx < 0 ? (-dx > -dy ? 5 : 6) : (dx > -dy ? 8 : 7);
}

// dibdrv's solid_pen_line for a 1-px cosmetic pen: horizontal and vertical runs from the start
// to before the end (order_end_points), else the Bresenham walk of max(|dx|, |dy|) pixels from
// the start with err = 2 minor - major and the bias of octants 3, 5, 6 and 8 [REF Wine gdi32
// dibdrv objects.c solid_pen_line, init_bres_params; primitives.c solid_line]. Pixels outside
// the bitmap are dropped: dibdrv's clip_line keeps the walk's pixels inside the clip [DESIGN].
template <class Plot>
void pen_line(int sx, int sy, int ex, int ey, Plot plot) {
    if (sy == ey) {
        int l = sx, r = ex;
        if (l > r) {
            const int t = l + 1;
            l = r + 1;
            r = t;
        }
        for (int x = l; x < r; ++x) plot(x, sy);
        return;
    }
    if (sx == ex) {
        int t = sy, b = ey;
        if (t > b) {
            const int u = t + 1;
            t = b + 1;
            b = u;
        }
        for (int y = t; y < b; ++y) plot(sx, y);
        return;
    }
    const int dx = ex - sx, dy = ey - sy, adx = std::abs(dx), ady = std::abs(dy);
    const unsigned mask = 1u << (octant(dx, dy) - 1);
    const int bias = (mask & 0xb4u) != 0 ? 1 : 0;
    const bool x_major = (mask & (1u | 8u | 16u | 128u)) != 0;
    const int x_inc = (mask & (1u | 2u | 64u | 128u)) != 0 ? 1 : -1;
    const int y_inc = (mask & (1u | 2u | 4u | 8u)) != 0 ? 1 : -1;
    int x = sx, y = sy;
    if (x_major) {
        int err = 2 * ady - adx;
        for (int n = adx; n > 0; --n) {
            plot(x, y);
            if (err + bias > 0) {
                y += y_inc;
                err += 2 * ady - 2 * adx;
            } else {
                err += 2 * ady;
            }
            x += x_inc;
        }
    } else {
        int err = 2 * adx - ady;
        for (int n = ady; n > 0; --n) {
            plot(x, y);
            if (err + bias > 0) {
                x += x_inc;
                err += 2 * adx - 2 * ady;
            } else {
                err += 2 * adx;
            }
            y += y_inc;
        }
    }
}

}  // namespace

PadGeometry pad_geometry(int play_h, int client_h) {
    PadGeometry g;
    g.play_h = play_h;
    g.client_h = client_h;
    // shl ax, 2; cwd; idiv 3; sub 0x44c [BYTES seg15:0x483d seg15:0x4840 seg15:0x4847 seg15:0x484b]
    g.q = idiv16(lo16(play_h * 4), 3);
    g.k = sub16(g.q, 1100);
    g.top = add16(idiv16(g.k, 22), 139);  // DGROUP:0x3ce4 [BYTES seg15:0x4856 seg15:0x485b]
    g.pw = add16(idiv16(g.k, 36), 164);   // DGROUP:0x3ce6 [BYTES seg15:0x4868 seg15:0x486d]
    g.pc = add16(idiv16(g.k, 30), 80);    // DGROUP:0x3c96, 0x3c9a [BYTES seg15:0x48ad-0x48c2]
    g.poly[0] = {sub16(half16(g.pw), half16(g.top)), 1};  // [BYTES seg15:0x4876-0x489f]
    g.poly[1] = {add16(half16(g.top), half16(g.pw)), 1};  // [BYTES seg15:0x4893-0x48a3]
    g.poly[2] = {g.pw, g.pc};                             // [BYTES seg15:0x48a7 seg15:0x48bf]
    g.poly[3] = {0, g.pc};                                // DGROUP:0x3c98 = 0 [BYTES seg15:0x48f7 seg15:0x48c2]
    // dec ax; neg ax; imul ax, ax, 0x64; cwd; idiv di, and the same by P1.x - pw [BYTES
    // seg15:0x48c5-0x48e1].
    const std::int16_t num = lo16(100 * static_cast<std::int32_t>(sub16(1, g.pc)));
    g.m1 = idiv16(num, g.poly[0].x);
    g.m2 = idiv16(num, sub16(g.poly[1].x, g.pw));
    g.x_left = sub16(half16(g.q), half16(g.pw));  // DGROUP:0x3c9c [BYTES seg15:0x48e6-0x48f2]
    // The sext32 products and differences through seg5:0x314 and seg5:0x110 [BYTES
    // seg15:0x4902-0x4975].
    const std::int32_t dm = static_cast<std::int32_t>(g.m1) - static_cast<std::int32_t>(g.m2);
    g.vx = lo16(ldiv32(static_cast<std::int32_t>(g.x_left) * dm - static_cast<std::int32_t>(g.pw) * g.m2, dm));
    const std::int32_t dx = sub16(g.vx, g.x_left);
    g.vy = sub16(add16(lo16(ldiv32(static_cast<std::int32_t>(g.m1) * dx, 100)), client_h), 0x34);
    return g;
}

std::vector<std::uint8_t> pad_mask(const PadGeometry& g) {
    const int w = g.pw, h = g.pc;
    if (w <= 0 || h <= 0) return {};
    std::vector<std::uint8_t> m(static_cast<std::size_t>(w) * h, 0);  // PatBlt BLACKNESS
    const PadPoint p0 = g.poly[0], p1 = g.poly[1], p2 = g.poly[2], p3 = g.poly[3];
    // The interior: edges P3-P0 (top P0) and P1-P2 (top P1), horizontal edges ignored, scanlines
    // from the top y to the bottom y - 1 [REF Wine gdi32 region.c].
    if (p3.y > p0.y && p2.y > p1.y) {
        Edge L = edge_init(p3.y - p0.y, p0.x, p3.x), R = edge_init(p2.y - p1.y, p1.x, p2.x);
        for (int y = p0.y; y < p3.y; ++y) {
            int a = L.x, b = R.x;
            if (a > b) std::swap(a, b);
            if (y >= 0 && y < h)
                for (int x = std::max(a, 0); x < std::min(b, w); ++x) m[static_cast<std::size_t>(y) * w + x] = 1;
            if (y < p3.y - 1) {
                edge_step(L);
                edge_step(R);
            }
        }
    }
    // The outline with the default BLACK_PEN, P0 -> P1 -> P2 -> P3 -> P0 [REF Wine gdi32 dibdrv
    // objects.c solid_pen_lines].
    const auto plot = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < w && y < h) m[static_cast<std::size_t>(y) * w + x] = 0;
    };
    for (int i = 0; i < 4; ++i) {
        const PadPoint a = g.poly[static_cast<std::size_t>(i)], b = g.poly[static_cast<std::size_t>((i + 1) % 4)];
        pen_line(a.x, a.y, b.x, b.y, plot);
    }
    return m;
}

PadPicture pad_picture(const PadGeometry& g) {
    PadPicture p;
    if (g.pw <= 0 || g.pc <= 0) return p;
    p.w = g.pw;
    p.h = g.height();
    p.grey.assign(static_cast<std::size_t>(p.w) * p.h, 0);
    const std::vector<std::uint8_t> mask = pad_mask(g);
    const auto fill = [&](int x, int y, int w, int h, int v) {  // PatBlt PATCOPY, clipped
        for (int yy = std::max(y, 0); yy < std::min(y + h, p.h); ++yy)
            for (int xx = std::max(x, 0); xx < std::min(x + w, p.w); ++xx)
                p.grey[static_cast<std::size_t>(yy) * p.w + xx] = static_cast<std::uint8_t>(v);
    };
    // The bands [BYTES seg15:0x4aa6-0x4b46] (verified claims 10-12).
    const int step = idiv16(1700, g.pc);
    int val = 450;
    for (int y = 0; sub16(g.pc, 2) > y; y = add16(y, 2)) {
        const int v = idiv16(val, 10) & 0xff;  // PALETTERGB(v, v, v), the low byte [BYTES seg15:0x4ad1-0x4aee]
        // BitBlt(pad, 0, y, pw, 2, mask, 0, y, MERGECOPY): P AND S, the mono source's 1 the
        // destination's background white [BYTES seg15:0x4afc-0x4b19] [REF Win16 SDK BitBlt].
        for (int yy = y; yy < y + 2 && yy < g.pc; ++yy)
            for (int x = 0; x < p.w; ++x)
                p.grey[static_cast<std::size_t>(yy) * p.w + x] =
                    mask[static_cast<std::size_t>(yy) * g.pw + x] ? static_cast<std::uint8_t>(v) : 0;
        val = add16(val, step);
        val = val > 0x9f6 ? 0x9f6 : add16(val, step);  // [BYTES seg15:0x4b30-0x4b46]
    }
    // The grey-128 band [BYTES seg15:0x4b64-0x4b96] (verified claim 13).
    fill(g.poly[3].x + 1, g.pc, g.pw - 2, 10, 0x80);
    // The column [BYTES seg15:0x4bad-0x4c6c] (verified claim 14).
    val = 450;
    for (int i = 0; i < 0x12; i += 2) {
        const int v = idiv16(val, 10) & 0xff;
        fill(add16(half16(g.pw), i) + g.poly[3].x - 0x12, g.pc + 10, 2, 0x2a, v);
        fill(sub16(half16(g.pw), i) + g.poly[3].x + 0x10, g.pc + 10, 2, 0x2a, v);
        val = std::min(add16(val, 0xe3), std::int16_t{0x9f6});
    }
    return p;
}

std::vector<int> pad_greys(const PadGeometry& g) {
    std::vector<int> out;
    const auto add = [&out](int v) {
        if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
    };
    if (g.pc <= 0) return out;
    const int step = idiv16(1700, g.pc);
    int val = 450;
    for (int y = 0; sub16(g.pc, 2) > y; y = add16(y, 2)) {
        add(idiv16(val, 10) & 0xff);
        val = add16(val, step);
        val = val > 0x9f6 ? 0x9f6 : add16(val, step);
    }
    add(0x80);
    val = 450;
    for (int i = 0; i < 0x12; i += 2) {
        add(idiv16(val, 10) & 0xff);
        val = std::min(add16(val, 0xe3), std::int16_t{0x9f6});
    }
    return out;
}

}  // namespace is::gfx
