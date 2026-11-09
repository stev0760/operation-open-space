#include "gfx/gfx.h"

#include <algorithm>
#include <cstring>

#include "assets/asset_store.h"
#include "shell/geometry.h"

namespace is::gfx {

Gfx::Gfx(const assets::AssetStore* store) : store_(store) {
    // ISPACE.DLL loads ALPHABET inside command 0 while DGROUP:0x1ea2 is 0 and deletes it with
    // the last palette [BYTES ISPACE.DLL seg5:0x3712 seg5:0x3b95].
    palettes_.on_alphabet([this](bool load) {
        if (!load) {
            alphabet_.reset();
            return;
        }
        try {
            const fmt::Dib d = alphabet_dib_ ? *alphabet_dib_
                               : store_      ? store_->bitmap("ALPHABET", assets::kGraphicsDll)
                                             : fmt::Dib{};
            if (d.width > 0) {
                if (is_black_and_white(d))
                    alphabet_ = std::make_shared<Surface>(
                        Surface::mono(d.width, d.height, dib_mono_bits(d)));
                else
                    alphabet_ = image_from_dib(d);
            }
        } catch (const std::exception&) {
            alphabet_.reset();  // ordinal 33 then draws nothing [DESIGN]
        }
    });
    if (store_) {
        try {
            const fmt::Palette5500 p = store_->palette5500();
            Colours5500 c{};
            for (std::size_t i = 0; i < c.size(); ++i)
                c[i] = {p.colours[i].r, p.colours[i].g, p.colours[i].b};
            palettes_.set_colours_5500(c);
        } catch (const std::exception&) {
            // without ISPACE.DLL the scheme's entries 11-102 stay black [DESIGN]
        }
    }
}

Gfx::~Gfx() = default;

void Gfx::set_client_size(int client_w, int client_h, int scale_key) {
    if (client_w <= 0 || client_h <= 0) return;
    client_w_ = client_w;
    client_h_ = client_h;
    factor_ = shell::scale_factor(scale_key, client_w, client_h);
    const int w = shell::logical_length(client_w, factor_),
              h = shell::logical_length(client_h, factor_);
    if (w == surface_.width() && h == surface_.height()) return;
    Surface s = Surface::index8(w, h);
    if (!surface_.empty()) {
        surface_.flush();
        const int cw = std::min(w, surface_.width()), ch = std::min(h, surface_.height());
        for (int y = 0; y < ch; ++y)
            std::memcpy(s.pixels() + static_cast<std::size_t>(s.stride()) * y,
                        surface_.pixels() + static_cast<std::size_t>(surface_.stride()) * y,
                        static_cast<std::size_t>(cw));
    }
    surface_ = std::move(s);
#ifdef _WIN32
    presenter_.invalidate();
#endif
}

void Gfx::blacken() {
    if (surface_.empty()) return;
#ifdef _WIN32
    PatBlt(surface_.hdc(), 0, 0, surface_.width(), surface_.height(), BLACKNESS);
#else
    std::memset(surface_.pixels(), 0, static_cast<std::size_t>(surface_.stride()) * surface_.height());
#endif
}

// ---- palette wrappers (INSPACE seg10, ispace.md "Call sites in INSPACE")

void Gfx::palette_create() { hpal_ = palettes_.create(0); }  // seg10:0x6e00 [BYTES seg10:0x6e1c]
void Gfx::palette_delete() {                                   // seg10:0x6e26
    palettes_.destroy(hpal_);
}
void Gfx::palette_animate() { palettes_.animate(hpal_); }  // seg10:0x6e4c
void Gfx::palette_scheme(int c, int mode) {                // seg10:0x6e76 [BYTES seg10:0x6e95]
    palettes_.load_scheme(hpal_, c, mode);
    palettes_.animate(hpal_);
}
void Gfx::palette_save(int set, int blank) {  // seg10:0x6da6 [BYTES seg10:0x6dc9]
    palettes_.save_set(hpal_, set, blank);
    palettes_.animate(hpal_);
}
void Gfx::palette_restore(int set) {  // seg10:0x6dd4 [BYTES seg10:0x6df6]
    palettes_.restore_set(hpal_, set);
    palettes_.animate(hpal_);
}
int Gfx::palette_colour(int r, int g, int b) {  // seg10:0x6d7a
    return palettes_.allocate_colour(hpal_, r, g, b);
}
void Gfx::palette_ramp(int c) { palettes_.allocate_ramp(hpal_, c); }  // seg10:0x6ea0
void Gfx::palette_fade(int mode, int percent) {                         // seg10:0x993a
    palettes_.scale(hpal_, mode, percent);
}

// ---- images

fmt::Dib Gfx::sprite(int id) const {
    if (!store_) throw std::runtime_error("no asset store");
    // seg5:0x1b8e takes 220, 221 and 300 from ISPACE.DLL, the others from IGRAPHIC.DLL
    // [BYTES ISPACE.DLL seg5:0x1c23 seg5:0x1c38].
    const bool own = id == 220 || id == 221 || id == 300;
    return store_->sprite(static_cast<std::uint16_t>(id),
                          own ? assets::kGameDll : assets::kGraphicsDll);
}

ImagePtr Gfx::create_image(int w, int h) {
    return std::make_shared<Surface>(Surface::index8(w, h));
}

ImagePtr Gfx::image_from_dib(const fmt::Dib& dib) {
    const std::vector<std::uint8_t> idx = dib_indices(dib, palettes_.device());
    Surface s = Surface::index8(dib.width, dib.height);
    for (int y = 0; y < dib.height; ++y)
        std::memcpy(s.pixels() + static_cast<std::size_t>(s.stride()) * y,
                    idx.data() + static_cast<std::size_t>(dib.width) * y,
                    static_cast<std::size_t>(dib.width));
    return std::make_shared<Surface>(std::move(s));
}

fmt::Dib Gfx::graphic_dib(int id, int d, int a, int b, int last, bool ordinal12) {
    fmt::Dib dib = sprite(id);
    // seg5:0x1b8e, then the loader's seg6:0x7fc8 while DGROUP:0x1ea8 is set (ordinal 1 at
    // seg5:0x571-0x57c, ordinal 12 at seg7:0x13aa-0x13b8): graphic 76 of the landing corridor's far
    // boxes takes the fixed set (review H4) [BYTES ISPACE.DLL seg5:0x571-0x57c seg7:0x13aa-0x13b8].
    if (dib.bpp <= 8 && palettes_.recolour_graphic(hpal_, id, dib.palette, d, a, b, last, ordinal12))
        fixed_set_remap(dib);
    return dib;
}

// seg10:0x6c02 -> ordinal 1 (ISPACE.DLL seg5:0x402). At 8 bpp mode 3 is mode 0 and the call is
// seg5:0x1b8e(hPal, id, name, 3, 12, 12, 0) for a kind-0 palette (ispace.md ordinal 1). The
// separate paths of ids 280 and 650 and the flags below -100 are not ported [OPEN].
ImagePtr Gfx::load_graphic(int id, int mode, int arg) {
    (void)mode;
    (void)arg;
    const int d = palettes_.kind(hpal_) == 0 ? 3 : 2;
    const fmt::Dib dib = graphic_dib(id, d, 12, 12, 0, false);
    graphic_height_ = dib.height;  // ISPACE.DLL DGROUP:0x1c72 for ordinal 5
    return image_from_dib(dib);
}

// seg10:0x6d38 -> ordinal 3 (ISPACE.DLL seg5:0x612): LoadBitmap from IGRAPHIC.DLL, or from
// ISPACE.DLL for SD_LOGO.
ImagePtr Gfx::load_bitmap(const std::string& name) {
    if (!store_) throw std::runtime_error("no asset store");
    const fmt::Dib dib =
        store_->bitmap(name, name == "SD_LOGO" ? assets::kGameDll : assets::kGraphicsDll);
    if (is_black_and_white(dib))
        return std::make_shared<Surface>(Surface::mono(dib.width, dib.height, dib_mono_bits(dib)));
    return image_from_dib(dib);
}

ImagePtr Gfx::mask(const Surface& src) {  // seg10:0x6ec6 -> ordinal 6
    return std::make_shared<Surface>(Surface::mono(src.width(), src.height(), mask_bits(src)));
}

#ifdef _WIN32
// seg10:0x39a8 -> ordinal 2 with id -1000 and gap 6 -> ISPACE.DLL seg5:0x17c2 (ispace.md
// "Logo strip") [BYTES ISPACE.DLL seg5:0x17f5 seg5:0x1869 seg5:0x18bb seg5:0x18ea seg5:0x1946
// seg5:0x19dd seg5:0x1a3c seg5:0x1a9e seg5:0x1aff].
ImagePtr Gfx::logo_strip() {
    constexpr int g = 6, c = 40;
    ImagePtr ring = load_graphic(220, 3, -1);
    ImagePtr cells = load_graphic(221, 3, -1);
    ImagePtr m = mask(*cells);
    ImagePtr strip = create_image(14 * c + 15 * g, c + 2 * g);
    HDC d = strip->hdc();
    PatBlt(d, 0, 0, strip->width(), strip->height(), BLACKNESS);
    for (int i = 0; i < 14; ++i)
        BitBlt(d, (g + c) * i + g, g, c, c, ring->hdc(), 0, 0, SRCCOPY);
    const auto put = [&](int x, int src_cell) {
        BitBlt(d, x, g, c, c, m->hdc(), c * src_cell, 0, SRCAND);
        BitBlt(d, x, g, c, c, cells->hdc(), c * src_cell, 0, SRCPAINT);
    };
    for (int k = 0; k < 4; ++k) {
        put((g + c) * (k + 1) + g, k);
        // turned 180 degrees: negative extents from (46(9 - k) + 45, 45)
        const int x = (g + c) * (9 - k) + g + c - 1;
        StretchBlt(d, x, g + c - 1, -c, -c, m->hdc(), c * k, 0, c, c, SRCAND);
        StretchBlt(d, x, g + c - 1, -c, -c, cells->hdc(), c * k, 0, c, c, SRCPAINT);
    }
    put(g * 6 + 200, 4);
    for (int k = 0; k < 4; ++k) put((g + c) * (k + 10) + g, k + 5);
    return strip;
}

void Gfx::font_text(int font, int align, std::uint32_t colour, int x, int y,
                    const std::string& text) {
    FontCell cell;
    if (!font_cell_size(font, &cell) || !alphabet_ || surface_.empty()) return;
    draw_font_text(surface_.hdc(), *alphabet_, cell.w, cell.h, align, dc_colour(colour), x, y,
                   text);
}

namespace {
COLORREF resolve(COLORREF c, const void* ctx) { return static_cast<const Gfx*>(ctx)->dc_colour(c); }
}  // namespace

void Gfx::frame(int x, int y, int w, int h, int style) {
    if (!surface_.empty()) draw_frame(surface_.hdc(), x, y, w, h, style, resolve, this);
}

void Gfx::bar(int x, int y, int w, int h, int value) {
    if (!surface_.empty()) draw_bar(surface_.hdc(), x, y, w, h, value, resolve, this);
}

bool Gfx::present_dc(HDC dc, int client_w, int client_h, bool force) {
    return presenter_.present(dc, client_w, client_h, factor_, surface_, palettes_.device(),
                              palettes_.generation(), force);
}

bool Gfx::present(HWND hwnd, bool force) {
    if (!hwnd || surface_.empty() || IsIconic(hwnd)) return false;
    RECT rc;
    GetClientRect(hwnd, &rc);
    // The child controls (the HUD scrollbar) are clipped out: they paint themselves over the
    // presented surface (ADR 0013 decision 5 and Consequences) [DESIGN].
    HDC dc = GetDCEx(hwnd, nullptr, DCX_CACHE | DCX_CLIPCHILDREN);
    if (!dc) return false;
    const bool drew = present_dc(dc, rc.right, rc.bottom, force);
    ReleaseDC(hwnd, dc);
    return drew;
}
#endif

}  // namespace is::gfx
