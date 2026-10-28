// Builds small synthetic PE images with a resource directory for the reader tests.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PeFakeRes {
    std::uint16_t type;
    std::uint16_t id;      // 0 with a name
    std::u16string name;
    std::uint16_t lang;
    std::vector<std::uint8_t> data;
};

namespace pe_builder {
inline void w16(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v) {
    if (b.size() < at + 2) b.resize(at + 2);
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
}
inline void w32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v) {
    w16(b, at, v & 0xFFFF);
    w16(b, at + 2, v >> 16);
}
}  // namespace pe_builder

// MZ stub, PE header at 0x40, one ".rsrc" section at RVA 0x1000, file offset 0x200. Resources
// keep the given order within each level; types, names and languages group by first appearance.
inline std::vector<std::uint8_t> build_pe(const std::vector<PeFakeRes>& res, bool pe32_plus = false) {
    using namespace pe_builder;
    const std::uint32_t kVa = 0x1000, kRaw = 0x200;
    // the tree: type -> name -> lang -> resource index
    struct Name { std::uint16_t id; std::u16string name; std::vector<std::size_t> leaves; };
    struct Type { std::uint16_t type; std::vector<Name> names; };
    std::vector<Type> types;
    for (std::size_t i = 0; i < res.size(); ++i) {
        const PeFakeRes& r = res[i];
        Type* t = nullptr;
        for (auto& x : types) if (x.type == r.type) t = &x;
        if (!t) { types.push_back({r.type, {}}); t = &types.back(); }
        Name* n = nullptr;
        for (auto& x : t->names) if (x.id == r.id && x.name == r.name) n = &x;
        if (!n) { t->names.push_back({r.id, r.name, {}}); n = &t->names.back(); }
        n->leaves.push_back(i);
    }
    auto dir_size = [](std::size_t k) { return 16 + 8 * k; };
    std::size_t at = dir_size(types.size());
    std::vector<std::size_t> type_dir, name_dir_start;
    for (auto& t : types) { type_dir.push_back(at); at += dir_size(t.names.size()); }
    std::vector<std::vector<std::size_t>> lang_dir(types.size());
    for (std::size_t ti = 0; ti < types.size(); ++ti)
        for (auto& n : types[ti].names) { lang_dir[ti].push_back(at); at += dir_size(n.leaves.size()); }
    std::vector<std::size_t> data_entry(res.size());
    for (std::size_t i = 0; i < res.size(); ++i) { data_entry[i] = at; at += 16; }
    std::vector<std::uint8_t> d(at, 0);
    // names
    auto put_name = [&](const std::u16string& s) {
        const std::size_t off = d.size();
        w16(d, off, static_cast<std::uint32_t>(s.size()));
        for (std::size_t k = 0; k < s.size(); ++k) w16(d, off + 2 + 2 * k, s[k]);
        while (d.size() % 4) d.push_back(0);
        return static_cast<std::uint32_t>(off) | 0x80000000u;
    };
    auto put_dir = [&](std::size_t off, std::size_t named, std::size_t ids) {
        w16(d, off + 12, static_cast<std::uint32_t>(named));
        w16(d, off + 14, static_cast<std::uint32_t>(ids));
    };
    put_dir(0, 0, types.size());
    for (std::size_t ti = 0; ti < types.size(); ++ti) {
        w32(d, 16 + 8 * ti, types[ti].type);
        w32(d, 16 + 8 * ti + 4, static_cast<std::uint32_t>(type_dir[ti]) | 0x80000000u);
        std::size_t named = 0;
        for (auto& n : types[ti].names) named += n.name.empty() ? 0 : 1;
        put_dir(type_dir[ti], named, types[ti].names.size() - named);
        for (std::size_t ni = 0; ni < types[ti].names.size(); ++ni) {
            const Name& n = types[ti].names[ni];
            const std::size_t e = type_dir[ti] + 16 + 8 * ni;
            w32(d, e, n.name.empty() ? n.id : put_name(n.name));
            w32(d, e + 4, static_cast<std::uint32_t>(lang_dir[ti][ni]) | 0x80000000u);
            put_dir(lang_dir[ti][ni], 0, n.leaves.size());
            for (std::size_t li = 0; li < n.leaves.size(); ++li) {
                const std::size_t l = lang_dir[ti][ni] + 16 + 8 * li;
                w32(d, l, res[n.leaves[li]].lang);
                w32(d, l + 4, static_cast<std::uint32_t>(data_entry[n.leaves[li]]));
            }
        }
    }
    for (std::size_t i = 0; i < res.size(); ++i) {
        while (d.size() % 8) d.push_back(0);
        const std::size_t off = d.size();
        d.insert(d.end(), res[i].data.begin(), res[i].data.end());
        w32(d, data_entry[i], kVa + static_cast<std::uint32_t>(off));
        w32(d, data_entry[i] + 4, static_cast<std::uint32_t>(res[i].data.size()));
    }
    while (d.size() % 0x200) d.push_back(0);

    std::vector<std::uint8_t> b(kRaw, 0);
    b[0] = 'M'; b[1] = 'Z';
    w32(b, 0x3C, 0x40);
    b[0x40] = 'P'; b[0x41] = 'E';
    const std::size_t coff = 0x44;
    const std::uint16_t opt_size = pe32_plus ? 0xF0 : 0xE0;
    w16(b, coff, pe32_plus ? 0x8664 : 0x14C);
    w16(b, coff + 2, 1);
    w16(b, coff + 16, opt_size);
    const std::size_t opt = coff + 20;
    w16(b, opt, pe32_plus ? 0x20B : 0x10B);
    const std::size_t ndirs = opt + (pe32_plus ? 108 : 92);
    w32(b, ndirs, 16);
    w32(b, ndirs + 4 + 2 * 8, kVa);
    w32(b, ndirs + 4 + 2 * 8 + 4, static_cast<std::uint32_t>(at));
    const std::size_t sec = opt + opt_size;
    const char nm[] = ".rsrc";
    for (int k = 0; k < 5; ++k) b[sec + k] = static_cast<std::uint8_t>(nm[k]);
    w32(b, sec + 8, static_cast<std::uint32_t>(d.size()));
    w32(b, sec + 12, kVa);
    w32(b, sec + 16, static_cast<std::uint32_t>(d.size()));
    w32(b, sec + 20, kRaw);
    b.insert(b.end(), d.begin(), d.end());
    return b;
}
