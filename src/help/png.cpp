#include "help/png.h"

#include <algorithm>
#include <array>

namespace is::help {

namespace {

const std::array<std::uint32_t, 256>& crc_table() {
    static const std::array<std::uint32_t, 256> t = [] {
        std::array<std::uint32_t, 256> a{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            a[n] = c;
        }
        return a;
    }();
    return t;
}

void be32(std::vector<std::uint8_t>& o, std::uint32_t v) {
    o.push_back(static_cast<std::uint8_t>(v >> 24));
    o.push_back(static_cast<std::uint8_t>(v >> 16));
    o.push_back(static_cast<std::uint8_t>(v >> 8));
    o.push_back(static_cast<std::uint8_t>(v));
}

void chunk(std::vector<std::uint8_t>& o, const char type[4], const std::vector<std::uint8_t>& data) {
    be32(o, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = o.size();
    o.insert(o.end(), type, type + 4);
    o.insert(o.end(), data.begin(), data.end());
    be32(o, crc32(o.data() + start, o.size() - start));
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* p, std::size_t n, std::uint32_t crc) {
    crc = ~crc;
    for (std::size_t i = 0; i < n; ++i) crc = crc_table()[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

std::uint32_t adler32(const std::uint8_t* p, std::size_t n) {
    std::uint32_t a = 1, b = 0;
    for (std::size_t i = 0; i < n; ++i) {
        a = (a + p[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

std::vector<std::uint8_t> png_from_dib(const fmt::Dib& dib) {
    bool indexed = dib.bpp <= 8 && !dib.palette.empty();
    // An index past the palette would make the PNG invalid: such a picture goes out as RGB.
    for (int y = 0; indexed && y < dib.height; ++y)
        for (int x = 0; indexed && x < dib.width; ++x)
            if (dib.index_at(x, y) >= dib.palette.size()) indexed = false;
    const std::size_t w = static_cast<std::size_t>(dib.width), h = static_cast<std::size_t>(dib.height);
    // Raw scanlines, each with filter byte 0.
    std::vector<std::uint8_t> raw;
    raw.reserve(h * (1 + w * (indexed ? 1 : 3)));
    for (std::size_t y = 0; y < h; ++y) {
        raw.push_back(0);
        for (std::size_t x = 0; x < w; ++x) {
            if (indexed) {
                raw.push_back(dib.index_at(static_cast<int>(x), static_cast<int>(y)));
            } else {
                const std::uint32_t c = dib.rgb_at(static_cast<int>(x), static_cast<int>(y));
                raw.push_back(static_cast<std::uint8_t>(c >> 16));
                raw.push_back(static_cast<std::uint8_t>(c >> 8));
                raw.push_back(static_cast<std::uint8_t>(c));
            }
        }
    }
    // zlib stream: header 78 01, stored blocks of at most 65535 bytes, Adler-32.
    std::vector<std::uint8_t> z = {0x78, 0x01};
    std::size_t pos = 0;
    do {
        const std::size_t len = std::min<std::size_t>(65535, raw.size() - pos);
        const bool last = pos + len == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(len));
        z.push_back(static_cast<std::uint8_t>(len >> 8));
        z.push_back(static_cast<std::uint8_t>(~len));
        z.push_back(static_cast<std::uint8_t>(~len >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                 raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
    } while (pos < raw.size());
    be32(z, adler32(raw.data(), raw.size()));

    std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<std::uint8_t> ihdr;
    be32(ihdr, static_cast<std::uint32_t>(w));
    be32(ihdr, static_cast<std::uint32_t>(h));
    ihdr.push_back(8);                // bit depth
    ihdr.push_back(indexed ? 3 : 2);  // colour type: palette or RGB
    ihdr.push_back(0);                // deflate
    ihdr.push_back(0);                // adaptive filtering
    ihdr.push_back(0);                // no interlace
    chunk(out, "IHDR", ihdr);
    if (indexed) {
        std::vector<std::uint8_t> plte;
        for (std::size_t i = 0; i < std::min<std::size_t>(dib.palette.size(), 256); ++i) {
            const std::uint32_t c = dib.palette[i];
            plte.push_back(static_cast<std::uint8_t>(c >> 16));
            plte.push_back(static_cast<std::uint8_t>(c >> 8));
            plte.push_back(static_cast<std::uint8_t>(c));
        }
        chunk(out, "PLTE", plte);
    }
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    return out;
}

}  // namespace is::help
