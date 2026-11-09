// The ship sheet (ship_sheet.h): ISPACE.DLL seg6:0x11c, DGROUP:0x1e9a >= 8 branch. Portable.
//
// Coordinates follow the original's buffers: rows bottom-up as in a DIB ("memory rows"),
// converted to top-down only when the row DIBs are blitted into the sheet. Every buffer the
// original keeps in a GlobalAlloc(GMEM_ZEROINIT) block is a zero-filled byte vector here, and
// byte arithmetic wraps as it does there.
#include "gfx/ship_sheet.h"

#include <algorithm>
#include <cstdlib>

namespace is::gfx {

const std::array<std::int16_t, 16> kSheetSin = {0,   196,  362,  473,  512,  473,  362,  196,
                                                0,   -196, -362, -473, -512, -473, -362, -196};
const std::array<std::int16_t, 16> kSheetCos = {-512, -473, -362, -196, 0,   196,  362,  473,
                                                512,  473,  362,  196,  0,   -196, -362, -473};

namespace {

std::int16_t w16(long v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xffff)); }
std::uint8_t b8(int v) { return static_cast<std::uint8_t>(v & 0xff); }

int brightness(long r1, long g1, long b1) {
    const long q = (r1 * r1 + b1 * b1 + g1 * g1) / 3;  // seg3:0xfe products, seg3:0x130 divide
    const long m = (std::labs(r1) + std::labs(g1) + std::labs(b1)) / 3;
    return newton_root(w16(m), q);
}

// The intensity and colour buffers of one rendered row: 2h x 2h bytes each, memory rows.
struct Buffers {
    int side = 0;  // 2h
    // Intensity buffers DGROUP:0xec8[(r N + d) 2 + layer], colour buffers DGROUP:0x1048[r N + d].
    std::vector<std::vector<std::uint8_t>> ink, col;
};

}  // namespace

int scheme_ramp_id(int scheme) {
    switch (scheme) {  // [BYTES ISPACE.DLL seg5:0x6f58]
    case 0: case 0x10: return 2;
    case 1: return 5;
    case 2: return 0xd;
    case 3: return 6;
    case 4: return 1;
    case 5: return 7;
    case 6: return 0xc;
    case 7: return 8;
    case 8: case 9: return 0;
    case 10: case 11: return 4;
    case 0xd: return 10;
    case 0xe: return 0xb;
    case 0xf: return 3;
    case 0x11: return 9;
    default: return -1;
    }
}

// seg5:0x66ea transliterated from the listing; the labels name the branch targets. Every
// division is a 16-bit idiv (or a byte div) of non-negative values, so C++ '/' matches.
int colour_ramp_id(Rgb c) {
    // [bp+6], [bp+8], [bp+0xa] = the PALETTEENTRY's r, g, b, each raised to 1
    // [BYTES ISPACE.DLL seg5:0x66f9-0x672d seg6:0x126e-0x12c6]
    const int r = c.r ? c.r : 1, g = c.g ? c.g : 1, b = c.b ? c.b : 1;
    const auto d = [](int num, int den) { return num * 100 / den; };
    const auto spread = [&] {  // seg2:0x802 is |x| [BYTES ISPACE.DLL seg2:0x80c-0x81a]
        const int rb = std::abs(r - b), rg = std::abs(r - g);
        return rb >= rg ? rb : rg;
    };
    // seg5:0x6757: a grey; 3 when green leads with little blue, else 2
    if (d(spread(), r) < 10) return (g > r && d(b, g) < 0x5d) ? 3 : 2;
    // seg5:0x678a-0x67eb
    if (d(std::abs(r - g), r) < 8 && d(std::abs(r - b), r) < 0x1e && (b > r || b > g))
        return (g > r && d(b, g) < 0x5d) ? 3 : 2;  // seg5:0x67d9
    // seg5:0x67ed-0x682c
    if (d(std::abs(g - b), g) < 6 && d(std::abs(r - g), r) < 0x15) return 2;
    // seg5:0x682f-0x688a: a red, else seg5:0x6a4e
    const bool reddish = (r - g > 10 && r - b > 0x32) || (r > 0xd2 && g <= r && r - b > 0x1e);
    if (reddish && (b - g < 0x46 || r / b > 0x96)) {
        if (r / g > 10 && r / b > 10) return 1;  // seg5:0x688d
        if (d(r, b) > 0x28a || r - b > 0xb4) {  // seg5:0x68ab -> seg5:0x68cd
            const int gb = d(g, b);
            if (gb > 0x58 && gb < 0x6f && (r / g < 4 || (r < 0x96 && r / g < 8))) return 0xb;
            const int gr = d(g, r);  // seg5:0x6900
            if (gr > 0x55) return r > 0xd2 ? 1 : 5;
            if (gr > 0x4a) return 5;  // seg5:0x691d
            if (gr > 0x41 && r < 0xaa) return 5;
            if (gr > 0x3c) return 1;  // seg5:0x6939
            if (gr <= 0x23) return 1;
            if (d(b, g) <= 0xc) return 1;
            return r - g < 0x6e ? 9 : 1;
        }
        const int gb = d(g, b);  // seg5:0x697f
        if (gb > 0x55 && gb < 0x73 && (r / g < 4 || (r < 0x96 && r / g < 10))) return 0xb;
        const int rg = r - g;  // seg5:0x69b2
        if (rg > 0x1e && rg < 0x50 && d(b, g) < 0x55) {
            const int gr = d(g, r);
            if (gr > 0x2d && gr < 0x55) return 9;
        }
        const int bg = d(b, g);  // seg5:0x69e9
        if (bg > 0x37 && bg < 100) return 0xd;
        if (bg > 0x19 && bg < 100 && d(g, r) > 0x3c) return 1;  // seg5:0x6a06
        return d(r, b < g ? g : b) > 200 ? 1 : 9;  // seg5:0x6a2e
    }
    // seg5:0x6a4e-0x6acc
    const int rb = d(r, b);
    if (r * 0x5e / g > rb && r - g > 0x14) {
        if (r < 0xb4 && b < 200 && g < 0x96) return rb < 0x55 ? 8 : 7;
        if (rb < 0x55) return 8;  // seg5:0x6aa0
        if (rb >= 0x6e) return 7;
        return d(g, b) <= 0x32 ? 7 : 8;
    }
    // seg5:0x6ace-0x6b64
    if (d(spread(), r) < 0x23 && b < g && b < r) return 3;
    if (b > g) return 0;
    if (d(b, g) >= 0x1e) return 10;
    return d(r, g) > 0x50 ? 5 : 4;
}

int newton_root(long x, long q) {
    for (int i = 0; i < 3; ++i) {  // [BYTES ISPACE.DLL seg6:0x7f64-0x7fbd]
        if (x < 2) x = 2;
        x += (q - x * x) / (2 * x);
    }
    return w16(x);
}

int colour_brightness(Rgb c) {
    return brightness(c.r - (c.r >> 2), c.g, (c.b >> 1) + (c.b >> 3));
}

int entry_brightness(Rgb c) { return brightness(c.r - (c.r >> 2), c.g, c.b >> 1); }

int nearest_ramp_entry(const PaletteArray& pal, int avg, int lo, int hi, int p) {
    const auto lum = [&](int e) { return e >= 0 && e < 256 ? entry_brightness(pal[e].rgb()) : 0; };
    const int lp = lum(p);
    int best = std::abs(lp - avg);  // seg2:0x802
    if (best < 3) return p;          // [BYTES ISPACE.DLL seg5:0x6ca7]
    const int step = lp > avg ? 1 : -1;  // [BYTES ISPACE.DLL seg5:0x6cb8]
    for (int e = p + step; step > 0 ? e <= hi : e >= lo; e += step) {
        const int d = std::abs(lum(e) - avg);
        if (d > best) return e - step;  // [BYTES ISPACE.DLL seg5:0x6df9 seg5:0x6f41]
        if (d < 3) return e;            // [BYTES ISPACE.DLL seg5:0x6ccb seg5:0x6e12]
        best = d;
    }
    return step > 0 ? hi : lo;  // [BYTES ISPACE.DLL seg5:0x6ce2 seg5:0x6e27]
}

ShipSheet render_ship_sheet(const fmt::Dib& body, const std::vector<std::uint32_t>& colours,
                            const SheetArgs& args, const SheetPalette& palette) {
    ShipSheet out;
    if (body.bpp != 4 || body.width <= 0 || body.height <= 0 || colours.size() < 16) return out;
    static const PaletteArray kBlack{};
    const PaletteArray& pal = palette.working ? *palette.working : kBlack;

    // ---- measures [BYTES ISPACE.DLL seg6:0x25e-0x322]
    const int W = body.width, h = body.height;
    int g = args.gap;
    if (g == 99) g = (h & 1) ? 5 : 6;
    const int F = W / h;
    const int R = (args.mode == 1 && F > 1) ? 2 : F;
    int N;
    if (g == -100) {
        N = 1;
        g = 0;
    } else {
        N = args.four != 0 ? 4 : 16;
    }
    const int s = args.scale ? 1 : 0;
    out.cell = h;
    out.gap = g;
    out.scale = s;
    out.frames = F;
    out.rows = R;
    out.dirs = N;
    if (F <= 0) return out;

    // The source bits as stored: rows bottom-up, the stride seg6:0x11c computes,
    // ((W + 1) / 2) padded to 4 [BYTES ISPACE.DLL seg6:0x18a0-0x18c4].
    std::vector<std::uint8_t> bits;
    const int half = (W + 1) / 2;
    const int ec = half + ((half % 4) ? 4 - half % 4 : 0);
    bits.assign(static_cast<std::size_t>(ec) * h, 0);
    for (int y = 0; y < h; ++y) {
        const int src_row = body.bottom_up ? y : h - 1 - y;
        const std::size_t so = static_cast<std::size_t>(src_row) * body.stride();
        for (int i = 0; i < half && so + i < body.bits.size(); ++i)
            bits[static_cast<std::size_t>(y) * ec + i] = body.bits[so + i];
    }
    const auto byte_at = [&](long i) -> std::uint8_t {
        return i >= 0 && i < static_cast<long>(bits.size()) ? bits[i] : 0;
    };

    // ---- colour classes, DGROUP:0x1e9a >= 8 [BYTES ISPACE.DLL seg6:0xd44-0x1860]
    // bright[c]: the brightness of colour c (+5 for 0-9); group[c]: its ramp group.
    std::array<int, 16> bright{};
    std::array<int, 17> group{};  // local_14a[c + 1]; [16] = 100 for colour 15
    group[16] = 100;
    struct Group {
        int lo = 0, hi = 0, t_lo = 0, t_hi = 0;  // DGROUP:0x1108/0x111c/0x1130/0x1144 or 0x1ca4...
    };
    std::array<std::vector<Group>, 2> groups;
    const auto rgb = [&](int c) { return Rgb::of(colours[c]); };
    for (int layer = 0; layer < 2; ++layer) {
        const int scheme = layer == 0 ? args.scheme_a : args.scheme_b;
        const int first = layer == 0 ? 0 : 10, count = layer == 0 ? 10 : 5;
        const bool keep = static_cast<std::int8_t>(scheme) == 12;
        std::vector<int> ids;  // local_ca / local_b6
        int current = 0;       // local_1ba
        const int fixed = keep ? -1 : scheme_ramp_id(scheme);  // local_ca[0] from the set-up
        for (int k = 0; k < count; ++k) {
            const int c = first + k;
            bool fresh = true;
            int id = fixed;
            if (keep) {
                id = palette.colour_ramp ? palette.colour_ramp(rgb(c)) : -1;  // seg5:0x66ea
                for (std::size_t j = 0; j < ids.size(); ++j)
                    if (ids[j] == id) {
                        current = static_cast<int>(j);
                        fresh = false;
                        break;
                    }
            }
            if (fresh && (keep || k == 0)) {
                Group gr;
                if (palette.kind0) {
                    // seg5:0x61b2, the record /4, the brightness of both ends; layer 1 runs
                    // seg6:0x7f52 a second time on each [BYTES ISPACE.DLL seg6:0xe12-0xfcc
                    // seg6:0x1374 seg6:0x1529 seg6:0x153d seg6:0x16d3 seg6:0x16e7].
                    const RampRange rr = palette.ramp ? palette.ramp(id) : RampRange{};
                    gr.lo = rr.lo;
                    gr.hi = rr.hi;
                    const auto ends = [&](int e) {
                        const Rgb c1 = e >= 0 && e < 256 ? pal[e].rgb() : Rgb{};
                        int v = entry_brightness(c1);
                        if (layer == 1) {
                            const long r1 = c1.r - (c1.r >> 2), g1 = c1.g, bb = c1.b >> 1;
                            v = newton_root(v, (r1 * r1 + bb * bb + g1 * g1) / 3);
                        }
                        return v;
                    };
                    gr.t_lo = ends(gr.lo);
                    gr.t_hi = ends(gr.hi);
                }
                current = static_cast<int>(groups[layer].size());
                groups[layer].push_back(gr);
                ids.push_back(id);
            }
            bright[c] = colour_brightness(rgb(c)) + (layer == 0 ? 5 : 0);
            group[c + 1] = current;
        }
    }

    // ---- buffers [BYTES ISPACE.DLL seg6:0x334-0x3c7]
    const int S = 2 * h;  // local_72
    Buffers B;
    B.side = S;
    const std::size_t area = static_cast<std::size_t>(S) * S;
    B.ink.assign(static_cast<std::size_t>(R) * N * 2, std::vector<std::uint8_t>(area, 0));
    B.col.assign(static_cast<std::size_t>(R) * N, std::vector<std::uint8_t>(area, 0));
    const auto at = [&](std::vector<std::uint8_t>& v, long i) -> std::uint8_t& {
        static std::uint8_t sink;
        if (i < 0 || i >= static_cast<long>(v.size())) {
            sink = 0;
            return sink;  // the original would touch the neighbouring block [DESIGN]
        }
        return v[static_cast<std::size_t>(i)];
    };

    // ---- 2x expansion with layer edges [BYTES ISPACE.DLL seg6:0x18cb-0x2a55]
    for (int r = 0; r < R; ++r) {
        int f = r;
        if (R != F) f = r > 0 ? F - 1 : 0;  // [BYTES ISPACE.DLL seg6:0x18cb-0x18d8]
        const bool odd = ((f * h) & 1) != 0;
        const long base = (f * h) >> 1;
        auto& colbuf = B.col[static_cast<std::size_t>(r) * N];
        for (int L = 0; L < 2; ++L) {
            auto& ink = B.ink[static_cast<std::size_t>(r) * N * 2 + L];
            for (int y = 0; y < h; ++y) {
                const long row = static_cast<long>(y) * ec;
                for (int x = 0; x < h; ++x) {
                    if (x == 0 && odd) x = 1;  // [BYTES ISPACE.DLL seg6:0x18e6-0x1902]
                    const long bi = (x >> 1) + base + row;
                    const std::uint8_t by = byte_at(bi);
                    const bool hi_nib = (x & 1) == 0;
                    const int nib = hi_nib ? by >> 4 : by & 0xf;
                    // The neighbours' nibbles: up and down in the same half of the byte one
                    // stride away, left and right in the other half.
                    const int up = hi_nib ? byte_at(bi - ec) >> 4 : byte_at(bi - ec) & 0xf;
                    const int dn = hi_nib ? byte_at(bi + ec) >> 4 : byte_at(bi + ec) & 0xf;
                    const int lf = hi_nib ? byte_at(bi - 1) & 0xf : by >> 4;
                    const int rt = hi_nib ? by & 0xf : byte_at(bi + 1) >> 4;
                    bool skip = true, marker = false;
                    int n196 = 0, n195 = 0, n194 = 0, n193 = 0;  // corner counts
                    bool sp_up = true, sp_dn = true, sp_lf = true, sp_rt = true;
                    std::int8_t half_l = 0, quarter_l = 0;
                    int grp = 0;
                    if (nib == 15) {
                        marker = true;
                    } else {
                        const int L1 = bright[nib];
                        half_l = static_cast<std::int8_t>(b8(L1 >> 1));
                        quarter_l = static_cast<std::int8_t>(b8(L1 >> 2));
                        grp = group[nib + 1];
                        const int pl = nib >= 10 ? 1 : 0;
                        if (pl == L) {
                            // "Other": the other layer or transparent. Layer 0 sees >= 10.
                            const auto other = [&](int v) { return L == 1 ? (v < 10 || v == 15) : v > 9; };
                            if (y != 0 && other(up)) {
                                sp_up = false;
                                if (up != 15) ++n196, ++n195;
                            }
                            if (y < h - 1 && other(dn)) {
                                sp_dn = false;
                                if (dn != 15) ++n194, ++n193;
                            }
                            if (x != 0 && other(lf)) {
                                sp_lf = false;
                                if (lf != 15) ++n196, ++n194;
                            }
                            if (x < h - 1 && other(rt)) {
                                sp_rt = false;
                                if (rt != 15) ++n195, ++n193;
                            }
                            skip = half_l == 0;
                        }
                    }
                    const long p = 2L * x + 2L * y * S;  // local_98
                    if (!skip) {
                        const std::uint8_t cv = b8(grp * 2 + L);
                        at(colbuf, p) = cv;
                        at(colbuf, p + 1) = cv;
                        at(colbuf, p + S) = cv;
                        at(colbuf, p + S + 1) = cv;
                        const auto add = [&](long i, int v) { at(ink, i) = b8(at(ink, i) + v); };
                        add(p, half_l);
                        add(p + 1, half_l);
                        add(p + S, half_l);
                        add(p + S + 1, half_l);
                        if (n196) add(p, quarter_l << (n196 - 1));
                        if (n195) add(p + 1, quarter_l << (n195 - 1));
                        if (n194) add(p + S, quarter_l << (n194 - 1));
                        if (n193) add(p + S + 1, quarter_l << (n193 - 1));
                        if (y > 0 && sp_up) add(p - S, quarter_l), add(p - S + 1, quarter_l);
                        if (y < h - 1 && sp_dn) add(p + 2L * S, quarter_l), add(p + 2L * S + 1, quarter_l);
                        if (x > 0 && sp_lf) add(p - 1, quarter_l), add(p + S - 1, quarter_l);
                        if (x < h - 1 && sp_rt) add(p + 2, quarter_l), add(p + S + 2, quarter_l);
                    } else if (marker) {
                        at(colbuf, p) = 100;
                        at(colbuf, p + 1) = 100;
                        at(colbuf, p + S) = 100;
                        at(colbuf, p + S + 1) = 100;
                    }
                }
            }
        }

        // ---- staircase pass [BYTES ISPACE.DLL seg6:0x2a50-0x3534]. It indexes the buffers
        // by the row r alone: colour buffer r and intensity buffers 2r, 2r + 1, which are
        // row r's own only for r = 0 [BYTES ISPACE.DLL seg6:0x2a60 seg6:0x2a63 seg6:0x2ced
        // seg6:0x2cf0].
        auto& C = B.col[static_cast<std::size_t>(r)];
        const auto cls = [&](long i) -> int {
            const std::uint8_t v = at(C, i);
            return v == 100 ? 2 : (v & 1);
        };
        for (int y = 2; y < h - 2; ++y) {
            for (int x = 2; x < h - 2; ++x) {
                const long p = 2L * x + 2L * y * S;
                const int c0 = cls(p), cl = cls(p - 2), cu = cls(p - 2L * S), cr = cls(p + 2),
                          cd = cls(p + 2L * S);
                auto& I0 = B.ink[static_cast<std::size_t>(2 * r)];
                auto& I1 = B.ink[static_cast<std::size_t>(2 * r + 1)];
                const auto fix = [&](long dst, long from, int layer) {
                    at(I1, dst) = 0;
                    at(I0, dst) = at(I1, dst);
                    if (c0 == 2 || layer != 2) {
                        auto& I = B.ink[static_cast<std::size_t>(2 * r + layer)];
                        at(I, dst) = at(I, from);
                        at(C, dst) = b8(at(C, from) + 101);
                    }
                };
                if (c0 != cl) {
                    if (cu == cl && (c0 != 0 || (at(C, p + 2L * S - 2) & 1) == cl))
                        fix(p, p - S, cl);
                    if (cd == cl && (c0 != 0 || (at(C, p - 2L * S - 2) & 1) == cl))
                        fix(p + S, p + 2L * S, cl);
                }
                if (c0 != cr) {
                    if (cu == cr && (c0 != 0 || (at(C, p + 2L * S + 2) & 1) == cr))
                        fix(p + 1, p - S + 1, cr);
                    if (cr == cd && (c0 != 0 || (at(C, p - 2L * S + 2) & 1) == cr))
                        fix(p + S + 1, p + 2L * S + 1, cr);
                }
            }
        }
    }

    // ---- rotation [BYTES ISPACE.DLL seg6:0x3587-0x381d]: forward, 16-bit products, the
    // shift floors (the rounding terms are zeroed at seg6:0x35a1 and seg6:0x35a6).
    for (int r = 0; r < R; ++r)
        for (int d = 1; d < N; ++d)
            for (int L = 0; L < 2; ++L) {
                const std::size_t k = static_cast<std::size_t>(r) * N * 2 + L;
                const auto& src = B.ink[k];
                auto& dst = B.ink[k + 2 * d];
                const auto& csrc = B.col[static_cast<std::size_t>(r) * N];
                auto& cdst = B.col[static_cast<std::size_t>(r) * N + d];
                for (int y = 0; y < S; ++y)
                    for (int x = 0; x < S; ++x) {
                        const std::size_t i = static_cast<std::size_t>(y) * S + x;
                        if (src[i] <= 30) continue;
                        const int a = h - x, b = h - y;
                        const std::int16_t u = w16(static_cast<long>(kSheetCos[d]) * a - static_cast<long>(kSheetSin[d]) * b);
                        const std::int16_t v = w16(static_cast<long>(kSheetCos[d]) * b + static_cast<long>(kSheetSin[d]) * a);
                        const int X = (u >> 9) + h, Y = (v >> 9) + h;
                        if (X < 0 || X >= S || Y < 0 || Y >= S) continue;
                        const std::size_t j = static_cast<std::size_t>(Y) * S + X;
                        dst[j] = src[i];
                        cdst[j] = csrc[i];
                    }
            }

    // ---- hole fill [BYTES ISPACE.DLL seg6:0x381d-0x3a6f]: directions 1 .. N - 1, interior.
    for (int r = 0; r < R; ++r)
        for (int d = 1; d < N; ++d) {
            const std::size_t k = static_cast<std::size_t>(r) * N + d;
            auto& I0 = B.ink[2 * k];
            auto& I1 = B.ink[2 * k + 1];
            auto& C = B.col[k];
            for (int y = 1; y < S - 1; ++y)
                for (int x = 1; x < S - 1; ++x) {
                    const std::size_t p = static_cast<std::size_t>(y) * S + x;
                    if (I0[p] >= 0x1c || I1[p] >= 0x1c) continue;
                    const int a = I0[p - 1] ? 1 : I1[p - 1] ? 2 : 0;
                    const int b = I0[p + 1] ? 1 : I1[p + 1] ? 2 : 0;
                    if (!a || !b) continue;
                    auto& I = a == 1 ? I0 : I1;
                    I[p] = I[p - 1];
                    C[p] = C[p - 1];
                }
        }

    // ---- the sheet [BYTES ISPACE.DLL seg6:0x3a83-0x3b6f]: PatBlt BLACKNESS.
    const int E = args.mode == 1 ? (h + g) << s : 0;
    if (N == 1) {
        out.width = h << s;
        out.height = h << s;
    } else {
        out.width = ((16 * h + 17 * g) << s) + E;
        out.height = ((h + g) * F + g) << s;
    }
    out.px.assign(static_cast<std::size_t>(out.width) * out.height, ShipSheet::kBlackness);

    // ---- the rows [BYTES ISPACE.DLL seg6:0x3bd5-0x5474]
    const int rw = N == 1 ? h << s : (16 * h + 17 * g) << s;  // row DIB width
    const int rh = N == 1 ? h << s : (2 * g + h) << s;        // row DIB height
    const int stride = rw + ((rw % 4) ? 4 - rw % 4 : 0);    // local_ec
    const long pitch = static_cast<long>(h + g) << s;         // local_e8
    const long base = (static_cast<long>(g) << s) + ((static_cast<long>(g) * stride) << s);  // local_e4
    const int H = h << s;                                      // local_17a
    std::vector<std::uint8_t> dib(static_cast<std::size_t>(stride) * rh);
    for (int r = 0; r < R; ++r) {
        std::fill(dib.begin(), dib.end(), 0);  // seg5:0x23a4 zero-fills
        const auto put = [&](long a, std::uint8_t v) {
            if (a >= 0 && a < static_cast<long>(dib.size())) dib[static_cast<std::size_t>(a)] = v;
        };
        for (int d = 0; d < N; ++d)
            for (int L = 0; L < 2; ++L) {
                const std::size_t k = (static_cast<std::size_t>(r) * N + d) * 2 + L;
                const auto& I = B.ink[k];
                const auto& C = B.col[static_cast<std::size_t>(r) * N + d];
                const auto& tab = groups[L];
                int colour = 0;  // local_16e keeps its value between pixels
                for (int y = 0; y < H; ++y)
                    for (int x = 0; x < H; ++x) {
                        int avg, cnt;
                        if (s == 0) {
                            const long p = 2L * x + 2L * y * S;
                            const long q[4] = {p, p + 1, p + S, p + S + 1};
                            int sum = 0;
                            cnt = 0;
                            for (long i : q) {
                                const std::uint8_t v = I[static_cast<std::size_t>(i)];
                                if (v) {
                                    sum += v;
                                    ++cnt;
                                    colour = C[static_cast<std::size_t>(i)];
                                }
                            }
                            avg = cnt ? b8(sum / cnt) : 0;
                        } else {
                            const std::size_t p = static_cast<std::size_t>(y) * S + x;  // (2x >> 1)
                            avg = I[p];
                            colour = C[p];
                            cnt = 4;
                        }
                        // [BYTES ISPACE.DLL seg6:0x4caf-0x4cc6]
                        if (avg == 0 || cnt <= 1 || colour == 100) continue;
                        int c = colour;
                        if (c > 100) c -= 101;
                        int pix = 0xf5;
                        if (L == 0) {
                            if (c % 2 == 1) c = 0;
                        } else {
                            if ((c & 1) == 0) c = 1;
                        }
                        const std::size_t gi = static_cast<std::size_t>(c >> 1);
                        // An index past the groups reads other DGROUP words in the original
                        // [OPEN]; the port takes a ramp-less group (0xf5) [DESIGN].
                        if (gi < tab.size() && palette.kind0) {
                            const Group& gr = tab[gi];
                            if (gr.t_hi < 2 * avg) {  // [BYTES ISPACE.DLL seg6:0x4cd2-0x4d9d]
                                const int den = w16(gr.t_lo - gr.t_hi);
                                const long num = static_cast<long>(w16(gr.hi - gr.lo)) * w16(avg - gr.t_hi);
                                // den = 0 (T(lo) = T(hi)) faults the original's long divide
                                // seg3:0x130; the port takes 0 [DESIGN]. The clamp to lo..hi
                                // follows the divide, within the range above.
                                int pv = w16(gr.hi - (den ? num / den : 0));
                                if (pv < gr.lo)
                                    pv = gr.lo;
                                else if (pv > gr.hi)
                                    pv = gr.hi;
                                pix = nearest_ramp_entry(pal, avg, gr.lo, gr.hi, pv) & 0xff;
                            }
                        }
                        const std::uint8_t v8 = b8(pix);
                        put(d * pitch + static_cast<long>(y) * stride + x + base, v8);
                        if (N > 1) {  // the quarter turns [BYTES ISPACE.DLL seg6:0x4df9-0x4f8b]
                            put(static_cast<long>(H - y - 1) * stride + (d + 8) * pitch - x + H + base - 1, v8);
                            put(static_cast<long>(H - x - 1) * stride + (d + 4) * pitch + y + base, v8);
                            put((d + 12) * pitch + static_cast<long>(x) * stride - y + H + base - 1, v8);
                        }
                    }
            }
        // BitBlt SRCCOPY at (0, ((h + g) r) << s), the DIB's memory rows bottom-up.
        const int y0 = N == 1 ? 0 : ((h + g) * r) << s;
        for (int m = 0; m < rh; ++m) {
            const int ty = y0 + rh - 1 - m;
            if (ty < 0 || ty >= out.height) continue;
            for (int x = 0; x < rw && x < out.width; ++x)
                out.px[static_cast<std::size_t>(ty) * out.width + x] =
                    dib[static_cast<std::size_t>(m) * stride + x];
        }
    }
    return out;
}

std::vector<std::int16_t> sheet_cell(const ShipSheet& sh, int heading, int row) {
    const int n = sh.cell_size();
    std::vector<std::int16_t> c(static_cast<std::size_t>(n) * n, ShipSheet::kBlackness);
    if (!sh.ok()) return c;
    const int x0 = sh.dirs == 1 ? 0 : sh.cell_x(heading & 15), y0 = sh.dirs == 1 ? 0 : sh.cell_y(row);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (x0 + x < sh.width && y0 + y < sh.height)
                c[static_cast<std::size_t>(y) * n + x] = sh.at(x0 + x, y0 + y);
    return c;
}

std::vector<std::uint32_t> sheet_rgb(const ShipSheet& sh, const PaletteArray& pal) {
    std::vector<std::uint32_t> out(sh.px.size(), 0);
    for (std::size_t i = 0; i < sh.px.size(); ++i)
        if (sh.px[i] >= 0) out[i] = pal[static_cast<std::size_t>(sh.px[i])].rgb().rgb();
    return out;
}

ShipSheet ship_sheet_for(const fmt::Dib& body, int body_id, PaletteManager& pm, PalHandle pal,
                         const SheetArgs& args) {
    const int kind = pm.kind(pal);
    const int mode = pm.mode(pal);
    const bool kind0 = kind == 0;
    // Set-up: seg5:0x6f58 and seg5:0x61b2 for each scheme that is not 12 [BYTES ISPACE.DLL
    // seg6:0x176-0x1be].
    if (kind0) {
        for (int sc : {args.scheme_a, args.scheme_b})
            if (static_cast<std::int8_t>(sc) != 12) pm.allocate_ramp(pal, scheme_ramp_id(sc));
    }
    // The load: seg5:0x1b8e(hPal, id, lib, d = 1, a, b, 0) with DGROUP:0x188 = 1
    // [BYTES ISPACE.DLL seg6:0x1cf-0x1ec].
    std::vector<std::uint32_t> table = body.palette;
    // seg6:0x7fc8 while DGROUP:0x1ea8 is set [BYTES ISPACE.DLL seg6:0x1f5-0x200] (palette.h
    // fixed_set_remap).
    fmt::Dib remapped;
    const fmt::Dib* src = &body;
    if (pm.recolour_graphic(pal, body_id, table, 1, args.scheme_a, args.scheme_b, 0, true)) {
        remapped = body;
        fixed_set_remap(remapped);
        src = &remapped;
    }
    if (table.size() < 16) table.resize(16, 0);
    SheetPalette sp;
    sp.working = pm.working(pal);
    sp.kind0 = kind0;
    sp.colour_ramp = colour_ramp_id;  // seg5:0x66ea for scheme 12 [BYTES ISPACE.DLL seg6:0x12c6 seg6:0x1850]
    sp.ramp = [&pm, pal, mode](int c) {
        pm.allocate_ramp(pal, c);  // seg5:0x61b2 [BYTES ISPACE.DLL seg6:0xe12 seg6:0x1374]
        const int rec = pm.ramp_of(mode, c);
        if (rec < 0) return RampRange{};  // [OPEN] the original reads DGROUP:0x1dea as it is
        const auto rr = pm.ramp_record(mode, rec);
        return RampRange{rr.first, rr.last};
    };
    return render_ship_sheet(*src, table, args, sp);
}

}  // namespace is::gfx
