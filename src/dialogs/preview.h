// The pictures of the LevClass windows (docs/re/shell.md "Game-start dialogs", Ship previews
// and Logo; docs/re/ispace.md ordinal 10): the frame of ISPACE.DLL seg10:0x309c, the ship
// bitmap of seg10:0x2aac with the slot paint seg10:0x2c68, and the Order logo of seg10:0x2f4.
// Portable: the pictures are computed here as 0x00RRGGBB pixels, and src/dialogs/dialogs.cpp
// hands them to GDI with the original's calls.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "formats/dib.h"

namespace is::dialogs {

// Top-down 0x00RRGGBB pixels.
struct Image {
    int w = 0, h = 0;
    std::vector<std::uint32_t> px;
    Image() = default;
    Image(int width, int height, std::uint32_t fill = 0)
        : w(width), h(height), px(static_cast<std::size_t>(width) * height, fill) {}
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    std::uint32_t at(int x, int y) const { return px[static_cast<std::size_t>(y) * w + x]; }
    void set(int x, int y, std::uint32_t c) {
        if (inside(x, y)) px[static_cast<std::size_t>(y) * w + x] = c;
    }
    void fill(int x0, int y0, int x1, int y1, std::uint32_t c);  // [x0, x1) × [y0, y1)
};

constexpr std::uint32_t kLtGray = 0xC0C0C0, kDkGray = 0x404040, kGray = 0x808080;

// seg10:0x309c's frame on the client rectangle (0, 0, w, h), over `bg`, with GDI's rules
// (FillRect and LineTo leave out the right and bottom edges and the end point) [BYTES ISPACE.DLL
// seg10:0x310d seg10:0x3137 seg10:0x3166 seg10:0x3198 seg10:0x31cb seg10:0x31d5 seg10:0x325a
// seg10:0x3262 seg10:0x32a3 seg10:0x32d8]. DKGRAY_BRUSH is 0x404040 [REF Win32 SDK
// GetStockObject] [ORACLE dialogs-0001 orig-1 08-newgame].
Image lev_frame(int w, int h, std::uint32_t bg = 0);

// seg10:0x2aac's bitmap for a ship: the body graphic (IGRAPHIC.DLL #4000 BOD + 0x21) through
// seg5:0x0 → ordinal 1's flag path, whose seg5:0x26a2 recolours entries 0-9 with DGROUP:0x117c
// row a = SHP + 0x07 and 10-14 with DGROUP:0x1cdc row b = SHP + 0x08 (src/gfx/palette.h,
// PaletteManager::recolour_graphic with no palette, as at true colour). Then w = bmHeight and,
// with flags ≠ 0, d = trunc((w - BOD + 0x1a)/2): flags ≥ 2 blacks (w - d, 0, d, w) and subtracts
// 2, a flag still set blacks (0, 0, d, w) [BYTES ISPACE.DLL seg10:0x2b2f seg10:0x2b62
// seg10:0x2bdb seg10:0x2bdf seg10:0x2bf8 seg10:0x2c06 seg10:0x2c2b seg10:0x2c52 seg5:0x26a2].
struct ShipBitmap {
    Image image;  // the whole recoloured body strip
    int w = 0;    // DGROUP:0x1544[slot]: bmHeight
};
ShipBitmap ship_bitmap(const fmt::Dib& body, int body_id, int a, int b, int body_1a, int flags);

// seg10:0x2c68(slot) on the 96 × 96 client: PatBlt(3, 3, 90, 90, BLACKNESS), then with a bitmap
// the 2× stretch of its w × w cell at (0, 0) to (48 - w, 48 - w), plain pixel doubling as every
// Win16 stretch mode gives it at exactly 2× [BYTES ISPACE.DLL seg10:0x2c9c seg10:0x2cd5
// seg10:0x2d09 seg10:0x2e0d].
void paint_slot(Image& client, const ShipBitmap* bitmap);

// The Order logo (seg10:0x2f4): ISPACE.DLL #4000 300 (138 × 138) and its ordinal-6 mask, white
// (true) where the image is black, patched on the first paint with BLACK_BRUSH:
// PatBlt(125, 29, 2, 2, PATCOPY), FloodFill(35, 47, 0), FloodFill(105, 48, 0) [BYTES ISPACE.DLL
// seg10:0x30e seg10:0x341 seg10:0x3a4 seg10:0x3c8 seg10:0x3dd seg10:0x3ed seg5:0x681].
struct Logo {
    Image image;
    std::vector<std::uint8_t> mask;  // 1 = white
    int w = 0, h = 0;
    bool white(int x, int y) const { return mask[static_cast<std::size_t>(y) * w + x] != 0; }
};
Logo order_logo(const fmt::Dib& graphic300);
// FloodFill(x, y, crColor 0) with a black brush on the mask: the white region around (x, y),
// 4-connected, becomes black [REF Win32 SDK FloodFill] [DESIGN: the connectivity].
void flood_black(Logo& logo, int x, int y);
// Every paint: BitBlt(4, 4, 138, 106, mask, SRCAND), BitBlt(4, 4, 138, 106, image, SRCPAINT)
// [BYTES ISPACE.DLL seg10:0x404 seg10:0x407 seg10:0x416 seg10:0x434].
constexpr int kLogoX = 4, kLogoY = 4, kLogoW = 138, kLogoH = 106;
void paint_logo(Image& client, const Logo& logo);

}  // namespace is::dialogs
