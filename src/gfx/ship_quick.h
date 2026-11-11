// ISPACE.DLL ordinal 17 (seg6:0x5538), the quick renderer: INSPACE's seg11:0x4b00 (the shield
// toggle) calls it through seg10:0x7328 to render body frames 1 and 2 at the ship's heading into
// column 16 of the ship's sheet, rows 0 and 1 (work/analysis/m4/s5-sheet.md sections 4.1-4.4,
// verified in s5-sheet-verify.md claims 22-34 with claim 31 CORRECTED and the open item on gap
// 0x63). Unlike the sheet renderer seg6:0x11c (2x layers, averaging, ramp requantisation;
// gfx/ship_sheet.h) it writes one palette index per source nibble through the colour map
// DGROUP:0x1484 (gfx/palette.h quick_colour_map), so column 16 looks coarser than the heading
// cells (core-0005's f1 and f2 pictures) [ORACLE core-0005].
//
// Portable and pure. Only the DC path INSPACE takes is ported: a DC argument, heading 0 ... 15,
// two iterations, one column. The quick sheet (heading -1, 16 columns: seg6:0x78ba's call for
// INI `Quick`, ispace.md ordinal 2) is not [OPEN].
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "formats/dib.h"
#include "gfx/palette.h"
#include "gfx/ship_flame.h"

namespace is::gfx {

// The DIB of one iteration as seg5:0x23a4 allocates it (GlobalAlloc 0x42: zeroed) and the
// pixel loop fills it, top-down here: side = h + 2 gap square, the h x h cell at (gap, gap)
// [BYTES ISPACE.DLL seg6:0x56c5-0x56ed seg5:0x23f2-0x240d] (verified claim 34). Memory row Y of
// the bottom-up original is top-down row side - 1 - Y.
struct QuickCells {
    int h = 0;     // the source height (ISPACE.DLL DGROUP:0x1c72), the cell side
    int gap = 0;   // gap word 0x63 becomes 5 for an odd h, 6 for an even one [BYTES ISPACE.DLL seg6:0x5e1b-0x5e2b]
    int side = 0;  // h + 2 gap
    std::array<std::vector<std::uint8_t>, 2> buf;  // iterations it = 0, 1: body frames 1, 2
    std::uint8_t at(int it, int x, int y) const {
        return buf[static_cast<std::size_t>(it)][static_cast<std::size_t>(y) * side + x];
    }
    // The h x h cell at (gap, gap) of iteration it, top-down.
    std::vector<std::uint8_t> cell(int it) const;
    bool ok() const { return side > 0; }
};

// The pixel loop of ordinal 17 with a DC (N = 2 iterations, one column, F = R = 1, fsrc = 0)
// [BYTES ISPACE.DLL seg6:0x5800-0x5be2] (verified claims 28-32):
//  - iteration it reads its source from byte trunc(h / 2) (it + 1) of the 4-bpp strip, row by
//    memory row (the stride trunc((W + 1) / 2) padded to 4), and starts x at 1 when (it + 1) h
//    is odd: with an odd h body frame 1 lands one column right and frame 2 two columns right
//    (their last columns are never read); an even h has no shift;
//  - an even x reads the high nibble, an odd x the low one; nibble 15 is transparent;
//  - (x, y) goes to X = ((lo16(C[d] (c - x)) + lo16(S[d] (y - c)) + 0x100) sar 9) + c + gap,
//    Y = ((lo16(S[d] (c - x)) + lo16(C[d] (c - y)) + 0x100) sar 9) + c + gap, c = trunc(h / 2),
//    S and C the sheet's tables (gfx/ship_sheet.h kSheetSin, kSheetCos), 16-bit sums with `add
//    ah, 1`, kept only for gap <= X < h + gap and gap <= Y < h + gap, and gets map[v];
//  - headings 2, 6, 10 and 14 ((d - 0xfe) & 3 = 0 on the byte) get the gap fill: over memory
//    rows 0 ... h - 1 and columns gap ... h + gap - 1, left to right in place, a 0 whose left and
//    right neighbours are both non-zero takes the left one. The cell lies on memory rows gap ...
//    h + gap - 1, so its top `gap` image rows are never filled.
// `body` is the strip as stored (4 bpp); `gap_word` 0x63 (99) lets h choose. Returns an empty
// result for a body that is not 4 bpp or a heading outside 0 ... 15 [DESIGN]. Source bytes past
// the bitmap read as transparent [OPEN: a body of fewer than 3 frames reads past its strip at
// it = 1] [DESIGN].
QuickCells render_quick_cells(const fmt::Dib& body, const QuickColourMap& map, int gap_word, int heading);

// The copy of iteration it into the sheet DC [BYTES ISPACE.DLL seg6:0x5c6f-0x5d38] (verified
// claim 33): with byte `scale` (INI `Size`, DGROUP:0x9156) = 1 a BitBlt SRCCOPY of the h x h cell
// at (gap, gap) to (16 h + 17 gap, (h + gap) it + gap); otherwise a StretchBlt to those values
// << scale sar 1, side (h << scale) sar 1, from (gap, gap, h, h).
struct QuickBlit {
    int x = 0, y = 0, w = 0, h = 0;     // destination in the sheet
    int sx = 0, sy = 0, sw = 0, sh = 0;  // source in the iteration's picture
    bool stretch = false;
};
QuickBlit quick_blit(int h, int gap, int it, int scale);

// ---- GDI's StretchBlt on index pixels (M4 s8 SIZE-SITES)

// The port's StretchBlt in COLORONCOLOR (STRETCH_DELETESCANS) on one axis, as Wine's GDI walks it
// (the oracle's renderer, ruling R4, ADR 0017's approach): map[d] is the source index that
// destination index d of `dst_len` takes from `src_len`. A stretch (dst >= src) walks the
// destination with e = 2 src - dst, a shrink walks the source with e = dst - src - 1; after each
// step e > 0 moves on and adds 2 (min - max), else adds 2 min. In a shrink each source column
// overwrites the current destination column, so the last of each run stays, while a source row is
// written only when it opens its run, so the first stays (`rows`): 2:1 keeps the second column and
// the first row of each pair, and an odd extent's one run of three falls in the middle (31 to 15:
// columns 1, 3, ..., 13, 16, 18, ..., 30). A column halving to one pixel takes floor((src - 1) /
// 2). 1:2 repeats each pixel; other stretches are not pinned. The original's Size 0 TRANSFORMER is
// its Size 1 picture halved by that rule, pixel for pixel (core-0011 tr3 sw01 / sw10, the cell at
// (512, 341)) [ORACLE core-0011], and tests/test_world_size_sites.cpp holds the rule against GDI
// under Wine 11.20 (CTest) and the oracle's Wine 11.0 old WoW64 [DESIGN ADR 0017]. Its users:
// quick_blit's stretch at `Size` != 1 on the port's sheet copy (world/player_sheet.h shield_render)
// and seg10:0x6308 on a sheet's pixels (rescaled_sheet_pixels).
std::vector<int> gdi_stretch_map(int src_len, int dst_len, bool rows);
// The same walk's runs for a shrink (dst_len < src_len): the destination index each source index
// falls in, -1 where the walk ends before it. WHITEONBLACK ORs every source pixel of a run into its
// destination pixel, along both axes, where COLORONCOLOR keeps the one gdi_stretch_map names
// (world/rescale.h stretch_index) (M4 s8 fixer, review M1) [DESIGN ADR 0017].
std::vector<int> gdi_shrink_owner(int src_len, int dst_len);

// seg10:0x7032's seg10:0x6308(-1, -1, bitmap, -1) on ordinal 18's flame bitmap outside seg18 mode
// [BYTES seg10:0x7071-0x708f]: ((w << Size) sar 1) x ((h << Size) sar 1) through gdi_stretch_map,
// so its cell (h / 2) becomes seg10:0x5c42's f = (9 << Size) sar 1, which the copies then take
// unstretched ([bp-0x1a] = f, seg10:0x7092-0x7095); gfx/ship_flame.h flame_headings(result, f)
// is seg10:0x7032's result at that Size. Size 1 returns `bmp` (M4 s8 SIZE-SITES).
FlameBitmap rescaled_flame_bitmap(const FlameBitmap& bmp, int size_level);

// The state ordinal 17 keeps between calls: DGROUP:0x147c and DGROUP:0x1480 (seg5:0x6f58 of b1
// and b2, written only for a scheme other than 12) [BYTES ISPACE.DLL seg6:0x55a8-0x55e5] and
// DGROUP:0x1a6, seg5:0x64f8's last entry (DGROUP:0x1a6 = 40 in the image, entry 10) [BYTES
// ISPACE.DLL DGROUP:0x147c DGROUP:0x1480 DGROUP:0x1a6]. The flame (gfx/ship_flame.h) keeps its own
// DGROUP:0x1a6 per call [DESIGN].
struct QuickState {
    int id_a = 0, id_b = 0;
    int dg_1a6 = 10;
};
QuickState& quick_state();

// The whole call as seg10:0x7328 makes it for the shield, ordinal 17(hPal, id, lib, hdc, 0x63,
// a3, b1, b2, heading, DC, 1, Size) on palette `pal` of `pm` [BYTES seg10:0x7335-0x7365]
// [BYTES ISPACE.DLL seg6:0x5538-0x5e72] (verified claims 23-27):
//  - for b1 != 12: DGROUP:0x147c = seg5:0x6f58(b1) and seg5:0x61b2(slot, it); for b2 likewise
//    DGROUP:0x1480 [BYTES ISPACE.DLL seg6:0x55a8-0x55e5];
//  - DGROUP:0x15a = DGROUP:0x15c = DGROUP:0x188 = 1, then seg5:0x1b8e(hPal, id, lib, 3 for a
//    kind-0 palette, b1, b2, 0): seg5:0x26a2's recolour of the colour table (rows b1 and b2, the
//    duplicate nudges; DGROUP:0x188 makes entry 15's red count), its DGROUP:0x15c pass
//    (quick_colour_map) and, for d = 3, a 16-entry palette DGROUP:0x1eae that seg5:0x3a68 deletes
//    again (no game-palette entry written) [BYTES ISPACE.DLL seg6:0x55e8-0x562c seg5:0x2c02-0x2c76].
//    DGROUP:0x15a also copies the table into row 12 of DGROUP:0x117c / DGROUP:0x1cdc, which no
//    ramp the port allocates reads (palette.h ramp_row has no row 12) [DESIGN];
//  - render_quick_cells.
// The words a3 (design +0xa9/+0xab) are never read (verified claim 24).
QuickCells quick_cells_for(const fmt::Dib& body, int body_id, PaletteManager& pm, PalHandle pal, int b1, int b2,
                           int heading, int gap_word = 99);

}  // namespace is::gfx
