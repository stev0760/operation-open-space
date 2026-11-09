#ifdef _WIN32
#include "desk/gdi_desk.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gfx/launch_ship.h"
#include "gfx/pad.h"
#include "gfx/palette.h"
#include "desk/scene_gdi.h"

namespace is::desk {

namespace {
constexpr COLORREF palettergb(int r, int g, int b) { return 0x02000000u | RGB(r, g, b); }
std::wstring wide(const std::string& cp1252) {
    if (cp1252.empty()) return {};
    const int n = MultiByteToWideChar(1252, 0, cp1252.data(), static_cast<int>(cp1252.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(1252, 0, cp1252.data(), static_cast<int>(cp1252.size()), w.data(), n);
    return w;
}
}  // namespace

ShipSource& program_ships() {
    static ShipSource s;
    return s;
}

GdiDesk::~GdiDesk() {
    if (font_) DeleteObject(font_);
}

HDC GdiDesk::screen() const {
    gfx::Gfx* x = g();
    return x && !x->surface().empty() ? x->dc() : nullptr;
}

// ---- palette

void GdiDesk::palette_save(int set, int blank) { if (g()) g()->palette_save(set, blank); }
void GdiDesk::palette_scheme(int c, int mode) { if (g()) g()->palette_scheme(c, mode); }
void GdiDesk::palette_restore(int set) { if (g()) g()->palette_restore(set); }
void GdiDesk::palette_fade(int mode, int percent) { if (g()) g()->palette_fade(mode, percent); }

// A scene colour: ordinal 32 at 8 bpp takes PALETTEINDEX entries of the command-6 ramps; the
// oracle (24 bpp) shows the PALETTERGB colours of the other depths. The port allocates each
// colour it draws through command 6 and draws it with PALETTERGB, so the 8-bit surface shows
// the oracle's colours where the palette has room (ADR 0007) [DESIGN].
COLORREF GdiDesk::colour(const Rgb& c) {
    const auto key = std::make_tuple(int(c.r), int(c.g), int(c.b));
    if (auto it = colours_.find(key); it != colours_.end()) return it->second;
    gfx::Gfx* x = g();
    if (!x) return 0;
    x->palette_colour(c.r, c.g, c.b);
    const COLORREF v = x->dc_colour(palettergb(c.r, c.g, c.b));
    colours_[key] = v;
    return v;
}

// ---- the scene DC

// ordinal 32 through seg10:0x9526: CreateCompatibleBitmap(hdc, W, H) and ordinal 32's GDI calls
// replayed on it (src/desk/scene_gdi.h: the clear PatBlt BLACKNESS, SetStretchBltMode, strips,
// pens, texts, patterns) [BYTES ISPACE.DLL seg8:0x2d0 seg8:0x302 seg8:0x31b seg8:0x15ec].
void GdiDesk::scene(const Render& r) {
    gfx::Gfx* x = g();
    if (!x) return;
    colours_.clear();
    scene_ = x->create_image(r.proj.W, r.proj.H);
    // seg15:0xcb0's DGROUP:0x3d48 / 0x3d4a (slot 0's left and top), slot 0's width and DGROUP:0x92de
    // = max(slot 2 bottom, slot 3 bottom), the column of seg15:0x4c76's composite [BYTES seg15:0xd44
    // seg15:0xe66 seg15:0xe69] (M4 s6 PAD).
    client_h_ = r.proj.H;
    if (r.slots.size() > 3)
        column_ = {r.slots[0].l, r.slots[0].t, r.slots[0].r, std::max(r.slots[2].b, r.slots[3].b)};
    // The monitor's DKGRAY_BRUSH and the tower's LEDs first, so the 8-bit surface holds them
    // exactly; draw_scene then allocates the scene's colours in drawing order and publishes them
    // [DESIGN] (ADR 0007).
    for (const Rgb& c : {Rgb{64, 64, 64}, Rgb{0, 255, 0}, Rgb{127, 255, 0}, Rgb{127, 0, 0}})
        x->palette_colour(c.r, c.g, c.b);
    draw_scene(*x, scene_->hdc(), r, todo_);
    colours_.clear();  // dc_colour matches the published palette: look the colours up again
}

// seg15:0x8604 [BYTES seg15:0x8604 seg15:0x8611 seg15:0x862e seg15:0x8848 seg15:0x8a98
// seg15:0x8abf]. The bevels' "dark grey" is the stock DKGRAY_BRUSH (0x404040 in newgame-0002).
void GdiDesk::monitor(const Rect& s) {
    HDC d = scene_dc();
    gfx::Gfx* x = g();
    if (!d || !x) return;
    const int L = s.l, T = s.t, R = s.r, B = s.b, h = B - T - 15;
    auto fill = [&](int l, int t, int r, int b, COLORREF c) {
        RECT rc{l, t, r, b};
        HBRUSH br = CreateSolidBrush(c);
        FillRect(d, &rc, br);
        DeleteObject(br);
    };
    const COLORREF lt = x->dc_colour(RGB(192, 192, 192)), wh = x->dc_colour(RGB(255, 255, 255)),
                   bk = x->dc_colour(0), dk = colour({64, 64, 64}), gr = x->dc_colour(RGB(128, 128, 128));
    x->palette_animate();
    fill(L, T, R, B, lt);                             // LTGRAY_BRUSH
    fill(L + 25, T + 25, R - 25, T + h - 25, bk);     // the screen, DGROUP:0x6084
    fill(L + 20, T + 20, R - 20, T + 25, dk);         // top bevel
    fill(L + 20, T + 20, L + 25, T + h - 20, dk);     // left bevel
    fill(L + 20, T + h - 25, R - 20, T + h - 20, wh); // bottom bevel
    fill(R - 25, T + 20, R - 20, T + h - 20, wh);     // right bevel
    fill(L, T, R, T + 2, wh);                         // outer edges, 2 px
    fill(L, T, L + 2, B, wh);
    fill(R - 2, T, R, B, dk);
    fill(L, B - 2, R, B, dk);
    auto pen_line = [&](COLORREF c, std::initializer_list<POINT> pts) {
        HPEN p = CreatePen(PS_SOLID, 1, c);
        HGDIOBJ old = SelectObject(d, p);
        bool first = true;
        for (const POINT& pt : pts) {
            if (first) MoveToEx(d, pt.x, pt.y, nullptr);
            else LineTo(d, pt.x, pt.y);
            first = false;
        }
        SelectObject(d, old);
        DeleteObject(p);
    };
    pen_line(wh, {{L, T + h - 5}, {R - 1, T + h - 5}});  // WHITE_PEN
    try {
        if (gfx::ImagePtr logo = x->load_graphic(290, 3, -1))  // IGRAPHIC.DLL #4000 290
            BitBlt(d, L + 20, T + h - 17, 44, 10, logo->hdc(), 0, 0, SRCCOPY);
    } catch (const std::exception& e) {
        todo_(std::string("#4000 290: ") + e.what(), "seg15:0x8848");
    }
    pen_line(x->dc_colour(0x0000FF00), {{R - 19, T + h - 5}, {R - 38, T + h - 5}});
    pen_line(wh, {{R - 20, B - 16}, {R - 20, B - 6}, {R - 35, B - 6}});
    pen_line(gr, {{R - 35, B - 6}, {R - 35, B - 16}, {R - 20, B - 16}});
    try {
        if (gfx::ImagePtr corner = x->load_bitmap("CORNER")) {
            BitBlt(d, L + 20, T + h - 25, 5, 5, corner->hdc(), 0, 0, SRCCOPY);
            BitBlt(d, R - 26, T + 20, 5, 5, corner->hdc(), 0, 0, SRCCOPY);
        }
    } catch (const std::exception& e) {
        todo_(std::string("CORNER: ") + e.what(), "seg15:0x8abf");
    }
}

// Ordinal 34 on the scene DC (ispace.md "Ordinal notes" 34) [BYTES ISPACE.DLL seg8:0x5ac
// seg8:0x602 seg8:0x60b seg8:0x611 seg8:0x633 seg8:0x657 seg8:0x6c2 seg8:0x707 seg8:0x71a].
void GdiDesk::frame34(HDC d, int x, int y, int w, int h, int style) {
    gfx::Gfx* gx = g();
    const COLORREF fillc = gx->dc_colour(style == 0 ? 0 : RGB(192, 192, 192));
    HBRUSH b = CreateSolidBrush(fillc);
    HGDIOBJ ob = SelectObject(d, b);
    PatBlt(d, x, y, w, h, PATCOPY);
    SelectObject(d, ob);
    DeleteObject(b);
    COLORREF p1 = 0, p2 = 0;
    bool has2 = true;
    switch (style) {
    case 0: p1 = RGB(0xc0, 0xc0, 0xc0); has2 = false; break;
    case 1:
    case 2: p1 = 0; p2 = RGB(255, 255, 255); break;
    default: p1 = RGB(255, 255, 255); p2 = RGB(0x80, 0x80, 0x80); break;
    }
    auto poly = [&](COLORREF c, POINT a, POINT m, POINT z) {
        HPEN p = CreatePen(PS_SOLID, 1, gx->dc_colour(c));
        HGDIOBJ op = SelectObject(d, p);
        MoveToEx(d, a.x, a.y, nullptr);
        LineTo(d, m.x, m.y);
        LineTo(d, z.x, z.y);
        SelectObject(d, op);
        DeleteObject(p);
    };
    poly(p1, {x + w, y}, {x + w, y + h}, {x, y + h});
    if (has2) poly(p2, {x + w, y}, {x, y}, {x, y + h});
    if (style == 2) {
        HPEN p = CreatePen(PS_SOLID, 1, gx->dc_colour(RGB(0x80, 0x80, 0x80)));
        HGDIOBJ op = SelectObject(d, p);
        MoveToEx(d, x, y + h, nullptr);
        LineTo(d, x + w, y + h);
        SelectObject(d, op);
        DeleteObject(p);
    }
}

// seg15:0x8ae6 [BYTES seg15:0x8b17 seg15:0x8b61 seg15:0x8bad seg15:0x8bdb seg15:0x8c25
// seg15:0x8c75 seg15:0x8cac seg15:0x8cd8 seg15:0x8d12 seg15:0x8d6d]. The slot's (L, T, w, h)
// are its left, top, right − left and bottom + 1 − top [OPEN seg15:0x8ae6: which of the slot's
// edges the panel sizes take].
void GdiDesk::tower(const Rect& s) {
    HDC d = scene_dc();
    gfx::Gfx* x = g();
    if (!d || !x) return;
    const int L = s.l, T = s.t, w = s.w(), h = s.h();
    frame34(d, L, T, w, h, 2);
    frame34(d, L + w / 16, T + w / 16, 7 * w / 8, h - w / 8, 3);
    frame34(d, L + w / 8, T + 5 * h / 8 + 1, 3 * w / 4, 3 * h / 8 - w / 8, 1);
    frame34(d, L + w / 8, T + h / 2, 3 * w / 4, h / 8, 1);
    try {
        if (gfx::ImagePtr logo = x->load_graphic(290, 3, -1))
            BitBlt(d, L + w / 8 + 2, T + 5 * h / 8 + 3, std::min(3 * w / 4, 44), 10, logo->hdc(), 0, 0, SRCCOPY);
    } catch (const std::exception&) {
    }
    const int y = T + 17 * h / 32;
    const COLORREF leds[3] = {palettergb(0, 255, 0), palettergb(127, 255, 0), palettergb(127, 0, 0)};
    for (int i = 0; i < 3; ++i) {
        colour({static_cast<std::uint8_t>(GetRValue(leds[i])), static_cast<std::uint8_t>(GetGValue(leds[i])),
                static_cast<std::uint8_t>(GetBValue(leds[i]))});
        HBRUSH b = CreateSolidBrush(x->dc_colour(leds[i]));
        HGDIOBJ ob = SelectObject(d, b);
        PatBlt(d, L + (3 + i) * w / 8 - 2, y, 5, 3, PATCOPY);
        SelectObject(d, ob);
        DeleteObject(b);
    }
    x->palette_animate();
}

// seg15:0xf2e's drawing [BYTES seg15:0xf95 seg15:0x1023 seg15:0x102f seg15:0x1065].
void GdiDesk::meal(int graphic, const Rect& s) {
    HDC d = scene_dc();
    gfx::Gfx* x = g();
    if (!d || !x || graphic == 0) return;
    try {
        gfx::ImagePtr img = x->load_graphic(graphic, 3, -1);  // ordinal 1 (seg10:0x6c02)
        if (!img) return;
        gfx::ImagePtr mask = x->mask(*img);                  // seg10:0x6ec6
        StretchBlt(d, s.l, s.t, s.w(), s.h(), mask->hdc(), 0, 0, img->width(), img->height(), SRCAND);
        SetStretchBltMode(d, COLORONCOLOR);
        StretchBlt(d, s.l, s.t, s.w(), s.h(), img->hdc(), 0, 0, img->width(), img->height(), SRCPAINT);
    } catch (const std::exception& e) {
        todo_(std::string("meal graphic: ") + e.what(), "seg15:0xf95");
    }
}

void GdiDesk::scene_to_screen() {
    if (HDC s = screen(); s && scene_) BitBlt(s, 0, 0, scene_->width(), scene_->height(), scene_->hdc(), 0, 0, SRCCOPY);
}
void GdiDesk::screen_to_scene(int x, int y, int w, int h) {
    if (HDC s = screen(); s && scene_) BitBlt(scene_->hdc(), x, y, w, h, s, x, y, SRCCOPY);
}
void GdiDesk::scene_to_screen(int x, int y, int w, int h) {
    if (HDC s = screen(); s && scene_) BitBlt(s, x, y, w, h, scene_->hdc(), x, y, SRCCOPY);
}

// ---- the screen

void GdiDesk::black(int x, int y, int w, int h) {
    if (HDC s = screen()) PatBlt(s, x, y, w, h, BLACKNESS);
}

void GdiDesk::block(int x, int y, int w, int h, Colour brush) {
    HDC s = screen();
    if (!s) return;
    HBRUSH b = CreateSolidBrush(g()->dc_colour(brush));
    HGDIOBJ ob = SelectObject(s, b);
    PatBlt(s, x, y, w, h, PATCOPY);
    SelectObject(s, ob);
    DeleteObject(b);
}

void GdiDesk::text_font(int h, int w) {
    if (font_) DeleteObject(font_);
    font_ = CreateFontW(h, w, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, 0, 0, PROOF_QUALITY,
                        VARIABLE_PITCH | FF_SWISS, L"Arial");
}

// TextOut with background 0, OPAQUE, TA_LEFT and colour 0x0000FF00 [BYTES seg15:0x15ec
// seg15:0x1605 seg15:0x17a4]. The port sets them on every call: its DC is the surface's, which
// other drawers share [DESIGN].
void GdiDesk::text_char(int x, int y, char c) {
    HDC s = screen();
    if (!s) return;
    HGDIOBJ of = SelectObject(s, font_);
    SetBkColor(s, g()->dc_colour(0));
    SetBkMode(s, OPAQUE);
    SetTextAlign(s, TA_LEFT | TA_TOP);
    SetTextColor(s, g()->dc_colour(0x0000FF00));
    const std::wstring w = wide(std::string(1, c));
    TextOutW(s, x, y, w.c_str(), static_cast<int>(w.size()));
    SelectObject(s, of);
}

int GdiDesk::text_extent(const std::string& cp1252) {
    HDC s = screen();
    if (!s || cp1252.empty()) return 0;
    HGDIOBJ of = SelectObject(s, font_);
    const std::wstring w = wide(cp1252);
    SIZE sz{};
    GetTextExtentPoint32W(s, w.c_str(), static_cast<int>(w.size()), &sz);
    SelectObject(s, of);
    return sz.cx;
}

void GdiDesk::scroll_up(const Rect& r, int pixels) {
    HDC s = screen();
    if (!s) return;
    RECT rc{r.l, r.t, r.r, r.b};
    for (int i = 0; i < pixels; ++i) ScrollDC(s, 0, -1, &rc, &rc, nullptr, nullptr);
}

// seg15:0x84b0's drawing [BYTES seg15:0x84cd seg15:0x84d5 seg15:0x856b seg15:0x85f7].
void GdiDesk::flash(int x, int y) {
    HDC s = screen();
    if (!s) return;
    // The stock WHITE_PEN in the original; the port's pen carries the same white through
    // dc_colour, the colour form of the emulated palette (ADR 0007) [DESIGN].
    HGDIOBJ op = SelectObject(s, GetStockObject(WHITE_PEN));
    HPEN wp = CreatePen(PS_SOLID, 1, g()->dc_colour(RGB(255, 255, 255)));
    SelectObject(s, wp);
    // The vertical arm first, then the horizontal; LineTo leaves out its end point, so each arm
    // spans -k .. k-1 [BYTES seg15:0x84f6-0x854b], as briefing.cpp crt_flash draws it.
    for (int i = 2; i < 200; ++i) {
        const int k = i / 10;
        MoveToEx(s, x, y - k, nullptr);
        LineTo(s, x, y + k);
        MoveToEx(s, x - k, y, nullptr);
        LineTo(s, x + k, y);
    }
    const COLORREF bk = g()->dc_colour(0);
    for (int i = 200; i >= 1; --i) {
        const int k = i / 10;
        SetPixel(s, x - k, y, bk);
        SetPixel(s, x + k, y, bk);
        SetPixel(s, x, y - k, bk);
        SetPixel(s, x, y + k, bk);
        PatBlt(s, x - 1, y - 1, 2, 2, BLACKNESS);
    }
    SelectObject(s, op);
    DeleteObject(wp);
}

// ---- launch (M4 s6 PAD: work/analysis/m4/s6-pad.md sections 1, 2, 4 and 5 with s6-pad-verify.md)

namespace {
constexpr std::int16_t lo16(std::int32_t v) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(static_cast<std::uint32_t>(v) & 0xffffu));
}
constexpr std::int16_t idiv16(std::int32_t n, std::int32_t d) {
    return lo16(d) == 0 ? std::int16_t{0} : lo16(static_cast<std::int32_t>(lo16(n)) / lo16(d));
}
constexpr std::int16_t half16(std::int32_t v) {
    const std::int16_t w = lo16(v);
    return static_cast<std::int16_t>((w - (w < 0 ? -1 : 0)) >> 1);
}
}  // namespace

gfx::ImagePtr GdiDesk::pad_now() const {
    if (pad_) return pad_;
    return visit_pictures_ ? visit_pictures_().pad : nullptr;
}

void GdiDesk::make_pad(int w, int h) {
    // The Canvas's sized form, for the fakes; this canvas overrides make_pad_for, which the desk's
    // step 2 calls with the play height (Desk::frame), so nothing reaches it (review L3) [DESIGN].
    if (todo_) todo_("the pad of " + std::to_string(w) + " x " + std::to_string(h) + " without a play height", "seg15:0x482e");
}

// seg15:0x482e's build and seg15:0x4a2e's drawing (gfx/pad.h) [BYTES seg15:0x4978-0x4a22
// seg15:0x4a3b-0x4c6c] (verified claims 7-14), 0 px against newgame-0002's intro-04 (tests/
// test_gfx_pad.cpp) [ORACLE newgame-0002].
void GdiDesk::make_pad_for(int play_h, int client_h) {
    gfx::Gfx* x = g();
    if (!x) return;
    if (client_h > 0) client_h_ = client_h;
    pad_play_h_ = play_h;
    const gfx::PadGeometry geo = gfx::pad_geometry(play_h, client_h_);
    const gfx::PadPicture pic = gfx::pad_picture(geo);
    if (!pic.ok()) return;
    // PALETTERGB(v, v, v) of each brush, allocated as the scene's colours are (ADR 0007) [DESIGN].
    for (const int v : gfx::pad_greys(geo)) colour(Rgb{static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v)});
    x->palette_animate();
    colours_.clear();
    pad_ = gfx::pad_image(*x, pic, [x](int v) { return x->colour_index(palettergb(v, v, v)); });
}

// seg15:0x9e6 [BYTES seg15:0xa54-0xca3] (verified claims 41-42): the 16 frames stacked at x 0 in
// DGROUP:0x3ccc (4s x the heights' sum), each seg10:0x9962 from the square DGROUP:0x92b2's (0, 0,
// k s, k s) with COLORONCOLOR, then the mask DGROUP:0x3cce.
int GdiDesk::build_frames(int s) {
    gfx::Gfx* x = g();
    if (!x || !square_ || s <= 0) return 0;
    frames_ = ship_frames(s);
    const int total = frames_[kShipFrames - 1].y + frames_[kShipFrames - 1].h;
    ship_ = x->create_image(4 * s, total);
    HDC fd = ship_->hdc();
    PatBlt(fd, 0, 0, 4 * s, total, BLACKNESS);
    SetStretchBltMode(fd, COLORONCOLOR);
    const int ks = lo16(gfx::kLaunchK * s);
    for (const ShipFrame& f : frames_) StretchBlt(fd, 0, f.y, f.w, f.h, square_->hdc(), 0, 0, ks, ks, SRCCOPY);
    ship_mask_ = x->mask(*ship_);  // seg10:0x6ec6(DGROUP:0x3ccc)
    x->palette_animate();
    return s;
}

// seg15:0x9e6 with seg15:0x2a08's branch A in state 0 (gfx/launch_ship.h ship_2a08) [BYTES
// seg15:0x2a2f-0x302c seg15:0xa07-0xa32]. INI MainShip comes in the 20-byte buffer
// [BYTES seg15:0x2a39-0x2a50] (the host's).
int GdiDesk::make_ship(const std::string& main_ship) {
    gfx::Gfx* x = g();
    if (!x) return 0;
    std::optional<ShipArt> art;
    if (ships_) art = ships_(main_ship);
    if (!art) {
        if (todo_) todo_("launch ship: MainShip " + main_ship + " has no readable body", "seg15:0x2a08");
        return 0;
    }
    const gfx::LaunchShip sh = gfx::ship_2a08(*x, *art, todo_);
    if (!sh.ok()) return 0;
    body_ = sh.body;
    square_ = sh.square;
    // Ordinal 12's ramps write working entries 185-229 after the pad's greys were allocated
    // (desk step 2 builds the pad first [BYTES seg15:0x1143]), so the port looks the pad's greys up
    // again: those it allocated there are taken anew, as the scene's are (ADR 0007) [DESIGN].
    if (pad_) make_pad_for(pad_play_h_, client_h_);
    return build_frames(sh.s);
}

// seg15:0x9e6 in state 6 [BYTES seg15:0x11fe seg15:0xa07-0xa32]: seg15:0x2a08 takes branch B
// (DGROUP:0x92b0 exists) and returns at its early exit unless a wing repair was bought (DGROUP:0x953f
// != 0) or a new body (DGROUP:0x9563 != 0), and seg15:0x9e6 skips it after a new body; s =
// trunc(bmHeight(DGROUP:0x92b0) / k) and the frames come from the visit's DGROUP:0x92b2 [BYTES
// seg15:0x2bff-0x2c48 seg15:0x2ca3-0x2cbb] (verified claims 23, 25, 40). With the wing repair the
// common part runs on the visit's pictures (gfx::ship_2a08_b): the uncut body over the landing's cut
// square and the flame (review H2) [BYTES seg15:0x2cbe-0x2eda]. Without the visit's pictures,
// branch A from MainShip.
int GdiDesk::visit_ship(const std::string& main_ship) {
    const LaunchPictures p = visit_pictures_ ? visit_pictures_() : LaunchPictures{};
    if (!p.body || !p.square) return make_ship(main_ship);
    body_ = p.body;
    square_ = p.square;
    gfx::Gfx* x = g();
    if (p.wing_repair && p.look && x) return build_frames(gfx::ship_2a08_b(*x, *body_, *square_, *p.look, todo_));
    return build_frames(idiv16(body_->height(), gfx::kLaunchK));
}

// seg15:0x4c76(first, 0) in state 0 [BYTES seg15:0x4cec-0x52d8] (verified claims 53-61, the
// composite of Note 58).
void GdiDesk::rise(int pad_w, int pad_h, int s4, int x, int y) {
    gfx::Gfx* gx = g();
    HDC s = screen();
    if (!gx || !s) return;
    const int S = s4 + 0x3e;
    if (!rise_started_) {  // the first call, down = 0
        rise_started_ = true;
        // DGROUP:0x953c = CreateCompatibleBitmap(padW, s4 + 0x43) when absent [BYTES seg15:0x4cec-0x4db1].
        if (!strip_) strip_ = gx->create_image(pad_w, s4 + 0x43);
        gfx::strip_rebuild(*gx, *strip_, pad_w, pad_h - 0x34, s4, pad_now().get(), body_.get());
        // Desk phase 6 (DGROUP:0x95b8 = 6 throughout the desk scene): when DGROUP:0x10 - s4 - 0x3e
        // < DGROUP:0x92de and DGROUP:0x92ac = 0, seg15:0x3bfa(padW, s4 + 0x3e) and DGROUP:0x181e =
        // seg10:0x6ec6(DGROUP:0x953c) [BYTES seg15:0x4fb5-0x4ffe].
        if (client_h_ - S < column_.b && !rise_work_) {
            rise_work_ = gx->create_image(pad_w, S);
            strip_mask_ = gx->mask(*strip_);
        }
    }
    if (!strip_) return;
    // C = IntersectRect(A, B), A the monitor column down to the desk, B the strip at (x, y)
    // [BYTES seg15:0x5003-0x5064].
    const int cl = std::max(column_.l, x), ct = std::max(column_.t, y);
    const int cr = std::min(column_.r, x + pad_w), cb = std::min(column_.b, y + S);
    if (rise_work_ && strip_mask_ && scene_ && cl < cr && ct < cb) {
        // The five blits into the work DC and its copy to the screen (s6-pad-verify.md Note 58)
        // [BYTES seg15:0x5098-0x51bf]: the mask and the strip are read from (0, 0) whatever the
        // offset o, which is (0, 0) on the desk.
        HDC w = rise_work_->hdc(), st = strip_->hdc();
        const int ox = cl - x, oy = ct - y, rw = cr - cl, rh = cb - ct;
        BitBlt(w, ox, cb - y, pad_w, S - rh, st, 0, rh, SRCCOPY);                       // [BYTES seg15:0x50c8]
        BitBlt(w, ox, oy, pad_w, S, st, 0, 0, SRCCOPY);                                 // [BYTES seg15:0x50f1]
        // The scene at (C.left - A.left + DGROUP:0x3ca4, C.top - A.top + DGROUP:0x3ca6), both pairs
        // slot 0's left and top, so at C itself [BYTES seg15:0x5135].
        BitBlt(w, ox, oy, rw, rh, scene_->hdc(), cl, ct, SRCCOPY);
        BitBlt(w, ox, oy, rw, rh, strip_mask_->hdc(), 0, 0, SRCAND);                     // [BYTES seg15:0x5167]
        BitBlt(w, ox, oy, rw, rh, st, 0, 0, SRCPAINT);                                  // [BYTES seg15:0x5199]
        BitBlt(s, x, y, pad_w, S, w, 0, 0, SRCCOPY);                                    // [BYTES seg15:0x51bf]
        return;
    }
    BitBlt(s, x, y, pad_w, S, strip_->hdc(), 0, 0, SRCCOPY);  // [BYTES seg15:0x51ed-0x520d] (verified claim 59)
}

// The rise's end in state 0: the work DC and DGROUP:0x181e freed, the strip DGROUP:0x953c kept until
// seg15:0x1328(1) [BYTES seg15:0x5278-0x52d6] (verified claim 61).
void GdiDesk::free_rise() {
    rise_work_.reset();
    strip_mask_.reset();
    rise_started_ = false;
}

void GdiDesk::pad_to_screen(int x, int y) {
    HDC s = screen();
    gfx::ImagePtr pad = pad_now();
    if (!s || !pad) return;
    BitBlt(s, x, y, pad->width(), pad->height(), pad->hdc(), 0, 0, SRCCOPY);
    pad_x_ = x;
    pad_y_ = y;
    pad_shown_ = true;
}

// One seg15:0x0 drawing (shell.md item 9, Drawing) [BYTES seg15:0x163 seg15:0x30b seg15:0x32b
// seg15:0x377 seg15:0x390 seg15:0x3d4 seg15:0x4e1 seg15:0x518 seg15:0x55f seg15:0x58d], with the
// shadow between the pad part and the frame at depth <= 8 (s6-pad.md section 5; ruling R2 of M4
// stage 6, ADR 0007 Consequences) [BYTES seg15:0x3d9-0x4d1] (verified claim 63).
void GdiDesk::launch_frame(const LaunchDraw& d) {
    gfx::Gfx* x = g();
    HDC s = screen();
    if (!x || !s || !ship_) return;
    const int ww = d.ship.w(), wh = d.ship.h();
    if (!work_ || work_->width() != ww || work_->height() != wh) work_ = x->create_image(ww, wh);
    HDC w = work_->hdc();
    PatBlt(w, 0, 0, ww, wh, BLACKNESS);
    const auto part = [&](const Rect& r, HDC src, int sx0, int sy0) {
        const int l = std::max(r.l, d.ship.l), t = std::max(r.t, d.ship.t);
        const int rr = std::min(r.r, d.ship.r), b = std::min(r.b, d.ship.b);
        if (l >= rr || t >= b) return;  // IntersectRect: empty
        BitBlt(w, l - d.ship.l, t - d.ship.t, rr - l, b - t, src, l - sx0, t - sy0, SRCCOPY);
    };
    if (d.scene && scene_) part(d.column, scene_->hdc(), 0, 0);
    if (gfx::ImagePtr pad = pad_now()) part(d.pad, pad->hdc(), d.pad.l, d.pad.t);
    const ShipFrame& f = frames_[static_cast<std::size_t>(std::clamp(d.f, 0, kShipFrames - 1))];
    const int fx = half16(frames_[0].w) - half16(f.w);
    {
        // risen = y0 - trunc(pos10 / 10); outside state 0 none from 80 risen; in state 0 d =
        // trunc(pos10 / 10) - ye replaces it below 40 and it is capped at 40; off = trunc(risen / 3)
        // + 4, hh = min(h0 - (DGROUP:0x17cc - DGROUP:0x17dc) - off + 14, h_f).
        const int top = idiv16(d.pos10, 10);
        int risen = lo16(d.y0 - top);
        if (!(d.state != 0 && risen >= 0x50)) {
            if (d.state == 0) {
                const int dd = lo16(top - d.ye);
                if (dd < 40) risen = dd;
                risen = std::min(risen, 40);
            }
            const int off = idiv16(risen, 3) + 4;
            const int hh = std::min(frames_[0].h - lo16(d.prev_top - top) - off + 14, f.h);
            gfx::shadow_blit(w, fx, off, f.w, hh, ship_mask_->hdc(), 0, f.y);
        }
    }
    BitBlt(w, fx, 0, f.w, f.h, ship_mask_->hdc(), 0, f.y, SRCAND);
    BitBlt(w, fx, 0, f.w, f.h, ship_->hdc(), 0, f.y, SRCPAINT);
    BitBlt(s, d.ship.l, d.ship.t, ww, wh, w, 0, 0, SRCCOPY);
}

void GdiDesk::free_all() {
    strip_.reset();
    rise_work_.reset();
    strip_mask_.reset();
    rise_started_ = false;
    pad_.reset();
    body_.reset();
    square_.reset();
    ship_.reset();
    ship_mask_.reset();
    work_.reset();
    scene_.reset();
    pad_shown_ = false;
}

}  // namespace is::desk
#endif
