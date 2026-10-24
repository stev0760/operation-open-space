// Walks the Win32 templates that res16's converters write the way user32 reads them, and
// compares what it reads with the Win16 resource the template came from. The model is Wine's
// user32: dialog.c DIALOG_ParseTemplate32 and DIALOG_GetControl32 (DLGTEMPLATE, not
// DLGTEMPLATEEX) and menu.c LoadMenuIndirectW and MENU_ParseResource (version 0). A field
// that is read past the buffer, or a walk that ends short of the buffer's end, is a template
// that user32 misreads.
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "doctest.h"
#include "formats/res16.h"

namespace win32_templates {

using Bytes = std::vector<std::uint8_t>;

struct WalkError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// A string or 0xFFFF + ordinal.
struct SzOrOrd {
    bool is_ord = false;
    std::uint16_t ord = 0;
    std::u16string str;
};

class Cursor {
public:
    explicit Cursor(const Bytes& t) : t_(t) {}
    std::size_t pos() const { return p_; }
    std::size_t size() const { return t_.size(); }

    std::uint16_t peek() const {
        if (p_ > t_.size() || t_.size() - p_ < 2) throw WalkError("word past the end at " + std::to_string(p_));
        return static_cast<std::uint16_t>(t_[p_] | t_[p_ + 1] << 8);
    }
    std::uint16_t word() {
        const std::uint16_t v = peek();
        p_ += 2;
        return v;
    }
    std::uint32_t dword() {
        const std::uint32_t lo = word();
        return lo | std::uint32_t{word()} << 16;
    }
    std::u16string sz() {
        std::u16string s;
        for (std::uint16_t c = word(); c != 0; c = word()) s.push_back(static_cast<char16_t>(c));
        return s;
    }
    SzOrOrd sz_or_ord() {
        SzOrOrd n;
        if (peek() == 0xFFFF) {
            word();
            n.is_ord = true;
            n.ord = word();
        } else {
            n.str = sz();
        }
        return n;
    }
    Bytes bytes_at(std::size_t at, std::size_t n) const {
        if (at > t_.size() || n > t_.size() - at)
            throw WalkError(std::to_string(n) + " bytes past the end at " + std::to_string(at));
        return Bytes(t_.begin() + static_cast<std::ptrdiff_t>(at),
                     t_.begin() + static_cast<std::ptrdiff_t>(at + n));
    }
    void skip(std::size_t n) {
        if (p_ > t_.size() || n > t_.size() - p_) throw WalkError("skip past the end at " + std::to_string(p_));
        p_ += n;
    }
    void seek(std::size_t at) { p_ = at; }  // checked by the next read
    void align4() { p_ = (p_ + 3) & ~std::size_t{3}; }

private:
    const Bytes& t_;
    std::size_t p_ = 0;
};

// ---- dialogs

struct WalkControl {
    std::uint32_t style = 0, exstyle = 0;
    std::int16_t x = 0, y = 0, cx = 0, cy = 0;
    std::uint16_t id = 0;
    SzOrOrd cls, text;
    Bytes data;  // the creation data after its size word
};

struct WalkDialog {
    std::uint32_t style = 0, exstyle = 0;
    std::int16_t x = 0, y = 0, cx = 0, cy = 0;
    SzOrOrd menu, cls;
    std::u16string caption;
    bool has_font = false;
    std::uint16_t point_size = 0;
    std::u16string face;
    std::vector<WalkControl> controls;
    std::size_t end = 0;  // where user32 stops reading: after the last control, before padding
};

inline WalkDialog walk_dialog(const Bytes& t) {
    Cursor c(t);
    WalkDialog d;
    d.style = c.dword();
    if (d.style == 0xFFFF0001) throw WalkError("a DLGTEMPLATEEX");
    d.exstyle = c.dword();
    const unsigned n = c.word();
    d.x = static_cast<std::int16_t>(c.word());
    d.y = static_cast<std::int16_t>(c.word());
    d.cx = static_cast<std::int16_t>(c.word());
    d.cy = static_cast<std::int16_t>(c.word());
    d.menu = c.sz_or_ord();
    d.cls = c.sz_or_ord();
    d.caption = c.sz();
    if (d.style & is::fmt::kDsSetFont) {
        d.has_font = true;
        d.point_size = c.word();
        if (d.point_size != 0x7FFF) d.face = c.sz();  // 0x7FFF: the message box font, no face
    }
    d.end = c.pos();
    for (unsigned i = 0; i < n; ++i) {
        c.align4();
        WalkControl k;
        k.style = c.dword();
        k.exstyle = c.dword();
        k.x = static_cast<std::int16_t>(c.word());
        k.y = static_cast<std::int16_t>(c.word());
        k.cx = static_cast<std::int16_t>(c.word());
        k.cy = static_cast<std::int16_t>(c.word());
        k.id = c.word();
        k.cls = c.sz_or_ord();
        k.text = c.sz_or_ord();
        // the size word, then size / 2 words: Wine's advance, which leaves out an odd last byte
        const std::size_t at = c.pos();
        const std::uint16_t size = c.word();
        if (size != 0) {
            k.data = c.bytes_at(at + 2, size);
            c.seek(at + 2 + size / 2 * 2);
        }
        d.end = c.pos();
        d.controls.push_back(std::move(k));
    }
    return d;
}

inline void check_name(const SzOrOrd& got, const is::fmt::NameOrOrd& want) {
    CHECK(got.is_ord == want.is_ord);
    if (want.is_ord) CHECK(got.ord == want.ord);
    else CHECK(got.str == is::fmt::cp1252_to_utf16(want.name));
}

// The template of `d` converted with `or_style` reads back as `d`, to the last byte.
inline void check_dialog_template(const is::fmt::Dialog16& d, const Bytes& t, std::uint32_t or_style = 0) {
    WalkDialog w;
    try {
        w = walk_dialog(t);
    } catch (const WalkError& x) {
        FAIL_CHECK("the template does not walk: " << std::string(x.what()));
        return;
    }
    CHECK(w.end == t.size());
    const std::uint32_t style = d.style | or_style;
    CHECK(w.style == style);
    CHECK(w.exstyle == 0);
    CHECK(w.x == d.x);
    CHECK(w.y == d.y);
    CHECK(w.cx == d.cx);
    CHECK(w.cy == d.cy);
    check_name(w.menu, d.menu);
    CHECK_FALSE(w.cls.is_ord);
    CHECK(w.cls.str == is::fmt::cp1252_to_utf16(d.class_name));
    CHECK(w.caption == is::fmt::cp1252_to_utf16(d.caption));
    CHECK(w.has_font == ((style & is::fmt::kDsSetFont) != 0));
    if (w.has_font) {
        CHECK(w.point_size == d.point_size);
        CHECK(w.face == is::fmt::cp1252_to_utf16(d.face));
    }
    REQUIRE(w.controls.size() == d.items.size());
    for (std::size_t i = 0; i < d.items.size(); ++i) {
        CAPTURE(i);
        const is::fmt::DialogItem16& want = d.items[i];
        const WalkControl& got = w.controls[i];
        CHECK(got.style == want.style);
        CHECK(got.exstyle == 0);
        CHECK(got.x == want.x);
        CHECK(got.y == want.y);
        CHECK(got.cx == want.cx);
        CHECK(got.cy == want.cy);
        CHECK(got.id == want.id);
        CHECK(got.cls.is_ord == (want.class_atom != 0));
        if (want.class_atom) CHECK(got.cls.ord == want.class_atom);
        else CHECK(got.cls.str == is::fmt::cp1252_to_utf16(want.class_name));
        check_name(got.text, want.text);
        CHECK(got.data == want.extra);
    }
}

// ---- menus

struct WalkMenuItem {
    std::uint16_t flags = 0;
    std::uint16_t id = 0;
    std::u16string text;
    std::vector<WalkMenuItem> children;
};

struct WalkMenu {
    std::vector<WalkMenuItem> items;
    std::size_t end = 0;
};

inline void walk_menu_level(Cursor& c, std::vector<WalkMenuItem>& out, int depth) {
    if (depth > 64) throw WalkError("menu deeper than 64 levels");
    for (;;) {
        WalkMenuItem it;
        it.flags = c.word();
        if (!(it.flags & is::fmt::kMfPopup)) it.id = c.word();
        it.text = c.sz();
        if (it.flags & is::fmt::kMfPopup) walk_menu_level(c, it.children, depth + 1);
        const bool last = it.flags & is::fmt::kMfEnd;
        out.push_back(std::move(it));
        if (last) return;
    }
}

inline WalkMenu walk_menu(const Bytes& t) {
    Cursor c(t);
    if (c.word() != 0) throw WalkError("not a version 0 menu template");
    c.skip(c.word());
    WalkMenu m;
    walk_menu_level(c, m.items, 1);
    m.end = c.pos();
    return m;
}

inline void check_menu_items(const std::vector<WalkMenuItem>& got, const std::vector<is::fmt::MenuItem16>& want) {
    REQUIRE(got.size() == want.size());
    for (std::size_t i = 0; i < want.size(); ++i) {
        CAPTURE(i);
        CHECK(got[i].flags == want[i].flags);
        CHECK(got[i].id == want[i].id);
        CHECK(got[i].text == is::fmt::cp1252_to_utf16(want[i].text));
        check_menu_items(got[i].children, want[i].children);
    }
}

// The template of `items` reads back as `items`, to the last byte.
inline void check_menu_template(const std::vector<is::fmt::MenuItem16>& items, const Bytes& t) {
    WalkMenu w;
    try {
        w = walk_menu(t);
    } catch (const WalkError& x) {
        FAIL_CHECK("the template does not walk: " << std::string(x.what()));
        return;
    }
    CHECK(w.end == t.size());
    check_menu_items(w.items, items);
}

}  // namespace win32_templates
