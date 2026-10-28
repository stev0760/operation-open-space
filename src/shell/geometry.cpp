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

HudValues hud_values(int w, bool race) {
    HudValues v;
    w = w16(w);
    const int half_w = w16(half(w));  // cwd; sub ax, dx; sar ax, 1
    if (w < 0x294) {                  // seg11:0xb77: jge on 660
        v.left = 1;
        v.left_w = 0x49;
    } else if (w < 0x2f8) {  // seg11:0xb8d: 760
        v.left = 1;
        v.left_w = 0x5a;
    } else {  // seg11:0xba3
        v.left = 4;
        v.left_w = 0x86;
    }
    v.target_w = w16(half_w - v.left_w - 0x60);           // seg11:0xbb7-0xbbe
    if (w < 0x2bc) v.target_w = w16(v.target_w + 9);      // seg11:0xbc1-0xbcc: 700
    v.target_x = w16(v.left_w + v.left + 4);              // seg11:0xbcf-0xbd9
    if (w < 0x2bc) v.target_x = w16(v.target_x - 2);      // seg11:0xbdc-0xbe6
    v.radar_x = w16(half_w - 0x32);                       // seg11:0xbf1-0xbf4
    v.fuel_x = w16(v.radar_x - 0x1e);                     // seg11:0xbf7-0xbfa
    if (w >= 0x2bc) v.fuel_x = w16(v.fuel_x - 4);         // seg11:0xbfd-0xc08 (jl skips)
    v.damage_x = w16(half_w + 0x44);                      // seg11:0xc13-0xc16
    if (race) v.damage_x = w16(v.damage_x - 0xe);         // seg11:0xc19-0xc23
    v.clock_x = w16(v.damage_x + 0x1c);                   // seg11:0xc26-0xc29
    v.weapon_x = w16(v.damage_x + 0x58);                  // seg11:0xc2c-0xc32
    v.weapon_w = w16(half_w - 0xa0);                      // seg11:0xc35-0xc40
    if (w < 0x2bc) v.weapon_w = w16(v.weapon_w + 4);      // seg11:0xc43-0xc4e
    if (race) v.weapon_w = w16(v.weapon_w + 0xe);         // seg11:0xc51-0xc58
    return v;
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
