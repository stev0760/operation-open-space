// The ship sheet: ISPACE.DLL ordinal 2 (seg6:0x0) on its plain path, the renderer
// ISPACE.DLL seg6:0x11c, as INSPACE.EXE seg14:0x11a8 calls it through seg10:0x6ee6 to build a
// ship's sprite sheet (slot 0x28 + i of the DC table DGROUP:0x39e8; docs/re/structs.md "Image
// slots", docs/re/world.md "The player's sprite (seg7:0x1208)"). The renderer measures the
// brightness of the body graphic's 16 colours, expands the 4-bpp picture to 2x into two
// brightness layers with smoothed edges, rotates the layers by a 16-entry table, averages
// every 2x2 block back down and requantises it onto the game palette's colour ramps. The
// result is an 8-bpp picture whose values are indices into hPal's logical palette.
//
// Portable and pure: the palette side (seg5:0x61b2 with the ramp records, seg5:0x66ea) comes
// in through callbacks; ship_sheet_for() wires them to gfx::PaletteManager. The branch for
// ISPACE.DLL DGROUP:0x1e9a >= 8 (max(BITSPIXEL, PLANES): 8-bpp displays and up, so both the
// oracle's 24-bpp display and the port's 8-bpp device) is the one ported [BYTES ISPACE.DLL
// seg6:0x3d4a seg5:0x3673-0x367b]; the branch below 8 bpp (Floyd-Steinberg, seg5:0x4b40) is
// not [DESIGN].
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "formats/dib.h"
#include "gfx/palette.h"

namespace is::gfx {

// ISPACE.DLL DGROUP:0x11a and DGROUP:0x13a: 512 sin(22.5 d) and -512 cos(22.5 d), the
// rotation table [BYTES ISPACE.DLL DGROUP:0x11a DGROUP:0x13a].
extern const std::array<std::int16_t, 16> kSheetSin;  // DGROUP:0x11a
extern const std::array<std::int16_t, 16> kSheetCos;  // DGROUP:0x13a

// ISPACE.DLL seg5:0x6f58: the colour-scheme row (SHP +0x07/+0x08) to the ramp id that
// seg5:0x61b2 and DGROUP:0x1dea take. Rows 12 and 0x12 and above have no case and return the
// uninitialised local [BYTES ISPACE.DLL seg5:0x6f58]; the port returns -1 [DESIGN].
int scheme_ramp_id(int scheme);
// ISPACE.DLL seg5:0x66ea(r, g, b): the ramp id of a colour (0-13), for scheme 12's colour
// classes [BYTES ISPACE.DLL seg5:0x66ea-0x6b6f]. Pure: no palette state.
int colour_ramp_id(Rgb c);

// ISPACE.DLL seg6:0x7f52(x, q): three Newton steps toward sqrt(q) from x, x raised to 2
// before each step, 32-bit with a truncating divide; the low word is returned
// [BYTES ISPACE.DLL seg6:0x7f52-0x7fc5].
int newton_root(long x, long q);
// The brightness seg6:0x11c gives a body colour: R' = r - (r >> 2), G' = g,
// B' = (b >> 1) + (b >> 3), seg6:0x7f52((R' + G' + B') / 3, (R'^2 + G'^2 + B'^2) / 3)
// [BYTES ISPACE.DLL seg6:0x1176-0x1247].
int colour_brightness(Rgb c);
// The brightness of a palette entry, as seg6:0x11c (ramp ends) and seg5:0x6b72 measure it:
// the same with B' = b >> 1 [BYTES ISPACE.DLL seg6:0xe1a-0xfc7 seg5:0x6b81-0x6c92].
int entry_brightness(Rgb c);

// ISPACE.DLL seg5:0x6b72(avg, lo, hi, p): from entry p, step one entry at a time toward hi
// (when p is brighter than avg) or lo while |brightness - avg| does not grow; stop at once
// below 3. Entries are read from `pal` [BYTES ISPACE.DLL seg5:0x6b72-0x6f57].
int nearest_ramp_entry(const PaletteArray& pal, int avg, int lo, int hi, int p);

// The arguments seg6:0x11c receives (ordinal 2's push order; ispace.md ordinal 2).
struct SheetArgs {
    int gap = 99;     // w2: the gap; 99 picks 5 for an odd height, 6 for an even one;
                      // -100 renders one picture with no gap
    int four = 1;     // w5: non-zero renders 4 directions and fills 16 by quarter turns
    int scale = 0;    // w6: the scale shift s (seg10:0x6ee6 makes it 1 for Size >= 2)
    int scheme_a = 12;  // b1: SHP +0x07, the scheme of colours 0-9 (12: keep)
    int scheme_b = 12;  // b2: SHP +0x08, the scheme of colours 10-14 (12: keep)
    int mode = 0;     // b3: 1 renders only frame 0 and the last frame and widens the sheet
};

// The ends of a colour ramp: palette entries, first (bright end) and last.
struct RampRange {
    int lo = 0, hi = 0;
};

// The palette side of seg6:0x11c.
struct SheetPalette {
    // hPal's working entries (ISPACE.DLL DGROUP:0x116c[slot] + 4, kept in DGROUP:0x1168).
    const PaletteArray* working = nullptr;
    // DGROUP:0x1e92[slot] == 0 (the game palette): ramps are looked up. Otherwise the ramp
    // tables stay unset [BYTES ISPACE.DLL seg6:0x176 seg6:0xe03].
    bool kind0 = true;
    // seg5:0x61b2(slot, c), then the record DGROUP:0x1398 + 4 (mode 19 + DGROUP:0x1dea[mode 25
    // + c]) divided by 4: the ramp of id c [BYTES ISPACE.DLL seg6:0xe12-0xe51].
    std::function<RampRange(int c)> ramp;
    // seg5:0x66ea(r, g, b): the ramp id of a colour, for scheme 12 (colour_ramp_id; empty:
    // scheme 12 groups have no ramp).
    std::function<int(Rgb)> colour_ramp;
};

// The sheet as seg6:0x11c leaves its bitmap: top-down, one value per pixel. 0-255 are indices
// into hPal's logical palette (the row DIBs go through seg5:0x2080 with DIB_PAL_COLORS);
// kBlackness marks the pixels only PatBlt(BLACKNESS) reached.
struct ShipSheet {
    static constexpr std::int16_t kBlackness = -1;
    int width = 0, height = 0;
    int cell = 0;    // h, the body height (DGROUP:0x1c72)
    int gap = 0;     // g
    int scale = 0;   // s
    int frames = 0;  // F = width / height of the body strip
    int rows = 0;    // R, the rendered rows
    int dirs = 0;    // N, DGROUP:0x1ccc
    std::vector<std::int16_t> px;
    std::int16_t at(int x, int y) const { return px[static_cast<std::size_t>(y) * width + x]; }
    // The cell of heading d in row r, as seg7:0x1208 picks it: x = d (h + g) + g,
    // y = r (h + g) + g, all << s [BYTES ISPACE.DLL seg6:0x5434-0x5474].
    int cell_x(int d) const { return (d * (cell + gap) + gap) << scale; }
    int cell_y(int r) const { return (r * (cell + gap) + gap) << scale; }
    int cell_size() const { return cell << scale; }
    bool ok() const { return width > 0; }
};

// seg6:0x11c on the DGROUP:0x1e9a >= 8 branch. `body` is the 4-bpp body strip as stored (bottom-up
// rows); `colours` its 16-entry colour table after seg5:0x26a2's recolour (the far LOGPALETTE
// DGROUP:0x1178, filled with d = 1). Returns an empty sheet when the body is not 4 bpp
// [DESIGN: every ship body of the distribution is].
ShipSheet render_ship_sheet(const fmt::Dib& body, const std::vector<std::uint32_t>& colours,
                            const SheetArgs& args, const SheetPalette& palette);

// One cell (heading 0-15, row 0 coasting or 1 thrust in mode 1), top-down.
std::vector<std::int16_t> sheet_cell(const ShipSheet& sheet, int heading, int row);

// The sheet's colours (0x00RRGGBB, top-down): indices through `pal`, kBlackness black.
std::vector<std::uint32_t> sheet_rgb(const ShipSheet& sheet, const PaletteArray& pal);

// The whole call as seg14:0x11a8 makes it for ship design n on palette `pal` of `pm`:
// seg5:0x1b8e with d = 1 and DGROUP:0x188 = 1 recolours the body's table (scheme rows a = SHP
// +0x07, b = SHP +0x08), the ramps come from seg5:0x61b2 on `pm` (command 7, which also
// counts the uses as the original does), then render_ship_sheet. w1 = 1, w3 and w4 are not
// read on this path [BYTES ISPACE.DLL seg6:0x1cf-0x1ec seg6:0x2e8 seg6:0x30c].
ShipSheet ship_sheet_for(const fmt::Dib& body, int body_id, PaletteManager& pm, PalHandle pal,
                         const SheetArgs& args);

}  // namespace is::gfx
