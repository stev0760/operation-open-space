// owner: M4 stage 6, track PAD.
// The landing pad: INSPACE.EXE's seg15:0x482e (the geometry, DGROUP:0x3c8c ... 0x3ca2, 0x3ce4,
// 0x3ce6, 0x92dc) and its drawer seg15:0x4a2e (a 1-bpp trapezoid mask, the grey bands through
// MERGECOPY, the grey-128 band and the 36-px column) into the pad bitmap DGROUP:0x92a8 of the pad
// DC DGROUP:0x92b6 (work/analysis/m4/s6-pad.md section 1, claims 1-15 CONFIRMED in
// s6-pad-verify.md). The desk (state 0, desk step 2) and the visit (state 6, seg15:0x3590's
// sub-step 0) build it; the descent, the pad's move, the bare-pad redraw and the launch read it.
//
// Portable and pure: the picture is grey levels as the bytes compute them (0 = black), the
// PALETTERGB(v, v, v) of each brush; the 8-bit display turns each into the nearest palette entry
// of the moment (ADR 0007), which is the caller's (src/desk/gdi_desk.cpp, src/hud/visit_win32.cpp).
// All arithmetic is 16-bit signed as the original's (`idiv`, the `cwd; sub ax, dx; sar ax, 1`
// halving), 32-bit only where seg5:0x314 and seg5:0x110 work.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace is::gfx {

struct PadPoint {
    int x = 0, y = 0;
};

// seg15:0x482e from the play height DGROUP:0x90ec and the client height DGROUP:0x10 [BYTES
// seg15:0x483d-0x4975] (verified claims 1-5):
//   q = trunc(lo16(H << 2) / 3), k = q - 1100, A = trunc(k / 22) + 139 (DGROUP:0x3ce4),
//   pw = trunc(k / 36) + 164 (DGROUP:0x3ce6), pc = trunc(k / 30) + 80 (DGROUP:0x3c96, 0x3c9a);
//   the POINTs DGROUP:0x3c8c ... 0x3c9b = (half(pw) - half(A), 1), (half(A) + half(pw), 1),
//   (pw, pc), (0, pc); DGROUP:0x92dc = 200; X = half(q) - half(pw) (DGROUP:0x3c9c,
//   DGROUP:0x3c9e = 0); the slopes m1 = trunc(lo16(100 (1 - pc)) / P0.x) and m2 = trunc(lo16(100
//   (1 - pc)) / (P1.x - pw)); the vanishing point vx = lo16((X (m1 - m2) - pw m2) / (m1 - m2))
//   (DGROUP:0x3ca0, which nothing reads) and vy = lo16(m1 (vx - X) / 100) + DGROUP:0x10 - 52
//   (DGROUP:0x3ca2, the end of seg15:0x454c's LineTo calls, verified claim 6).
// A zero divisor (P0.x = 0, P1.x = pw, m1 = m2), which no window size reaches, faults the
// original; the port takes a quotient of 0 [DESIGN].
struct PadGeometry {
    int play_h = 0, client_h = 0;  // DGROUP:0x90ec, DGROUP:0x10
    int q = 0, k = 0;
    int top = 0;  // A, DGROUP:0x3ce4: the trapezoid's top width
    int pw = 0;   // DGROUP:0x3ce6, padW
    int pc = 0;   // DGROUP:0x3c9a, padC: the trapezoid's height; the bitmap is pc + 0x34 high
    std::array<PadPoint, 4> poly{};  // DGROUP:0x3c8c ... 0x3c9b
    int m1 = 0, m2 = 0;
    int x_left = 0;      // X, DGROUP:0x3c9c
    int vx = 0, vy = 0;  // DGROUP:0x3ca0, DGROUP:0x3ca2
    int d92dc = 200;     // DGROUP:0x92dc [BYTES seg15:0x4870]
    int height() const { return pc + 0x34; }
};
PadGeometry pad_geometry(int play_h, int client_h);

// seg15:0x4a2e's mask: CreateCompatibleBitmap of a fresh memory DC (1 bpp) of pw x pc, PatBlt
// BLACKNESS, then Polygon(DGROUP:0x3c8c, 4) with the stock WHITE_BRUSH and the DC's default pen
// (BLACK_PEN) [BYTES seg15:0x4a3b-0x4aa1] (verified claims 8-9). The raster is Wine's GDI's, the
// oracle's renderer [DESIGN, as src/gfx/wing_cut.h]: the interior is the region of
// CreatePolygonRgn (ALTERNATE: the X11 scan converter, the spans [xl, xr) of the two edges'
// integer Bresenham walks on rows 1 ... pc - 1), then the outline overwrites it with dibdrv's
// pen lines (each edge from its start, its end left out, the octant bias of octants 3, 5, 6
// and 8) [REF Wine gdi32 region.c CreatePolyPolygonRgn, dibdrv objects.c solid_pen_line]. It
// equals what Wine 11.0 (old WoW64) and Wine 11.20 draw, 32- and 64-bit, for every play height
// 1 ... 3000 (tests/test_gfx_pad_win32.cpp draws them again and compares bit for bit). Row 0 is
// outside the polygon, row 1 the pen's top edge. Top-down, pw x pc, 1 = white.
std::vector<std::uint8_t> pad_mask(const PadGeometry& g);

// The pad bitmap DGROUP:0x92a8 as grey levels, pw x (pc + 0x34), top-down, 0 = black (the
// PatBlt BLACKNESS of seg15:0x482e [BYTES seg15:0x4a0a-0x4a1c]) [BYTES seg15:0x4aa6-0x4c6c]
// (verified claims 10-14):
//  - the bands: step = trunc(1700 / pc), val = 450, y = 0; while pc - 2 > y (signed), brush
//    PALETTERGB(v, v, v) with v = trunc(val / 10) (0x02000000 + v 0x010101) and BitBlt(pad, 0,
//    y, pw, 2, mask, 0, y, MERGECOPY), so v where the mask is white and 0 elsewhere; then val +=
//    step and val = val > 2550 ? 2550 : val + step (the add runs twice), y += 2;
//  - PALETTERGB(128, 128, 128), PatBlt(DGROUP:0x3c98 + 1, pc, pw - 2, 10, PATCOPY);
//  - the column: val = 450, i = 0, 2, ... 16: v = trunc(val / 10), PatBlt(half(pw) + i +
//    DGROUP:0x3c98 - 18, pc + 10, 2, 42) and PatBlt(half(pw) - i + DGROUP:0x3c98 + 16, pc + 10,
//    2, 42), PATCOPY, then val = min(val + 227, 2550).
struct PadPicture {
    int w = 0, h = 0;
    std::vector<std::uint8_t> grey;
    std::uint8_t at(int x, int y) const { return grey[static_cast<std::size_t>(y) * w + x]; }
    bool ok() const { return w > 0 && h > 0; }
};
PadPicture pad_picture(const PadGeometry& g);

// The brushes' greys in the order seg15:0x4a2e creates them (the bands, 128, the nine column
// pairs), each once, for the caller's colour allocation.
std::vector<int> pad_greys(const PadGeometry& g);

// seg15:0x3590's grey allocation before seg15:0x482e in state 6: seg10:0x6d7a(v, v, v) for v =
// 40, 44, ... 224 (47 calls), twice [BYTES seg15:0x3619-0x36aa] (verified claim 16, CORRECTED).
constexpr int kVisitGreyFirst = 40, kVisitGreyLast = 224, kVisitGreyStep = 4;

}  // namespace is::gfx
