// hud::Canvas on the ADR 0006 surface and the HUD's index bitmaps (ADR 0007), with the GDI
// calls the original makes. Every colour goes through gfx::Gfx::dc_colour. Win32 only.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <array>
#include <functional>
#include <string>

#include "gfx/gfx.h"
#include "hud/canvas.h"

namespace is::hud {

class GdiCanvas final : public Canvas {
public:
    // `font` gives DGROUP:0x90fe, the HFONT of the IS_INFO texts.
    GdiCanvas(gfx::Gfx& gfx, std::function<HFONT()> font);
    ~GdiCanvas() override;
    GdiCanvas(const GdiCanvas&) = delete;
    GdiCanvas& operator=(const GdiCanvas&) = delete;

    // seg11:0x0's HUD loads [BYTES seg11:0x4b-0xdd]: BITMAP METAL, graphics 250 and 240
    // (seg10:0x6c02(id, 0, -1)), graphic 45, graphic 260 (mode 0 at 8 bits per pixel), BITMAP
    // DIGITAL and BITMAP SYSTEMS, and the METAL pattern brush DGROUP:0x500a. RADSHIP, the radar
    // bitmap and the radar brushes are the radar's (implementer D). Returns false, with `why`,
    // when one is missing.
    bool load(std::string* why);
    // An image in a slot (tests, and the weapon picture DGROUP:0x5040 [OPEN: its loader]).
    void set_image(Img img, gfx::ImagePtr image);
    gfx::ImagePtr image(Img img) const { return images_[static_cast<std::size_t>(img)]; }
    // DGROUP:0x500a, the METAL pattern brush (null before load()) (M3 s2 fixer: the radar's
    // brush 3).
    HBRUSH metal_brush() const { return metal_; }

    bool has(Img img) const override;
    int width(Img img) const override;
    int height(Img img) const override;
    void create(Img img, int w, int h) override;
    void pat_blt(Img dst, int x, int y, int w, int h, std::uint32_t rop,
                 const Brush& brush) override;
    void bit_blt(Img dst, int x, int y, int w, int h, Img src, int sx, int sy, std::uint32_t rop,
                 const Brush& brush) override;
    void lines(Img dst, const Pen& pen, const Point* p, int n) override;
    void rectangle(Img dst, const Pen& pen, const Brush& brush, int l, int t, int r,
                   int b) override;
    void set_pixel(Img dst, int x, int y, Colour c) override;
    void frame(Img dst, int x, int y, int w, int h, int style) override;
    void font_text(int font, int align, Colour c, int x, int y, const std::string& text) override;
    void text_out(Img dst, int x, int y, unsigned align, Colour c,
                  const std::string& text) override;
    void set_view(int x, int y, int w, int h) override;
    void clear_view() override;

private:
    HDC dc(Img img) const;
    HBRUSH make_brush(const Brush& b, bool* owned) const;
    HPEN make_pen(const Pen& p, bool* owned) const;

    gfx::Gfx& gfx_;
    std::function<HFONT()> font_;
    std::array<gfx::ImagePtr, static_cast<std::size_t>(Img::kCount)> images_{};
    HBRUSH metal_ = nullptr;  // DGROUP:0x500a
    int view_saved_ = 0;
};

}  // namespace is::hud
#endif
