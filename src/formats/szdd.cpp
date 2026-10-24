#include "formats/szdd.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace is::fmt {

namespace {
constexpr std::uint8_t kMagic[8] = {'S', 'Z', 'D', 'D', 0x88, 0xF0, 0x27, 0x33};
constexpr std::uint8_t kModeLzss = 0x41;
constexpr std::size_t kWindow = 4096;
}  // namespace

bool is_szdd(const std::uint8_t* p, std::size_t n) {
    return n >= sizeof kMagic && std::memcmp(p, kMagic, sizeof kMagic) == 0;
}

SzddHeader parse_szdd_header(const std::uint8_t* p, std::size_t n) {
    if (n < kSzddHeaderSize || !is_szdd(p, n)) throw FormatError("not an SZDD file");
    Reader r(p, n, 8);
    SzddHeader h;
    h.mode = r.u8();
    h.last_char = static_cast<char>(r.u8());
    h.size = r.u32();
    return h;
}

SzddFile szdd_expand(const std::uint8_t* p, std::size_t n) {
    const SzddHeader h = parse_szdd_header(p, n);
    if (h.mode != kModeLzss) throw FormatError("unsupported SZDD mode " + std::to_string(h.mode));
    const std::uint8_t* src = p + kSzddHeaderSize;
    const std::size_t len = n - kSzddHeaderSize;
    const std::size_t size = h.size;

    SzddFile f;
    f.last_char = h.last_char;
    // A flag byte and eight 2-byte matches give at most 144 bytes from 17: never reserve
    // more than the stream can produce, whatever the header claims.
    f.data.reserve(std::min(size, len * 9));
    std::array<std::uint8_t, kWindow> win;
    win.fill(0x20);
    std::size_t pos = kWindow - 16;
    auto put = [&](std::uint8_t c) {
        f.data.push_back(c);
        win[pos] = c;
        pos = (pos + 1) & (kWindow - 1);
    };

    std::size_t i = 0;
    while (i < len && f.data.size() < size) {
        const std::uint8_t flags = src[i++];
        for (unsigned bit = 0; bit < 8 && i < len && f.data.size() < size; ++bit) {
            if (flags & (1u << bit)) {
                put(src[i++]);
                continue;
            }
            if (i + 1 >= len) {
                i = len;  // half a match: the stream is short
                break;
            }
            const std::uint8_t lo = src[i], hi = src[i + 1];
            i += 2;
            std::size_t off = lo | std::size_t{static_cast<std::uint8_t>(hi & 0xF0)} << 4;
            const unsigned count = (hi & 0x0Fu) + 3;
            for (unsigned k = 0; k < count && f.data.size() < size; ++k)
                put(win[off++ & (kWindow - 1)]);
        }
    }
    if (f.data.size() != size)
        throw FormatError("SZDD stream ended after " + std::to_string(f.data.size()) + " of " +
                          std::to_string(size) + " bytes");
    return f;
}

}  // namespace is::fmt
