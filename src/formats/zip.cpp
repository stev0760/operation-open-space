#include "formats/zip.h"

#include <array>
#include <fstream>
#include <iterator>

namespace is::fmt {

// ---- CRC-32

namespace {

std::array<std::uint32_t, 256> make_crc_table() {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        t[i] = c;
    }
    return t;
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* p, std::size_t n, std::uint32_t crc) {
    static const std::array<std::uint32_t, 256> table = make_crc_table();
    crc = ~crc;
    for (std::size_t i = 0; i < n; ++i) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

// ---- inflate (RFC 1951)

namespace {

class BitReader {
public:
    BitReader(const std::uint8_t* p, std::size_t n) : p_(p), n_(n) {}
    // n bits, least significant first (RFC 1951 3.1.1).
    std::uint32_t bits(int count) {
        while (have_ < count) {
            if (pos_ >= n_) throw FormatError("deflate: stream truncated");
            buf_ |= std::uint32_t{p_[pos_++]} << have_;
            have_ += 8;
        }
        const std::uint32_t v = buf_ & ((count == 32) ? 0xFFFFFFFFu : ((1u << count) - 1));
        buf_ >>= count;
        have_ -= count;
        return v;
    }
    void align() {
        buf_ = 0;
        have_ = 0;
    }
    std::uint8_t byte() {
        if (pos_ >= n_) throw FormatError("deflate: stream truncated");
        return p_[pos_++];
    }

private:
    const std::uint8_t* p_;
    std::size_t n_;
    std::size_t pos_ = 0;
    std::uint32_t buf_ = 0;
    int have_ = 0;
};

// Canonical Huffman decoding table (RFC 1951 3.2.2): counts per length and the symbols in
// code order, decoded one bit at a time (the codes are read MSB first).
struct Huffman {
    std::array<std::uint16_t, 16> count{};
    std::vector<std::uint16_t> symbol;

    void build(const std::uint8_t* lengths, int n) {
        count.fill(0);
        for (int i = 0; i < n; ++i) ++count[lengths[i]];
        count[0] = 0;
        int left = 1;
        for (int len = 1; len < 16; ++len) {
            left <<= 1;
            left -= count[len];
            if (left < 0) throw FormatError("deflate: over-subscribed code");
        }
        std::array<std::uint16_t, 16> offs{};
        for (int len = 1; len < 15; ++len) offs[len + 1] = offs[len] + count[len];
        symbol.assign(n, 0);
        for (int i = 0; i < n; ++i)
            if (lengths[i] != 0) symbol[offs[lengths[i]]++] = static_cast<std::uint16_t>(i);
    }

    int decode(BitReader& br) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= static_cast<int>(br.bits(1));
            const int c = count[len];
            if (code - c < first) return symbol[index + (code - first)];
            index += c;
            first += c;
            first <<= 1;
            code <<= 1;
        }
        throw FormatError("deflate: bad code");
    }
};

constexpr std::array<std::uint16_t, 29> kLenBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,
                                                    15, 17, 19, 23, 27, 31, 35, 43, 51,  59,
                                                    67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<std::uint8_t, 29> kLenExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<std::uint16_t, 30> kDistBase = {
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<std::uint8_t, 30> kDistExtra = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                     6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void over_limit() { throw FormatError("deflate: output past the entry's size"); }

void inflate_block(BitReader& br, const Huffman& lit, const Huffman& dist,
                   std::vector<std::uint8_t>& out, std::size_t limit) {
    for (;;) {
        const int sym = lit.decode(br);
        if (sym < 256) {
            if (out.size() >= limit) over_limit();
            out.push_back(static_cast<std::uint8_t>(sym));
        } else if (sym == 256) {
            return;
        } else {
            const int li = sym - 257;
            if (li >= 29) throw FormatError("deflate: bad length symbol");
            const std::size_t len = kLenBase[li] + br.bits(kLenExtra[li]);
            const int di = dist.decode(br);
            if (di >= 30) throw FormatError("deflate: bad distance symbol");
            const std::size_t d = kDistBase[di] + br.bits(kDistExtra[di]);
            if (d > out.size()) throw FormatError("deflate: distance before the start");
            if (len > limit - out.size()) over_limit();
            const std::size_t from = out.size() - d;
            for (std::size_t k = 0; k < len; ++k) out.push_back(out[from + k]);
        }
    }
}

}  // namespace

std::vector<std::uint8_t> inflate(const std::uint8_t* p, std::size_t n, std::size_t size_hint,
                                  std::size_t limit) {
    std::vector<std::uint8_t> out;
    out.reserve(size_hint < limit ? size_hint : limit);
    BitReader br(p, n);
    Huffman fixed_lit, fixed_dist;
    bool fixed_built = false;
    for (bool last = false; !last;) {
        last = br.bits(1) != 0;
        const std::uint32_t type = br.bits(2);
        if (type == 0) {  // stored (RFC 1951 3.2.4)
            br.align();
            const std::uint16_t len = static_cast<std::uint16_t>(br.byte() | (br.byte() << 8));
            const std::uint16_t nlen = static_cast<std::uint16_t>(br.byte() | (br.byte() << 8));
            if (static_cast<std::uint16_t>(~nlen) != len) throw FormatError("deflate: bad stored length");
            if (len > limit - out.size()) over_limit();
            for (std::uint32_t k = 0; k < len; ++k) out.push_back(br.byte());
        } else if (type == 1) {  // fixed codes (RFC 1951 3.2.6)
            if (!fixed_built) {
                std::array<std::uint8_t, 288> l{};
                for (int i = 0; i < 144; ++i) l[i] = 8;
                for (int i = 144; i < 256; ++i) l[i] = 9;
                for (int i = 256; i < 280; ++i) l[i] = 7;
                for (int i = 280; i < 288; ++i) l[i] = 8;
                fixed_lit.build(l.data(), 288);
                std::array<std::uint8_t, 30> dl{};
                dl.fill(5);
                fixed_dist.build(dl.data(), 30);
                fixed_built = true;
            }
            inflate_block(br, fixed_lit, fixed_dist, out, limit);
        } else if (type == 2) {  // dynamic codes (RFC 1951 3.2.7)
            const int hlit = static_cast<int>(br.bits(5)) + 257;
            const int hdist = static_cast<int>(br.bits(5)) + 1;
            const int hclen = static_cast<int>(br.bits(4)) + 4;
            if (hlit > 286 || hdist > 30) throw FormatError("deflate: bad code counts");
            static constexpr std::array<std::uint8_t, 19> order = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                                   11, 4,  12, 3, 13, 2, 14, 1, 15};
            std::array<std::uint8_t, 19> cl{};
            for (int i = 0; i < hclen; ++i) cl[order[i]] = static_cast<std::uint8_t>(br.bits(3));
            Huffman clh;
            clh.build(cl.data(), 19);
            std::array<std::uint8_t, 320> lens{};
            for (int i = 0; i < hlit + hdist;) {
                const int sym = clh.decode(br);
                if (sym < 16) {
                    lens[i++] = static_cast<std::uint8_t>(sym);
                    continue;
                }
                std::uint8_t v = 0;
                int rep = 0;
                if (sym == 16) {
                    if (i == 0) throw FormatError("deflate: repeat with no previous length");
                    v = lens[i - 1];
                    rep = 3 + static_cast<int>(br.bits(2));
                } else if (sym == 17) {
                    rep = 3 + static_cast<int>(br.bits(3));
                } else {
                    rep = 11 + static_cast<int>(br.bits(7));
                }
                if (i + rep > hlit + hdist) throw FormatError("deflate: lengths overrun");
                while (rep--) lens[i++] = v;
            }
            if (lens[256] == 0) throw FormatError("deflate: no end-of-block code");
            Huffman lit, dist;
            lit.build(lens.data(), hlit);
            dist.build(lens.data() + hlit, hdist);
            inflate_block(br, lit, dist, out, limit);
        } else {
            throw FormatError("deflate: reserved block type");
        }
    }
    return out;
}

// ---- the archive

namespace {

constexpr std::uint32_t kSigLocal = 0x04034b50, kSigCentral = 0x02014b50, kSigEnd = 0x06054b50;

}  // namespace

ZipArchive ZipArchive::load(const std::filesystem::path& path) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) throw FormatError("zip: cannot open " + path_utf8(path));
    if (size > kZipMaxBytes) throw FormatError("zip: " + path_utf8(path) + " is too large");
    std::ifstream f(path, std::ios::binary);
    if (!f) throw FormatError("zip: cannot open " + path_utf8(path));
    std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return parse(std::move(b));
}

ZipArchive ZipArchive::parse(std::vector<std::uint8_t> bytes) {
    ZipArchive z;
    z.data_ = std::move(bytes);
    const std::vector<std::uint8_t>& d = z.data_;
    // End of central directory: 22 bytes plus a comment of at most 65535 (APPNOTE 4.3.16).
    if (d.size() < 22) throw FormatError("zip: too short");
    std::size_t eocd = std::string::npos;
    const std::size_t lo = d.size() > 22 + 0xFFFF ? d.size() - 22 - 0xFFFF : 0;
    for (std::size_t i = d.size() - 22 + 1; i-- > lo;) {
        Reader r(d.data(), d.size(), i);
        if (r.u32() == kSigEnd) {
            Reader c(d.data(), d.size(), i + 20);
            if (i + 22 + c.u16() == d.size()) {
                eocd = i;
                break;
            }
        }
    }
    if (eocd == std::string::npos) throw FormatError("zip: no end of central directory");
    Reader e(d.data(), d.size(), eocd + 4);
    const std::uint16_t disk = e.u16(), cd_disk = e.u16();
    const std::uint16_t n_here = e.u16(), n_total = e.u16();
    const std::uint32_t cd_size = e.u32(), cd_off = e.u32();
    if (disk != 0 || cd_disk != 0 || n_here != n_total) throw FormatError("zip: multi-disk archive");
    if (n_total == 0xFFFF || cd_off == 0xFFFFFFFFu) throw FormatError("zip: ZIP64 is not read");
    if (cd_off > eocd || cd_size > eocd - cd_off) throw FormatError("zip: central directory out of range");
    Reader r(d.data(), eocd, cd_off);
    for (std::uint16_t k = 0; k < n_total; ++k) {
        if (r.u32() != kSigCentral) throw FormatError("zip: bad central directory entry");
        ZipEntry en;
        r.skip(4);  // version made by, version needed
        en.flags = r.u16();
        en.method = r.u16();
        en.dos_time = r.u16();
        en.dos_date = r.u16();
        en.crc = r.u32();
        en.csize = r.u32();
        en.usize = r.u32();
        const std::uint16_t nlen = r.u16(), xlen = r.u16(), clen = r.u16();
        r.skip(8);  // disk start, internal attributes, external attributes
        en.local_offset = r.u32();
        if (nlen > r.remaining()) throw FormatError("zip: name past end");
        en.name.assign(reinterpret_cast<const char*>(r.ptr()), nlen);
        r.skip(nlen);
        r.skip(xlen);
        r.skip(clen);
        en.utf8 = (en.flags & 0x800) != 0;
        if (en.local_offset >= cd_off) throw FormatError("zip: local header of " + en.name + " out of range");
        z.entries_.push_back(std::move(en));
    }
    return z;
}

std::vector<std::uint8_t> ZipArchive::extract(const ZipEntry& e) const {
    if (e.flags & 1) throw FormatError("zip: " + e.name + " is encrypted");
    if (e.local_offset >= data_.size()) throw FormatError("zip: local header of " + e.name + " out of range");
    Reader r(data_.data(), data_.size(), e.local_offset);
    if (r.u32() != kSigLocal) throw FormatError("zip: bad local header for " + e.name);
    r.skip(2);  // version needed
    const std::uint16_t lflags = r.u16(), lmethod = r.u16();
    r.skip(16);  // time, date, crc, sizes: the central directory's values are used (flag bit 3)
    const std::uint16_t nlen = r.u16(), xlen = r.u16();
    // The local header must agree with the central directory on what the data are [DESIGN].
    if (lmethod != e.method || (lflags & 1) != (e.flags & 1)) throw FormatError("zip: local header of " + e.name + " disagrees");
    if (nlen > r.remaining()) throw FormatError("zip: name of " + e.name + " past end");
    if (std::string(reinterpret_cast<const char*>(r.ptr()), nlen) != e.name)
        throw FormatError("zip: local name of " + e.name + " disagrees");
    r.skip(nlen);
    r.skip(xlen);
    if (e.csize > r.remaining()) throw FormatError("zip: data of " + e.name + " past end");
    const std::uint8_t* p = r.ptr();
    std::vector<std::uint8_t> out;
    if (e.method == 0) {
        if (e.csize != e.usize) throw FormatError("zip: size mismatch in " + e.name);
        out.assign(p, p + e.csize);
    } else if (e.method == 8) {
        out = inflate(p, e.csize, e.usize, e.usize);  // never past the stated size
    } else {
        throw FormatError("zip: " + e.name + " uses method " + std::to_string(e.method));
    }
    if (out.size() != e.usize) throw FormatError("zip: size mismatch in " + e.name);
    if (crc32(out.data(), out.size()) != e.crc) throw FormatError("zip: CRC mismatch in " + e.name);
    return out;
}

namespace {

// A Win32 DOS device name (CON, PRN, AUX, NUL, COM1 ... COM9, LPT1 ... LPT9), with or without an
// extension, any case [REF Microsoft "Naming Files, Paths, and Namespaces"].
bool device_name(const std::string& part) {
    std::string base = part.substr(0, part.find('.'));
    while (!base.empty() && base.back() == ' ') base.pop_back();
    for (char& c : base) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL") return true;
    return base.size() == 4 && (base.compare(0, 3, "COM") == 0 || base.compare(0, 3, "LPT") == 0) &&
           base[3] >= '1' && base[3] <= '9';
}

}  // namespace

std::filesystem::path zip_entry_path(const std::string& name, bool utf8) {
    std::filesystem::path out;
    std::string part;
    const auto flush = [&]() -> bool {
        if (part.empty() || part == ".") {
            part.clear();
            return true;
        }
        // "..", a component Win32 would strip (a trailing '.' or ' ', which makes "A." and "A"
        // one file) and a device name are refused [DESIGN].
        if (part == ".." || part.back() == '.' || part.back() == ' ' || device_name(part)) return false;
        out /= utf8 ? std::filesystem::u8path(part) : std::filesystem::path(cp1252_to_utf16(part));
        part.clear();
        return true;
    };
    if (name.empty() || name[0] == '/' || name[0] == '\\') return {};
    for (char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        // NUL, control characters and the characters a Win32 file name cannot hold [REF
        // Microsoft "Naming Files, Paths, and Namespaces"]; ':' also stops a drive or a stream.
        if (u < 0x20 || u == 0x7f || c == ':' || c == '<' || c == '>' || c == '"' || c == '|' || c == '?' ||
            c == '*')
            return {};
        if (c == '/' || c == '\\') {
            if (!flush()) return {};
        } else {
            part.push_back(c);
        }
    }
    if (!flush()) return {};
    return out;
}

}  // namespace is::fmt
