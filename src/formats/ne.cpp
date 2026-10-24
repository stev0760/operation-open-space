#include "formats/ne.h"

#include <algorithm>

namespace is::fmt {

NeModule::NeModule(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {
    Reader r(bytes_.data(), bytes_.size());
    if (r.u16() != 0x5A4D) throw FormatError("not an MZ executable");
    r.seek(0x3C);
    const std::size_t ne = r.u32();
    r.seek(ne);
    if (r.u16() != 0x454E) throw FormatError("not an NE module");
    r.seek(ne + 0x24);
    const std::size_t rsrc = ne + r.u16();
    const std::size_t resnames = ne + r.u16();

    r.seek(resnames);
    module_name_ = r.pstr();
    if (rsrc == resnames) return;  // no resource table

    auto name_at = [&](std::uint16_t rel) {
        Reader n(bytes_.data(), bytes_.size(), rsrc + rel);
        return n.pstr();
    };
    r.seek(rsrc);
    shift_ = r.u16();
    if (shift_ > 15) throw FormatError("bad resource alignment");
    for (;;) {
        const std::uint16_t type = r.u16();
        if (type == 0) break;
        const std::uint16_t count = r.u16();
        r.skip(4);
        for (unsigned i = 0; i < count; ++i) {
            NeResource e;
            e.offset = std::uint32_t{r.u16()} << shift_;
            e.length = std::uint32_t{r.u16()} << shift_;
            e.flags = r.u16();
            const std::uint16_t id = r.u16();
            r.skip(4);
            if (type & 0x8000) e.type = type & 0x7FFF; else e.type_name = name_at(type);
            if (id & 0x8000) e.id = id & 0x7FFF; else e.name = name_at(id);
            if (e.offset > bytes_.size()) throw FormatError("resource past end of file");
            e.length = static_cast<std::uint32_t>(
                std::min<std::size_t>(e.length, bytes_.size() - e.offset));
            res_.push_back(std::move(e));
        }
    }
}

NeModule NeModule::load(const std::filesystem::path& path) { return NeModule(read_file(path)); }

const NeResource* NeModule::find(std::uint16_t type, std::uint16_t id) const {
    for (const auto& e : res_)
        if (e.type == type && e.type_name.empty() && e.name.empty() && e.id == id) return &e;
    return nullptr;
}

const NeResource* NeModule::find(std::uint16_t type, const std::string& name) const {
    for (const auto& e : res_)
        if (e.type == type && e.type_name.empty() && e.name == name) return &e;
    return nullptr;
}

std::vector<const NeResource*> NeModule::all(std::uint16_t type) const {
    std::vector<const NeResource*> out;
    for (const auto& e : res_)
        if (e.type == type && e.type_name.empty()) out.push_back(&e);
    return out;
}

}  // namespace is::fmt
