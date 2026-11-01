// The thrust flame's pictures: ISPACE.DLL ordinal 18 (seg6:0x5e76), which draws IGRAPHIC.DLL
// graphic 280 (an 18 x 18 4-bpp flame) turned into n directions, averaged down 2x2 and snapped
// onto the game palette's ramps, and its INSPACE wrapper seg10:0x7032, which mirrors the 5
// directions to 16 headings (DGROUP:0x3c60). seg10:0x5d3c composites these over a ship's sheet
// patches (src/world/player_sheet.h FlameStrip) and seg11:0xb09c pastes the result into the
// ship's sheet (work/analysis/m5/s2-flame.md).
//
// Portable and pure: the palette side (the working entries, the ramp records of seg5:0x64f8 and
// the remap) comes in as a FlamePalette; flame_bitmap_for() wires it to gfx::PaletteManager. Only
// the path INSPACE takes is ported: x = 0, ISPACE.DLL DGROUP:0x1e9a >= 8 (8-bpp displays and
// up, the oracle's and the port's), palette kind 0 [BYTES ISPACE.DLL seg6:0x5f0e seg6:0x5fa2-
// 0x5fc4 seg6:0x7079-0x70b0]. The x != 0 branch (seg7:0x8e6), the depth < 8 Floyd-Steinberg
// branch (seg5:0x4b40) and palette kind 1 (seg5:0x4e96) are not [DESIGN].
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "formats/dib.h"
#include "gfx/palette.h"

namespace is::gfx {

// ISPACE.DLL DGROUP:0xf6 and DGROUP:0x108, indexed by the direction d: 512 sin(22.5 d) and
// -512 cos(22.5 d) [BYTES ISPACE.DLL DGROUP:0xf6 DGROUP:0x108].
extern const std::array<std::int16_t, 9> kFlameT1;  // DGROUP:0xf6
extern const std::array<std::int16_t, 9> kFlameT2;  // DGROUP:0x108
// The per-direction offsets ordinal 18 adds after the rounding, stack words its entry sets for
// d = 0 ... 4 [BYTES ISPACE.DLL seg6:0x5e86-0x5eb4]. Directions 5 and up read words the entry
// does not set [OPEN]; the port draws at most 5.
constexpr int kFlameDirsMax = 5;
extern const std::array<std::int8_t, kFlameDirsMax> kFlameOffX;
extern const std::array<std::int8_t, kFlameDirsMax> kFlameOffY;
// The IGRAPHIC.DLL #4000 graphic ordinal 18 loads, 0x118 [BYTES ISPACE.DLL seg6:0x5f35-0x5f59].
constexpr int kFlameGraphic = 280;
// The darkest colour ordinal 18 keeps: r, g and b all <= 0x32 become 0 [BYTES ISPACE.DLL
// seg6:0x7079-0x70b0].
constexpr int kFlameDarkMax = 0x32;

// The palette side of ordinal 18 and seg5:0x64f8.
struct FlamePalette {
    // hPal's working entries (DGROUP:0x1168, the slot's DGROUP:0x116c).
    const PaletteArray* working = nullptr;
    // DGROUP:0x1e9c[slot] = 2: seg5:0x64f8 returns al = 0 (xor al, al) and ordinal 18 clears ah,
    // so the pixel is 0 [BYTES ISPACE.DLL seg5:0x6512 seg6:0x71a8].
    bool mode2 = false;
    // The ramp records seg5:0x64f8 scans, in order: those of schemes 1 and -1, i.e. record
    // DGROUP:0x1dea[mode 25 + 1] and none (first > last: none) [BYTES ISPACE.DLL seg5:0x6517-
    // 0x655a seg5:0x66a5-0x66df]. Entries, inclusive.
    struct Range {
        int first = 0, last = -1;
    };
    std::array<Range, 2> scan{};
    // The remap of the second row, (v - lo1) span0 / span1 + lo0 with the records of schemes 1
    // and 0: lo = trunc(lo word / 4), span = trunc((hi word - lo word) / 4), which is the
    // entries last - first of the port's divided records when the ramp words are multiples of 4
    // (trunc(hi / 4) - trunc(lo / 4) = trunc((hi - lo) / 4) then; seg5:0x4581/0x4587's 0x2c and
    // 0x6c are) [DESIGN: the port keeps the divided entries], the product kept to 16 bits [BYTES ISPACE.DLL
    // seg6:0x700d-0x7075 seg6:0x71c4-0x71ec] (s2-flame-verify.md claim 8). A scheme
    // without a record reads DGROUP:0x1398 + 4 (19 mode - 1) [OPEN]; flame_bitmap_for leaves
    // the row 0 then [DESIGN].
    int lo0 = 0, span0 = 0, lo1 = 0, span1 = 0;
    bool remap = false;
};

// ISPACE.DLL seg5:0x64f8(slot, r, g, b, 1, -1): the first entry of the scanned ramps within
// distance 14 (|dr| + |dg| + |db| against the working entries), else the nearest (the first of
// equal ones), as DGROUP:0x1a6 >> 2 [BYTES ISPACE.DLL seg5:0x655d-0x66df]. DGROUP:0x1a6 keeps
// the entry of the last call, so `last` carries it in and out; with no ramp to scan it is
// returned as it is (its initial value is 0x28 >> 2 = 10 [BYTES ISPACE.DLL DGROUP:0x1a6]).
int nearest_flame_entry(const FlamePalette& fp, Rgb c, int& last);

// ordinal 18's picture, top-down: width (H n) / 2, height (H rows) / 2, one value per pixel, an
// index into hPal's logical palette (seg5:0x2080 maps the DIB with DIB_PAL_COLORS). Row block 0
// (the top H / 2 rows) is flame type 0, ramp 1 (orange); row block 1 is type 1, the same
// remapped onto ramp 0 (blue). Cell k (x = k H / 2) is direction k [BYTES ISPACE.DLL
// seg6:0x7722-0x7739 seg6:0x76bb].
struct FlameBitmap {
    int width = 0, height = 0;
    int cell = 0;  // H / 2
    int dirs = 0;
    std::vector<std::uint8_t> px;
    std::uint8_t at(int x, int y) const { return px[static_cast<std::size_t>(y) * width + x]; }
    bool ok() const { return width > 0; }
};

// Ordinal 18 with (hPal, b1, b2, x = 0, n, y) on the path above. `flame` is graphic 280 as stored
// (4 bpp, rows bottom-up); `colours` its 16-entry colour table after seg5:0x1b8e's load (d = 1,
// schemes 12 / 12: the picture's own colours). The steps [BYTES ISPACE.DLL seg6:0x61b3-0x6371
// seg6:0x638c-0x6411 seg6:0x6ce5-0x6f28 seg6:0x70cd-0x71ec]:
//  1. every non-zero nibble of source (c, r) (r the memory row, 0 <= r < W, 0 <= c < H) goes to
//     X = round((r - c0) T1[d] + (c0 - c) T2[d], 512) + offx[d] + c0 and Y = round((c0 - r) T2[d]
//     + (c0 - c) T1[d], 512) + offy[d] + c0 (c0 = H / 2, round half away from zero, 16-bit) in
//     a zeroed 8-bpp (W n) x H buffer at H d + X + Y stride, dropped outside 0 <= X < W, 0 <= Y < H;
//  2. holes: in rows 1 ... H - 2 and columns 1 ... H n - 2, left to right in place, a 0 between
//     two non-zero pixels takes the left one;
//  3. each 2 x 2 block's four colours are averaged per channel ((sum) >> 2);
//  4. a colour with r, g, b <= 0x32 becomes 0, any other nearest_flame_entry; row block 1 gets 0
//     or the remap.
// Returns an empty bitmap for a picture that is not 4 bpp or n outside 1 ... 5 [DESIGN].
FlameBitmap render_flame(const fmt::Dib& flame, const std::vector<std::uint32_t>& colours, int n,
                         const FlamePalette& fp, int& dg_1a6);

// The whole call as seg10:0x7032 makes it, ordinal 18(DGROUP:0x920e, 1, 1, 0, 5, 0), on palette
// `pal` of `pm` [BYTES seg10:0x7041-0x7051]: seg5:0x1b8e(hPal, 0x118, "", d = 1, 12, 12, 0) loads
// the colours with DGROUP:0x188 = 0 [BYTES ISPACE.DLL seg6:0x5f35-0x5f59], the ramps are pm's
// records (seg5:0x64f8 does not allocate) and DGROUP:0x1a6 is pm-independent state here, 10 at
// first [DESIGN].
FlameBitmap flame_bitmap_for(const fmt::Dib& flame, PaletteManager& pm, PalHandle pal);

// seg10:0x7032's result DGROUP:0x3c60: a 16 f x 2 f picture, black (0) where nothing is drawn,
// holding ordinal 18's 5 x 2 cells at x = 0 ... 5 f and their mirrors, for both rows: heading 8 is
// the vertical flip of 0, heading 12 the horizontal flip of 4, and for k = 1 ... 3 heading 8 - k
// the vertical flip, k + 8 the 180-degree turn and 16 - k the horizontal flip of k [BYTES
// seg10:0x70a4-0x72a7]. The copies StretchBlt cells of s = ordinal 18's cell to f; at Size 1
// (f = 9 = s) they are exact. seg10:0x6308's rescale of the bitmap at other sizes is [OPEN]; the
// port stretches nearest-neighbour then [DESIGN].
struct FlameHeadings {
    int f = 0;
    int width = 0, height = 0;  // 16 f x 2 f
    std::vector<std::uint8_t> px;
    std::uint8_t at(int x, int y) const { return px[static_cast<std::size_t>(y) * width + x]; }
    bool ok() const { return width > 0; }
};
FlameHeadings flame_headings(const FlameBitmap& bmp, int f);

// seg10:0x5c42's cell f: (9 << Size) >> 1, or 6 Size + 3 while DGROUP:0x9130 != 0 [BYTES
// seg10:0x5c42].
int flame_cell(int size_level, bool seg18_mode = false);

}  // namespace is::gfx
