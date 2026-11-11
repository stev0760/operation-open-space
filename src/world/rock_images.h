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
// image of the renderer's (index) format [DESIGN ADR 0013]. At the end seg10:0x6308(k, -1, -1,
// -1) for k = 7, 6, 9, 8 [BYTES seg10:0x3e5c-0x3e95] at `size_level` (DGROUP:0x9156, rescale.h
// rescale_image): at Size 2 slot 9 (8) takes slot 7's (6's) unscaled image, DGROUP:0x740 = 1 and
// the calls for 9 and 8 return at once, so the small rocks draw from the big rocks at their own
// pitch 48; at Size 0 all four are halved (869 x 53 to 434 x 26, 473 x 31 to 236 x 15) (M4 s8
// SIZE-SITES).
void build_rock_images(Blitter& out, ImageSlotTable& images, std::int16_t& dg_740, int size_level = 1);
// The seg9:0x22b4 graphics chain as the port has it: seg10:0x3a2e (build_rock_images), the gate
// images seg10:0x3ea2 (seg9:0x26e5) and the wave's brush tables (create_wave_brushes) into
// wave_graphics() [DESIGN: one call for the chain's images] (M3 s4 DRAW). The integration's
// load (src/hud/wave_win32.cpp load_wave_resources) calls it. The chain also loads the explosion
// images seg10:0x5102 (effects.h build_effect_images) with `enhance`, INI `Enhance`
// (DGROUP:0x8fe8, 1 by default, formats.md) (M4 s1 DEATH).
// `size_level` is DGROUP:0x9156, which each builder's own seg10:0x6308 reads (M4 s8 SIZE-SITES).
void build_wave_images(Blitter& out, ImageSlotTable& images, std::int16_t& dg_740,
                       std::int16_t enhance = 1, int size_level = 1);

// seg10:0x6308(-1, -1, bitmap, -1), the bitmap form of rescale.h rescale_image, for the routines
// that load a picture and rescale it before they use it (OUT_SIGN seg12:0x1b8c, graphic 230
// seg7:0x47d0, seg13:0x4693 and seg13:0x4756, graphic 360 seg7:0x256f): `bitmap` itself at Size 1,
// else a new image of ((w << Size) sar 1) x ((h << Size) sar 1) with `bitmap` deleted. The bitmap
// form touches neither the slot table nor DGROUP:0x740 [BYTES seg10:0x6316-0x664b]. kDcNone
// stays kDcNone [DESIGN: the original would GetObject a null handle] (M4 s8 SIZE-SITES).
DcId rescale_bitmap(Blitter& out, DcId bitmap, int size_level);

// ---- the launched weapons' image slots seg10:0x533a (M4 s2 MISSILE; s2-missile.md 6.2, verified
// claim 74 with its precision note)

constexpr int kSlotWeaponFirst = 0x30;  // DGROUP:0x3a48 ... DGROUP:0x3a58: slots 0x30 ... 0x38
constexpr int kSlotWeaponEnd = 0x39;    // [BYTES seg10:0x5393 seg10:0x5414]
// What seg10:0x533a(w) chose: slot k, -1 for none; `load` when it took an empty slot, which
// the caller then fills (seg10:0x6ee6, or seg10:0x4412 when W(w)+0x06 = 4) [BYTES
// seg10:0x5449-0x5461 seg10:0x53b3-0x540c].
struct WeaponSlotChoice {
    int slot = -1;
    bool load = false;
};
// seg10:0x533a(w)'s choice [BYTES seg10:0x533a-0x5464]: nothing for w = 0x1b or 0x16; else the
// first occupied slot k in 0x30 ... 0x38 that a weapon j = 0 ... 0x1b with W(j)+0x0a != 0 holds
// (W(j)-4 = k) with the same graphic W(j)-2 as w: W(w)-4 = k, nothing else written; else the
// first empty slot k: W(w)-4 = k and pool-6 record 0's `size` = W(w)-8, `gap` = W(w)-6, raw
// (the launcher's scaled values replace them at every launch); with no empty slot nothing.
WeaponSlotChoice weapon_image_slot(WeaponWords& words, const ImageSlotTable& images, Pools& pools,
                                   int w);

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
    // DGROUP:0x3b4c + 2n, n = 0 ... 19 (seg10:0x4dfe, Bright 0): the shot brushes (shots.h
    // kShotBrushes), of which the stars use 10 ... 14 (M4 s2 fixer). The Bright != 0 table
    // (kShotBrushesBright) is not selected here [OPEN: seg10:0x4dfe's Bright test at the load].
    std::array<BrushId, 20> star{};
    BrushId mask = kBrushNone;        // DGROUP:0x5ea8, the sparse-dot pattern brush (seg10:0xd8)
    BrushId checker = kBrushNone;     // DGROUP:0x5eaa, the checkerboard pattern brush (seg10:0xd8)
    // OUT_SIGN as the flash draws it: seg12:0x1776 loads BITMAP OUT_SIGN on every flash and
    // rescales it with seg10:0x6308(-1, -1, bitmap, -1) [BYTES seg12:0x1b76-0x1b94], so its
    // frames are sprite_side(26) wide at every Size; the port keeps one copy made at
    // `out_sign_level` and makes it again when Size differs (refresh_wave_graphics) [DESIGN] (M4
    // s8 SIZE-SITES).
    DcId out_sign = kDcNone;
    int out_sign_level = 1;
    DcId gate_dc = kDcNone;           // DGROUP:0x39ea, slot 1's DC, which the flash draws on
    bool brushes = false;             // create_wave_brushes ran
};
// The program's tables (the window Blitter's objects).
WaveGraphics& wave_graphics();
// The 20 brushes DGROUP:0x3b4c made again on the palette in force: a PALETTERGB brush resolves
// against the palette when it is used in the original, while the port's brush keeps the index
// it was made with (ADR 0007), so the wave start makes them again on the wave's palette
// [DESIGN ADR 0007] (M4 s2 fixer).
void refresh_shot_brushes(Blitter& out);

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
// +0x24 = 6 [BYTES seg10:0x40ca seg10:0x40de] are the placement's values at Size 1. Last,
// seg10:0x6308(1, -1, -1, -1) and seg10:0x6308(2, -1, -1, -1) [BYTES seg10:0x42c6-0x42e1] at
// `size_level`: 166 x 46 becomes 332 x 92 at Size 2 and 83 x 23 at Size 0, the cells (34 << Size)
// sar 1 with gaps (6 << Size) sar 1. `wg.out_sign` is then OUT_SIGN rescaled for the flash
// (WaveGraphics::out_sign) (M4 s8 SIZE-SITES).
void build_gate_images(Blitter& out, ImageSlotTable& images, WaveGraphics& wg, int size_level = 1);
// The port's copies of slot words in `wg` after the slot table changed (a live rescale,
// rescale.h RescaleHost::images_changed): `gate_dc` = slot 1, and OUT_SIGN made again at
// `size_level` when its copy was made at another Size (seg12:0x1776's per-flash load and
// seg10:0x6308) [DESIGN ADR 0016 decision 3] (M4 s8 SIZE-SITES).
void refresh_wave_graphics(Blitter& out, const ImageSlotTable& images, WaveGraphics& wg, int size_level);
// seg10:0x4b2e's heat and beam brushes, seg10:0x4dfe's star brushes (Bright = 0, the branch at
// seg10:0x4f52) and the start-up's pattern brushes, once per Blitter [DESIGN: the port makes
// them at the wave's load] (M3 s4 DRAW).
void create_wave_brushes(Blitter& out, WaveGraphics& wg);

}  // namespace is::world
