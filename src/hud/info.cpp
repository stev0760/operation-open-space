#include "hud/info.h"

#include <string>

namespace is::hud {

namespace {

int w16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
// cwd; sub ax, dx; sar ax, 1: a signed halving toward zero.
int half(int v) { return w16(v) / 2; }

// The 0x1e-byte stride of DGROUP:0x6e38 [BYTES seg11:0x2d00 seg13:0x25dd].
constexpr Colour kMarkWhite = 0x02FFFFFF, kMarkYellow = 0x0200FFFF, kMarkGreen = 0x0200FF00,
                 kMarkDark = 0x02007200;
constexpr Colour kRule = 0x00008000;     // seg11:0x24a4, seg11:0x285f
constexpr Colour kWeaponRule = 0x00FF00FF;  // seg11:0x2b6b [OPEN Weapon rule colour] (M3 s2 fixer)
constexpr Colour kOther = 0x00008000;    // a target that is not a ship [BYTES seg11:0x266e]
constexpr Colour kYellow = 0x0000FFFF;   // seg11:0x2816-0x281b

// The attitude colour of a ship target [BYTES seg11:0x2640-0x2669 seg11:0x2784-0x27a7].
Colour attitude_colour(int r) {
    if (r > 0x50) return 0x02FFB900;
    if (r < -0x50) return 0x02006EFF;
    return 0x02AAAAAA;
}

}  // namespace

std::int32_t newton_root(std::int32_t guess, std::int32_t n) {
    if (guess < 3) guess = 3;  // seg8:0x129d-0x12a4: jge
    // idiv: truncating division; the sum and the shift stay 32-bit (sar eax, 1).
    const std::int64_t s = static_cast<std::int64_t>(n / guess) + guess;
    return static_cast<std::int32_t>(s >> 1);
}

int mark_segment(int dir, Point out[3]) {
    // seg11:0x2dac's jump table [BYTES seg11:0x2f50]: (x0, y0) = the origin, sides 14, 29, 43.
    switch (static_cast<unsigned>(dir)) {
    case 0: out[0] = {14, 0}; out[1] = {29, 0}; return 2;
    case 1: out[0] = {29, 0}; out[1] = {43, 0}; out[2] = {43, 14}; return 3;
    case 2: out[0] = {43, 14}; out[1] = {43, 29}; return 2;
    case 3: out[0] = {29, 43}; out[1] = {43, 43}; out[2] = {43, 29}; return 3;
    case 4: out[0] = {14, 43}; out[1] = {29, 43}; return 2;
    case 5: out[0] = {14, 43}; out[1] = {0, 43}; out[2] = {0, 29}; return 3;
    case 6: out[0] = {0, 14}; out[1] = {0, 29}; return 2;
    case 7: out[0] = {14, 0}; out[1] = {0, 0}; out[2] = {0, 14}; return 3;
    default: return 0;  // ja seg11:0x2f60 [BYTES seg11:0x2f43-0x2f46]
    }
}

// ---- the windows

void Info::layout(const shell::HudLayout& l, const shell::HudValues& v) {
    rect_[0] = l.info1;  // seg11:0xd4e: (DGROUP:0x93ba + 5, H' + 5, DGROUP:0x5024 - 9, 76)
    rect_[1] = l.info2;  // seg11:0xd9f: (DGROUP:0x93c4 + 5, H' + 5, DGROUP:0x5026 - 9, 76)
    values_ = v;
    w_ = h_.area().w;
    exists_ = true;
    // New windows get their first WM_PAINT after the repaint returns [DESIGN ADR 0013
    // decision 4].
    dirty_[0] = dirty_[1] = true;
}

void Info::invalidate(int id) {
    if (!exists_) return;  // PostMessage to a zero handle does nothing
    dirty_[id == kInfoWeapon ? 1 : 0] = true;
}

void Info::flush() {
    if (!exists_) return;
    if (dirty_[0]) paint(kInfoTarget);
    if (dirty_[1]) paint(kInfoWeapon);
}

// seg11:0x22fa WM_PAINT: InvalidateRect(hwnd, NULL, TRUE), BeginPaint, seg11:0x238e(hwnd, hdc),
// EndPaint [BYTES seg11:0x2326-0x235c]. The class has no background brush to erase with; the
// paint's PatBlt BLACKNESS covers the client area [BYTES seg11:0x23ed-0x2400].
void Info::paint(int id) {
    if (!exists_) return;
    const int k = id == kInfoWeapon ? 1 : 0;
    dirty_[k] = false;
    const shell::Box& b = rect_[k];
    if (b.cx <= 0 || b.cy <= 0) return;
    c_.set_view(b.x, b.y, b.cx, b.cy);
    c_.pat_blt(Img::kScreen, 0, 0, b.cx, b.cy, kBlackness, Brush::none());
    if (id == kInfoTarget)
        paint_target(b);
    else
        paint_weapon(b);
    c_.clear_view();
}

void Info::paint_target(const shell::Box& b) {
    // seg11:0x240c-0x2448: font DGROUP:0x90fe, TA_LEFT, TRANSPARENT; white on a 1-bit display,
    // else green. The colour stays until a later SetTextColor (TextOut takes the current one).
    switch (mode()) {
    case kModeTarget:
        if (h_.race())
            h_.draw_race(c_);  // seg11:0x2475: seg11:0x41bc(hdc, 1, 1)
        else
            target_display(b.cx);
        break;
    case kModeMessage: message_display(); break;
    case kModeFrameRate: frame_rate_display(); break;
    default: break;  // seg11:0x2464: any other mode draws only the rule
    }
    // seg11:0x247b-0x24e0: colour and font restored; every mode but 3 ends with the rule.
    if (mode() != kModeMessage) {
        const Point p[2] = {{0, 14}, {b.cx, 14}};
        c_.lines(Img::kScreen, Pen::solid(kRule), p, 2);
    }
}

void Info::target_display(int width) {
    const Colour title = h_.monochrome() ? 0x00FFFFFF : 0x0000FF00;  // seg11:0x242f-0x2448
    Mark& m = marks_[0];
    // seg11:0x24e3-0x2514: an object that is no target and of kind 0 bumps DGROUP:0x9498.
    if (!h_.target_ok(m.pool, m.rec) && h_.target_kind(m.pool, m.rec) == 0) h_.bump_bad_index();
    draw_mark(0);             // seg11:0x251e: seg11:0x2ce6(hdc, 0)
    target_picture(1);        // seg11:0x2529: seg13:0x58dc(hdc, 1)
    const bool narrow = w_ < 0x2bc;  // seg11:0x2531: jge 700
    const int margin = narrow ? 0xc : 0xf;
    // The title: DGROUP:0xacb "" or string 930, then 931, 932 or 933, then DGROUP:0xacc ")".
    std::string t = narrow ? std::string() : h_.lang(0x3a2);
    t += h_.lang(0x3a3);
    t += h_.lang(manual_ ? 0x3a5 : 0x3a4);  // seg11:0x259b: AUTO when DGROUP:0xa62 = 0
    t += ")";
    c_.text_out(Img::kScreen, 1, 0, kTaLeft, title, t);  // seg11:0x25f4
    // seg11:0x25f9: TA_RIGHT from here on.
    const int x = w16(values_.target_w - margin);
    c_.text_out(Img::kScreen, x, 0x12, kTaRight, title, h_.text_9628());  // seg11:0x261e
    const bool ship = m.pool == h_.ship_pool();
    int r = 0;
    Colour c = kOther;
    if (ship) {
        r = h_.attitude(m.rec, h_.player());  // seg11:0x2634: seg13:0x2688(DGROUP:0x6e52, player)
        c = attitude_colour(r);
    }
    c_.text_out(Img::kScreen, x, 0x20, kTaRight, c, h_.text_8718());  // seg11:0x2693
    c_.text_out(Img::kScreen, x, 0x2e, kTaRight, c, h_.text_960a());  // seg11:0x26b3
    if (ship) {
        attitude_text(r);                                                // seg11:0x26c8
        c_.text_out(Img::kScreen, x, 0x3c, kTaRight, c, text_93e0_);  // seg11:0x26e7
    }
    (void)width;
}

void Info::message_display() {
    const Colour title = h_.monochrome() ? 0x00FFFFFF : 0x0000FF00;
    draw_mark(1);             // seg11:0x26f5: seg11:0x2ce6(hdc, 1)
    target_picture(1);        // seg11:0x2700: seg13:0x58dc(hdc, 1)
    c_.text_out(Img::kScreen, 1, 0, kTaLeft, title, h_.lang(0x3a1));  // seg11:0x2758: 929
    const Mark& m = marks_[1];
    if (m.pool == h_.ship_pool()) {  // seg11:0x2767
        const int r = h_.attitude(m.rec, h_.player());
        const Colour c = attitude_colour(r);
        c_.text_out(Img::kScreen, 1, 0x12, kTaLeft, c, h_.text_8740());  // seg11:0x27c7
        attitude_text(r);                                                // seg11:0x27d1
        if (m.rec != h_.player())                                         // seg11:0x27d5
            c_.text_out(Img::kScreen, 1, 0x20, kTaLeft, c, text_93e0_);
    } else {
        c_.text_out(Img::kScreen, 1, 0x12, kTaLeft, kOther, h_.text_8740());  // seg11:0x27ef
    }
    c_.text_out(Img::kScreen, 1, 0x2e, kTaLeft, kYellow, h_.text_8768());  // seg11:0x2837
    c_.text_out(Img::kScreen, 1, 0x3c, kTaLeft, kYellow, h_.text_87b8());  // seg11:0x2851
    const Point p[2] = {{0, 14}, {0x50, 14}};                             // seg11:0x2870-0x2883
    c_.lines(Img::kScreen, Pen::solid(kRule), p, 2);
}

void Info::frame_rate_display() {
    const Colour title = h_.monochrome() ? 0x00FFFFFF : 0x0000FF00;
    c_.text_out(Img::kScreen, 1, 0, kTaLeft, title, h_.lang(0x3a0));  // seg11:0x28d6: 928
    std::string s;
    const int set = h_.frame_rate_set();
    if (set == 0x4b0)  // seg11:0x28db: FULL OUT at 1200
        s = h_.lang(0x88f);
    else
        s = std::to_string(w16(set)) + h_.lang(0x39f) + h_.lang(0x890);  // seg11:0x2907-0x295e
    c_.text_out(Img::kScreen, 1, 0x16, kTaLeft, title, s);  // seg11:0x2978
    s = std::to_string(w16(h_.frame_rate_actual())) + h_.lang(0x39f) + h_.lang(0x891);
    c_.text_out(Img::kScreen, 1, 0x26, kTaLeft, title, s);  // seg11:0x29e8 -> 0x26e7
}

void Info::paint_weapon(const shell::Box& b) {
    // seg11:0x29eb-0x2a16: font DGROUP:0x90fe, TA_LEFT, TRANSPARENT, red.
    const Colour red = 0x000000FF;
    c_.text_out(Img::kScreen, 1, 0, kTaLeft, red, h_.lang(0x3a6));  // seg11:0x2a5b: 934
    const WeaponView w = h_.weapon();
    std::string s;
    if (w.counted && !w.no_count) {  // seg11:0x2a72-0x2a7e
        s = std::to_string(static_cast<std::int8_t>(w.count)) + " - ";  // DGROUP:0xace
    } else if (w.name_id == 0xc9 || w.name_id == 0xba || w.name_id == 0xbc) {
        s.clear();  // DGROUP:0xad2 ""
    } else {
        s = h_.lang(0x3a7);  // seg11:0x2ae8: 935 "FUEL - "
    }
    s += h_.lang(w.name_id);
    c_.text_out(Img::kScreen, 1, 0x11, kTaLeft, red, s);  // seg11:0x2b48
    const Point p[2] = {{0, 15}, {b.cx, 15}};              // seg11:0x2b64-0x2b94
    c_.lines(Img::kScreen, Pen::solid(kWeaponRule), p, 2);
    if (!c_.has(Img::kWeapon)) return;  // seg11:0x2baa: DGROUP:0x5040 = 0
    const int bw = c_.width(Img::kWeapon), bh = c_.height(Img::kWeapon);
    int y;
    if (w.counted && w.name_id != 0xc4 && w.name_id != 0xc6 && w.name_id != 0xcb)
        y = 0x1c;  // seg11:0x2c28
    else if (w.name_id == 0xcb)
        y = 0x28;
    else
        y = 0x23;
    const int x = w16(half(values_.weapon_w) - half(bw));  // seg11:0x2c46-0x2c58
    c_.bit_blt(Img::kScreen, x, y, bw, bh, Img::kWeapon, 0, 0, kSrcPaint);  // seg11:0x2c73
}

// ---- the direction marks

void Info::frame_step(std::uint32_t now) {
    if (!(now > deadline_)) return;  // seg7:0xc24: cmp; jbe (unsigned, strictly greater)
    deadline_ = now + 0x118;         // seg7:0xc2b-0xc34
    if (mode() == kModeTarget)
        update_mark(0);  // seg7:0xc3f-0xc4c
    else if (mode() == kModeMessage)
        update_mark(1);  // seg7:0xc4a
    if (h_.mark2_window()) update_mark(2);  // seg7:0xc52-0xc5b
}

void Info::update_mark(int d) {
    int pool = 0, rec = 0;
    if (d == 0) {
        if (h_.race()) return;                     // seg13:0x24bf
        if (h_.player_target_pool() == -1) return;  // seg13:0x24ce
        const int tp = h_.player_target_pool(), tr = h_.player_target_rec();
        if (!h_.target_ok(tp, tr) && h_.target_kind(tp, tr) != 1) {  // seg13:0x24e2-0x2501
            const int t = h_.next_target();                           // seg13:0x2504
            // seg3:0xf4(player, high byte, low byte) [BYTES seg13:0x250a-0x2518]
            h_.set_target(h_.player(), (t >> 8) & 0xff, t & 0xff);
        }
        pool = h_.player_target_pool();  // seg13:0x2520-0x252d
        rec = h_.player_target_rec();
    } else if (d == 1) {
        if (marks_[1].pool == -1) return;  // seg13:0x2539
        pool = marks_[1].pool;
        rec = marks_[1].rec;
    } else if (d == 2) {
        pool = marks_[2].pool;
        rec = marks_[2].rec;
    } else {
        return;  // [OPEN] the original reads uninitialised words for d > 2
    }
    if (pool < 0) return;  // seg13:0x255f
    const Point t = h_.screen_point(pool, rec);
    const Point p = h_.player_point();
    const int dx = w16(t.x - p.x), dy = w16(t.y - p.y);  // seg13:0x25c0-0x25cf
    Mark& m = marks_[d];
    m.dir = half(h_.direction16(dx, dy));  // seg13:0x25d3-0x25e1: seg8:0x19b0 / 2
    // seg13:0x25e5-0x262c: seg8:0x1290(|dx| + |dy|, dx·dx + dy·dy), 32-bit.
    const std::int32_t sq = static_cast<std::int32_t>(dx) * dx + static_cast<std::int32_t>(dy) * dy;
    const std::int32_t manhattan = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    const int dist = w16(newton_root(manhattan, sq));
    if (dist < 0x12c)
        m.colour = kMarkWhite;  // seg13:0x2639
    else if (dist < 0x1e0)
        m.colour = kMarkYellow;
    else if (dist < 0x2e4)
        m.colour = kMarkGreen;
    else
        m.colour = kMarkDark;
    blink_mark(d);  // seg13:0x2679: seg11:0x3ed8(d)
}

void Info::blink_mark(int d) {
    Mark& m = marks_[d];
    m.lit = m.lit == 0 ? 1 : 0;  // seg11:0x3ee5-0x3ef2: cmp 1; sbb; neg
    if (d == 2) {
        h_.draw_mark2();  // GetDC(DGROUP:0x95fc) [OPEN: the window of mark 2]
        return;
    }
    // GetDC(DGROUP:0xa7c) and seg11:0x2ce6(hdc, d) [BYTES seg11:0x3f01-0x3f15]: on the left
    // panel, without a repaint.
    if (!exists_) return;
    const shell::Box& b = rect_[0];
    c_.set_view(b.x, b.y, b.cx, b.cy);
    draw_mark(d);
    c_.clear_view();
}

// seg11:0x2ce6(hdc, d) and seg11:0x2dac(hdc, dir, pen, d) on the current view.
void Info::draw_mark(int d) {
    Mark& m = marks_[d];
    if (d == 0) {
        m.ox = 2;  // seg11:0x2d04: dword 0x00110002
        m.oy = 0x11;
    } else if (d == 1) {
        m.ox = w16(values_.target_w - 0x38);  // seg11:0x2d46-0x2d54
        m.oy = 2;
    } else if (d == 2) {
        m.ox = 2;
        m.oy = 2;
    }
    const auto segment = [&](int dir, bool pen) {
        Point p[3];
        const int n = mark_segment(dir, p);
        if (n == 0) return;
        for (int i = 0; i < n; ++i) {
            p[i].x += m.ox;
            p[i].y += m.oy;
        }
        c_.lines(Img::kScreen, pen ? Pen::solid(m.colour) : Pen::stock_object(kBlackPen), p, n);
    };
    if (m.dir != m.shown) {  // seg11:0x2d18-0x2d3b, seg11:0x2d65-0x2d88: erase the old one
        segment(m.shown, false);
        m.shown = m.dir;
    }
    segment(m.dir, m.lit != 0);  // seg11:0x2d3f-0x2d9d
}

// seg13:0x58dc(hdc, flag) [BYTES seg13:0x58e9-0x59af].
void Info::target_picture(int flag) {
    if (!exists_) return;  // seg13:0x58e9: DGROUP:0xa7c = 0
    const int m = mode();
    if (m != kModeTarget && m != kModeMessage) return;    // seg13:0x58f2-0x58fe
    if (h_.game_mode() == 6 && m == kModeTarget) return;  // seg13:0x5902-0x590e
    Mark& k = marks_[m - 2];                              // seg13:0x5912-0x5928
    if (k.pool != 7 && k.pool != 0xc && !h_.target_ok(k.pool, k.rec)) {  // seg13:0x592b-0x5941
        if (m == kModeMessage) {  // seg13:0x5943-0x595b: back to the target display
            set_mode(kModeTarget);
            invalidate(kInfoTarget);
        }
        return;
    }
    CellInputs in;
    if (!h_.cell(k.pool, k.rec, in)) return;  // no such record in the port's pools [DESIGN]
    // seg13:0x5996-0x59af: seg13:0x5a6a(hdc, origin +0x16/+0x18, pool, rec, mark, flag, 0).
    in.x = k.ox;
    in.y = k.oy;
    in.pool = k.pool;
    in.rec = k.rec;
    in.flag = flag;
    in.scale = 0;
    const CellPlan p = plan_cell(in, k.glint);
    if (p.clear) c_.pat_blt(Img::kScreen, p.clear_x, p.clear_y, p.clear_size, p.clear_size, kBlackness,
                            Brush::none());
    // The glint (seg13:0x5460, seg13:0x54da) and the pictures other than the icon strip's are
    // not drawn [OPEN]; with flag 1 the glint never runs (seg13:0x5af3).
    if (p.picture && p.slot == 0xd)
        c_.stretch_blt(Img::kScreen, p.dx, p.dy, p.dw, p.dh, Img::kIconStrip, p.sx, p.sy, p.sw, p.sh,
                       kSrcCopy);
}

void Info::target_picture_now() {
    // GetDC(DGROUP:0xa7c) when hdc = 0 [BYTES seg13:0x5967-0x597b].
    if (!exists_) return;
    const shell::Box& b = rect_[0];
    c_.set_view(b.x, b.y, b.cx, b.cy);
    target_picture(1);
    c_.clear_view();
}

void Info::attitude_text(int r) {
    // seg11:0x3f3b-0x3f86: strings 580-584 by attitude [BYTES seg11:0x3f3b seg11:0x3f4b
    // seg11:0x3f5a seg11:0x3f69].
    int id;
    if (r < -0xfa)
        id = 0x244;
    else if (r < -0x50)
        id = 0x245;
    else if (r < 0x50)
        id = 0x246;
    else if (r < 0xfa)
        id = 0x247;
    else
        id = 0x248;
    text_93e0_ = h_.lang(id);
    text_93e0_ += r > 0 ? " +" : " ";             // DGROUP:0xad3, DGROUP:0xad6
    text_93e0_ += std::to_string(w16(r) / 10);  // seg11:0x3fad-0x3fb7: idiv 10, seg6:0xf1a
}

// ---- the target codes

void Info::target_code(bool ctrl, int code) {
    manual_ = true;  // seg11:0x7738
    if (mode() != kModeTarget) {
        set_mode(kModeTarget);      // seg11:0x7745
        invalidate(kInfoTarget);    // seg11:0x7757
        if (code_accepts(marks_[0].pool, marks_[0].rec, code)) return;  // seg11:0x7767
    } else if (ctrl) {
        marks_[0].pool = 3;  // seg11:0x777b: DGROUP:0x6e54 = 3
        marks_[0].rec = 0;   // DGROUP:0x6e52 = 0
    }
    select_target(code);  // seg11:0x778a
}

bool Info::code_accepts(int pool, int rec, int code) const {
    if (!h_.target_ok(pool, rec)) return false;              // seg3:0x66d
    if (pool == 0xe && rec == h_.player()) return false;     // seg3:0x67b-0x687
    switch (static_cast<unsigned>(code)) {                    // seg3:0x6cb: ja -> 1
    case 0: return true;
    case 1: return pool == 0xe;
    case 2: return pool == 0 || pool == 0xb;
    case 3: return pool == 8 && h_.gate_post_live(rec);
    case 4: return pool == 9;
    case 5: return pool == 2;
    default: return true;
    }
}

void Info::set_target_of(bool mark2, int pool, int rec) {
    if (mark2) {  // seg3:0x63e-0x647
        marks_[2].pool = pool;
        marks_[2].rec = rec;
    } else {
        h_.set_target(h_.player(), pool, rec);  // seg3:0x654: seg3:0xf4(player, pool, rec)
    }
}

void Info::select_target(int code) {
    bool mark2 = false;
    int pool, rec;
    if (code == 6) {  // seg3:0x46d-0x495
        rec = marks_[2].rec;
        pool = marks_[2].pool;
        code = pool == 0xe ? 1 : 5;
        mark2 = true;
    } else {
        pool = marks_[0].pool;
        rec = marks_[0].rec;
    }
    const auto in_use = [&](int p) { return p >= 0 ? h_.in_use(p) : 0; };  // [DESIGN] see below
    if (!code_accepts(pool, rec, code)) {
        // seg3:0x4b9-0x550: the nearest object that suits the code, within 20000.
        int best = 0x4e20, bp = 0, br = 0;
        for (int p = 0; p <= 0xe; ++p)
            for (int r = 0; r < in_use(p); ++r) {
                if (p == pool && rec >= r) continue;  // seg3:0x4d7-0x4df
                if (!code_accepts(p, r, code)) continue;
                const int d = w16(h_.distance(p, r, 0xe, h_.player()));  // seg3:0x51c
                if (d < best) {
                    best = d;
                    bp = p;
                    br = r;
                }
            }
        if (best < 0x4e20) {
            set_target_of(mark2, bp, br);
            return;
        }
    }
    // seg3:0x55b-0x5c2: the next one after the current target.
    if (pool <= 0xe || rec < h_.record_bound_9422()) {
        // A pool of -1 (no target) makes the original read the word before DGROUP:0x9406 as a
        // count; seg3:0x0 refuses every record of a negative pool, so the port counts 0
        // [DESIGN].
        for (int p = code == 1 ? 0xe : pool; p <= 0xe; ++p)
            for (int r = 0; r < in_use(p); ++r) {
                if (p == pool && rec >= r) continue;
                if (code_accepts(p, r, code)) {
                    set_target_of(mark2, p, r);
                    return;
                }
            }
    }
    // seg3:0x5c4-0x610: from the start.
    for (int p = code == 1 ? 0xe : 0; p <= 0xe; ++p)
        for (int r = 0; r < in_use(p); ++r)
            if (code_accepts(p, r, code)) {
                set_target_of(mark2, p, r);
                return;
            }
    set_target_of(mark2, 0xc, 1);  // seg3:0x612-0x614: pool 12, record 1
}

void Info::toggle_manual() {
    manual_ = !manual_;  // seg11:0x7138-0x7141: cmp 1; sbb; neg
    if (manual_) {       // seg11:0x7144-0x7163
        const int t = h_.next_target();
        h_.set_target(h_.player(), (t >> 8) & 0xff, t & 0xff);
    }
    invalidate(kInfoTarget);  // seg11:0x71b6-0x71cc
}

void Info::show_frame_rate() {
    set_mode(kModeFrameRate);  // seg11:0x71d4-0x71db
    invalidate(kInfoTarget);  // seg11:0x71b6-0x71cc
}

void Info::show_messages() {
    if (mode() == kModeMessage) return;  // seg11:0x71a9-0x71ae
    set_mode(kModeMessage);
    invalidate(kInfoTarget);
}

void Info::frame_rate_measured() {
    if (mode() == kModeFrameRate) invalidate(kInfoTarget);  // seg7:0xadf-0xaf2
}

}  // namespace is::hud
