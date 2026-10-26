// The briefing's Canvas on the renderer of src/gfx: the GDI calls of seg9:0x8934, seg9:0x8d18,
// seg11:0x1774 and seg17 on the surface's DC (DGROUP:0x13dc), every COLORREF through
// Gfx::dc_colour (ADR 0007). Win32 only; the host preset builds an empty translation unit.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <map>
#include <string>
#include <tuple>

#include "briefing/briefing.h"
#include "gfx/gfx.h"

namespace is::briefing {

class GdiCanvas final : public Canvas {
public:
    explicit GdiCanvas(gfx::Gfx& gfx) : gfx_(gfx) {}
    ~GdiCanvas() override;

    void black(int x, int y, int w, int h) override;
    void fill(int x, int y, int w, int h, Colour brush) override;
    void line(Colour pen, int x0, int y0, int x1, int y1) override;
    void rectangle(Colour pen, bool ltgray, int l, int t, int r, int b) override;
    void set_pixel(int x, int y, Colour c) override;
    void text(const Font& f, Colour c, unsigned align, int x, int y,
              const std::string& cp1252) override;
    int text_width(const Font& f, const std::string& cp1252) override;
    void clip(bool on, int l, int t, int r, int b) override;
    void icon(int x, int y, int w, int h, int sx, int sy, int sw, int sh) override;
    // Slot 5, the logo strip of seg10:0x39a8, from the renderer; the other slots are the wave
    // set-up's (M3) and report false [OPEN].
    bool has_slot(int slot) override;
    void slot_blit(int slot, int x, int y, int w, int h, int sx, int sy, int sw, int sh) override;

    // The icon cells DGROUP:0x3a02 (an image of the renderer), or none.
    void set_icon_cells(gfx::ImagePtr cells) { cells_ = std::move(cells); }

private:
    HDC dc() const { return gfx_.dc(); }
    HFONT font(const Font& f);

    gfx::Gfx& gfx_;
    gfx::ImagePtr cells_;
    gfx::ImagePtr strip_;  // DGROUP:0x39e8[5]
    bool strip_tried_ = false;
    // The fonts by (height, width, weight): the original creates and deletes one per call; the
    // port keeps them, which draws the same [DESIGN].
    std::map<std::tuple<int, int, int>, HFONT> fonts_;
};

}  // namespace is::briefing
#endif
