// The thrust flame's pictures (ship_flame.h): ISPACE.DLL ordinal 18 (seg6:0x5e76) on INSPACE's
// path and seg10:0x7032's mirrors. Portable.
//
// Buffers follow the original's: the rotation buffer and the RGB buffer are in memory rows
// (bottom-up, as the DIBs store them), turned top-down only for the result.
#include "gfx/ship_flame.h"

#include <cstdlib>

namespace is::gfx {

const std::array<std::int16_t, 9> kFlameT1 = {0, 196, 362, 473, 512, 473, 362, 196, 0};
const std::array<std::int16_t, 9> kFlameT2 = {-512, -473, -362, -196, 0, 196, 362, 473, 512};
const std::array<std::int8_t, kFlameDirsMax> kFlameOffX = {0, -1, 1, 1, 0};
const std::array<std::int8_t, kFlameDirsMax> kFlameOffY = {0, 2, 1, -1, -1};

namespace {

std::int16_t w16(long v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xffff)); }

// (a + 256) / 512 for a >= 0, (a - 256) / 512 below: the idiv truncates, so this rounds half away
// from zero [BYTES ISPACE.DLL seg6:0x6262-0x62c5].
int round512(int a) {
    const int b = w16(a + (a >= 0 ? 256 : -256));
    return b / 512;
}

// The 4-bpp nibble of column c in memory row r: high nibble for even c [BYTES ISPACE.DLL
// seg6:0x61f0-0x6230].
int nibble(const fmt::Dib& d, int c, int r) {
    const std::size_t at = static_cast<std::size_t>(r) * d.stride() + static_cast<std::size_t>(c / 2);
    if (at >= d.bits.size()) return 0;
    const std::uint8_t b = d.bits[at];
    return (c & 1) == 0 ? (b >> 4) : (b & 0x0f);
}

}  // namespace

int nearest_flame_entry(const FlamePalette& fp, Rgb c, int& last) {
    if (fp.mode2) return 0;  // al = 0 [BYTES ISPACE.DLL seg5:0x6512 seg6:0x71a8]
    if (!fp.working) return last;
    int best = 1000;  // [bp-0x10] [BYTES ISPACE.DLL seg5:0x6562]
    for (const FlamePalette::Range& r : fp.scan) {
        for (int e = r.first; e <= r.last; ++e) {
            if (e < 0 || e > 255) continue;  // [DESIGN] storage bound
            const PalEntry& p = (*fp.working)[static_cast<std::size_t>(e)];
            // |dr| + |dg| + |db| as unsigned byte differences [BYTES ISPACE.DLL seg5:0x6573-0x664d]
            const int dist = std::abs(c.r - p.r) + std::abs(c.g - p.g) + std::abs(c.b - p.b);
            if (dist < best) {  // jge: strictly smaller [BYTES ISPACE.DLL seg5:0x6650-0x665e]
                best = dist;
                last = e;
            }
            if (dist < 14) return last;  // [BYTES ISPACE.DLL seg5:0x6661-0x666f]
        }
    }
    return last;
}

FlameBitmap render_flame(const fmt::Dib& flame, const std::vector<std::uint32_t>& colours, int n,
                         const FlamePalette& fp, int& dg_1a6) {
    FlameBitmap out;
    const int W = flame.width, H = flame.height;
    // [DESIGN] the shipped graphic 280 is 18 x 18 at 4 bpp; the original assumes W = H and an
    // even H (its row loop runs to W, its column loop to H, its 2 x 2 loop to H by twos).
    if (flame.bpp != 4 || W <= 0 || H <= 0 || W != H || (H & 1) != 0 || n < 1 || n > kFlameDirsMax)
        return out;
    // The 16 colours as bytes r, g, b (DGROUP:0x1178 + 4) [BYTES ISPACE.DLL seg6:0x6e3d].
    std::array<Rgb, 16> col{};
    for (int i = 0; i < 16; ++i)
        col[static_cast<std::size_t>(i)] =
            Rgb::of(i < static_cast<int>(colours.size()) ? colours[static_cast<std::size_t>(i)] : 0);

    // 1. The rotation: seg5:0x23a4(W n, H, 8, hPal), zeroed [BYTES ISPACE.DLL seg6:0x6191 seg6:0x61b3-
    // 0x6371].
    const int bw = W * n;
    const int stride = (bw + 3) / 4 * 4;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(stride) * H, 0);
    const int c0 = H / 2;
    for (int d = 0; d < n; ++d)
        for (int r = 0; r < W; ++r)
            for (int c = 0; c < H; ++c) {
                const int v = nibble(flame, c, r);
                if (v == 0) continue;  // index 0 is transparent
                const int A = w16((r - c0) * kFlameT1[static_cast<std::size_t>(d)] +
                                  (c0 - c) * kFlameT2[static_cast<std::size_t>(d)]);
                const int B = w16((c0 - r) * kFlameT2[static_cast<std::size_t>(d)] +
                                  (c0 - c) * kFlameT1[static_cast<std::size_t>(d)]);
                const int X = round512(A) + kFlameOffX[static_cast<std::size_t>(d)] + c0;
                const int Y = round512(B) + kFlameOffY[static_cast<std::size_t>(d)] + c0;
                if (X < 0 || X >= W || Y < 0 || Y >= H) continue;
                buf[static_cast<std::size_t>(H * d + X + Y * stride)] = static_cast<std::uint8_t>(v);
            }
    // 2. The holes [BYTES ISPACE.DLL seg6:0x638c-0x6411].
    for (int r = 1; r <= H - 2; ++r)
        for (int c = 1; c <= H * n - 2; ++c) {
            const std::size_t p = static_cast<std::size_t>(r) * stride + c;
            if (buf[p] == 0 && buf[p - 1] != 0 && buf[p + 1] != 0) buf[p] = buf[p - 1];
        }
    // 3. The 2 x 2 average into (H n / 2) x (H / 2) RGB [BYTES ISPACE.DLL seg6:0x6ce5-0x6f28].
    const int ow = H * n / 2, oh = H / 2;
    std::vector<Rgb> rgb(static_cast<std::size_t>(ow) * oh);
    for (int y = 0; y < H; y += 2)
        for (int x = 0; x < H * n; x += 2) {
            const std::size_t p00 = static_cast<std::size_t>(y) * stride + x;
            const std::size_t p01 = static_cast<std::size_t>(y + 1) * stride + x;
            const Rgb& a = col[buf[p00]];
            const Rgb& b = col[buf[p00 + 1]];
            const Rgb& e = col[buf[p01]];
            const Rgb& f = col[buf[p01 + 1]];
            Rgb& o = rgb[static_cast<std::size_t>(y / 2) * ow + x / 2];
            o.r = static_cast<std::uint8_t>((a.r + b.r + e.r + f.r) >> 2);
            o.g = static_cast<std::uint8_t>((a.g + b.g + e.g + f.g) >> 2);
            o.b = static_cast<std::uint8_t>((a.b + b.b + e.b + f.b) >> 2);
        }
    // 4. The output DIB seg5:0x23a4(H n / 2, H e4 / 2, 8, hPal) with e4 = 2: row block 0 at memory
    // row H / 2 (the top), row block 1 at memory row 0 [BYTES ISPACE.DLL seg6:0x5fa2-0x5fc4
    // seg6:0x7722-0x7739].
    out.width = ow;
    out.height = H;
    out.cell = H / 2;
    out.dirs = n;
    out.px.assign(static_cast<std::size_t>(ow) * H, 0);
    for (int y = 0; y < oh; ++y)
        for (int x = 0; x < ow; ++x) {
            const Rgb& c = rgb[static_cast<std::size_t>(y) * ow + x];
            int v = 0;
            if (!(c.r <= kFlameDarkMax && c.g <= kFlameDarkMax && c.b <= kFlameDarkMax))
                v = nearest_flame_entry(fp, c, dg_1a6) & 0xff;  // the low byte [BYTES ISPACE.DLL seg6:0x71a0]
            int v1 = 0;
            if (v != 0 && fp.remap && fp.span1 != 0)  // 16-bit idiv [BYTES ISPACE.DLL seg6:0x71c4-0x71ec]
                v1 = w16(w16((v - fp.lo1) * fp.span0) / fp.span1 + fp.lo0);
            // Memory row H / 2 + y is top-down row H - 1 - (H / 2 + y); memory row y is H - 1 - y.
            out.px[static_cast<std::size_t>(H - 1 - (H / 2 + y)) * ow + x] = static_cast<std::uint8_t>(v);
            out.px[static_cast<std::size_t>(H - 1 - y) * ow + x] = static_cast<std::uint8_t>(v1 & 0xff);
        }
    return out;
}

FlameBitmap flame_bitmap_for(const fmt::Dib& flame, PaletteManager& pm, PalHandle pal) {
    // seg5:0x1b8e(hPal, 0x118, DGROUP:0x15e, 1, 12, 12, 0) with DGROUP:0x188 = 0 [BYTES ISPACE.DLL
    // seg6:0x5f35-0x5f59].
    std::vector<std::uint32_t> table = flame.palette;
    pm.recolour_graphic(pal, kFlameGraphic, table, 1, 12, 12, 0, false);
    if (table.size() < 16) table.resize(16, 0);
    FlamePalette fp;
    fp.working = pm.working(pal);
    const int mode = pm.mode(pal);
    fp.mode2 = mode == 2;
    // The palette kind DGROUP:0x1e92[slot] = 0 path [BYTES ISPACE.DLL seg6:0x7079-0x70b0]; kind 1
    // (seg5:0x4e96, one row) is not ported [DESIGN].
    if (pm.kind(pal) != 0) return {};
    const auto range = [&pm, mode](int scheme) {
        FlamePalette::Range r;
        const int rec = pm.ramp_of(mode, scheme);
        if (rec < 0) return r;
        const auto rr = pm.ramp_record(mode, rec);
        r.first = rr.first;
        r.last = rr.last;
        return r;
    };
    fp.scan[0] = range(1);  // seg5:0x64f8(slot, r, g, b, 1, -1) [BYTES ISPACE.DLL seg6:0x71a0]
    // fp.scan[1]: scheme -1, none [BYTES ISPACE.DLL seg5:0x653a-0x6545].
    const FlamePalette::Range r1 = range(1), r0 = range(0);
    if (r1.last >= r1.first && r0.last >= r0.first) {
        fp.remap = true;
        fp.lo1 = r1.first;
        fp.span1 = r1.last - r1.first;
        fp.lo0 = r0.first;
        fp.span0 = r0.last - r0.first;
    }
    int dg_1a6 = 0x28 >> 2;  // [BYTES ISPACE.DLL DGROUP:0x1a6] [DESIGN] per call
    return render_flame(flame, table, kFlameDirsMax, fp, dg_1a6);
}

int flame_cell(int size_level, bool seg18_mode) {
    if (seg18_mode) return 6 * size_level + 3;
    return (9 << (size_level & 0x1f)) >> 1;
}

FlameHeadings flame_headings(const FlameBitmap& bmp, int f) {
    FlameHeadings h;
    if (!bmp.ok() || f <= 0 || bmp.cell <= 0) return h;
    const int s = bmp.cell;
    h.f = f;
    h.width = 16 * f;
    h.height = 2 * f;
    h.px.assign(static_cast<std::size_t>(h.width) * h.height, 0);  // PatBlt BLACKNESS
    // The source pixel of cell k, row r at (x, y) of an f x f cell (nearest-neighbour for s != f).
    const auto src = [&](int k, int r, int x, int y) -> std::uint8_t {
        const int sx = k * s + x * s / f, sy = r * s + y * s / f;
        if (sx < 0 || sy < 0 || sx >= bmp.width || sy >= bmp.height) return 0;
        return bmp.at(sx, sy);
    };
    const auto put = [&](int col, int r, int x, int y, std::uint8_t v) {
        h.px[static_cast<std::size_t>(r * f + y) * h.width + col * f + x] = v;
    };
    for (int r = 0; r < 2; ++r)
        for (int k = 0; k < kFlameDirsMax && k < bmp.dirs; ++k)
            for (int y = 0; y < f; ++y)
                for (int x = 0; x < f; ++x) {
                    const std::uint8_t v = src(k, r, x, y);
                    put(k, r, x, y, v);  // the 5 x 2 cells [BYTES seg10:0x70a4]
                    const int fx = f - 1 - x, fy = f - 1 - y;
                    if (k == 0) {
                        put(8, r, x, fy, v);  // vertical flip
                    } else if (k == 4) {
                        put(12, r, fx, y, v);  // horizontal flip
                    } else {
                        put(8 - k, r, x, fy, v);    // vertical flip
                        put(k + 8, r, fx, fy, v);   // 180 degrees
                        put(16 - k, r, fx, y, v);   // horizontal flip
                    }
                }
    return h;
}

}  // namespace is::gfx
