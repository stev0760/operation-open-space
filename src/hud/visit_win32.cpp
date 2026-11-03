// owner: M4 stage 3, track VISIT.
// The landing scene's GDI work (visit_win32.h). INSPACE.EXE addresses; the verdicts are
// work/analysis/m4/s3-visit-verify.md's.
#ifdef _WIN32
#include "hud/visit_win32.h"

#include <algorithm>

#include "hud/canvas.h"
#include "hud/gdi_canvas.h"

namespace is::hud {

namespace {
std::wstring wide1252(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
}  // namespace

std::function<int(dialogs::AmbulanceEnv&)>& ambulance_dialog_slot() {
    static std::function<int(dialogs::AmbulanceEnv&)> f;
    return f;
}

VisitGdi::~VisitGdi() {
    if (font_) DeleteObject(font_);
}

HDC VisitGdi::screen() const {
    gfx::Gfx* x = gfx_();
    return x && !x->surface().empty() ? x->dc() : nullptr;
}

// ---- seg15:0x302e / seg15:0x3590 / seg15:0x374c ---------------------------------------------------
void VisitGdi::landing_scene(const world::VisitLayout& l, const std::string& slow_text) {
    // The black screen and, on a slow machine, the "Incoming Jet / Approaching Landing Pad.."
    // line through seg10:0x94c4(0, 6, 0xff00, 0, W/2, H/2, text) [BYTES seg15:0x30e3-0x3105].
    // The #6600 scene 0x69 (105) rendered into DGROUP:0x929a is not drawn: the port shows the
    // panels on black [OPEN: the render; the oracle's scene appears whole in one frame].
    black_screen();
    if (!slow_text.empty())
        if (gfx::Gfx* x = gfx_()) x->font_text(0, 6, 0x0000FF00, l.W / 2, l.H / 2, slow_text);
    incoming_ = 0;
}

// seg15:0x3caa: the port's stand-in ends at once [OPEN: the incoming jet is read in summary
// only; core-0003 saw no frame of it].
int VisitGdi::incoming(int first) {
    if (first != 0) incoming_ = 0;
    return 1;
}

void VisitGdi::descent_start(const world::VisitLayout& l) {
    black_screen();
    redraw(l, 1, 0, true);
}

void VisitGdi::draw_pad(HDC d, int x, int y, int w, int h) {
    // The pad of seg15:0x482e as the desk draws its stand-in: a plate lighter toward its front
    // and a column below [OPEN seg15:0x4a2e] (src/desk/gdi_desk.cpp make_pad).
    gfx::Gfx* g = gfx_();
    if (!g || w <= 0 || h <= 0) return;
    PatBlt(d, x, y, w, h, BLACKNESS);
    const int plate = std::min(45, h / 2);
    for (int r = 0; r < plate; ++r) {
        const int inset = (plate - r) * 10 / std::max(plate, 1) + 8;
        const int v = 120 + 110 * r / std::max(plate, 1);
        HBRUSH b = CreateSolidBrush(g->dc_colour(RGB(v, v, v)));
        RECT rc{x + inset, y + r, x + w - inset, y + r + 1};
        FillRect(d, &rc, b);
        DeleteObject(b);
    }
    HBRUSH b = CreateSolidBrush(g->dc_colour(RGB(150, 150, 150)));
    RECT rc{x + w / 2 - 10, y + plate, x + w / 2 + 10, y + h};
    FillRect(d, &rc, b);
    DeleteObject(b);
}

void VisitGdi::draw_ship(HDC d, int x, int y, int s4) {
    // The ship's DCs DGROUP:0x3cdc / 0x3cde (SRCAND then SRCPAINT) [BYTES seg16:0xe0b
    // seg16:0xe2a] are the desk's frames; the port stretches the wave sheet's first cell into
    // s4 x s4 over black [OPEN: seg15:0x9e6's frames are not reachable from the HUD].
    gfx::ImagePtr img = ship_ ? ship_() : nullptr;
    if (!img || s4 <= 0) return;
    const int side = std::min(img->width(), img->height());
    SetStretchBltMode(d, COLORONCOLOR);
    StretchBlt(d, x, y, s4, s4, img->hdc(), 0, 0, side, side, SRCPAINT);
}

void VisitGdi::descent_frame(const world::VisitLayout& l, int y, int s4, bool) {
    HDC s = screen();
    if (!s) return;
    const int px = l.W / 2 - l.pad_w / 2, py = l.H - l.pad_c - 0x34;
    const int x = l.W / 2 - s4 / 2;
    PatBlt(s, x, 0, s4, py, BLACKNESS);  // the column the ship falls through
    draw_pad(s, px, py, l.pad_w, l.pad_c + 52);
    draw_ship(s, x, y, s4);
}

// ---- seg16:0x2144 ------------------------------------------------------------------------------------
void VisitGdi::panels(HDC s, const world::VisitLayout& l) {
    // PatBlt with the METAL brush DGROUP:0x500a, PATCOPY, and a 0xc0c0c0 line on the top, left and
    // bottom edges (the left panel's on the top, right and bottom) [BYTES seg16:0x2144-0x23e0].
    gfx::Gfx* g = gfx_();
    GdiCanvas* cv = canvas_();
    HBRUSH metal = cv ? cv->metal_brush() : nullptr;
    HPEN pen = CreatePen(PS_SOLID, 1, g->dc_colour(0x00c0c0c0));
    HGDIOBJ op = SelectObject(s, pen);
    const auto panel = [&](int x, bool right_side) {
        HGDIOBJ ob = SelectObject(s, metal ? static_cast<HGDIOBJ>(metal) : GetStockObject(GRAY_BRUSH));
        PatBlt(s, x, l.ry, l.panel_w, l.panel_bg_h, PATCOPY);
        SelectObject(s, ob);
        const int r = x + l.panel_w - 1, b = l.ry + l.panel_bg_h - 1;
        if (right_side) {
            MoveToEx(s, r, l.ry, nullptr);
            LineTo(s, x, l.ry);
            LineTo(s, x, b);
            LineTo(s, r + 1, b);
        } else {
            MoveToEx(s, x, l.ry, nullptr);
            LineTo(s, r, l.ry);
            LineTo(s, r, b);
            LineTo(s, x - 1, b);
        }
    };
    panel(l.rx, true);
    panel(l.lx, false);
    SelectObject(s, op);
    DeleteObject(pen);
}

void VisitGdi::monitor_faces(HDC s, const world::VisitLayout& l) {
    // The monitor face DGROUP:0x95a0 of seg16:0x27ea on both panels, the insides blacked [OPEN:
    // the face is a bevelled frame of ordinal 34 here].
    gfx::Gfx* g = gfx_();
    const int t = l.T() + 3;
    for (const int x : {l.L() + 3, l.lx + 15}) {
        g->frame(x - 4, t - 4, l.mon_w + 8, l.mon_h + 8, 1);
        PatBlt(s, x, t, l.mon_w, l.mon_h, BLACKNESS);
    }
    if (monitor_) BitBlt(s, l.L() + 7, l.T() + 7, monitor_->width(), monitor_->height(), monitor_->hdc(), 0, 0, SRCCOPY);
}

void VisitGdi::status_block(const world::VisitLayout& l) {
    // [BYTES seg16:0x2448-0x25c9] (verified claim 51, CORRECTED)
    gfx::Gfx* g = gfx_();
    GdiCanvas* cv = canvas_();
    HDC s = screen();
    const VisitStatus st = status_ ? status_() : VisitStatus{};
    g->frame(l.rx + 4, l.ry + l.panel_h + 15, l.panel_w - 20, 128, 1);  // seg10:0x949c
    gfx::ImagePtr labels = cv ? cv->image(Img::kSystems) : nullptr;     // DGROUP:0x5030
    for (int k = 0; k < 5; ++k) {
        int src_y = 0;
        if (k < 3) {
            src_y = st.row[k] == -1 ? 0 : 11 * (st.row[k] + 1);
        } else if (k == 3) {
            if (st.row[3] == -1) continue;  // the label and the gauge skipped [BYTES seg16:0x24b6 seg16:0x24f5]
            const int r = st.shield ? 5 : st.cloak ? 4 : st.row[3];
            src_y = 11 * (r + 1);
        }
        if (labels)  // 11 x 32 to (rx + 8, ry + panel_h + 0x17 + 25k) [BYTES seg16:0x2530]
            BitBlt(s, l.rx + 8, l.ry + l.panel_h + 0x17 + 25 * k, 32, 11, labels->hdc(), 0, src_y, SRCCOPY);
        g->frame(l.rx + 0x39, l.ry + l.panel_h + 20 + 25 * k, l.panel_w - 0x4c, 14, 0);
        g->bar(l.bar_x(), l.bar_y(k), l.bar_w(), 10, st.value[k]);
    }
}

void VisitGdi::redraw(const world::VisitLayout& l, int phase, int saved_phase, bool left_on) {
    HDC s = screen();
    if (!s) return;
    panels(s, l);
    monitor_faces(s, l);
    status_block(l);
    // The pad strip DGROUP:0x953c while phase < 63 (and not -1 with DGROUP:0x958c >= 63), else
    // the bare pad [BYTES seg16:0x25d1-0x2650].
    const int px = l.W / 2 - l.pad_w / 2;
    const bool before_launch = phase < 63 && !(phase == -1 && saved_phase >= 63);
    if (strip_ && before_launch)
        BitBlt(s, strip_x_, strip_y_, strip_->width(), strip_->height() - 5, strip_->hdc(), 0, 5, SRCCOPY);
    else
        draw_pad(s, px, l.H - l.pad_c - 0x34, l.pad_w, l.pad_c + 52);
    if (left_on) left_monitor(l, limited ? limited() : true);
}

// ---- phase 2 and seg15:0x4c76 ------------------------------------------------------------------------
void VisitGdi::ship_on_pad(const world::VisitLayout& l, int x, int y, int s4, int sh) {
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g) return;
    draw_pad(s, l.W / 2 - l.pad_w / 2, l.H - l.pad_c - 0x34, l.pad_w, l.pad_c + 52);
    draw_ship(s, x, y, s4);
    // DGROUP:0x953c: padW x (sh + 0x43), black, with the screen's padW x (sh + 0x3e) from
    // (W/2 - padW/2, H - sh - 0x3e) at y 5 [BYTES seg16:0xe54-0xf32].
    if (!strip_) strip_ = g->create_image(l.pad_w, sh + 0x43);
    if (!strip_) return;
    PatBlt(strip_->hdc(), 0, 0, l.pad_w, sh + 0x43, BLACKNESS);
    strip_x_ = l.W / 2 - l.pad_w / 2;
    strip_y_ = l.H - sh - 0x3e;
    BitBlt(strip_->hdc(), 0, 5, l.pad_w, sh + 0x3e, s, strip_x_, strip_y_, SRCCOPY);
}

void VisitGdi::pad_strip(const world::VisitLayout& l, int offset, int) {
    // The strip lowered by `offset` [OPEN: seg15:0x4c76's state-6 drawing, read with the desk's
    // rise (src/desk/desk.cpp Desk::rise)].
    HDC s = screen();
    if (!s || !strip_) return;
    PatBlt(s, strip_x_, strip_y_, strip_->width(), l.H - strip_y_, BLACKNESS);
    BitBlt(s, strip_x_, strip_y_ + offset, strip_->width(), strip_->height() - 5, strip_->hdc(), 0, 5, SRCCOPY);
}

void VisitGdi::bar(int x, int y, int w, int h, int v) {
    if (gfx::Gfx* g = gfx_()) g->bar(x, y, w, h, v);
}

// ---- seg16:0x2e3c's monitor ------------------------------------------------------------------------
void VisitGdi::monitor_begin(int font_h) {
    HDC s = screen();
    if (!s) return;
    if (!font_) {
        // CreateFont(h - 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0x22, "Arial") (verified claim 11)
        // [BYTES seg16:0x2f07-0x2f28].
        font_ = CreateFontW(font_h - 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, PROOF_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Arial");
    }
    saved_dc_ = SaveDC(s);
    old_font_ = SelectObject(s, font_);
    SetBkColor(s, gfx_()->dc_colour(0));
    SetBkMode(s, TRANSPARENT);
    SetTextAlign(s, TA_LEFT | TA_TOP);
    SetTextColor(s, gfx_()->dc_colour(0x0000FF00));
}

void VisitGdi::monitor_end() {
    HDC s = screen();
    if (!s) return;
    if (old_font_) SelectObject(s, old_font_);
    old_font_ = nullptr;
    if (saved_dc_) RestoreDC(s, saved_dc_);
    saved_dc_ = 0;
}

int VisitGdi::text_extent(const std::string& cp1252) {
    HDC s = screen();
    if (!s || cp1252.empty()) return 0;
    HGDIOBJ of = font_ ? SelectObject(s, font_) : nullptr;
    const std::wstring w = wide1252(cp1252);
    SIZE sz{};
    GetTextExtentPoint32W(s, w.c_str(), static_cast<int>(w.size()), &sz);
    if (of) SelectObject(s, of);
    return sz.cx;
}

void VisitGdi::glyph(int x, int y, char c) {
    HDC s = screen();
    if (!s) return;
    const std::wstring w = wide1252(std::string(1, c));
    TextOutW(s, x, y, w.c_str(), static_cast<int>(w.size()));
}

void VisitGdi::black(int x, int y, int w, int h) {
    if (HDC s = screen()) PatBlt(s, x, y, w, h, BLACKNESS);
}

void VisitGdi::block(int x, int y, int w, int h, std::uint32_t colour) {
    HDC s = screen();
    if (!s) return;
    HBRUSH b = CreateSolidBrush(gfx_()->dc_colour(colour));  // CreateSolidBrush(0x0000FF80) [BYTES seg16:0x316b]
    HGDIOBJ ob = SelectObject(s, b);
    PatBlt(s, x, y, w, h, PATCOPY);
    SelectObject(s, ob);
    DeleteObject(b);
}

void VisitGdi::scroll_up(int l, int t, int r, int b, int n) {
    HDC s = screen();
    if (!s) return;
    RECT rc{l, t, r, b};
    for (int i = 0; i < n; ++i) ScrollDC(s, 0, -1, &rc, &rc, nullptr, nullptr);
}

void VisitGdi::flash(int x, int y) {
    // seg15:0x84b0 as src/desk/gdi_desk.cpp draws it: a white cross growing to 20 px, then erased
    // [BYTES seg15:0x84cd seg15:0x84d5 seg15:0x856b seg15:0x85f7].
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g) return;
    HPEN wp = CreatePen(PS_SOLID, 1, g->dc_colour(RGB(255, 255, 255)));
    HGDIOBJ op = SelectObject(s, wp);
    for (int i = 2; i < 200; ++i) {
        const int k = i / 10;
        MoveToEx(s, x, y - k, nullptr);
        LineTo(s, x, y + k);
        MoveToEx(s, x - k, y, nullptr);
        LineTo(s, x + k, y);
    }
    PatBlt(s, x - 21, y - 21, 42, 42, BLACKNESS);
    SelectObject(s, op);
    DeleteObject(wp);
}

void VisitGdi::save_monitor(int x, int y, int w, int h) {
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g || w <= 0 || h <= 0) return;
    if (!monitor_) monitor_ = g->create_image(w, h);  // DGROUP:0x953a [BYTES seg16:0x125a-0x1273]
    if (monitor_) BitBlt(monitor_->hdc(), 0, 0, w, h, s, x, y, SRCCOPY);
}

void VisitGdi::left_monitor(const world::VisitLayout& l, bool) {
    // seg16:0x64fa with DGROUP:0x9530 = 3: the record's w x h blacked at DGROUP:0x6e08, then
    // seg10:0x94c4(2, 6, 0x0000FF00, 0, w/2, y, text): the Limited edition (DGROUP:0x920c = 0)
    // 2330 at h/2 - 18 and 2331 + "..." at h/2 (verified claim 49, CORRECTED) [BYTES
    // seg16:0x6557-0x6600] [ORACLE core-0003: "ORDER FULL GAME TO SEE / WHAT APPEARS HERE...."].
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g || !lang) return;
    const int x = l.lx + 15, y = l.T() + 3;
    PatBlt(s, x, y, l.mon_w, l.mon_h, BLACKNESS);
    const bool lim = limited ? limited() : true;
    const std::string a = lim ? lang(2330) : lang(2027);
    // The full edition's second line is the subgame record's name (DGROUP:0x6da4 + 8) [OPEN M6].
    const std::string b = (lim ? lang(2331) : std::string("INNER DEMON")) + "...";
    g->font_text(2, 6, 0x0000FF00, x + l.mon_w / 2, y + l.mon_h / 2 - 18, a);
    g->font_text(2, 6, 0x0000FF00, x + l.mon_w / 2, y + l.mon_h / 2, b);
}

void VisitGdi::black_screen() {
    if (gfx::Gfx* g = gfx_())
        if (!g->surface().empty()) PatBlt(g->dc(), 0, 0, g->logical_width(), g->logical_height(), BLACKNESS);
}

void VisitGdi::free_scene() {
    strip_.reset();    // DGROUP:0x953c
    monitor_.reset();  // DGROUP:0x953a
}

}  // namespace is::hud
#endif
