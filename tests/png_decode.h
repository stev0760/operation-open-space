// A small PNG reader for the tests that compare the port's pixels with the oracle's
// screenshots: 8-bit greyscale, RGB, RGBA and palette images and 1-, 2- and 4-bit greyscale
// and palette images (ImageMagick writes the GAME OVER shot as a 2-bit colormap), non-interlaced, with the zlib
// inflate (stored, fixed and dynamic Huffman blocks) of RFC 1950/1951 [REF RFC 1951, PNG 1.2].
#pragma once

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace pngtest {

struct Image {
    int w = 0, h = 0;
    std::vector<std::uint32_t> rgb;  // 0x00RRGGBB, top-down
    std::uint32_t at(int x, int y) const { return rgb[static_cast<std::size_t>(y) * w + x]; }
};

namespace detail {

struct Bits {
    const std::uint8_t* p;
    std::size_t n, pos = 0;
    std::uint32_t buf = 0;
    int cnt = 0;
    bool bad = false;
    int bit() {
        if (cnt == 0) {
            if (pos >= n) {
                bad = true;
                return 0;
            }
            buf = p[pos++];
            cnt = 8;
        }
        const int b = buf & 1;
        buf >>= 1;
        --cnt;
        return b;
    }
    int bits(int k) {
        int v = 0;
        for (int i = 0; i < k; ++i) v |= bit() << i;
        return v;
    }
};

struct Huff {
    std::vector<int> count, symbol;
    void build(const int* lengths, int n) {
        count.assign(16, 0);
        symbol.assign(static_cast<std::size_t>(n), 0);
        for (int i = 0; i < n; ++i) ++count[static_cast<std::size_t>(lengths[i])];
        count[0] = 0;
        std::vector<int> offs(16, 0);
        for (int i = 1; i < 16; ++i) offs[static_cast<std::size_t>(i)] = offs[static_cast<std::size_t>(i - 1)] + count[static_cast<std::size_t>(i - 1)];
        for (int i = 0; i < n; ++i)
            if (lengths[i]) symbol[static_cast<std::size_t>(offs[static_cast<std::size_t>(lengths[i])]++)] = i;
    }
    int decode(Bits& b) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= b.bit();
            const int c = count[static_cast<std::size_t>(len)];
            if (code - c < first) return symbol[static_cast<std::size_t>(index + (code - first))];
            index += c;
            first += c;
            first <<= 1;
            code <<= 1;
            if (b.bad) return -1;
        }
        return -1;
    }
};

inline bool inflate(const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>& out) {
    if (in.size() < 2) return false;
    Bits b{in.data() + 2, in.size() - 2};
    static const int lbase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const int lext[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const int dbase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const int dext[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    int last = 0;
    while (!last) {
        last = b.bit();
        const int type = b.bits(2);
        if (b.bad) return false;
        if (type == 0) {
            b.cnt = 0;
            if (b.pos + 4 > b.n) return false;
            const int len = b.p[b.pos] | (b.p[b.pos + 1] << 8);
            b.pos += 4;
            if (b.pos + static_cast<std::size_t>(len) > b.n) return false;
            out.insert(out.end(), b.p + b.pos, b.p + b.pos + len);
            b.pos += static_cast<std::size_t>(len);
            continue;
        }
        Huff lit, dist;
        if (type == 1) {
            int l[288];
            for (int i = 0; i < 144; ++i) l[i] = 8;
            for (int i = 144; i < 256; ++i) l[i] = 9;
            for (int i = 256; i < 280; ++i) l[i] = 7;
            for (int i = 280; i < 288; ++i) l[i] = 8;
            lit.build(l, 288);
            int d[30];
            for (int& x : d) x = 5;
            dist.build(d, 30);
        } else if (type == 2) {
            const int nlen = b.bits(5) + 257, ndist = b.bits(5) + 1, ncode = b.bits(4) + 4;
            static const int order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            int cl[19] = {};
            for (int i = 0; i < ncode; ++i) cl[order[i]] = b.bits(3);
            Huff ch;
            ch.build(cl, 19);
            int lens[320] = {};
            int i = 0;
            while (i < nlen + ndist) {
                const int sym = ch.decode(b);
                if (sym < 0) return false;
                if (sym < 16) {
                    lens[i++] = sym;
                } else {
                    int rep = 0, val = 0;
                    if (sym == 16) {
                        if (i == 0) return false;
                        val = lens[i - 1];
                        rep = 3 + b.bits(2);
                    } else if (sym == 17) {
                        rep = 3 + b.bits(3);
                    } else {
                        rep = 11 + b.bits(7);
                    }
                    if (i + rep > nlen + ndist) return false;
                    while (rep--) lens[i++] = val;
                }
            }
            lit.build(lens, nlen);
            dist.build(lens + nlen, ndist);
        } else {
            return false;
        }
        for (;;) {
            int sym = lit.decode(b);
            if (sym < 0) return false;
            if (sym < 256) {
                out.push_back(static_cast<std::uint8_t>(sym));
            } else if (sym == 256) {
                break;
            } else {
                sym -= 257;
                if (sym >= 29) return false;
                const int len = lbase[sym] + b.bits(lext[sym]);
                const int ds = dist.decode(b);
                if (ds < 0 || ds >= 30) return false;
                const std::size_t d = static_cast<std::size_t>(dbase[ds] + b.bits(dext[ds]));
                if (d > out.size()) return false;
                for (int k = 0; k < len; ++k) out.push_back(out[out.size() - d]);
            }
        }
    }
    return true;
}

inline std::uint32_t be32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

}  // namespace detail

// Reads a PNG; an empty image when the file is missing or not one this reader handles.
inline Image read(const std::string& path) {
    Image img;
    std::ifstream f(path, std::ios::binary);
    if (!f) return img;
    std::vector<std::uint8_t> d((std::istreambuf_iterator<char>(f)), {});
    if (d.size() < 8 || d[1] != 'P' || d[2] != 'N' || d[3] != 'G') return img;
    std::size_t pos = 8;
    int w = 0, h = 0, depth = 0, ctype = 0, interlace = 0;
    std::vector<std::uint32_t> pal;
    std::vector<std::uint8_t> z;
    while (pos + 8 <= d.size()) {
        const std::uint32_t len = detail::be32(&d[pos]);
        const std::string type(reinterpret_cast<const char*>(&d[pos + 4]), 4);
        const std::uint8_t* body = &d[pos + 8];
        if (pos + 12 + len > d.size()) return img;
        if (type == "IHDR") {
            w = static_cast<int>(detail::be32(body));
            h = static_cast<int>(detail::be32(body + 4));
            depth = body[8];
            ctype = body[9];
            interlace = body[12];
        } else if (type == "PLTE") {
            for (std::uint32_t i = 0; i + 2 < len; i += 3)
                pal.push_back((static_cast<std::uint32_t>(body[i]) << 16) | (body[i + 1] << 8) | body[i + 2]);
        } else if (type == "IDAT") {
            z.insert(z.end(), body, body + len);
        } else if (type == "IEND") {
            break;
        }
        pos += 12 + len;
    }
    if (interlace != 0) return img;
    // Sub-byte samples: greyscale or palette at 1, 2 or 4 bits, packed from the high bit; the
    // filters work on whole bytes (bpp 1) [REF PNG 1.2 sections 2.3, 6.3].
    const bool packed = (ctype == 0 || ctype == 3) && (depth == 1 || depth == 2 || depth == 4);
    if (depth != 8 && !packed) return img;
    const int bpp = ctype == 0 ? 1 : ctype == 2 ? 3 : ctype == 3 ? 1 : ctype == 4 ? 2 : ctype == 6 ? 4 : 0;
    if (bpp == 0) return img;
    std::vector<std::uint8_t> raw;
    if (!detail::inflate(z, raw)) return img;
    const std::size_t stride = packed ? (static_cast<std::size_t>(w) * depth + 7) / 8
                                      : static_cast<std::size_t>(w) * bpp;
    if (raw.size() < (stride + 1) * static_cast<std::size_t>(h)) return img;
    std::vector<std::uint8_t> cur(stride), prev(stride, 0);
    img.w = w;
    img.h = h;
    img.rgb.resize(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* row = &raw[(stride + 1) * static_cast<std::size_t>(y)];
        const int filter = row[0];
        for (std::size_t i = 0; i < stride; ++i) {
            const int a = i >= static_cast<std::size_t>(bpp) ? cur[i - bpp] : 0;
            const int b = prev[i];
            const int c = i >= static_cast<std::size_t>(bpp) ? prev[i - bpp] : 0;
            int v = row[1 + i];
            switch (filter) {
            case 1: v += a; break;
            case 2: v += b; break;
            case 3: v += (a + b) / 2; break;
            case 4: {
                const int pp = a + b - c;
                const int pa = pp > a ? pp - a : a - pp, pb = pp > b ? pp - b : b - pp,
                          pc = pp > c ? pp - c : c - pp;
                v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                break;
            }
            default: break;
            }
            cur[i] = static_cast<std::uint8_t>(v);
        }
        if (packed) {
            const int per = 8 / depth, mask = (1 << depth) - 1;
            for (int x = 0; x < w; ++x) {
                const int shift = 8 - depth * (x % per + 1);
                int v = (cur[static_cast<std::size_t>(x / per)] >> shift) & mask;
                std::uint32_t c = 0;
                if (ctype == 3) {
                    c = static_cast<std::size_t>(v) < pal.size() ? pal[static_cast<std::size_t>(v)] : 0;
                } else {
                    v = v * 255 / mask;
                    c = (static_cast<std::uint32_t>(v) << 16) | (v << 8) | v;
                }
                img.rgb[static_cast<std::size_t>(y) * w + x] = c;
            }
            std::swap(cur, prev);
            continue;
        }
        for (int x = 0; x < w; ++x) {
            const std::uint8_t* px = &cur[static_cast<std::size_t>(x) * bpp];
            std::uint32_t c = 0;
            switch (ctype) {
            case 0: case 4: c = (px[0] << 16) | (px[0] << 8) | px[0]; break;
            case 2: case 6: c = (static_cast<std::uint32_t>(px[0]) << 16) | (px[1] << 8) | px[2]; break;
            case 3: c = px[0] < pal.size() ? pal[px[0]] : 0; break;
            default: break;
            }
            img.rgb[static_cast<std::size_t>(y) * w + x] = c;
        }
        std::swap(cur, prev);
    }
    return img;
}

}  // namespace pngtest
