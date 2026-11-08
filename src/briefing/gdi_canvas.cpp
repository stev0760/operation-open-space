#include "briefing/gdi_canvas.h"

#include "world/world.h"

#ifdef _WIN32
#include <exception>
#include <string>
#include <vector>

namespace is::briefing {

namespace {
std::wstring widen1252(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
}  // namespace

GdiCanvas::~GdiCanvas() {
    for (auto& [k, f] : fonts_)
        if (f) DeleteObject(f);
}

HFONT GdiCanvas::font(const Font& f) {
    const auto key = std::make_tuple(f.height, f.width, f.weight);
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;
    // CreateFont(h, w, 0, 0, weight, 0, 0, 0, 0, 0, 0, 2, 0x22, "Arial").
    HFONT h = CreateFontW(f.height, f.width, 0, 0, f.weight, 0, 0, 0, 0, 0, 0, 2, 0x22, L"Arial");
    fonts_.emplace(key, h);
    return h;
}

void GdiCanvas::black(int x, int y, int w, int h) { PatBlt(dc(), x, y, w, h, BLACKNESS); }

void GdiCanvas::fill(int x, int y, int w, int h, Colour brush) {
    HBRUSH b = CreateSolidBrush(gfx_.dc_colour(brush));
    HGDIOBJ old = SelectObject(dc(), b);
    PatBlt(dc(), x, y, w, h, PATCOPY);
    SelectObject(dc(), old);
    DeleteObject(b);
}

void GdiCanvas::line(Colour pen, int x0, int y0, int x1, int y1) {
    HPEN p = CreatePen(PS_SOLID, 1, gfx_.dc_colour(pen));
    HGDIOBJ old = SelectObject(dc(), p);
    MoveToEx(dc(), x0, y0, nullptr);
    LineTo(dc(), x1, y1);
    SelectObject(dc(), old);
    DeleteObject(p);
}

void GdiCanvas::rectangle(Colour pen, bool ltgray, int l, int t, int r, int b) {
    HPEN p = CreatePen(PS_SOLID, 1, gfx_.dc_colour(pen));
    HGDIOBJ oldp = SelectObject(dc(), p);
    // LTGRAY_BRUSH is a static colour (0xC0C0C0); the surface takes it through dc_colour like
    // the other colours [DESIGN].
    HBRUSH br = ltgray ? CreateSolidBrush(gfx_.dc_colour(0x00C0C0C0))
                       : static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HGDIOBJ oldb = SelectObject(dc(), br);
    Rectangle(dc(), l, t, r, b);
    SelectObject(dc(), oldb);
    SelectObject(dc(), oldp);
    DeleteObject(p);
    if (ltgray) DeleteObject(br);
}

void GdiCanvas::set_pixel(int x, int y, Colour c) { SetPixel(dc(), x, y, gfx_.dc_colour(c)); }

void GdiCanvas::text(const Font& f, Colour c, unsigned align, int x, int y,
                     const std::string& cp1252) {
    HGDIOBJ old = SelectObject(dc(), font(f));
    const COLORREF bk = SetBkColor(dc(), gfx_.dc_colour(0));
    const int mode = SetBkMode(dc(), TRANSPARENT);
    const UINT al = SetTextAlign(dc(), align);
    const COLORREF tc = SetTextColor(dc(), gfx_.dc_colour(c));
    const std::wstring w = widen1252(cp1252);
    // seg9:0x8bec's TextOut (GDI.33) [BYTES seg9:0x8cab seg9:0x8cdb], with the glyphs placed at the
    // advances of the text extent. On the port's 8-bit DIB section (ADR 0007 decision 2) Wine draws
    // monochrome glyphs and steps by their hinted cells, which fall short of the extent TA_RIGHT and
    // TA_CENTER place the string by; on the original's true-colour window DC it steps by the
    // extent. CreateFont gets the original's arguments (seg9:0x8c19-0x8c3d: h, w, 0, 0, 600, ...,
    // 2, 0x22, "Arial"), and the font is not the difference: "PRESS 'ESC' TO CONTINUE" at (14, 0)
    // is 160 px of extent at both depths, its ink spans 158 px on a 32-bit DIB as in the oracle's
    // debriefing (642 ... 799), 150 px on an 8-bit DIB, and 158 px on the 8-bit DIB with the
    // extent's advances (a Win32 GDI probe under the oracle's Wine 11.0). Where the two agree, as
    // GDI on Windows keeps them, the advances change nothing [DESIGN ADR 0007] [ORACLE core-0001]
    // (M4 s5, triage S1-7).
    const int n = static_cast<int>(w.size());
    std::vector<INT> part(static_cast<std::size_t>(n)), dx(static_cast<std::size_t>(n));
    SIZE ext{};
    if (n > 1 && GetTextExtentExPointW(dc(), w.c_str(), n, 0, nullptr, part.data(), &ext)) {
        for (int i = 0; i < n; ++i)
            dx[static_cast<std::size_t>(i)] = part[static_cast<std::size_t>(i)] - (i > 0 ? part[static_cast<std::size_t>(i - 1)] : 0);
        ExtTextOutW(dc(), x, y, 0, nullptr, w.c_str(), static_cast<UINT>(n), dx.data());
    } else {
        TextOutW(dc(), x, y, w.c_str(), n);
    }
    SetTextColor(dc(), tc);
    SetTextAlign(dc(), al);
    SetBkMode(dc(), mode);
    SetBkColor(dc(), bk);
    SelectObject(dc(), old);
}

int GdiCanvas::text_width(const Font& f, const std::string& cp1252) {
    HGDIOBJ old = SelectObject(dc(), font(f));
    const std::wstring w = widen1252(cp1252);
    SIZE sz{};
    GetTextExtentPoint32W(dc(), w.c_str(), static_cast<int>(w.size()), &sz);
    SelectObject(dc(), old);
    return sz.cx;
}

void GdiCanvas::clip(bool on, int l, int t, int r, int b) {
    if (!on) {
        SelectClipRgn(dc(), nullptr);
        return;
    }
    HRGN rgn = CreateRectRgn(l, t, r, b);
    SelectClipRgn(dc(), rgn);  // the DC keeps a copy
    DeleteObject(rgn);
}

void GdiCanvas::icon(int x, int y, int w, int h, int sx, int sy, int sw, int sh) {
    if (!cells_) return;
    // SetStretchBltMode(3) in seg17:0x600; seg10:0x9962's stretch with SRCCOPY.
    const int old = SetStretchBltMode(dc(), COLORONCOLOR);
    StretchBlt(dc(), x, y, w, h, cells_->hdc(), sx, sy, sw, sh, SRCCOPY);
    SetStretchBltMode(dc(), old);
}

gfx::ImagePtr GdiCanvas::slot_image(int slot) {
    // A ship's sheet, slot 0x28 + i of DGROUP:0x39e8, comes from the wave's image table
    // (src/world/world.h slot_image) [BYTES seg17:0x2109-0x211a seg17:0x255e-0x2563].
    if (slot >= kSlotShipBase) return world::slot_image(slot);
    if (slot != kSlotStrip) return {};
    if (!strip_tried_) {
        strip_tried_ = true;
        try {
            strip_ = gfx_.logo_strip();
        } catch (const std::exception&) {
            strip_.reset();
        }
    }
    return strip_;
}

bool GdiCanvas::has_slot(int slot) { return slot_image(slot) != nullptr; }

bool GdiCanvas::ship_sheets_linked() { return world::slot_images_linked(); }

void GdiCanvas::slot_blit(int slot, int x, int y, int w, int h, int sx, int sy, int sw, int sh) {
    const gfx::ImagePtr img = slot_image(slot);
    if (!img) return;
    // SetStretchBltMode(3) and seg10:0x9962's stretch with SRCCOPY [BYTES seg17:0x1c9e
    // seg17:0x1cde seg17:0x253a-0x2581].
    const int old = SetStretchBltMode(dc(), COLORONCOLOR);
    StretchBlt(dc(), x, y, w, h, img->hdc(), sx, sy, sw, sh, SRCCOPY);
    SetStretchBltMode(dc(), old);
}

}  // namespace is::briefing
#endif
