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

}  // namespace is::intro
