// The LevClass pictures (preview.h). Portable.
#include "dialogs/preview.h"

#include <algorithm>
#include <vector>

#include "gfx/palette.h"

namespace is::dialogs {

void Image::fill(int x0, int y0, int x1, int y1, std::uint32_t c) {
    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    x1 = std::min(x1, w);
    y1 = std::min(y1, h);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) px[static_cast<std::size_t>(y) * w + x] = c;
}

namespace {

// LineTo from (x0, y0) to (x1, y1) for the axis-parallel lines of the frame: the end point is
// left out [REF Win32 SDK LineTo].
void line(Image& im, int x0, int y0, int x1, int y1, std::uint32_t c) {
    const int dx = x1 > x0 ? 1 : x1 < x0 ? -1 : 0, dy = y1 > y0 ? 1 : y1 < y0 ? -1 : 0;
    for (int x = x0, y = y0; x != x1 || y != y1; x += dx, y += dy) im.set(x, y, c);
}

}  // namespace

Image lev_frame(int w, int h, std::uint32_t bg) {
    Image im(w, h, bg);
    const int l = 0, t = 0, r = w, b = h;
    im.fill(l, t, l + 3, b, kLtGray);
    im.fill(l, t, r, t + 3, kLtGray);
    im.fill(l + 2, b - 3, r, b, kDkGray);
    im.fill(r - 3, t + 1, r, b - 1, kDkGray);
    for (int i = 0; i < 3; ++i) {
        im.set(r - 2 - i, t + i, kLtGray);
        im.set(r - 1 - i, t + i, kGray);
        im.set(l + 2 - i, b - 3 + i, kLtGray);
        im.set(l + 3 - i, b - 3 + i, 0);
    }
    line(im, l, b - 1, l, t, 0xFFFFFF);
    line(im, l, t, r - 1, t, 0xFFFFFF);
    line(im, r - 1, t, r - 1, b - 1, 0);
    line(im, r - 1, b - 1, l - 1, b - 1, 0);
    return im;
}

ShipBitmap ship_bitmap(const fmt::Dib& body, int body_id, int a, int b, int body_1a, int flags) {
    ShipBitmap out;
    std::vector<std::uint32_t> table = body.palette;
    const std::size_t n = table.size();
    // No palette handle: the recolour pass rewrites the table only, as at true colour.
    gfx::PaletteManager pm;
    pm.recolour_graphic(0, body_id, table, 0, a, b, 0, false);
    table.resize(std::max(n, table.size()));
    out.image = Image(body.width, body.height);
    for (int y = 0; y < body.height; ++y)
        for (int x = 0; x < body.width; ++x) {
            if (body.bpp <= 8) {
                const std::size_t k = body.index_at(x, y);
                out.image.set(x, y, k < table.size() ? table[k] & 0xFFFFFF : 0);
            } else {
                out.image.set(x, y, body.rgb_at(x, y) & 0xFFFFFF);
            }
        }
    out.w = body.height;
    if (flags != 0) {
        const int h = out.w;
        const int d = (h - static_cast<std::int8_t>(body_1a)) / 2;
        if (flags >= 2) {
            out.image.fill(h - d, 0, h, h, 0);
            flags -= 2;
        }
        if (flags != 0) out.image.fill(0, 0, d, h, 0);
    }
    return out;
}

void paint_slot(Image& client, const ShipBitmap* bm) {
    client.fill(3, 3, 93, 93, 0);
    if (!bm) return;
    const int w = bm->w, x0 = 48 - w, y0 = 48 - w;
    for (int y = 0; y < 2 * w; ++y)
        for (int x = 0; x < 2 * w; ++x) {
            const int sx = x / 2, sy = y / 2;
            if (bm->image.inside(sx, sy)) client.set(x0 + x, y0 + y, bm->image.at(sx, sy));
        }
}

Logo order_logo(const fmt::Dib& g) {
    Logo lg;
    lg.w = g.width;
    lg.h = g.height;
    lg.image = Image(g.width, g.height);
    lg.mask.assign(static_cast<std::size_t>(g.width) * g.height, 0);
    for (int y = 0; y < g.height; ++y)
        for (int x = 0; x < g.width; ++x) {
            const std::uint32_t c = g.rgb_at(x, y) & 0xFFFFFF;
            lg.image.set(x, y, c);
            lg.mask[static_cast<std::size_t>(y) * g.width + x] = c == 0 ? 1 : 0;
        }
    // The first-paint patch.
    for (int y = 29; y < 31; ++y)
        for (int x = 125; x < 127; ++x)
            if (x < lg.w && y < lg.h) lg.mask[static_cast<std::size_t>(y) * lg.w + x] = 0;
    flood_black(lg, 35, 47);
    flood_black(lg, 105, 48);
    return lg;
}

void flood_black(Logo& lg, int x, int y) {
    if (x < 0 || y < 0 || x >= lg.w || y >= lg.h || !lg.white(x, y)) return;
    std::vector<std::pair<int, int>> stack{{x, y}};
    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        stack.pop_back();
        if (cx < 0 || cy < 0 || cx >= lg.w || cy >= lg.h) continue;
        std::uint8_t& m = lg.mask[static_cast<std::size_t>(cy) * lg.w + cx];
        if (!m) continue;
        m = 0;
        stack.push_back({cx + 1, cy});
        stack.push_back({cx - 1, cy});
        stack.push_back({cx, cy + 1});
        stack.push_back({cx, cy - 1});
    }
}

void paint_logo(Image& client, const Logo& lg) {
    for (int y = 0; y < kLogoH && y < lg.h; ++y)
        for (int x = 0; x < kLogoW && x < lg.w; ++x) {
            const int dx = kLogoX + x, dy = kLogoY + y;
            if (!client.inside(dx, dy)) continue;
            const std::uint32_t d = lg.white(x, y) ? client.at(dx, dy) : 0;  // SRCAND
            client.set(dx, dy, d | lg.image.at(x, y));                       // SRCPAINT
        }
}

}  // namespace is::dialogs
