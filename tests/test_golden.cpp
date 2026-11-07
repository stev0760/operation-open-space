// Golden tests against the original files and the Python isre outputs (work/assets).
// They skip when IS_ORIGINAL_DIR or IS_ASSETS_DIR is unset or missing.
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "doctest.h"
#include "formats/dib.h"
#include "formats/res16.h"
#include "win32_templates.h"

using namespace is::fmt;
using win32_templates::check_dialog_template;
using win32_templates::check_menu_template;
namespace fs = std::filesystem;

namespace {
fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v || !fs::is_directory(v)) return {};
    return fs::path(v);
}

fs::path original(const char* file) {
    fs::path d = env_dir("IS_ORIGINAL_DIR");
    return d.empty() ? fs::path() : d / file;
}

std::string unescape(const std::string& s) {
    std::string o;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char c = s[++i];
            o.push_back(c == 't' ? '\t' : c == 'r' ? '\r' : c == 'n' ? '\n' : c);
        } else {
            o.push_back(s[i]);
        }
    }
    return o;
}

std::string strip_amp(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '&'), s.end());
    return s;
}

// An MZ file whose dword at 0x3C points at an "NE" signature.
bool is_ne_file(const std::vector<std::uint8_t>& b) {
    if (b.size() < 0x40 || b[0] != 'M' || b[1] != 'Z') return false;
    const std::size_t ne = b[0x3C] | (b[0x3D] << 8) | (b[0x3E] << 16) |
                           (static_cast<std::size_t>(b[0x3F]) << 24);
    return ne <= b.size() - 2 && b[ne] == 'N' && b[ne + 1] == 'E';
}

struct TemplateCounts {
    std::size_t modules = 0, dialogs = 0, menus = 0;
};

// Converts every DIALOG and MENU of every NE module of `dir` and walks each template as
// user32 reads it (win32_templates.h).
TemplateCounts walk_templates(const fs::path& dir) {
    TemplateCounts n;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        std::vector<std::uint8_t> bytes = read_file(e.path());
        if (!is_ne_file(bytes)) continue;
        const NeModule m(std::move(bytes));
        ++n.modules;
        for (const NeResource& r : m.resources()) {
            if (!r.type_name.empty()) continue;
            INFO(e.path().filename().u8string() << " " << r.type << " " << (r.name.empty() ? std::to_string(r.id) : r.name));
            if (r.type == RT16_DIALOG) {
                ++n.dialogs;
                const Dialog16 d = parse_dialog16(m.data(r), m.size(r));
                check_dialog_template(d, dialog_to_win32(d));
                check_dialog_template(d, dialog_to_win32(d, 0x10), 0x10);  // DS_NOFAILCREATE
            } else if (r.type == RT16_MENU) {
                ++n.menus;
                const auto items = parse_menu16(m.data(r), m.size(r));
                check_menu_template(items, menu_to_win32(items));
            }
        }
    }
    return n;
}
}  // namespace

TEST_CASE("ENG_LANG.DLL: strings match isre's strings.tsv") {
    fs::path f = original("ENG_LANG.DLL");
    fs::path assets = env_dir("IS_ASSETS_DIR");
    if (f.empty() || assets.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR / IS_ASSETS_DIR not available");
        return;
    }
    NeModule m = NeModule::load(f);
    CHECK(m.module_name() == "IN_DLOG");
    auto strings = all_strings(m);
    CHECK(strings.size() == 1446);
    CHECK(strings.at(2338) == "Limited Inner Space");

    std::ifstream tsv(assets / "eng_lang.dll" / "strings.tsv", std::ios::binary);
    REQUIRE(tsv);
    std::string line;
    std::getline(tsv, line);  // header
    std::size_t n = 0;
    while (std::getline(tsv, line)) {
        if (line.empty()) continue;
        const auto tab = line.find('\t');
        const auto id = static_cast<std::uint16_t>(std::stoi(line.substr(0, tab)));
        // strings.tsv is UTF-8; compare through UTF-16 so cp1252 bytes line up
        const std::string text = unescape(line.substr(tab + 1));
        auto it = strings.find(id);
        REQUIRE_MESSAGE(it != strings.end(), "missing id " << id);
        const std::u16string ours = cp1252_to_utf16(it->second);
        std::u16string theirs;
        for (std::size_t i = 0; i < text.size();) {  // minimal UTF-8 decode
            unsigned char c = static_cast<unsigned char>(text[i]);
            char32_t cp;
            int len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
            cp = len == 1 ? c : len == 2 ? (c & 0x1F) : len == 3 ? (c & 0x0F) : (c & 0x07);
            for (int k = 1; k < len; ++k) cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
            theirs.push_back(static_cast<char16_t>(cp));
            i += static_cast<std::size_t>(len);
        }
        CHECK_MESSAGE(ours == theirs, "string " << id);
        ++n;
    }
    CHECK(n == 1446);
}

TEST_CASE("ENG_LANG.DLL: every dialog, menu and accelerator table converts") {
    fs::path f = original("ENG_LANG.DLL");
    if (f.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    NeModule m = NeModule::load(f);
    auto dialogs = m.all(RT16_DIALOG);
    CHECK(dialogs.size() == 22);
    for (const NeResource* r : dialogs) {
        CAPTURE(r->id);
        Dialog16 d = parse_dialog16(m.data(*r), m.size(*r));
        check_dialog_template(d, dialog_to_win32(d));
    }
    const NeResource* ng = m.find(RT16_DIALOG, 321);
    REQUIRE(ng);
    Dialog16 d = parse_dialog16(m.data(*ng), m.size(*ng));
    CHECK(d.caption == "New Game");
    CHECK(d.face == "Helv");
    CHECK(d.point_size == 8);
    CHECK(d.cx == 198);
    CHECK(d.cy == 117);

    std::size_t menus = 0;
    for (const auto& r : m.resources())
        if (r.type == RT16_MENU) {
            ++menus;
            const auto items = parse_menu16(m.data(r), m.size(r));
            check_menu_template(items, menu_to_win32(items));
        }
    CHECK(menus == 4);
    const NeResource* bar = m.find(RT16_MENU, 101);
    REQUIRE(bar);
    auto items = parse_menu16(m.data(*bar), m.size(*bar));
    REQUIRE(items.size() == 5);
    CHECK(strip_amp(items[0].text) == "Game");
    CHECK(strip_amp(items[4].text) == "Help");

    const NeResource* main = m.find(RT16_ACCELERATOR, "MAINACCEL");
    const NeResource* fact = m.find(RT16_ACCELERATOR, "FACTACCEL");
    REQUIRE(main);
    REQUIRE(fact);
    CHECK(parse_accel16(m.data(*main), m.size(*main)).size() == 26);
    CHECK(parse_accel16(m.data(*fact), m.size(*fact)).size() == 29);
}

TEST_CASE("every DIALOG and MENU template reads back as its Win16 resource") {
    const fs::path ship = env_dir("IS_ORIGINAL_DIR");
    if (ship.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const TemplateCounts s = walk_templates(ship);
    CHECK(s.dialogs == 35u);
    CHECK(s.menus == 4u);
    // THEWORKS beside the game tree (work/extracted/theworks)
    fs::path base = ship.lexically_normal();
    if (base.filename().empty()) base = base.parent_path();
    const fs::path works = base.parent_path() / "theworks";
    if (!fs::is_directory(works)) {
        MESSAGE("theworks skipped: no theworks directory beside IS_ORIGINAL_DIR");
        return;
    }
    const TemplateCounts w = walk_templates(works);
    CHECK(w.dialogs == 5u);
    CHECK(w.menus == 1u);
}

TEST_CASE("IGRAPHIC.DLL: the sprite DIBs decode") {
    fs::path f = original("IGRAPHIC.DLL");
    if (f.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    NeModule m = NeModule::load(f);
    auto sprites = m.all(4000);
    CHECK(sprites.size() == 113);
    for (const NeResource* r : sprites) {
        Dib d = parse_dib(m.data(*r), m.size(*r));
        CHECK(d.bpp == 4);
        CHECK(d.palette.size() == 16);
    }
    const NeResource* xform = m.find(4000, 60);
    REQUIRE(xform);
    Dib d = parse_dib(m.data(*xform), m.size(*xform));
    CHECK(d.width == 132);
    CHECK(d.height == 33);
    CHECK(d.palette[15] == 0x000000);
    std::size_t named = 0;
    for (const auto& r : m.resources())
        if (r.type == RT16_BITMAP && !r.name.empty()) {
            ++named;
            CHECK_NOTHROW(parse_dib(m.data(r), m.size(r)));
        }
    CHECK(named == 15);
}
