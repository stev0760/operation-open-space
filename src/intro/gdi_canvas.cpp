#ifdef _WIN32
#include "intro/gdi_canvas.h"

#include <cstring>
#include <exception>

#include "formats/bytes.h"
#include "gfx/smooth_render.h"
#include "intro/pixels.h"

namespace is::intro {

namespace {

std::wstring widen(const std::string& cp1252) {
    const std::u16string u = fmt::cp1252_to_utf16(cp1252);
    return std::wstring(u.begin(), u.end());
}

// The image graphic 300 lives in (shell.md "Picture").
constexpr int kTitleGraphic = 300;
// IGRAPHIC.DLL #4000 340: nine 37 x 37 cells; the slot is 383 x 47 with cell k at (5 + 42k, 5)
// [BYTES seg10:0x5925 seg10:0x5980 seg10:0x5991 seg10:0x59da seg10:0x5a1c].
constexpr int kCreditGraphic = 340;

}  // namespace

GdiCanvas::GdiCanvas(gfx::Gfx& gfx, std::function<void(const std::string&)> log)
    : gfx_(gfx), log_(std::move(log)) {}

GdiCanvas::~GdiCanvas() = default;

HDC GdiCanvas::dc() const { return gfx_.surface().empty() ? nullptr : gfx_.dc(); }

gfx::ImagePtr& GdiCanvas::slot(Image image) {
    switch (image) {
    case Image::kStrip: return strip_;
    case Image::kSdLogo: return sd_logo_;
    case Image::kCredits: return credits_;
    case Image::kTitle: return title_;
    case Image::kTitleReveal: break;
    }
    return reveal_;
}

const gfx::Surface* GdiCanvas::image(Image image) const {
    return const_cast<GdiCanvas*>(this)->slot(image).get();
}

void GdiCanvas::fail(const char* what, const std::exception& x) {
    if (log_) log_(std::string(what) + ": " + x.what());
}

// ---- images

void GdiCanvas::build_strip() {
    if (strip_) return;
    try {
        strip_ = gfx_.logo_strip();
    } catch (const std::exception& x) {
        fail("logo strip (seg10:0x39a8)", x);
    }
}

void GdiCanvas::load_sd_logo() {
    try {
        sd_logo_ = gfx_.load_bitmap("SD_LOGO");
    } catch (const std::exception& x) {
        fail("SD_LOGO (seg15:0x5914)", x);
    }
}

void GdiCanvas::build_credit_cells(int mode) {
    credits_.reset();  // seg10:0x552(0x16, 1)
    try {
        const gfx::ImagePtr cells = gfx_.load_graphic(kCreditGraphic, mode, -1);
        gfx::ImagePtr slot = gfx_.create_image(383, 47);
        PatBlt(slot->hdc(), 0, 0, 383, 47, BLACKNESS);
        for (int k = 0; k < 9; ++k)
            BitBlt(slot->hdc(), 5 + 42 * k, 5, 37, 37, cells->hdc(), 37 * k, 0, SRCCOPY);
        slot->flush();
        credits_ = slot;
    } catch (const std::exception& x) {
        fail("credit logo cells (seg10:0x58ce)", x);
    }
}

void GdiCanvas::load_title_picture() {
    try {
        title_ = gfx_.load_graphic(kTitleGraphic, 0, -1);
    } catch (const std::exception& x) {
        fail("title picture (seg10:0x6c02)", x);
    }
}

// Ordinal 12 through seg10:0x73a6(300, "", -2, -2, 2, 0, 1, 1, 12) [BYTES seg15:0x7567-0x758c]
// (s6-ord12 verified claim 2): seg5:0x1b8e with d = 0, a = 1, b = 12 and its last argument
// DGROUP:0x1e92[slot] != 0 writes the ramps of entries 185-229 and animates (shell.md "Title
// palette"), then seg7:0x12d8 renders the smoothed 276 x 276 picture (gfx/smooth_render.h; with
// both flags set it takes b1 = b2 = 100) and returns the bitmap the title reveals row by row
// [ORACLE probe-0001: 57 960 of 57 960 pixels of (374, 229, 276 x 210)].
void GdiCanvas::make_title_reveal() {
    reveal_.reset();
    try {
        reveal_ = gfx::smooth_bitmap(gfx_, kTitleGraphic, gfx::smooth_args(1, 12));
    } catch (const std::exception& x) {
        fail("title picture (ordinal 12)", x);
    }
}

void GdiCanvas::free_image(Image image) { slot(image).reset(); }

// ---- drawing

void GdiCanvas::black(int x, int y, int w, int h) {
    if (HDC d = dc()) PatBlt(d, x, y, w, h, BLACKNESS);
}

void GdiCanvas::blit(Image src, int dx, int dy, int w, int h, int sx, int sy) {
    const gfx::ImagePtr& s = slot(src);
    if (HDC d = dc(); d && s) BitBlt(d, dx, dy, w, h, s->hdc(), sx, sy, SRCCOPY);
}

void GdiCanvas::stretch(Image src, int dx, int dy, int dw, int dh, int sx, int sy, int sw,
                        int sh) {
    const gfx::ImagePtr& s = slot(src);
    HDC d = dc();
    if (!d || !s) return;
    SetStretchBltMode(d, COLORONCOLOR);
    StretchBlt(d, dx, dy, dw, dh, s->hdc(), sx, sy, sw, sh, SRCCOPY);
}

void GdiCanvas::double_copy(Image src, int dx, int dy, int sx, int sy, int sw, int sh) {
    const gfx::ImagePtr& s = slot(src);
    if (!s || gfx_.surface().empty()) return;
    // seg15:0x7416 sets COLORONCOLOR on DGROUP:0x13dc before the copy.
    SetStretchBltMode(gfx_.dc(), COLORONCOLOR);
    gfx::Surface& surf = gfx_.surface();
    surf.flush();
    s->flush();
    intro::double_copy({surf.pixels(), surf.width(), surf.height(), surf.stride()}, dx, dy,
                       {s->pixels(), s->width(), s->height(), s->stride()}, sx, sy, sw, sh);
}

void GdiCanvas::merge(Image src, int dx, int dy, int w, int h, int sx, int sy, Colour brush) {
    const gfx::ImagePtr& s = slot(src);
    HDC d = dc();
    if (!d || !s) return;
    HBRUSH b = CreateSolidBrush(gfx_.dc_colour(brush));
    HGDIOBJ old = SelectObject(d, b);
    BitBlt(d, dx, dy, w, h, s->hdc(), sx, sy, MERGECOPY);
    SelectObject(d, old);
    DeleteObject(b);
}

void GdiCanvas::line(Colour pen, int width, int x0, int y0, int x1, int y1) {
    HDC d = dc();
    if (!d) return;
    HPEN p = CreatePen(PS_SOLID, width, gfx_.dc_colour(pen));
    HGDIOBJ old = SelectObject(d, p);
    MoveToEx(d, x0, y0, nullptr);
    LineTo(d, x1, y1);
    SelectObject(d, old);
    DeleteObject(p);
}

// ---- text

FontId GdiCanvas::create_font(const FontSpec& f) {
    // CreateFont(h, w, 0, 0, weight, italic, 0, 0, 0, 0, 0, 2, 0x22, face): ANSI_CHARSET,
    // default precisions, PROOF_QUALITY, VARIABLE_PITCH | FF_SWISS.
    return reinterpret_cast<FontId>(CreateFontW(f.height, f.width, 0, 0, f.weight,
                                                f.italic ? 1 : 0, 0, 0, ANSI_CHARSET, 0, 0,
                                                PROOF_QUALITY, VARIABLE_PITCH | FF_SWISS,
                                                widen(f.face).c_str()));
}

FontId GdiCanvas::select_font(FontId font) {
    HDC d = dc();
    if (!d || !font) return 0;
    return reinterpret_cast<FontId>(SelectObject(d, reinterpret_cast<HGDIOBJ>(font)));
}

void GdiCanvas::delete_font(FontId font) {
    if (font) DeleteObject(reinterpret_cast<HGDIOBJ>(font));
}

Colour GdiCanvas::text_colour(Colour c) {
    const Colour old = text_;
    text_ = c;
    if (HDC d = dc()) SetTextColor(d, gfx_.dc_colour(c));
    return old;
}

Colour GdiCanvas::bk_colour(Colour c) {
    const Colour old = bk_;
    bk_ = c;
    if (HDC d = dc()) SetBkColor(d, gfx_.dc_colour(c));
    return old;
}

void GdiCanvas::bk_mode(int mode) {
    if (HDC d = dc()) SetBkMode(d, mode == 1 ? TRANSPARENT : OPAQUE);
}

void GdiCanvas::text_align(unsigned align) {
    if (HDC d = dc()) SetTextAlign(d, align);
}

void GdiCanvas::text_out(int x, int y, const std::string& s) {
    HDC d = dc();
    if (!d) return;
    const std::wstring w = widen(s);
    TextOutW(d, x, y, w.c_str(), static_cast<int>(w.size()));
}

void GdiCanvas::move_to(int x, int y) {
    if (HDC d = dc()) MoveToEx(d, x, y, nullptr);
}

Point GdiCanvas::current_position() {
    POINT p{};
    if (HDC d = dc()) GetCurrentPositionEx(d, &p);
    return {static_cast<int>(p.x), static_cast<int>(p.y)};
}

int GdiCanvas::text_width(const std::string& s) {
    HDC d = dc();
    if (!d) return 0;
    const std::wstring w = widen(s);
    SIZE sz{};
    GetTextExtentPoint32W(d, w.c_str(), static_cast<int>(w.size()), &sz);
    return sz.cx;
}

void GdiCanvas::font_text(int font, int align, Colour colour, int x, int y,
                          const std::string& s) {
    gfx_.font_text(font, align, colour, x, y, s);
}

// ---- palette

void GdiCanvas::palette_scheme(int c, int mode) { gfx_.palette_scheme(c, mode); }
void GdiCanvas::palette_ramp(int c) { gfx_.palette_ramp(c); }
void GdiCanvas::palette_animate() { gfx_.palette_animate(); }

// The defaults of a fresh GetDC: text black, background white, OPAQUE, TA_LEFT | TA_TOP, the
// current position at (0, 0), BLACKONWHITE and the stock objects (timing.md "Message loop and
// idle step": the pass releases DGROUP:0x13dc) [DESIGN: the port's surface DC persists, so
// the state the intro leaves is reset here].
void GdiCanvas::release_dc() {
    HDC d = dc();
    if (!d) return;
    text_ = 0x00000000;
    bk_ = 0x00FFFFFF;
    SetTextColor(d, 0x10FF0000);  // DIBINDEX(0)
    SetBkColor(d, 0x10FF00FF);    // DIBINDEX(255)
    SetBkMode(d, OPAQUE);
    SetTextAlign(d, TA_LEFT | TA_TOP | TA_NOUPDATECP);
    MoveToEx(d, 0, 0, nullptr);
    SetStretchBltMode(d, BLACKONWHITE);
    SelectObject(d, GetStockObject(SYSTEM_FONT));
    SelectObject(d, GetStockObject(BLACK_PEN));
    SelectObject(d, GetStockObject(WHITE_BRUSH));
}

}  // namespace is::intro
#endif
