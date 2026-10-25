#include "gfx/present.h"

#include <cstring>
#include <stdexcept>

#include "gfx/surface.h"

namespace is::gfx {

void fill_bgra(const std::uint8_t* src, int src_stride, int w, int h, const DeviceColours& pal,
               std::uint32_t* dst, int dst_stride) {
    std::uint32_t lut[256];
    for (int i = 0; i < 256; ++i) lut[i] = bgra(pal[i]);
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* s = src + static_cast<std::size_t>(src_stride) * y;
        std::uint32_t* d = dst + static_cast<std::size_t>(dst_stride) * y;
        for (int x = 0; x < w; ++x) d[x] = lut[s[x]];
    }
}

#ifdef _WIN32
Presenter::~Presenter() {
    if (dc_) {
        SelectObject(dc_, old_);
        DeleteDC(dc_);
    }
    if (bmp_) DeleteObject(bmp_);
}

void Presenter::ensure(int w, int h) {
    if (w == w_ && h == h_ && bmp_) return;
    if (dc_) {
        SelectObject(dc_, old_);
        DeleteDC(dc_);
        dc_ = nullptr;
    }
    if (bmp_) DeleteObject(bmp_);
    bmp_ = nullptr;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    bmp_ = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp_ || !bits) throw std::runtime_error("CreateDIBSection (presenter) failed");
    bits_ = static_cast<std::uint32_t*>(bits);
    dc_ = CreateCompatibleDC(nullptr);
    old_ = SelectObject(dc_, bmp_);
    w_ = w;
    h_ = h;
    valid_ = false;
}

bool Presenter::present(HDC dst, int client_w, int client_h, int factor, const Surface& surface,
                        const DeviceColours& pal, std::uint32_t pal_generation, bool force) {
    if (surface.empty() || factor < 1) return false;
    const int w = surface.width(), h = surface.height();
    ensure(w, h);
    surface.flush();
    const std::size_t n = static_cast<std::size_t>(surface.stride()) * h;
    const bool same = valid_ && generation_ == pal_generation && last_.size() == n &&
                      std::memcmp(last_.data(), surface.pixels(), n) == 0;
    if (!same) {
        fill_bgra(surface.pixels(), surface.stride(), w, h, pal, bits_, w);
        GdiFlush();
        last_.assign(surface.pixels(), surface.pixels() + n);
        generation_ = pal_generation;
        valid_ = true;
    } else if (!force) {
        return false;
    }
    SetStretchBltMode(dst, COLORONCOLOR);
    if (factor == 1)
        BitBlt(dst, 0, 0, w, h, dc_, 0, 0, SRCCOPY);
    else
        StretchBlt(dst, 0, 0, w * factor, h * factor, dc_, 0, 0, w, h, SRCCOPY);
    if (client_w > w * factor) PatBlt(dst, w * factor, 0, client_w - w * factor, client_h, BLACKNESS);
    if (client_h > h * factor) PatBlt(dst, 0, h * factor, w * factor, client_h - h * factor, BLACKNESS);
    return true;
}
#endif

}  // namespace is::gfx
