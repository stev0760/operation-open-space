#include "hud/panels.h"

#include <string>

namespace is::hud {

namespace {

int w16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
int half(int v) { return w16(v) / 2; }  // cwd; sub ax, dx; sar ax, 1

constexpr Colour kGrey = 0x02808080, kSilver = 0x02C0C0C0;  // seg11:0x17b6, seg11:0x19c3

// seg6:0xf1a(value, buf): the decimal text of a signed word [OPEN: read as itoa from its call
// sites seg11:0x1f53 seg11:0x2911 seg11:0x3fb7].
std::string itoa16(int v) { return std::to_string(w16(v)); }

}  // namespace

void Panels::compose() {
    // seg11:0x344: a memory DC on SYSTEMS, and the damage panel CreateCompatibleBitmap(84, 61)
    // [BYTES seg11:0x352-0x38e].
    c_.create(Img::kDamage, 0x54, 0x3d);
    c_.frame(Img::kDamage, 0, 0, 0x53, 0x3c, 2);  // seg11:0x39e
    for (int k = 0; k < 4; ++k) {                // seg11:0x3a6-0x3f0
        const int y = w16(k * 0xe + 5);
        c_.frame(Img::kDamage, 5, y, 0x1c, 7, 3);  // seg11:0x3c0: the lamp's well
        c_.bit_blt(Img::kDamage, 0x29, y, 0x20, 0xb, Img::kSystems, 0, w16((k + 1) * 0xb),
                   kSrcCopy);  // seg11:0x3e4: the label, SYSTEMS row k + 1
    }
    // The fuel gauge background CreateCompatibleBitmap(30, 85) [BYTES seg11:0x405-0x423].
    c_.create(Img::kFuelPanel, 0x1e, 0x55);
    c_.frame(Img::kFuelPanel, 0, 0, 0x1d, 0x54, 2);   // seg11:0x438
    c_.frame(Img::kFuelPanel, 6, 0x12, 0x10, 0x3e, 3);  // seg11:0x44d: the column's well
    c_.bit_blt(Img::kFuelPanel, 5, 5, 0x15, 0xb, Img::kSystems, 0, 0, kSrcCopy);  // seg11:0x46c
}

void Panels::update(int arg, bool gate) {
    if (!gate) return;  // seg11:0x4aa-0x4b0
    switch (arg) {      // seg11:0x4d4-0x4e1
    case 0: score_digits(); break;
    case 1: lamps(0); break;
    case 6: fuel(); break;
    case 7: clock(0, gate); break;
    default: break;
    }
}

void Panels::score_digits() {
    const std::uint32_t score = h_.score();  // dword DGROUP:0x1e58
    if (score == s_.score_shown) return;    // seg11:0x4ff-0x504
    s_.score_shown = score;
    // seg11:0x50f-0x532: "000" (DGROUP:0xa9e) for 0, else seg6:0xeb8(score) [OPEN: read as the
    // decimal text of the dword from its call site].
    const std::string text = score == 0 ? std::string("000") : std::to_string(score);
    Colour c;
    if (h_.score_alt())  // seg11:0x535-0x554: DGROUP:0x951c, then DGROUP:0x104c
        c = h_.score_alt_hi() ? 0x02FFC800 : 0x02FFFFFF;
    else  // seg11:0x556-0x567: DGROUP:0x90f0
        c = h_.colour_8bit() ? 0x0200A0FF : 0x000080FF;
    const int right = w16(v_.left_w + v_.left);
    if (w_ < 0x294)  // seg11:0x56f-0x599
        small_text(2, c, w16(right - 2), w16(hud_top_ + 0x2d), text);
    else if (w_ < 0x2f8)  // seg11:0x5a1-0x5cb: font 1
        c_.font_text(1, 2, c, w16(right - 1), w16(hud_top_ + 0x31), text);
    else  // seg11:0x5cd-0x5f1: font 0
        c_.font_text(0, 2, c, w16(right - 3), w16(hud_top_ + 0x31), text);
}

void Panels::lamps(int force) {
    const int player = h_.player();
    const ShipView ship = h_.ship(player);
    // seg11:0x60c-0x61a: no status messages in the wave's first 4 seconds.
    bool quiet = w16(s_.seconds) < 4;
    for (int k = 0; k < 4; ++k) {
        // seg11:0x837-0x84d, seg11:0x627-0x68e: the lamp state from the system's health.
        int s;
        if (ship.lamp_row[k] == -1)
            s = 0x2a;
        else if (w16(ship.health[k]) > 0x4b)
            s = 0;
        else if (w16(ship.health[k]) > 0x32)
            s = 1;
        else if (w16(ship.health[k]) > 0x19)
            s = 2;
        else if (w16(ship.health[k]) > 0)
            s = 0x1e;
        else
            s = 0x2a;
        const int shown = s_.lamp_shown[k];
        if (s != shown) {  // seg11:0x856-0x866
            if (shown > s) {
                // seg11:0x70e-0x755: better again, for another view's ship only on screen 0
                if (!h_.alt_view() && h_.watch_ship() != player && s == 0 && !quiet) {
                    if (k == 1) h_.message(player, 0x52);
                    if (k == 2) h_.message(player, 0x53);
                }
            } else if (s == 0x1e && k == 0) {  // seg11:0x86a-0x887
                if (!quiet) {
                    quiet = true;
                    h_.message(player, 0xa);
                }
            } else if (s == 0x2a && !quiet) {  // seg11:0x691-0x70c
                if (k == 1) h_.message(player, 0xb);
                if (k == 2) h_.message(player, 0xc);
            }
        }
        // seg11:0x6c9-0x6e1: unchanged lamps are skipped unless forced or blinking (0x1e).
        if (force == 0 && s != 0x1e && s_.lamp_shown[k] == s) continue;
        s_.lamp_shown[k] = s;  // seg11:0x6ed
        if (s == 0x1e) s = h_.blink() ? 2 : 0x2a;  // seg11:0x6f1-0x702, seg11:0x757
        const int x = w16(v_.damage_x + 6);
        const int y = w16(k * 0xe + hud_top_ + 0x1e);
        if (s == 0x2a)  // seg11:0x762-0x796: LTGRAY_BRUSH
            c_.pat_blt(Img::kScreen, x, y, 0x1b, 6, kPatCopy, Brush::stock_object(kLtGrayBrush));
        else  // seg11:0x7aa-0x805: graphic 250, row s
            c_.bit_blt(Img::kScreen, x, y, 0x1b, 6, Img::kLamps, 0, w16(s * 6), kSrcCopy);
    }
}

// seg11:0x894, the red button blink the pass's step 1 calls (world.md "Damage panel", "Red
// button"; M3 s3 fixer, C105, C106) [BYTES seg11:0x8a1 seg11:0x96d].
void Panels::button() {
    const std::uint32_t now = h_.now();
    if (s_.button_due > now) return;  // seg11:0x8a9-0x8ae: ja (unsigned)
    s_.button_due = now + 0x12c;      // seg11:0x8b2-0x8bb
    if (h_.view_mode() == 0 && h_.watch_ship() == h_.player())
        s_.button = s_.button == 0 ? 1 : 0;  // seg11:0x8cf-0x8d8: blinks
    else
        s_.button = 1;  // seg11:0x8dd
    if (s_.button == s_.button_shown) return;  // seg11:0x8e3-0x8ea
    // seg11:0x8ee-0x941: graphic 240's cell at (DGROUP:0x93c0, H' + 2), 20 x 20.
    c_.bit_blt(Img::kScreen, v_.damage_x, w16(hud_top_ + 2), 0x14, 0x14, Img::kButton,
               w16(s_.button * 0x14), 0, kSrcCopy);
    s_.button_shown = s_.button;  // seg11:0x96a-0x96d
}

void Panels::fuel() {
    const int player = h_.player();
    const ShipView ship = h_.ship(player);
    // seg11:0x986-0x9ab: the level 0..59 from the tank in sixtieths of the design's tank.
    const int per = w16(ship.fuel_max) / 0x3c;
    if (per == 0) return;  // idiv by 0 faults in the original [DESIGN]
    int v = w16(w16(ship.fuel) / per);
    if (v > 0x3b) v = 0x3b;
    if (v == s_.fuel_shown) return;  // seg11:0x9ad-0x9b1
    s_.fuel_shown = v;
    const std::uint32_t now = h_.now();
    const int slot = player >= 0 && player < 8 ? player : 0;
    if (!(s_.fuel_warn_until >= now)) {  // seg11:0x9bc-0x9c1: jae skips
        // seg11:0x9c3-0xa0b: crossing 250, 190, 130 or 70 litres raises message 9 once per 30 s.
        for (int t = 0xfa; t > 0x3c; t -= 0x3c) {
            if (s_.fuel_last[slot] < t) continue;
            if (w16(ship.fuel) >= t) continue;
            h_.message(player, 9);
            s_.fuel_warn_until = h_.now() + 0x7530;
        }
    }
    s_.fuel_last[slot] = w16(ship.fuel);  // seg11:0xa0d-0xa1d
    const int x = w16(v_.fuel_x + 8);
    if (s_.fuel_shown < 0x3c)  // seg11:0xa21-0xa5d: LTGRAY above the level
        c_.pat_blt(Img::kScreen, x, w16(hud_top_ + 0x13), 0xe, w16(0x3c - s_.fuel_shown),
                   kPatCopy, Brush::stock_object(kLtGrayBrush));
    // seg11:0xa9f-0xacd: graphic 260's column from row 60 - v, 13 wide.
    c_.bit_blt(Img::kScreen, x, w16(hud_top_ - s_.fuel_shown + 0x4f), 0xd, s_.fuel_shown,
               Img::kFuelBar, 0, w16(0x3c - s_.fuel_shown), kSrcCopy);
}

void Panels::clock(int force, bool gate) {
    if (!gate) return;  // seg11:0x1f04-0x1f0a
    // seg11:0x1f0e-0x1f1e: the wave's seconds, the frame counter / 36 (seg5:0x2b4, unsigned
    // long division), kept in DGROUP:0x934e.
    const int secs = w16(static_cast<int>(h_.wave_ticks() / 0x24u));
    s_.seconds = secs;
    h_.set_wave_seconds(secs);
    const int minutes = secs / 0x3c;  // idiv: the word signed
    const int second = secs % 0x3c;
    if (second != s_.last_second) force = 1;  // seg11:0x1f34-0x1f3a
    if (force == 0) return;
    s_.last_second = second;
    std::string m = itoa16(minutes);
    if (minutes < 10) m = " " + m;  // DGROUP:0xac7 " "
    std::string sec = itoa16(second);
    if (second < 10) sec = "0" + sec;  // DGROUP:0xac9 "0"
    const Brush green = Brush::solid(0x0000FF00);  // seg11:0x1fd3-0x1fe2
    const int y = w16(hud_top_ + 4);
    for (std::size_t i = 0; i < m.size(); ++i) {  // seg11:0x2003-0x2091
        const int ch = static_cast<signed char>(m[i]);
        const int x = w16(static_cast<int>(i) * 10 + v_.clock_x + 5);
        if (ch > '/' && ch < ':')
            c_.bit_blt(Img::kScreen, x, y, 10, 10, Img::kDigital, w16((ch - '0') * 10), 0,
                       kMergeCopy746, green);
        else
            c_.pat_blt(Img::kScreen, x, y, 10, 10, kBlackness, Brush::none());
    }
    for (std::size_t i = 0; i < sec.size(); ++i) {  // seg11:0x2093-0x20f3
        const int ch = static_cast<signed char>(sec[i]);
        if (!(ch > '/' && ch < ':')) continue;
        const int x = w16((static_cast<int>(i) + 3) * 10 + v_.clock_x);
        c_.bit_blt(Img::kScreen, x, y, 10, 10, Img::kDigital, w16((ch - '0') * 10), 0,
                   kMergeCopy, green);
    }
}

void Panels::small_text(int align, Colour c, int x, int y, const std::string& text) {
    const int n = static_cast<int>(text.size());
    int x0;
    switch (align) {  // seg11:0x2167-0x21dc
    case 0: x0 = x; break;
    case 2: x0 = w16(x - n * 10); break;
    case 6: x0 = w16(x - half(n * 10)); break;
    default: x0 = x; break;  // [OPEN] the original leaves [bp-0xc] uninitialised
    }
    const Brush b = Brush::solid(c);  // seg11:0x2181-0x218f
    for (int i = 0; i < n; ++i) {     // seg11:0x2280-0x22cb
        const int ch = static_cast<signed char>(text[static_cast<std::size_t>(i)]);
        int cell;
        if (ch > '/' && ch < ':') {
            cell = ch - '0';
        } else {
            switch (ch) {  // seg11:0x21de-0x2246: the letters DIGITAL draws
            case 'S': cell = 5; break;
            case 'C': cell = 0xb; break;
            case 'E': cell = 0xd; break;
            case 'O': cell = 0; break;
            case 'R': cell = 0xc; break;
            default: cell = 0x64; break;
            }
        }
        const int cx = w16(i * 10 + x0);
        if (cell == 0x64)
            c_.pat_blt(Img::kScreen, cx, y, 10, 10, kBlackness, Brush::none());  // seg11:0x2225
        else
            c_.bit_blt(Img::kScreen, cx, y, 10, 10, Img::kDigital, w16(cell * 10), 0,
                       kMergeCopy746, b);  // seg11:0x2278
    }
}

void Panels::panel_frame(int style, int x0, int y0, int x1, int y1) {
    // seg11:0x1781-0x1798: style -1 is style 2 with an LTGRAY fill.
    bool filled = false;
    int n = style;
    if (n == -1) {
        n = 2;
        filled = true;
    }
    const int l = x0, t = y0, r = x1, b = y1;  // seg11:0x17a9 SetRect
    const auto line = [&](const Pen& pen, std::initializer_list<Point> pts) {
        c_.lines(Img::kScreen, pen, pts.begin(), static_cast<int>(pts.size()));
    };
    const Pen grey = Pen::solid(kGrey);
    line(grey, {{l, b}, {r, b}});                                   // seg11:0x17d4-0x17e3
    line(grey, {{l + 1, b - 1}, {r - 1, b - 1}, {r - 1, t}});       // seg11:0x17f6-0x181a
    line(grey, {{l + n + 2, b - n - 3}, {l + n + 2, t + n + 2}, {r - n - 2, t + n + 2}});
    if (n == 4) line(grey, {{r, b}, {r, t}});  // seg11:0x1871-0x188e
    if (n != 4) line(Pen::stock_object(kBlackPen), {{r, b}, {r, t}});  // seg11:0x18a5-0x18d6
    const Pen white = Pen::stock_object(kWhitePen);  // seg11:0x18e7-0x18f3
    line(white, {{l + n + 2, b - n - 2}, {r - n - 2, b - n - 2}, {r - n - 2, t + n + 1}});
    line(white, {{l, b}, {l, t}, {r, t}});                      // seg11:0x194b-0x1971
    line(white, {{l + 1, b - 1}, {l + 1, t + 1}, {r - 1, t + 1}});  // seg11:0x1976-0x19aa
    // seg11:0x19bb-0x1a34: n rectangles in silver, hollow or LTGRAY.
    const Brush fill = filled ? Brush::stock_object(kLtGrayBrush) : Brush::stock_object(kNullBrush);
    for (int k = 0; k < n; ++k)
        c_.rectangle(Img::kScreen, Pen::solid(kSilver), fill, l + k + 2, t + k + 2, r - k - 1,
                     b - k - 1);
    c_.set_pixel(Img::kScreen, r, t, kGrey);              // seg11:0x1a62
    c_.set_pixel(Img::kScreen, r - 1, t + 1, kSilver);    // seg11:0x1a7b
    c_.set_pixel(Img::kScreen, l, b, kSilver);            // seg11:0x1a90
    c_.set_pixel(Img::kScreen, l + 1, b - 1, kSilver);    // seg11:0x1aa9
}

void Panels::compose_lamps(int ship) {
    const ShipView sv = h_.ship(ship);
    for (int k = 0; k < 4; ++k) {  // seg11:0x3d01-0x3da3
        int s;
        if (k == 3 && sv.b_e9)
            s = 5;  // seg11:0x3dad-0x3dbb
        else if (k == 3 && sv.b_e7)
            s = 4;  // seg11:0x3d09-0x3d15
        else
            s = sv.lamp_row[k];  // seg11:0x3d1c-0x3d30
        const int y = w16(k * 0xe + 5);
        if (s == -1)  // seg11:0x3d39-0x3d62
            c_.pat_blt(Img::kDamage, 0x29, y, 0x20, 0xb, kPatCopy,
                       Brush::stock_object(kLtGrayBrush));
        else  // seg11:0x3d74-0x3d97: SYSTEMS row s + 1
            c_.bit_blt(Img::kDamage, 0x29, y, 0x20, 0xb, Img::kSystems, 0, w16((s + 1) * 0xb),
                       kSrcCopy);
    }
}

}  // namespace is::hud
