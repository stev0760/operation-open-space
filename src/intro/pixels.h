// Pixel rules of the title picture (docs/re/shell.md "Title (phase 2)", Picture): the exact 2x
// pixel doubling of seg10:0x9962's ordinal-36 path. Ordinal 12's smoothed 2x picture is
// gfx/smooth_render.h. Portable, on 8-bit index rows.
#pragma once

#include <cstdint>
#include <vector>

#include "formats/dib.h"

namespace is::intro {

// An 8-bit index image in memory: top-down rows of `stride` bytes.
struct IndexView {
    std::uint8_t* pixels = nullptr;
    int w = 0, h = 0, stride = 0;
};
struct ConstIndexView {
    const std::uint8_t* pixels = nullptr;
    int w = 0, h = 0, stride = 0;
};

// Copies the source rectangle (sx, sy, sw, sh) to (dx, dy, 2 sw, 2 sh), every pixel as a 2 x 2
// block: what seg10:0x9962 gives when the destination is twice the source (ordinal 36 at 8 bpp,
// "an exact pixel doubling" at 32 bpp) [BYTES seg10:0x997b seg10:0x99f3 seg10:0x9a1d]
// [ORACLE probe-0001]. Clipped to both images.
void double_copy(IndexView dst, int dx, int dy, ConstIndexView src, int sx, int sy, int sw, int sh);

}  // namespace is::intro
