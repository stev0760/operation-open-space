#include "desk/scene.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace is::desk {

namespace {

// seg3:0x130: the signed long divide, truncating toward zero; the callers map a zero divisor.
long tdiv(long a, long b) { return b == 0 ? 0 : a / b; }
int s16(long v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xffff)); }

int word_at(const std::uint8_t* p, std::size_t o) {
    return static_cast<std::int16_t>(p[o] | (p[o + 1] << 8));
}
std::int32_t dword_at(const std::uint8_t* p, std::size_t o) {
    return static_cast<std::int32_t>(p[o] | (p[o + 1] << 8) | (p[o + 2] << 16) |
                                     (static_cast<std::uint32_t>(p[o + 3]) << 24));
}

// The record word at +5 + 2·code that the solver reads for codes 1-4 (and a line's end-point
// codes above 5): +7 SX, +9 SY, +0xb SZ, +0xd the X position code, and on through +0x17.
int field_word(const Record& r, int offset) {
    switch (offset) {
    case 0x01: return r.pos[0];
    case 0x03: return r.pos[1];
    case 0x05: return r.pos[2];
    case 0x07: return r.size[0];
    case 0x09: return r.size[1];
    case 0x0b: return r.size[2];
    case 0x0d: return r.pos_code[0];
    case 0x0f: return r.pos_code[1];
    case 0x11: return r.pos_code[2];
    case 0x13: return r.size_code[0];
    case 0x15: return r.size_code[1];
    case 0x17: return r.size_code[2];
    case 0x19: return r.string_id;
    default: return 0;  // [OPEN] a code that reaches past +0x19 reads the byte fields
    }
}

int peak_channel(const Rgb& c) {
    // seg8:0x14e2-0x151d: the channel with the largest value, the first on a tie [OPEN: the
    // comparison's strictness; it matters only for the greys, where every channel is equal].
    int k = 0, best = c.r;
    if (c.g > best) k = 1, best = c.g;
    if (c.b > best) k = 2;
    return k;
}
int channel(const Rgb& c, int k) { return k == 0 ? c.r : k == 1 ? c.g : c.b; }

}  // namespace

Scene decode(const fmt::Scene6600& raw) {
    Scene s;
    const std::uint8_t* h = raw.header.data();
    Header& hd = s.header;
    hd.raw = raw.header;
    for (int i = 0; i < 3; ++i) {
        hd.colour[i] = {static_cast<std::uint8_t>(word_at(h, 6 * i)),
                        static_cast<std::uint8_t>(word_at(h, 6 * i + 2)),
                        static_cast<std::uint8_t>(word_at(h, 6 * i + 4))};
        hd.dark[i] = {static_cast<std::uint8_t>(word_at(h, 0x2c + 6 * i)),
                      static_cast<std::uint8_t>(word_at(h, 0x2e + 6 * i)),
                      static_cast<std::uint8_t>(word_at(h, 0x30 + 6 * i))};
        hd.divisor[i] = word_at(h, 0x26 + 2 * i);
    }
    hd.count = word_at(h, 0x24);
    hd.mode_x = h[0x50];
    hd.mode_y = h[0x51];
    hd.d5c = dword_at(h, 0x5c);
    hd.d60 = dword_at(h, 0x60);
    hd.d64 = dword_at(h, 0x64);
    hd.d68 = dword_at(h, 0x68);
    for (const auto& b : raw.records) {
        const std::uint8_t* p = b.data();
        Record r;
        r.resolved = p[0] != 0;
        for (int k = 0; k < 3; ++k) {
            r.pos[k] = word_at(p, 1 + 2 * k);
            r.size[k] = word_at(p, 7 + 2 * k);
            r.pos_code[k] = word_at(p, 0xd + 2 * k);
            r.size_code[k] = word_at(p, 0x13 + 2 * k);
        }
        r.string_id = word_at(p, 0x19);
        r.parent = p[0x1b];
        r.parent2 = p[0x1c];
        r.colour = static_cast<std::int8_t>(p[0x1d]);
        r.type = p[0x1e];
        r.slot = static_cast<std::int8_t>(p[0x1f]);
        r.brightness = p[0x20];
        r.pattern = p[0x21];
        s.records.push_back(r);
    }
    return s;
}

int Projection::scale(int y) const {
    // s = d4c·d28 / (Y·d28/1000 + d4c) [BYTES ISPACE.DLL seg8:0x2920]
    const long a = tdiv(static_cast<long>(y) * d28, 1000);
    return static_cast<int>(tdiv(static_cast<long>(d4c) * d28, a + d4c));
}
int Projection::yb(int s) const { return static_cast<int>(tdiv(static_cast<long>(s) * d48, 2000)) + d2e; }
int Projection::centre(int y) const {
    return static_cast<int>(tdiv(static_cast<long>(c6c - y) * 1000, d44)) + static_cast<int>(tdiv(W - d28, 2));
}

// seg8:0x0 [BYTES ISPACE.DLL seg8:0x1b seg8:0xd0 seg8:0x11b seg8:0x162 seg8:0x18c seg8:0x1f5
// seg8:0x254 seg8:0x27f].
Projection projection(const Header& h, int W, int H, int cx, int cy) {
    Projection p;
    p.W = W;
    p.H = H;
    p.cx = cx;
    p.cy = cy;
    p.d28 = s16(tdiv(static_cast<long>(H) << 2, 3));
    auto mode = [&](int m, std::int32_t v) -> int {
        if (m == 6) return s16(tdiv(static_cast<long>(v) * W, 1000));
        if (m == 7) return s16(tdiv(static_cast<long>(v) * H, 1000));
        if (m == 8) return s16(v);
        return 0;  // [OPEN] another mode leaves the word as it was
    };
    p.d2c = mode(h.mode_x, h.d5c);
    p.d2e = mode(h.mode_y, h.d60);
    p.d4c = s16(tdiv(static_cast<long>(h.d64) * p.d28, 1000));
    p.c6c = s16(tdiv(static_cast<long>(h.d68) * H, 1000));
    p.d48 = s16(tdiv(static_cast<long>(p.c6c - p.d2e) * 2000, p.d28));
    long lc = tdiv(p.d28 - W, 2) + p.d2c;
    if (lc == 0) lc = 1;
    p.d44 = s16(tdiv(static_cast<long>(p.c6c - p.d2e) * 1000, lc));
    if (p.d44 == 0) p.d44 = 1;
    if (p.d48 == 0) p.d48 = 1;
    return p;
}

// seg8:0x1641 [BYTES ISPACE.DLL seg8:0x1641 seg8:0x169b seg8:0x1791 seg8:0x179e seg8:0x17a8
// seg8:0x17af].
bool condition_holds(const Header& h, int v, const Conditions& c) {
    const int at = 0x48 - v;
    if (at < 0 || at >= static_cast<int>(h.raw.size())) return false;
    switch (h.raw[at]) {
    case 1: return c.a8 == 1;
    case 2: return c.c70 > 3;
    case 3: return c.async_wave;
    case 4: return c.c70 < 4;
    case 5: return c.stereo;
    default: return false;
    }
}

// seg8:0xa36's resolve passes [BYTES ISPACE.DLL seg8:0xa57 seg8:0xa60 seg8:0xa76 seg8:0xa93
// seg8:0xb00 seg8:0xb5d seg8:0xc41 seg8:0xc5d seg8:0xd64 seg8:0xdaa seg8:0xe00 seg8:0xe40
// seg8:0xfd9 seg8:0x1036 seg8:0x120a seg8:0x12ef seg8:0x13d0 seg8:0x13dc].
int solve(Scene& s, const Projection& p) {
    auto& R = s.records;
    const int n = static_cast<int>(R.size());
    int max_slot = -1;
    if (n == 0) return max_slot;
    for (bool changed = true; changed;) {
        changed = false;
        for (int i = 1; i < n; ++i) {
            Record& r = R[i];
            if (r.resolved || r.parent >= n || !R[r.parent].resolved) continue;
            const int t = r.type;
            if (t < 1 || t > 5) continue;
            if (t == 2 && (r.parent2 >= n || !R[r.parent2].resolved)) continue;
            const Record& pa = R[r.parent];
            for (int k = 0; k < 3; ++k) {
                // phase 1: the position
                int c = r.pos_code[k];
                bool has = true;
                long ref = 0;
                if (c == 0) ref = pa.size[k];
                else if (c == 5) ref = R[0].size[0];
                else if (c >= 1 && c < 5) ref = field_word(pa, 5 + 2 * c);
                else has = false;
                if (has && ref != -1) {
                    r.pos[k] = s16(tdiv(r.pos[k] * ref, 1000) + pa.pos[k]);
                    if (k == 0) r.pos[0] = s16(r.pos[0] - tdiv(pa.size[0], 2));
                    if (k == 2) r.pos[2] = s16(r.pos[2] + pa.size[2]);
                }
                c = r.size_code[k];
                if (t != 2) {
                    has = true;
                    if (c == 0) ref = pa.size[0];
                    else if (k != 0 && c == 4) ref = r.size[0];
                    else if (c == 5) ref = R[0].size[0];
                    else if (c >= 1 && c < 6) ref = field_word(pa, 5 + 2 * c);
                    else has = false;
                    if (has && ref != -1) r.size[k] = s16(tdiv(r.size[k] * ref, 1000));
                } else {
                    const Record& q = R[r.parent2];
                    if (c == 0) ref = q.size[k];
                    else if (c == 5) ref = R[0].size[0];
                    else ref = field_word(q, 5 + 2 * c);
                    r.size[k] = s16(tdiv(r.size[k] * ref, 1000) + q.pos[k]);
                    if (k == 0) r.size[0] = s16(r.size[0] - tdiv(q.size[0], 2));
                    if (k == 2) r.size[2] = s16(r.size[2] + q.size[2]);
                }
            }
            // phase 2: screen-unit codes
            const int sc = p.scale(r.pos[1]);
            const int yb = p.yb(sc);
            const long half = tdiv(p.W - p.d28, 2);
            const long off = tdiv(static_cast<long>(p.c6c - yb) * 1000, p.d44);
            auto per_mille = [&](long v, int c) -> long {
                switch (c) {
                case 6: return tdiv(v * p.W, 1000);
                case 7: return tdiv(v * p.H, 1000);
                case 8: return v;
                case 9: return tdiv(v * p.cx, 1000);
                case 10: return tdiv(v * p.cy, 1000);
                default: return std::max<long>(tdiv(v * p.H, 1000), c);
                }
            };
            if (r.pos_code[0] >= 6 && sc != 0) {
                const long x = per_mille(r.pos[0], r.pos_code[0]);
                r.pos[0] = s16(tdiv((x - off - half) * 1000, sc));
            }
            if (r.pos_code[2] >= 6 && sc != 0) {
                const long y = per_mille(r.pos[2], r.pos_code[2]);
                r.pos[2] = s16(tdiv((yb - y) * 1000, sc));
            }
            for (int k : {0, 2}) {
                const int c = r.size_code[k];
                if (c < 6 || sc == 0) continue;
                const long v = r.size[k];
                long m = 0;
                switch (c) {
                case 6: m = v * p.W; break;
                case 7: m = v * p.H; break;
                case 8: m = v * 1000; break;
                case 9: m = v * p.cx; break;
                case 10: m = v * p.cy; break;
                default: m = std::max<long>(v * p.H, static_cast<long>(c) * 1000); break;
                }
                r.size[k] = s16(tdiv(m, sc));
            }
            r.resolved = true;
            max_slot = std::max(max_slot, r.slot);
            changed = true;
        }
    }
    return max_slot;
}

namespace {
Face face_at(const Record& r, const Projection& p, int y) {
    Face f;
    const int sc = p.scale(y);
    f.yb = p.yb(sc);
    const int x = static_cast<int>(tdiv(static_cast<long>(r.pos[0]) * sc, 1000));
    const int z = static_cast<int>(tdiv(static_cast<long>(r.pos[2]) * sc, 1000));
    f.height = static_cast<int>(tdiv(static_cast<long>(r.size[2]) * sc, 1000));
    f.width = static_cast<int>(tdiv(static_cast<long>(r.size[0]) * sc, 1000));
    const int cx = p.centre(f.yb) + x;
    f.left = cx - static_cast<int>(tdiv(f.width, 2));
    f.top = f.yb - f.height - z;
    return f;
}
}  // namespace

// [BYTES ISPACE.DLL seg8:0x2920 seg8:0x2966 seg8:0x298b seg8:0x2a34 seg8:0x2a6f seg8:0x2a84
// seg8:0x2aaa seg8:0x2ac5]
Face front_face(const Record& r, const Projection& p) { return face_at(r, p, r.pos[1]); }
Face back_face(const Record& r, const Projection& p) { return face_at(r, p, r.pos[1] + r.size[1]); }

// seg8:0x27f0 above 8 bpp [BYTES ISPACE.DLL seg8:0x27f0 seg8:0x2802].
Rgb shade_rgb(const Rgb& colour, int level) {
    level = std::clamp(level, 0, 255);
    const int k = peak_channel(colour);
    const int peak = channel(colour, k);
    auto ch = [&](int j) -> std::uint8_t {
        if (j == k || peak == 0) return static_cast<std::uint8_t>(j == k ? level : 0);
        return static_cast<std::uint8_t>(std::clamp(channel(colour, j) * level / peak, 0, 255));
    };
    return {ch(0), ch(1), ch(2)};
}

std::vector<std::vector<Rgb>> shade_ramps(const Header& h) {
    std::vector<std::vector<Rgb>> out;
    for (int i = 0; i < std::min(h.count, 3); ++i) {
        const Rgb c = h.colour[i], d = h.dark[i];
        const int k = peak_channel(c);
        const int div = h.divisor[i] == 0 ? 1 : h.divisor[i];
        const int n = (channel(c, k) - channel(d, k)) / div;
        std::vector<Rgb> ramp;
        for (int j = 0; j < n; ++j) {
            auto mix = [&](int ch) {
                return static_cast<std::uint8_t>(channel(d, ch) + (channel(c, ch) - channel(d, ch)) * j / n);
            };
            ramp.push_back({mix(0), mix(1), mix(2)});
        }
        out.push_back(std::move(ramp));
    }
    return out;
}

namespace {

struct Drawer {
    const Scene& s;
    const Projection& p;
    Render& out;
    int index = 0;

    int level_of(const Record& r) const {
        const Rgb& c = s.header.colour[std::clamp(r.colour, 0, 2)];
        return channel(c, peak_channel(c)) * r.brightness / 50;
    }
    Rgb colour(const Record& r, int level) const {
        return shade_rgb(s.header.colour[std::clamp(r.colour, 0, 2)], level);
    }
    // (Yb - d2e)/(c6c - d2e) as a fraction num/den.
    long num(int yb) const { return yb - p.d2e; }
    long den() const { return p.c6c - p.d2e == 0 ? 1 : p.c6c - p.d2e; }

    void fill(int x, int y, int w, int h, Rgb c) {
        if (w <= 0 || h <= 0) return;
        Prim q;
        q.kind = Prim::kFill;
        q.x = x, q.y = y, q.w = w, q.h = h, q.c = c, q.record = index;
        out.prims.push_back(q);
    }
    void line(int x0, int y0, int x1, int y1, int width, Rgb c) {
        Prim q;
        q.kind = Prim::kLine;
        q.x = x0, q.y = y0, q.x1 = x1, q.y1 = y1, q.width = width, q.c = c, q.record = index;
        out.prims.push_back(q);
    }
    // The strip shade: level × 2 × (Yb − d2e)²/(c6c − d2e)² >> 1 [OPEN ISPACE.DLL seg8:0x1fe6:
    // the exact arithmetic; this form matches the oracle's desk top within a few levels].
    Rgb strip_colour(const Record& r, int level, int yb) const {
        const long n = num(yb), d = den();
        const long v = ((2L * level * n * n) / (d * d)) >> 1;
        return colour(r, static_cast<int>(v));
    }

    // seg8:0x1fe6 for a top or bottom face: rows from the back edge y0 (xl0..xr0, depth yb0)
    // to the front edge y1 (xl1..xr1, yb1), 5-px strips with one brush each [OPEN
    // ISPACE.DLL seg8:0x2267-0x25d5: the strip boundaries and the 1-px staircase ends are
    // approximated by exact per-row spans].
    void rows(const Record& r, int level, int y0, int xl0, int xr0, int yb0, int y1, int xl1,
              int xr1, int yb1) {
        if (y1 == y0) return;
        const int dir = y1 > y0 ? 1 : -1;
        const int extent = std::abs(y1 - y0);
        const int thick = extent < 20 ? 2 : extent < 35 ? 3 : extent < 45 ? 4 : 5;
        for (int a = 0; a < extent; a += thick) {
            const int n = std::min(thick, extent - a);
            const int yb = yb0 + (yb1 - yb0) * a / extent;
            const Rgb c = strip_colour(r, level, yb);
            for (int j = 0; j < n; ++j) {
                const int k = a + j;
                const int y = y0 + dir * k;
                const int xl = xl0 + (xl1 - xl0) * k / extent;
                const int xr = xr0 + (xr1 - xr0) * k / extent;
                fill(std::min(xl, xr), y, std::abs(xr - xl), 1, c);
            }
        }
    }
    // seg8:0x1fe6 for a side face: columns from the front edge x0 (top t0, bottom b0) to the
    // back edge x1 [OPEN as rows()].
    void columns(const Record& r, int level, int x0, int t0, int b0, int yb0, int x1, int t1,
                 int b1, int yb1) {
        if (x1 == x0) return;
        const int dir = x1 > x0 ? 1 : -1;
        const int extent = std::abs(x1 - x0);
        const int thick = extent < 20 ? 2 : extent < 35 ? 3 : extent < 45 ? 4 : 5;
        for (int a = 0; a < extent; a += thick) {
            const int n = std::min(thick, extent - a);
            const int yb = yb0 + (yb1 - yb0) * a / extent;
            const Rgb c = strip_colour(r, level, yb);
            for (int j = 0; j < n; ++j) {
                const int k = a + j;
                const int x = dir > 0 ? x0 + k : x0 - 1 - k;
                const int t = t0 + (t1 - t0) * k / extent;
                const int b = b0 + (b1 - b0) * k / extent;
                fill(x, std::min(t, b), 1, std::abs(b - t), c);
            }
        }
    }

    // seg8:0x28de, the box, and an approximation of seg8:0x3874 (type 3, the monitor's foot
    // disc and neck) as a box whose front face is shaded per column like a cylinder [OPEN
    // ISPACE.DLL seg8:0x3874].
    void box(const Record& r, bool cylinder) {
        const Face f = front_face(r, p);
        const Face b = back_face(r, p);
        Rect slot{f.left, f.top, 0, 0};
        int left = f.left, width = f.width;
        if (r.colour >= 0 && b.left < f.left) {  // [BYTES ISPACE.DLL seg8:0x3464 seg8:0x3475]
            left += 2;
            width -= 2;
        }
        slot = {left, f.top, left + width, f.top + f.height + 1};
        if (r.slot >= 0 && r.slot < static_cast<int>(out.slots.size())) out.slots[r.slot] = slot;
        if (r.colour < 0) return;  // record 60: the slot only [BYTES ISPACE.DLL seg8:0x188d]
        const int L = level_of(r);
        // The other faces [BYTES ISPACE.DLL seg8:0x2c2a seg8:0x2c50 seg8:0x2cd9 seg8:0x2d2f
        // seg8:0x2d6c seg8:0x306a]; drawn before the front face, which seg8:0x3423 fills last.
        const int fb = f.top + f.height, bb = b.top + b.height;
        if (f.top > p.d2e) {
            rows(r, L, b.top, b.left, b.right(), b.yb, f.top, f.left, f.right(), f.yb);
        } else if (fb < p.d2e) {
            rows(r, L, bb, b.left, b.right(), b.yb, fb, f.left, f.right(), f.yb);
        }
        if (b.right() > f.right()) {
            columns(r, L * 3 / 4, f.right(), f.top, fb + 1, f.yb, b.right(), b.top, bb + 1, b.yb);
        } else if (b.left < f.left) {
            columns(r, L * 3 / 4, f.left, f.top, fb + 1, f.yb, b.left, b.top, bb + 1, b.yb);
        }
        // The front face: PatBlt PATCOPY (left, top, width, SZ' + 1) [BYTES ISPACE.DLL
        // seg8:0x3423 seg8:0x345f seg8:0x3544 seg8:0x357c].
        const long shade = ((2L * L * 12 / 13) * num(f.yb) / den()) >> 1;
        if (!cylinder) {
            fill(left, f.top, width, f.height + 1, colour(r, static_cast<int>(shade)));
            return;
        }
        for (int x = 0; x < width; ++x) {
            const double u = (2.0 * x + 1) / width - 1.0;
            const double k = 0.31 + 0.69 * std::sqrt(std::max(0.0, 1.0 - u * u));
            fill(left + x, f.top, 1, f.height + 1, colour(r, static_cast<int>(shade * k)));
        }
    }

    // seg8:0x5470 twice and seg8:0x5632 [BYTES ISPACE.DLL seg8:0x1a6c seg8:0x5470 seg8:0x5566
    // seg8:0x56a3 seg8:0x579b]. The shade per step, (2L × (Yi − d2e)²/(c6c − d2e)²) >> 1 with
    // Yi the interpolated depth, is [OPEN ISPACE.DLL seg8:0x5632].
    void segment(const Record& r) {
        auto project = [&](int x, int y, int z, int* sx, int* sy, int* yb) {
            const int sc = p.scale(y);
            *yb = p.yb(sc);
            *sx = p.centre(*yb) + static_cast<int>(tdiv(static_cast<long>(x) * sc, 1000));
            *sy = *yb - static_cast<int>(tdiv(static_cast<long>(z) * sc, 1000));
        };
        int x0, y0, b0, x1, y1, b1;
        project(r.pos[0], r.pos[1], r.pos[2], &x0, &y0, &b0);
        project(r.size[0], r.size[1], r.size[2], &x1, &y1, &b1);
        if (y1 < y0) std::swap(x0, x1), std::swap(y0, y1), std::swap(b0, b1);
        const int L = level_of(r);
        if (y0 == y1) {
            fill(std::min(x0, x1), y0, std::abs(x1 - x0), 2, strip_colour(r, L, b0));
            return;
        }
        const int dy = y1 - y0;
        int px = x0, py = y0;
        for (int a = 5; ; a += 5) {
            const int k = std::min(a, dy);
            const int nx = x0 + (x1 - x0) * k / dy, ny = y0 + k;
            const int yb = b0 + (b1 - b0) * (a - 5) / dy;
            line(px, py, nx, ny, 2, strip_colour(r, L, yb));
            px = nx, py = ny;
            if (k == dy) break;
        }
    }
};

}  // namespace

// Ordinal 32 [BYTES ISPACE.DLL seg8:0x0 seg8:0x44 seg8:0x2d0 seg8:0x31b seg8:0x36b
// seg8:0x1afb seg8:0x1b31 seg8:0x1b38 seg8:0x1b66 seg8:0x15ec].
Render render(const fmt::Scene6600& raw, int W, int H, int cx, int cy, const Conditions& c) {
    Render out;
    Scene s = decode(raw);
    out.proj = projection(s.header, W, H, cx, cy);
    out.ramps = shade_ramps(s.header);
    if (!s.records.empty()) s.records[0].resolved = true;
    out.max_slot = solve(s, out.proj);
    out.slots.assign(std::max(out.max_slot + 1, 0), Rect{});
    Drawer d{s, out.proj, out};
    // The draw loop: records 1 ... n − 1 in index order, no depth sort [BYTES ISPACE.DLL
    // seg8:0x1b7c seg8:0x1baa seg8:0x1afb].
    for (std::size_t i = 1; i < s.records.size(); ++i) {
        const Record& r = s.records[i];
        d.index = static_cast<int>(i);
        if (r.colour < 0 && r.slot < 0) continue;
        switch (r.type) {
        case 1:
            if (r.slot < -1 && !condition_holds(s.header, r.slot, c)) continue;
            d.box(r, false);
            break;
        case 2:
            if (r.colour >= 0) d.segment(r);
            break;
        case 3:
            if (r.colour >= 0) d.box(r, true);
            break;
        default: break;  // types 4 and 5 (seg8:0x50ec, seg8:0x4206): scene 100 has none
        }
    }
    return out;
}

}  // namespace is::desk
