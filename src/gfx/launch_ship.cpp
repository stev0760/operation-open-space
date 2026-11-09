// owner: M4 stage 6, track PAD.
// The launch pictures (src/gfx/launch_ship.h). INSPACE.EXE addresses; 16-bit arithmetic as the
// original's (ADR 0013 decision 7).
#include "gfx/launch_ship.h"

#include <algorithm>
#include <cstdint>

#include "gfx/ship_flame.h"
#include "gfx/smooth_render.h"

namespace is::gfx {

namespace {

constexpr std::int16_t lo16(std::int32_t v) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(static_cast<std::uint32_t>(v) & 0xffffu));
}
constexpr std::int16_t idiv16(std::int32_t n, std::int32_t d) {
    return lo16(d) == 0 ? std::int16_t{0} : lo16(static_cast<std::int32_t>(lo16(n)) / lo16(d));
}
// cwd; sub ax, dx; sar ax, 1.
constexpr std::int16_t half16(std::int32_t v) {
    const std::int16_t w = lo16(v);
    return static_cast<std::int16_t>((w - (w < 0 ? -1 : 0)) >> 1);
}

}  // namespace

int visit_ship_size(const VisitShipArt& v) { return lo16(v.r7a + half16(v.r7b - 6)); }

void jet_frame0(int sz, int detail, int& w, int& h) {
    switch (detail) {  // [BYTES seg15:0x2855-0x28a7]
    case 0: w = lo16(4 * sz), h = idiv16(lo16(16 * sz), 10); break;
    case 1: w = lo16(5 * sz), h = lo16(2 * sz); break;
    case 2: w = lo16(6 * sz), h = idiv16(lo16(24 * sz), 10); break;
    case 3: w = lo16(7 * sz), h = idiv16(lo16(28 * sz), 10); break;
    default: w = 0, h = 0; break;
    }
}

// seg15:0x1a42's table (launch_ship.h) [OPEN: read from the listing by the PAD track and spot-
// checked by the reviewer, unchecked by a verifier (shell.md "The landing scene"); a verify round
// on seg15:0x1a4f-0x1ca1 settles it] (review M2).
std::array<JetFrame, 16> jet_table(int sz, int detail) {
    std::array<JetFrame, 16> t{};
    int w0 = 0, h0 = 0;
    jet_frame0(sz, detail, w0, h0);
    if (w0 == 0) return t;
    int c = 0, dc = 0, q = 1;
    switch (detail) {  // [BYTES seg15:0x1ac2-0x1b2f]
    case 0: c = 350, dc = 16, q = 4; break;
    case 1: c = 450, dc = 27, q = 5; break;
    case 2: c = 550, dc = 39, q = 6; break;
    default: c = 650, dc = 50, q = 7; break;
    }
    t[0] = {w0, h0, 0};  // DGROUP:0x3ce8 = 0 [BYTES seg15:0x1aad-0x1abc]
    int total = h0;
    for (int i = 1; i < 10; ++i) {  // [BYTES seg15:0x1bf1-0x1c42]
        const int w = idiv16(lo16(idiv16(w0, q) * idiv16(c, 10)), 10);
        const int h = idiv16(lo16(40 * w), 100);
        t[static_cast<std::size_t>(i)] = {w, h, total};
        total = lo16(total + h);
        c = lo16(c - dc);
    }
    for (int i = 0, m = 10, p = 100; i < 6; ++i, m += 2, p -= 12) {  // [BYTES seg15:0x1c44-0x1ca1]
        const int w = idiv16(lo16(idiv16(w0, q) * m), 10);
        const int h = idiv16(lo16(w * p), 100);
        t[static_cast<std::size_t>(15 - i)] = {w, h, total};
        total = lo16(total + h);
    }
    return t;
}

int jet_table_height(const std::array<JetFrame, 16>& t) {
    int total = 0;
    for (const JetFrame& f : t) total = lo16(total + f.h);
    return total;
}

#ifdef _WIN32
namespace {

constexpr DWORD kShadowRop = 0x00A803A9;  // DPSoa [BYTES seg15:0x4456-0x4459]

// One index per pixel into a fresh image.
void put(Surface& s, int x, int y, std::uint8_t v) {
    if (x >= 0 && y >= 0 && x < s.width() && y < s.height()) s.pixels()[static_cast<std::size_t>(y) * s.stride() + x] = v;
}

// The flame pair of seg15:0x2a08 step 5 and seg15:0x20ca step 6 at DGROUP:0x90f0 != 0:
// seg10:0x9962(0, Q, fx k, fy k, 9k, 9k, FM, 0, sy, 18, 18, SRCAND), then of F SRCPAINT [BYTES
// seg15:0x2e35-0x2e98]; below 8 bpp the 36 x 36 BitBlt pair from (0, 2 sy) [BYTES
// seg15:0x2dd9-0x2e2e] (verified claims 27, 28).
void flame_pair(HDC q, const LaunchFlame& f, int fx, int fy, int sy) {
    if (!f.img || !f.mask) return;
    constexpr int k = kLaunchK;
    if (kBitsPixel >= 8) {
        SetStretchBltMode(q, COLORONCOLOR);
        StretchBlt(q, fx * k, fy * k, 9 * k, 9 * k, f.mask->hdc(), 0, sy, 18, 18, SRCAND);
        StretchBlt(q, fx * k, fy * k, 9 * k, 9 * k, f.img->hdc(), 0, sy, 18, 18, SRCPAINT);
    } else {
        BitBlt(q, fx * k, fy * k, 36, 36, f.mask->hdc(), 0, 2 * sy, SRCAND);
        BitBlt(q, fx * k, fy * k, 36, 36, f.img->hdc(), 0, 2 * sy, SRCPAINT);
    }
}

// The body image DGROUP:0x92b0 of seg10:0x73a6(id, name, -2, -2, 2, 0, 1, a, b) [BYTES
// seg15:0x2b75-0x2b97 seg15:0x2137-0x216d] (smooth_render.h; verified claim 32, CORRECTED: the
// bytes pushed as AX after `mov al`).
ImagePtr body_image(Gfx& g, const fmt::Dib& body, int id, int a, int b, const LaunchTodo& todo) {
    if (body.height <= 0) {
        if (todo) todo("launch ship: no body graphic " + std::to_string(id), "seg15:0x2b8f");
        return nullptr;
    }
    const SmoothPicture pic = smooth_for(body, id, g.palettes(), g.palette(), smooth_args(a & 0xff, b & 0xff));
    ImagePtr img = smooth_image(g, pic);
    if (!img && todo) todo("launch ship: ordinal 12 returned no bitmap for graphic " + std::to_string(id), "seg6:0xc4");
    return img;
}

}  // namespace

ImagePtr pad_image(Gfx& g, const PadPicture& p, const std::function<int(int)>& index) {
    if (!p.ok()) return nullptr;
    ImagePtr img = g.create_image(p.w, p.h);
    img->flush();
    std::uint8_t map[256] = {};
    bool known[256] = {};
    for (int y = 0; y < p.h; ++y)
        for (int x = 0; x < p.w; ++x) {
            const int v = p.at(x, y);
            if (v == 0) continue;
            if (!known[v]) {
                map[v] = static_cast<std::uint8_t>(index ? index(v) : v);
                known[v] = true;
            }
            put(*img, x, y, map[v]);
        }
    return img;
}

HBRUSH checker_brush() {
    std::uint8_t bits[16] = {};
    for (int i = 0; i < 8; ++i) bits[2 * i] = (i % 2 == 0) ? 0xaa : 0x55;  // DGROUP:0x6b6 [BYTES DGROUP:0x6b6]
    HBITMAP bm = CreateBitmap(8, 8, 1, 1, bits);
    if (!bm) return nullptr;
    HBRUSH h = CreatePatternBrush(bm);
    DeleteObject(bm);
    return h;
}

void shadow_blit(HDC dst, int x, int y, int w, int h, HDC mask, int sx, int sy, int sw, int sh) {
    if (!dst || !mask) return;
    HBRUSH br = checker_brush();  // DGROUP:0x5eaa at DGROUP:0x90ee >= 8
    HGDIOBJ old = br ? SelectObject(dst, br) : nullptr;
    if (kBitsPixel <= 8) {  // the blit runs at depth <= 8 only (ruling R2)
        if (sw < 0) {
            BitBlt(dst, x, y, w, h, mask, sx, sy, kShadowRop);
        } else {
            SetStretchBltMode(dst, COLORONCOLOR);
            StretchBlt(dst, x, y, w, h, mask, sx, sy, sw, sh, kShadowRop);
        }
    }
    if (old) SelectObject(dst, old);
    if (br) DeleteObject(br);
}

LaunchFlame launch_flame(Gfx& g, const fmt::Dib& flame280, int row) {
    (void)row;  // y != 0 sets ISPACE.DLL DGROUP:0x1ea6 = 1 [BYTES ISPACE.DLL seg6:0x5f0e-0x5f23] [OPEN: its effect]
    LaunchFlame out;
    if (flame280.height <= 0 || flame280.bpp != 4) return out;
    // The load seg5:0x1b8e(hPal, 0x118, DGROUP:0x15f, 0, 4, -1, 0) [BYTES ISPACE.DLL seg6:0x5f2f-0x5f59]
    // (verified claim 69).
    fmt::Dib d = flame280;
    g.palettes().recolour_graphic(g.palette(), kFlameGraphic, d.palette, 0, 4, -1, 0, false);
    // Direction 0 turns nothing (T1 = 0, T2 = -512, offsets 0) [BYTES ISPACE.DLL DGROUP:0xf6
    // DGROUP:0x108], so cell 0 is the graphic's non-zero nibbles in place; the gap fill of rows and
    // columns 1 ... 16 [BYTES ISPACE.DLL seg6:0x638c-0x6411]; no 2 x 2 average for x != 0 [BYTES
    // ISPACE.DLL seg6:0x6413-0x6432]; the bitmap through seg5:0x23a4 [OPEN: read as seg5:0x2080's
    // nearest entries] [DESIGN].
    const int w = std::min(d.width, 18), h = std::min(d.height, 18);
    std::vector<std::uint8_t> nib(18 * 18, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) nib[static_cast<std::size_t>(y) * 18 + x] = d.index_at(x, y);
    for (int y = 1; y < 17; ++y)
        for (int x = 1; x < 17; ++x) {
            std::uint8_t& p = nib[static_cast<std::size_t>(y) * 18 + x];
            const std::uint8_t l = nib[static_cast<std::size_t>(y) * 18 + x - 1], r = nib[static_cast<std::size_t>(y) * 18 + x + 1];
            if (p == 0 && l != 0 && r != 0) p = l;
        }
    out.img = g.create_image(18, 18);
    out.img->flush();
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 18; ++x) {
            const std::uint8_t v = nib[static_cast<std::size_t>(y) * 18 + x];
            if (v == 0 || v >= d.palette.size()) continue;
            const std::uint32_t c = d.palette[v] & 0xffffffu;
            put(*out.img, x, y, static_cast<std::uint8_t>(nearest_index(g.palettes().device(), Rgb::of(c))));
        }
    out.mask = g.mask(*out.img);  // seg10:0x6ec6(DGROUP:0x3cc8) [BYTES seg15:0x2bed]
    return out;
}

LaunchShip ship_2a08(Gfx& g, const LaunchShipArt& art, const LaunchTodo& todo) {
    LaunchShip out;
    constexpr int k = kLaunchK;
    // Step 4: DGROUP:0x92b0 [BYTES seg15:0x2b75-0x2b97].
    out.body = body_image(g, art.body, art.id, art.a, art.b, todo);
    if (!out.body) return out;
    // Step 5: s and w8 [BYTES seg15:0x2b9a-0x2bb9] (verified claim 20).
    out.s = idiv16(out.body->height(), k);
    const int s = out.s, ks = lo16(k * s);
    const int w8 = half16(s - art.band);
    // Step 6: DGROUP:0x92b2 [BYTES seg15:0x2bbc-0x2bcd].
    out.square = g.create_image(ks, ks);
    // Step 7: the flame, row = SHP +0x09 bit 0 clear [BYTES seg15:0x2ae2-0x2af0 seg15:0x2bd0-0x2bf9].
    const int row = (art.ship_flags & 1) == 0 ? 1 : 0;
    if (todo) todo("ordinal 18 with x = 1 (seg10:0x7032(1, 1, row)): direction 0's 18 x 18 cell, its colours mapped to the nearest entries (ORD18-x1)",
                   "ISPACE.DLL seg6:0x641c");
    const LaunchFlame flame = launch_flame(g, art.flame, row);
    // The common part [BYTES seg15:0x2c88-0x302c]: DGROUP:0x92b4 deleted (out.flameless null).
    ImagePtr mask = g.mask(*out.body);  // seg10:0x6ec6(DGROUP:0x92b0), before the cuts [BYTES seg15:0x2cbe]
    HDC q = out.square->hdc(), b = out.body->hdc();
    BitBlt(q, 0, 0, ks, ks, b, 0, 0, SRCCOPY);  // [BYTES seg15:0x2d66-0x2d81]
    const bool flame_on = (art.body_flags & 0x01) == 0;  // [BYTES seg15:0x2ac7-0x2ad5]
    const bool behind = (art.body_flags & 0x08) != 0;    // [BYTES seg15:0x2b49-0x2b57]
    const bool wings = (art.body_flags & 0x04) != 0;     // [BYTES seg15:0x2a86-0x2aa5]
    if (flame_on) {  // [BYTES seg15:0x2d8c-0x2e98] (verified claims 27-29)
        const int fx = half16(s - 9);
        const int sy = (k == 4 && row != 0) ? 0x12 : 0;
        if (behind) PatBlt(q, 0, 0, ks, ks, BLACKNESS);
        flame_pair(q, flame, fx, art.fy, sy);
    }
    if (behind) {  // reached with the flame off too [BYTES seg15:0x2d8c seg15:0x2e9b-0x2eda]
        BitBlt(q, 0, 0, ks, ks, mask->hdc(), 0, 0, SRCAND);
        BitBlt(q, 0, 0, ks, ks, b, 0, 0, SRCPAINT);
    }
    // Step 7 of the common part: the cuts (verified claim 30, CORRECTED: each needs the wings flag)
    // [BYTES seg15:0x2edf-0x2f8c].
    if (art.keep && wings) {
        if (!art.left_wing) {
            PatBlt(q, 0, 0, k * w8, ks, BLACKNESS);
            PatBlt(b, 0, 0, k * w8, ks, BLACKNESS);
        }
        if (!art.right_wing) {
            PatBlt(q, k * (s - w8), 0, k * w8, ks, BLACKNESS);
            PatBlt(b, k * (s - w8), 0, k * w8, ks, BLACKNESS);
        }
    }
    GdiFlush();
    return out;
}

LaunchShip ship_20ca(Gfx& g, const VisitShipArt& v, const LaunchTodo& todo) {
    LaunchShip out;
    constexpr int k = kLaunchK;
    const int sz = visit_ship_size(v), ks = lo16(k * sz);
    out.s = sz;
    // Step 2 [BYTES seg15:0x2137-0x216d] (verified claim 32, CORRECTED).
    out.body = body_image(g, v.body, v.id, v.a, v.b, todo);
    if (!out.body || ks <= 0) return out;
    // Step 3: two colour squares [BYTES seg15:0x2170-0x2191].
    out.square = g.create_image(ks, ks);
    out.flameless = g.create_image(ks, ks);
    // Step 4: the body mask and the flame of row DGROUP:0x9716 + d != 0 [BYTES seg15:0x2194-0x2260].
    ImagePtr mask = g.mask(*out.body);
    const int frow = v.flame_row != 0 ? 1 : 0;
    if (todo) todo("ordinal 18 with x = 1 (seg10:0x7032(1, 1, row)): direction 0's 18 x 18 cell, its colours mapped to the nearest entries (ORD18-x1)",
                   "ISPACE.DLL seg6:0x641c");
    const LaunchFlame flame = launch_flame(g, v.flame, frow);
    HDC q = out.square->hdc(), q2 = out.flameless->hdc(), b = out.body->hdc();
    // w8 = half(sz) - trunc8(sext(BOD +0x1a) / 2), the `idiv cl` byte divide [BYTES seg15:0x22d1-0x2310].
    const int w8 = half16(sz) - static_cast<std::int8_t>(static_cast<std::int8_t>(v.band) / 2);
    const auto cuts = [&](HDC d) {  // [BYTES seg15:0x2310-0x237a seg15:0x26d5-0x27aa]
        if (v.ability != 3) return;
        if (!v.left_wing) PatBlt(d, 0, 0, k * w8, ks, BLACKNESS);
        if (!v.right_wing) PatBlt(d, k * (sz - w8), 0, k * w8, ks, BLACKNESS);
    };
    const auto dead = [&] {  // seg15:0x28b2(Q, Q2, k, sz) when DGROUP:0x9e5b = 0 (verified claim 36)
        if (v.dead && todo) todo("seg15:0x28b2, the dead ship's squares", "seg15:0x28b2");
    };
    if (v.behind == 0) {  // step 5 [BYTES seg15:0x2273-0x23b3] (verified claim 33)
        BitBlt(q, 0, 0, ks, ks, b, 0, 0, SRCCOPY);
        cuts(q);
        BitBlt(q2, 0, 0, ks, ks, q, 0, 0, SRCCOPY);  // DGROUP:0x92b4: the flameless, cut ship
        dead();
    }
    if (v.flame_on) {  // step 6 [BYTES seg15:0x23b9-0x25cd] (verified claim 35)
        const int fx = half16(sz) - 4;
        const int sy = (k == 4 && frow != 0) ? 0x12 : 0;
        if (v.behind != 0) PatBlt(q, 0, 0, ks, ks, BLACKNESS);
        flame_pair(q, flame, fx, v.fy, sy);
    }
    if (v.behind != 0) {  // step 7 [BYTES seg15:0x25d0-0x27c3]
        BitBlt(q, 0, 0, ks, ks, mask->hdc(), 0, 0, SRCAND);
        BitBlt(q, 0, 0, ks, ks, b, 0, 0, SRCPAINT);
        BitBlt(q2, 0, 0, ks, ks, b, 0, 0, SRCCOPY);
        cuts(q);
        cuts(q2);
        dead();
    }
    GdiFlush();
    return out;
}

int ship_2a08_b(Gfx& g, const Surface& body, Surface& square, const VisitShipArt& v, const LaunchTodo& todo) {
    constexpr int k = kLaunchK;
    const int s = idiv16(body.height(), k), ks = lo16(k * s);  // [BYTES seg15:0x2c31-0x2c48]
    const int frow = v.flame_row != 0 ? 1 : 0;                 // seg15:0x20ca's DGROUP:0x3cc8 row
    const LaunchFlame flame = launch_flame(g, v.flame, frow);
    // seg10:0x6ec6(DGROUP:0x92b0) and the DCs [BYTES seg15:0x2cbe-0x2d64].
    ImagePtr mask = g.mask(body);
    HDC q = square.hdc(), b = body.hdc();
    BitBlt(q, 0, 0, ks, ks, b, 0, 0, SRCCOPY);  // [BYTES seg15:0x2d66-0x2d81]
    if (v.flame_on) {  // [bp-0x3e] = sext ship byte +0xe4 [BYTES seg15:0x2c5e-0x2c63 seg15:0x2d86-0x2d8c]
        const int fx = half16(s - 9);                                // [BYTES seg15:0x2d8f-0x2d9a]
        const int sy = (k == 4 && v.flame_row != 0) ? 0x12 : 0;     // [BYTES seg15:0x2da3-0x2dbb]
        if (v.behind != 0) PatBlt(q, 0, 0, ks, ks, BLACKNESS);      // [BYTES seg15:0x2dbb-0x2dd4]
        flame_pair(q, flame, fx, v.fy, sy);
    }
    if (v.behind != 0) {  // reached with the flame off too [BYTES seg15:0x2d8c seg15:0x2e9b-0x2eda]
        BitBlt(q, 0, 0, ks, ks, mask->hdc(), 0, 0, SRCAND);
        BitBlt(q, 0, 0, ks, ks, b, 0, 0, SRCPAINT);
    }
    if (v.ability == 3 && (!v.left_wing || !v.right_wing) && todo)
        todo("seg15:0x2a08 branch B's wing cut reads the unwritten w8 (ADR 0016 decision 1): skipped",
             "seg15:0x2edf");
    GdiFlush();
    return s;
}

void strip_rebuild(Gfx& g, Surface& strip, int pw, int pc, int s4, const Surface* pad, const Surface* body) {
    HDC d = strip.hdc();
    SetStretchBltMode(d, COLORONCOLOR);                 // the strip DC's set-up [BYTES seg15:0x4cec-0x4db1]
    PatBlt(d, 0, 0, pw, s4 + 0x3e, BLACKNESS);          // [BYTES seg15:0x4de1-0x4e1f] (verified claim 56)
    if (pad) BitBlt(d, 0, s4 + 0x3e - pc - 0x34, pw, pc + 0x34, pad->hdc(), 0, 0, SRCCOPY);
    if (!body || body->height() <= 0) return;
    const int bmh = body->height();
    ImagePtr mask = g.mask(*body);  // [BYTES seg15:0x4e24-0x4f67] (verified claim 57)
    const int x = half16(pw) - half16(s4);
    shadow_blit(d, x, 9, s4, s4, mask->hdc(), 0, 0, bmh, bmh);
    SetStretchBltMode(d, COLORONCOLOR);
    StretchBlt(d, x, 5, s4, s4, mask->hdc(), 0, 0, bmh, bmh, SRCAND);
    StretchBlt(d, x, 5, s4, s4, body->hdc(), 0, 0, bmh, bmh, SRCPAINT);
}

#endif

}  // namespace is::gfx
