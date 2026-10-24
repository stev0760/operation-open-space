#include "formats/dib.h"

#include <limits>

#include "formats/bytes.h"

namespace is::fmt {

namespace {
// `icon`: the header height covers the XOR image and the AND mask, so the image is half
// as tall; `bits_end` receives the offset just past the image's bits.
Dib parse_impl(const std::uint8_t* p, std::size_t n, bool icon, std::size_t* bits_end) {
    Reader r(p, n);
    std::size_t base = 0;
    std::size_t bits_off = 0;
    if (n >= 2 && p[0] == 'B' && p[1] == 'M') {
        r.skip(10);
        bits_off = r.u32();
        base = 14;
    }
    Dib d;
    const std::uint32_t hsize = r.u32();
    std::size_t colours = 0;
    bool core = false;
    if (hsize == 12) {  // BITMAPCOREHEADER
        core = true;
        d.width = r.u16();
        const int h = r.i16();
        r.u16();
        d.bpp = r.u16();
        d.height = h;
    } else if (hsize >= 40) {
        d.width = r.i32();
        const std::int32_t h = r.i32();
        r.u16();  // planes
        d.bpp = r.u16();
        if (r.u32() != 0) throw FormatError("compressed DIBs are not supported");
        r.skip(12);  // size image, resolution
        colours = r.u32();
        r.u32();     // important colours
        // -INT32_MIN overflows int; no such height passes the size check below anyway
        if (h == std::numeric_limits<std::int32_t>::min()) throw FormatError("bad DIB size");
        d.bottom_up = h > 0;
        d.height = h > 0 ? h : -h;
    } else {
        throw FormatError("unknown DIB header");
    }
    if (icon) {
        if (d.height % 2 != 0) throw FormatError("odd icon DIB height");
        d.height /= 2;
    }
    if (d.width <= 0 || d.height <= 0 || d.width > 8192 || d.height > 8192)
        throw FormatError("bad DIB size");
    if (d.bpp != 1 && d.bpp != 4 && d.bpp != 8 && d.bpp != 24)
        throw FormatError("unsupported DIB depth");
    if (d.bpp <= 8 && (colours == 0 || colours > (1u << d.bpp))) colours = std::size_t{1} << d.bpp;
    if (d.bpp > 8) colours = 0;

    // biSize is a full dword: compare before adding, so a 32-bit size_t cannot wrap
    if (hsize > n - base) throw FormatError("DIB header past end");
    r.seek(base + hsize);
    for (std::size_t i = 0; i < colours; ++i) {
        const std::uint32_t b = r.u8(), g = r.u8(), rd = r.u8();
        if (!core) r.u8();
        d.palette.push_back(rd << 16 | g << 8 | b);
    }
    if (bits_off) r.seek(bits_off);
    const std::size_t need = d.stride() * static_cast<std::size_t>(d.height);
    if (r.remaining() < need) throw FormatError("DIB bits truncated");
    d.bits.assign(r.ptr(), r.ptr() + need);
    if (bits_end) *bits_end = r.pos() + need;
    return d;
}
}  // namespace

Dib parse_dib(const std::uint8_t* p, std::size_t n) { return parse_impl(p, n, false, nullptr); }

Dib parse_icon_xor(const std::uint8_t* p, std::size_t n, std::size_t& mask_offset) {
    if (n >= 2 && p[0] == 'B' && p[1] == 'M') throw FormatError("icon image with a file header");
    return parse_impl(p, n, true, &mask_offset);
}

std::uint8_t Dib::index_at(int x, int y) const {
    const int row = bottom_up ? height - 1 - y : y;
    const std::uint8_t* line = bits.data() + stride() * static_cast<std::size_t>(row);
    switch (bpp) {
    case 1: return (line[x >> 3] >> (7 - (x & 7))) & 1;
    case 4: return (x & 1) ? (line[x >> 1] & 0x0F) : (line[x >> 1] >> 4);
    case 8: return line[x];
    default: return 0;
    }
}

std::uint32_t Dib::rgb_at(int x, int y) const {
    if (bpp <= 8) {
        const std::uint8_t i = index_at(x, y);
        return i < palette.size() ? palette[i] : 0;
    }
    const int row = bottom_up ? height - 1 - y : y;
    const std::uint8_t* px = bits.data() + stride() * static_cast<std::size_t>(row) + 3 * x;
    return std::uint32_t{px[2]} << 16 | std::uint32_t{px[1]} << 8 | px[0];
}

std::vector<std::uint32_t> to_bgra(const Dib& d, int transparent_index, std::uint32_t background) {
    std::vector<std::uint32_t> out(static_cast<std::size_t>(d.width) * d.height);
    for (int y = 0; y < d.height; ++y)
        for (int x = 0; x < d.width; ++x) {
            std::uint32_t c;
            if (d.bpp <= 8 && d.index_at(x, y) == transparent_index)
                c = background & 0x00FFFFFF;
            else
                c = 0xFF000000 | d.rgb_at(x, y);
            out[static_cast<std::size_t>(y) * d.width + x] = c;
        }
    return out;
}

}  // namespace is::fmt
