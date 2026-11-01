// The rock images of the wave's load, seg10:0x3a2e (called from seg9:0x22b4, which picks it or
// seg10:0x5102 by DGROUP:0x90f0): image slots 6 and 7 (DGROUP:0x39f4, 0x39f6), 869 x 53 with the 18 big-rock
// frames of 43 x 43 at (48k + 5, 5), and slots 8 and 9 (DGROUP:0x39f8, 0x39fa), 473 x 31 with
// the same frames stretched to 21 x 21 at (26k + 5, 5). Slots 6 and 8 hold BITMAP ROCKS (the
// heated rocks' MERGECOPY source), slots 7 and 9 IGRAPHIC.DLL #4000 50 in mode 3 (the colour
// rocks the rock mover draws). The rock mover's cells (setup.cpp templates: size 44 or 22, gap
// 4) read them at x = (size + gap)·frame + gap.
//
// Ported from the bytes by the M3 s2 fixer for a display of 8 bits per pixel or more
// (DGROUP:0x90f0 != 0, which the port's surface is, ADR 0007); world.md has no section for it
// yet [BYTES seg10:0x3a2e-0x3e98]. Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>

#include "world/records.h"
#include "world/sprite.h"

namespace is::world {

// The image slots seg10:0x3a2e fills.
constexpr int kSlotRockMaskBig = 6;    // DGROUP:0x39f4
constexpr int kSlotRockBig = 7;        // DGROUP:0x39f6
constexpr int kSlotRockMaskSmall = 8;  // DGROUP:0x39f8
constexpr int kSlotRockSmall = 9;      // DGROUP:0x39fa

// seg10:0x3a2e: runs only while slot 6 is empty [BYTES seg10:0x3a3d]. Sets DGROUP:0x740 = 0
// through `dg_740` [BYTES seg10:0x3aa1]. The 1-bpp CreateBitmap of slots 6 and 8 becomes an
// image of the renderer's (index) format [DESIGN ADR 0013]. seg10:0x6308(slot, -1, -1, -1) for
// slots 7, 6, 9, 8 at the end is [OPEN].
void build_rock_images(Blitter& out, ImageSlotTable& images, std::int16_t& dg_740);
// The seg9:0x22b4 graphics chain as the port has it: seg10:0x3a2e (build_rock_images), the gate
// images seg10:0x3ea2 (seg9:0x26e5) and the wave's brush tables (create_wave_brushes) into
// wave_graphics() [DESIGN: one call for the chain's images] (M3 s4 DRAW). The integration's
// load (src/hud/wave_win32.cpp load_wave_resources) calls it.
void build_wave_images(Blitter& out, ImageSlotTable& images, std::int16_t& dg_740);

// ---- the gate images seg10:0x3ea2 (work/analysis/m3/s4-draw.md 1.3)

constexpr int kSlotGate = 1;      // DGROUP:0x39ea, the colour posts (CreateCompatibleBitmap)
constexpr int kSlotGateMask = 2;  // DGROUP:0x39ec, the monochrome posts (BW_BLOCKS)
// The cell side c and gap g seg10:0x3ea2 forces (constants, not Size-scaled) and the images'
// (4c + 5g) x (2g + c) [BYTES seg10:0x40c6-0x40ea].
constexpr int kGateCell = 34, kGateGap = 6;
constexpr int kGateImageW = 4 * kGateCell + 5 * kGateGap;  // 166
constexpr int kGateImageH = 2 * kGateGap + kGateCell;      // 46
// OUT_SIGN: two 26 x 13 frames side by side [BYTES seg10:0x4202 seg10:0x4262].
constexpr int kOutSignW = 26, kOutSignH = 13;

// The wave's GDI objects beside the image slots, by their DGROUP tables. Brush index n of a
// table is the word at table + 2n; 0 is none.
struct WaveGraphics {
    std::array<BrushId, 10> heat{};   // DGROUP:0x3b74 + 2n, n = 1 ... 9 (seg10:0x4b2e)
    std::array<BrushId, 6> beam{};    // DGROUP:0x3b88 + 2n, n = 1 ... 5 (seg10:0x4b2e)
    std::array<BrushId, 15> star{};   // DGROUP:0x3b4c + 2n, n = 10 ... 14 (seg10:0x4dfe, Bright 0)
    BrushId mask = kBrushNone;        // DGROUP:0x5ea8, the sparse-dot pattern brush (seg10:0xd8)
    BrushId checker = kBrushNone;     // DGROUP:0x5eaa, the checkerboard pattern brush (seg10:0xd8)
    // OUT_SIGN as seg10:0x3ea2 loaded it. seg12:0x1776 loads and frees it on every flash; the
    // port keeps the one copy (the Blitter frees no image) [DESIGN].
    DcId out_sign = kDcNone;
    DcId gate_dc = kDcNone;           // DGROUP:0x39ea, slot 1's DC, which the flash draws on
    bool brushes = false;             // create_wave_brushes ran
};
// The program's tables (the window Blitter's objects).
WaveGraphics& wave_graphics();

// The brush colours as the original passes them to CreateSolidBrush (COLORREF 0x00bbggrr; the
// stars' 0x02 is PALETTERGB) [BYTES seg10:0x4b67-0x4bd9 seg10:0x4d44-0x4d84 seg10:0x4fda-0x501d].
extern const std::array<std::uint32_t, 10> kHeatColours;  // [1 ... 9]
extern const std::array<std::uint32_t, 6> kBeamColours;   // [1 ... 5]
extern const std::array<std::uint32_t, 15> kStarColours;  // [10 ... 14]
// The pattern rows of DGROUP:0x6c6 (DGROUP:0x5ea8) and DGROUP:0x6b6 (DGROUP:0x5eaa), shell.md
// "Pattern brushes" [BYTES DGROUP:0x6b6 DGROUP:0x6c6].
extern const std::uint8_t kMaskRows[8];
extern const std::uint8_t kCheckerRows[8];

// seg10:0x3ea2: slots 1 and 2, only while slot 1 is empty [BYTES seg10:0x3eb1]. Cell 0 at
// (g, g) is the post, cell 1 at (2g + c, g) its mirror with the right 29 columns re-mirrored,
// cells 2 and 3 at (2c + 3g, g) and (4g + 3c, g) the two cells of graphic 820 (BW_MBLOCKS);
// slot 1 carries OUT_SIGN frame 1 on cells 0 and 1. Pool-8 record 0's +0x22 = 34 and
// +0x24 = 6 [BYTES seg10:0x40ca seg10:0x40de] are the placement's values at Size 1; the Size
// rescale seg10:0x6308(1 and 2, -1, -1, -1) is not ported [OPEN Size != 1].
void build_gate_images(Blitter& out, ImageSlotTable& images, WaveGraphics& wg);
// seg10:0x4b2e's heat and beam brushes, seg10:0x4dfe's star brushes (Bright = 0, the branch at
// seg10:0x4f52) and the start-up's pattern brushes, once per Blitter [DESIGN: the port makes
// them at the wave's load] (M3 s4 DRAW).
void create_wave_brushes(Blitter& out, WaveGraphics& wg);

}  // namespace is::world
