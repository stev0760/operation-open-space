#include "formats/records.h"

#include <algorithm>
#include <limits>
#include <string>

#include "formats/bytes.h"

namespace is::fmt {

namespace {
const NeResource& need(const NeModule& m, std::uint16_t type, std::uint16_t id) {
    const NeResource* r = m.find(type, id);
    if (!r)
        throw FormatError(m.module_name() + ": resource #" + std::to_string(type) + " " +
                          std::to_string(id) + " is missing");
    return *r;
}
}  // namespace

Palette5500 parse_palette5500(const std::uint8_t* p, std::size_t n) {
    Reader r(p, n);
    Palette5500 pal;
    for (Rgb8& c : pal.colours) {
        c.r = r.u8();
        c.g = r.u8();
        c.b = r.u8();
    }
    return pal;
}

Palette5500 load_palette5500(const NeModule& ispace) {
    const NeResource& r = need(ispace, Palette5500::kType, Palette5500::kId);
    return parse_palette5500(ispace.data(r), ispace.size(r));
}

Table5600 parse_table5600(const std::uint8_t* p, std::size_t n) {
    Reader r(p, n);
    Table5600 t;
    for (auto& row : t.rows)
        for (std::int16_t& w : row) w = r.i16();
    return t;
}

Table5600 load_table5600(const NeModule& ispace) {
    const NeResource& r = need(ispace, Table5600::kType, Table5600::kId);
    return parse_table5600(ispace.data(r), ispace.size(r));
}

std::size_t global_size(std::size_t n, std::size_t granularity) {
    if (granularity == 0 || (granularity & (granularity - 1)) != 0 || granularity > kMaxGlobalGranularity)
        throw FormatError("GlobalSize granularity is not a power of two up to 64 KiB");
    if (n > std::numeric_limits<std::size_t>::max() - (granularity - 1))
        throw FormatError("GlobalSize overflows");
    return (n + granularity - 1) & ~(granularity - 1);
}

Scene6600 parse_scene6600(const std::uint8_t* p, std::size_t n, std::size_t granularity) {
    global_size(0, granularity);  // reject a bad granularity before reading
    Reader r(p, n);
    Scene6600 s;
    for (std::uint8_t& b : s.header) b = r.u8();
    r.seek(0x50);
    s.byte_50 = r.u8();
    s.byte_51 = r.u8();
    r.seek(0x5c);
    s.dword_5c = r.i32();
    s.dword_60 = r.i32();
    s.body_offset = global_size(Scene6600::kHeaderSize, granularity);
    r.seek(s.body_offset);  // throws when the resource ends inside the header block
    s.body_length = r.remaining();
    s.body.assign(global_size(s.body_length, granularity), 0);  // GMEM_ZEROINIT
    std::copy(r.ptr(), r.ptr() + s.body_length, s.body.begin());
    for (std::size_t at = 0; s.body.size() - at >= Scene6600::kRecordSize; at += Scene6600::kRecordSize) {
        std::array<std::uint8_t, Scene6600::kRecordSize> rec{};
        for (std::size_t k = 0; k < rec.size(); ++k) rec[k] = s.body[at + k];
        s.records.push_back(rec);
    }
    return s;
}

Scene6600 load_scene6600(const NeModule& igraphic, std::uint16_t id, std::size_t granularity) {
    const NeResource& r = need(igraphic, Scene6600::kType, id);
    return parse_scene6600(igraphic.data(r), igraphic.size(r), granularity);
}

Bytes5000 parse_bytes5000(const std::uint8_t* p, std::size_t n) {
    Bytes5000 b;
    b.bytes.assign(p, p + n);
    b.length = n;
    while (b.length > 0 && b.bytes[b.length - 1] == 0) --b.length;
    return b;
}

Bytes5000 load_bytes5000(const NeModule& ispace) {
    const NeResource& r = need(ispace, Bytes5000::kType, Bytes5000::kId);
    return parse_bytes5000(ispace.data(r), ispace.size(r));
}

}  // namespace is::fmt
