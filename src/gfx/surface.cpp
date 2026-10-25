#include "gfx/surface.h"

#include <cstring>
#include <stdexcept>
#include <utility>

namespace is::gfx {

namespace {
int index_stride(int w) { return (w + 3) & ~3; }
}  // namespace

const DeviceColours& index_table() {
    static const DeviceColours table = [] {
        DeviceColours t = default_device_colours();
        for (int i = 10; i < 246; ++i)
            t[i] = {static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(255 - i), 0x80};
        return t;
    }();
    return table;
}

Surface Surface::index8(int w, int h) {
    if (w <= 0 || h <= 0 || w > 32767 || h > 32767) throw std::invalid_argument("surface size");
    Surface s;
    s.format_ = Format::kIndex8;
    s.w_ = w;
    s.h_ = h;
    s.stride_ = index_stride(w);
#ifdef _WIN32
    struct {
        BITMAPINFOHEADER h;
        RGBQUAD c[256];
    } bi{};
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biWidth = w;
    bi.h.biHeight = -h;  // top-down
    bi.h.biPlanes = 1;
    bi.h.biBitCount = 8;
    bi.h.biCompression = BI_RGB;
    bi.h.biClrUsed = 256;
    const DeviceColours& colours = index_table();
    for (int i = 0; i < 256; ++i) bi.c[i] = RGBQUAD{colours[i].b, colours[i].g, colours[i].r, 0};
    void* bits = nullptr;
    s.bmp_ = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits,
                              nullptr, 0);
    if (!s.bmp_ || !bits) throw std::runtime_error("CreateDIBSection failed");
    s.bits_ = static_cast<std::uint8_t*>(bits);
    std::memset(s.bits_, 0, static_cast<std::size_t>(s.stride_) * h);
    s.dc_ = CreateCompatibleDC(nullptr);
    if (!s.dc_) throw std::runtime_error("CreateCompatibleDC failed");
    s.old_ = SelectObject(s.dc_, s.bmp_);
    SetTextColor(s.dc_, 0x10FF0000);  // DIBINDEX(0)
    SetBkColor(s.dc_, 0x10FF00FF);    // DIBINDEX(255)
#else
    s.store_.assign(static_cast<std::size_t>(s.stride_) * h, 0);
    s.bits_ = s.store_.data();
#endif
    return s;
}

Surface Surface::mono(int w, int h, std::vector<std::uint8_t> bits) {
    if (w <= 0 || h <= 0 || w > 32767 || h > 32767) throw std::invalid_argument("bitmap size");
    Surface s;
    s.format_ = Format::kMono1;
    s.w_ = w;
    s.h_ = h;
    s.stride_ = mono_stride(w);
    bits.resize(static_cast<std::size_t>(s.stride_) * h, 0);
    s.store_ = std::move(bits);
    s.bits_ = s.store_.data();
#ifdef _WIN32
    s.bmp_ = CreateBitmap(w, h, 1, 1, s.store_.data());
    if (!s.bmp_) throw std::runtime_error("CreateBitmap failed");
    s.dc_ = CreateCompatibleDC(nullptr);
    if (!s.dc_) throw std::runtime_error("CreateCompatibleDC failed");
    s.old_ = SelectObject(s.dc_, s.bmp_);
#endif
    return s;
}

Surface::Surface(Surface&& o) noexcept { *this = std::move(o); }

Surface& Surface::operator=(Surface&& o) noexcept {
    if (this == &o) return *this;
    release();
    format_ = o.format_;
    w_ = o.w_;
    h_ = o.h_;
    stride_ = o.stride_;
    const bool own = o.bits_ == o.store_.data();
    store_ = std::move(o.store_);
    bits_ = own ? store_.data() : o.bits_;
#ifdef _WIN32
    dc_ = o.dc_;
    bmp_ = o.bmp_;
    old_ = o.old_;
    o.dc_ = nullptr;
    o.bmp_ = nullptr;
    o.old_ = nullptr;
#endif
    o.bits_ = nullptr;
    o.w_ = o.h_ = o.stride_ = 0;
    return *this;
}

Surface::~Surface() { release(); }

void Surface::release() {
#ifdef _WIN32
    if (dc_) {
        SelectObject(dc_, old_);
        DeleteDC(dc_);
    }
    if (bmp_) DeleteObject(bmp_);
    dc_ = nullptr;
    bmp_ = nullptr;
    old_ = nullptr;
#endif
    bits_ = nullptr;
    store_.clear();
}

std::uint8_t Surface::at(int x, int y) const {
    if (!bits_ || x < 0 || y < 0 || x >= w_ || y >= h_) return 0;
    const std::uint8_t* row = bits_ + static_cast<std::size_t>(stride_) * y;
    if (format_ == Format::kMono1) return (row[x >> 3] >> (7 - (x & 7))) & 1;
    return row[x];
}

void Surface::flush() const {
#ifdef _WIN32
    GdiFlush();
#endif
}

std::vector<std::uint8_t> dib_indices(const fmt::Dib& dib, const DeviceColours& device) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(dib.width) * dib.height);
    std::vector<std::uint8_t> map;
    if (dib.bpp < 8) {
        map.resize(std::size_t{1} << dib.bpp, 0);
        for (std::size_t i = 0; i < map.size(); ++i)
            map[i] = static_cast<std::uint8_t>(
                nearest_index(device, Rgb::of(i < dib.palette.size() ? dib.palette[i] : 0)));
    }
    for (int y = 0; y < dib.height; ++y)
        for (int x = 0; x < dib.width; ++x) {
            std::uint8_t v;
            if (dib.bpp == 8)
                v = dib.index_at(x, y);
            else if (dib.bpp < 8)
                v = map[dib.index_at(x, y)];
            else
                v = static_cast<std::uint8_t>(nearest_index(device, Rgb::of(dib.rgb_at(x, y))));
            out[static_cast<std::size_t>(y) * dib.width + x] = v;
        }
    return out;
}

bool is_black_and_white(const fmt::Dib& dib) {
    if (dib.bpp != 1 || dib.palette.size() != 2) return false;
    const std::uint32_t a = dib.palette[0] & 0xffffff, b = dib.palette[1] & 0xffffff;
    return (a == 0 && b == 0xffffff) || (a == 0xffffff && b == 0);
}

std::vector<std::uint8_t> dib_mono_bits(const fmt::Dib& dib) {
    const int stride = mono_stride(dib.width);
    std::vector<std::uint8_t> out(static_cast<std::size_t>(stride) * dib.height, 0);
    const auto white = [](std::uint32_t c) {
        const long r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
        const long to_black = r * r + g * g + b * b;
        const long to_white = (255 - r) * (255 - r) + (255 - g) * (255 - g) + (255 - b) * (255 - b);
        return to_white < to_black;  // ties go to black, entry 0 of the mono table
    };
    for (int y = 0; y < dib.height; ++y)
        for (int x = 0; x < dib.width; ++x)
            if (white(dib.rgb_at(x, y)))
                out[static_cast<std::size_t>(y) * stride + (x >> 3)] |=
                    static_cast<std::uint8_t>(0x80 >> (x & 7));
    return out;
}

std::vector<std::uint8_t> mask_bits(const Surface& src) {
    src.flush();
    const int stride = mono_stride(src.width());
    std::vector<std::uint8_t> out(static_cast<std::size_t>(stride) * src.height(), 0);
    for (int y = 0; y < src.height(); ++y)
        for (int x = 0; x < src.width(); ++x)
            if (src.at(x, y) == 0)
                out[static_cast<std::size_t>(y) * stride + (x >> 3)] |=
                    static_cast<std::uint8_t>(0x80 >> (x & 7));
    return out;
}

}  // namespace is::gfx
