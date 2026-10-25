#include "shell/geometry.h"

#include <algorithm>

namespace is::shell {

namespace {
// 16-bit wrap, as the original's word arithmetic.
int w16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
// cdq; sub ax, dx; sar ax, 1: halving toward zero.
int half(int v) { return v / 2; }
}  // namespace

Rect clamp_show_rect(Rect s, int screen_cx, int screen_cy) {
    if (s.right <= 0) {  // seg6:0x75: jg on ShowRight
        const int w = w16(w16(screen_cx << 2) / 5);  // seg6:0x83-0x8a
        const int h = w16(w16(screen_cy << 2) / 5);
        s.left = w16(half(w16(screen_cx - w)));  // seg6:0xaf
        s.top = w16(half(w16(screen_cy - h)));   // seg6:0xc1
        s.right = w16(w + s.left);                // seg6:0xcd
        s.bottom = w16(h + s.top);                // seg6:0xd7
    }
    const int dx = w16(screen_cx - s.right);  // seg6:0xe1
    if (dx < 0) {                             // jns
        s.right = w16(s.right + dx);          // seg6:0xe7
        s.left = w16(s.left + dx);            // seg6:0xeb
    }
    const int dy = w16(screen_cy - s.bottom);  // seg6:0xf6
    if (dy < 0) {
        s.bottom = w16(s.bottom + dy);  // seg6:0xfc
        s.bottom = w16(s.bottom + dy);  // seg6:0x100: the second add also goes to ShowBottom
    }
    return s;
}

PlayArea play_area_from_size(std::uint16_t cx, std::uint16_t cy, int screen_cx, int screen_cy) {
    PlayArea a;
    a.w = static_cast<std::uint16_t>(screen_cx) < cx ? screen_cx : cx;  // seg2:0xb08 jb
    a.h = static_cast<std::uint16_t>(screen_cy) < cy ? screen_cy : cy;  // seg2:0xb27 jb
    a.w = w16(a.w);
    a.h = w16(a.h);
    a.play_h = w16(a.h - kHudHeight);   // seg2:0xb3d
    a.hud_top = w16(a.h - 85);          // seg2:0xb51
    return a;
}

PlayArea play_area_from_client(int client_right, int client_bottom, int screen_cx,
                               int screen_cy) {
    PlayArea a;
    a.w = screen_cx > client_right ? client_right : screen_cx;    // seg2:0x1af9 jg
    a.h = screen_cy > client_bottom ? client_bottom : screen_cy;  // seg2:0x1b14 jg
    a.play_h = w16(a.h - kHudHeight);  // seg2:0x1b25
    a.hud_top = w16(a.h - 85);         // seg2:0x1b35
    return a;
}

bool needs_full_screen_move(bool full, const Rect& r, int screen_cx, int screen_cy) {
    if (!full) return false;
    return !(r.left == 0 && r.top == 0 && r.width() == screen_cx && r.height() == screen_cy);
}

int border_inset(int w) {
    const int r = w16(w) % 58;  // idiv, signed remainder
    return 78 - half(58 - r);   // seg2:0xcc1-0xccd
}

Clip clip_rect(const PlayArea& a, bool border, bool play_mode) {
    Clip c;
    int l = 0, t = 0, m = 0;
    if (border) {  // seg2:0xcb8-0xcf4
        l = border_inset(a.w);
        t = 78;
        m = 20;
        c.text_line = {l, w16(a.play_h - 40), w16(a.w - 2 * l), 20};
    } else {  // seg2:0xcf9-0xd15
        c.text_line = {0, w16(a.h - 105), a.w, 20};
    }
    if (play_mode) m += 20;  // seg2:0xd22
    c.play = {l, t, w16(a.w - l), w16(a.play_h - m)};  // seg2:0xd54
    return c;
}

HudLayout hud_layout(const PlayArea& a, const HudInputs& in) {
    HudLayout h;
    h.scrollbar = {a.w / 2 + 50, a.hud_top, 14, 85};                      // seg11:0xcd9
    h.info1 = {in.panel1_x + 5, a.hud_top + 5, in.panel1_w - 9, 76};     // seg11:0xd4e
    h.info2 = {in.panel2_x + 5, a.hud_top + 5, in.panel2_w - 9, 76};     // seg11:0xd9f
    h.fill = {0, a.hud_top, a.w, 85};                                    // seg11:0xdcc
    return h;
}

int scale_factor(int key, int client_w, int client_h) {
    int f;
    if (key >= 1 && key <= kMaxScale)
        f = key;
    else
        f = client_h / kAutoScaleHeight;
    if (f < 1) f = 1;
    while (f > 1 && (client_w / f < kMinTrackWidth || client_h / f < kMinTrackHeight)) --f;
    return f;
}

Rect logical_cover(const Rect& physical, int factor, int w, int h) {
    const int f = factor > 1 ? factor : 1;
    const auto floor_div = [f](int v) { return v >= 0 ? v / f : -((-v + f - 1) / f); };
    const auto ceil_div = [f](int v) { return v >= 0 ? (v + f - 1) / f : -(-v / f); };
    Rect r{floor_div(physical.left), floor_div(physical.top), ceil_div(physical.right),
           ceil_div(physical.bottom)};
    r.left = std::clamp(r.left, 0, w);
    r.top = std::clamp(r.top, 0, h);
    r.right = std::clamp(r.right, 0, w);
    r.bottom = std::clamp(r.bottom, 0, h);
    if (r.right < r.left) r.right = r.left;
    if (r.bottom < r.top) r.bottom = r.top;
    return r;
}

}  // namespace is::shell
