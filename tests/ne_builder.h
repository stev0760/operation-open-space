// Builds small synthetic NE modules for the reader tests.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FakeRes {
    std::uint16_t type;
    std::uint16_t id;          // 0 with a name
    std::string name;
    std::vector<std::uint8_t> data;
};

inline void put16(std::vector<std::uint8_t>& b, std::size_t at, std::uint16_t v) {
    if (b.size() < at + 2) b.resize(at + 2);
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
}

// MZ stub, NE header at 0x40, resource table at NE+0x40, alignment shift 4.
inline std::vector<std::uint8_t> build_ne(const std::string& module, const std::vector<FakeRes>& res) {
    std::vector<std::uint8_t> b(0x80, 0);
    b[0] = 'M'; b[1] = 'Z';
    b[0x3C] = 0x40;
    b[0x40] = 'N'; b[0x41] = 'E';
    const std::size_t ne = 0x40, rt = ne + 0x40;
    std::vector<std::uint8_t> t;
    put16(t, 0, 4);
    // one TYPEINFO per resource keeps the builder simple
    std::size_t names_at = 2 + res.size() * (8 + 12) + 2;
    std::vector<std::uint8_t> names;
    std::vector<std::size_t> name_off(res.size(), 0);
    for (std::size_t i = 0; i < res.size(); ++i)
        if (!res[i].name.empty()) {
            name_off[i] = names_at + names.size();
            names.push_back(static_cast<std::uint8_t>(res[i].name.size()));
            names.insert(names.end(), res[i].name.begin(), res[i].name.end());
        }
    names.push_back(0);
    const std::size_t resnames = rt + names_at + names.size();
    std::size_t data_at = (resnames + 1 + module.size() + 1 + 15) & ~std::size_t{15};
    std::vector<std::size_t> offs;
    for (const auto& r : res) {
        offs.push_back(data_at);
        data_at = (data_at + r.data.size() + 15) & ~std::size_t{15};
    }
    std::size_t p = 2;
    for (std::size_t i = 0; i < res.size(); ++i) {
        put16(t, p, static_cast<std::uint16_t>(0x8000 | res[i].type));
        put16(t, p + 2, 1);
        put16(t, p + 4, 0);
        put16(t, p + 6, 0);
        put16(t, p + 8, static_cast<std::uint16_t>(offs[i] >> 4));
        put16(t, p + 10, static_cast<std::uint16_t>((res[i].data.size() + 15) >> 4));
        put16(t, p + 12, 0x30);
        put16(t, p + 14, res[i].name.empty() ? static_cast<std::uint16_t>(0x8000 | res[i].id)
                                             : static_cast<std::uint16_t>(name_off[i]));
        put16(t, p + 16, 0);
        put16(t, p + 18, 0);
        p += 20;
    }
    put16(t, p, 0);
    t.insert(t.end(), names.begin(), names.end());
    put16(b, ne + 0x24, static_cast<std::uint16_t>(rt - ne));
    put16(b, ne + 0x26, static_cast<std::uint16_t>(resnames - ne));
    b.resize(rt);
    b.insert(b.end(), t.begin(), t.end());
    b.push_back(static_cast<std::uint8_t>(module.size()));
    b.insert(b.end(), module.begin(), module.end());
    b.push_back(0);
    for (std::size_t i = 0; i < res.size(); ++i) {
        b.resize(offs[i]);
        b.insert(b.end(), res[i].data.begin(), res[i].data.end());
    }
    b.resize(data_at);
    return b;
}

inline std::vector<std::uint8_t> string_block(const std::vector<std::string>& slots) {
    std::vector<std::uint8_t> d;
    for (std::size_t i = 0; i < 16; ++i) {
        const std::string s = i < slots.size() ? slots[i] : std::string();
        d.push_back(static_cast<std::uint8_t>(s.size()));
        d.insert(d.end(), s.begin(), s.end());
    }
    return d;
}
