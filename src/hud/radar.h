// The radar (docs/re/world.md "Radar", "Scrollbar"): the 86 x 71 off-screen bitmap, its scale
// and zoom, what it shows, the refresh gate seg11:0x3396, the repaint seg11:0x33cc, the race
// repaint seg11:0x367c, the colour tables for both colour depths, the four radar_draw
// callbacks, and the zoom entry points (the plus and minus keys seg11:0x7215/0x7268, the
// scrollbar handler seg11:0x7798). It draws through world::Blitter (ADR 0013 decision 1: the
// radar blit goes to the surface that stands for the window). Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "world/records.h"
#include "world/sprite.h"
#include "world/world.h"

namespace is::hud {

using world::BrushId;
using world::DcId;

// The bitmap: DGROUP:0xa72 = 86 and DGROUP:0xa74 = 71 (words), and the same values as the
// dwords DGROUP:0xa64 and DGROUP:0xa68 the mapping multiplies by [BYTES DGROUP:0xa72
// DGROUP:0xa74 seg11:0x3464 seg11:0x349a].
constexpr int kRadarW = 86;
constexpr int kRadarH = 71;
// The zoom DGROUP:0xa6c (dword): 120 in the file, 100 ... 500 [BYTES DGROUP:0xa6c seg11:0x780c
// seg11:0x781f].
constexpr std::int32_t kZoomFile = 120;
constexpr std::int32_t kZoomMin = 100;
constexpr std::int32_t kZoomMax = 500;
// The scrollbar shows 600 - zoom [BYTES seg11:0xcf6 seg11:0x723a seg11:0x7848].
constexpr int scroll_pos_of(std::int32_t zoom) {
    return static_cast<std::int16_t>(600 - static_cast<std::int16_t>(zoom));
}
// DGROUP:0x9008 always ends as 4 (seg6:0xe30 overwrites the computed value) [BYTES seg6:0xe22
// seg6:0xe2d seg6:0xe30].
constexpr std::int16_t kRadarRateWord = 4;
// The cap of DGROUP:0x93d6 [BYTES seg11:0x3408 seg11:0x340d].
constexpr std::int16_t kRefreshCap = 26;

// ---- colours (world.md "Radar colours") [BYTES seg11:0xe9-0x17b]

constexpr int kRadarBrushes = 8;  // DGROUP:0x5004 + 2i
// Entry 3 is the pattern brush of bitmap METAL (DGROUP:0x500a), not a colour.
constexpr int kRadarBrushMetal = 3;
// The COLORREF of brush i: `deep` is DGROUP:0x90f0 != 0 (8 bits per pixel or more). Entries 0,
// 2, 5 and 7 are PALETTERGB (0x02 in the top byte); 0 and 2 have their own values on a shallow
// display. Entry 3 returns 0 (the METAL pattern brush).
std::uint32_t radar_brush_colour(int i, bool deep);

// ---- callbacks (object +0x62 `radar_draw`)

// FarPtr slots hold port handler ids, world::handler(seg, off) [DESIGN ADR 0013]; the radar's
// four are world.h's: kRadarShip seg11:0x37a0 (5 x 5 RADSHIP cell), kRadarSmall seg11:0x3914
// (SetPixel in radar_color), kRadarDot seg11:0x39d8 (PATCOPY box, brush +0x46), kRadarBig
// seg11:0x3bae (a pixel or a square by zoom).
using world::kRadarBig;
using world::kRadarDot;
using world::kRadarShip;
using world::kRadarSmall;

// What the radar reads from the wave, under the original's names.
struct RadarInputs {
    const world::Pools& pools;
    const world::ShipTable& ships;
    DcId window_dc = world::kDcWindow;  // DGROUP:0x13dc
    bool hud_gate = true;                // seg11:0x2120's result: METAL brush, DGROUP:0x9130 = 0, state 1 or 7
    std::int16_t radar_first = 0;        // DGROUP:0x9326
    std::int16_t ship_pool = world::kPoolShips;  // DGROUP:0x9324
    std::int16_t player = 0;             // DGROUP:0x9646
    std::int16_t last_ship = 0;          // DGROUP:0x9316
    std::int16_t race = 0;               // DGROUP:0x94e
    std::int16_t dg_951a = 0;            // DGROUP:0x951a: the centre is DGROUP:0x6b50/0x6b52 when not 0
    std::int16_t alt_centre_x = 0;       // DGROUP:0x6b50
    std::int16_t alt_centre_y = 0;       // DGROUP:0x6b52
    std::int16_t view_x = 0;             // DGROUP:0x2400, the view origin
    std::int16_t view_y = 0;             // DGROUP:0x2402
    std::int16_t play_w = 0;             // W, DGROUP:0x90ea
    std::int16_t play_h = 0;             // H, DGROUP:0x90ec
    std::int16_t shift = 3;              // s, DGROUP:0x9150
    std::int16_t view_rects = 1;         // DGROUP:0x9158
    std::int16_t hud_top = 0;            // H', DGROUP:0x8e5a
    std::int16_t detail = 0;             // DGROUP:0x9208 `Detail`
};

class Radar {
public:
    // seg11:0x0's radar part, once (while DGROUP:0x5042 is 0): the divisors DGROUP:0x501c and
    // DGROUP:0x5020, the 86 x 71 bitmap in DC DGROUP:0x93ce, the RADSHIP bitmap in DC
    // DGROUP:0x93cc and the brush table DGROUP:0x5004 (entry 3 is `metal`, the HUD's
    // DGROUP:0x500a) [BYTES seg11:0xd seg11:0x1b seg11:0x29 seg11:0x43 seg11:0xc6 seg11:0xe9-0x17b
    // seg11:0x182 seg11:0x19b].
    void create(world::Blitter& out, bool deep, BrushId metal);
    bool created() const { return dc != world::kDcNone; }

    // seg11:0xafe's centre: DGROUP:0x5050 = 86/2, DGROUP:0x5052 = 71/2 [BYTES seg11:0xb0c
    // seg11:0xb14 seg11:0xb1f].
    void set_centre() {
        cx = kRadarW / 2;
        cy = kRadarH / 2;
    }

    // seg11:0x3396: counts calls and repaints when ((((8 - Detail) << 4) >> 6) * DGROUP:0x93d6)
    // (16-bit) <= the count. Returns whether it repainted.
    bool refresh(const RadarInputs& in, world::Blitter& out);
    // seg11:0x33cc: recomputes DGROUP:0x93d6, redraws the bitmap and blits it to
    // (W/2 - 43, H' + 7).
    void repaint(const RadarInputs& in, world::Blitter& out);

    // seg11:0x1ab6 / seg11:0x1b44: the signed torus distance in 1/8-unit cells from the centre
    // to the 32-bit world coordinate v, as the 32-bit sign extension of a 16-bit result.
    static std::int32_t wrap_delta(std::int32_t v, std::int16_t centre_units,
                                   std::int16_t world_cells);
    std::int32_t wrap_x(const RadarInputs& in, std::int32_t v) const;
    std::int32_t wrap_y(const RadarInputs& in, std::int32_t v) const;
    // The mapping of every radar point: X = (d * zoom * 86) / DGROUP:0x501c + 43, Y likewise.
    world::Point16 map(const RadarInputs& in, std::int32_t x, std::int32_t y) const;

    // The record's radar_draw callback with (pool, record, DGROUP:0x93ce); an unknown id
    // draws nothing [OPEN].
    void draw_record(const RadarInputs& in, world::Blitter& out, int pool, int rec);

    // seg11:0x7215 (plus, +8 capped at 500) and seg11:0x7268 (minus, -8 floored at 100): sets
    // the zoom, then SetScrollPos(600 - zoom) and repaints, changed or not.
    void zoom_key(bool plus, const RadarInputs& in, world::Blitter& out);
    // The keys' tail alone, for a key switch that computes the zoom itself (world::KeyActions::
    // zoom_changed): stores it, SetScrollPos(600 - zoom), repaint [BYTES seg11:0x7233
    // seg11:0x723a seg11:0x7242 seg11:0x7247].
    void zoom_changed(std::int32_t new_zoom, const RadarInputs& in, world::Blitter& out);
    // seg11:0x7798(code, pos): SB_LINEUP +20, SB_LINEDOWN -20, SB_THUMBPOSITION and
    // SB_THUMBTRACK 600 - pos, SB_TOP 500, SB_BOTTOM 100; clamped; when it changed, repaints
    // and then SetScrollPos(600 - zoom). Returns whether it changed.
    bool scroll(int code, int pos, const RadarInputs& in, world::Blitter& out);
    // SetScrollPos(DGROUP:0xa7a, SB_CTL, pos, TRUE): the scrollbar's owner sets it.
    std::function<void(int pos)> set_scroll_pos;

    // ---- the radar's DGROUP words
    std::int32_t zoom = kZoomFile;          // DGROUP:0xa6c (dword)
    std::int16_t cx = kRadarW / 2;          // DGROUP:0x5050
    std::int16_t cy = kRadarH / 2;          // DGROUP:0x5052
    std::int32_t div_x = 0;                 // DGROUP:0x501c = 100 * DGROUP:0x5a0
    std::int32_t div_y = 0;                 // DGROUP:0x5020 = 100 * DGROUP:0x5a2
    world::Rect16 clip;                     // DGROUP:0x5048
    std::int16_t refresh_div = 0;           // DGROUP:0x93d6 (BSS)
    std::int16_t counter = 0;               // DGROUP:0x93de (BSS)
    std::int16_t rate_word = kRadarRateWord;  // DGROUP:0x9008
    DcId dc = world::kDcNone;               // DGROUP:0x93ce, the radar bitmap's DC
    DcId ship_dc = world::kDcNone;          // DGROUP:0x93cc, RADSHIP
    DcId dots_dc = world::kDcNone;          // DGROUP:0x93d0, the race dots' backing; its creator is [OPEN]
    std::array<BrushId, kRadarBrushes> brushes{};  // DGROUP:0x5004
    // DGROUP:0x5054 + 4k and DGROUP:0x5056 + 4k: the race dots, one per ship.
    std::array<std::int16_t, world::kShipCount> dot_x{}, dot_y{};

private:
    void race_repaint(const RadarInputs& in, world::Blitter& out);  // seg11:0x367c
    BrushId brush(int i) const {
        return i >= 0 && i < kRadarBrushes ? brushes[static_cast<std::size_t>(i)]
                                           : world::kBrushNone;
    }
};

}  // namespace is::hud
