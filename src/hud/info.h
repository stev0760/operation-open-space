// The two IS_INFO displays of the HUD strip (docs/re/world.md "IS_INFO displays", ADR 0013
// decision 4): port-owned panels with the original's rectangles, drawn on the surface through
// hud::Canvas. The left one (id 0xd2) shows the target display (mode 2), the message display
// (mode 3) or the frame-rate display (mode 4), the right one (id 0xdc) the weapon display. A
// PostMessage(WM_PAINT) to a window becomes invalidate(), which the next frame or repaint
// honours with flush(). Also here: the direction marks (seg13:0x24aa, seg11:0x3ed8,
// seg11:0x2ce6, seg11:0x2dac), their 280 ms deadline of the wave frame, the attitude text
// seg11:0x3f2e and the target codes seg11:0x772e / seg3:0x65e / seg3:0x45a. Portable.
#pragma once

#include <cstdint>
#include <string>

#include "hud/canvas.h"
#include "hud/host.h"
#include "shell/geometry.h"

namespace is::hud {

// IS_INFO control ids [BYTES seg11:0xd43 seg11:0xd94].
constexpr int kInfoTarget = 0xd2;  // DGROUP:0xa7c, the left display
constexpr int kInfoWeapon = 0xdc;  // DGROUP:0xa7e, the right display

// DGROUP:0xa76, the left display's mode.
enum InfoMode : int { kModeTarget = 2, kModeMessage = 3, kModeFrameRate = 4 };

// One entry of the direction-mark table DGROUP:0x6e38 + 0x1e·d, d = 0..2 [BYTES seg11:0x2ce6
// seg11:0x2dac seg13:0x24aa seg11:0x3ed8]. Entry 0's object is the target display's
// (DGROUP:0x6e52/0x6e54), entry 1's the message sender's (DGROUP:0x6e70/0x6e72), entry 2's
// DGROUP:0x6e8e/0x6e90.
struct Mark {
    int dir = 0;              // +0x00 (DGROUP:0x6e38): the direction 0..7
    int shown = 0;            // +0x02 (DGROUP:0x6e3a): the direction on screen
    int lit = 0;              // +0x04 (DGROUP:0x6e3c): drawn in colour (1) or black (0)
    Colour colour = 0;        // +0x0c (DGROUP:0x6e44): the pen colour
    int glint = 0;            // +0x14 (DGROUP:0x6e4c): seg13:0x5a6a's glint counter 0..2
    int ox = 0, oy = 0;       // +0x16/+0x18 (DGROUP:0x6e4e/0x6e50): the frame origin
    int rec = 0;              // +0x1a (DGROUP:0x6e52)
    int pool = 0;             // +0x1c (DGROUP:0x6e54)
};

// seg8:0x1290(guess, n): one Newton step for sqrt(n) from max(guess, 3), 32-bit signed
// [BYTES seg8:0x129d-0x12c4]; the loop after it is unreachable.
std::int32_t newton_root(std::int32_t guess, std::int32_t n);
// The segment of direction `dir` (0..7) of seg11:0x2dac, as MoveTo/LineTo points relative to
// the 43 x 43 frame's origin. Returns the point count (2 or 3), 0 above 7 (unsigned).
int mark_segment(int dir, Point out[3]);

class Info {
public:
    Info(Canvas& canvas, Host& host) : c_(canvas), h_(host) {}

    // DGROUP:0xa76 (Host::info_mode): seg9:0x60bc sets 2 at the wave start [BYTES seg9:0x60bc].
    int mode() const { return h_.info_mode(); }
    void set_mode(int m) { h_.set_info_mode(m); }
    // DGROUP:0xa62: the target display's MANUAL (1, the file value) or AUTO (0).
    bool manual() const { return manual_; }
    void set_manual(bool m) { manual_ = m; }
    Mark& mark(int d) { return marks_[d]; }
    const Mark& mark(int d) const { return marks_[d]; }

    // The repaint's DestroyWindow/CreateWindow of both windows [BYTES seg11:0xd05-0xda4]: the
    // panels take their rectangles and, as new windows, are due for painting. The original's
    // window procedure keeps no state per window, so the reset is the rectangles alone.
    void layout(const shell::HudLayout& l, const shell::HudValues& v);
    bool exists() const { return exists_; }
    // seg11:0x2f0 for the panels: the windows are gone, nothing paints.
    void close() { exists_ = false; dirty_[0] = dirty_[1] = false; }
    // PostMessage(window, WM_PAINT, 0, 0) [DESIGN ADR 0013 decision 4].
    void invalidate(int id);
    bool dirty(int id) const { return dirty_[id == kInfoWeapon ? 1 : 0]; }
    // Paints the panels that are due (seg11:0x22fa WM_PAINT -> seg11:0x238e).
    void flush();
    // seg11:0x238e(hwnd, hdc) for one panel, now.
    void paint(int id);

    // The wave frame's information step [BYTES seg7:0xc1c-0xc5b]: when `now` is past the
    // deadline DGROUP:0x17b8 (unsigned), the deadline becomes now + 280 and the marks update.
    void frame_step(std::uint32_t now);
    // seg13:0x24aa(d): the direction and colour of mark d, then seg11:0x3ed8(d).
    void update_mark(int d);
    // seg11:0x3ed8(d): toggles the mark's colour and redraws it on its window.
    void blink_mark(int d);
    // seg11:0x2ce6(d) on the left panel: the frame origin, then the mark.
    void draw_mark(int d);
    // seg13:0x58dc(hdc, flag) on the current view: the picture of the left display's object
    // (mark a76 - 2) through seg13:0x5a6a / seg13:0x5c22; in mode 3 an object that is gone
    // returns the display to mode 2 (M3 stage 4).
    void target_picture(int flag);
    // seg13:0x58dc(0, 1) from seg7:0x65aa: on the left panel, without a repaint.
    void target_picture_now();
    // seg11:0x3f2e(r): the attitude text DGROUP:0x93e0.
    void attitude_text(int r);
    const std::string& text_93e0() const { return text_93e0_; }

    // Ctrl+T (seg11:0x7138): DGROUP:0xa62 toggled; turning MANUAL on retargets through
    // seg13:0x2206 and seg3:0xf4; either way the left panel repaints.
    void toggle_manual();
    // seg11:0x772e(ctrl, code): the target keys T, R, I, Y, V, U of seg11:0x6b64.
    void target_code(bool ctrl, int code);
    // seg3:0x65e(pool, rec, code): the object suits the target code.
    bool code_accepts(int pool, int rec, int code) const;
    // seg3:0x45a(code): picks the next target for the code.
    void select_target(int code);
    // The B key (seg11:0x71d4): mode 4 and a repaint of the left panel.
    void show_frame_rate();
    // The M key (seg11:0x71a9): mode 3 and a repaint, unless the mode is 3 already.
    void show_messages();
    // seg7:0xadf: the frame rate was measured; mode 4 repaints.
    void frame_rate_measured();
    // seg7:0x119c(mode, 0): DGROUP:0xa76 = mode, and WM_PAINT to the left panel while it
    // exists [BYTES seg7:0x11a6-0x11bf] (M4 s1 SCREENS).
    void set_mode_119c(int m);
    // seg11:0x3e6e: unless the mode is 4, GetDC, PatBlt(0, 0, client, WHITENESS), ReleaseDC and
    // WM_PAINT to the left panel [BYTES seg11:0x3e7b-0x3ecb] (M4 s1 SCREENS).
    void flash();

    std::uint32_t deadline() const { return deadline_; }

private:
    void paint_target(const shell::Box& b);
    void paint_weapon(const shell::Box& b);
    void target_display(int width);
    void message_display();
    void frame_rate_display();
    void set_target_of(bool mark2, int pool, int rec);  // seg3:0x62e

    Canvas& c_;
    Host& h_;
    bool manual_ = true;    // DGROUP:0xa62 (1 in the file)
    Mark marks_[3];
    std::uint32_t deadline_ = 0;  // dword DGROUP:0x17b8
    std::string text_93e0_;       // DGROUP:0x93e0
    shell::Box rect_[2];          // the windows' rectangles
    shell::HudValues values_;
    int w_ = 0;                   // DGROUP:0x90ea at the last layout
    bool exists_ = false;
    bool dirty_[2] = {false, false};
};

}  // namespace is::hud
