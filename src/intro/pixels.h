// Pixel rules of the title picture (docs/re/shell.md "Title (phase 2)", Picture): the exact 2x
// pixel doubling of seg10:0x9962's ordinal-36 path and the port's stand-in for ordinal 12's
// 2x picture. Portable, on 8-bit index rows.
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

// The palette entry that stands for colour k of graphic 300 in the ordinal-12 picture: the
// first entry of colour k's ramp step, 185 + 3k for k = 0-9 and 213 + 4(k - 10) for k = 10-14
// (palette-tables.md "Title palette at 8 bpp"), and black (0) for colour 15 [DESIGN].
int ordinal12_entry(int k);

// The port's ordinal-12 picture of a 4-bit graphic: a plain 2x doubling of the whole DIB with
// each colour k as ordinal12_entry(k), (2 w) x (2 h) top-down indices. The original's smoothed
// per-pixel rule is [OPEN] (ispace.md ordinal 12); this keeps its size and its ramp entries
// without the in-between steps [DESIGN].
std::vector<std::uint8_t> ordinal12_plain(const fmt::Dib& dib);

}  // namespace is::intro
