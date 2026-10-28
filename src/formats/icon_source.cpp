#include "formats/icon_source.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>

#include "formats/icon.h"
#include "formats/ne.h"

namespace is::fmt {

namespace {

// A member drawn over black at its own size.
struct Drawn {
    int w = 0, h = 0, bpp = 0;
    std::vector<std::uint32_t> rgb;  // 0x00RRGGBB, top-down
};

std::uint32_t le32(const std::uint8_t* p) {
    return std::uint32_t{p[0]} | std::uint32_t{p[1]} << 8 | std::uint32_t{p[2]} << 16 |
           std::uint32_t{p[3]} << 24;
}

// A 32 bpp icon image: BITMAPINFOHEADER, BGRA rows, AND mask. parse_dib takes 1-24 bpp only.
Drawn draw_32(const std::uint8_t* p, std::size_t n) {
    Reader r(p, n);
    const std::uint32_t hsize = r.u32();
    if (hsize < 40) throw FormatError("unknown DIB header");
    const std::int32_t w = r.i32();
    const std::int32_t h2 = r.i32();
    r.u16();
    if (r.u16() != 32) throw FormatError("not a 32 bpp icon");
    const std::uint32_t comp = r.u32();
    if (comp != 0 && comp != 3) throw FormatError("compressed DIBs are not supported");
    if (w <= 0 || w > 1024 || h2 <= 0 || h2 > 2048 || h2 % 2) throw FormatError("bad DIB size");
    if (hsize > n) throw FormatError("DIB header past end");
    // BI_BITFIELDS masks follow a 40-byte header; V4 and V5 headers hold them (M3 s1 fixer)
    // [REF Win32 SDK BITMAPV4HEADER].
    std::size_t at = hsize + (comp == 3 && hsize == 40 ? 12 : 0);
    const std::size_t h = static_cast<std::size_t>(h2 / 2), ww = static_cast<std::size_t>(w);
    const std::size_t need = ww * h * 4;
    if (at > n || n - at < need) throw FormatError("DIB bits truncated");
    Drawn d;
    d.w = w;
    d.h = static_cast<int>(h);
    d.bpp = 32;
    d.rgb.resize(ww * h);
    bool alpha = false;
    for (std::size_t i = 0; i < ww * h; ++i) alpha |= p[at + 4 * i + 3] != 0;
    for (std::size_t y = 0; y < h; ++y)
        for (std::size_t x = 0; x < ww; ++x) {
            const std::uint8_t* px = p + at + ((h - 1 - y) * ww + x) * 4;  // bottom-up
            std::uint32_t b = px[0], g = px[1], rd = px[2];
            if (alpha) {
                const std::uint32_t a = px[3];
                b = (b * a + 127) / 255;
                g = (g * a + 127) / 255;
                rd = (rd * a + 127) / 255;
            }
            d.rgb[y * ww + x] = rd << 16 | g << 8 | b;
        }
    return d;
}

// DrawIcon over black: (0 AND mask) XOR image = image, for every pixel.
Drawn draw_member(const std::uint8_t* p, std::size_t n) {
    if (n >= 8 && p[0] == 0x89 && p[1] == 'P' && p[2] == 'N' && p[3] == 'G')
        throw FormatError("PNG icon images are not supported");
    if (n >= 16 && le32(p) >= 40 && p[14] == 32 && p[15] == 0) return draw_32(p, n);
    const Icon ic = parse_icon(p, n);
    Drawn d;
    d.w = ic.image.width;
    d.h = ic.image.height;
    d.bpp = ic.image.bpp;
    const auto px = icon_to_bgra(ic);
    d.rgb.reserve(px.size());
    for (std::uint32_t c : px) d.rgb.push_back(c & 0x00FFFFFF);
    return d;
}

// A member's size and depth from its header alone, without decoding it; nullopt for PNG, an
// unknown header or a side above kMaxIconSide (M3 s1 fixer) [DESIGN ADR 0012].
std::optional<Drawn> peek_member(const std::uint8_t* p, std::size_t n) {
    if (!p || n < 16) return std::nullopt;
    if (p[0] == 0x89 && p[1] == 'P' && p[2] == 'N' && p[3] == 'G') return std::nullopt;
    const std::uint32_t hsize = le32(p);
    Drawn d;
    if (hsize == 12) {  // BITMAPCOREHEADER
        d.w = p[4] | p[5] << 8;
        d.h = (p[6] | p[7] << 8) / 2;
        d.bpp = p[10] | p[11] << 8;
    } else if (hsize >= 40 && n >= 16) {
        d.w = static_cast<std::int32_t>(le32(p + 4));
        d.h = static_cast<std::int32_t>(le32(p + 8)) / 2;
        d.bpp = p[14] | p[15] << 8;
    } else {
        return std::nullopt;
    }
    if (d.w <= 0 || d.h <= 0 || d.w > kMaxIconSide || d.h > kMaxIconSide) return std::nullopt;
    return d;
}

bool better(const Drawn& a, const Drawn& b, int display_bpp) {
    const int da = std::abs(a.w - kIconCell) + std::abs(a.h - kIconCell);
    const int db = std::abs(b.w - kIconCell) + std::abs(b.h - kIconCell);
    if (da != db) return da < db;
    if (a.w * a.h != b.w * b.h) return a.w * a.h > b.w * b.h;
    const bool fa = a.bpp <= display_bpp, fb = b.bpp <= display_bpp;
    if (fa != fb) return fa;
    if (a.bpp != b.bpp) return fa ? a.bpp > b.bpp : a.bpp < b.bpp;
    return false;  // the first in group order stays
}

IconCell to_cell(const Drawn& d) {
    IconCell c;
    for (int y = 0; y < kIconCell; ++y)
        for (int x = 0; x < kIconCell; ++x) {
            const int sx = x * d.w / kIconCell, sy = y * d.h / kIconCell;
            c.bgra[static_cast<std::size_t>(y) * kIconCell + x] =
                0xFF000000u | d.rgb[static_cast<std::size_t>(sy) * d.w + sx];
        }
    return c;
}

}  // namespace

// The parsed module and its groups. A group or member is a byte range in the module's bytes.
struct IconSource::Impl {
    std::optional<NeModule> ne;
    std::optional<PeModule> pe;
    std::vector<std::pair<const std::uint8_t*, std::size_t>> groups;

    // The RT_ICON member `id` of the module, or {nullptr, 0}.
    std::pair<const std::uint8_t*, std::size_t> member(std::uint16_t id) const {
        if (ne) {
            const NeResource* r = ne->find(RT16_ICON, id);
            if (r) return {ne->data(*r), ne->size(*r)};
        } else if (pe) {
            const PeResource* r = pe->find(RTPE_ICON, id);
            if (r) return {pe->data(*r), pe->size(*r)};
        }
        return {nullptr, 0};
    }
};

IconSource::IconSource(std::vector<std::uint8_t> bytes) {
    const ModuleKind k = module_kind(bytes.data(), bytes.size());
    if (k == ModuleKind::kNone) return;
    auto impl = std::make_unique<Impl>();
    try {
        if (k == ModuleKind::kNe) {
            // Win16 ExtractIcon walks the resource table in its stored order. [REF Win16 SDK]
            impl->ne.emplace(std::move(bytes));
            for (const NeResource* g : impl->ne->all(RT16_GROUP_ICON))
                impl->groups.emplace_back(impl->ne->data(*g), impl->ne->size(*g));
        } else {
            // Win32 ExtractIcon indexes the RT_GROUP_ICON names in directory order, first
            // language. [REF Win32 SDK ExtractIcon]
            impl->pe.emplace(std::move(bytes));
            for (const PeResource* g : impl->pe->all(RTPE_GROUP_ICON)) {
                // a group whose bytes are not in the file is still counted; cell() gives nullopt
                std::pair<const std::uint8_t*, std::size_t> range{nullptr, 0};
                try {
                    range = {impl->pe->data(*g), impl->pe->size(*g)};
                } catch (const FormatError&) {
                }
                impl->groups.push_back(range);
            }
        }
    } catch (const std::exception&) {
        return;  // not a valid module, or out of memory (M3 s1 fixer): count 0
    }
    kind_ = k;
    impl_ = std::move(impl);
}

namespace {
IconSource none() { return IconSource(std::vector<std::uint8_t>{}); }
}  // namespace

IconSource IconSource::read(std::uint64_t size, const ReadAt& read_at) {
    // (M3 s1 fixer) [DESIGN ADR 0012 decision 5]: only the headers and the resource section.
    try {
        if (!read_at || size < 0x40) return none();
        std::vector<std::uint8_t> mz(0x40);
        if (!read_at(0, mz.size(), mz.data()) || mz[0] != 'M' || mz[1] != 'Z') return none();
        const std::uint64_t lfanew = le32(mz.data() + 0x3C);
        if (lfanew < 0x40 || lfanew > size || size - lfanew < 24) return none();
        std::uint8_t coff[24];
        if (!read_at(lfanew, sizeof coff, coff)) return none();
        if (coff[0] == 'N' && coff[1] == 'E') {
            if (size > kMaxNeImage) return none();  // [DESIGN ADR 0012] [OPEN]
            std::vector<std::uint8_t> all(static_cast<std::size_t>(size));
            if (!read_at(0, all.size(), all.data())) return none();
            return IconSource(std::move(all));
        }
        if (!(coff[0] == 'P' && coff[1] == 'E' && coff[2] == 0 && coff[3] == 0)) return none();
        const std::uint64_t nsec = coff[6] | coff[7] << 8;
        const std::uint64_t opt_size = coff[20] | coff[21] << 8;
        const std::uint64_t opt = lfanew + 24;
        const std::uint64_t sec = opt + opt_size;
        const std::uint64_t hdr_end = sec + nsec * 40;
        if (hdr_end > size || hdr_end > kMaxPeHeaders) return none();
        std::vector<std::uint8_t> img(static_cast<std::size_t>(hdr_end));
        if (!read_at(0, img.size(), img.data())) return none();
        // The resource data directory, as PeModule reads it.
        std::uint32_t rsrc = 0;
        if (opt_size >= 2) {
            const unsigned magic = img[opt] | img[opt + 1] << 8;
            const std::uint64_t ndir_at = magic == 0x20B ? 108 : 92;
            if ((magic == 0x10B || magic == 0x20B) && opt_size >= ndir_at + 4 + 3 * 8 &&
                le32(&img[opt + ndir_at]) > 2)
                rsrc = le32(&img[opt + ndir_at + 4 + 2 * 8]);
        }
        // The first section holding it (PeModule::file_offset's choice) keeps its raw data,
        // moved to the end of the headers; the others keep none.
        bool kept = false;
        for (std::uint64_t i = 0; i < nsec; ++i) {
            std::uint8_t* s = &img[static_cast<std::size_t>(sec + i * 40)];
            const std::uint64_t vsize = le32(s + 8), va = le32(s + 12);
            const std::uint64_t raw_size = le32(s + 16), raw_off = le32(s + 20);
            const auto put = [](std::uint8_t* q, std::uint64_t v) {
                for (int k = 0; k < 4; ++k) q[k] = static_cast<std::uint8_t>(v >> (8 * k));
            };
            if (!kept && rsrc != 0 && rsrc >= va && rsrc < va + std::max(vsize, raw_size)) {
                kept = true;
                std::uint64_t n = raw_off < size ? std::min(raw_size, size - raw_off) : 0;
                n = std::min(n, kMaxPeResources);
                const std::size_t at = img.size();
                img.resize(at + static_cast<std::size_t>(n));
                if (n && !read_at(raw_off, static_cast<std::size_t>(n), img.data() + at)) return none();
                s = &img[static_cast<std::size_t>(sec + i * 40)];  // after the resize
                put(s + 16, n);
                put(s + 20, at);
            } else {
                put(s + 16, 0);
                put(s + 20, 0);
            }
        }
        return IconSource(std::move(img));
    } catch (...) {  // std::bad_alloc and its kin: count 0 (M3 s1 fixer)
        return none();
    }
}

IconSource IconSource::load(const std::filesystem::path& path) {
    try {
        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(path, ec);
        if (ec) return none();
        std::ifstream f(path, std::ios::binary);
        if (!f) return none();
        return read(size, [&f](std::uint64_t off, std::size_t n, std::uint8_t* out) {
            f.clear();
            f.seekg(static_cast<std::streamoff>(off));
            if (!f) return false;
            f.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(n));
            return static_cast<std::size_t>(f.gcount()) == n;
        });
    } catch (...) {
        return none();
    }
}

IconSource::IconSource(IconSource&&) noexcept = default;
IconSource& IconSource::operator=(IconSource&&) noexcept = default;
IconSource::~IconSource() = default;

int IconSource::count() const { return impl_ ? static_cast<int>(impl_->groups.size()) : 0; }

std::optional<IconCell> IconSource::cell(int index, int display_bpp) const {
    if (!impl_ || index < 0 || index >= count()) return std::nullopt;
    const auto [gp, gn] = impl_->groups[static_cast<std::size_t>(index)];
    if (!gp) return std::nullopt;
    try {
        const std::vector<IconDirEntry> entries = parse_group_icon(gp, gn);
        // Rank the members by their headers, then decode in rank order until one decodes: the
        // best member that decodes, as before, without decoding the others (M3 s1 fixer).
        struct Candidate {
            Drawn head;
            const std::uint8_t* p;
            std::size_t n;
        };
        std::vector<Candidate> c;
        for (const IconDirEntry& e : entries) {
            try {
                const auto [mp, mn] = impl_->member(e.id);
                if (auto h = peek_member(mp, mn)) c.push_back({std::move(*h), mp, mn});
            } catch (const FormatError&) {
            }
        }
        std::stable_sort(c.begin(), c.end(), [display_bpp](const Candidate& a, const Candidate& b) {
            return better(a.head, b.head, display_bpp);
        });
        for (const Candidate& k : c) {
            try {
                return to_cell(draw_member(k.p, k.n));
            } catch (const FormatError&) {
                continue;  // an unreadable member is not a candidate
            }
        }
    } catch (const std::exception&) {  // FormatError, std::bad_alloc (M3 s1 fixer)
    }
    return std::nullopt;
}

std::vector<std::uint8_t> cell_to_index8(const IconCell& cell,
                                         const std::function<int(std::uint32_t rgb)>& nearest) {
    std::vector<std::uint8_t> out(cell.bgra.size());
    // icons hold few colours: resolve each once
    std::vector<std::pair<std::uint32_t, std::uint8_t>> seen;
    for (std::size_t i = 0; i < cell.bgra.size(); ++i) {
        const std::uint32_t rgb = cell.bgra[i] & 0x00FFFFFF;
        std::size_t k = 0;
        while (k < seen.size() && seen[k].first != rgb) ++k;
        if (k == seen.size()) seen.emplace_back(rgb, static_cast<std::uint8_t>(nearest(rgb)));
        out[i] = seen[k].second;
    }
    return out;
}

bool is_uniform(const IconCell& cell, std::uint32_t reference) {
    for (std::uint32_t c : cell.bgra)
        if (c != reference) return false;
    return true;
}

bool is_uniform(const std::vector<std::uint8_t>& index8, std::uint8_t reference) {
    for (std::uint8_t c : index8)
        if (c != reference) return false;
    return true;
}

}  // namespace is::fmt
