// seg12:0x7256, the clipped line (line.h) (M4 s7 TETHER).
#include "world/line.h"

namespace is::world {

namespace {
constexpr std::int32_t sext(std::int16_t v) { return v; }
}  // namespace

std::optional<LineEnds> clip_line(std::int16_t w, std::int16_t h, std::int16_t x0,
                                  std::int16_t y0, std::int16_t x1, std::int16_t y1) {
    // P0 = P1 = (-1, -1) [BYTES seg12:0x7264-0x7270].
    std::int16_t p[4] = {-1, -1, -1, -1};
    const bool inside = x0 >= 0 && x0 <= w && x1 >= 0 && x1 <= w && y0 >= 0 && y0 <= h &&
                        y1 >= 0 && y1 <= h;  // [BYTES seg12:0x7273-0x72a5]
    if (!inside) {
        // Step 2: degenerate ends nudged [BYTES seg12:0x72a8-0x72bb].
        if (x1 == x0) x1 = add16(x1, 1);
        if (y1 == y0) y1 = add16(y1, 1);
        // Step 3: both ends beyond one edge [BYTES seg12:0x72be-0x72f7].
        if (y0 < 0 && y1 < 0) return std::nullopt;
        if (y0 > h && y1 > h) return std::nullopt;
        if (x0 < 0 && x1 < 0) return std::nullopt;
        if (x0 > w && x1 > w) return std::nullopt;
        // Step 4: the slope and the intercept [BYTES seg12:0x72fa-0x7352].
        std::int32_t m = rtl_ldiv(rtl_lshl(sext(sub16(y1, y0)), 7), sext(sub16(x1, x0)));
        if (add16(sub16(x0, x1), 1) == 0) x1 = x0;  // [BYTES seg12:0x731e-0x732a]
        std::int32_t b = sub32(sext(y0), rtl_lsar(rtl_lmul(m, sext(x0)), 7));
        // Step 5: the top edge [BYTES seg12:0x7355-0x73c1].
        if (y0 < 0 || y1 < 0) {
            const int i = y0 < 0 ? 0 : 1;
            if (m == 0) return std::nullopt;  // [BYTES seg12:0x7379]
            const std::int32_t xi = neg32(rtl_ldiv(rtl_lshl(b, 7), m));
            if (xi > 0 && xi < sext(w)) {  // `jl`/`je` reject, then `jae` [BYTES seg12:0x73a0-0x73b6]
                p[2 * i] = lo16(xi);
                p[2 * i + 1] = 0;
            }
        }
        // Step 6: the bottom edge [BYTES seg12:0x73c6-0x7435].
        if (y0 > h || y1 > h) {
            const int i = y0 > h ? 0 : 1;
            if (m == 0) return std::nullopt;  // [BYTES seg12:0x73ea]
            const std::int32_t xi = rtl_ldiv(rtl_lshl(sub32(sext(h), b), 7), m);
            if (xi > 0 && xi < sext(w)) {  // [BYTES seg12:0x740d-0x7427]
                p[2 * i] = lo16(xi);
                p[2 * i + 1] = h;
            }
        }
        // Step 7: the inverse slope, with x1 as step 4 left it [BYTES seg12:0x7438-0x7490].
        m = rtl_ldiv(rtl_lshl(sext(sub16(x1, x0)), 7), sext(sub16(y1, y0)));
        if (add16(sub16(y0, y1), 1) == 0) y1 = y0;  // [BYTES seg12:0x745c-0x7468]
        b = sub32(sext(x0), rtl_lsar(rtl_lmul(m, sext(y0)), 7));
        // Step 8: the left edge [BYTES seg12:0x7493-0x7501] ...
        if (x0 < 0 || x1 < 0) {
            const int i = x0 < 0 ? 0 : 1;
            if (m == 0) return std::nullopt;  // [BYTES seg12:0x74b7]
            const std::int32_t yi = neg32(rtl_ldiv(rtl_lshl(b, 7), m));
            if (yi > 0 && yi < sext(h)) {  // [BYTES seg12:0x74de-0x74f4]
                p[2 * i] = 0;
                p[2 * i + 1] = lo16(yi);
            }
        }
        // ... and the right edge [BYTES seg12:0x7504-0x7573].
        if (x0 > w || x1 > w) {
            const int i = x0 > w ? 0 : 1;
            if (m == 0) return std::nullopt;  // [BYTES seg12:0x7528]
            const std::int32_t yi = rtl_ldiv(rtl_lshl(sub32(sext(w), b), 7), m);
            if (yi > 0 && yi < sext(h)) {  // [BYTES seg12:0x754b-0x7565]
                p[2 * i] = w;
                p[2 * i + 1] = lo16(yi);
            }
        }
    }
    // Step 9: an end still (-1, -1) takes its own point [BYTES seg12:0x7576-0x75a3].
    if (p[0] == -1 && p[1] == -1) {
        p[0] = x0;
        p[1] = y0;
    }
    if (p[2] == -1 && p[3] == -1) {
        p[2] = x1;
        p[3] = y1;
    }
    return LineEnds{p[0], p[1], p[2], p[3]};
}

void draw_clipped_line(Blitter& out, DcId dc, std::int16_t w, std::int16_t h, std::int16_t x0,
                       std::int16_t y0, std::int16_t x1, std::int16_t y1) {
    const std::optional<LineEnds> e = clip_line(w, h, x0, y0, x1, y1);
    if (!e) return;
    out.move_to(dc, e->x0, e->y0);  // GDI.20 MoveTo [BYTES seg12:0x75af]
    out.line_to(dc, e->x1, e->y1);  // GDI.19 LineTo [BYTES seg12:0x75bd]
}

}  // namespace is::world
