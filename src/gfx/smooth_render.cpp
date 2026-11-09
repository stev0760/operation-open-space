// ISPACE.DLL ordinal 12, the smoothed 2x renderer (smooth_render.h). Portable except smooth_image.
//
// Buffers follow the original's: rows bottom-up as in a DIB ("memory rows"), turned top-down only
// for the returned picture. Every GlobalAlloc(GMEM_ZEROINIT) block is a zero-filled byte vector,
// byte arithmetic wraps as there, and word arithmetic goes through the 16-bit helpers below.
#include "gfx/smooth_render.h"
#include "gfx/guard.h"

#include <algorithm>
#include <cstring>

#include "gfx/ship_sheet.h"

namespace is::gfx {

namespace {

// ---- 16-bit arithmetic as the DLL computes it (the helpers of world/records.h, ADR 0013
// decision 7; kept local because gfx does not depend on world): products whose high word is
// dropped (`imul` then `cwd`), wrapping sums, truncating `idiv`.
constexpr std::int16_t to_i16(std::uint32_t v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v & 0xffffu);
    return w < 0x8000u ? static_cast<std::int16_t>(w) : static_cast<std::int16_t>(static_cast<int>(w) - 0x10000);
}
constexpr std::int16_t lo16(std::int32_t v) { return to_i16(static_cast<std::uint32_t>(v)); }
constexpr std::int16_t mul16(std::int32_t a, std::int32_t b) {
    return to_i16(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
constexpr std::int16_t add16(std::int32_t a, std::int32_t b) {
    return to_i16(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
constexpr std::int16_t sub16(std::int32_t a, std::int32_t b) {
    return to_i16(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
// `idiv` on 16-bit operands, truncating toward zero; d = 0 traps in the original and gives 0
// here [DESIGN].
constexpr std::int16_t idiv16(std::int32_t n, std::int32_t d) {
    return d == 0 ? std::int16_t{0} : to_i16(static_cast<std::uint32_t>(static_cast<std::int32_t>(lo16(n)) / lo16(d)));
}
constexpr std::int16_t imod16(std::int32_t n, std::int32_t d) {
    return d == 0 ? std::int16_t{0} : to_i16(static_cast<std::uint32_t>(static_cast<std::int32_t>(lo16(n)) % lo16(d)));
}
// `cwd; and dx, 3; add ax, dx; sar ax, 2`: a signed word divided by 4, truncating.
constexpr std::int16_t sdiv4(std::int32_t v) { return idiv16(v, 4); }
// `sar` on a word.
constexpr int sar16(std::int32_t v, int n) {
    const std::int16_t w = lo16(v);
    return w >= 0 ? (w >> n) : ~(~static_cast<int>(w) >> n);
}
constexpr std::uint8_t b8(int v) { return static_cast<std::uint8_t>(v & 0xff); }

// DGROUP:0x178[kind], 0x17c[kind], 0x180[kind]: byte offsets into the working PALETTEENTRY array
// [BYTES ISPACE.DLL DGROUP:0x178 DGROUP:0x17c DGROUP:0x180] (bytes e4 02 28 00 54 03 4c 01 98 03
// c0 01). Kinds above 1 read past the table in the original; the port takes kind 1's [DESIGN ADR
// 0016 decision 2: no palette of another kind exists, palette.h create].
struct Bases {
    int a, b, e;
};
Bases bases(int kind) { return kind == 0 ? Bases{0x2e4, 0x354, 0x398} : Bases{0x28, 0x14c, 0x1c0}; }

// seg7:0x4fc8's switch from c to a scheme row, -1 for c outside 0-13 (left unset there)
// [BYTES ISPACE.DLL seg7:0x4f5c-0x4fe7] (verified claim 21).
int row_4fc8(int c) {
    switch (c) {
    case 0: return 8;
    case 1: return 4;
    case 2: return 0;
    case 3: return 15;
    case 4: return 10;
    case 5: return 1;
    case 6: return 3;
    case 7: return 5;
    case 8: return 7;
    case 9: return 17;
    case 10: return 13;
    case 11: return 14;
    case 12: return 6;
    case 13: return 2;
    default: return -1;
    }
}

// Byte x of DGROUP:0x117c (rows of 10 r, g, b triples) or DGROUP:0x1cdc (rows of 5).
template <std::size_t N>
std::uint8_t table_byte(const std::array<std::array<Rgb, N>, kSchemeRows>& t, int x) {
    constexpr int row_bytes = static_cast<int>(N) * 3;
    if (x < 0 || x >= kSchemeRows * row_bytes) {  // past the table: 0 [DESIGN ADR 0016 decision 2]
        static bool once = false;
        guard_todo(once, "ordinal 12 reads past a scheme table (DGROUP:0x117c / 0x1cdc)", "ISPACE.DLL seg7:0x4fc8");
        return 0;
    }
    const Rgb& c = t[static_cast<std::size_t>(x / row_bytes)][static_cast<std::size_t>((x % row_bytes) / 3)];
    const int ch = x % 3;
    return ch == 0 ? c.r : ch == 1 ? c.g : c.b;
}

// One byte of the working PALETTEENTRY array at byte offset `off` (DGROUP:0x1168 + off).
void put_working(PaletteArray& w, int off, std::uint8_t v) {
    if (off < 0 || off >= 256 * 4) return;  // [DESIGN] outside the 256 entries
    PalEntry& e = w[static_cast<std::size_t>(off >> 2)];
    switch (off & 3) {
    case 0: e.r = v; break;
    case 1: e.g = v; break;
    case 2: e.b = v; break;
    default: e.flags = v; break;
    }
}

int brightness_of(const PaletteArray& w, int e) {
    return e >= 0 && e < 256 ? entry_brightness(w[static_cast<std::size_t>(e)].rgb()) : 0;  // [DESIGN] bound
}

}  // namespace

SmoothArgs smooth_args(int b1, int b2) {
    SmoothArgs a;
    a.b1 = b1;
    a.b2 = b2;
    return a;
}

// ---- seg7:0x4ee8

int smooth_ramp(SmoothPalette& p, int c, int count, int first, SmoothGroup& out) {
    static PaletteArray scratch{};
    PaletteArray& w = p.working ? *p.working : scratch;
    const auto& s10 = p.schemes10 ? *p.schemes10 : kSchemes10;
    const auto& s5 = p.schemes5 ? *p.schemes5 : kSchemes5;
    int f = 0;
    if (first >= 10) {  // [BYTES ISPACE.DLL seg7:0x4ef7-0x4f09]
        f = 1;
        first -= 10;
    }
    const Bases bs = bases(p.kind);  // [BYTES ISPACE.DLL seg7:0x4f0d-0x4f2a]
    if (c == -1) return -1;          // [BYTES ISPACE.DLL seg7:0x4f2d-0x4f33]
    int row = -1;
    if (c >= 100)
        f += 2;  // [BYTES ISPACE.DLL seg7:0x4f4e seg7:0x4fe9]
    else
        row = row_4fc8(c);  // [BYTES ISPACE.DLL seg7:0x4f57-0x4fc8]
    SmoothGroup g;
    if (f == 0) {
        // Mode 0 [BYTES ISPACE.DLL seg7:0x4ff6-0x5128].
        const std::int16_t D = sub16(bs.b, bs.a);
        const std::int16_t end = add16(4 * idiv16(mul16(D, first + count), 40), bs.a);  // seg7:0x50b7-0x50ce
        std::int16_t off = add16(4 * idiv16(mul16(D, first), 40), bs.a);               // seg7:0x4ff6-0x500b
        for (int i = 0; end > off; ++i, off = add16(off, 4)) {                           // seg7:0x50d1, seg7:0x50b0
            for (int j = 0; j < 3; ++j) {
                // seg3:0x130(3600 i, sext(lo16(D count))): the 32-bit product 0xe10 i over the
                // sign-extended low word [BYTES ISPACE.DLL seg7:0x5022-0x503b].
                const std::int32_t den = mul16(D, count);
                int t = lo16(den != 0 ? (3600 * i) / den : 0);
                if (t >= 90) t = 89;  // [BYTES ISPACE.DLL seg7:0x503e-0x5043]
                if (row < 0) continue;  // [DESIGN] R unset in the original
                const int x = (idiv16(t, 10) + 10 * row) * 3 + j;  // [BYTES ISPACE.DLL seg7:0x5048-0x505a]
                const int t0 = table_byte(s10, x), t1 = table_byte(s10, x + 3);  // DGROUP:0x117c, 0x117f
                const std::uint8_t v = b8(t0 + idiv16(mul16(t1 - t0, imod16(t, 10)), 10));  // seg7:0x5065-0x5084
                put_working(w, off + j, v);  // [BYTES ISPACE.DLL seg7:0x5086-0x50aa]
            }
        }
        g.lo = sdiv4(add16(idiv16(mul16(D, first), 10), bs.a));                 // [BYTES ISPACE.DLL seg7:0x50de-0x5102]
        g.hi = sub16(sdiv4(add16(idiv16(mul16(D, first + count), 10), bs.a)), 1);  // [BYTES ISPACE.DLL seg7:0x5106-0x5124]
    } else if (f == 1) {
        // Mode 1 [BYTES ISPACE.DLL seg7:0x538f-0x54b7].
        const std::int16_t D = sub16(bs.e, bs.b);
        const std::int16_t end = add16(4 * idiv16(mul16(D, first + count), 20), bs.b);  // seg7:0x544a-0x5461
        std::int16_t off = add16(4 * idiv16(mul16(D, first), 20), bs.b);               // seg7:0x538f-0x53a4
        for (int i = 0; end > off; ++i, off = add16(off, 4)) {
            for (int j = 0; j < 3; ++j) {
                // `imul ax, [bp - 0x14], 0x320`, then a 16-bit idiv by lo16(D count)
                // [BYTES ISPACE.DLL seg7:0x53bb-0x53ce].
                int t = idiv16(mul16(800, i), mul16(D, count));
                if (t >= 40) t = 39;  // [BYTES ISPACE.DLL seg7:0x53d1-0x53d6]
                if (row < 0) continue;  // [DESIGN] R unset in the original
                const int x = (idiv16(t, 10) + 5 * row) * 3 + j;  // [BYTES ISPACE.DLL seg7:0x53db-0x53ed]
                const int t0 = table_byte(s5, x), t1 = table_byte(s5, x + 3);  // DGROUP:0x1cdc, 0x1cdf
                const std::uint8_t v = b8(t0 + idiv16(mul16(t1 - t0, imod16(t, 10)), 10));  // seg7:0x53f8-0x5417
                put_working(w, off + j, v);  // [BYTES ISPACE.DLL seg7:0x5419-0x543d]
            }
        }
        g.lo = sdiv4(add16(idiv16(mul16(D, first), 5), bs.b));                 // [BYTES ISPACE.DLL seg7:0x5471-0x5495]
        g.hi = sub16(sdiv4(add16(idiv16(mul16(D, first + count), 5), bs.b)), 1);  // [BYTES ISPACE.DLL seg7:0x5499-0x54b7]
    } else if (f == 2) {
        g.lo = sdiv4(bs.a);  // [BYTES ISPACE.DLL seg7:0x5708-0x572c]
        g.hi = sub16(sdiv4(bs.b), 1);
    } else {
        g.lo = sdiv4(bs.b);  // [BYTES ISPACE.DLL seg7:0x595f-0x5983]
        g.hi = sub16(sdiv4(bs.e), 1);
    }
    // T1 and T0: one seg6:0x7f52 each with B' = b >> 1 [BYTES ISPACE.DLL seg7:0x5128-0x5245
    // seg7:0x5249-0x536b seg7:0x54bb-0x56f8 seg7:0x572c-0x594f seg7:0x5983-0x5ba6] (claim 25).
    g.t1 = brightness_of(w, g.lo);
    g.t0 = brightness_of(w, g.hi);
    out = g;
    if (f < 2 && p.animate) p.animate();  // seg5:0x4378 [BYTES ISPACE.DLL seg7:0x536f-0x5383 seg7:0x56fc]
    return f;
}

// ---- seg7:0x1541-0x1ffe

SmoothTables smooth_tables(const std::vector<std::uint32_t>& colours, int b1, int b2, SmoothPalette& p) {
    SmoothTables t;
    const auto rgb = [&](int k) {
        return k < static_cast<int>(colours.size()) ? Rgb::of(colours[static_cast<std::size_t>(k)]) : Rgb{};
    };
    t.id.fill(100);    // [BYTES ISPACE.DLL seg7:0x1555-0x156a]
    t.grp[15] = 100;   // [bp - 0x128] [BYTES ISPACE.DLL seg7:0x1ffe]
    const auto call = [&](int layer, int c, int group, int count, int first) {
        SmoothGroup g;
        const int mode = smooth_ramp(p, c, count, first, g);
        t.modes.push_back(mode);
        auto& v = t.groups[static_cast<std::size_t>(layer)];
        if (static_cast<int>(v.size()) <= group) v.resize(static_cast<std::size_t>(group) + 1);
        if (mode >= 0) v[static_cast<std::size_t>(group)] = g;
    };
    for (int layer = 0; layer < 2; ++layer) {
        const std::int8_t b = static_cast<std::int8_t>(layer == 0 ? b1 : b2);  // a byte [BYTES ISPACE.DLL seg7:0x156c seg7:0x1a9e]
        const int fc = layer == 0 ? 0 : 10, n = layer == 0 ? 10 : 5;
        // Brightness and ids [BYTES ISPACE.DLL seg7:0x158b-0x16fb seg7:0x1abd-0x1c2d]: no +5
        // (verified claim 16).
        for (int k = fc; k < fc + n; ++k) {
            t.L[static_cast<std::size_t>(k)] = colour_brightness(rgb(k));
            t.id[static_cast<std::size_t>(k)] = b == 12 ? colour_ramp_id(rgb(k)) : scheme_ramp_id(b);  // seg5:0x66ea, seg5:0x6f58
        }
        if (b != 12) {
            // One group: seg7:0x4ee8(slot, b + 100, 0, n, fc) [BYTES ISPACE.DLL seg7:0x1708-0x174a
            // seg7:0x1c3a-0x1c7d] (verified claims 17 and 18).
            call(layer, b + 100, 0, n, fc);
            for (int k = fc; k < fc + n; ++k) t.grp[static_cast<std::size_t>(k)] = 0;
        } else {
            // Runs [BYTES ISPACE.DLL seg7:0x1a4c-0x1a99 seg7:0x1fa5-0x1ff5] (verified claim 19).
            int count = 0, group = 0;
            for (int i = fc + 1; i <= fc + n; ++i) {
                ++count;
                t.grp[static_cast<std::size_t>(i - 1)] = group;
                if (t.id[static_cast<std::size_t>(i - 1)] == t.id[static_cast<std::size_t>(i)]) continue;
                call(layer, t.id[static_cast<std::size_t>(i - 1)], group, count, i - count);
                ++group;
                count = 0;
            }
        }
    }
    return t;
}

// ---- seg7:0x200d-0x4cb0

SmoothPicture render_smooth(const fmt::Dib& graphic, const SmoothTables& t, const PaletteArray& working,
                            const SmoothArgs& a, int dgroup_1ccc) {
    SmoothPicture out;
    if (graphic.bpp != 4 || graphic.width <= 0 || graphic.height <= 0) return out;
    const int W = graphic.width, H = graphic.height;
    out.h = H;

    // ---- measures [BYTES ISPACE.DLL seg7:0x1437-0x1455 seg7:0x14f2-0x152c]
    int w1 = a.w1, w2 = a.w2;
    if (w1 < 0) {  // `imul word ptr [bp + 0x12]; neg ax` with AX = biHeight; w2's sign is not tested
        w1 = lo16(-mul16(H, w1));
        w2 = lo16(-mul16(H, w2));
    }
    const int N = a.w4 == 0 ? 0 : dgroup_1ccc;  // DGROUP:0x1ccc [BYTES ISPACE.DLL seg7:0x130a-0x1310]
    constexpr int k = 1;  // DGROUP:0x1e9a >= 8 [BYTES ISPACE.DLL seg7:0x3e05-0x3e1b]
    const int S = 2 * H;  // plane stride
    const std::size_t area = static_cast<std::size_t>(S) * static_cast<std::size_t>(S);
    // The source bits as memory rows with seg7:0x12d8's stride: trunc((W + 1) / 2) padded to 4.
    const int half_w = (W + 1) / 2;
    const int ec = half_w + ((half_w % 4) ? 4 - half_w % 4 : 0);
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(ec) * static_cast<std::size_t>(H), 0);
    for (int y = 0; y < H; ++y) {
        const int src_row = graphic.bottom_up ? y : H - 1 - y;
        const std::size_t so = static_cast<std::size_t>(src_row) * graphic.stride();
        for (int i = 0; i < half_w && so + static_cast<std::size_t>(i) < graphic.bits.size(); ++i)
            bits[static_cast<std::size_t>(y) * ec + i] = graphic.bits[so + static_cast<std::size_t>(i)];
    }
    // Columns x < H of a narrower graphic read on into the next row as in the original; past the
    // block the port reads 0 [DESIGN ADR 0016 decision 2].
    const auto nib = [&](int y, int x) -> int {
        const long i = static_cast<long>(y) * ec + (x >> 1);
        const bool in = i >= 0 && i < static_cast<long>(bits.size());
        if (!in) {
            static bool once = false;
            guard_todo(once, "ordinal 12 reads a nibble past the source block", "ISPACE.DLL seg7:0x2049");
        }
        const std::uint8_t by = in ? bits[static_cast<std::size_t>(i)] : 0;
        return (x & 1) == 0 ? by >> 4 : by & 0xf;  // even x the high nibble [BYTES ISPACE.DLL seg7:0x2049-0x2053]
    };

    // Four planes and the class buffer, GlobalAlloc(0x42) of 4 H^2 bytes each [BYTES ISPACE.DLL
    // seg7:0x1481-0x14ee] (verified claim 12).
    std::array<std::vector<std::uint8_t>, 4> P;
    for (auto& pl : P) pl.assign(area, 0);
    std::vector<std::uint8_t> C(area, 0);
    const auto at = [area](std::vector<std::uint8_t>& v, long i) -> std::uint8_t& {
        static std::uint8_t sink;
        if (i < 0 || static_cast<std::size_t>(i) >= area) {
            sink = 0;
            return sink;  // the guards keep every offset inside; the port checks anyway [DESIGN]
        }
        return v[static_cast<std::size_t>(i)];
    };

    // ---- 2x expansion [BYTES ISPACE.DLL seg7:0x2004 seg7:0x200d-0x30f2] (s6-ord12.md 2.5)
    for (int pass = 2; pass <= 3; ++pass) {  // [bp - 0x168] = 2, 3 [BYTES ISPACE.DLL seg7:0x2004 seg7:0x30d6]
        const int layer = pass - 2;
        auto& pl = P[static_cast<std::size_t>(pass)];
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < H; ++x) {  // x < H, not biWidth [BYTES ISPACE.DLL seg7:0x304e-0x3055]
                const int v = nib(y, x);
                const long p = 2L * x + 2L * y * S;
                if (v == 15) {  // both passes [BYTES ISPACE.DLL seg7:0x24c7-0x24d2 seg7:0x2fc8-0x3047]
                    at(C, p) = 100;
                    at(C, p + 1) = 100;
                    at(C, p + S) = 100;
                    at(C, p + S + 1) = 100;
                    continue;
                }
                const int L = t.L[static_cast<std::size_t>(v)];
                int half = b8(sar16(L, 1));                    // [BYTES ISPACE.DLL seg7:0x2056-0x205e]
                if (a.w3 == 1) half = b8(half + sar16(L, 2));  // [BYTES ISPACE.DLL seg7:0x2062-0x206d]
                const int q = b8(sar16(L, 2));                 // [bp - 0x90] = 2 [BYTES ISPACE.DLL seg7:0x20a0-0x20a6]
                // Skip: the other layer's pass or a zero half (Omission 29) [BYTES ISPACE.DLL
                // seg7:0x2445-0x244c seg7:0x2472-0x247e seg7:0x2900-0x2907 seg7:0x292c-0x2937
                // seg7:0x295d-0x2964].
                if ((v >= 10) != (layer == 1) || half == 0) continue;
                // "Other" neighbours (Omission 30): layer 0 sees 10-15, layer 1 sees 0-9 and 15;
                // 15 clears the spread but does not count [BYTES ISPACE.DLL seg7:0x20b2-0x2271
                // seg7:0x2271-0x243f seg7:0x24ac-0x295d].
                const auto other = [layer](int n) { return layer == 1 ? (n < 10 || n == 15) : n >= 10; };
                int n0 = 0, n1 = 0, n2 = 0, n3 = 0;  // corners p, p + 1, p + S, p + S + 1
                bool su = true, sd = true, sl = true, sr = true;
                if (y > 0) {
                    const int u = nib(y - 1, x);
                    if (other(u)) {
                        su = false;
                        if (u != 15) ++n0, ++n1;
                    }
                }
                if (y < H - 1) {
                    const int d = nib(y + 1, x);
                    if (other(d)) {
                        sd = false;
                        if (d != 15) ++n2, ++n3;
                    }
                }
                if (x > 0) {
                    const int l = nib(y, x - 1);
                    if (other(l)) {
                        sl = false;
                        if (l != 15) ++n0, ++n2;
                    }
                }
                if (x < H - 1) {
                    const int r = nib(y, x + 1);
                    if (other(r)) {
                        sr = false;
                        if (r != 15) ++n1, ++n3;
                    }
                }
                // Class (grp - 1) 2 + pass, a byte [BYTES ISPACE.DLL seg7:0x29a1-0x2a39].
                const std::uint8_t cv = b8((t.grp[static_cast<std::size_t>(v)] - 1) * 2 + pass);
                at(C, p) = cv;
                at(C, p + 1) = cv;
                at(C, p + S) = cv;
                at(C, p + S + 1) = cv;
                const auto add = [&](long i, int val) { at(pl, i) = b8(at(pl, i) + val); };
                add(p, half);  // [BYTES ISPACE.DLL seg7:0x2a40-0x2b25]
                add(p + 1, half);
                add(p + S, half);
                add(p + S + 1, half);
                // Corners: `shl al, cl`, a byte [BYTES ISPACE.DLL seg7:0x2b28-0x2c34] (claim 30).
                if (n0) add(p, b8(q << (n0 - 1)));
                if (n1) add(p + 1, b8(q << (n1 - 1)));
                if (n2) add(p + S, b8(q << (n2 - 1)));
                if (n3) add(p + S + 1, b8(q << (n3 - 1)));
                // Spreads [BYTES ISPACE.DLL seg7:0x2cdb-0x2f4b] (claim 31).
                if (y > 0 && su) add(p - S, q), add(p - S + 1, q);
                if (y < H - 1 && sd) add(p + 2L * S, q), add(p + 2L * S + 1, q);
                if (x > 0 && sl) add(p - 1, q), add(p + S - 1, q);
                if (x < H - 1 && sr) add(p + 2, q), add(p + S + 2, q);
            }
        }
    }

    // ---- blur [BYTES ISPACE.DLL seg7:0x30f4-0x3516] (claims 32-33): P[k + 2] into P[k], with the
    // shifts [bp - 0x17e] = 1 and [bp - 0x180] = 4 [BYTES ISPACE.DLL seg7:0x1316-0x131c].
    for (int kk = 0; kk < 2; ++kk) {
        const auto& src = P[static_cast<std::size_t>(kk + 2)];
        auto& dst = P[static_cast<std::size_t>(kk)];
        for (int Y = 1; Y < S - 1; ++Y)
            for (int X = 1; X < S - 1; ++X) {
                const std::size_t i = static_cast<std::size_t>(Y) * S + X;
                const int s = src[i];
                if (s == 0) continue;
                int acc = dst[i] + (s >> 1) + (s >> 2);
                for (const std::size_t n : {i - S - 1, i - S + 1, i + S - 1, i + S + 1}) {
                    const int vv = src[n];
                    acc += (vv > 20 ? vv : s) >> 4;  // `cmp n, 0x14; jbe` [BYTES ISPACE.DLL seg7:0x349c]
                }
                dst[i] = b8(acc);  // byte adds
            }
    }

    // ---- staircase [BYTES ISPACE.DLL seg7:0x3518-0x3e03] (claim 35; the body from the Ghidra C,
    // seg7:0x3539-0x3d91, render_ship_sheet's staircase on the blurred planes).
    const auto cls = [&](long i) -> int {
        const std::uint8_t v = at(C, i);
        return v == 100 ? 2 : (v & 1);
    };
    for (int y = 2; y < H - 2; ++y) {
        for (int x = 2; x < H - 2; ++x) {
            const long p = 2L * x + 2L * y * S;
            const int c0 = cls(p), cl = cls(p - 2), cu = cls(p - 2L * S), cr = cls(p + 2), cd = cls(p + 2L * S);
            const auto fix = [&](long dst, long from, int layer) {
                at(P[1], dst) = 0;
                at(P[0], dst) = at(P[1], dst);
                if (c0 == 2 || layer != 2) {
                    auto& pl = P[static_cast<std::size_t>(layer)];
                    at(pl, dst) = at(pl, from);
                    at(C, dst) = b8(at(C, from) + 101);
                    ++out.fixes;
                }
            };
            if (cl != c0) {
                if (cu == cl && (c0 != 0 || (at(C, p + 2L * S - 2) & 1) == cl)) fix(p, p - S, cl);
                if (cd == cl && (c0 != 0 || (at(C, p - 2L * S - 2) & 1) == cl)) fix(p + S, p + 2L * S, cl);
            }
            if (cr != c0) {
                if (cu == cr && (c0 != 0 || (at(C, p + 2L * S + 2) & 1) == cr)) fix(p + 1, p - S + 1, cr);
                if (cd == cr && (c0 != 0 || (at(C, p - 2L * S + 2) & 1) == cr)) fix(p + S + 1, p + 2L * S + 1, cr);
            }
        }
    }

    // ---- output at depth >= 8 [BYTES ISPACE.DLL seg7:0x3e05-0x3eba seg7:0x450f-0x46b5
    // seg7:0x4b20-0x4cb0] (claims 37-41): seg5:0x23a4's zero-filled 2kH-square DIB, memory rows.
    out.side = lo16(2 * mul16(k, H));  // [BYTES ISPACE.DLL seg7:0x4e4d-0x4e55]
    out.px.assign(static_cast<std::size_t>(out.side) * static_cast<std::size_t>(out.side), 0);
    for (std::size_t e = 0; e < area; ++e) {
        const int a0 = P[0][e], a1 = P[1][e];
        int c = C[e];
        int layer, v;
        if (a0 > 2 && a1 < a0) {  // `jbe`, `jb` on bytes [BYTES ISPACE.DLL seg7:0x4bb0-0x4be4]
            layer = 0;
            v = a0;
            if (c > 100) c -= 101;
            if (c % 2 == 1) c = 0;  // `idiv 2; dec dx; jne` [BYTES ISPACE.DLL seg7:0x4be7-0x4c00]
        } else if (a1 > 2) {  // `cmp [plane1], 2; ja` [BYTES ISPACE.DLL seg7:0x45bc-0x45c2]
            layer = 1;
            v = a1;
            if (c > 100) c -= 101;
            if ((c & 1) == 0) c = 1;  // [BYTES ISPACE.DLL seg7:0x45c5-0x45d8]
        } else {
            continue;  // unwritten: 0
        }
        const auto& tab = t.groups[static_cast<std::size_t>(layer)];
        const std::size_t gi = static_cast<std::size_t>(c >> 1);
        // An index past the stored groups reads a zeroed group (the original reads the
        // neighbouring DGROUP words [OPEN]) [DESIGN ADR 0016 decision 2] (review M7).
        SmoothGroup gr{};
        if (gi < tab.size()) {
            gr = tab[gi];
        } else {
            static bool once = false;
            guard_todo(once, "ordinal 12 reads a class group past the stored groups", "ISPACE.DLL seg7:0x45fe");
        }
        int pix = 0xf5;  // [BYTES ISPACE.DLL seg7:0x4689]
        if (2 * v > gr.t0) {  // signed `jg` [BYTES ISPACE.DLL seg7:0x4c29-0x4c38 seg7:0x45fe-0x460b]
            // (v - T0)(hi - lo), a 16 x 16 -> 32 `imul`, over sext(T1 - T0) by seg3:0x130
            // [BYTES ISPACE.DLL seg7:0x4c3b-0x4c6b seg7:0x460d-0x463d] (claim 41).
            const std::int32_t num = static_cast<std::int32_t>(sub16(v, gr.t0)) * sub16(gr.hi, gr.lo);
            const std::int32_t den = sub16(gr.t1, gr.t0);
            int pv = sub16(gr.hi, lo16(den != 0 ? num / den : 0));
            if (pv < gr.lo)  // lo first, then hi (Omission 41) [BYTES ISPACE.DLL seg7:0x4c6b-0x4c78 seg7:0x463d-0x4647]
                pv = gr.lo;
            else if (pv > gr.hi)
                pv = gr.hi;
            pix = nearest_ramp_entry(working, v, gr.lo, gr.hi, pv) & 0xff;  // seg5:0x6b72, `sub ah, ah`
        }
        const std::size_t Y = e / static_cast<std::size_t>(S), X = e % static_cast<std::size_t>(S);
        out.px[(static_cast<std::size_t>(out.side) - 1 - Y) * static_cast<std::size_t>(out.side) + X] = b8(pix);
    }

    // ---- the bitmap's measures [BYTES ISPACE.DLL seg7:0x4d98-0x4db8 seg7:0x4e35-0x4e44]
    out.bm_w = mul16(mul16(k, w1), N + 1);
    out.bm_h = mul16(k, w2);
    out.dst_w = mul16(k, w1);
    out.dst_h = mul16(k, w2);
    return out;
}

// ---- ordinal 12

SmoothPicture smooth_loaded(const fmt::Dib& loaded, PaletteManager& pm, PalHandle pal, const SmoothArgs& a,
                            int dgroup_1ccc) {
    PaletteArray* w = pm.working_entries(pal);
    if (!w) return {};
    const int kind = pm.kind(pal);
    // DGROUP:0x1e80[3 slot], [3 slot + 1] = -1 before the load [BYTES ISPACE.DLL seg7:0x1328-0x134a];
    // the load writes b1, b2 there instead when kind != 0 (its last argument) [BYTES ISPACE.DLL
    // seg5:0x26a2 (shell.md "Graphic colour tables")].
    if (kind == 0) {
        pm.set_blend_from(pal, 0, -1);
        pm.set_blend_from(pal, 1, -1);
    }
    int b1 = a.b1, b2 = a.b2;
    if (pm.dg_1eaa() != 0) b1 = 100;  // `mov byte ptr [bp + 8], 0x64` [BYTES ISPACE.DLL seg7:0x1541-0x1547]
    if (pm.dg_1eac() != 0) b2 = 100;  // [BYTES ISPACE.DLL seg7:0x154b-0x1551]
    SmoothPalette sp;
    sp.working = w;
    sp.kind = kind;
    sp.schemes10 = &pm.schemes10();
    sp.schemes5 = &pm.schemes5();
    sp.animate = [&pm, pal] { pm.animate(pal); };
    // The colours: DGROUP:0x1178 + 4, the load's 16 recoloured entries [BYTES ISPACE.DLL
    // seg7:0x13c0-0x13ce]; a shorter table reads black [DESIGN].
    std::vector<std::uint32_t> colours = loaded.palette;
    if (colours.size() < 16) colours.resize(16, 0);
    const SmoothTables t = smooth_tables(colours, b1, b2, sp);
    SmoothArgs args = a;
    args.b1 = b1;
    args.b2 = b2;
    SmoothPicture pic = render_smooth(loaded, t, *w, args, dgroup_1ccc);
    // seg5:0x3a68 deletes the 16-entry palette DGROUP:0x1eae, which the port does not build
    // (palette.h recolour_graphic) [DESIGN]; then seg5:0x4378(hPal, 0, DGROUP:0x1a32).
    pm.animate(pal);  // [BYTES ISPACE.DLL seg7:0x4ec4-0x4ed2]
    return pic;
}

SmoothPicture smooth_for(const fmt::Dib& graphic, int id, PaletteManager& pm, PalHandle pal, const SmoothArgs& a,
                         int dgroup_1ccc) {
    if ((a.b1 & 0xff) == 0xff || (a.b2 & 0xff) == 0xff) return {};  // [BYTES ISPACE.DLL seg6:0xda seg6:0xe0]
    if (!pm.valid(pal)) return {};  // [DESIGN] the original goes on with slot 3
    fmt::Dib d = graphic;
    // seg5:0x1b8e(hPal, id, lib, 0, b1, b2, DGROUP:0x1e92[slot] != 0) with DGROUP:0x188 = 1
    // [BYTES ISPACE.DLL seg7:0x134c-0x139a] (verified claim 7).
    if (pm.recolour_graphic(pal, id, d.palette, 0, a.b1, a.b2, pm.kind(pal) != 0 ? 1 : 0, true))
        fixed_set_remap(d);  // seg6:0x7fc8 [BYTES ISPACE.DLL seg7:0x13aa-0x13b8]
    return smooth_loaded(d, pm, pal, a, dgroup_1ccc);
}

SmoothPicture smooth_for(Gfx& gfx, int id, const SmoothArgs& a, int dgroup_1ccc) {
    // seg6:0xc4 returns 0 before the load for a byte 0xff [BYTES ISPACE.DLL seg6:0xda seg6:0xe0]:
    // the sprite is not read then.
    if ((a.b1 & 0xff) == 0xff || (a.b2 & 0xff) == 0xff) return {};
    PaletteManager& pm = gfx.palettes();
    const PalHandle pal = gfx.palette();
    if (!pm.valid(pal)) return {};  // [DESIGN] the original goes on with slot 3
    // Gfx::graphic_dib is seg5:0x1b8e: the sprite by its module rule and recolour_graphic, here
    // (hPal, id, lib, 0, b1, b2, DGROUP:0x1e92[slot] != 0) with DGROUP:0x188 = 1 [BYTES ISPACE.DLL
    // seg7:0x134c-0x139a] (verified claim 7).
    const fmt::Dib d = gfx.graphic_dib(id, 0, a.b1, a.b2, pm.kind(pal) != 0 ? 1 : 0, true);
    return smooth_loaded(d, pm, pal, a, dgroup_1ccc);
}

#ifdef _WIN32
ImagePtr smooth_image(Gfx& gfx, const SmoothPicture& p) {
    if (!p.ok() || p.bm_w <= 0 || p.bm_h <= 0) return nullptr;
    // seg5:0x2080: the 8-bpp DIB with DIB_PAL_COLORS as a device bitmap [BYTES ISPACE.DLL
    // seg7:0x4d75-0x4d84]: an index image of the indices.
    ImagePtr src = gfx.create_image(p.side, p.side);
    src->flush();
    for (int y = 0; y < p.side; ++y)
        std::memcpy(src->pixels() + static_cast<std::size_t>(src->stride()) * static_cast<std::size_t>(y),
                    p.px.data() + static_cast<std::size_t>(p.side) * static_cast<std::size_t>(y),
                    static_cast<std::size_t>(p.side));
    ImagePtr dst = gfx.create_image(p.bm_w, p.bm_h);  // CreateCompatibleBitmap [BYTES ISPACE.DLL seg7:0x4d98-0x4db8]
    SetStretchBltMode(dst->hdc(), COLORONCOLOR);       // [BYTES ISPACE.DLL seg7:0x4e22-0x4e28]
    StretchBlt(dst->hdc(), 0, 0, p.dst_w, p.dst_h, src->hdc(), 0, 0, p.side, p.side,
               SRCCOPY);  // 0x00CC0020 [BYTES ISPACE.DLL seg7:0x4e2d-0x4e5e]
    dst->flush();
    return dst;
}

ImagePtr smooth_bitmap(Gfx& gfx, int id, const SmoothArgs& a, int dgroup_1ccc) {
    return smooth_image(gfx, smooth_for(gfx, id, a, dgroup_1ccc));
}
#endif

}  // namespace is::gfx
