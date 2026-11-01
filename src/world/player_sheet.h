// A ship's sprite sheet, image slot 0x28 + i of the DC table DGROUP:0x39e8 (docs/re/structs.md
// "Image slots"; docs/re/world.md "The player's sprite (seg7:0x1208)"): the sheet builder
// seg14:0x11a8, its call of the per-slot cache seg10:0x6ee6 into ISPACE.DLL ordinal 2
// (docs/re/ispace.md), the sheet bitmap's measures of ISPACE.DLL seg6:0x11c, and the values
// seg14:0x0 reads back. The renderer that fills the cells (the rotation of ISPACE.DLL
// seg6:0x11c) is the render track's (src/gfx/ship_sheet.h); this side takes it as a
// SheetRenderer. Portable, no Win32 (M5 s1 SPRITE).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "world/records.h"
#include "world/sprite.h"

namespace is::gfx {
struct SheetArgs;
struct ShipSheet;
struct FlameHeadings;
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
    // The pixels the render put in the slot (empty when it did not render): the sheet DC as
    // seg10:0x5d3c reads it right after the build (build_flame_strip) [DESIGN] (M5 s2 FLAME).
    SheetPixels pixels;
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

// ---- the thrust flame (M5 s2 FLAME; work/analysis/m5/s2-flame.md, verified in
// s2-flame-verify.md)
//
// ISPACE.DLL ordinal 18 draws the flame in 5 directions and seg10:0x7032 mirrors them to 16
// headings (src/gfx/ship_flame.h). seg10:0x5d3c(i) composites them over ship i's sheet patches
// into the strip DC DGROUP:0x3a2c, and seg11:0xb09c(i, on) pastes strip row 2i + on into the
// ship's sheet in place. The paste is an edge event (flame on at thrust start, off at release or
// brake; src/world/flight.cpp), so the flame stays in the sheet in between, and since the
// "off" patches of headings 5 ... 15 are mirrors of headings 1 ... 4's, the first flame-off
// changes those cells for good, as the original's do [BYTES seg10:0x5e36-0x5eb6
// seg10:0x6104-0x6263] [ORACLE wave-0001 08-fly-right].

// The design words the flame reads [BYTES seg14:0x1d55-0x1d69 seg14:0x1ed6-0x1f09
// seg14:0x1c7f-0x1c88]:
//  - type, design word +0x44 (DGROUP:0x9716): 0 when SHP +0x09 bit 0x01 is set, else 1, OR 0x20
//    when SHP +0x09 bit 0x20 is set. seg10:0x5d3c uses the whole word as the flame row, so with
//    0x20 the row lies outside the 2-row flame bitmap [OPEN: what Win16 GDI blits from there;
//    the port leaves the destination as it is, as a clipped source does] [DESIGN];
//  - behind, design word +0x48 (DGROUP:0x971a): BOD +0x20 bit 0x08 (Thrust Behind Ship);
//  - radius, BOD byte +0x19 (signed), negated into design word +0x04 (DGROUP:0x96d6).
struct FlameDesign {
    std::int16_t type = 0;
    bool behind = false;
    std::int8_t radius = 0;
};
FlameDesign flame_design(std::uint8_t shp_flags, std::uint8_t bod_flags, std::int8_t bod_19);

// Design word +0x04 after seg14:0x0: -(BOD +0x19) from seg14:0x1c88, then, when negative,
// 4 - v - side / 2 (truncating) with `side` the raw cell side seg14:0x0 holds in [bp - 0x24]
// (one more at Size 0 for an odd side) [BYTES seg14:0xbb5-0xbc2 seg14:0xc42-0xc68].
std::int16_t flame_radius_word(std::int8_t bod_19, std::int16_t side);

// seg10:0x16ba(n)'s anchors, design words +0x50 + 4k (DGROUP:0x9722, x then y), k = 0 ... 4: the
// top-left of the flame's f x f box in heading k's cell, in normal mode (DGROUP:0x9130 = 0)
// [BYTES seg10:0x16ea-0x170a seg10:0x1715-0x17f8 seg10:0x17fa-0x185a seg10:0x1861-0x18a4]:
//   half = (side2a << Size) >> 1, c2 = half / 2 (truncating), r = 2 (r04 + 1), less 1 for
//   k = 0, 3, 4 and 1 more for k = 0; cl = 2 - Size;
//   D.x = (trunc(Cx[k] r / 256) >> cl) - (wx >> cl) + c2,
//   D.y = c2 - (trunc(Cy[k] r / 256) >> cl) - (wy >> cl),
// with Cx = DGROUP:0x6d6, Cy = DGROUP:0x6e0, (wx, wy) = (9,9), (7,9), (9,9), (9, side2a > 0x1d ? 10
// : 9), (Size >= 2 ? 8 : 6, 9), 16-bit products and arithmetic shifts. `side2a` is design word
// +0x2a, the raw side seg14:0x0 reads from object +0x22 before its size step [BYTES seg14:0xb97
// seg14:0xba9]. The seg18 branch (DGROUP:0x9130 != 0) is not ported [OPEN M6]. Size outside
// 0 ... 2 shifts by (2 - Size) & 0x1f as a 386 does [DESIGN].
extern const std::array<std::int16_t, 5> kFlameCx;  // DGROUP:0x6d6
extern const std::array<std::int16_t, 5> kFlameCy;  // DGROUP:0x6e0
struct FlameAnchors {
    std::array<Point16, 5> d{};
};
FlameAnchors flame_anchors(std::int16_t side2a, std::int16_t r04, int size_level);

// The strip DC DGROUP:0x3a2c and its bitmap DGROUP:0x3b14: 16 f x 16 f, black, two rows of 16
// cells per ship (row 2i the sheet patches under the anchors, row 2i + 1 the same with the
// flame), as seg10:0x5c42 creates it once [BYTES seg10:0x5c94-0x5cf2]. The pixels are logical
// palette indices, 0 black (the port's BLACKNESS index, ADR 0007 decision 2). `anchors` holds
// each ship's seg10:0x16ba result: the original keeps them in the design record [DESIGN: per
// ship, the design record is not modelled]. `dc` is the strip as the Blitter's image, uploaded
// after each build (upload_flame_strip) [DESIGN].
struct FlameStrip {
    int f = 0;
    int w = 0, h = 0;
    std::vector<std::uint8_t> px;
    std::array<FlameAnchors, 8> anchors{};
    DcId dc = kDcNone;
    std::uint8_t at(int x, int y) const { return px[static_cast<std::size_t>(y) * w + x]; }
    bool ok() const { return w > 0; }
};
// seg10:0x5c42's strip part: creates the black 16 f x 16 f strip when there is none (f =
// gfx::flame_cell). A strip of another f (Ctrl +/- changes Size) keeps the original's: it is
// created once [BYTES seg10:0x5c8f]; the port remakes it at the new f [DESIGN].
void flame_strip_create(FlameStrip& strip, int f);

// seg10:0x5d3c(i) on the strip, given the flame's 16 headings (seg10:0x7032's DGROUP:0x3c60),
// ship i's sheet as built (SheetBuild::pixels), the sheet's cell size h and gap g (object words
// +0x22 and +0x24), and the design [BYTES seg10:0x5d3c-0x6263]:
//  - has_flame (ship byte +0xe4) = 0: only the mirror step [BYTES seg10:0x5e1e-0x5e30];
//  - row 2i, cells 0 ... 4: the f x f sheet patch at (g + k (h + g) + D.x, g + D.y), SRCCOPY;
//  - row 2i + 1, cells 0 ... 4, design +0x48 = 0: row 2i, SRCAND the flame's mask, SRCPAINT the
//    flame, both from (0, type f): the flame where it is not black [BYTES seg10:0x5fa2-0x6019];
//    design +0x48 != 0: the flame, SRCAND the mask of the whole strip from (0, 0) (ship 0's row
//    0 for every i, as the original reads it), SRCPAINT row 2i [BYTES seg10:0x5ecc-0x5f9b];
//  - BLACKNESS on row 2i + 1's cells outside the ship's cell: top f x max(0, -D.y), left
//    max(0, -D.x) x f, bottom at max(0, h - D.y) of height max(0, D.y - h + f), right at
//    min(h - D.x - 1, f) of width max(0, D.x - h + f + 1) [BYTES seg10:0x601e-0x6100];
//  - rows 2i and 2i + 1 mirrored to 16 headings as gfx::flame_headings does [BYTES
//    seg10:0x6104-0x6263].
// The ordinal-6 masks are 1 where a pixel is black; the port takes index 0 as black [DESIGN].
// SRCAND / SRCPAINT work on the palette indices as an 8-bpp DC's raster operations do. Source
// pixels outside a bitmap leave the destination as it is (a clipped source) [DESIGN].
void build_flame_strip(FlameStrip& strip, const gfx::FlameHeadings& flame, int i,
                       const SheetPixels& sheet, std::int16_t size, std::int16_t gap,
                       const FlameDesign& design, const FlameAnchors& anchors,
                       std::uint8_t has_flame);
// The strip into the Blitter as an image (create_image_from), replacing the previous one.
void upload_flame_strip(Blitter& out, FlameStrip& strip);

// One f x f SRCCOPY of seg11:0xb09c: from the strip at (sx, sy) to the sheet at (dx, dy).
struct FlamePaste {
    int dx = 0, dy = 0, sx = 0, sy = 0;
};
// seg11:0xb105 to the end of seg11:0xb09c(i, on) [BYTES seg11:0xb105-0xb302]:
//  - nothing when ship byte +0xe4 (has_flame) = 0 [BYTES seg11:0xb11d-0xb127];
//  - the offsets (ox, oy) of heading h: D[h] for h < 5, (D[8-h].x, size - D[8-h].y - f) for
//    5 ... 8, (size - D[h-8].x - f, size - D[h-8].y - f) for 9 ... 11 and (size - D[16-h].x - f,
//    D[16-h].y) for 12 ... 15 [BYTES seg11:0xb152-0xb1bf seg11:0xb2d7-0xb2f7];
//  - the source (f h, (2i + on) f) [BYTES seg11:0xb1c2-0xb1e7];
//  - ship word +0x10 (`type`) <= 0 or >= 7: every heading, to (h (size + gap) + gap + ox,
//    base + gap + oy), base = size + gap for type > 3, else 0 [BYTES seg11:0xb105-0xb118
//    seg11:0xb266-0xb2ab];
//  - type 1 ... 6: heading object +0x28 only, to column 16 at (16 size + 17 gap + ox, gap + oy)
//    and (the same x, 2 gap + size + oy) [BYTES seg11:0xb1ed-0xb264 seg11:0xb2b9-0xb2d5].
// It reads neither object +0x2f nor ship byte +0xe9 (s2-flame-verify.md claim 26).
std::vector<FlamePaste> flame_pastes(int f, const FlameAnchors& anchors, int i, int on,
                                     std::int16_t type, std::int16_t heading, std::int16_t size,
                                     std::int16_t gap, std::uint8_t has_flame);
// The pastes on a sheet's pixels (the tests' path and the port's own copy of the sheet).
void apply_flame_pastes(SheetPixels& sheet, const FlameStrip& strip,
                        const std::vector<FlamePaste>& pastes);
// The pastes as the original makes them: BitBlt SRCCOPY from the strip's image to the sheet DC
// DGROUP:0x39e8[0x28 + i] [BYTES seg11:0xb2a6].
void paste_flame(Blitter& out, const ImageSlotTable& images, const FlameStrip& strip, int i,
                 const std::vector<FlamePaste>& pastes);

// The program's one strip DGROUP:0x3a2c.
FlameStrip& flame_strip();

// The two calls the drawing side makes on the program's strip:
//  - after ship i's sheet build (seg14:0x0 at seg14:0xc7e, seg14:0x11a8's shared branch at
//    seg14:0x1200, Ctrl +/- at seg10:0xa04c): seg10:0x5c42's strip (made once, f =
//    gfx::flame_cell(Size)), seg10:0x5d3c(i) from the build's pixels and the upload. `flame` is
//    seg10:0x7032's bitmap (gfx::flame_headings(gfx::flame_bitmap_for(graphic 280), f)); the
//    anchors take design +0x2a = the build's raw side (SheetBuild::side) [BYTES seg14:0xb97
//    seg14:0xba9]. Nothing without a rendered build [DESIGN];
//  - SpriteLinks::flame_tail(on, i): flame_pastes and paste_flame with ship word +0x10, object
//    +0x28, +0x22, +0x24 and ship byte +0xe4. The caller's seg13:0x6716 redraws (flight.cpp).
void ship_flame_after_sheet(Blitter& out, const gfx::FlameHeadings& flame, int i,
                            const SheetBuild& build, const ObjectRecord& obj,
                            const ShipRecord& ship, const FlameDesign& design, int size_level);
void ship_flame_tail(Blitter& out, const ImageSlotTable& images, int i, int on,
                     const ShipRecord& ship, const ObjectRecord& obj);

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
