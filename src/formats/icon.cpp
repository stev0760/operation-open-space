#include "formats/icon.h"

#include <string>

#include "formats/bytes.h"

namespace is::fmt {

std::vector<IconDirEntry> parse_group_icon(const std::uint8_t* p, std::size_t n) {
    Reader r(p, n);
    if (r.u16() != 0) throw FormatError("icon group: reserved word is not 0");
    if (r.u16() != 1) throw FormatError("icon group: not an icon directory");
    const std::uint16_t count = r.u16();
    if (count == 0) throw FormatError("icon group: no entries");
    std::vector<IconDirEntry> out;
    out.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        IconDirEntry e;
        e.width = r.u8();
        e.height = r.u8();
        e.colors = r.u8();
        e.reserved = r.u8();
        e.planes = r.u16();
        e.bpp = r.u16();
        e.bytes = r.u32();
        e.id = r.u16();
        out.push_back(e);
    }
    return out;
}

Icon parse_icon(const std::uint8_t* p, std::size_t n) {
    Icon ic;
    std::size_t mask_at = 0;
    ic.image = parse_icon_xor(p, n, mask_at);
    const std::size_t need = ic.mask_stride() * static_cast<std::size_t>(ic.image.height);
    if (n - mask_at < need) throw FormatError("icon AND mask truncated");
    ic.mask.assign(p + mask_at, p + mask_at + need);
    ic.size = mask_at + need;
    return ic;
}

bool Icon::masked(int x, int y) const {
    const int row = image.bottom_up ? image.height - 1 - y : y;
    const std::uint8_t* line = mask.data() + mask_stride() * static_cast<std::size_t>(row);
    return (line[x >> 3] >> (7 - (x & 7))) & 1;
}

std::vector<std::uint32_t> icon_to_bgra(const Icon& icon) {
    const Dib& d = icon.image;
    std::vector<std::uint32_t> out(static_cast<std::size_t>(d.width) * d.height);
    for (int y = 0; y < d.height; ++y)
        for (int x = 0; x < d.width; ++x) {
            const std::uint32_t rgb = d.rgb_at(x, y);
            out[static_cast<std::size_t>(y) * d.width + x] = icon.masked(x, y) ? rgb : 0xFF000000 | rgb;
        }
    return out;
}

namespace {
void put16(std::vector<std::uint8_t>& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void put32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    put16(b, v & 0xFFFF);
    put16(b, v >> 16);
}
}  // namespace

std::vector<std::uint8_t> build_ico(const NeModule& m, const NeResource& group) {
    const auto entries = parse_group_icon(m.data(group), m.size(group));
    struct Image {
        const std::uint8_t* data;
        std::size_t size;
    };
    std::vector<Image> images;
    for (const IconDirEntry& e : entries) {
        const NeResource* r = m.find(RT16_ICON, e.id);
        if (!r) throw FormatError("icon group member " + std::to_string(e.id) + " is missing");
        images.push_back({m.data(*r), parse_icon(m.data(*r), m.size(*r)).size});
    }
    std::vector<std::uint8_t> out;
    put16(out, 0);
    put16(out, 1);
    put16(out, static_cast<std::uint32_t>(entries.size()));
    std::size_t off = 6 + 16 * entries.size();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const IconDirEntry& e = entries[i];
        out.push_back(e.width);
        out.push_back(e.height);
        out.push_back(e.colors);
        out.push_back(0);
        put16(out, e.planes);
        put16(out, e.bpp);
        put32(out, static_cast<std::uint32_t>(images[i].size));
        put32(out, static_cast<std::uint32_t>(off));
        off += images[i].size;
    }
    for (const Image& im : images) out.insert(out.end(), im.data, im.data + im.size);
    return out;
}

}  // namespace is::fmt
