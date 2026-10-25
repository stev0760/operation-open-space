// The main window's geometry rules of docs/re/shell.md "Window geometry": the Show rectangle
// default and edge corrections, the play area and HUD strip, the clip rectangle with the
// Border and PlayMode insets, and the HUD element rectangles. The arithmetic is the
// original's 16-bit arithmetic. Portable.
#pragma once

#include <cstdint>

namespace is::shell {

struct Rect {
    int left = 0, top = 0, right = 0, bottom = 0;
    int width() const { return right - left; }
    int height() const { return bottom - top; }
    bool operator==(const Rect& o) const {
        return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
    }
};

// An (x, y, cx, cy) rectangle, as the original stores DGROUP:0x1d28 and creates children.
struct Box {
    int x = 0, y = 0, cx = 0, cy = 0;
    bool operator==(const Box& o) const {
        return x == o.x && y == o.y && cx == o.cx && cy == o.cy;
    }
};

// Window styles (shell.md "Start-up" step 15, "Mode switching").
constexpr std::uint32_t kStyleFull = 0x80000000u;      // WS_POPUP
constexpr std::uint32_t kStyleWindowed = 0x80CF0000u;  // WS_POPUP | WS_OVERLAPPEDWINDOW bits
constexpr std::uint32_t kWindowedBits = 0x00CF0000u;   // ORed in by the double click, Alt+B
constexpr std::uint32_t kFullScreenMask = 0xFF30FFFFu; // seg2:0xfc4 ANDs the style with it

// DGROUP:0xa70, the HUD strip at the bottom of the client area [BYTES DGROUP:0xa70].
constexpr int kHudHeight = 85;
// The minimum track size while DGROUP:0x22 is set [BYTES seg2:0x3f6].
constexpr int kMinTrackWidth = 600, kMinTrackHeight = 400;

// seg6:0x0 [BYTES seg6:0x75-0x100]: when ShowRight <= 0 the four values become a centred
// rectangle of 4/5 of the screen; then a rectangle past the right edge moves left by the
// excess, and one past the bottom loses twice the excess from ShowBottom (both adds go to
// DGROUP:0x1d26) while ShowTop stays.
Rect clamp_show_rect(Rect show, int screen_cx, int screen_cy);

// The play-area globals.
struct PlayArea {
    int w = 0;         // DGROUP:0x90ea, the play width
    int h = 0;         // DGROUP:0x10, the client height
    int play_h = 0;    // DGROUP:0x90ec = h - DGROUP:0xa70
    int hud_top = 0;   // DGROUP:0x8e5a = h - 85
};

// WM_SIZE seg2:0xa74 with client size (cx, cy) from lParam: unsigned minimum with the screen
// size [BYTES seg2:0xafc-0xb3d], then DGROUP:0x8e5a [BYTES seg2:0xb41-0xb51].
PlayArea play_area_from_size(std::uint16_t cx, std::uint16_t cy, int screen_cx, int screen_cy);
// seg2:0x1ac8 and seg2:0xba4: the client rectangle's right and bottom, each replaced by the
// screen size unless the screen size is greater (signed) [BYTES seg2:0x1af6-0x1b38].
PlayArea play_area_from_client(int client_right, int client_bottom, int screen_cx,
                               int screen_cy);

// WM_SIZE with Full set: true when the window rectangle is not (0, 0, screen) and the handler
// moves the window back to full screen instead of updating the size [BYTES seg2:0xab0-0xaf5].
bool needs_full_screen_move(bool full, const Rect& window, int screen_cx, int screen_cy);

// seg2:0xca4: the play rectangle DGROUP:0x1d30, which is also the clip region DGROUP:0x12, and
// the text-line rectangle DGROUP:0x1d28.
struct Clip {
    Rect play;      // (L, T, W - L, DGROUP:0x90ec - m)
    Box text_line;  // DGROUP:0x1d28..0x1d2e
};
Clip clip_rect(const PlayArea& a, bool border, bool play_mode);
// The Border inset L of seg2:0xca4 and of seg11:0xafe's border tiles: 78 - (58 - W mod 58) / 2.
int border_inset(int w);

// seg11:0xafe's HUD children and fill (shell.md "HUD"). The panel origins and widths
// (DGROUP:0x93ba, DGROUP:0x5024, DGROUP:0x93c4, DGROUP:0x5026) come from the HUD drawing,
// which M3 ports; they are inputs here.
struct HudInputs {
    int panel1_x = 0;  // DGROUP:0x93ba
    int panel1_w = 0;  // DGROUP:0x5024
    int panel2_x = 0;  // DGROUP:0x93c4
    int panel2_w = 0;  // DGROUP:0x5026
};
struct HudLayout {
    Box scrollbar;  // id 200, only while DGROUP:0x94e = 0
    Box info1;      // IS_INFO id 210
    Box info2;      // IS_INFO id 220
    Box fill;       // the PATCOPY fill of the strip
};
HudLayout hud_layout(const PlayArea& a, const HudInputs& in);

}  // namespace is::shell
