// owner: M4 stage 3, track VISIT.
// The visit seg16:0xcfe and its parts (visit.h). INSPACE.EXE addresses; the verdicts are
// work/analysis/m4/s3-visit-verify.md's, the Ghidra C and listings those of
// work/analysis/m4/s3-visit-packs/seg16_0xcfe.md and its siblings.
#include "world/visit.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "briefing/briefing.h"
#include "briefing/lang_format.h"
#include "world/ambulance.h"
#include "world/world.h"

namespace is::world {

using briefing::FArg;

VisitState& visit_state() {
    static VisitState v;
    return v;
}

VisitLinks& visit_links() {
    static VisitLinks l;
    return l;
}

VisitLayout visit_layout(int W, int H) {
    VisitLayout l;
    l.W = W;
    l.H = H;
    // seg15:0x482e's pad: k = 4·H/3 - 1100, width k/36 + 164, the column k/30 + 80 (the desk's
    // values; src/desk/desk.cpp step 2) [BYTES seg15:0x483d seg15:0x48c2].
    const int k = idiv16(lo16(4 * H), 3) - 1100;
    l.pad_w = idiv16(lo16(k), 36) + 164;
    l.pad_c = idiv16(lo16(k), 30) + 80;
    // seg16:0x1f78 (verified claim 47) [BYTES seg16:0x1f78-0x2142].
    l.panel_w = std::min<int>(260, idiv16(lo16(W - l.pad_w), 2) - 20);
    l.panel_h = 200;
    l.ry = std::min<int>(idiv16(lo16(2 * H), 5), H - 370) - (W > 700 ? 10 : 0);
    l.panel_bg_h = std::max<int>(idiv16(lo16(H), 2), 370);
    l.rx = std::min<int>(idiv16(lo16(W), 13) + idiv16(lo16(W), 2) + 90, W - l.panel_w);
    l.lx = std::max<int>(0, idiv16(lo16(W), 2) - idiv16(lo16(W), 13) - l.panel_w - 90);
    // seg16:0x27ea (verified claim 48): one dword write 0x000a000c [BYTES seg16:0x27f8-0x2815].
    l.mon_dx = 12;
    l.mon_dy = 10;
    l.mon_w = l.panel_w - 30;
    l.mon_h = l.panel_h - 36;
    return l;
}

bool visit_pace_gate(const VisitState& v) { return v.left_on != 0 && v.left_mode == 1; }
bool visit_pace_slow(const VisitState& v) { return v.phase == 1 && v.descent == 0; }

namespace {

ShipRecord& served(VisitCtx& c) { return c.w.ship(c.w.w952e); }
ShipRecord& player(VisitCtx& c) { return c.w.ship(c.w.player); }
std::int16_t dword(VisitCtx& c, int design, int off) { return c.ph.design_word(design, off); }
std::int16_t tank_of(VisitCtx& c, const ShipRecord& s) { return dword(c, s.design_index, 0x1e); }
std::int16_t row_of(VisitCtx& c, const ShipRecord& s, int k) { return dword(c, s.design_index, 0x22 + 2 * k); }

// seg10:0xa332(DGROUP:0x6db8, id, 1, args...).
void fmt(VisitCtx& c, int id, const std::vector<FArg>& args) {
    c.v.text.clear();
    briefing::lang_format_into(c.v.text, id, 1, args, [&c](int i) { return c.h.lang(i); });
}
// The %D arguments (pool 14, the served ship, type 1) with the name seg3:0x1542 gives.
std::vector<FArg> served_d(VisitCtx& c) {
    return {FArg(c.h.ship_name(c.w.w952e)), FArg(static_cast<int>(c.w.w952e)), FArg(1)};
}
// seg9:0x95f4(id): LoadString(DGROUP:0x9212, id, DGROUP:0x6db8, 0x5a) [BYTES seg9:0x95f4].
void load(VisitCtx& c, int id) {
    c.v.text = c.h.lang(id);
    if (c.v.text.size() > 0x59) c.v.text.resize(0x59);
}
// seg11:0x3fd4(text, 1): a ... z to upper case [BYTES seg11:0x3fd4].
void upper(std::string& t) {
    for (char& ch : t)
        if (ch > '`' && ch < '{') ch = static_cast<char>(ch - 0x20);
}
void bar_k(VisitCtx& c, int k, int v) {
    const VisitLayout& l = c.v.lay;
    c.scr.bar(l.bar_x(), l.bar_y(k), l.bar_w(), 10, v);
}
// A deadline test on the dword `now` (DGROUP:0x1e50): passed once now > d, unsigned (`ja`
// waits) [BYTES seg16:0x11c6 seg16:0x1460].
bool passed(std::uint32_t now, std::uint32_t d) { return d < now; }

// seg15:0x3590, the descent of phase 1 [BYTES seg15:0x3590-0x374a] [OPEN: seg15:0x3caa,
// seg15:0x1eac and the palette ramps are summarised].
void descent(VisitCtx& c) {
    VisitState& v = c.v;
    switch (v.descent) {
    case 0:
        if (c.scr.incoming(0) == 0) return;
        // seg15:0x1328(1), seg15:0x1eac, the black screen, the fades and ramps, seg15:0x482e
        // (the pad) and seg15:0x374c(1).
        c.scr.descent_start(v.lay);
        {
            // seg15:0x374c(1): the end y = pad top - s4 + pad_c - 10 and the start 0x136 - 10
            // above it; the speed starts at 150 [BYTES seg15:0x374c] (s3-visit.md 2.1).
            const int pad_top = v.lay.H - v.lay.pad_c - 0x34;
            v.end_y = pad_top - v.s4 + v.lay.pad_c - 10;
            v.start_y = v.end_y - 0x136 + 10;
            v.pos10 = v.start_y * 10;
            v.speed = 150;
        }
        v.descent = 1;
        return;
    case 1: {
        // seg15:0x374c(0) each tick until it returns 1: the speed falls by 3 (Calibrate < 1300)
        // or 5 a call, kept within 50 ... 120; the position (x 10) grows by 2·speed/5 [OPEN:
        // summarised; the braking near the ends is not ported]; with DGROUP:0x928 the ship is
        // set down at once [OPEN].
        bool done = false;
        if (v.fast != 0) {
            v.pos10 = v.end_y * 10;
            done = true;
        } else {
            v.speed -= c.h.calibrate() < 1300 ? 3 : 5;
            v.speed = std::clamp(v.speed, 50, 120);
            v.pos10 += 2 * v.speed / 5;
            if (v.pos10 >= v.end_y * 10) {
                v.pos10 = v.end_y * 10;
                done = true;
            }
        }
        c.scr.descent_frame(v.lay, v.pos10 / 10, v.s4, v.speed <= 60);
        if (done) {
            c.h.sound(9);
            v.descent_due = c.h.now() + 1000;
            v.descent = 2;
        }
        return;
    }
    default:
        if (v.descent_due != 0 && !passed(c.h.now(), v.descent_due)) return;
        v.descent_due = 0;
        ++v.phase;  // sub-step 2 in state 6 (verified claim 6) [BYTES seg15:0x3717]
        return;
    }
}

// Phase 0's seg15:0x302e [BYTES seg15:0x302e-0x358d] (s3-visit.md 2.1).
void landing(VisitCtx& c) {
    VisitState& v = c.v;
    c.h.sound(0x6e);
    std::string slow;
    const int cal = c.h.calibrate();
    if (v.demon == 0 && (cal < 850 || (cal < 1600 && c.h.depth() < 8)))
        slow = (v.lay.W > 700 ? c.h.lang(2137) : std::string()) + c.h.lang(2138);
    c.scr.landing_scene(v.lay, slow);
    v.descent = 0;
    c.scr.incoming(1);
}

// The repair step of phases 24 and 29 for system k: below 100 and with a lamp row, old = sys;
// sys += 1, then += 1 again when old < 101 (else 100), so an odd value ends at 101 (ADR 0014
// decision 6) [BYTES seg16:0x14f0-0x1543 seg16:0x1630-0x1672].
bool repair_step(VisitCtx& c, int k) {
    ShipRecord& S = served(c);
    std::int16_t& sys = system_word(S, k);
    if (!(sys < 100 && row_of(c, S, k) != -1)) return false;
    const std::int16_t old = sys;
    sys = add16(sys, 1);
    if (old < 0x65)
        sys = add16(sys, 1);
    else
        sys = 100;
    bar_k(c, k, sys);
    return true;
}

}  // namespace

// ---- seg16:0x2e3c(init, blank) ---------------------------------------------------------------------
void visit_type(VisitCtx& c, int init, int blank) {
    VisitState& v = c.v;
    const VisitLayout& l = v.lay;
    const int L = l.L(), T = l.T();
    const int h = l.W < 700 ? 12 : l.W < 860 ? 13 : 14;  // [BYTES seg16:0x2e4b-0x2e69] (verified claim 11)
    if (init != 0) {
        // [BYTES seg16:0x2e79-0x2ecb] (verified claim 9)
        c.scr.flash(idiv16(lo16(l.mon_w), 2) + L + 3, idiv16(lo16(l.mon_h), 2) + T + 3);
        v.index = 0;
        v.lines = 0;
        v.pen_x = L + 8;
        v.pen_y = T + 7;
        v.type_due = c.h.now() + 500;
        return;
    }
    if (v.type_due > c.h.now()) return;  // `ja` exits while the deadline is later (verified claim 10)
    v.type_due = c.h.now() + 40;
    c.scr.monitor_begin(h);
    const int right = L + l.mon_w + 3;  // DGROUP:0x6e18 + 0x6e14 + 0x3d48 + 3
    const int bottom = T + l.mon_h;     // DGROUP:0x6e1a + 0x6e16 + 0x3d4a
    const auto scroll = [&] {
        c.scr.scroll_up(L + 6, T + 7, L + l.mon_w + 3, T + l.mon_h + 3, 12);  // 12 x 1 px
        v.cur_y -= 12;
    };
    for (int n = 4; n > 0; --n) {  // [bp-0x16] = 4 [BYTES seg16:0x31a5]
        const std::size_t ix = static_cast<std::size_t>(v.index);
        const char ch = ix < v.text.size() ? v.text[ix] : '\0';
        bool wrap = false;
        if (ch == ' ') {
            // The word after the space up to its terminator (' ', NUL or CR), measured with the
            // buffer's first byte, which the original never writes; the port takes a space for
            // it [DESIGN: the natural width, "the space before the word"]. The count includes
            // the terminator: margin 22 for a word of at most 4 letters, else 9 (verified claim
            // 16) [BYTES seg16:0x31b5-0x3257 seg16:0x2f82-0x2f93].
            std::string word;
            std::size_t j = ix + 1;
            for (; j < v.text.size(); ++j) {
                const char d = v.text[j];
                if (d == ' ' || d == '\r') break;
                word += d;
            }
            const int count = static_cast<int>(word.size()) + 1;
            const int ext = c.scr.text_extent(" " + word);
            const int margin = count <= 5 ? 22 : 9;
            if (right - margin < v.pen_x + ext) wrap = true;
        }
        if (ch == '\0' || wrap) {
            // A new line ends the call [BYTES seg16:0x2f98-0x304a] (verified claims 13-14).
            v.pen_x = L + 8;
            if (ch == '\0') {
                ++v.phase;  // [BYTES seg16:0x2fbf]
                v.index = 0;
            }
            ++v.lines;
            if (bottom - 13 <= v.pen_y)
                scroll();
            else
                v.pen_y += 12;
            c.scr.monitor_end();
            return;
        }
        if (v.lines != 0 || v.index != 0) c.scr.black(v.cur_x + 2, v.cur_y, 6, 10);  // [BYTES seg16:0x305d]
        if (v.index == 0 && blank != 0) {  // the blank line first (verified claim 15)
            ++v.lines;
            if (v.lines >= 9)
                scroll();
            else
                v.pen_y += 12;
        }
        c.scr.glyph(v.pen_x, v.pen_y, ch);  // TextOut [BYTES seg16:0x310f]
        v.pen_x += c.scr.text_extent(std::string(1, ch));
        c.scr.block(v.pen_x + 2, v.pen_y, 6, 10, 0x0000FF80);  // the cursor [BYTES seg16:0x313d-0x316b]
        v.cur_x = v.pen_x;
        v.cur_y = v.pen_y;
        ++v.index;
        c.h.sound_at(0x2b, 0, 0, 0);  // a click per character (verified claim 12) [BYTES seg16:0x318f]
    }
    c.scr.monitor_end();
}

// ---- seg15:0x4c76(first, down) in state 6 ----------------------------------------------------------
void visit_pad(VisitCtx& c, int first, int down) {
    VisitState& v = c.v;
    if (first != 0) {
        if (down == 0 && c.s.new_body != 0) {
            // DeleteObject of DGROUP:0x92b0 ... 0x92b4, seg15:0x2a08 (the ship of INI MainShip,
            // the new body) and a 2000 ms busy-wait (verified claim 33) [BYTES seg15:0x4c8b-0x4ce7].
            c.scr.rebuild_ship();
            c.h.busy_wait(2000);
        }
        c.h.sound(5);  // [BYTES seg15:0x4dbd]
        v.pad_max = v.s4 + 0x3e;
        if (down != 0) v.pad_off = 0;  // [BYTES seg15:0x4dd1]
        c.scr.pad_strip(v.lay, v.pad_off, v.s4);
        return;
    }
    // (verified claim 32) [BYTES seg15:0x5238-0x52d8]
    if (down != 0) {
        if (v.pad_off >= v.pad_max)
            ++v.phase;
        else
            v.pad_off = std::min(v.pad_max, v.pad_off + 2);
    } else {
        if (v.pad_off > 5)
            v.pad_off = std::max(5, v.pad_off - 2);
        else
            ++v.phase;  // in state 6 (state 0 is the desk's)
    }
    c.scr.pad_strip(v.lay, v.pad_off, v.s4);
}

// ---- seg16:0x638a ------------------------------------------------------------------------------------
void visit_click(VisitCtx& c, int x, int y, int esc) {
    VisitState& v = c.v;
    // Nothing unless a player game with the left monitor's record (verified claim 50).
    if (!c.player_game || v.left_on == 0) return;
    v.clicked = 1;  // DGROUP:0x958a on every call past the guard [BYTES seg16:0x638a-0x64c6]
    const VisitLayout& l = v.lay;
    // DGROUP:0x6e08, the left monitor's rectangle (lx + 15, T + 3, + mon_w, + mon_h).
    const int ml = l.lx + 15, mt = l.ry + l.mon_dy + 3;
    bool hit;
    if (esc != 0)
        hit = v.left_mode != 1;  // the hit flag without a PtInRect test [BYTES seg16:0x63c1]
    else
        hit = x >= ml && x < ml + l.mon_w && y >= mt && y < mt + l.mon_h;
    if (!hit) {
        if (v.left_mode == 1) {  // outside the full-screen subgame: back to the visit
            v.left_mode = 4;
            v.phase = v.saved_phase;
            c.h.todo("the Inner Demon subgame's end (M6)", "seg18:0xc5c");
        }
        return;
    }
    if (c.h.limited()) {
        c.h.box_2322();  // seg16:0x68d6(DGROUP:0x9532, 2322, -1) [BYTES seg16:0x640c-0x6423]
        return;
    }
    if (v.left_mode == 3 || v.left_mode == 4) {
        v.saved_phase = v.phase;
        v.phase = -1;
        v.left_mode = 1;
        c.h.demon_click();  // the subgame full-screen (M6)
    }
}

// ---- seg16:0x872 ------------------------------------------------------------------------------
namespace {
// The visit's routing of ambulance.h ambulance_repairs_done, the one port of seg16:0x872 (M4 s3
// fixer, review H6): seg16:0x3784 through the visit's PurchaseHost, seg3:0x6ec through its
// VisitHost [DESIGN].
class VisitAmbulanceHost final : public AmbulanceHost {
public:
    explicit VisitAmbulanceHost(VisitCtx& c) : c_(c) {}
    void purchases(int ship, int mode) override {
        PurchaseCtx p{c_.w, c_.s, c_.W, c_.ph, c_.player_game, c_.restored, c_.plus};
        apply_purchases(p, ship, mode);
    }
    void message(int recipient, int pool, int rec, int code) override {
        (void)pool, (void)rec;  // (7, 0) [BYTES seg16:0x889-0x893]
        c_.h.message(recipient, code);
    }
    void todo(const std::string& what, const char* address) override { c_.h.todo(what, address); }

private:
    VisitCtx& c_;
};
}  // namespace

void visit_purchases(VisitCtx& c) {
    VisitAmbulanceHost h(c);
    AmbulanceCtx a{c.w, ambulance_state(), c.h.now(), h};
    ambulance_repairs_done(a);  // seg16:0x3784(DGROUP:0x952e, 0), seg3:0x6ec(..., 0x54) [BYTES seg16:0x87e-0x893]
    // seg16:0x3784's end writes the dword DGROUP:0x9e5b = 1 (DGROUP:0x9e5d = 0); the port keeps
    // that word in both Shop and AmbulanceState and writes both [DESIGN] [BYTES seg16:0x3b07].
    ambulance_state().set_served_alive_dword(1);
}

// ---- phase 64's free spot ------------------------------------------------------------------------
void visit_free_spot(VisitCtx& c, std::int16_t& x, std::int16_t& y) {
    World& w = c.w;
    for (;;) {
        // SetRect(x - 800, y - 1200, x + 2240, y + 1200), 16-bit [BYTES seg16:0x1c56-0x1c7c].
        const Rect16 r = rect16(x - 800, y - 0x4b0, x + 0x8c0, y + 0x4b0);
        bool good = true;
        for (int p = 0; p <= w.roles.ships && good; ++p) {
            if (p == 7) continue;
            for (int k = 0; k < w.pools.in_use(p) && k < w.pools.records(p); ++k) {
                if (p == w.roles.ships && k == w.w952e) continue;
                const ObjectRecord& o = w.pools.rec(p, k);
                // The bounds tests sit inside the record loop (s3-visit.md 1.8) [BYTES
                // seg16:0x1c84-0x1cf7].
                if (pt_in_rect(r, o.x, o.y) || x < 0x410 || y < 0x2d0 ||
                    x > (kWorldCells - 0x82) * 8 || y > (kWorldCells - 0x5a) * 8) {
                    good = false;
                    break;
                }
            }
        }
        if (good) return;
        x = add16(x, lo16((c.h.rand() % 0x96 + 100) * 8));  // [BYTES seg16:0x1d0a]
        y = add16(y, lo16((c.h.rand() % 0x96 + 100) * 8));  // [BYTES seg16:0x1d1e]
        if (x > kWorldCells * 8) x = 0;  // DGROUP:0x5a0 · 8 [BYTES seg16:0x1d2a-0x1d44]
        if (y > kWorldCells * 8) y = 0;
    }
}

// ---- the entry and seg16:0xcfe -------------------------------------------------------------------
void visit_begin(VisitCtx& c) {
    c.v.phase = 0;  // DGROUP:0x914a = 0 [BYTES seg7:0x600]
    visit(c, 1);    // seg16:0xcfe(1) [BYTES seg7:0x608]
}

void visit(VisitCtx& c, int repaint) {
    VisitState& v = c.v;
    Shop& s = c.s;
    // [BYTES seg16:0xd0d-0xd37] (verified claim 2)
    if (repaint != 0) {
        if ((v.phase > 0 || v.phase == -1) && (v.phase >= 2 || v.descent >= 1))
            c.scr.redraw(v.lay, v.phase, v.saved_phase, v.left_on != 0);  // seg16:0x2144
        return;
    }
    if (s.dlg != 0) return;  // DGROUP:0x9532 (verified claim 1) [BYTES seg16:0xd3b-0xd40]
    // [BYTES seg16:0xd44-0xd6b] (verified claim 3)
    if (c.h.ctrl_held() && v.left_on == 0 && v.descent != 1) v.fast = 1;
    if (c.h.w8f70() > 1) v.fast = 1;
    const std::uint32_t now = c.h.now();
    ShipRecord& S = served(c);
    bool advance = true;  // the cases that `break` reach seg16:0x17f5: phase++
    switch (v.phase) {
    case 0:  // [BYTES seg16:0xd77-0xdb6] (verified claims 4-5)
        c.h.enter_calls();  // seg10:0xa856, seg11:0x2f0 (and the scene's own calls)
        if (v.demon != 0)
            v.w959a = 1;
        else
            v.w9662 = 0;
        v.left_on = v.demon != 0 ? 1 : 0;
        {
            // s4 of the player's roster record (bytes +0x7a, +0x7b) [BYTES seg16:0xdc1-0xdea].
            const ShipRecord& P = player(c);
            const int a = static_cast<std::int8_t>(c.ph.roster_byte(P.roster_index, 0x7a));
            const int b = static_cast<std::int8_t>(c.ph.roster_byte(P.roster_index, 0x7b));
            v.s4 = mul16(add16(lo16(a), idiv16(lo16(b - 6), 2)), 4);
        }
        v.lay = visit_layout(c.w.W, c.w.H);
        landing(c);  // seg15:0x302e
        if (c.w.w952e == c.w.player) {
            c.w.w1e84 = 0;  // [BYTES seg16:0xdae]
            c.w.w1e80 = 0;  // [BYTES seg16:0xdb2]
        }
        // seg16:0x27ea's DGROUP:0x952c = seg16:0x619e() when DGROUP:0x93a6 = 0: the left
        // monitor's record, from the panel layout of the descent [BYTES seg16:0x27f8-0x2815].
        if (v.demon == 0) v.left_on = 1;
        v.left_mode = 3;  // seg16:0x64c8 [BYTES seg16:0x64c8]
        break;
    case 1:  // seg15:0x3590 [BYTES seg16:0xdb9]
        descent(c);
        advance = false;
        break;
    case 2: {  // [BYTES seg16:0xdc1-0xf55] (verified claims 7-8)
        const int x = idiv16(lo16(v.lay.W), 2) - idiv16(lo16(v.s4), 2);
        const ObjectRecord& o = c.w.ship_object(c.w.player);
        const int sh = static_cast<std::int16_t>(shl16(o.size, 3)) >> (c.w.size & 0x1f);  // DGROUP:0x959e [OPEN: Size at the pickup]
        c.scr.ship_on_pad(v.lay, x, v.end_y + 2, v.s4, sh);
        v.strip_saved = true;
        fmt(c, 1184, served_d(c));  // "%D HAS LANDED.."
        visit_type(c, 1, 0);
        break;
    }
    case 3: case 7: case 12: case 14: case 32: case 37:  // [BYTES seg16:0x170e]
        visit_type(c, 0, 0);
        advance = false;
        break;
    case 4: {  // the shape line [BYTES seg16:0xf5b-0x104e] (verified claims 17-18)
        int mean = 0;  // [bp-0xa]: never set on the DGROUP:0x93a6 path [OPEN: uninitialised]
        if (v.demon == 0) {
            const int n = row_of(c, S, 3) == -1 ? 3 : 4;
            std::int16_t sum = 0;
            for (int k = 0; k < n; ++k) sum = add16(sum, system_word(S, k));
            mean = idiv16(sum, static_cast<std::int16_t>(n));
            if (s.alive == 0) {
                fmt(c, 2021, {FArg(c.h.pilot())});  // "YOU'RE DEAD, %s"
            } else {
                const int id = mean >= 0x47 ? 311 : mean >= 0x2e ? 312 : mean >= 0x1c ? 313 : 314;
                fmt(c, 1183, {FArg(id), FArg(c.h.pilot())});  // "YOU'RE IN %S SHAPE, %s"
            }
        } else {
            fmt(c, 2020, {FArg(c.h.pilot())});  // "WELL DONE, %s"
        }
        upper(v.text);
        v.text += (mean > 0x1b && s.alive != 0) ? ". " : "!";  // DGROUP:0x9ee, DGROUP:0x1124
        break;
    }
    case 5: case 9: case 16: case 23: case 34: case 39: case 42: case 55: case 57: case 62:
        visit_type(c, 0, 1);  // a blank line first [BYTES seg16:0x1430]
        advance = false;
        break;
    case 6: {  // "%S REQUIRED" [BYTES seg16:0x1051-0x1112] (verified claim 19)
        if (v.demon != 0) {
            v.d1c48 = 0;
            v.phase = 60;  // [BYTES seg16:0x1ac7] (verified claim 20)
            advance = false;
            break;
        }
        std::int16_t sum = 0;
        for (int k = 0; k < 3; ++k) sum = add16(sum, system_word(S, k));
        const std::int16_t m3 = idiv16(sum, 3);
        const std::int16_t tank = tank_of(c, S);
        int id;
        if (S.fuel < idiv16(tank, 5)) {
            id = 1177;  // REFUELING
        } else if (m3 < 50) {
            id = 2516;  // BASIC REPAIRS
        } else if (row_of(c, S, 3) == -1 || system_word(S, 3) > 0x31) {
            if (idiv16(tank, 2) <= S.fuel) {
                v.phase = 8;
                advance = false;
                break;
            }
            id = 1177;
        } else {
            id = 2517;  // WING REPAIR
        }
        fmt(c, 1157, {FArg(id)});
        v.text += ". ";  // DGROUP:0x9ee
        break;
    }
    case 8:  // [BYTES seg16:0x1115-0x116d] (verified claim 21)
        ++v.phase;
        if (S.resources() < 1)
            fmt(c, 2527, served_d(c));  // "%D HAS NO RESOURCES..."
        else
            fmt(c, 1173, {FArg(briefing::hundredths_text(S.resources(), 2))});  // seg6:0x1010(res, 100, 2)
        advance = false;
        break;
    case 10:  // [BYTES seg16:0x1170-0x11bb] (verified claims 22-23)
        c.h.sound(0x6e);
        {  // the lift words and DGROUP:0x111e (ambulance.h AmbulanceState) [BYTES seg16:0x1178] (M4 s3 fixer)
            AmbulanceState& a = ambulance_state();
            a.lift_step = -1;  // DGROUP:0x9e41
            a.spin = 50;       // DGROUP:0x9e43
            a.frame = 5;       // DGROUP:0x9e45
            a.lift = 15;       // DGROUP:0x9e47
            a.busy = 0;        // DGROUP:0x111e
        }
        v.d1c44 = now + 0x9c4;  // 2500 ms
        s.weapons = static_cast<std::int8_t>(S.slots_cb());  // DGROUP:0x958e
        if (S.resources() < 1) {
            v.phase = 13;
            advance = false;
        }
        break;
    case 11:  // [BYTES seg16:0x11be-0x11e9] (verified claim 24)
        if (passed(now, v.d1c44)) {
            ++v.phase;
            v.d1c44 = now + 1000;
            load(c, 1174);  // "TRANSFER COMPLETE. "
        }
        advance = false;
        break;
    case 13:  // credit += resources [BYTES seg16:0x11ec-0x122f]
        set_credit(S, credit_of(S) + S.resources());
        S.set_resources(0);
        fmt(c, 1158, {FArg(briefing::hundredths_text(credit_of(S), 2))});  // seg6:0xf54(credit, 100, 2)
        break;
    case 15: {  // the dialog or the body chooser [BYTES seg16:0x1232-0x142d] (verified claims 25-30)
        if (s.dlg == 0) {
            c.h.busy_wait(1300);  // only when DGROUP:0x9532 = 0 (verified claim 25, CORRECTED)
            const VisitLayout& l = v.lay;
            c.scr.save_monitor(l.L() + 7, l.T() + 7, l.mon_w - 4, l.mon_h - 4);
            v.monitor_saved = true;
            if (c.player_game) c.h.sound(0x13);
            const ShipRecord& P = player(c);
            v.wings_refit = (P.be5 == 0 || (sbyte_at(P, 0xac) != 0 && sbyte_at(P, 0xad) != 0)) ? 0 : 1;
            if (v.left_mode == 1) v.left_mode = 4;
            if (s.alive == 0) {
                s.dlg = 1;  // blocks re-entry
                c.h.body_chooser();  // seg10:0x82a2(6); its result is never read (verified claim 28)
                s.unify = s.double_guns = s.wing = s.refuel = s.basic = 0;
                s.bought.fill(0);
                s.sold.fill(0);
                s.new_body = 1;
                s.cost = 600;
                for (int k = 0; k < 4; ++k) {
                    if (row_of(c, S, k) != -1) {
                        system_word(S, k) = 100;  // [BYTES seg16:0x13d0]
                        bar_k(c, k, 100);
                    }
                }
                s.dlg = 0;
            } else {
                c.h.options_dialog();  // DialogBox(0xc2) [BYTES seg16:0x1339-0x1344]
                if (!c.player_game) {
                    PurchaseCtx p{c.w, c.s, c.W, c.ph, c.player_game, c.restored, c.plus};
                    computer_shop(p, c.w.player, 0);  // seg16:0x3b18(player, 0)
                }
            }
            c.scr.redraw(v.lay, v.phase, v.saved_phase, v.left_on != 0);  // seg16:0x2144
        }
        if (s.new_body == 0) {
            v.phase = 22;
            advance = false;
        } else {
            load(c, 866);  // "LOADING NEW BODY..."
        }
        break;
    }
    case 17: visit_pad(c, 1, 1); break;                      // [BYTES seg16:0x15f7]
    case 18: case 28: visit_pad(c, 0, 1); advance = false; break;  // [BYTES seg16:0x160e]
    case 19: break;                                          // [BYTES seg16:0x17f5]
    case 20: visit_pad(c, 1, 0); break;                      // [BYTES seg16:0x16e0]
    case 21: case 30: visit_pad(c, 0, 0); advance = false; break;  // [BYTES seg16:0x16e5]
    case 22:  // [BYTES seg16:0x1435-0x1459]
        if (s.basic == 0) {
            v.phase = 25;
            advance = false;
            break;
        }
        fmt(c, 1175, {FArg(2516)});  // "BASIC REPAIRS IN PROGRESS.."
        break;
    case 24: {  // basic repairs every 50 ms [BYTES seg16:0x145c-0x1547] (verified claim 34)
        if (!passed(now, v.d1c48)) {
            advance = false;
            break;
        }
        for (int k = 0; k < 3; ++k) c.ph.set_roster_byte(c.w.player_roster, 0x38 + k, 100);  // DGROUP:0x931c's record
        v.d1c48 = now + 0x32;
        bool done = true;
        for (int k = 0; k < 3; ++k)
            if (repair_step(c, k)) done = false;
        advance = done;
        break;
    }
    case 25: {  // the wing set-up [BYTES seg16:0x154a-0x15e0] (verified claim 35)
        if (s.wing == 0) {
            v.phase = 31;
            advance = false;
            break;
        }
        fmt(c, 1175, {FArg(2517)});  // "WING REPAIR IN PROGRESS.."
        ShipRecord& P = player(c);
        set_byte_at(P, 0xad, 1);
        set_byte_at(P, 0xac, 1);
        const int r = c.w.player_roster;
        c.ph.set_roster_byte(r, 0x73, 1);
        c.ph.set_roster_byte(r, 0x72, c.ph.roster_byte(r, 0x73));
        P.w24 = P.w0a;
        c.ph.set_roster_byte(r, 0x74, static_cast<std::uint8_t>(P.w0a));
        if (P.w24 == 2) {
            set_byte_at(P, 0xe1, 1);
            set_byte_at(P, 0xe0, 1);
        }
        c.ph.set_roster_byte(r, 0x3b, 100);
        break;
    }
    case 26:  // [BYTES seg16:0x15e3] (verified claim 36)
        visit_type(c, 0, s.basic == 0 ? 1 : 0);
        advance = false;
        break;
    case 27:  // [BYTES seg16:0x15f0] (verified claim 31)
        if (v.wings_refit == 0) {
            v.phase += 2;
            advance = false;
            break;
        }
        visit_pad(c, 1, 1);
        break;
    case 29: {  // the wing repair every 50 ms [BYTES seg16:0x161a-0x16e2]
        if (!passed(now, v.d1c48)) {
            advance = false;
            break;
        }
        c.ph.set_roster_byte(c.w.player_roster, 0x3b, 100);
        v.d1c48 = now + 0x32;
        if (repair_step(c, 3)) {
            advance = false;
            break;
        }
        if (v.wings_refit == 0) {
            v.phase += 2;
            advance = false;
            break;
        }
        visit_pad(c, 1, 0);
        break;
    }
    case 31:  // [BYTES seg16:0x16ea-0x170b]
        if (!s.any_repair()) {
            v.phase = 33;
            advance = false;
            break;
        }
        load(c, 1154);  // "REPAIRS COMPLETE."
        break;
    case 33:  // [BYTES seg16:0x171c-0x1745]
        if (s.refuel == 0) {
            v.phase = 38;
            advance = false;
            break;
        }
        fmt(c, 2540, served_d(c));  // "REFUELING %D..."
        break;
    case 35: {  // the refuel every 50 ms, to the PLAYER's tank [BYTES seg16:0x1748-0x17f1]
        if (!passed(now, v.d1c48)) {
            advance = false;
            break;
        }
        v.d1c48 = now + 0x32;
        const std::int16_t tank = tank_of(c, player(c));
        bool done = true;
        if (S.fuel < tank) {
            const std::int16_t f = add16(S.fuel, 0x10);
            S.fuel = f < tank ? f : tank;
            const std::int16_t ptank = tank_of(c, player(c));
            const int g = ptank != 0 ? div32(static_cast<std::int32_t>(player(c).fuel) * 100, ptank) : 0;
            bar_k(c, 4, g);  // the bar at y offset 0x7a [BYTES seg16:0x17ad]
            done = false;
        }
        advance = done;
        break;
    }
    case 36:  // "REFUEL COMPLETE." (verified claim 38) [BYTES seg16:0x17fc]
        fmt(c, 1176, {FArg(1)});
        break;
    case 38:  // (verified claim 39, CORRECTED) [BYTES seg16:0x1806-0x1875]
        v.weapon = 1;
        v.blank_w = 1;
        if (s.double_guns == 0) {
            v.phase = 41;
            advance = false;
            break;
        }
        fmt(c, 1185, {FArg(1)});  // "INSTALLING DOUBLE GUNS"
        v.text += "...";          // DGROUP:0x111a
        v.blank_w = 0;
        break;
    case 40:  // busy 800 ms, no pump [BYTES seg16:0x1831-0x1850]
        c.h.busy_wait(800);
        break;
    case 41:  // [BYTES seg16:0x1852-0x187e]
        if (s.unify == 0) {
            v.phase = 44;
            advance = false;
            break;
        }
        load(c, 1971);  // "UNIFYING FORCES"
        v.text += "...";
        v.blank_w = 0;
        break;
    case 43:  // sound 0x22 and the pump for 800 ms (verified claim 40) [BYTES seg16:0x1881-0x18af]
        c.h.sound(0x22);
        c.h.pump_wait(800);
        break;
    case 44:  // [BYTES seg16:0x18b2]
        v.blank_p = 1;
        v.power = 0;
        break;
    case 45:  // (verified claim 41) [BYTES seg16:0x18c1-0x18eb]
        if (s.powerup[static_cast<std::size_t>(v.power)] == 0) {
            if (v.power != 5)
                ++v.power;
            else
                v.phase = 48;
            advance = false;
            break;
        }
        fmt(c, 1972, {FArg(1973 + v.power)});  // "POWERUP %S..."
        break;
    case 46:
        visit_type(c, 0, v.blank_p);
        advance = false;
        break;
    case 47:  // [BYTES seg16:0x18f5-0x1932]
        v.blank_p = 0;
        c.h.pump_wait(800);
        if (v.power < 5) {
            ++v.power;
            v.phase = 45;
            advance = false;
        }
        break;
    case 48:  // [BYTES seg16:0x1935-0x1953]
        if (s.sold[static_cast<std::size_t>(v.weapon)] == 0) {
            v.phase = 50;
            advance = false;
            break;
        }
        fmt(c, 1199, {FArg(186 + v.weapon)});  // "UNLOADING %S..."
        break;
    case 49: case 52:  // [BYTES seg16:0x19ff-0x1a12]
        visit_type(c, 0, v.blank_w);
        v.blank_w = 0;
        advance = false;
        break;
    case 50:  // [BYTES seg16:0x1956-0x1998]
        if (v.weapon > 25) {
            ++v.phase;
            v.weapon = 1;
        } else {
            if (s.sold[static_cast<std::size_t>(v.weapon)] != 0) c.h.busy_wait(800);
            ++v.weapon;
            v.phase = 48;
        }
        advance = false;
        break;
    case 51: {  // (verified claim 42) [BYTES seg16:0x199b-0x19fc]
        if (s.bought[static_cast<std::size_t>(v.weapon)] == 0) {
            v.phase = 53;
            advance = false;
            break;
        }
        bool owned = false;
        for (int j = 0; j < 10; ++j)
            if (static_cast<std::int8_t>(player(c).weapon[static_cast<std::size_t>(j)]) == v.weapon) owned = true;
        fmt(c, owned ? 1182 : 1181, {FArg(186 + v.weapon)});  // "RELOADING / LOADING %S..."
        break;
    }
    case 53:  // [BYTES seg16:0x1a15-0x1a52]
        if (v.weapon < 26) {
            if (s.bought[static_cast<std::size_t>(v.weapon)] != 0) c.h.busy_wait(800);
            ++v.weapon;
            v.phase = 51;
            advance = false;
        }
        break;
    case 54:  // [BYTES seg16:0x1a55-0x1a84]
        if (s.sold_total == 0) {
            v.phase = 56;
            advance = false;
            break;
        }
        fmt(c, 1159, {FArg(briefing::hundredths_text(s.sold_total, 2))});
        break;
    case 56:  // [BYTES seg16:0x1a87-0x1ab6]
        if (s.cost == 0) {
            v.phase = 58;
            advance = false;
            break;
        }
        fmt(c, 1179, {FArg(briefing::hundredths_text(s.cost, 2))});
        break;
    case 58: {  // the charge, no floor [BYTES seg16:0x1ab9-0x1afd]
        if (s.cost == 0 && s.sold_total == 0) {
            v.d1c48 = 0;
            v.phase = 60;
            advance = false;
            break;
        }
        // credit -= (int32)cost - (int32)sold_total, 32-bit [BYTES seg16:0x1a87 seg16:0x1aa0]
        const std::int32_t d = static_cast<std::int32_t>(s.cost) - static_cast<std::int32_t>(s.sold_total);
        set_credit(S, static_cast<std::int32_t>(static_cast<std::uint32_t>(credit_of(S)) - static_cast<std::uint32_t>(d)));
        s.cost = 0;
        fmt(c, 1158, {FArg(briefing::hundredths_text(credit_of(S), 2))});
        break;
    }
    case 59:  // [BYTES seg16:0x1b00-0x1b14]
        visit_type(c, 0, 0);
        v.d1c48 = 0;
        advance = false;
        break;
    case 60:  // two clocks: armed from GetCurrentTime, tested on DGROUP:0x1e50 (verified claim 43)
        if (v.d1c48 == 0) {
            v.d1c48 = c.h.current_time() + (v.demon != 0 ? 6000 : 1000);
            advance = false;
        } else if (!passed(now, v.d1c48)) {
            advance = false;
        }
        break;
    case 61:  // [BYTES seg16:0x1b55-0x1b7c]
        load(c, v.demon != 0 ? 2022 : 1180);  // "PREPARE FOR LAUNCH"
        v.text += "...";
        c.h.launch_enter();  // seg15:0xcb0
        break;
    case 63:  // seg15:0x10be; it advances the phase at its end [BYTES seg16:0x1b7f]
        if (c.h.launch_frame()) ++v.phase;
        advance = false;
        break;
    case 64: {  // the hand-back [BYTES seg16:0x1b87-0x1e5c] (verified claims 44-45)
        c.scr.black_screen();
        c.scr.free_scene();  // seg15:0x1328(0), seg16:0x1f24
        c.h.palette_back();  // seg10:0x6dd4(0), seg10:0x6e76(8, 0)
        c.h.set_state(v.demon != 0 ? 9 : 1);
        v.left_on = 0;  // seg16:0x631c: the left monitor's record freed, DGROUP:0x9530 = 2
        v.left_mode = 2;
        v.fast = 0;
        v.monitor_saved = v.strip_saved = false;
        advance = false;
        if (v.demon == 1) {
            c.h.recreate_dcs();
            c.h.demon_end();  // DGROUP:0x92be = 0, seg16:0x7210(1)
            break;
        }
        ambulance_state().dist = 1200;  // DGROUP:0x9e4b [BYTES seg16:0x1bf3] (M4 s3 fixer)
        ObjectRecord& so = c.w.ship_object(c.w.w952e);
        ObjectRecord& amb = c.w.pools.rec(kPool7, 0);
        so.state = 1;   // [BYTES seg16:0x1bfb]
        amb.state = 1;  // [BYTES seg16:0x1c05]
        ambulance_state().reentry_due = now + 3000u;  // dword DGROUP:0x6da8, the re-entry deadline [BYTES seg16:0x1c0f] (M4 s3 fixer)
        c.h.recreate_dcs();  // seg10:0xa90c
        if (v.w9662 != 0) c.h.seg18_7b2();  // with DGROUP:0x92e4 = 1 around it
        std::int16_t x = amb.x, y = amb.y;
        visit_free_spot(c, x, y);
        so.x = amb.x = x;
        so.y = amb.y = y;
        std::int16_t sx = 0, sy = 0;
        c.h.to_screen(x, y, sx, sy);  // seg8:0x29ea
        so.scr_x = amb.scr_x = sx;
        so.scr_y = amb.scr_y = sy;
        so.x = add16(so.x, 8);          // [BYTES seg16:0x1dd6]
        so.scr_x = add16(so.scr_x, 1);  // [BYTES seg16:0x1de4]
        c.h.watch_windows(2);  // seg13:0x65f2(2)
        if (c.player_game) c.h.cursor_home();
        c.h.music_back();      // DGROUP:0x73e = 0; seg10:0x7480(1 or 2, 1)
        visit_purchases(c);    // seg16:0x872
        c.h.repaint_wave();    // seg11:0xafe
        break;
    }
    default:
        advance = false;
        break;
    }
    if (advance) ++v.phase;  // seg16:0x17f5
    // The tail [BYTES seg16:0x1ef0]: the left monitor while clicked, paused or in the Inner Demon
    // visit; dwords DGROUP:0x5a8 and DGROUP:0x1e68 += 1 (the frame counters; the port's frame
    // count is the World's) [OPEN: DGROUP:0x5a8].
    if (v.clicked != 0 || v.phase == -1 || (v.demon != 0 && v.phase >= 2)) {
        c.w.frames += 1;
        c.scr.left_monitor(v.lay, c.h.limited());
    }
}

}  // namespace is::world
