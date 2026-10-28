#include "formats/pe.h"

#include <algorithm>

namespace is::fmt {

namespace {
constexpr std::size_t kMaxSections = 96;       // the PE/COFF loader's limit [REF PE/COFF spec]
constexpr std::size_t kMaxLeaves = 65536;      // bounds for hostile directories [DESIGN ADR 0012]
constexpr std::size_t kMaxEntries = 1u << 20;   // directory entries read, all levels together
// UTF-16 units of resource names read and copied into the leaves, all together: every entry may
// name the same 65535-unit string (M3 s1 fixer) [DESIGN ADR 0012].
constexpr std::size_t kMaxNameUnits = 1u << 20;
constexpr std::uint32_t kSubdir = 0x80000000u; // IMAGE_RESOURCE_DATA_IS_DIRECTORY
constexpr std::uint32_t kNamed = 0x80000000u;  // IMAGE_RESOURCE_NAME_IS_STRING

std::size_t lfanew(const std::uint8_t* p, std::size_t n) {
    if (n < 0x40 || p[0] != 'M' || p[1] != 'Z') return 0;
    return std::size_t{p[0x3C]} | std::size_t{p[0x3D]} << 8 | std::size_t{p[0x3E]} << 16 |
           std::size_t{p[0x3F]} << 24;
}
}  // namespace

ModuleKind module_kind(const std::uint8_t* p, std::size_t n) {
    const std::size_t at = lfanew(p, n);
    if (at == 0 || at > n || n - at < 2) return ModuleKind::kNone;
    if (p[at] == 'N' && p[at + 1] == 'E') return ModuleKind::kNe;
    if (n - at >= 4 && p[at] == 'P' && p[at + 1] == 'E' && p[at + 2] == 0 && p[at + 3] == 0)
        return ModuleKind::kPe;
    return ModuleKind::kNone;
}

PeModule::PeModule(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {
    Reader r(bytes_.data(), bytes_.size());
    if (r.u16() != 0x5A4D) throw FormatError("not an MZ executable");
    r.seek(0x3C);
    const std::size_t pe = r.u32();
    r.seek(pe);
    if (r.u32() != 0x00004550) throw FormatError("not a PE image");
    machine_ = r.u16();
    const std::size_t nsec = r.u16();
    r.skip(12);  // time stamp, symbol table, symbol count
    const std::size_t opt_size = r.u16();
    r.u16();     // characteristics
    const std::size_t opt = r.pos();
    if (nsec > kMaxSections) throw FormatError("too many PE sections");
    if (opt_size < 2) throw FormatError("PE optional header missing");
    const std::uint16_t magic = r.u16();
    if (magic != 0x10B && magic != 0x20B) throw FormatError("unknown PE optional header");
    pe32_plus_ = magic == 0x20B;
    const std::size_t ndir_at = pe32_plus_ ? 108 : 92;
    std::uint32_t rsrc_rva = 0, rsrc_size = 0;
    if (opt_size >= ndir_at + 4) {
        r.seek(opt + ndir_at);
        const std::uint32_t ndirs = r.u32();
        // data directory 2 is the resource table
        if (ndirs > 2 && opt_size >= ndir_at + 4 + 3 * 8) {
            r.seek(opt + ndir_at + 4 + 2 * 8);
            rsrc_rva = r.u32();
            rsrc_size = r.u32();
        }
    }
    if (opt_size > bytes_.size() - opt) throw FormatError("PE optional header past end");
    r.seek(opt + opt_size);
    for (std::size_t i = 0; i < nsec; ++i) {
        PeSection s;
        for (int k = 0; k < 8; ++k) {
            const char c = static_cast<char>(r.u8());
            if (c) s.name.push_back(c);
        }
        s.virtual_size = r.u32();
        s.virtual_address = r.u32();
        s.raw_size = r.u32();
        s.raw_offset = r.u32();
        r.skip(16);  // relocations, line numbers, their counts, characteristics
        sections_.push_back(std::move(s));
    }
    if (rsrc_rva != 0) read_resources(rsrc_rva, rsrc_size);
}

PeModule PeModule::load(const std::filesystem::path& path) { return PeModule(read_file(path)); }

std::size_t PeModule::file_offset(std::uint32_t rva, std::uint32_t size) const {
    for (const PeSection& s : sections_) {
        const std::uint64_t va = s.virtual_address;
        const std::uint64_t span = std::max(s.virtual_size, s.raw_size);
        if (rva < va || rva >= va + span) continue;
        const std::uint64_t rel = rva - va;
        if (rel + size > s.raw_size) throw FormatError("PE data past its section's raw data");
        const std::uint64_t off = std::uint64_t{s.raw_offset} + rel;
        if (off + size > bytes_.size()) throw FormatError("PE data past end of file");
        return static_cast<std::size_t>(off);
    }
    throw FormatError("PE address in no section");
}

void PeModule::read_resources(std::uint32_t rva, std::uint32_t size) {
    (void)size;  // the directory's own size is often wrong; the section's raw data bounds it
    const std::size_t base = file_offset(rva, 16);
    // the end of the section holding the directory
    std::size_t limit = bytes_.size();
    for (const PeSection& s : sections_) {
        const std::uint64_t va = s.virtual_address;
        if (rva >= va && rva < va + std::max(s.virtual_size, s.raw_size)) {
            limit = static_cast<std::size_t>(
                std::min<std::uint64_t>(bytes_.size(), std::uint64_t{s.raw_offset} + s.raw_size));
            break;
        }
    }
    Reader r(bytes_.data(), limit);

    struct Key {
        std::uint16_t id = 0;
        std::u16string name;
        bool subdir = false;
        std::uint32_t target = 0;
    };
    std::size_t read = 0;
    std::size_t name_units = 0;
    const auto spend = [&name_units](std::size_t units) {
        name_units += units;
        if (name_units > kMaxNameUnits) throw FormatError("PE resource names too long");
    };
    // IMAGE_RESOURCE_DIRECTORY and its entries at `off` (relative to the directory base).
    auto entries = [&](std::uint32_t off) {
        if (off > limit - base) throw FormatError("PE resource directory past end");
        r.seek(base + off);
        r.skip(12);  // characteristics, time stamp, version
        const std::size_t named = r.u16();
        const std::size_t count = named + r.u16();
        read += count;
        if (read > kMaxEntries) throw FormatError("too many PE resource directory entries");
        std::vector<Key> out;
        out.reserve(std::min<std::size_t>(count, r.remaining() / 8));
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint32_t name = r.u32();
            const std::uint32_t data = r.u32();
            Key k;
            k.subdir = (data & kSubdir) != 0;
            k.target = data & ~kSubdir;
            if (name & kNamed) {
                const std::size_t here = r.pos();
                const std::uint32_t at = name & ~kNamed;
                if (at > limit - base) throw FormatError("PE resource name past end");
                r.seek(base + at);
                const std::size_t len = r.u16();
                spend(len);
                for (std::size_t c = 0; c < len; ++c) k.name.push_back(static_cast<char16_t>(r.u16()));
                r.seek(here);
            } else {
                k.id = static_cast<std::uint16_t>(name);
            }
            out.push_back(std::move(k));
        }
        return out;
    };

    // Exactly three levels (type, name, language), so a directory that points back at an
    // ancestor is never followed further; entries of the wrong shape are skipped. [DESIGN]
    for (const Key& t : entries(0)) {
        if (!t.subdir) continue;
        for (const Key& n : entries(t.target)) {
            if (!n.subdir) continue;
            for (const Key& l : entries(n.target)) {
                if (l.subdir) continue;
                if (l.target > limit - base) throw FormatError("PE resource data entry past end");
                r.seek(base + l.target);
                spend(t.name.size() + n.name.size());
                PeResource e;
                e.type = t.name.empty() ? t.id : 0;
                e.type_name = t.name;
                e.id = n.name.empty() ? n.id : 0;
                e.name = n.name;
                e.lang = l.id;
                e.rva = r.u32();
                e.size = r.u32();
                e.codepage = r.u32();
                if (res_.size() >= kMaxLeaves) throw FormatError("too many PE resources");
                res_.push_back(std::move(e));
            }
        }
    }
}

const PeResource* PeModule::find(std::uint16_t type, std::uint16_t id) const {
    for (const auto& e : res_)
        if (e.type == type && e.type_name.empty() && e.name.empty() && e.id == id) return &e;
    return nullptr;
}

const PeResource* PeModule::find(std::uint16_t type, const std::u16string& name) const {
    for (const auto& e : res_)
        if (e.type == type && e.type_name.empty() && !name.empty() && e.name == name) return &e;
    return nullptr;
}

std::vector<const PeResource*> PeModule::all(std::uint16_t type) const {
    std::vector<const PeResource*> out;
    for (std::size_t i = 0; i < res_.size(); ++i) {
        const PeResource& e = res_[i];
        if (e.type != type || !e.type_name.empty()) continue;
        // a later language of the name just listed
        if (!out.empty() && out.back()->id == e.id && out.back()->name == e.name) continue;
        out.push_back(&e);
    }
    return out;
}

}  // namespace is::fmt
