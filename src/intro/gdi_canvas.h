// The intro's Canvas on the renderer of src/gfx: the GDI calls of the original on the surface's
// DC (DGROUP:0x13dc), every COLORREF through Gfx::dc_colour (ADR 0007), the images as index
// bitmaps of the renderer. Win32 only; the host preset builds an empty translation unit.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <functional>
#include <map>
#include <string>

#include "gfx/gfx.h"
#include "intro/intro.h"

namespace is::intro {

class GdiCanvas final : public Canvas {
public:
    // `log` receives a line when an image cannot be made (a missing resource); the drawing that
    // needs it then does nothing [DESIGN].
    GdiCanvas(gfx::Gfx& gfx, std::function<void(const std::string&)> log);
    ~GdiCanvas() override;

    void build_strip() override;
    void load_sd_logo() override;
    void build_credit_cells(int mode) override;
    void load_title_picture() override;
    void make_title_reveal() override;
    void free_image(Image image) override;

    void black(int x, int y, int w, int h) override;
    void blit(Image src, int dx, int dy, int w, int h, int sx, int sy) override;
    void stretch(Image src, int dx, int dy, int dw, int dh, int sx, int sy, int sw,
                 int sh) override;
    void double_copy(Image src, int dx, int dy, int sx, int sy, int sw, int sh) override;
    void merge(Image src, int dx, int dy, int w, int h, int sx, int sy, Colour brush) override;
    void line(Colour pen, int width, int x0, int y0, int x1, int y1) override;

    FontId create_font(const FontSpec& spec) override;
    FontId select_font(FontId font) override;
    void delete_font(FontId font) override;
    Colour text_colour(Colour c) override;
    Colour bk_colour(Colour c) override;
    void bk_mode(int mode) override;
    void text_align(unsigned align) override;
    void text_out(int x, int y, const std::string& cp1252) override;
    void move_to(int x, int y) override;
    Point current_position() override;
    int text_width(const std::string& cp1252) override;
    void font_text(int font, int align, Colour colour, int x, int y,
                   const std::string& cp1252) override;

    void palette_scheme(int c, int mode) override;
    void palette_ramp(int c) override;
    void palette_animate() override;
    void release_dc() override;

    // The image behind `image`, for the tests.
    const gfx::Surface* image(Image image) const;

private:
    HDC dc() const;
    gfx::ImagePtr& slot(Image image);
    void fail(const char* what, const std::exception& x);

    gfx::Gfx& gfx_;
    std::function<void(const std::string&)> log_;
    gfx::ImagePtr strip_, sd_logo_, credits_, title_, reveal_;
    Colour text_ = 0x00000000, bk_ = 0x00FFFFFF;  // the logical colours, as last set
};

}  // namespace is::intro
#endif
