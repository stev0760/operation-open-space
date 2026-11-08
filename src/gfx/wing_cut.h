// ISPACE.DLL ordinal 15 (seg5:0x11a4), the wing cut: INSPACE's seg8:0x2cd8 calls it through
// seg10:0x7370 on a ship's sheet DC DGROUP:0x3a38[ship] when a wing is torn off
// (work/analysis/m4/s5-sheet.md sections 2.1-2.8, verified in s5-sheet-verify.md claims 11-21,
// claim 17 as CORRECTED). It SRCANDs a monochrome half-plane mask into row 0 of all 16 heading
// cells, so each cell keeps the side away from the torn wing plus a band of half-width D (design
// word +0x3c); with its flag set it first copies the cell of the ship's heading and erases the
// kept side from the copy (DSna), the torn wing's picture for the debris.
//
// Portable and pure: the sheet is the port's index picture (ADR 0007 decision 2), so the raster
// operations become keep and erase on indices: SRCAND with a white mask bit keeps the pixel and
// with a black one makes it 0 (black), DSna the other way round. Mono-to-colour conversion maps
// mask 1 to the destination DC's background (white by default) and 0 to its text colour (black)
// [REF Win32 SDK BitBlt], and the oracle's kept pixels keep their colours (core-0001 35-f1-20)
// [ORACLE core-0001].
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace is::gfx {

// The mask DGROUP:0x184 (bitmap DGROUP:0x186) that ordinal 4 command 0 builds once: a 450 x 90
// monochrome bitmap of five 90 x 90 cells [BYTES ISPACE.DLL seg5:0x371a-0x386e].
constexpr int kWingMaskW = 450, kWingMaskH = 90, kWingMaskCell = 90;

// ISPACE.DLL DGROUP:0x1a and DGROUP:0x10, the offset tables of the band [BYTES ISPACE.DLL
// DGROUP:0x10 DGROUP:0x1a].
extern const std::array<std::int16_t, 5> kWingT1;  // DGROUP:0x1a: -128, -118, -91, -50, 0
extern const std::array<std::int16_t, 5> kWingT2;  // DGROUP:0x10: 0, 50, 91, 118, 128

// The mask's bits, 1 = white, row by row (y * 450 + x):
//  1. PatBlt(0, 0, 450, 90, BLACKNESS) [BYTES ISPACE.DLL seg5:0x3751-0x3762];
//  2. PatBlt(45, 0, 46, 90, WHITENESS): columns 45 ... 90 [BYTES ISPACE.DLL seg5:0x3767-0x3778];
//  3. PatBlt(360, 45, 90, 46, WHITENESS): cell 4's rows 45 ... 89 [BYTES ISPACE.DLL
//     seg5:0x377d-0x378f];
//  4. WHITE_PEN and WHITE_BRUSH, then Polygon (180,89)(269,89)(269,0), (112,89)(179,89)(179,0)
//     (157,0) and (270,68)(270,89)(359,89)(359,21) [BYTES ISPACE.DLL seg5:0x3794-0x3851].
// The polygons' pixels are GDI's: the port's table is what Wine's GDI draws for these calls into
// a 1-bpp bitmap, the same under Wine 11.0 (old WoW64, the oracle's) and Wine 11.20, 32- and
// 64-bit; tests/test_gfx_wing_cut_win32.cpp draws them again and compares bit for bit. Each row
// of cells 1, 2 and 3 is one white run from a left edge to the cell's last column: cell 1 from
// 157 - (y + 1) / 2, cell 2 from 269 - y, cell 3 from kWingCell3Left[y - 21] for rows 21 ... 67
// and from 270 below [DESIGN: the reference raster is Wine's GDI, the oracle's renderer].
extern const std::array<std::int16_t, 47> kWingCell3Left;
struct WingMask {
    std::vector<std::uint8_t> bit;  // kWingMaskW * kWingMaskH
    bool at(int x, int y) const {
        return x >= 0 && y >= 0 && x < kWingMaskW && y < kWingMaskH &&
               bit[static_cast<std::size_t>(y) * kWingMaskW + x] != 0;
    }
};
const WingMask& wing_mask();

// Cell k's window of the mask for a cell side s and band half-width D [BYTES ISPACE.DLL
// seg5:0x11b3-0x11f4 seg5:0x12e2-0x1321 seg5:0x1494-0x14d3 seg5:0x152d-0x156c
// seg5:0x16dc-0x171b] (verified claims 12, 13):
//   o = 45 - trunc(s / 2), A[k] = lo16(T1[k] D) sar 7, B[k] = lo16(T2[k] D) sar 7,
//   sx = 90 k - A[k] + o, sy = B[k] + o, w = min(A[k] - o + 89, s), h = min(89 - sy, s)
// (min as a signed `jle`, 16-bit throughout). For s > 90, o < 0 and cell 0's window starts left
// of the bitmap [OPEN: no shipped body reaches it].
struct WingWindow {
    int sx = 0, sy = 0, w = 0, h = 0;
};
WingWindow wing_window(int s, int D, int k);

// The switch at ISPACE.DLL seg5:0x12ab (table seg5:0x12b0): heading f to its mask cell, 0 and 8
// -> 0; 1, 7, 9, 15 -> 1; 2, 6, 10, 14 -> 2; 3, 5, 11, 13 -> 3; 4, 12 -> 4 [BYTES ISPACE.DLL
// seg5:0x127b-0x12ce] (verified claim 16). -1 outside 0 ... 15 (`ja` past the table; the
// original keeps [bp-4] = 5 from the offset loop then [OPEN]).
int wing_cell(int f);

// How cell k is placed into heading f (verified claims 18, 19): a mirror is a StretchBlt with
// the mirrored extent negative and the origin at the far edge (x0 + s - 1 or y0 + s - 1), the
// source extent positive and equal, so nothing scales; under the Windows rule a negative extent
// covers x + w + 1 ... x, so destination column x0 + s - 1 - i takes source column sx + i
// (docs/re/ispace.md ordinal 2's logo strip) [REF Wine gdi32 tests test_StretchBlt].
//   cut = 1: 0 ... 4 identity, 5 ... 8 mirror x, 9 ... 11 turn, 12 ... 15 mirror y;
//   cut = 0: the same composed with a turn: 0 ... 4 turn, 5 ... 7 mirror y, 8 ... 12 identity,
//   13 ... 15 mirror x [BYTES ISPACE.DLL seg5:0x13ad-0x1488 seg5:0x14d6-0x1521
//   seg5:0x160a-0x16cb seg5:0x1721-0x1780 seg5:0x1324-0x13ab seg5:0x1570-0x15fd].
enum class WingOrient : std::uint8_t { kIdentity, kMirrorX, kMirrorY, kTurn };
WingOrient wing_orient(int f, bool cut);

// What the sheet pass does to the s x s cell of heading f: 1 where the pixel stays, 0 where
// SRCAND (0x008800C6) blacks it; pixels outside the window's destination rectangle stay.
// `cut` is ordinal 15's [bp+0x12], seg8:0x2cd8's [bp-4]: 1 when wing +0xac goes.
std::vector<std::uint8_t> wing_keep(int s, int D, int f, bool cut);

// The sheet pass [BYTES ISPACE.DLL seg5:0x1494-0x1521 seg5:0x16dc-0x1780]: for every heading f =
// 0 ... 15 the window of cell wing_cell(f) at wing_orient(f, cut) into the cell at ((g + s) f +
// g, g), row 0 only; `px` is the sheet's top-down index picture, `width` x `height`. Rows >= 1
// and column 16 keep their pixels.
void wing_cut_sheet(std::vector<std::uint8_t>& px, int width, int height, int s, int g, int D, bool cut);

// The flag's copy [BYTES ISPACE.DLL seg5:0x11ff-0x1276 seg5:0x12e2-0x1372 seg5:0x152d-0x15fd]
// (verified claims 15, 17 CORRECTED): a black s x s bitmap (PatBlt BLACKNESS), the sheet's
// heading-f cell of row 0 copied in with SRCCOPY (pixels outside the sheet stay black), then the
// window of cell wing_cell(f) at wing_orient(f, cut) at (0, 0) with DSna (0x00220326), which
// blacks the copy where the mask is white: the torn side outside the band stays. Taken before
// the cut, as ordinal 15 does. Top-down, s x s.
std::vector<std::uint8_t> wing_debris_cell(const std::vector<std::uint8_t>& px, int width, int height, int s,
                                           int g, int D, int f, bool cut);

}  // namespace is::gfx
