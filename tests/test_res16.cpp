#include <cstring>

#include "doctest.h"
#include "formats/res16.h"
#include "ne_builder.h"

using namespace is::fmt;

namespace {
std::uint16_t w16(const std::vector<std::uint8_t>& b, std::size_t at) {
    return static_cast<std::uint16_t>(b[at] | b[at + 1] << 8);
}
std::uint32_t w32(const std::vector<std::uint8_t>& b, std::size_t at) {
    return w16(b, at) | std::uint32_t{w16(b, at + 2)} << 16;
}
void add(std::vector<std::uint8_t>& b, std::initializer_list<int> v) {
    for (int x : v) b.push_back(static_cast<std::uint8_t>(x));
}
void adds(std::vector<std::uint8_t>& b, const char* s) {
    b.insert(b.end(), s, s + std::strlen(s) + 1);
}
}  // namespace

TEST_CASE("strings come from block (id >> 4) + 1, slot id & 15") {
    std::vector<std::string> slots(16);
    slots[2] = "Limited Inner Space";  // id 2338 = block 147, slot 2
    NeModule m(build_ne("IN_DLOG", {{RT16_STRING, 147, "", string_block(slots)}}));
    CHECK(load_string(m, 2338) == std::optional<std::string>("Limited Inner Space"));
    CHECK_FALSE(load_string(m, 2339));
    CHECK_FALSE(load_string(m, 10));
    auto all = all_strings(m);
    CHECK(all.size() == 1);
    CHECK(all.at(2338) == "Limited Inner Space");
}

TEST_CASE("a Win16 dialog converts to a DWORD-aligned Win32 DLGTEMPLATE") {
    std::vector<std::uint8_t> d;
    add(d, {0xC0, 0x00, 0xC0, 0x90});  // style 0x90C000C0 (DS_SETFONT, DS_MODALFRAME, ...)
    add(d, {2});                       // item count
    add(d, {62, 0, 18, 0, 198, 0, 117, 0});
    add(d, {0});                       // no menu
    add(d, {0});                       // default class
    adds(d, "New Game");
    add(d, {8, 0});
    adds(d, "Helv");
    // item 1: BUTTON "&OK", id 1
    add(d, {146, 0, 7, 0, 47, 0, 14, 0, 1, 0, 0x01, 0x00, 0x01, 0x50, 0x80});
    adds(d, "&OK");
    add(d, {0});
    // item 2: STATIC icon by ordinal 0x0100, custom extra bytes
    add(d, {6, 0, 9, 0, 32, 0, 10, 0, 108, 0, 0x03, 0x00, 0x00, 0x50, 0x82, 0xFF, 0x00, 0x01, 2, 0xAA, 0xBB});
    Dialog16 dlg = parse_dialog16(d.data(), d.size());
    CHECK(dlg.caption == "New Game");
    CHECK(dlg.face == "Helv");
    CHECK(dlg.point_size == 8);
    REQUIRE(dlg.items.size() == 2);
    CHECK(dlg.items[0].class_atom == 0x80);
    CHECK(dlg.items[0].text.name == "&OK");
    CHECK(dlg.items[1].text.is_ord);
    CHECK(dlg.items[1].text.ord == 0x100);
    CHECK(dlg.items[1].extra == std::vector<std::uint8_t>{0xAA, 0xBB});

    auto t = dialog_to_win32(dlg, 0x10);
    CHECK(w32(t, 0) == (0x90C000C0u | 0x10));
    CHECK(w32(t, 4) == 0);
    CHECK(w16(t, 8) == 2);
    CHECK(w16(t, 14) == 198);
    CHECK(w16(t, 18) == 0);       // menu: none
    CHECK(w16(t, 20) == 0);       // class: default
    CHECK(w16(t, 22) == u'N');    // caption "New Game"
    std::size_t p = 22 + 9 * 2;
    CHECK(w16(t, p) == 8);        // point size
    CHECK(w16(t, p + 2) == u'H'); // "Helv"
    p += 2 + 5 * 2;
    p = (p + 3) & ~std::size_t{3};
    CHECK(w32(t, p) == 0x50010001);
    CHECK(w16(t, p + 16) == 1);       // id
    CHECK(w16(t, p + 18) == 0xFFFF);  // class atom
    CHECK(w16(t, p + 20) == 0x0080);
    CHECK(w16(t, p + 22) == u'&');
    p += 22 + 4 * 2;
    CHECK(w16(t, p) == 0);            // no creation data
    p = (p + 2 + 3) & ~std::size_t{3};
    CHECK(w16(t, p + 16) == 108);
    CHECK(w16(t, p + 20) == 0x0082);
    CHECK(w16(t, p + 22) == 0xFFFF);  // text by ordinal
    CHECK(w16(t, p + 24) == 0x0100);
    CHECK(w16(t, p + 26) == 2);       // creation data size, then the bytes
    CHECK(t[p + 28] == 0xAA);
}

TEST_CASE("a Win16 menu keeps its structure and widens its text") {
    std::vector<std::uint8_t> m;
    add(m, {0, 0, 0, 0});        // header
    add(m, {0x10, 0});           // popup "&Game"
    adds(m, "&Game");
    add(m, {0x00, 0, 192, 0});   // item 192
    adds(m, "&Options");
    add(m, {0x80, 0, 176, 0});   // last item of the popup
    adds(m, "E&xit");
    add(m, {0x80, 0, 240, 0});   // last top-level item
    adds(m, "&Help");
    auto items = parse_menu16(m.data(), m.size());
    REQUIRE(items.size() == 2);
    CHECK(items[0].text == "&Game");
    REQUIRE(items[0].children.size() == 2);
    CHECK(items[0].children[1].id == 176);
    CHECK(items[1].id == 240);
    auto t = menu_to_win32(items);
    CHECK(w16(t, 0) == 0);
    CHECK(w16(t, 4) == 0x10);
    CHECK(w16(t, 6) == u'&');           // popups carry no id
    CHECK(w16(t, 6 + 6 * 2) == 0x00);   // first child flags
    CHECK(w16(t, 6 + 6 * 2 + 2) == 192);
}

TEST_CASE("accelerator entries end at flag 0x80") {
    const std::uint8_t a[] = {0x11, 'G', 0, 192, 0, 0x91, 'X', 0, 176, 0, 0xEE};
    auto v = parse_accel16(a, sizeof a);
    REQUIRE(v.size() == 2);
    CHECK(v[0].flags == 0x11);
    CHECK(v[1].flags == 0x11);
    CHECK(v[1].cmd == 176);
}
