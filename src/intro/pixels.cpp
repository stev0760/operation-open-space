#include "intro/pixels.h"

namespace is::intro {

void double_copy(IndexView dst, int dx, int dy, ConstIndexView src, int sx, int sy, int sw, int sh) {
    if (!dst.pixels || !src.pixels) return;
    for (int y = 0; y < 2 * sh; ++y) {
        const int ty = dy + y, fy = sy + y / 2;
        if (ty < 0 || ty >= dst.h || fy < 0 || fy >= src.h) continue;
        std::uint8_t* out = dst.pixels + static_cast<std::size_t>(dst.stride) * ty;
        const std::uint8_t* in = src.pixels + static_cast<std::size_t>(src.stride) * fy;
        for (int x = 0; x < 2 * sw; ++x) {
            const int tx = dx + x, fx = sx + x / 2;
            if (tx < 0 || tx >= dst.w || fx < 0 || fx >= src.w) continue;
            out[tx] = in[fx];
        }
    }
}

int ordinal12_entry(int k) {
    if (k >= 0 && k <= 9) return 185 + 3 * k;
    if (k >= 10 && k <= 14) return 213 + 4 * (k - 10);
    return 0;
}

std::vector<std::uint8_t> ordinal12_plain(const fmt::Dib& dib) {
    const int w = dib.width * 2, h = dib.height * 2;
    std::vector<std::uint8_t> out(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    if (dib.bpp > 8) return out;
    for (int y = 0; y < dib.height; ++y)
        for (int x = 0; x < dib.width; ++x) {
            const auto e = static_cast<std::uint8_t>(ordinal12_entry(dib.index_at(x, y)));
            for (int j = 0; j < 2; ++j)
                for (int i = 0; i < 2; ++i)
                    out[static_cast<std::size_t>(2 * y + j) * w + 2 * x + i] = e;
        }
    return out;
}

}  // namespace is::intro
