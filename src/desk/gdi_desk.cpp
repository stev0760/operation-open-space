#ifdef _WIN32
#include "desk/gdi_desk.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gfx/palette.h"

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

// ordinal 32 through seg10:0x9526: CreateCompatibleBitmap(hdc, W, H), PatBlt BLACKNESS, then the
// draw loop's PatBlts and pens [BYTES ISPACE.DLL seg8:0x2d0 seg8:0x31b seg8:0x1afb seg8:0x3423
// seg8:0x5632].
void GdiDesk::scene(const Render& r) {
    gfx::Gfx* x = g();
    if (!x) return;
    colours_.clear();
    scene_ = x->create_image(r.proj.W, r.proj.H);
    HDC d = scene_->hdc();
    PatBlt(d, 0, 0, r.proj.W, r.proj.H, BLACKNESS);
    // The monitor's DKGRAY_BRUSH and the tower's LEDs first, so the 8-bit surface holds them
    // exactly; then the scene's colours in drawing order [DESIGN] (ADR 0007).
    for (const Rgb& c : {Rgb{64, 64, 64}, Rgb{0, 255, 0}, Rgb{127, 255, 0}, Rgb{127, 0, 0}}) colour(c);
    for (const Prim& p : r.prims) colour(p.c);
    x->palette_animate();
    colours_.clear();  // dc_colour matches the published palette: look the colours up again
    for (const Prim& p : r.prims) {
        const COLORREF c = colour(p.c);
        if (p.kind == Prim::kFill) {
            HBRUSH b = CreateSolidBrush(c);
            HGDIOBJ old = SelectObject(d, b);
            PatBlt(d, p.x, p.y, p.w, p.h, PATCOPY);
            SelectObject(d, old);
            DeleteObject(b);
        } else {
            HPEN pen = CreatePen(PS_SOLID, p.width, c);
            HGDIOBJ old = SelectObject(d, pen);
            MoveToEx(d, p.x, p.y, nullptr);
            LineTo(d, p.x1, p.y1);
            SelectObject(d, old);
            DeleteObject(pen);
        }
    }
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

// ---- launch: the pad drawer seg15:0x4a2e is an approximation drawn to look like newgame-0002's
// intro-03/04 shots [OPEN seg15:0x4a2e]; its size is seg15:0x482e's (src/desk/desk.cpp).

void GdiDesk::make_pad(int w, int h) {
    gfx::Gfx* x = g();
    if (!x || w <= 0 || h <= 0) return;
    pad_ = x->create_image(w, h);
    HDC d = pad_->hdc();
    PatBlt(d, 0, 0, w, h, BLACKNESS);
    const int plate = std::min(45, h / 2);
    for (int r = 0; r < plate; ++r) {  // the plate: a trapezoid, lighter toward its front
        const int inset = (plate - r) * 10 / plate + 8;
        const int v = 120 + 110 * r / std::max(plate, 1);
        HBRUSH b = CreateSolidBrush(colour({static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v)}));
        RECT rc{inset, r, w - inset, r + 1};
        FillRect(d, &rc, b);
        DeleteObject(b);
    }
    const int pole = 20, px = w / 2 - pole / 2;  // the pole below it
    for (int c = 0; c < pole; ++c) {
        const double u = (2.0 * c + 1) / pole - 1.0;
        const int v = static_cast<int>(60 + 150 * std::sqrt(std::max(0.0, 1 - u * u)));
        HBRUSH b = CreateSolidBrush(colour({static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v)}));
        RECT rc{px + c, plate, px + c + 1, h};
        FillRect(d, &rc, b);
        DeleteObject(b);
    }
    x->palette_animate();
}

void GdiDesk::draw_ship(HDC d, int x0, int y0, int s) {
    // A silver arrowhead with three spikes and a green gem.
    auto poly = [&](COLORREF c, std::initializer_list<POINT> pts) {
        std::vector<POINT> v;
        for (POINT p : pts) v.push_back({x0 + p.x * s / 128, y0 + p.y * s / 128});
        HBRUSH b = CreateSolidBrush(c);
        HPEN p = CreatePen(PS_SOLID, 1, c);
        HGDIOBJ ob = SelectObject(d, b), op = SelectObject(d, p);
        Polygon(d, v.data(), static_cast<int>(v.size()));
        SelectObject(d, ob);
        SelectObject(d, op);
        DeleteObject(b);
        DeleteObject(p);
    };
    const COLORREF light = colour({200, 200, 200}), mid = colour({140, 140, 140}),
                   dark = colour({90, 90, 90}), gem = colour({0, 200, 0}), gemd = colour({0, 110, 0});
    poly(mid, {{4, 120}, {20, 40}, {30, 90}, {50, 70}, {64, 10}, {78, 70}, {98, 90}, {108, 40}, {124, 120}, {80, 110}, {64, 126}, {48, 110}});
    poly(light, {{20, 40}, {30, 90}, {14, 112}});
    poly(light, {{108, 40}, {98, 90}, {114, 112}});
    poly(dark, {{48, 110}, {64, 126}, {80, 110}, {64, 100}});
    poly(gem, {{64, 30}, {80, 70}, {64, 96}, {48, 70}});
    poly(gemd, {{64, 30}, {80, 70}, {64, 96}});
    poly(gem, {{56, 108}, {72, 108}, {64, 118}});
}

// seg15:0x2a08 and seg15:0x9e6 (shell.md "Game-start dialogs", item 7, Ship image and Frames)
// [BYTES seg15:0x2a2f seg15:0x2a50 seg15:0x2a7d seg15:0x2b8f seg15:0x2ba7 seg15:0x9f4
// seg15:0xa35 seg15:0xb97 seg15:0xbee seg15:0xc2e seg15:0xc50 seg15:0xc56].
int GdiDesk::make_ship(const std::string& main_ship) {
    gfx::Gfx* x = g();
    if (!x) return 0;
    std::optional<ShipArt> art;
    if (ships_) art = ships_(main_ship);
    int s = 0;
    gfx::ImagePtr square;  // DGROUP:0x92b2, k·s × k·s
    if (art && art->body.height > 0) {
        // seg10:0x73a6(BOD + 0x21, BOD + 0x23, -2, -2, 2, 0, 1, SHP + 0x07, SHP + 0x08): ordinal 12
        // at twice the body height through seg5:0x1b8e(d 0, a, b) with DGROUP:0x188 = 1. The
        // port's device answers 8 bpp (ADR 0007), so k = 2 and s is the body height. Ordinal 12's
        // smoothing, the flame and wing overlays of seg15:0x2a08 and which part of the 2× strip
        // fills the square are [OPEN]: the port doubles the body's first w × w cell, as the
        // dialogs' preview shows it [OPEN seg15:0x2a7d ISPACE.DLL seg7:0x12d8].
        std::vector<std::uint32_t> table = art->body.palette;
        gfx::PaletteManager pm;
        pm.recolour_graphic(0, art->id, table, 0, art->a, art->b, 0, true);
        const int h = art->body.height;
        s = h;
        const int k = 2;
        // The colours through command 6 and PALETTERGB, as the scene's (ADR 0007) [DESIGN].
        for (std::uint32_t c : table)
            colour(Rgb{static_cast<std::uint8_t>(c >> 16), static_cast<std::uint8_t>(c >> 8), static_cast<std::uint8_t>(c)});
        x->palette_animate();
        colours_.clear();
        square = x->create_image(k * s, k * s);
        HDC sq = square->hdc();
        PatBlt(sq, 0, 0, k * s, k * s, BLACKNESS);
        for (int yy = 0; yy < h; ++yy)
            for (int xx = 0; xx < h && xx < art->body.width; ++xx) {
                const std::size_t i = art->body.bpp <= 8 ? art->body.index_at(xx, yy) : 0;
                const std::uint32_t c = art->body.bpp <= 8 ? (i < table.size() ? table[i] : 0) : art->body.rgb_at(xx, yy);
                if ((c & 0xFFFFFF) == 0) continue;
                const COLORREF cr = colour(Rgb{static_cast<std::uint8_t>(c >> 16), static_cast<std::uint8_t>(c >> 8), static_cast<std::uint8_t>(c)});
                HBRUSH br = CreateSolidBrush(cr);
                RECT rc{k * xx, k * yy, k * xx + k, k * yy + k};
                FillRect(sq, &rc, br);
                DeleteObject(br);
            }
    } else {
        // No MainShip record or body: what seg15:0x2a08 does then is [OPEN seg15:0x2a50]; the
        // port draws its drawn stand-in at s = 32 [DESIGN].
        if (todo_) todo_("launch ship: MainShip " + main_ship + " not rendered", "seg15:0x2a08");
        s = 32;
        square = x->create_image(2 * s, 2 * s);
        PatBlt(square->hdc(), 0, 0, 2 * s, 2 * s, BLACKNESS);
        draw_ship(square->hdc(), 0, 0, 2 * s);
    }
    // seg15:0x9e6: the 16 frames stacked at x 0, each a COLORONCOLOR stretch of the whole square.
    frames_ = ship_frames(s);
    const int total = frames_[kShipFrames - 1].y + frames_[kShipFrames - 1].h;
    ship_ = x->create_image(4 * s, total);
    HDC fd = ship_->hdc();
    PatBlt(fd, 0, 0, 4 * s, total, BLACKNESS);
    SetStretchBltMode(fd, COLORONCOLOR);
    for (const ShipFrame& f : frames_)
        StretchBlt(fd, 0, f.y, f.w, f.h, square->hdc(), 0, 0, square->width(), square->height(), SRCCOPY);
    ship_mask_ = x->mask(*ship_);  // seg10:0x6ec6(DGROUP:0x3ccc)
    x->palette_animate();
    return s;
}

// seg15:0x4c76's strip [BYTES seg15:0x4cf2 seg15:0x4d0f seg15:0x4dbf seg15:0x4eb8 seg15:0x4f16
// seg15:0x4f55 seg15:0x51ed]; the shadow at 8 bpp (ROP 0x00A803A9 under DGROUP:0x5eaa) is left
// out [OPEN seg15:0x4f16].
void GdiDesk::rise(int pad_w, int pad_h, int s4, int x, int y) {
    gfx::Gfx* gx = g();
    HDC s = screen();
    if (!gx || !s) return;
    const int sh = s4 + 0x43;
    if (!strip_) strip_ = gx->create_image(pad_w, sh);
    HDC d = strip_->hdc();
    PatBlt(d, 0, 0, pad_w, sh, BLACKNESS);
    if (pad_) BitBlt(d, 0, sh - pad_h, pad_w, pad_h, pad_->hdc(), 0, 0, SRCCOPY);
    if (ship_) {
        BitBlt(d, pad_w / 2 - s4 / 2, 5, s4, s4, ship_mask_->hdc(), 0, 0, SRCAND);
        BitBlt(d, pad_w / 2 - s4 / 2, 5, s4, s4, ship_->hdc(), 0, 0, SRCPAINT);
    }
    BitBlt(s, x, y, pad_w, sh, d, 0, 0, SRCCOPY);
}

void GdiDesk::free_rise() { strip_.reset(); }

void GdiDesk::pad_to_screen(int x, int y) {
    HDC s = screen();
    if (!s || !pad_) return;
    BitBlt(s, x, y, pad_->width(), pad_->height(), pad_->hdc(), 0, 0, SRCCOPY);
    pad_x_ = x;
    pad_y_ = y;
    pad_shown_ = true;
}

// One seg15:0x0 drawing (shell.md item 9, Drawing) [BYTES seg15:0x163 seg15:0x30b seg15:0x32b
// seg15:0x377 seg15:0x390 seg15:0x3d4 seg15:0x4e1 seg15:0x518 seg15:0x55f seg15:0x58d]. The shadow
// that the original adds at 8 bpp or less (ROP 0x00A803A9, seg15:0x4c5) is left out: the port
// shows what the oracle's true-colour display shows [DESIGN, ADR 0007] [OPEN seg15:0x4c5].
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
    if (pad_) part(d.pad, pad_->hdc(), d.pad.l, d.pad.t);
    const ShipFrame& f = frames_[static_cast<std::size_t>(std::clamp(d.f, 0, kShipFrames - 1))];
    const int fx = frames_[0].w / 2 - f.w / 2;
    BitBlt(w, fx, 0, f.w, f.h, ship_mask_->hdc(), 0, f.y, SRCAND);
    BitBlt(w, fx, 0, f.w, f.h, ship_->hdc(), 0, f.y, SRCPAINT);
    BitBlt(s, d.ship.l, d.ship.t, ww, wh, w, 0, 0, SRCCOPY);
}

void GdiDesk::free_all() {
    strip_.reset();
    pad_.reset();
    ship_.reset();
    ship_mask_.reset();
    work_.reset();
    scene_.reset();
    pad_shown_ = false;
}

}  // namespace is::desk
#endif
