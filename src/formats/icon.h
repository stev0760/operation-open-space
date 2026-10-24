// Win16 icons: GROUP_ICON directories and the ICON images they name. An ICON resource is a
// packed DIB whose header height covers the XOR image and the 1 bpp AND mask after it; the
// GROUP_ICON is a GRPICONDIR whose 14-byte entries end in the ICON's resource id.
// docs/re/resources.md#win16-layouts. INSPACE.EXE's `GAME` group holds a 32x32 and a
// 16x16 image, both 4 bpp (docs/re/resources.md#inventory).
#pragma once

#include <cstdint>
#include <vector>

#include "formats/dib.h"
#include "formats/ne.h"

namespace is::fmt {

// GRPICONDIRENTRY. The values are copied as stored; `bytes` is the stored image size.
struct IconDirEntry {
    std::uint8_t width = 0;
    std::uint8_t height = 0;
    std::uint8_t colors = 0;  // 0 for 8 bpp and more
    std::uint8_t reserved = 0;
    std::uint16_t planes = 0;
    std::uint16_t bpp = 0;
    std::uint32_t bytes = 0;
    std::uint16_t id = 0;     // integer id of the ICON resource
};

// GRPICONDIR: reserved WORD 0, type WORD 1, count WORD, then `count` entries.
std::vector<IconDirEntry> parse_group_icon(const std::uint8_t* p, std::size_t n);

struct Icon {
    Dib image;                       // the XOR image
    std::vector<std::uint8_t> mask;  // AND mask rows in the image's row order, 4-byte padded
    std::size_t size = 0;            // exact image size: header, palette, XOR bits, AND mask

    std::size_t mask_stride() const { return ((static_cast<std::size_t>(image.width) + 31) / 32) * 4; }
    // The AND bit at (x, y), y counted from the top. 1 keeps the screen pixel.
    bool masked(int x, int y) const;
};

// The resource bytes may carry the NE alignment padding; `size` excludes it.
Icon parse_icon(const std::uint8_t* p, std::size_t n);

// Top-down 32-bit pixels in to_bgra's layout (0xAARRGGBB). Pixels whose AND bit is 0 get
// alpha 0xFF; pixels whose AND bit is 1 get alpha 0 and keep the XOR colour, which is black
// for a plain transparent pixel. A non-black XOR colour under the mask inverts the screen
// when Windows draws the icon, which alpha cannot express [DESIGN].
std::vector<std::uint32_t> icon_to_bgra(const Icon& icon);

// The bytes of a .ico file for a GROUP_ICON of `m`: ICONDIR, one 16-byte ICONDIRENTRY per
// group entry (width, height, colours, planes and bpp from the group; reserved 0; the
// exact image size and its file offset), then the images in group order, each cut to its
// exact size. The layout equals what `isre res` writes (re/isre/res_formats.py
// build_group_file). A member missing from `m` throws FormatError.
std::vector<std::uint8_t> build_ico(const NeModule& m, const NeResource& group);

}  // namespace is::fmt
