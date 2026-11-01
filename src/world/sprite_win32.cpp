// GdiBlitter: the wave's Blitter on the renderer's surface and images (ADR 0013 decision 1,
// ADR 0006 decision 2, ADR 0007). Win32 only.
#ifdef _WIN32
#include <windows.h>

#include <exception>

#include "gfx/gfx.h"
#include "world/rock_images.h"
#include "world/sprite.h"

namespace is::world {

GdiBlitter::GdiBlitter(gfx::Gfx& gfx) : gfx_(gfx) { set_wave_blitter(this); }

GdiBlitter::~GdiBlitter() {
    if (wave_blitter() == this) {
        set_wave_blitter(nullptr);
        // The brush and image ids of wave_graphics() are this Blitter's: none outlives it
        // (M3 s4 fixer, review L4).
        wave_graphics() = WaveGraphics{};
    }
    // An owned brush may still be selected into the surface DC (the radar callbacks leave theirs
    // selected, as the original does): a stock brush goes back first (M3 s2 fixer, review L2).
    if (HDC d = gfx_.dc()) SelectObject(d, GetStockObject(WHITE_BRUSH));
    for (auto& [id, img] : images_)
        if (img && img->hdc()) SelectObject(img->hdc(), GetStockObject(WHITE_BRUSH));
    for (auto& [id, b] : brushes_)
        if (b.owned && b.h) DeleteObject(b.h);
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

}  // namespace is::world
#endif
