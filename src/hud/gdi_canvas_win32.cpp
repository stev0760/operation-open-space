// Win32 only: the host build compiles this file empty.
#ifdef _WIN32
#include "hud/gdi_canvas.h"

#include <exception>

#include "formats/bytes.h"
#include "gfx/font.h"

namespace is::hud {

namespace {

COLORREF resolve(COLORREF c, const void* ctx) {
    return static_cast<const gfx::Gfx*>(ctx)->dc_colour(c);
}

}  // namespace

GdiCanvas::GdiCanvas(gfx::Gfx& gfx, std::function<HFONT()> font)
    : gfx_(gfx), font_(std::move(font)) {}

GdiCanvas::~GdiCanvas() {
    if (metal_) DeleteObject(metal_);
}

bool GdiCanvas::load(std::string* why) {
    try {
        set_image(Img::kMetal, gfx_.load_bitmap("METAL"));      // seg11:0x4f: DGROUP:0x5032
        set_image(Img::kLamps, gfx_.load_graphic(0xfa, 0, -1));  // seg11:0x61: DGROUP:0x5034
        set_image(Img::kButton, gfx_.load_graphic(0xf0, 0, -1)); // seg11:0x73: DGROUP:0x5036
        set_image(Img::kTiles, gfx_.load_graphic(0x2d, 0, -1));  // seg11:0x89: DGROUP:0x5044
        // seg11:0x94-0xa8: graphic 260 in mode 0 at 8 bits per pixel (DGROUP:0x90f0 != 0).
        set_image(Img::kFuelBar, gfx_.load_graphic(0x104, 0, -1));
        set_image(Img::kDigital, gfx_.load_bitmap("DIGITAL"));  // seg11:0xb7: DGROUP:0x5038
        set_image(Img::kSystems, gfx_.load_bitmap("SYSTEMS"));  // seg11:0xd5: DGROUP:0x5030
    } catch (const std::exception& x) {
        if (why) *why = x.what();
        return false;
    }
    if (metal_) DeleteObject(metal_);
    metal_ = CreatePatternBrush(images_[static_cast<std::size_t>(Img::kMetal)]->bitmap());  // seg11:0xe9
    if (!metal_) {
        if (why) *why = "CreatePatternBrush(METAL) failed";
        return false;
    }
    return true;
}

void GdiCanvas::set_image(Img img, gfx::ImagePtr image) {
    images_[static_cast<std::size_t>(img)] = std::move(image);
}

HDC GdiCanvas::dc(Img img) const {
    if (img == Img::kScreen) return gfx_.surface().empty() ? nullptr : gfx_.dc();
    const auto& p = images_[static_cast<std::size_t>(img)];
    return p ? p->hdc() : nullptr;
}

bool GdiCanvas::has(Img img) const {
    if (img == Img::kScreen) return !gfx_.surface().empty();
    return images_[static_cast<std::size_t>(img)] != nullptr;
}

int GdiCanvas::width(Img img) const {
    if (img == Img::kScreen) return gfx_.logical_width();
    const auto& p = images_[static_cast<std::size_t>(img)];
    return p ? p->width() : 0;
}

int GdiCanvas::height(Img img) const {
    if (img == Img::kScreen) return gfx_.logical_height();
    const auto& p = images_[static_cast<std::size_t>(img)];
    return p ? p->height() : 0;
}

void GdiCanvas::create(Img img, int w, int h) {
    if (img == Img::kScreen) return;
    set_image(img, gfx_.create_image(w, h));  // CreateCompatibleBitmap: black
}

HBRUSH GdiCanvas::make_brush(const Brush& b, bool* owned) const {
    *owned = false;
    switch (b.kind) {
    case Brush::Kind::kSolid:
        *owned = true;
        return CreateSolidBrush(gfx_.dc_colour(b.colour));
    case Brush::Kind::kStock:
        if (b.stock == kNullBrush) return static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        // The stock brushes are plain RGB colours; the LTGRAY one is RGB(192, 192, 192), a
        // static colour (font.cpp draw_frame does the same) [DESIGN ADR 0007].
        *owned = true;
        if (b.stock == kLtGrayBrush) return CreateSolidBrush(gfx_.dc_colour(RGB(0xc0, 0xc0, 0xc0)));
        return CreateSolidBrush(gfx_.dc_colour(RGB(0, 0, 0)));
    case Brush::Kind::kMetal: return metal_;
    case Brush::Kind::kNone: break;
    }
    return nullptr;
}

HPEN GdiCanvas::make_pen(const Pen& p, bool* owned) const {
    *owned = true;
    if (p.is_stock)  // WHITE_PEN and BLACK_PEN are the static white and black [DESIGN ADR 0007]
        return CreatePen(PS_SOLID, 1, gfx_.dc_colour(p.stock == kWhitePen ? RGB(255, 255, 255)
                                                                          : RGB(0, 0, 0)));
    return CreatePen(PS_SOLID, 1, gfx_.dc_colour(p.colour));
}

void GdiCanvas::pat_blt(Img dst, int x, int y, int w, int h, std::uint32_t rop,
                        const Brush& brush) {
    HDC d = dc(dst);
    if (!d) return;
    bool owned = false;
    HBRUSH b = make_brush(brush, &owned);
    HGDIOBJ old = b ? SelectObject(d, b) : nullptr;
    PatBlt(d, x, y, w, h, rop);
    if (old) SelectObject(d, old);
    if (owned && b) DeleteObject(b);
}

void GdiCanvas::bit_blt(Img dst, int x, int y, int w, int h, Img src, int sx, int sy,
                        std::uint32_t rop, const Brush& brush) {
    HDC d = dc(dst), s = dc(src);
    if (!d || !s) return;
    bool owned = false;
    HBRUSH b = make_brush(brush, &owned);
    HGDIOBJ old = b ? SelectObject(d, b) : nullptr;
    // A monochrome source maps through the destination's text (0) and background (1) colours;
    // the original's DCs had black and white [REF Win16 SDK BitBlt] [DESIGN ADR 0007].
    const COLORREF tc = SetTextColor(d, 0x10FF0000u | 0u);
    const COLORREF bc = SetBkColor(d, 0x10FF0000u | 255u);
    BitBlt(d, x, y, w, h, s, sx, sy, rop);
    SetTextColor(d, tc);
    SetBkColor(d, bc);
    if (old) SelectObject(d, old);
    if (owned && b) DeleteObject(b);
}

void GdiCanvas::lines(Img dst, const Pen& pen, const Point* p, int n) {
    HDC d = dc(dst);
    if (!d || n < 1) return;
    bool owned = false;
    HPEN hp = make_pen(pen, &owned);
    HGDIOBJ old = SelectObject(d, hp);
    MoveToEx(d, p[0].x, p[0].y, nullptr);
    for (int i = 1; i < n; ++i) LineTo(d, p[i].x, p[i].y);
    SelectObject(d, old);
    if (owned) DeleteObject(hp);
}

void GdiCanvas::rectangle(Img dst, const Pen& pen, const Brush& brush, int l, int t, int r,
                          int b) {
    HDC d = dc(dst);
    if (!d) return;
    bool pen_owned = false, brush_owned = false;
    HPEN hp = make_pen(pen, &pen_owned);
    HBRUSH hb = make_brush(brush, &brush_owned);
    HGDIOBJ op = SelectObject(d, hp);
    HGDIOBJ ob = hb ? SelectObject(d, hb) : nullptr;
    Rectangle(d, l, t, r, b);
    SelectObject(d, op);
    if (ob) SelectObject(d, ob);
    if (pen_owned) DeleteObject(hp);
    if (brush_owned && hb) DeleteObject(hb);
}

void GdiCanvas::set_pixel(Img dst, int x, int y, Colour c) {
    if (HDC d = dc(dst)) SetPixel(d, x, y, gfx_.dc_colour(c));
}

void GdiCanvas::stretch_blt(Img dst, int x, int y, int w, int h, Img src, int sx, int sy, int sw,
                            int sh, std::uint32_t rop) {
    HDC d = dc(dst), s = dc(src);
    if (!d || !s) return;
    const int mode = SetStretchBltMode(d, COLORONCOLOR);  // seg13:0x5c98: mode 3
    StretchBlt(d, x, y, w, h, s, sx, sy, sw, sh, rop);
    SetStretchBltMode(d, mode);
}

void GdiCanvas::frame(Img dst, int x, int y, int w, int h, int style) {
    if (HDC d = dc(dst)) gfx::draw_frame(d, x, y, w, h, style, resolve, &gfx_);
}

void GdiCanvas::font_text(int font, int align, Colour c, int x, int y, const std::string& text) {
    if (text.empty()) return;
    gfx_.font_text(font, align, c, x, y, text);
}

void GdiCanvas::text_out(Img dst, int x, int y, unsigned align, Colour c,
                         const std::string& text) {
    HDC d = dc(dst);
    if (!d || text.empty()) return;
    const int saved = SaveDC(d);
    if (font_) {
        if (HFONT f = font_()) SelectObject(d, f);
    }
    SetBkMode(d, TRANSPARENT);
    SetTextAlign(d, align);
    SetTextColor(d, gfx_.dc_colour(c));
    const std::u16string w = fmt::cp1252_to_utf16(text);
    TextOutW(d, x, y, reinterpret_cast<const wchar_t*>(w.c_str()), static_cast<int>(w.size()));
    RestoreDC(d, saved);
}

void GdiCanvas::set_view(int x, int y, int w, int h) {
    HDC d = dc(Img::kScreen);
    if (!d) return;
    view_saved_ = SaveDC(d);
    SetViewportOrgEx(d, x, y, nullptr);
    IntersectClipRect(d, 0, 0, w, h);
}

void GdiCanvas::clear_view() {
    HDC d = dc(Img::kScreen);
    if (!d || view_saved_ == 0) return;
    RestoreDC(d, view_saved_);
    view_saved_ = 0;
}

}  // namespace is::hud
#endif
