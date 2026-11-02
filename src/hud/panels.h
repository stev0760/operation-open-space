// The status panels of the HUD strip (docs/re/world.md "Status panels"): the SCORE box and its
// digits, the bevelled panel frames, the fuel gauge, the damage panel with its lamps and the
// lamp repaint, the red button and the clock. The routines are ported from INSPACE.EXE seg11
// and draw through hud::Canvas; their state is the DGROUP words they keep. Portable.
//
// The routine bodies seg11:0x344, 0x4a0, 0x4ea, 0x5fe, 0x894, 0x978, 0x1774, 0x1ef6 and 0x2154
// are read from the bytes for this port; world.md lists them under "Panel routines" [OPEN], so
// each reading is cited at its instructions and awaits a verifier (the stage report lists them).
#pragma once

#include <cstdint>
#include <string>

#include "hud/canvas.h"
#include "hud/host.h"
#include "shell/geometry.h"

namespace is::hud {

// The DGROUP words the panel routines keep, with their file values [BYTES DGROUP:0xa9e
// DGROUP:0xaa2 DGROUP:0x1840 DGROUP:0x1868].
struct PanelState {
    std::uint32_t score_shown = 0;        // dword DGROUP:0x5018
    int lamp_shown[4] = {-2, -2, -2, -2};  // DGROUP:0xaa2 + 2k
    int fuel_shown = 0;                   // DGROUP:0x93d4
    std::uint32_t fuel_warn_until = 0;    // dword DGROUP:0x502c
    int fuel_last[8] = {};                // DGROUP:0x1840 + 2·ship
    std::uint32_t button_due = 0;         // dword DGROUP:0x5028
    int button = 0;                       // DGROUP:0x93d8 (seg11:0x0 sets it to 1)
    int button_shown = 0;                 // DGROUP:0x93da
    int seconds = 0;                      // DGROUP:0x934e, the wave's seconds
    int last_second = 0;                  // DGROUP:0x1868
};

// The score panel's text of seg11:0x4ea: "000" for 0, else seg6:0xeb8's signed decimal of the
// dword [BYTES seg11:0x519 seg6:0xeb8-0xeed].
std::string score_text(std::uint32_t score);

class Panels {
public:
    Panels(Canvas& canvas, Host& host) : c_(canvas), h_(host) {}

    PanelState& state() { return s_; }
    const PanelState& state() const { return s_; }
    // The layout table of the last repaint (seg11:0xafe).
    void set_layout(const shell::HudValues& v, int hud_top, int w) {
        v_ = v;
        hud_top_ = hud_top;
        w_ = w;
    }
    const shell::HudValues& layout() const { return v_; }

    // seg11:0x344: composes the damage panel DGROUP:0x503c (84 x 61) and the fuel gauge
    // background DGROUP:0x503e (30 x 85) from SYSTEMS.
    void compose();
    // seg11:0x4a0(arg), for the game's updates: 0 the score digits, 1 the lamps, 6 the fuel
    // level, 7 the clock; each only while seg11:0x2120 passes (`gate`).
    void update(int arg, bool gate);
    // seg11:0x4ea: the score digits, when the score changed since the last call.
    void score_digits();
    // seg11:0x5fe(force): the four lamps of the damage panel on the window DC.
    void lamps(int force);
    // seg11:0x894: the red button, every 300 ms.
    void button();
    // seg11:0x978: the fuel level.
    void fuel();
    // seg11:0x1ef6(force): the clock digits; `gate` is seg11:0x2120.
    void clock(int force, bool gate);
    // seg11:0x2154: DIGITAL glyphs for the narrow layouts (align 0, 2 or 6).
    void small_text(int align, Colour c, int x, int y, const std::string& text);
    // seg11:0x1774(style, x0, y0, x1, y1): a bevelled panel frame.
    void panel_frame(int style, int x0, int y0, int x1, int y1);
    // The lamp composition of seg11:0x3cc2(ship) into DGROUP:0x503c, before its repaint.
    void compose_lamps(int ship);

private:
    Canvas& c_;
    Host& h_;
    PanelState s_;
    shell::HudValues v_;
    int hud_top_ = 0, w_ = 0;
};

}  // namespace is::hud
