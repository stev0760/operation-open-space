// WinHelp 3.1 reader: a port of re/isre/hlp.py (HelpFile and its helpers). The comments
// name the reference's functions; the format facts are in hlp.h.
#include "formats/hlp.h"

#include <algorithm>
#include <cstdio>
#include <set>

namespace is::fmt {

namespace {

constexpr std::uint32_t kHlpMagic = 0x00035F3F;
constexpr std::uint16_t kBtreeMagic = 0x293B;
constexpr std::uint16_t kSystemMagic = 0x036C;
constexpr std::size_t kBtreeHeaderSize = 38;
constexpr std::size_t kFileHeaderSize = 9;
constexpr std::size_t kTopicLinkSize = 21;
constexpr std::int32_t kTopicBlockHeaderSize = 12;
constexpr std::int32_t kDecompressedBlock = 0x4000;

constexpr std::uint8_t kTopicHeader = 0x02, kDisplay = 0x20, kTable = 0x23;

using Bytes = std::vector<std::uint8_t>;

[[noreturn]] void fail(const std::string& what) { throw FormatError("HLP: " + what); }

std::string hex(std::uint32_t v, int width = 0) {
    char buf[16];
    std::snprintf(buf, sizeof buf, width ? "%0*X" : "%#x", width, v);
    if (!width && v == 0) return "0x0";
    return buf;
}

// ---- bounds-checked little-endian access (struct.unpack_from raises past the end)

std::uint8_t at(const Bytes& b, std::size_t o) {
    if (o >= b.size()) fail("read past end of data at " + std::to_string(o));
    return b[o];
}
std::uint16_t u16(const Bytes& b, std::size_t o) {
    if (o + 2 > b.size()) fail("read past end of data at " + std::to_string(o));
    return static_cast<std::uint16_t>(b[o] | (b[o + 1] << 8));
}
std::int16_t i16(const Bytes& b, std::size_t o) { return static_cast<std::int16_t>(u16(b, o)); }
std::uint32_t u32(const Bytes& b, std::size_t o) {
    if (o + 4 > b.size()) fail("read past end of data at " + std::to_string(o));
    return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (std::uint32_t{b[o + 3]} << 24);
}
std::int32_t i32(const Bytes& b, std::size_t o) { return static_cast<std::int32_t>(u32(b, o)); }

// Python slicing: clamps, never throws.
Bytes slice(const Bytes& b, std::size_t from, std::size_t to) {
    from = std::min(from, b.size());
    to = std::min(std::max(to, from), b.size());
    return Bytes(b.begin() + static_cast<std::ptrdiff_t>(from),
                 b.begin() + static_cast<std::ptrdiff_t>(to));
}

// _cstr: the NUL-terminated string at o and the offset after its NUL.
std::string cstr(const Bytes& b, std::size_t o, std::size_t* next = nullptr) {
    std::string s;
    std::size_t i = o;
    while (i < b.size() && b[i] != 0) s.push_back(static_cast<char>(b[i++]));
    if (next) *next = i < b.size() ? i + 1 : b.size();
    return s;
}

std::vector<std::string> split_nul(const Bytes& b) {
    std::vector<std::string> out(1);
    for (std::uint8_t c : b) {
        if (c == 0)
            out.emplace_back();
        else
            out.back().push_back(static_cast<char>(c));
    }
    return out;
}

// Compressed integers: cu16, ci16, cu32, ci32.
std::uint32_t cu16(const Bytes& b, std::size_t& o) {
    if (at(b, o) & 1) {
        const std::uint32_t v = u16(b, o) >> 1;
        o += 2;
        return v;
    }
    return b[o++] >> 1;
}
std::int32_t ci16(const Bytes& b, std::size_t& o) {
    if (at(b, o) & 1) {
        const std::int32_t v = (u16(b, o) >> 1) - 0x4000;
        o += 2;
        return v;
    }
    return (b[o++] >> 1) - 0x40;
}
std::uint32_t cu32(const Bytes& b, std::size_t& o) {
    if (at(b, o) & 1) {
        const std::uint32_t v = u32(b, o) >> 1;
        o += 4;
        return v;
    }
    const std::uint32_t v = u16(b, o) >> 1;
    o += 2;
    return v;
}
std::int32_t ci32(const Bytes& b, std::size_t& o) {
    if (at(b, o) & 1) {
        const std::int32_t v = static_cast<std::int32_t>(u32(b, o) >> 1) - 0x40000000;
        o += 4;
        return v;
    }
    const std::int32_t v = static_cast<std::int32_t>(u16(b, o) >> 1) - 0x4000;
    o += 2;
    return v;
}

// Python's str.isspace over cp1252 text: the C0 separators 0x1C-0x1F and NBSP count.
bool py_space(char c) {
    const auto u = static_cast<std::uint8_t>(c);
    return (u >= 0x09 && u <= 0x0D) || (u >= 0x1C && u <= 0x20) || u == 0xA0;
}

std::string py_strip(const std::string& s) {
    std::size_t a = 0, z = s.size();
    while (a < z && py_space(s[a])) ++a;
    while (z > a && py_space(s[z - 1])) --z;
    return s.substr(a, z - a);
}

bool legal_ctx(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           c == '_' || c == '.';
}

// _name_variants
std::vector<std::string> name_variants(const std::string& raw) {
    const std::string s = py_strip(raw);
    if (s.empty()) return {};
    std::vector<std::string> out;
    std::string a, b, c;
    for (char ch : s) {
        const char r = ch == ' ' ? '_' : ch;
        if (legal_ctx(r)) a.push_back(r);
        if (legal_ctx(ch)) b.push_back(ch);
        const char k = legal_ctx(ch) ? ch : '_';
        if (!(k == '_' && !c.empty() && c.back() == '_')) c.push_back(k);
    }
    // re.sub(r"_+", "_", ...) then strip("_")
    const std::size_t lo = c.find_first_not_of('_');
    c = lo == std::string::npos ? std::string() : c.substr(lo, c.find_last_not_of('_') - lo + 1);
    out.push_back(a);
    out.push_back(b);
    out.push_back(c);
    const std::size_t colon = s.find(':');
    if (colon != std::string::npos) {
        for (auto& v : name_variants(s.substr(colon + 1))) out.push_back(v);
    }
    std::vector<std::string> uniq;
    for (auto& v : out) {
        if (!v.empty() && std::find(uniq.begin(), uniq.end(), v) == uniq.end()) uniq.push_back(v);
    }
    return uniq;
}

// _HASH_TABLE, as signed bytes.
constexpr std::uint8_t kHashTable[256] = {
    0x00, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0x0b, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0x0c, 0xff,
    0x0a, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0d,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x0b, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
};

void push_u16(Bytes& o, std::uint32_t v) {
    o.push_back(static_cast<std::uint8_t>(v));
    o.push_back(static_cast<std::uint8_t>(v >> 8));
}
void push_u32(Bytes& o, std::uint32_t v) {
    push_u16(o, v & 0xFFFF);
    push_u16(o, v >> 16);
}

}  // namespace

std::uint32_t hlp_context_hash(const std::string& name) {
    if (name.empty()) return 1;
    std::uint32_t h = 0;
    for (char ch : name) {
        const auto s = static_cast<std::int8_t>(kHashTable[static_cast<std::uint8_t>(ch)]);
        h = h * 43 + static_cast<std::uint32_t>(static_cast<std::int32_t>(s));
    }
    return h;
}

std::vector<std::uint8_t> hlp_lz77_expand(const std::uint8_t* p, std::size_t n, long limit) {
    Bytes out;
    const auto full = [&] { return limit >= 0 && out.size() >= static_cast<std::size_t>(limit); };
    std::size_t i = 0;
    while (i < n) {
        const std::uint8_t flags = p[i++];
        for (int bit = 0; bit < 8; ++bit) {
            if (i >= n || full()) break;
            if (flags & (1 << bit)) {
                if (i + 1 >= n) {
                    i = n;
                    break;
                }
                const unsigned w = p[i] | (p[i + 1] << 8);
                i += 2;
                const std::size_t length = (w >> 12) + 3;
                const std::size_t dist = (w & 0x0FFF) + 1;
                if (dist > out.size()) fail("LZ77 reference before start of data");
                const std::size_t start = out.size() - dist;
                for (std::size_t k = 0; k < length; ++k) out.push_back(out[start + k]);
            } else {
                out.push_back(p[i++]);
            }
        }
        if (full()) break;
    }
    if (limit >= 0 && out.size() > static_cast<std::size_t>(limit)) out.resize(limit);
    return out;
}

std::vector<std::uint8_t> hlp_runlen_expand(const std::uint8_t* p, std::size_t n) {
    Bytes out;
    std::size_t i = 0;
    while (i < n) {
        const std::uint8_t c = p[i++];
        if (c & 0x80) {
            const std::size_t k = std::min<std::size_t>(c & 0x7F, n - i);
            out.insert(out.end(), p + i, p + i + k);
            i += c & 0x7F;
        } else {
            if (i >= n) break;
            out.insert(out.end(), c, p[i]);
            ++i;
        }
    }
    return out;
}

// ---- model helpers

std::string HlpParagraph::plain() const {
    std::string out;
    for (const auto& r : runs) {
        if (r.kind == HlpRunKind::kText)
            out += r.text;
        else if (r.kind == HlpRunKind::kTab)
            out += '\t';
        else if (r.kind == HlpRunKind::kBreak)
            out += '\n';
    }
    return out;
}

std::string HlpTableRow::plain() const {
    std::string out;
    for (std::size_t c = 0; c < cells.size(); ++c) {
        if (c) out += '\t';
        for (std::size_t i = 0; i < cells[c].size(); ++i) {
            if (i) out += " / ";
            out += cells[c][i].plain();
        }
    }
    return out;
}

std::string HlpTopic::plain() const {
    std::string out;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        if (i) out += '\n';
        if (const auto* p = std::get_if<HlpParagraph>(&blocks[i]))
            out += p->plain();
        else
            out += std::get<HlpTableRow>(blocks[i]).plain();
    }
    return out;
}

std::vector<const HlpParagraph*> HlpTopic::paragraphs() const {
    std::vector<const HlpParagraph*> out;
    for (const auto& b : blocks) {
        if (const auto* p = std::get_if<HlpParagraph>(&b)) {
            out.push_back(p);
        } else {
            for (const auto& cell : std::get<HlpTableRow>(b).cells)
                for (const auto& para : cell) out.push_back(&para);
        }
    }
    return out;
}

std::vector<int> HlpTopic::links() const {
    std::vector<int> out;
    for (const HlpParagraph* p : paragraphs())
        for (const auto& r : p->runs)
            if (r.link >= 0 && std::find(out.begin(), out.end(), r.link) == out.end())
                out.push_back(r.link);
    return out;
}

// Picture.to_bmp
std::vector<std::uint8_t> HlpPicture::to_bmp() const {
    if ((ptype != 5 && ptype != 6) || !width || !height) return {};
    const std::uint32_t bpp = bit_count * std::max<std::uint32_t>(planes, 1);
    const std::size_t dib_stride = ((std::size_t{width} * bpp + 31) / 32) * 4;
    Bytes px = bits;
    if (ptype == 5) {
        // DDB rows are padded to 16 bits and stored top-down; DIB rows to 32 bits, bottom-up.
        const std::size_t ddb_stride = ((std::size_t{width} * bpp + 15) / 16) * 2;
        Bytes out;
        for (std::size_t r = height; r-- > 0;) {
            Bytes row = slice(bits, r * ddb_stride, (r + 1) * ddb_stride);
            row.resize(dib_stride, 0);
            out.insert(out.end(), row.begin(), row.end());
        }
        px = std::move(out);
    }
    const std::size_t size = dib_stride * height;
    px.resize(size, 0);
    Bytes pal = palette;
    if (pal.empty() && bpp <= 8) {
        if (bpp == 1)
            pal = {0, 0, 0, 0, 0xff, 0xff, 0xff, 0};
        else
            pal.assign(4u << bpp, 0);
    }
    // round(x * 10000 / 254): x * 10000 / 254 is never exactly .5 for an integer x (254 =
    // 2 * 127 and 10000 x / 127 has no half), so half-up rounding equals Python's.
    const auto ppm = [](std::uint32_t dpi) -> std::int32_t {
        return dpi ? static_cast<std::int32_t>((std::uint64_t{dpi} * 10000 + 127) / 254) : 0;
    };
    Bytes info;
    push_u32(info, 40);
    push_u32(info, width);
    push_u32(info, height);
    push_u16(info, 1);
    push_u16(info, bpp);
    push_u32(info, 0);
    push_u32(info, static_cast<std::uint32_t>(size));
    push_u32(info, static_cast<std::uint32_t>(ppm(xdpi)));
    push_u32(info, static_cast<std::uint32_t>(ppm(ydpi)));
    push_u32(info, bpp <= 8 ? static_cast<std::uint32_t>(pal.size() / 4) : 0);
    push_u32(info, colors_important > 1 ? colors_important : 0);
    const std::uint32_t off = static_cast<std::uint32_t>(14 + info.size() + pal.size());
    Bytes out{'B', 'M'};
    push_u32(out, static_cast<std::uint32_t>(off + size));
    push_u32(out, 0);
    push_u32(out, off);
    out.insert(out.end(), info.begin(), info.end());
    out.insert(out.end(), pal.begin(), pal.end());
    out.insert(out.end(), px.begin(), px.begin() + static_cast<std::ptrdiff_t>(size));
    return out;
}

// ---- HelpFile

HlpFile::HlpFile(std::vector<std::uint8_t> data) : data_(std::move(data)) {
    if (data_.size() < 16 || u32(data_, 0) != kHlpMagic) fail("not a WinHelp file (bad magic)");
    const std::int32_t directory_start = i32(data_, 4);
    btree_leaves(raw_file(directory_start), [this](const Bytes& b, std::size_t& o) {
        std::string name = cstr(b, o, &o);
        const std::int32_t off = i32(b, o);
        o += 4;
        if (!file_index_.count(name)) files_.emplace_back(name, off);
        else
            for (auto& f : files_)
                if (f.first == name) f.second = off;
        file_index_[name] = off;
    });
    parse_system();
    lz77_ = system_.minor > 16 && (system_.flags == 4 || system_.flags == 8);
    block_size_ = system_.flags == 8 ? 2048 : 4096;
    parse_fonts();
    parse_context();
    parse_ctxomap();
    parse_titles();
    parse_keywords();
    for (const auto& f : files_) {
        if (f.first.rfind("|bm", 0) != 0) continue;
        const std::string name = f.first.substr(1);
        try {
            pictures_[name] = parse_picture(name, file(f.first));
        } catch (const FormatError& e) {
            std::string what = e.what();
            if (what.rfind("HLP: ", 0) == 0) what = what.substr(5);
            errors_.push_back(f.first + ": " + what);
        }
    }
    parse_topics();
    for (const auto& t : topics_) topic_offsets_.push_back(t.offset);
    recover_context_names();
    attach();
}

std::vector<std::uint8_t> HlpFile::raw_file(std::int32_t off) const {
    if (off < 0 || static_cast<std::size_t>(off) + kFileHeaderSize > data_.size())
        fail("internal file header at " + hex(static_cast<std::uint32_t>(off)) +
             " is outside the file");
    const std::int32_t used = i32(data_, static_cast<std::size_t>(off) + 4);
    const std::size_t start = static_cast<std::size_t>(off) + kFileHeaderSize;
    if (used < 0 || start + static_cast<std::size_t>(used) > data_.size())
        fail("internal file at " + hex(static_cast<std::uint32_t>(off)) + " (size " +
             std::to_string(used) + ") runs past end of file");
    return slice(data_, start, start + static_cast<std::size_t>(used));
}

bool HlpFile::has(const std::string& name) const { return file_index_.count(name) != 0; }

std::vector<std::uint8_t> HlpFile::file(const std::string& name) const {
    const auto it = file_index_.find(name);
    if (it == file_index_.end()) fail("no internal file " + name);
    return raw_file(it->second);
}

const HlpFont* HlpFile::font(int i) const {
    return i >= 0 && static_cast<std::size_t>(i) < fonts_.size() ? &fonts_[i] : nullptr;
}

// _btree_leaves: the leaf pages in chain order, each entry through `leaf`.
template <class Leaf>
void HlpFile::btree_leaves(const Bytes& b, Leaf&& leaf) {
    if (b.size() < kBtreeHeaderSize || u16(b, 0) != kBtreeMagic) fail("bad B+ tree magic");
    const std::size_t page_size = u16(b, 4);
    const int root = i16(b, 26);
    const int total_pages = i16(b, 30);
    const int levels = i16(b, 32);
    int page = root;
    for (int l = 0; l < levels - 1; ++l)  // index pages: follow PreviousPage
        page = i16(b, kBtreeHeaderSize + static_cast<std::size_t>(page) * page_size + 4);
    std::set<int> seen;
    while (page != -1) {
        if (seen.count(page) || page < 0 || page >= total_pages) {
            errors_.push_back("B+ tree leaf chain broken at page " + std::to_string(page));
            return;
        }
        seen.insert(page);
        const std::size_t base = kBtreeHeaderSize + static_cast<std::size_t>(page) * page_size;
        const int n = i16(b, base + 2);
        const int next = i16(b, base + 6);
        std::size_t o = base + 8;
        for (int k = 0; k < n; ++k) leaf(b, o);
        page = next;
    }
}

void HlpFile::parse_system() {
    const Bytes b = file("|SYSTEM");
    if (u16(b, 0) != kSystemMagic) fail("bad |SYSTEM magic");
    HlpSystem& si = system_;
    si.minor = u16(b, 2);
    si.major = u16(b, 4);
    si.gen_date = u32(b, 6);
    si.flags = u16(b, 10);
    if (si.minor <= 16)
        fail("Windows 3.0 help file (|SYSTEM minor " + std::to_string(si.minor) +
             ") is not supported");
    std::size_t o = 12;
    while (o + 4 <= b.size()) {
        const std::uint16_t rtype = u16(b, o), size = u16(b, o + 2);
        const Bytes d = slice(b, o + 4, o + 4 + size);
        o += 4 + size;
        switch (rtype) {
        case 1: si.title = cstr(d, 0); break;
        case 2: si.copyright = cstr(d, 0); break;
        case 3: si.contents = i32(d, 0); break;
        case 4: si.config.push_back(cstr(d, 0)); break;
        case 5: si.icon = d; break;
        case 6:
            if (d.size() >= 90) {
                HlpWindow w;
                w.flags = u16(d, 0);
                w.type = cstr(slice(d, 2, 12), 0);
                w.name = cstr(slice(d, 12, 21), 0);
                w.caption = cstr(slice(d, 21, 72), 0);
                w.x = i16(d, 72);
                w.y = i16(d, 74);
                w.width = i16(d, 76);
                w.height = i16(d, 78);
                w.maximize = i16(d, 80);
                w.rgb = u32(d, 82);
                w.rgb_nsr = u32(d, 86);
                si.windows.push_back(std::move(w));
            } else {
                si.other[rtype] = d;
            }
            break;
        case 8: si.citation = cstr(d, 0); break;
        default: si.other[rtype] = d; break;
        }
    }
    if (si.flags != 0 && si.flags != 4 && si.flags != 8)
        errors_.push_back("|SYSTEM: unknown flags " + hex(si.flags));
}

void HlpFile::parse_fonts() {
    if (!has("|FONT")) return;
    const Bytes b = file("|FONT");
    const std::uint16_t n_faces = u16(b, 0), n_desc = u16(b, 2), face_off = u16(b, 4),
                        desc_off = u16(b, 6);
    if (face_off >= 12) {
        errors_.push_back("|FONT: multimedia font descriptors are not supported");
        return;
    }
    const int face_len = n_faces ? (desc_off - face_off) / n_faces : 0;
    std::vector<std::string> faces;
    for (int i = 0; i < n_faces; ++i)
        faces.push_back(cstr(slice(b, face_off + static_cast<std::size_t>(i) * face_len,
                                   face_off + static_cast<std::size_t>(i + 1) * face_len),
                             0));
    for (int i = 0; i < n_desc; ++i) {
        const std::size_t o = desc_off + static_cast<std::size_t>(i) * 11;
        HlpFont f;
        f.attributes = at(b, o);
        f.half_points = at(b, o + 1);
        f.family = at(b, o + 2);
        const std::uint16_t face = u16(b, o + 3);
        if (face < faces.size()) f.face = faces[face];
        f.r = at(b, o + 5);
        f.g = at(b, o + 6);
        f.b = at(b, o + 7);
        fonts_.push_back(std::move(f));
    }
}

void HlpFile::parse_context() {
    if (!has("|CONTEXT")) return;
    btree_leaves(file("|CONTEXT"), [this](const Bytes& b, std::size_t& o) {
        contexts_.emplace_back(u32(b, o), i32(b, o + 4));
        o += 8;
    });
}

void HlpFile::parse_ctxomap() {
    if (!has("|CTXOMAP")) return;
    const Bytes b = file("|CTXOMAP");
    const std::uint16_t n = u16(b, 0);
    for (std::size_t i = 0; i < n; ++i) ctxomap_.emplace_back(i32(b, 2 + 8 * i), i32(b, 6 + 8 * i));
}

void HlpFile::parse_titles() {
    if (!has("|TTLBTREE")) return;
    btree_leaves(file("|TTLBTREE"), [this](const Bytes& b, std::size_t& o) {
        const std::int32_t off = i32(b, o);
        titles_[off] = cstr(b, o + 4, &o);
    });
}

void HlpFile::parse_keywords() {
    if (!has("|KWBTREE") || !has("|KWDATA")) return;
    const Bytes data = file("|KWDATA");
    btree_leaves(file("|KWBTREE"), [&](const Bytes& b, std::size_t& o) {
        HlpKeyword kw;
        kw.text = cstr(b, o, &o);
        const int count = i16(b, o);
        const std::int32_t off = i32(b, o + 2);
        o += 6;
        for (int k = 0; k < count; ++k)
            kw.offsets.push_back(i32(data, static_cast<std::size_t>(off) + 4 * k));
        keywords_.push_back(std::move(kw));
    });
}

HlpPicture HlpFile::parse_picture(const std::string& name, const Bytes& b) {
    const std::uint16_t magic = u16(b, 0), count = u16(b, 2);
    if (magic != 0x506C && magic != 0x706C) fail("bad picture magic " + hex(magic));
    if (count < 1) fail("picture file holds no pictures");
    const std::size_t base = u32(b, 4);  // the first (usually only) resolution
    HlpPicture pic;
    pic.name = name;
    pic.ptype = at(b, base);
    pic.packing = at(b, base + 1);
    pic.count = count;
    std::size_t o = base + 2;
    Bytes raw;
    if (pic.ptype == 5 || pic.ptype == 6) {
        pic.xdpi = cu32(b, o);
        pic.ydpi = cu32(b, o);
        pic.planes = cu16(b, o);
        pic.bit_count = cu16(b, o);
        pic.width = cu32(b, o);
        pic.height = cu32(b, o);
        pic.colors_used = cu32(b, o);
        pic.colors_important = cu32(b, o);
        const std::uint32_t csize = cu32(b, o);
        pic.hotspot_size = cu32(b, o);
        const std::uint32_t coff = u32(b, o);
        u32(b, o + 4);  // hotspot offset
        o += 8;
        if (pic.ptype == 6 && pic.bit_count <= 8) {
            const std::uint32_t ncolors = pic.colors_used ? pic.colors_used : 1u << pic.bit_count;
            pic.palette = slice(b, o, o + 4 * std::size_t{ncolors});
        }
        raw = slice(b, base + coff, base + coff + csize);
    } else if (pic.ptype == 8) {
        cu16(b, o);  // mapping mode
        pic.width = u16(b, o);
        pic.height = u16(b, o + 2);
        o += 4;
        cu32(b, o);  // decompressed size
        const std::uint32_t csize = cu32(b, o);
        pic.hotspot_size = cu32(b, o);
        const std::uint32_t coff = u32(b, o);
        raw = slice(b, base + coff, base + coff + csize);
        pic.note = "metafile (not converted)";
    } else {
        fail("unknown picture type " + std::to_string(pic.ptype));
    }
    if (pic.packing == 2 || pic.packing == 3) raw = hlp_lz77_expand(raw.data(), raw.size());
    if (pic.packing == 1 || pic.packing == 3) raw = hlp_runlen_expand(raw.data(), raw.size());
    pic.bits = raw;
    if (pic.ptype == 5 || pic.ptype == 6) {
        const std::size_t bits = std::size_t{pic.width} * pic.bit_count * std::max(pic.planes, 1u);
        const std::size_t stride = pic.ptype == 5 ? ((bits + 15) / 16) * 2 : ((bits + 31) / 32) * 4;
        const std::size_t want = stride * pic.height;
        if (raw.size() != want)
            errors_.push_back("|" + name + ": expanded " + std::to_string(raw.size()) +
                              " bytes, expected " + std::to_string(want));
    }
    if (count > 1)
        pic.note = (pic.note.empty() ? "" : pic.note + "; ") + std::to_string(count) +
                   " resolutions, first extracted";
    return pic;
}

void HlpFile::parse_topics() {
    if (system_.minor <= 16) return;
    const Bytes t = file("|TOPIC");
    std::vector<Bytes> blocks;
    std::vector<std::int32_t> first_link;  // TOPICBLOCKHEADER.FirstTopicLink
    for (std::size_t start = 0; start < t.size(); start += static_cast<std::size_t>(block_size_)) {
        const Bytes chunk = slice(t, start, start + static_cast<std::size_t>(block_size_));
        if (chunk.size() < static_cast<std::size_t>(kTopicBlockHeaderSize)) break;
        first_link.push_back(i32(chunk, 4));
        Bytes body = slice(chunk, kTopicBlockHeaderSize, chunk.size());
        if (lz77_) body = hlp_lz77_expand(body.data(), body.size(), kDecompressedBlock);
        blocks.push_back(std::move(body));
    }
    std::vector<std::size_t> starts;
    Bytes data;
    for (const auto& blk : blocks) {
        starts.push_back(data.size());
        data.insert(data.end(), blk.begin(), blk.end());
    }
    const auto floordiv = [](std::int64_t a, std::int64_t b) {
        return a >= 0 ? a / b : -((-a + b - 1) / b);
    };
    const auto to_off = [&](std::int32_t pos) -> std::size_t {
        const std::int64_t blk = floordiv(std::int64_t{pos} - kTopicBlockHeaderSize, kDecompressedBlock);
        const std::int64_t inb = std::int64_t{pos} - kTopicBlockHeaderSize - blk * kDecompressedBlock;
        if (pos < kTopicBlockHeaderSize || blk >= static_cast<std::int64_t>(blocks.size()) ||
            inb > static_cast<std::int64_t>(blocks[static_cast<std::size_t>(blk)].size()))
            fail("TOPICPOS " + hex(static_cast<std::uint32_t>(pos)) + " is outside |TOPIC");
        return starts[static_cast<std::size_t>(blk)] + static_cast<std::size_t>(inb);
    };

    std::int32_t pos = first_link.empty() ? kTopicBlockHeaderSize : first_link[0];
    std::int32_t topic_offset = 0;
    HlpTopic* topic = nullptr;
    std::set<std::int32_t> seen;
    while (true) {
        if (seen.count(pos)) {
            errors_.push_back("|TOPIC: record loop at " + hex(static_cast<std::uint32_t>(pos)));
            break;
        }
        seen.insert(pos);
        std::size_t o;
        try {
            o = to_off(pos);
        } catch (const FormatError& e) {
            errors_.push_back(std::string(e.what()).substr(5));
            break;
        }
        if (o + kTopicLinkSize > data.size()) {
            errors_.push_back("|TOPIC: truncated record at " + hex(static_cast<std::uint32_t>(pos)));
            break;
        }
        const std::int32_t size = i32(data, o), len2 = i32(data, o + 4), nxt = i32(data, o + 12),
                           len1 = i32(data, o + 16);
        const std::uint8_t rtype = data[o + 20];
        if (rtype == kTopicHeader) ++topic_headers_;
        if (nxt <= 0) {
            // The last TOPICLINK carries no data. In both Inner Space manuals it is an empty
            // topic header whose TOPICOFFSET ends the last real topic.
            end_offset_ = topic_offset;
            break;
        }
        if (len1 < static_cast<std::int32_t>(kTopicLinkSize) || size < len1 ||
            o + static_cast<std::size_t>(size) > data.size()) {
            errors_.push_back("|TOPIC: bad record sizes at " + hex(static_cast<std::uint32_t>(pos)));
            break;
        }
        const Bytes ld1 = slice(data, o + kTopicLinkSize, o + static_cast<std::size_t>(len1));
        Bytes ld2 = slice(data, o + static_cast<std::size_t>(len1), o + static_cast<std::size_t>(size));
        if (len2 > size - len1) fail("phrase-compressed topic text is not supported");
        ld2.resize(static_cast<std::size_t>(std::max(len2, 0)));
        if (rtype == kTopicHeader) {
            HlpTopic tp;
            tp.index = static_cast<int>(topics_.size()) + 1;
            tp.browse_prev = i32(ld1, 4);
            tp.browse_next = i32(ld1, 8);
            tp.number = i32(ld1, 12);
            tp.nonscroll_pos = i32(ld1, 16);
            tp.scroll_pos = i32(ld1, 20);
            i32(ld1, 24);  // NextTopic
            tp.offset = topic_offset;
            tp.pos = pos;
            const auto strings = split_nul(ld2);
            tp.title = strings[0];
            for (std::size_t i = 1; i < strings.size(); ++i)
                if (!strings[i].empty()) tp.macros.push_back(strings[i]);
            topics_.push_back(std::move(tp));
            topic = &topics_.back();
        } else if (rtype == kDisplay || rtype == kTable) {
            std::size_t q = 0;
            ci32(ld1, q);
            topic_offset += static_cast<std::int32_t>(cu16(ld1, q));
            if (!topic) {
                errors_.push_back("|TOPIC: display record before first topic header at " +
                                  hex(static_cast<std::uint32_t>(pos)));
            } else {
                const std::int32_t ns = topic->nonscroll_pos, sc = topic->scroll_pos;
                const bool nonscroll = ns != -1 && ns <= pos && (sc == -1 || pos < sc);
                try {
                    parse_display(*topic, rtype, ld1, ld2, nonscroll);
                } catch (const FormatError& e) {
                    errors_.push_back("topic " + std::to_string(topic->index) + " record " +
                                      hex(static_cast<std::uint32_t>(pos)) + ": " +
                                      std::string(e.what()).substr(5));
                }
            }
        } else {
            errors_.push_back("|TOPIC: unknown record type " + hex(rtype) + " at " +
                              hex(static_cast<std::uint32_t>(pos)));
        }
        // crossing into another block restarts the character count
        const std::int64_t next_block = floordiv(std::int64_t{nxt} - 12, kDecompressedBlock);
        if (next_block != floordiv(std::int64_t{pos} - 12, kDecompressedBlock))
            topic_offset = static_cast<std::int32_t>(next_block * 0x8000);
        pos = nxt;
    }
}

// _parse_display: one display (0x20) or table (0x23) record into topic.blocks.
void HlpFile::parse_display(HlpTopic& topic, std::uint8_t rtype, const Bytes& ld1,
                            const Bytes& ld2, bool nonscroll) {
    std::size_t p = 0;
    ci32(ld1, p);
    cu16(ld1, p);
    if (rtype == kTable) {
        const std::uint8_t ncols = at(ld1, p), ttype = at(ld1, p + 1);
        p += 2;
        if (ttype == 0 || ttype == 2) p += 2;
        p += 4 * std::size_t{ncols};
    }
    const auto strings = split_nul(ld2);
    std::size_t si = 0;
    std::optional<HlpTableRow> row;
    if (rtype == kTable) row.emplace(), row->nonscroll = nonscroll;
    std::optional<int> last_col;
    int link = -1;
    int font = -1;
    const std::size_t end = ld1.size();
    while (p < end) {
        int col = 0;
        if (rtype == kTable) {
            col = i16(ld1, p);
            if (col == -1) break;
            p += 5;
        }
        p += 4;
        const std::uint16_t bits = u16(ld1, p);
        p += 2;
        HlpParagraph para;
        para.nonscroll = nonscroll;
        if (bits & 0x0400) para.align = HlpAlign::kRight;
        if (bits & 0x0800) para.align = HlpAlign::kCenter;
        if (bits & 0x0001) ci32(ld1, p);
        for (unsigned flag = 0x02; flag <= 0x40; flag <<= 1) {
            if (!(bits & flag)) continue;
            const int v = ci16(ld1, p);
            switch (flag) {
            case 0x02: para.space_before = v; break;
            case 0x04: para.space_after = v; break;
            case 0x08: para.line_spacing = v; break;
            case 0x10: para.left_indent = v; break;
            case 0x20: para.right_indent = v; break;
            default: para.first_indent = v; break;
            }
        }
        if (bits & 0x0100) p += 3;
        if (bits & 0x0200) {
            const int ntabs = ci16(ld1, p);
            for (int k = 0; k < ntabs; ++k) {
                const std::uint32_t stop = cu16(ld1, p);
                const std::uint32_t type = stop & 0x4000 ? cu16(ld1, p) : 0;
                para.tabs.push_back(static_cast<int>(stop & 0x3FFF));
                para.tab_types.push_back(static_cast<int>(type));
            }
        }
        if (row) {
            if (!last_col || col != *last_col || row->cells.empty()) row->cells.emplace_back();
            last_col = col;
        }
        const auto emit = [&](HlpParagraph pa) {
            if (row)
                row->cells.back().push_back(std::move(pa));
            else
                topic.blocks.emplace_back(std::move(pa));
        };
        HlpParagraph cur = para;
        const auto add = [&](HlpRunKind kind, std::string text = {}, char align = 0) {
            HlpRun r;
            r.kind = kind;
            r.text = std::move(text);
            r.font = font;
            r.link = link;
            r.align = align;
            cur.runs.push_back(std::move(r));
        };
        const auto new_link = [&](HlpLinkKind kind, std::string target,
                                  std::optional<std::uint32_t> hash) {
            HlpLink l;
            l.kind = kind;
            l.target = std::move(target);
            l.hash = hash;
            topic.link_table.push_back(std::move(l));
            link = static_cast<int>(topic.link_table.size()) - 1;
        };
        while (true) {
            const std::string s = si < strings.size() ? strings[si] : std::string();
            ++si;
            if (!s.empty()) add(HlpRunKind::kText, s);
            if (p >= end) fail("character formatting ran past LinkData1");
            const std::uint8_t cmd = ld1[p];
            if (cmd == 0xFF) {
                ++p;
                break;
            }
            switch (cmd) {
            case 0x20: p += 5; break;
            case 0x21: p += 3; break;
            case 0x80:
                font = i16(ld1, p + 1);
                p += 3;
                break;
            case 0x81: {
                HlpRun r;  // a line break carries no font or link
                r.kind = HlpRunKind::kBreak;
                cur.runs.push_back(std::move(r));
                ++p;
                break;
            }
            case 0x82: {
                emit(std::move(cur));
                // The next paragraph of the record keeps the record's paragraph format.
                cur = para;
                ++p;
                break;
            }
            case 0x83:
                add(HlpRunKind::kTab);
                ++p;
                break;
            case 0x86:
            case 0x87:
            case 0x88: {
                const std::uint8_t ptype = at(ld1, p + 1);
                p += 2;
                const std::int32_t psize = ci32(ld1, p);
                if (ptype == 0x22) cu16(ld1, p);
                const std::size_t start = p;
                const char align = cmd == 0x86 ? 'c' : cmd == 0x87 ? 'l' : 'r';
                if (ptype == 3 || ptype == 0x22) {
                    if (u16(ld1, p) == 0) {
                        const std::string name = "bm" + std::to_string(u16(ld1, p + 2));
                        add(HlpRunKind::kImage, name, align);
                        if (std::find(topic.bitmaps.begin(), topic.bitmaps.end(), name) ==
                            topic.bitmaps.end())
                            topic.bitmaps.push_back(name);
                    } else {
                        add(HlpRunKind::kEmbed, "embedded picture (not extracted)", align);
                        errors_.push_back("topic " + std::to_string(topic.index) +
                                          ": embedded picture not extracted");
                    }
                } else if (ptype == 5) {
                    add(HlpRunKind::kEmbed, cstr(ld1, p + 6), align);
                } else {
                    errors_.push_back("topic " + std::to_string(topic.index) +
                                      ": unknown picture type " + hex(ptype));
                }
                p = start + static_cast<std::size_t>(static_cast<std::int64_t>(psize));
                break;
            }
            case 0x89:
                link = -1;
                ++p;
                break;
            case 0x8B:
                add(HlpRunKind::kText, " ");
                ++p;
                break;
            case 0x8C:  // non-breaking hyphen
                add(HlpRunKind::kText, "-");
                ++p;
                break;
            case 0xC8:
            case 0xCC: {
                const int mlen = i16(ld1, p + 1);
                new_link(HlpLinkKind::kMacro, cstr(ld1, p + 3), std::nullopt);
                p += 3 + static_cast<std::size_t>(mlen);
                break;
            }
            case 0xE0:
            case 0xE1:
                new_link(cmd == 0xE0 ? HlpLinkKind::kPopup : HlpLinkKind::kJump,
                         "TOPIC" + std::to_string(i32(ld1, p + 1)), std::nullopt);
                p += 5;
                break;
            case 0xE2:
            case 0xE3:
            case 0xE6:
            case 0xE7:
                new_link(cmd == 0xE2 || cmd == 0xE6 ? HlpLinkKind::kPopup : HlpLinkKind::kJump,
                         "", u32(ld1, p + 1));
                p += 5;
                break;
            case 0xEA:
            case 0xEB:
            case 0xEE:
            case 0xEF: {
                const int sz = i16(ld1, p + 1);
                const std::uint8_t kind = at(ld1, p + 3);
                const std::uint32_t h = u32(ld1, p + 4);
                std::string ext;
                if (kind == 4 || kind == 6) {
                    std::size_t q;
                    const std::string f1 = cstr(ld1, p + 8, &q);
                    ext = "@" + f1;
                    if (kind == 6) ext = ">" + f1 + "@" + cstr(ld1, q);
                } else if (kind == 1) {
                    ext = ">window" + std::to_string(at(ld1, p + 8));
                }
                new_link(HlpLinkKind::kExternal, ext, h);
                p += 3 + static_cast<std::size_t>(sz);
                break;
            }
            default:
                fail("unknown formatting command " + hex(cmd) + " at " + std::to_string(p));
            }
        }
        // a group that ends without 0x82 still closes its paragraph
        if (!cur.runs.empty()) emit(std::move(cur));
    }
    if (row && !row->cells.empty()) topic.blocks.emplace_back(std::move(*row));
}

// _recover_context_names: map context hashes to names by hashing candidate strings.
void HlpFile::recover_context_names() {
    std::set<std::uint32_t> wanted;
    for (const auto& c : contexts_) wanted.insert(c.first);
    for (const auto& t : topics_)
        for (int li : t.links())
            if (t.link_table[li].hash) wanted.insert(*t.link_table[li].hash);
    std::vector<std::string> candidates;
    for (const auto& t : topics_) candidates.push_back(t.title);
    for (const auto& kv : titles_) candidates.push_back(kv.second);
    for (const auto& k : keywords_) candidates.push_back(k.text);
    for (const auto& t : topics_) {
        for (const HlpParagraph* para : t.paragraphs()) {
            std::string text;
            for (const auto& r : para->runs)
                if (r.kind == HlpRunKind::kText && r.link >= 0) text += r.text;
            if (!text.empty()) candidates.push_back(text);
            const std::string first = py_strip(para->plain());
            if (!first.empty() && first.size() < 60) candidates.push_back(first);
        }
    }
    static const char* const kPrefixes[] = {"", "idh_", "helpid_", "IDH_"};
    for (const auto& c : candidates) {
        for (const auto& variant : name_variants(c)) {
            for (const char* prefix : kPrefixes) {
                const std::string name = prefix + variant;
                const std::uint32_t h = hlp_context_hash(name);
                if (wanted.count(h) && !context_names_.count(h)) context_names_[h] = name;
            }
        }
    }
}

std::string HlpFile::context_name(std::uint32_t hash) const {
    const auto it = context_names_.find(hash);
    return it != context_names_.end() ? it->second : "#" + hex(hash, 8);
}

const HlpTopic* HlpFile::topic_at(std::int32_t offset) const {
    if (offset < 0 || topics_.empty()) return nullptr;
    if (end_offset_ && offset >= *end_offset_) return nullptr;
    const auto it = std::upper_bound(topic_offsets_.begin(), topic_offsets_.end(), offset);
    const std::ptrdiff_t i = (it - topic_offsets_.begin()) - 1;
    return i >= 0 ? &topics_[static_cast<std::size_t>(i)] : nullptr;
}

const HlpTopic* HlpFile::topic_for_hash(std::uint32_t hash) const {
    for (const auto& c : contexts_)
        if (c.first == hash) return topic_at(c.second);
    return nullptr;
}

const HlpTopic* HlpFile::topic_for_map_id(std::int32_t id) const {
    for (const auto& m : ctxomap_)
        if (m.first == id) return topic_at(m.second);
    return nullptr;
}

const HlpTopic* HlpFile::contents_topic() const {
    if (system_.contents) {
        if (const HlpTopic* t = topic_at(*system_.contents)) return t;
    }
    return topics_.empty() ? nullptr : &topics_.front();
}

void HlpFile::attach() {
    for (const auto& c : contexts_) {
        if (const HlpTopic* t = topic_at(c.second))
            topics_[static_cast<std::size_t>(t->index - 1)].contexts.push_back(context_name(c.first));
        else
            errors_.push_back("|CONTEXT " + context_name(c.first) + " points outside any topic");
    }
    for (const auto& m : ctxomap_)
        if (const HlpTopic* t = topic_at(m.second))
            topics_[static_cast<std::size_t>(t->index - 1)].map_ids.push_back(m.first);
    for (const auto& kw : keywords_) {
        for (std::int32_t off : kw.offsets) {
            if (const HlpTopic* t = topic_at(off)) {
                auto& list = topics_[static_cast<std::size_t>(t->index - 1)].keywords;
                if (std::find(list.begin(), list.end(), kw.text) == list.end())
                    list.push_back(kw.text);
            }
        }
    }
    for (auto& t : topics_) {
        for (int li : t.links()) {
            HlpLink& l = t.link_table[static_cast<std::size_t>(li)];
            if (l.kind == HlpLinkKind::kExternal)
                l.target = context_name(*l.hash) + l.target;
            else if (l.hash)
                l.target = context_name(*l.hash);
        }
        for (const auto& name : t.bitmaps)
            if (!pictures_.count(name))
                errors_.push_back("topic " + std::to_string(t.index) + ": |" + name +
                                  " is referenced but missing");
    }
}

}  // namespace is::fmt
