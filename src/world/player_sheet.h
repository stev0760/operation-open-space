// A ship's sprite sheet, image slot 0x28 + i of the DC table DGROUP:0x39e8 (docs/re/structs.md
// "Image slots"; docs/re/world.md "The player's sprite (seg7:0x1208)"): the sheet builder
// seg14:0x11a8, its call of the per-slot cache seg10:0x6ee6 into ISPACE.DLL ordinal 2
// (docs/re/ispace.md), the sheet bitmap's measures of ISPACE.DLL seg6:0x11c, and the values
// seg14:0x0 reads back. The renderer that fills the cells (the rotation of ISPACE.DLL
// seg6:0x11c) is the render track's (src/gfx/ship_sheet.h); this side takes it as a
// SheetRenderer. Portable, no Win32 (M5 s1 SPRITE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "world/records.h"
#include "world/sprite.h"

namespace is::gfx {
struct SheetArgs;
struct ShipSheet;
}  // namespace is::gfx

namespace is::world {

// ---- the sheet bitmap of ISPACE.DLL seg6:0x11c

// The gap word seg14:0x11a8 passes: 99 lets the DLL choose [BYTES seg14:0x129c-0x12d0].
constexpr int kSheetGapAuto = 99;

// Gap word 99 becomes 5 for an odd source height, else 6 [BYTES ISPACE.DLL seg6:0x2b1
// seg6:0x2b7 seg6:0x2bb seg6:0x2c2]; INSPACE picks the same from ordinal 5's height
// [BYTES seg14:0x12d8 seg14:0x12e0]. Any other word is the gap.
std::int16_t sheet_gap(int gap_word, int side);

// The measures of the sheet bitmap: h = the source height (the cell side, ordinal 5), w the
// source width, g the gap, s the scale flag, e = (h + g) << s for row mode 1 else 0;
// CreateCompatibleBitmap(((16h + 17g) << s) + e, ((h + g)(w / h) + g) << s), then PatBlt
// BLACKNESS [BYTES ISPACE.DLL seg6:0x2c7 seg6:0x2d2 seg6:0x3a83-0x3ae4 seg6:0x3b3b-0x3b6f].
// `rows` is the number of frame rows the DLL renders: 2 for mode 1 with more than one source
// frame, else all w / h [BYTES ISPACE.DLL seg6:0x2d5 seg6:0x2d8 seg6:0x2de seg6:0x2e3]. Column 16
// (the extra column of mode 1) and what rows 2 and 3 hold are the render track's [OPEN].
struct SheetGeometry {
    int size = 0;    // h, the cell side before the scale
    int gap = 0;
    int frames = 0;  // w / h
    int rows = 0;    // the frame rows rendered
    int scale = 0;   // s
    int extra = 0;   // e
    int w = 0, h = 0;
};
SheetGeometry sheet_geometry(int src_w, int src_h, int gap_word, int mode, int scale);

// Cell (column c, row r) of a sheet: (c (size + gap) + gap, r (size + gap) + gap), 16-bit. The
// player's column is its heading (seg7:0x1208 [BYTES seg7:0x1338 seg7:0x133d]), row 0 at y = gap
// (seg9:0x6624's +0x12 = +0x24), row 1 at 2 gap + size (seg7:0x1384).
Point16 sheet_cell(int column, int row, std::int16_t size, std::int16_t gap);

// ---- seg14:0x11a8's arguments

// The row-mode byte: 0 when ship byte +0xe4 (`has_flame`) is set, 2 when it is 0 (a body-thrust
// ship), and 1 whenever ship byte +0xe9 (the body's Shield bit, design byte +0xa7) is set
// [BYTES seg14:0x1245 seg14:0x124c seg14:0x1252 seg14:0x125a seg14:0x1261 seg14:0x21f2].
std::uint8_t sheet_mode(std::uint8_t has_flame, std::uint8_t be9);

// `sel`, the renderer choice: 1 (the quick renderer ISPACE.DLL seg6:0x78ba) when INI `Quick`
// (DGROUP:0x931e) = 2, or when the screen state DGROUP:0x90f4 = 1, the frame counter dword
// DGROUP:0x1e68 != 0, `Quick` = 1 and ship i is not the player DGROUP:0x9646; else 0 (the full
// renderer seg6:0x11c) [BYTES seg14:0x1265-0x1297].
std::int16_t sheet_sel(int quick, int state, std::uint32_t frames, int ship, int player);

// seg10:0x6ee6's scale flag w6: 1 when `Size` (DGROUP:0x9156) >= 2, w6 != -1 and sel = 0, or when
// w6 > 1; else 0 [BYTES seg10:0x6f21-0x6f41]. seg14:0x11a8 passes w6 = 0.
std::int16_t sheet_scale_flag(int size_level, int w6, int sel);

// What seg14:0x11a8 hands to seg10:0x6ee6 and on to ordinal 2 (ISPACE.DLL seg6:0x0: hPal, id,
// lib, hdc, six words, three bytes, sel) [BYTES seg14:0x11c6 seg14:0x129c-0x12d0 seg10:0x6f46-0x6f76]
// [BYTES ISPACE.DLL seg6:0x27 seg6:0x74 seg6:0xa2 seg6:0xab].
struct SheetRequest {
    int slot = kShipImageSlotBase;  // 0x28 + i
    int graphic_id = 0;             // design word +0x36 (DGROUP:0x9708 + 0xad n): BOD +0x21
    std::string file;               // design +0x84 (DGROUP:0x9756 + 0xad n): BOD +0x23's name
    int w1 = 1;
    int gap_word = kSheetGapAuto;   // w2
    int recolour_a = 0;             // w3 = design word +0xa9 [OPEN: its writer]
    int recolour_b = 0;             // w4 = design word +0xab [OPEN: its writer]
    int w5 = 1;                     // ISPACE.DLL DGROUP:0x1ccc = 4 with w5 != 0 [BYTES ISPACE.DLL seg6:0x2f9]
    int scale = 0;                  // w6 after seg10:0x6ee6 (sheet_scale_flag)
    int colour_a = 0;               // design byte +0x18 (DGROUP:0x96ea): SHP +0x07
    int colour_b = 0;               // design byte +0x1a (DGROUP:0x96ec): SHP +0x08
    int mode = 0;                   // sheet_mode
    int sel = 0;                    // sheet_sel
    int size_level = 1;             // INI `Size` (DGROUP:0x9156), for the renderer
};

// What ordinal 2 returns: the sheet bitmap's top-down palette indices (w x h) and ordinal 5's
// value afterwards, the source height (ISPACE.DLL DGROUP:0x1c72) [BYTES ISPACE.DLL seg6:0x2ae
// seg6:0xba].
struct SheetPixels {
    int w = 0, h = 0;
    std::vector<std::uint8_t> index;
    int source_height = 0;
};
// seg14:0x11a8's arguments as ISPACE.DLL seg6:0x11c reads them [BYTES seg14:0x129c-0x12d0]:
// gap <- w2, four <- w5, scale <- w6, scheme_a/b <- b1/b2, mode <- b3 (gfx/ship_sheet.h).
gfx::SheetArgs sheet_args(const SheetRequest& req);
// Ordinal 2's result from the render track's sheet: the hPal indices as the port's index pixels
// (ADR 0007 decision 2), the BLACKNESS-only pixels as index 0, and ordinal 5's value the
// unscaled cell h (ISPACE.DLL DGROUP:0x1c72) [BYTES ISPACE.DLL seg6:0x2ae]. False for no sheet.
bool sheet_pixels_from(const gfx::ShipSheet& sh, SheetPixels& px);

// The render: false when there is nothing to render (no graphic), and ordinal 2 returns 0.
using SheetRenderer = std::function<bool(const SheetRequest&, SheetPixels&)>;

// The stand-in render until the render track's rotation lands (src/gfx/ship_sheet.h) [DESIGN]:
// a black sheet of sheet_geometry's measures with source frame r, unrotated, in every heading
// column 0 ... 15 of row r < rows (pixel-doubled when the scale flag is set); column 16 stays
// black. `src` holds the source graphic's top-down palette indices, src_w x src_h.
SheetPixels unrotated_sheet(const std::vector<std::uint8_t>& src, int src_w, int src_h,
                            const SheetRequest& req);

// What a build did: the DC in the slot (kDcNone when none), the raw side and gap (ordinal 5 and
// 5/6), whether seg14:0x11a8 writes them to the ship's object (roster index -1 and the slot
// empty before) or the roster bytes +0x7a/+0x7b, and the sheet's height (GetObject's bmHeight
// at seg14:0xd59).
struct SheetBuild {
    DcId dc = kDcNone;
    bool rendered = false;
    std::int16_t side = 0, gap = 0;
    bool to_object = false;
    std::int16_t bm_height = 0;
};

// seg14:0x11a8(i) [BYTES seg14:0x11a8-0x1427]:
//  - the shared sheet: ship word +0x50 != i and ship +0x50's bytes +0xad and +0xac both set
//    copies another ship's sheet (seg10:0x86e2, seg10:0x5d3c) [OPEN: not ported; the wave
//    set-up's seg14:0x0 sets +0x50 = i first, so the branch is dead there] [BYTES seg14:0x11cf
//    seg14:0x11db seg14:0x11e2 seg14:0x11f7 seg14:0x1200];
//  - already built: the slot holds a DC and bytes +0xac and +0xad are set [BYTES seg14:0x120e
//    seg14:0x1219 seg14:0x1220];
//  - otherwise seg10:0x6ee6: frees the slot's DC (seg10:0x552(slot, 0)), renders, and puts the
//    bitmap in a new DC in the slot [BYTES seg10:0x6ee6 seg10:0x6f7f seg10:0x6f89]; seg10:0x6308
//    returns at once at `Size` 1 [BYTES seg10:0x6308], and its rescale at other sizes is [OPEN];
//  - then bytes +0xac = +0xad = 1 [BYTES seg14:0x13eb seg14:0x13ef]. The design tail +0x68 ...
//    +0x6e behind DGROUP:0x95a6 and the weapon images seg10:0x533a are [OPEN] here.
// The object and roster writes are left to the caller (to_object says which) [DESIGN: the
// size step is the ship set-up's, setup.h ship_cell_sizes].
SheetBuild build_ship_sheet(Blitter& out, ImageSlotTable& images, ShipRecord& ship, int i,
                            const SheetRequest& req, const SheetRenderer& render);

// seg16:0x3468(i, 3)'s sheet part: byte +0xac = 0 so the build renders [BYTES seg16:0x3621], the
// build [BYTES seg16:0x362b], then object word +0x3a = DGROUP:0x3a38[i] (the sheet DC that
// seg12:0x2556 and seg7:0x158e read) [BYTES seg16:0x3636 seg16:0x3645]. Its other writes (ship word +0x20, +0x22, object
// +0x44) are the ship set-up's [OPEN].
SheetBuild rebuild_ship_sheet(Blitter& out, ImageSlotTable& images, ShipRecord& ship,
                              ObjectRecord& obj, int i, const SheetRequest& req,
                              const SheetRenderer& render);

// seg14:0x0's ship byte +0x5b after the sheet: (bmHeight - gap) / (size + gap) - 1 with the
// scaled object words +0x22 and +0x24, 16-bit signed division [BYTES seg14:0xd3f-0xd66
// seg14:0xc3e]. 0 when size + gap is 0 [DESIGN: the original divides by 0].
std::uint8_t sheet_last_row(std::int16_t bm_height, std::int16_t size, std::int16_t gap);

// ---- the calls into the sprite side from the frame (src/world/flight_hooks.cpp DefaultHost)

// The program's slots for the drawing the frame's host reaches (src/hud/wave_win32.cpp fills
// them): seg12:0x2556 + seg7:0x13b2(2, 14, i) at the ship entry (UpdateHost::ship_entered),
// seg7:0x158e (UpdateHost::seg7_158e), and seg11:0xb105 to the end of seg11:0xb09c
// (FlightHost::flame_tail). An empty slot does nothing.
struct SpriteLinks {
    std::function<void(int ship)> ship_entered;
    std::function<void()> entry_step;
    std::function<void(int on, int ship)> flame_tail;
};
SpriteLinks& sprite_links();

}  // namespace is::world
