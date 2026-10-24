// Device-independent bitmaps as the original stores them: file-format DIBs (IGRAPHIC.DLL
// #4000, with BITMAPFILEHEADER) and packed DIBs (BITMAP resources). 1, 4, 8 and 24 bpp,
// BI_RGB only. docs/re/resources.md#custom-types.
#pragma once

#include <cstdint>
#include <vector>

namespace is::fmt {

struct Dib {
    int width = 0;
    int height = 0;  // always positive
    int bpp = 0;
    std::vector<std::uint32_t> palette;  // 0x00RRGGBB
    std::vector<std::uint8_t> bits;      // rows as stored, each padded to 4 bytes
    bool bottom_up = true;

    std::size_t stride() const { return ((static_cast<std::size_t>(width) * bpp + 31) / 32) * 4; }
    // Palette index (bpp <= 8) of the pixel at (x, y), y counted from the top.
    std::uint8_t index_at(int x, int y) const;
    // 0x00RRGGBB colour of the pixel at (x, y).
    std::uint32_t rgb_at(int x, int y) const;
};

// Accepts both forms: with a "BM" file header or a bare BITMAPINFOHEADER/BITMAPCOREHEADER.
Dib parse_dib(const std::uint8_t* p, std::size_t n);

// Top-down 32-bit pixels, 0xAARRGGBB as a little-endian DWORD (B, G, R, A bytes, the
// Win32 32 bpp DIB order). Pixels of `transparent_index` get `background` instead
// (alpha 0); every other pixel has alpha 0xFF.
std::vector<std::uint32_t> to_bgra(const Dib& d, int transparent_index = -1,
                                   std::uint32_t background = 0);

}  // namespace is::fmt
