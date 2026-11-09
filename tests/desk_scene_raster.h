// A host rasteriser of ordinal 32's op list (src/desk/scene.h) for tests/test_desk_scene.cpp and
// tests/test_desk_scene_win32.cpp: PatBlt with GDI's rule for negative extents, solid, stock and
// pattern brushes, BLACKNESS. Pen lines and texts are not rasterised: the pixels they may touch
// are marked uncertain and the comparisons leave them out [DESIGN].
//
// GDI's negative-extent rule: a PatBlt of width w < 0 covers x + w + 1 ... x, and of height
// h < 0 covers y + h + 1 ... y [REF Wine dlls/gdi32 get_bounding_rect] (the same rectangles the
// Win16 GDI of the original filled).
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "desk/scene.h"

namespace scene_raster {

struct Image {
    int w = 0, h = 0;
    std::vector<std::uint32_t> px;        // 0x00RRGGBB
    std::vector<std::uint8_t> uncertain;  // 1 where a line or a text may have drawn
    Image() = default;
    Image(int W, int H) : w(W), h(H), px(static_cast<std::size_t>(W) * static_cast<std::size_t>(H), 0),
                          uncertain(static_cast<std::size_t>(W) * static_cast<std::size_t>(H), 0) {}
    std::uint32_t at(int x, int y) const { return px[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)]; }
    bool unsure(int x, int y) const { return uncertain[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] != 0; }
};

// A COLORREF (0x02BBGGRR or 0x00BBGGRR) as 0x00RRGGBB.
inline std::uint32_t rgb_of(std::uint32_t c) {
    return ((c & 0xffu) << 16) | (c & 0xff00u) | ((c >> 16) & 0xffu);
}

// A pattern bitmap: pw x ph pixels 0x00RRGGBB, row-major top-down.
struct PatternBits {
    int w = 0, h = 0;
    std::vector<std::uint32_t> px;
};

// The rectangle a PatBlt covers, normalised as GDI does.
struct Box {
    int l = 0, t = 0, r = 0, b = 0;  // right and bottom exclusive
};
inline Box patblt_box(int x, int y, int w, int h) {
    Box o{x, y, x + w, y + h};
    if (o.l > o.r) {
        const int t = o.l;
        o.l = o.r + 1;
        o.r = t + 1;
    }
    if (o.t > o.b) {
        const int t = o.t;
        o.t = o.b + 1;
        o.b = t + 1;
    }
    return o;
}

class Raster {
public:
    // `pattern` gives the bits of a pattern brush (empty: the brush paints nothing and its
    // pixels are marked uncertain).
    Raster(int W, int H, std::function<PatternBits(is::desk::Pattern)> pattern = {})
        : img_(W, H), pattern_(std::move(pattern)) {}

    void run(const is::desk::Op& o) {
        using is::desk::Op;
        switch (o.kind) {
        case Op::kBrush: kind_ = kSolid, colour_ = rgb_of(o.colour); break;
        case Op::kPattern:
            bits_ = pattern_ ? pattern_(o.pattern) : PatternBits{};
            kind_ = bits_.w > 0 && bits_.h > 0 ? kBits : kUnknown;
            break;
        case Op::kStock: kind_ = kSolid, colour_ = o.stock == 4 ? 0u : 0xc0c0c0u; break;
        case Op::kPatBlt: patblt(o.x, o.y, o.w, o.h, o.rop); break;
        case Op::kPen: pen_w_ = std::max(o.width, 1); break;
        case Op::kMoveTo: cx_ = o.x, cy_ = o.y; break;
        case Op::kLineTo:
            mark_line(cx_, cy_, o.x, o.y, pen_w_);
            cx_ = o.x, cy_ = o.y;
            break;
        case Op::kText: mark_text(o); break;
        case Op::kFrame:
            for (const Op& f : is::desk::frame_ops(o)) run(f);
            break;
        }
    }
    void run(const std::vector<is::desk::Op>& ops) {
        for (const auto& o : ops) run(o);
    }
    const Image& image() const { return img_; }

private:
    enum Kind { kSolid, kBits, kUnknown };
    void put(int x, int y, std::uint32_t c, bool unsure) {
        if (x < 0 || y < 0 || x >= img_.w || y >= img_.h) return;
        const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(img_.w) + static_cast<std::size_t>(x);
        img_.px[i] = c;
        img_.uncertain[i] = unsure ? 1 : 0;
    }
    void mark(int x, int y) {
        if (x < 0 || y < 0 || x >= img_.w || y >= img_.h) return;
        img_.uncertain[static_cast<std::size_t>(y) * static_cast<std::size_t>(img_.w) + static_cast<std::size_t>(x)] = 1;
    }
    void patblt(int x, int y, int w, int h, std::uint32_t rop) {
        const Box b = patblt_box(x, y, w, h);
        for (int yy = std::max(b.t, 0); yy < std::min(b.b, img_.h); ++yy)
            for (int xx = std::max(b.l, 0); xx < std::min(b.r, img_.w); ++xx) {
                if (rop == is::desk::kBlackness) put(xx, yy, 0, false);
                else if (kind_ == kSolid) put(xx, yy, colour_, false);
                else if (kind_ == kBits) {
                    const int px = ((xx % bits_.w) + bits_.w) % bits_.w, py = ((yy % bits_.h) + bits_.h) % bits_.h;
                    put(xx, yy, bits_.px[static_cast<std::size_t>(py * bits_.w + px)], false);
                } else {
                    put(xx, yy, 0, true);
                }
            }
    }
    // Every pixel within the pen's width of the segment's box [DESIGN: not rasterised].
    void mark_line(int x0, int y0, int x1, int y1, int w) {
        const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
        const int r = w / 2 + 1;
        for (int i = 0; i <= steps; ++i) {
            const int x = steps ? x0 + (x1 - x0) * i / steps : x0, y = steps ? y0 + (y1 - y0) * i / steps : y0;
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) mark(x + dx, y + dy);
        }
    }
    // A generous box around TextOut's ink: the font's cell height below y (TA_TOP) and the
    // string's length in cells of max(width, height), centred for TA_CENTER [DESIGN].
    void mark_text(const is::desk::Op& o) {
        const int n = static_cast<int>(o.text.size());
        if (n == 0) return;
        const int cell = std::max(std::max(o.font_w, o.font_h), 1);
        const int len = n * cell + 4, hgt = std::max(o.font_h, 1) * 3 / 2 + 4;
        const int l = (o.align & 6) == 6 ? o.x - len / 2 - 2 : o.x - 2;
        for (int y = o.y - 2; y < o.y + hgt; ++y)
            for (int x = l; x < l + len + 4; ++x) mark(x, y);
    }

    Image img_;
    std::function<PatternBits(is::desk::Pattern)> pattern_;
    Kind kind_ = kSolid;
    std::uint32_t colour_ = 0xffffffu;  // a new DC's WHITE_BRUSH
    PatternBits bits_;
    int pen_w_ = 1, cx_ = 0, cy_ = 0;
};

// The op list of a render on a black W x H image.
inline Image rasterise(const is::desk::Render& r, int W, int H,
                       std::function<PatternBits(is::desk::Pattern)> pattern = {}) {
    Raster ras(W, H, std::move(pattern));
    ras.run(r.ops);
    return ras.image();
}

}  // namespace scene_raster
