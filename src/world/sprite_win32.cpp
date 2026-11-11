// GdiBlitter: the wave's Blitter on the renderer's surface and images (ADR 0013 decision 1,
// ADR 0006 decision 2, ADR 0007). Win32 only.
#ifdef _WIN32
#include <windows.h>

#include <exception>

#include "gfx/gfx.h"
#include "world/rock_images.h"
#include "world/sprite.h"
#include "world/tether.h"

namespace is::world {

GdiBlitter::GdiBlitter(gfx::Gfx& gfx) : gfx_(gfx) { set_wave_blitter(this); }

GdiBlitter::~GdiBlitter() {
    if (wave_blitter() == this) {
        set_wave_blitter(nullptr);
        // The brush and image ids of wave_graphics() are this Blitter's: none outlives it
        // (M3 s4 fixer, review L4).
        wave_graphics() = WaveGraphics{};
        tether_state().pens = {};  // the rope pens' ids too (M4 s7 TETHER)
    }
    // An owned brush may still be selected into the surface DC (the radar callbacks leave theirs
    // selected, as the original does): a stock brush goes back first (M3 s2 fixer, review L2).
    if (HDC d = gfx_.dc()) SelectObject(d, GetStockObject(WHITE_BRUSH));
    for (auto& [id, img] : images_)
        if (img && img->hdc()) SelectObject(img->hdc(), GetStockObject(WHITE_BRUSH));
    for (auto& [id, b] : brushes_)
        if (b.owned && b.h) DeleteObject(b.h);
    // A created pen may still be selected: the stock black pen goes back first (M4 s7 TETHER).
    if (HDC d = gfx_.dc()) SelectObject(d, GetStockObject(BLACK_PEN));
    for (auto& [id, img] : images_)
        if (img && img->hdc()) SelectObject(img->hdc(), GetStockObject(BLACK_PEN));
    for (auto& [id, p] : pens_)
        if (p.owned && p.h) DeleteObject(p.h);
}

HDC GdiBlitter::clip(DcId id, HDC d) const {
    if (d && id == kDcWindow && clipped_) {
        SaveDC(d);
        IntersectClipRect(d, clip_.l, clip_.t, clip_.r, clip_.b);
    }
    return d;
}

void GdiBlitter::unclip(DcId id, HDC d) const {
    if (d && id == kDcWindow && clipped_) RestoreDC(d, -1);
}

DcId GdiBlitter::bind(std::shared_ptr<gfx::Surface> image) {
    if (!image || image->empty()) return kDcNone;
    const DcId id = next_dc_++;
    images_[id] = std::move(image);
    return id;
}

BrushId GdiBlitter::bind_brush(HBRUSH brush) {
    if (!brush) return kBrushNone;
    const BrushId id = next_brush_++;
    brushes_[id] = Brush{brush, false};
    return id;
}

HDC GdiBlitter::hdc(DcId id) const {
    if (id == kDcWindow || id == kDcHudWindow) return gfx_.dc();
    const auto it = images_.find(id);
    return it == images_.end() ? nullptr : it->second->hdc();
}

std::shared_ptr<gfx::Surface> GdiBlitter::image(DcId id) const {
    const auto it = images_.find(id);
    return it == images_.end() ? nullptr : it->second;
}

HBRUSH GdiBlitter::hbrush(BrushId id) const {
    if (id >= kBrushStock && id <= kBrushStock + BLACK_BRUSH)
        return static_cast<HBRUSH>(GetStockObject(id - kBrushStock));
    const auto it = brushes_.find(id);
    return it == brushes_.end() ? nullptr : it->second.h;
}

BrushId GdiBlitter::brush_id(HBRUSH h) {
    if (!h) return kBrushNone;
    for (int s = WHITE_BRUSH; s <= BLACK_BRUSH; ++s)
        if (GetStockObject(s) == h) return static_cast<BrushId>(kBrushStock + s);
    for (const auto& [id, b] : brushes_)
        if (b.h == h) return id;
    return bind_brush(h);  // a brush the DC held before: known from now on, not owned
}

void GdiBlitter::bit_blt(DcId dst, int x, int y, int w, int h, DcId src, int sx, int sy,
                         std::uint32_t rop) {
    HDC d = hdc(dst), s = hdc(src);
    if (!d || !s) return;
    clip(dst, d);
    BitBlt(d, x, y, w, h, s, sx, sy, rop);
    unclip(dst, d);
}

void GdiBlitter::pat_blt(DcId dst, int x, int y, int w, int h, std::uint32_t rop) {
    HDC d = hdc(dst);
    if (!d) return;
    clip(dst, d);
    PatBlt(d, x, y, w, h, rop);
    unclip(dst, d);
}

void GdiBlitter::stretch_blt(DcId dst, int x, int y, int w, int h, DcId src, int sx, int sy,
                             int sw, int sh, std::uint32_t rop) {
    HDC d = hdc(dst), s = hdc(src);
    if (!d || !s) return;
    clip(dst, d);
    StretchBlt(d, x, y, w, h, s, sx, sy, sw, sh, rop);
    unclip(dst, d);
}

void GdiBlitter::set_stretch_mode(DcId dc, int mode) {
    if (HDC d = hdc(dc)) SetStretchBltMode(d, mode);
}

BrushId GdiBlitter::select_brush(DcId dc, BrushId brush) {
    HDC d = hdc(dc);
    HBRUSH b = hbrush(brush);
    if (!d || !b) return kBrushNone;
    return brush_id(static_cast<HBRUSH>(SelectObject(d, b)));
}

void GdiBlitter::set_pixel(DcId dc, int x, int y, std::uint32_t colorref) {
    HDC d = hdc(dc);
    if (!d) return;
    clip(dc, d);
    SetPixel(d, x, y, gfx_.dc_colour(colorref));
    unclip(dc, d);
}

void GdiBlitter::frame_rect(DcId dc, const Rect16& r, BrushId brush) {
    HDC d = hdc(dc);
    HBRUSH b = hbrush(brush);
    if (!d || !b) return;
    const RECT rc{r.l, r.t, r.r, r.b};
    FrameRect(d, &rc, b);
}

DcId GdiBlitter::create_image(int w, int h) { return bind(gfx_.create_image(w, h)); }

// Surface::index8 and the indices row by row into its DIB section (stride (w + 3) & ~3); the
// DIB section is the bitmap, so GDI sees the pixels at once [DESIGN ADR 0013] (M5 s1 SPRITE).
DcId GdiBlitter::create_image_from(int w, int h, const std::vector<std::uint8_t>& index) {
    if (w <= 0 || h <= 0) return kDcNone;
    auto s = std::make_shared<gfx::Surface>(gfx::Surface::index8(w, h));
    if (s->empty() || !s->pixels()) return kDcNone;
    GdiFlush();
    const std::size_t need = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    for (int y = 0; y < h; ++y) {
        std::uint8_t* row = s->pixels() + static_cast<std::size_t>(y) * s->stride();
        for (int x = 0; x < w; ++x) {
            const std::size_t k = static_cast<std::size_t>(y) * w + x;
            row[x] = k < need && k < index.size() ? index[k] : 0;
        }
    }
    return bind(std::move(s));
}

void GdiBlitter::delete_image(DcId id) { images_.erase(id); }

// seg10:0x6ec6 -> ordinal 6 on the image behind `src` (Gfx::mask): a monochrome Surface in its
// own DC (M4 s1 DEATH).
DcId GdiBlitter::create_mask(DcId src) {
    const std::shared_ptr<gfx::Surface> img = image(src);
    if (!img || img->empty()) return kDcNone;
    return bind(gfx_.mask(*img));
}

DcId GdiBlitter::load_bitmap(const std::string& name) {
    try {
        return bind(gfx_.load_bitmap(name));
    } catch (const std::exception&) {
        return kDcNone;  // the resource is missing: nothing to draw from [DESIGN]
    }
}

DcId GdiBlitter::load_graphic(int id, int mode, int arg) {
    try {
        return bind(gfx_.load_graphic(id, mode, arg));
    } catch (const std::exception&) {
        return kDcNone;
    }
}

BrushId GdiBlitter::create_solid_brush(std::uint32_t colorref) {
    HBRUSH h = CreateSolidBrush(gfx_.dc_colour(colorref));
    if (!h) return kBrushNone;
    const BrushId id = next_brush_++;
    brushes_[id] = Brush{h, true};
    return id;
}

// CreateBitmap(8, 8, 1, 1, rows) and CreatePatternBrush, as seg10:0xd8-0x10f makes
// DGROUP:0x3820 and DGROUP:0x5ea8: word-aligned rows, the byte then 0x00 (shell.md "Pattern
// brushes") [BYTES seg10:0xe4 seg10:0x10a]. The bitmap is deleted at once: the brush keeps its
// own copy of the pattern [REF Win32 SDK CreatePatternBrush].
BrushId GdiBlitter::create_pattern_brush(const std::uint8_t (&rows)[8]) {
    std::uint8_t bits[16] = {};
    for (int i = 0; i < 8; ++i) bits[2 * i] = rows[i];
    HBITMAP bm = CreateBitmap(8, 8, 1, 1, bits);
    if (!bm) return kBrushNone;
    HBRUSH h = CreatePatternBrush(bm);
    DeleteObject(bm);
    if (!h) return kBrushNone;
    const BrushId id = next_brush_++;
    brushes_[id] = Brush{h, true};
    return id;
}

// --- Pens and lines (M4 s7 TETHER) -------------------------------------------------------------
// CreatePen(style, width, c) with c through Gfx::dc_colour, as create_solid_brush does (ADR
// 0007): the ropes' RGB COLORREFs reach the 8-bit surface by its colour rule [BYTES
// seg12:0x3289 seg10:0x4db2-0x4df3].
PenId GdiBlitter::create_pen(int style, int width, std::uint32_t colorref) {
    HPEN h = CreatePen(style, width, gfx_.dc_colour(colorref));
    if (!h) return kPenNone;
    const PenId id = next_pen_++;
    pens_[id] = Pen{h, true};
    return id;
}

HPEN GdiBlitter::hpen(PenId id) const {
    if (id >= kPenStock + WHITE_PEN && id <= kPenStock + NULL_PEN)
        return static_cast<HPEN>(GetStockObject(id - kPenStock));
    const auto it = pens_.find(id);
    return it == pens_.end() ? nullptr : it->second.h;
}

PenId GdiBlitter::pen_id(HPEN h) {
    if (!h) return kPenNone;
    for (int s = WHITE_PEN; s <= NULL_PEN; ++s)
        if (GetStockObject(s) == h) return static_cast<PenId>(kPenStock + s);
    for (const auto& [id, p] : pens_)
        if (p.h == h) return id;
    const PenId id = next_pen_++;  // a pen the DC held before: known from now on, not owned
    pens_[id] = Pen{h, false};
    return id;
}

PenId GdiBlitter::select_pen(DcId dc, PenId pen) {
    HDC d = hdc(dc);
    HPEN p = hpen(pen);
    if (!d || !p) return kPenNone;
    return pen_id(static_cast<HPEN>(SelectObject(d, p)));
}

// DeleteObject(pen): the original deletes a pen after it selected the old one back
// [BYTES seg12:0x32c8]; a stock pen is never deleted.
void GdiBlitter::delete_pen(PenId pen) {
    const auto it = pens_.find(pen);
    if (it == pens_.end() || !it->second.owned) return;
    if (it->second.h) DeleteObject(it->second.h);
    pens_.erase(it);
}

void GdiBlitter::move_to(DcId dc, int x, int y) {
    if (HDC d = hdc(dc)) MoveToEx(d, x, y, nullptr);  // GDI.20 MoveTo
}

void GdiBlitter::line_to(DcId dc, int x, int y) {
    HDC d = hdc(dc);
    if (!d) return;
    clip(dc, d);
    LineTo(d, x, y);  // GDI.19 LineTo
    unclip(dc, d);
}

}  // namespace is::world
#endif
