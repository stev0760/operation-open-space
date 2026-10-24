#include "formats/res16.h"

#include <algorithm>

namespace is::fmt {

namespace {

// Little-endian template writer with UTF-16 strings.
class Writer {
public:
    void u16(std::uint16_t v) {
        b_.push_back(static_cast<std::uint8_t>(v));
        b_.push_back(static_cast<std::uint8_t>(v >> 8));
    }
    void i16(std::int16_t v) { u16(static_cast<std::uint16_t>(v)); }
    void u32(std::uint32_t v) {
        u16(static_cast<std::uint16_t>(v));
        u16(static_cast<std::uint16_t>(v >> 16));
    }
    void wstr(const std::string& cp1252) {
        for (char16_t c : cp1252_to_utf16(cp1252)) u16(c);
        u16(0);
    }
    void name_or_ord(const NameOrOrd& n) {
        if (n.is_ord) {
            u16(0xFFFF);
            u16(n.ord);
        } else {
            wstr(n.name);  // an empty name is the single 0x0000 word
        }
    }
    void bytes(const std::vector<std::uint8_t>& v) { b_.insert(b_.end(), v.begin(), v.end()); }
    void align4() {
        while (b_.size() % 4) b_.push_back(0);
    }
    std::vector<std::uint8_t> take() { return std::move(b_); }

private:
    std::vector<std::uint8_t> b_;
};

// 0 = none, 0xFF + WORD = ordinal, otherwise an ASCIIZ string starting with that byte.
NameOrOrd read_name_or_ord(Reader& r) {
    NameOrOrd n;
    const std::uint8_t b = r.u8();
    if (b == 0xFF) {
        n.is_ord = true;
        n.ord = r.u16();
    } else if (b != 0) {
        n.name.push_back(static_cast<char>(b));
        n.name += r.cstr();
    }
    return n;
}

std::vector<std::string> string_block(const NeModule& m, const NeResource& r) {
    Reader rd(m.data(r), m.size(r));
    std::vector<std::string> out(16);
    for (auto& s : out) s = rd.pstr();
    return out;
}

void parse_menu_level(Reader& r, std::vector<MenuItem16>& out, int depth) {
    if (depth > kMaxMenuDepth)
        throw FormatError("MENU nesting deeper than " + std::to_string(kMaxMenuDepth) + " levels");
    for (;;) {
        MenuItem16 it;
        it.flags = r.u16();
        if (!(it.flags & kMfPopup)) it.id = r.u16();
        it.text = r.cstr();
        if (it.flags & kMfPopup) parse_menu_level(r, it.children, depth + 1);
        const bool end = it.flags & kMfEnd;
        out.push_back(std::move(it));
        if (end) return;
    }
}

void write_menu_level(Writer& w, const std::vector<MenuItem16>& items) {
    for (const auto& it : items) {
        w.u16(it.flags);
        if (!(it.flags & kMfPopup)) w.u16(it.id);
        w.wstr(it.text);
        if (it.flags & kMfPopup) write_menu_level(w, it.children);
    }
}

}  // namespace

const char* resource_type_name(std::uint16_t type) {
    switch (type) {
    case 1: return "CURSOR";
    case 2: return "BITMAP";
    case 3: return "ICON";
    case 4: return "MENU";
    case 5: return "DIALOG";
    case 6: return "STRING";
    case 7: return "FONTDIR";
    case 8: return "FONT";
    case 9: return "ACCELERATOR";
    case 10: return "RCDATA";
    case 11: return "MESSAGETABLE";
    case 12: return "GROUP_CURSOR";
    case 14: return "GROUP_ICON";
    case 15: return "NAMETABLE";
    case 16: return "VERSION";
    case 17: return "DLGINCLUDE";
    case 19: return "PLUGPLAY";
    case 20: return "VXD";
    case 21: return "ANICURSOR";
    case 22: return "ANIICON";
    case 23: return "HTML";
    case 24: return "MANIFEST";
    default: return nullptr;
    }
}

std::optional<std::string> load_string(const NeModule& m, std::uint16_t id) {
    const NeResource* r = m.find(RT16_STRING, static_cast<std::uint16_t>((id >> 4) + 1));
    if (!r) return std::nullopt;
    std::string s = string_block(m, *r)[id & 15];
    if (s.empty()) return std::nullopt;
    return s;
}

std::map<std::uint16_t, std::string> all_strings(const NeModule& m) {
    std::map<std::uint16_t, std::string> out;
    for (const NeResource* r : m.all(RT16_STRING)) {
        if (r->id == 0) continue;
        const auto block = string_block(m, *r);
        for (unsigned i = 0; i < 16; ++i)
            if (!block[i].empty()) out[static_cast<std::uint16_t>((r->id - 1) * 16 + i)] = block[i];
    }
    return out;
}

Dialog16 parse_dialog16(const std::uint8_t* p, std::size_t n) {
    Reader r(p, n);
    Dialog16 d;
    d.style = r.u32();
    const unsigned count = r.u8();
    d.x = r.i16();
    d.y = r.i16();
    d.cx = r.i16();
    d.cy = r.i16();
    d.menu = read_name_or_ord(r);
    d.class_name = r.cstr();
    d.caption = r.cstr();
    if (d.style & kDsSetFont) {
        d.point_size = r.u16();
        d.face = r.cstr();
    }
    for (unsigned i = 0; i < count; ++i) {
        DialogItem16 it;
        it.x = r.i16();
        it.y = r.i16();
        it.cx = r.i16();
        it.cy = r.i16();
        it.id = r.u16();
        it.style = r.u32();
        const std::uint8_t c = r.u8();
        if (c & 0x80) {
            it.class_atom = c;
        } else if (c != 0) {  // 0: an empty name, which the Win32 template writes as one 0x0000
            it.class_name.push_back(static_cast<char>(c));
            it.class_name += r.cstr();
        }
        it.text = read_name_or_ord(r);
        const unsigned extra = r.u8();
        it.extra.assign(r.ptr(), r.ptr() + std::min<std::size_t>(extra, r.remaining()));
        r.skip(extra);
        d.items.push_back(std::move(it));
    }
    return d;
}

std::vector<std::uint8_t> dialog_to_win32(const Dialog16& d, std::uint32_t or_style) {
    Writer w;
    const std::uint32_t style = d.style | or_style;
    w.u32(style);
    w.u32(0);  // dwExtendedStyle
    w.u16(static_cast<std::uint16_t>(d.items.size()));
    w.i16(d.x);
    w.i16(d.y);
    w.i16(d.cx);
    w.i16(d.cy);
    w.name_or_ord(d.menu);
    w.wstr(d.class_name);
    w.wstr(d.caption);
    if (style & kDsSetFont) {  // also when or_style adds it (DS_SHELLFONT includes it)
        w.u16(d.point_size);
        w.wstr(d.face);
    }
    for (const auto& it : d.items) {
        w.align4();
        w.u32(it.style);
        w.u32(0);
        w.i16(it.x);
        w.i16(it.y);
        w.i16(it.cx);
        w.i16(it.cy);
        w.u16(it.id);
        if (it.class_atom) {
            w.u16(0xFFFF);
            w.u16(it.class_atom);
        } else {
            w.wstr(it.class_name);
        }
        w.name_or_ord(it.text);
        w.u16(static_cast<std::uint16_t>(it.extra.size()));
        w.bytes(it.extra);
    }
    return w.take();
}

std::vector<MenuItem16> parse_menu16(const std::uint8_t* p, std::size_t n) {
    Reader r(p, n);
    r.u16();                  // version
    const std::uint16_t off = r.u16();
    r.skip(off);              // bytes between the header and the first item
    std::vector<MenuItem16> out;
    parse_menu_level(r, out, 1);
    return out;
}

std::vector<std::uint8_t> menu_to_win32(const std::vector<MenuItem16>& items) {
    Writer w;
    w.u16(0);  // versionNumber
    w.u16(0);  // offset
    write_menu_level(w, items);
    return w.take();
}

std::vector<Accel16> parse_accel16(const std::uint8_t* p, std::size_t n) {
    Reader r(p, n);
    std::vector<Accel16> out;
    for (;;) {
        Accel16 a;
        a.flags = r.u8();
        a.key = r.u16();
        a.cmd = r.u16();
        const bool last = a.flags & 0x80;
        a.flags &= 0x7F;
        out.push_back(a);
        if (last) return out;
    }
}

}  // namespace is::fmt
