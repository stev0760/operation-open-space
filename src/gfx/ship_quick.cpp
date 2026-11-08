// ISPACE.DLL ordinal 17, the quick renderer (src/gfx/ship_quick.h). 16-bit arithmetic as the
// original's (ADR 0013 decision 7).
#include "gfx/ship_quick.h"

#include "gfx/ship_sheet.h"

namespace is::gfx {

namespace {

std::int16_t w16(long v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xffff)); }

// cwd; sub ax, dx; sar ax, 1
std::int16_t half16(std::int16_t v) { return static_cast<std::int16_t>((v - (v < 0 ? -1 : 0)) >> 1); }

}  // namespace

std::vector<std::uint8_t> QuickCells::cell(int it) const {
    std::vector<std::uint8_t> c;
    if (!ok() || it < 0 || it > 1) return c;
    c.resize(static_cast<std::size_t>(h) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < h; ++x) c[static_cast<std::size_t>(y) * h + x] = at(it, gap + x, gap + y);
    return c;
}

QuickCells render_quick_cells(const fmt::Dib& body, const QuickColourMap& map, int gap_word, int heading) {
    QuickCells q;
    if (body.bpp != 4 || body.width <= 0 || body.height <= 0 || heading < 0 || heading > 15) return q;
    const std::int16_t h = static_cast<std::int16_t>(body.height);  // word dib+8 [BYTES ISPACE.DLL seg6:0x5d92-0x5e30]
    const std::int16_t W = static_cast<std::int16_t>(body.width);   // word dib+4
    std::int16_t gap = static_cast<std::int16_t>(gap_word);
    if (gap == 99) gap = (h & 1) != 0 ? 5 : 6;  // [BYTES ISPACE.DLL seg6:0x5e1b-0x5e2b]
    q.h = h;
    q.gap = gap;
    q.side = h + 2 * gap;  // (h + gap) 1 + gap square [BYTES ISPACE.DLL seg6:0x56c5-0x56e4]
    if (q.side <= 0) return QuickCells{};
    // The source row stride: trunc((W + 1) / 2) padded to 4 [BYTES ISPACE.DLL seg6:0x57a5-0x57d5].
    const std::int16_t rb = half16(w16(W + 1));
    const std::int16_t sstride = w16(rb + (rb % 4 == 0 ? 0 : 4 - rb % 4));
    const std::int16_t c = half16(h);  // [bp - 0x7a] [BYTES ISPACE.DLL seg6:0x5827-0x5830]
    const std::int16_t S = kSheetSin[static_cast<std::size_t>(heading)];  // DGROUP:0x11a
    const std::int16_t C = kSheetCos[static_cast<std::size_t>(heading)];  // DGROUP:0x13a
    const long nbytes = static_cast<long>(body.bits.size());
    for (int it = 0; it < 2; ++it) {
        std::vector<std::uint8_t> mem(static_cast<std::size_t>(q.side) * q.side, 0);  // GlobalAlloc 0x42
        // srcoff = sext(lo16(trunc(h / 2) (it + 1))) [BYTES ISPACE.DLL seg6:0x580a-0x5823]; x starts at
        // 1 when lo16((it + 1) h) is odd [BYTES ISPACE.DLL seg6:0x5833-0x5865 seg6:0x5900-0x590a].
        long srcoff = w16(static_cast<long>(half16(h)) * (it + 1));
        const bool odd = (w16(static_cast<long>(it + 1) * h) & 1) != 0;
        for (int y = 0; y < h; ++y, srcoff += sstride) {  // memory rows [BYTES ISPACE.DLL seg6:0x5aa6-0x5ac3]
            for (int x = odd ? 1 : 0; x < h; ++x) {
                const long a = (x >> 1) + srcoff;  // fbyte 0 [BYTES ISPACE.DLL seg6:0x590f-0x592e]
                if (a < 0 || a >= nbytes) continue;  // past the bitmap [DESIGN]
                const std::uint8_t b = body.bits[static_cast<std::size_t>(a)];
                const int v = (x & 1) == 0 ? b >> 4 : b & 15;  // [BYTES ISPACE.DLL seg6:0x5932-0x597e]
                if (v == 15) continue;
                // [BYTES ISPACE.DLL seg6:0x5981-0x59dc]
                const std::int16_t cx = w16(c - x), yc = w16(y - c), cy = w16(c - y);
                const std::int16_t X = w16(
                    (w16(w16(w16(static_cast<long>(C) * cx) + w16(static_cast<long>(S) * yc)) + 0x100) >> 9) + c + gap);
                const std::int16_t Y = w16(
                    (w16(w16(w16(static_cast<long>(S) * cx) + w16(static_cast<long>(C) * cy)) + 0x100) >> 9) + c + gap);
                // gap <= X < h + gap and gap <= Y < h + gap, 32-bit signed [BYTES ISPACE.DLL seg6:0x5a20-0x5a71]
                if (X < gap || X >= h + gap || Y < gap || Y >= h + gap) continue;
                // DGROUP:0x1484 + 2v's low byte [BYTES ISPACE.DLL seg6:0x58c4-0x58ee seg6:0x5a99-0x5aa3]
                mem[static_cast<std::size_t>(Y) * q.side + X] = map[static_cast<std::size_t>(v)];
            }
        }
        // The gap fill at headings 2, 6, 10, 14 [BYTES ISPACE.DLL seg6:0x5adb-0x5be2].
        if ((static_cast<std::uint8_t>(heading - 0xfe) & 3) == 0) {
            for (int r = 0; r < h; ++r)
                for (int x = gap; x < h + gap; ++x) {
                    const std::size_t p = static_cast<std::size_t>(r) * q.side + x;
                    if (mem[p] == 0 && mem[p - 1] != 0 && mem[p + 1] != 0) mem[p] = mem[p - 1];
                }
        }
        // Bottom-up memory rows to top-down.
        std::vector<std::uint8_t>& td = q.buf[static_cast<std::size_t>(it)];
        td.assign(mem.size(), 0);
        for (int Y = 0; Y < q.side; ++Y)
            for (int X = 0; X < q.side; ++X)
                td[static_cast<std::size_t>(q.side - 1 - Y) * q.side + X] = mem[static_cast<std::size_t>(Y) * q.side + X];
    }
    return q;
}

QuickBlit quick_blit(int h, int gap, int it, int scale) {
    QuickBlit b;
    const int x = w16(16L * h + 17L * gap), y = w16(static_cast<long>(h + gap) * it + gap);
    b.sx = gap;
    b.sy = gap;
    b.sw = h;
    b.sh = h;
    if (scale == 1) {  // byte [bp+6] = 1: BitBlt [BYTES ISPACE.DLL seg6:0x5ce6]
        b.x = x;
        b.y = y;
        b.w = b.h = h;
        return b;
    }
    // StretchBlt [BYTES ISPACE.DLL seg6:0x5d38]
    const int sh = scale & 0x1f;  // a 386 masks the count [DESIGN]
    b.stretch = true;
    b.x = w16(x << sh) >> 1;
    b.y = w16(y << sh) >> 1;
    b.w = b.h = w16(h << sh) >> 1;
    return b;
}

QuickState& quick_state() {
    static QuickState s;
    return s;
}

QuickCells quick_cells_for(const fmt::Dib& body, int body_id, PaletteManager& pm, PalHandle pal, int b1, int b2,
                           int heading, int gap_word) {
    QuickState& st = quick_state();
    const bool kind0 = pm.kind(pal) == 0;
    // [BYTES ISPACE.DLL seg6:0x55a8-0x55e5]
    if (static_cast<std::int8_t>(b1) != 12) {
        st.id_a = scheme_ramp_id(static_cast<std::int8_t>(b1));
        pm.allocate_ramp(pal, st.id_a);
    }
    if (static_cast<std::int8_t>(b2) != 12) {
        st.id_b = scheme_ramp_id(static_cast<std::int8_t>(b2));
        pm.allocate_ramp(pal, st.id_b);
    }
    // seg5:0x1b8e(hPal, id, lib, kind 0 ? 3 : 1, b1, b2, 0) with DGROUP:0x188 = 1 [BYTES ISPACE.DLL
    // seg6:0x55e8-0x562c]; the DGROUP:0x15c pass inside it [BYTES ISPACE.DLL seg5:0x2a69].
    std::vector<std::uint32_t> table = body.palette;
    pm.recolour_graphic(pal, body_id, table, kind0 ? 3 : 1, b1, b2, 0, true);
    if (table.size() < 16) table.resize(16, 0);
    const QuickColourMap map = quick_colour_map(pm, pal, table, b1, b2, st.id_a, st.id_b, st.dg_1a6);
    return render_quick_cells(body, map, gap_word, heading);
}

}  // namespace is::gfx
