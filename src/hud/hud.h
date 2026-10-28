// The wave's play-area frame and status display (docs/re/world.md "Play-area frame and status
// display", ADR 0013 decisions 1, 3, 4 and 5): the repaint routine seg11:0xafe with its gate
// seg11:0x2120, the layout table (shell::hud_values), the children, the METAL strip, the Border
// switch with the circuit-board tiles (seg11:0x170a) and the bevel, the HUD line, the status
// panels (src/hud/panels.h), and the state dispatch seg11:0x15b3 for states 1 and 7; the lamp
// repaint seg11:0x3cc2; the clip region and message line of seg2:0xca4 (shell::clip_rect).
// Everything draws through hud::Canvas. Portable.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "hud/canvas.h"
#include "hud/host.h"
#include "hud/info.h"
#include "hud/panels.h"
#include "shell/geometry.h"

namespace is::hud {

// ---- the border (world.md "Border and bevel")

// seg11:0x170a(reset), the tile choice: the counter DGROUP:0x1852 (0 in the file) is reset to
// -1 by a non-zero argument, incremented by every call, set to 0 above 6, and maps 0..6 to
// t = 0, 1, 2, 1, 0, 2, 1 [BYTES seg11:0x1717-0x1769 seg11:0x175b].
class TileCounter {
public:
    int next(int reset);
    int counter() const { return counter_; }

private:
    std::int16_t counter_ = 0;  // DGROUP:0x1852
};

// One border tile: BitBlt SRCCOPY of (w x h) from bitmap DGROUP:0x5044 at (58·t, 0).
struct Tile {
    int x = 0, y = 0, w = 0, h = 0, t = 0;
    bool operator==(const Tile& o) const {
        return x == o.x && y == o.y && w == o.w && h == o.h && t == o.t;
    }
};
// The tiles of seg11:0xdec-0xf7d in drawing order for play width `w` and HUD top `hud_top`
// (DGROUP:0x8e5a): the top row, the left column and its partial tile, the right column and its
// partial tile. The reset call seg11:0xe19(1) comes first.
std::vector<Tile> border_tiles(int w, int hud_top, TileCounter& counter);
// The border inset off = (58 - W mod 58) / 2 of seg11:0xdec-0xdfe (10 at W = 1024).
int tile_offset(int w);

// One bevel band: PatBlt PATCOPY of (w x h) with a solid PALETTERGB(v, v, v) brush.
struct Band {
    int x = 0, y = 0, w = 0, h = 0;
    Colour colour = 0;
    bool operator==(const Band& o) const {
        return x == o.x && y == o.y && w == o.w && h == o.h && colour == o.colour;
    }
};
// The 40 bands of seg11:0xf95-0x11a2 in drawing order: ten top bands, ten bottom, ten left,
// ten right [BYTES seg11:0xf9a seg11:0x101a seg11:0x10a5 seg11:0x1122].
std::vector<Band> bevel_bands(int w, int hud_top);

// ---- the children of the repaint (ADR 0013 decisions 4 and 5)

// What seg11:0xafe's DestroyWindow/CreateWindow pairs become: the scrollbar control is created
// once and moved (src/hud/scrollbar_win32.cpp); `race` is DGROUP:0x94e, which suppresses it.
class Children {
public:
    virtual ~Children() = default;
    virtual void layout(const shell::HudLayout& layout, bool race) = 0;
};

// ---- the repaint routine

class Hud {
public:
    Hud(Canvas& canvas, Host& host, Children* children = nullptr);

    Panels& panels() { return panels_; }
    Info& info() { return info_; }
    TileCounter& tiles() { return tiles_; }

    // seg11:0x0's HUD part has run: the bitmaps are in the canvas, the strip brush DGROUP:0x500a
    // exists, so the repaint's gate can pass. It composes the panels (seg11:0x344) and sets
    // DGROUP:0x93d8 = 1 [BYTES seg11:0x1cf seg11:0x1d2].
    void loaded();
    bool is_loaded() const { return loaded_; }

    // seg11:0x2120: brush DGROUP:0x500a != 0, DGROUP:0x9130 = 0 and state 1 or 7.
    bool gate() const;
    // seg11:0xafe. Its srand(GetCurrentTime()) [BYTES seg11:0xb22-0xb28] is Host::seed_rng.
    void repaint();
    // seg11:0x3cc2(ship): composes the lamps into DGROUP:0x503c; with DGROUP:0x951a set it blits
    // the panel and calls seg11:0x5fe(1), otherwise it runs the repaint. Returns true when it
    // ran the repaint: the caller then sets DGROUP:0x8f64 = (seg13:0x65f2(0) != 0).
    bool lamp_repaint(int ship);

    // The values of the last repaint: the layout table and the children's rectangles.
    const shell::HudValues& values() const { return values_; }
    const shell::HudLayout& layout() const { return layout_; }

private:
    void border(const shell::PlayArea& a);
    void score_box(const shell::PlayArea& a);
    void dispatch();

    Canvas& c_;
    Host& h_;
    Children* children_;
    Panels panels_;
    Info info_;
    TileCounter tiles_;
    bool loaded_ = false;
    shell::HudValues values_;
    shell::HudLayout layout_;
};

}  // namespace is::hud
