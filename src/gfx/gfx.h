// The renderer of M2 (ADR 0006, ADR 0007): an 8-bit index surface at the logical size, the game
// palette with ISPACE.DLL's palette manager, the original's images as index bitmaps, the bitmap
// font, and the presenter. This header is the API the game's screens build on; each entry point
// names the INSPACE.EXE wrapper (seg10) or the ISPACE.DLL ordinal it stands for.
//
// SURFACE (ADR 0006 decision 2, ADR 0007 decision 2)
//   Gfx::surface(), Gfx::dc()       DGROUP:0x13dc, the DC the original draws into. An 8-bpp
//                                   top-down DIB section of logical_width() x logical_height(),
//                                   pixels() / stride() everywhere, hdc() on Win32. Draw with the
//                                   GDI calls the original makes (PatBlt, BitBlt, StretchBlt,
//                                   MoveToEx/LineTo, TextOutW, CreateFontW).
//   Gfx::set_client_size()          WM_SIZE and the mode switches: factor and logical size
//                                   (shell::scale_factor); the surface keeps its overlap.
//   Gfx::factor()                   divide mouse coordinates by it (ADR 0006 Consequences).
//
// COLOURS
//   Gfx::dc_colour(COLORREF)        Every colour the game hands to GDI on the surface or an image
//                                   goes through it: it returns DIBINDEX(n) with n resolved as on
//                                   the 8-bit display (gfx::colour_index): PALETTEINDEX(n) is n,
//                                   PALETTERGB the nearest of the 256 entries, a plain RGB the
//                                   nearest static colour. [DESIGN] A COLORREF passed to GDI
//                                   without it matches against the fixed index_table(), which
//                                   is right only for the 20 static colours (stock objects).
//                                   New DCs start with text DIBINDEX(0) and background
//                                   DIBINDEX(255).
//
// PALETTE (ordinal 4 commands by name, shell.md "Palette"; the handle is DGROUP:0x920e)
//   palette_create()     seg10:0x6e00  command 0 (create, kind 0)
//   palette_delete()     seg10:0x6e26  command 2 (delete)
//   palette_animate()    seg10:0x6e4c  command 3 (animate = publish entries 10-245)
//   palette_scheme(c,b)  seg10:0x6e76  command 1 (load scheme c in mode b), then command 3
//   palette_save(b,c)    seg10:0x6da6  command 4 (save to set b, blank when c), then command 3
//   palette_restore(b)   seg10:0x6dd4  command 5 (restore set b), then command 3
//   palette_colour(r,g,b) seg10:0x6d7a command 6 (allocate a colour; entry or -1)
//   palette_ramp(c)      seg10:0x6ea0  command 7 (allocate ramp c)
//   palette_fade(m,p)    seg10:0x993a  ordinal 14 (scale ranges by percent p)
//   palettes()           the manager itself (command 8, the dispatcher, state for tests)
//   A publish changes no bitmap: every index bitmap has the fixed index_table(), so blits copy
//   indices, and the presenter converts with the device palette.
//
// IMAGES (index bitmaps are Surfaces with their own memory DC: draw from image->hdc(), and do
// not select image->bitmap() into another DC)
//   load_graphic(id,mode,arg)  seg10:0x6c02  ordinal 1: #4000 id (220, 221, 300 from
//                                            ISPACE.DLL, others IGRAPHIC.DLL) through the
//                                            recolour pass (d 3, a = b = 12) and seg5:0x2080
//   graphic_height()           seg10:0x86ca  ordinal 5: the height of the last load_graphic
//   graphic_dib(id,d,a,b,last) seg5:0x1b8e   the DIB with its recoloured table and its palette
//                                            effects: ordinal 12 (seg10:0x73a6) loads 300 with
//                                            d 0, a 1, b 12, which writes entries 185-229
//   load_bitmap(name)          seg10:0x6d38  ordinal 3: BITMAP name (SD_LOGO from ISPACE.DLL,
//                                            others IGRAPHIC.DLL); black-and-white ones are
//                                            monochrome, as LoadBitmap made them
//   mask(image)                seg10:0x6ec6  ordinal 6: 1 where the image is black
//   create_image(w,h)          CreateCompatibleBitmap(DGROUP:0x13dc, w, h) of the original,
//                              black
//   logo_strip()               seg10:0x39a8  ordinal 2 id -1000 (ISPACE.DLL seg5:0x17c2): the
//                                            650 x 52 strip of 14 logo cells
//
// TEXT AND HELPERS
//   font_text(font,align,colour,x,y,text)  seg10:0x94c4  ordinal 33 with cells 18x20, 12x18,
//                                          9x10 for fonts 0-2 (ALPHABET, opaque cells)
//   frame(x,y,w,h,style)                   seg10:0x949c  ordinal 34
//   bar(x,y,w,h,value)                     seg10:0x6d52  ordinal 35
//
// PRESENT (ADR 0007 Consequences: the one isolated step)
//   present(hwnd, force)        the frame path: converts and blits when anything changed
//   present_dc(hdc, w, h, force) the same into a given DC (WM_PAINT's BeginPaint DC)
//
// Not here: sound, ordinal 12's smoothed 2x picture (ispace.md, [OPEN]) and the other ordinals.
// Portable header: the GDI members are guarded by _WIN32; on the host the palette, surface
// buffer, image conversion and font layout work without drawing.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "formats/dib.h"
#include "gfx/font.h"
#include "gfx/palette.h"
#include "gfx/present.h"
#include "gfx/surface.h"

namespace is::assets {
class AssetStore;
}

namespace is::gfx {

using ImagePtr = std::shared_ptr<Surface>;

class Gfx {
public:
    // `store` gives ISPACE.DLL #5500 100, the graphics and the bitmaps; null in tests that hand
    // in what they need (set_colours_5500, set_alphabet).
    explicit Gfx(const assets::AssetStore* store);
    Gfx(const Gfx&) = delete;
    Gfx& operator=(const Gfx&) = delete;
    ~Gfx();

    // ---- surface and scale
    // The client size in physical pixels and [Port] Scale: recomputes the factor and the
    // logical size. A new size makes a new surface that keeps the old one's overlapping pixels
    // and is black elsewhere [DESIGN]. A zero size (minimised) changes nothing.
    void set_client_size(int client_w, int client_h, int scale_key);
    int factor() const { return factor_; }
    int logical_width() const { return surface_.width(); }
    int logical_height() const { return surface_.height(); }
    int client_width() const { return client_w_; }
    int client_height() const { return client_h_; }
    Surface& surface() { return surface_; }
    const Surface& surface() const { return surface_; }
    // PatBlt BLACKNESS over the whole surface (index 0).
    void blacken();

    // ---- palette
    PalHandle palette() const { return hpal_; }  // DGROUP:0x920e
    PaletteManager& palettes() { return palettes_; }
    const PaletteManager& palettes() const { return palettes_; }
    void palette_create();
    void palette_delete();
    void palette_animate();
    void palette_scheme(int c, int mode);
    void palette_save(int set, int blank);
    void palette_restore(int set);
    int palette_colour(int r, int g, int b);
    void palette_ramp(int c);
    void palette_fade(int mode, int percent);
    void set_colours_5500(const Colours5500& c) { palettes_.set_colours_5500(c); }

    // ---- colours
    int colour_index(std::uint32_t colorref) const {
        return gfx::colour_index(colorref, palettes_.device());
    }

    // ---- images
    ImagePtr load_graphic(int id, int mode, int arg);
    int graphic_height() const { return graphic_height_; }
    fmt::Dib graphic_dib(int id, int d, int a, int b, int last, bool ordinal12);
    ImagePtr load_bitmap(const std::string& name);
    ImagePtr mask(const Surface& src);
    ImagePtr create_image(int w, int h);
    // seg5:0x2080 for a DIB in hand: an index image (dib_indices with the device palette).
    ImagePtr image_from_dib(const fmt::Dib& dib);
    // The font bitmap ordinal 33 draws from (ISPACE.DLL DGROUP:0x1ea2): loaded by
    // palette_create while no palette exists, deleted with the last palette.
    const Surface* alphabet() const { return alphabet_.get(); }
    // The ALPHABET DIB to use instead of the store's (tests) [DESIGN].
    void set_alphabet(const fmt::Dib& dib) { alphabet_dib_ = dib; }

#ifdef _WIN32
    HDC dc() const { return surface_.hdc(); }
    COLORREF dc_colour(COLORREF c) const {
        return 0x10FF0000u | static_cast<COLORREF>(colour_index(c));
    }
    ImagePtr logo_strip();
    void font_text(int font, int align, std::uint32_t colour, int x, int y,
                   const std::string& text);
    void frame(int x, int y, int w, int h, int style);
    void bar(int x, int y, int w, int h, int value);
    bool present(HWND hwnd, bool force);
    bool present_dc(HDC dc, int client_w, int client_h, bool force);
    const Presenter& presenter() const { return presenter_; }
#endif

private:
    fmt::Dib sprite(int id) const;

    const assets::AssetStore* store_ = nullptr;
    PaletteManager palettes_;
    PalHandle hpal_ = 0;
    Surface surface_;
    int factor_ = 1, client_w_ = 0, client_h_ = 0;
    int graphic_height_ = 0;  // ISPACE.DLL DGROUP:0x1c72
    ImagePtr alphabet_;
    std::optional<fmt::Dib> alphabet_dib_;
#ifdef _WIN32
    Presenter presenter_;
#endif
};

}  // namespace is::gfx
