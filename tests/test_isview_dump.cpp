// isview's Win32-free parts (tools/isview/text.h, dump_json.h and wave_columns.h): the text
// conversions, the waveform's bounds, and the --dump JSON. The golden test builds the JSON
// of every NE module of the installed copy (IS_ORIGINAL_DIR), the distribution beside it
// (original/shipdisk, found from IS_ASSETS_DIR) and the THEWORKS directories, and compares it
// with isre's work/assets/<module>/index.json (IS_ASSETS_DIR). It skips like the other golden
// tests when the directories are not available.
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../tools/isview/dump_json.h"
#include "../tools/isview/text.h"
#include "../tools/isview/wave_columns.h"
#include "doctest.h"
#include "ne_builder.h"

using namespace is::isview;
using is::fmt::NeModule;
using is::fmt::Wave;
namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<std::uint8_t>;

// ---- a minimal JSON reader; strings stay UTF-8, numbers are integers

struct Json {
    enum Kind { Null, Bool, Num, Str, Arr, Obj } kind = Null;
    long long num = 0;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;  // in document order

    bool has(const std::string& k) const {
        for (const auto& kv : obj)
            if (kv.first == k) return true;
        return false;
    }
    // The member `k`, or a null value when there is none.
    const Json& operator[](const std::string& k) const {
        static const Json none;
        for (const auto& kv : obj)
            if (kv.first == k) return kv.second;
        return none;
    }
    bool operator==(const Json& o) const {
        return kind == o.kind && num == o.num && str == o.str && arr == o.arr && obj == o.obj;
    }
    bool operator!=(const Json& o) const { return !(*this == o); }
};

std::string show(const Json& j) {
    switch (j.kind) {
    case Json::Null: return "null";
    case Json::Bool: return j.num ? "true" : "false";
    case Json::Num: return std::to_string(j.num);
    case Json::Str: return "\"" + j.str + "\"";
    case Json::Arr: return "[" + std::to_string(j.arr.size()) + " items]";
    case Json::Obj: {
        std::string s = "{";
        for (const auto& kv : j.obj) s += (s.size() > 1 ? ", " : "") + kv.first + ": " + show(kv.second);
        return s + "}";
    }
    }
    return "?";
}

class JsonReader {
public:
    explicit JsonReader(const std::string& s) : s_(s) {}
    Json parse() {
        Json v = value();
        ws();
        if (i_ != s_.size()) fail();
        return v;
    }

private:
    [[noreturn]] void fail() { throw std::runtime_error("bad JSON at " + std::to_string(i_)); }
    void ws() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\n' || s_[i_] == '\r' || s_[i_] == '\t')) ++i_;
    }
    char peek() {
        ws();
        if (i_ >= s_.size()) fail();
        return s_[i_];
    }
    void expect(char c) {
        if (peek() != c) fail();
        ++i_;
    }
    bool word(const char* w) {
        const std::string t(w);
        if (s_.compare(i_, t.size(), t) != 0) return false;
        i_ += t.size();
        return true;
    }
    char16_t hex4() {
        if (s_.size() - i_ < 4) fail();
        const auto v = static_cast<char16_t>(std::stoul(s_.substr(i_, 4), nullptr, 16));
        i_ += 4;
        return v;
    }
    std::string string() {
        expect('"');
        std::string out;
        std::u16string escaped;  // \u escapes, UTF-16 until the next plain character
        auto flush = [&] {
            out += utf8_from_utf16(escaped);
            escaped.clear();
        };
        while (i_ < s_.size() && s_[i_] != '"') {
            if (s_[i_] != '\\') {
                flush();
                out.push_back(s_[i_++]);
                continue;
            }
            if (++i_ >= s_.size()) fail();
            const char e = s_[i_++];
            if (e == 'u') {
                escaped.push_back(hex4());
                continue;
            }
            flush();
            switch (e) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            default: out.push_back(e);
            }
        }
        flush();
        expect('"');
        return out;
    }
    Json value() {
        Json v;
        const char c = peek();
        if (c == '{') {
            v.kind = Json::Obj;
            ++i_;
            for (bool first = true; peek() != '}'; first = false) {
                if (!first) expect(',');
                std::string k = string();
                expect(':');
                v.obj.emplace_back(std::move(k), value());
            }
            ++i_;
            return v;
        }
        if (c == '[') {
            v.kind = Json::Arr;
            ++i_;
            for (bool first = true; peek() != ']'; first = false) {
                if (!first) expect(',');
                v.arr.push_back(value());
            }
            ++i_;
            return v;
        }
        if (c == '"') {
            v.kind = Json::Str;
            v.str = string();
            return v;
        }
        if (word("null")) return v;
        if (word("true") || word("false")) {
            v.kind = Json::Bool;
            v.num = s_[i_ - 4] == 't';
            return v;
        }
        v.kind = Json::Num;
        std::size_t used = 0;
        v.num = std::stoll(s_.substr(i_, 32), &used);
        if (used < s_.size() - i_ && (s_[i_ + used] == '.' || s_[i_ + used] == 'e' || s_[i_ + used] == 'E'))
            fail();  // index.json holds integers only
        i_ += used;
        return v;
    }

    const std::string& s_;
    std::size_t i_ = 0;
};

Json parse_json(const std::string& s) { return JsonReader(s).parse(); }

Json read_json(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + p.u8string());
    return parse_json(std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()));
}

Json str(const std::string& s) {
    Json j;
    j.kind = Json::Str;
    j.str = s;
    return j;
}

Json num(long long n) {
    Json j;
    j.kind = Json::Num;
    j.num = n;
    return j;
}

// ---- directories

fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v || !fs::is_directory(v)) return {};
    fs::path p = fs::path(v).lexically_normal();
    if (p.filename().empty()) p = p.parent_path();
    return p;
}

// The distribution as shipped: IS_DIST_DIR, else original/shipdisk in one of the three
// directories above IS_ASSETS_DIR (work/assets). Empty when there is none.
fs::path dist_dir() {
    if (fs::path d = env_dir("IS_DIST_DIR"); !d.empty()) return d;
    fs::path p = env_dir("IS_ASSETS_DIR");
    for (int up = 0; up < 3 && !p.empty() && p.has_relative_path(); ++up) {
        p = p.parent_path();
        if (fs::is_directory(p / "original" / "shipdisk")) return p / "original" / "shipdisk";
    }
    return {};
}

// THEWORKS next to a shipdisk tree (work/extracted/theworks, original/theworks).
fs::path theworks_beside(const fs::path& shipdisk) {
    if (shipdisk.empty()) return {};
    const fs::path p = shipdisk.parent_path() / "theworks";
    return fs::is_directory(p) ? p : fs::path();
}

// ---- WAV files

void app16(Bytes& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void app32(Bytes& b, std::uint32_t v) {
    app16(b, v & 0xFFFF);
    app16(b, v >> 16);
}

// A RIFF WAVE file with a 16-byte fmt chunk and the data chunk last, so that the vector ends
// where the data does and a sanitizer catches a read past it.
Bytes riff_wave(unsigned format_tag, unsigned channels, unsigned bits, unsigned block_align,
                const Bytes& data) {
    Bytes b = {'R', 'I', 'F', 'F'};
    app32(b, static_cast<std::uint32_t>(4 + 8 + 16 + 8 + data.size()));
    b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    app32(b, 16);
    app16(b, format_tag);
    app16(b, channels);
    app32(b, 11025);
    app32(b, 11025 * block_align);
    app16(b, block_align);
    app16(b, bits);
    b.insert(b.end(), {'d', 'a', 't', 'a'});
    app32(b, static_cast<std::uint32_t>(data.size()));
    b.insert(b.end(), data.begin(), data.end());
    return Bytes(b.begin(), b.end());  // no spare capacity after the data
}

// ---- the golden comparison

// Every field that index.json and isview's summary both carry, as dump_json.h lists them.
void compare_module(const Json& got, const Json& want, const std::string& expected_key) {
    INFO("module " << expected_key);
    CHECK(want["module"].str == expected_key);
    for (const char* f : {"module_name", "description", "resource_align_shift", "strings"}) {
        INFO(f);
        CHECK_MESSAGE(got[f] == want[f], show(got[f]) << " != " << show(want[f]));
    }
    // counts: the same types, counts and order
    REQUIRE(got["counts"].kind == Json::Obj);
    CHECK_MESSAGE(got["counts"] == want["counts"], show(got["counts"]) << " != " << show(want["counts"]));
    const auto& gr = got["resources"].arr;
    const auto& wr = want["resources"].arr;
    REQUIRE(gr.size() == wr.size());
    CHECK(got["resource_count"] == num(static_cast<long long>(wr.size())));
    for (std::size_t i = 0; i < gr.size(); ++i) {
        for (const char* f : {"type", "type_id", "id", "size", "file_offset", "flags"}) {
            INFO("resource " << i << " " << f);
            CHECK_MESSAGE(gr[i][f] == wr[i][f], show(gr[i][f]) << " != " << show(wr[i][f]));
        }
    }
}

}  // namespace

TEST_CASE("isview text: UTF-8 output, cp1252 file names") {
    CHECK(utf8_from_utf16(u"MASTER.SHP") == "MASTER.SHP");
    CHECK(utf8_from_utf16(u"tw-é €") == "tw-\xC3\xA9 \xE2\x82\xAC");
    CHECK(utf8_from_utf16(u"\U0001F600") == "\xF0\x9F\x98\x80");
    // unpaired surrogates
    CHECK(utf8_from_utf16(std::u16string(1, char16_t(0xD800)) + u"a") == "\xEF\xBF\xBD" "a");
    CHECK(utf8_from_utf16(std::u16string(1, char16_t(0xDC00))) == "\xEF\xBF\xBD");

    CHECK(cp1252_from_utf16(u"ENG_LANG.DLL") == std::string("ENG_LANG.DLL"));
    CHECK(cp1252_from_utf16(u"é€Š") == std::string("\xE9\x80\x8A"));
    CHECK(cp1252_from_utf16(u"\u0081") == std::string("\x81"));  // undefined bytes round-trip
    CHECK_FALSE(cp1252_from_utf16(u"Ā"));
    CHECK_FALSE(cp1252_from_utf16(u"\u0080"));  // 0x80 is the euro sign
}

TEST_CASE("isview text: exception messages decode UTF-8 paths and cp1252 names") {
    // "MASTER.BOD: no such file in /games/tw-é" with the path in UTF-8 (path::u8string)
    CHECK(message_to_utf16("MASTER.BOD: no such file in /games/tw-\xC3\xA9") ==
          u"MASTER.BOD: no such file in /games/tw-é");
    // a cp1252 name from game data
    CHECK(message_to_utf16("no body file D\xC9MON.DIB") == u"no body file DÉMON.DIB");
    // both in one message
    CHECK(message_to_utf16("\xE9t\xE9 in /tw-\xC3\xA9/") == u"été in /tw-é/");
    CHECK(message_to_utf16("\xE2\x82\xAC \x80") == u"€ €");
    CHECK(message_to_utf16("\xF0\x9F\x98\x80") == u"\U0001F600");
    // malformed UTF-8 falls back to cp1252 byte by byte
    CHECK(message_to_utf16("\xC0\xAF") == u"À¯");                      // overlong
    CHECK(message_to_utf16("\xED\xA0\x80") == u"í €");            // a surrogate
    CHECK(message_to_utf16("\xF4\x90\x80\x80") == u"ô\u0090€€");  // above U+10FFFF
    CHECK(message_to_utf16("\xC3") == u"Ã");                                // truncated
    CHECK(message_to_utf16("\xE2\x82x") == u"â‚x");
    CHECK(message_to_utf16("") == u"");
}

TEST_CASE("isview text: controls, DEL and cp1252's undefined bytes are escaped") {
    for (char16_t c : {char16_t(0), char16_t(0x1F), char16_t(0x7F), char16_t(0x81), char16_t(0x8D),
                       char16_t(0x8F), char16_t(0x90), char16_t(0x9D), char16_t(0x9F)})
        CHECK_FALSE(printable(c));
    for (char16_t c : {char16_t(' '), char16_t('~'), char16_t(0xA0), char16_t(0xFF), char16_t(0x20AC)})
        CHECK(printable(c));
    // every byte that cp1252 defines widens to a printable character
    for (int b = 0x20; b < 0x100; ++b) {
        const char16_t c = is::fmt::cp1252_to_utf16(std::string(1, static_cast<char>(b)))[0];
        const bool undefined = b == 0x7F || b == 0x81 || b == 0x8D || b == 0x8F || b == 0x90 || b == 0x9D;
        CHECK_MESSAGE(printable(c) == !undefined, "byte " << b);
    }
    const std::string bytes = "a\tb\r\n\x01\x7F\x80\x90\x9D";
    CHECK(escape_controls(is::fmt::cp1252_to_utf16(bytes)) == u"a\\tb\\r\\n\\x01\\x7F€\\x90\\x9D");
}

TEST_CASE("isview dump: type names follow isre's RES_TYPE_NAMES") {
    // re/isre/ne.py RES_TYPE_NAMES
    const std::pair<std::uint16_t, const char*> isre[] = {
        {1, "CURSOR"}, {2, "BITMAP"}, {3, "ICON"}, {4, "MENU"}, {5, "DIALOG"}, {6, "STRING"},
        {7, "FONTDIR"}, {8, "FONT"}, {9, "ACCELERATOR"}, {10, "RCDATA"}, {11, "MESSAGETABLE"},
        {12, "GROUP_CURSOR"}, {14, "GROUP_ICON"}, {15, "NAMETABLE"}, {16, "VERSION"},
        {17, "DLGINCLUDE"}, {19, "PLUGPLAY"}, {20, "VXD"}, {21, "ANICURSOR"}, {22, "ANIICON"},
        {23, "HTML"}, {24, "MANIFEST"},
    };
    std::set<std::uint16_t> named;
    for (const auto& [id, name] : isre) {
        REQUIRE(std_type_name(id) != nullptr);
        CHECK(std::string(std_type_name(id)) == name);
        named.insert(id);
    }
    int extra = 0;  // other integer types with a name
    for (unsigned t = 0; t < 0x8000; ++t)
        if (!named.count(static_cast<std::uint16_t>(t)) && std_type_name(static_cast<std::uint16_t>(t))) ++extra;
    CHECK(extra == 0);

    is::fmt::NeResource r;
    r.type = 4000;
    CHECK(type_label(r) == "4000");
    r.type = 14;
    CHECK(type_label(r) == "GROUP_ICON");
    r.type = 0;
    r.type_name = "MYTYPE";
    CHECK(type_label(r) == "MYTYPE");
    r.name = "MAINACCEL";
    CHECK(id_label(r) == "MAINACCEL");
}

TEST_CASE("isview dump: JSON strings and a module's summary") {
    CHECK(json_string(u"a\"b\\c\n\t\x01") == "\"a\\\"b\\\\c\\n\\t\\u0001\"");
    CHECK(json_cp1252("\xA9 SDi \x80") == "\"\xC2\xA9 SDi \xE2\x82\xAC\"");

    // a module with an integer custom type, a named id, a STRING block and a named type
    Bytes strings = string_block({"", "one", "", "three"});
    Bytes b = build_ne("TESTMOD", {{4000, 7, "", Bytes(20, 1)},
                                   {is::fmt::RT16_STRING, 1, "", strings},
                                   {is::fmt::RT16_BITMAP, 0, "LOGO", Bytes(8, 2)},
                                   {10, 9, "", Bytes(4, 3)},
                                   {4000, 8, "", Bytes(16, 4)}});
    // point the fourth resource's type at the name "LOGO" (relative to the resource table at
    // 0x80): a named type with an integer id
    const std::size_t names_at = 2 + 5 * 20 + 2;
    put16(b, 0x80 + 2 + 3 * 20, static_cast<std::uint16_t>(names_at));
    const NeModule ne(b);
    REQUIRE(ne.resources().size() == 5);
    REQUIRE(ne.resources()[3].type_name == "LOGO");

    ModuleInfo m;
    m.name = "Test\xE9.DLL";
    m.path = fs::path("dir") / "Test.DL1";
    m.size = b.size();
    m.description = "A \x93test\x94";
    m.ne = &ne;
    const Json j = parse_json(module_json(m));
    CHECK(j["module"] == str("test\xC3\xA9.dll"));  // ASCII lower case only
    CHECK(j["file"] == str("Test\xC3\xA9.DLL"));
    CHECK(j["stored_as"] == str("Test.DL1"));
    CHECK(j["installed_size"] == num(static_cast<long long>(b.size())));
    CHECK(j["module_name"] == str("TESTMOD"));
    CHECK(j["description"] == str("A \xE2\x80\x9Ctest\xE2\x80\x9D"));
    CHECK(j["resource_align_shift"] == num(4));
    CHECK(j["resource_count"] == num(5));
    CHECK(j["strings"] == num(2));
    const Json& counts = j["counts"];
    REQUIRE(counts.obj.size() == 4);
    CHECK(counts.obj[0] == std::make_pair(std::string("4000"), num(2)));  // first-appearance order
    CHECK(counts.obj[1] == std::make_pair(std::string("STRING"), num(1)));
    CHECK(counts.obj[2] == std::make_pair(std::string("BITMAP"), num(1)));
    CHECK(counts.obj[3] == std::make_pair(std::string("LOGO"), num(1)));
    const auto& rs = j["resources"].arr;
    REQUIRE(rs.size() == 5);
    CHECK(rs[0]["type"] == str("4000"));
    CHECK(rs[0]["type_id"] == num(4000));
    CHECK(rs[0]["id"] == num(7));
    CHECK(rs[0]["size"] == num(32));  // rounded to the 16-byte unit
    CHECK(rs[0]["file_offset"] == num(ne.resources()[0].offset));
    CHECK(rs[0]["flags"] == str("0x0030"));
    CHECK(rs[2]["type"] == str("BITMAP"));
    CHECK(rs[2]["type_id"] == num(2));
    CHECK(rs[2]["id"] == str("LOGO"));  // a named id is a string
    CHECK(rs[3]["type"] == str("LOGO"));
    CHECK(rs[3]["type_id"] == str("LOGO"));  // a named type is a string
    CHECK(rs[3]["id"] == num(9));

    // --dump *: an array of the summaries
    const Json all = parse_json(modules_json({m, m}));
    REQUIRE(all.kind == Json::Arr);
    CHECK(all.arr.size() == 2);
    CHECK(parse_json(modules_json({})).arr.empty());

    // a module that did not load: what() holds a UTF-8 path and a cp1252 name
    ModuleInfo bad;
    bad.name = "ISPACE.DLL";
    bad.path = "ISPACE.dll";
    bad.error = "ISPACE.DLL \x93x\x94: cannot read /games/tw-\xC3\xA9/ISPACE.dll";
    const Json e = parse_json(module_json(bad));
    CHECK(e["error"] == str("ISPACE.DLL \xE2\x80\x9Cx\xE2\x80\x9D: cannot read /games/tw-\xC3\xA9/ISPACE.dll"));
    CHECK_FALSE(e.has("resources"));
}

TEST_CASE("isview dump: module helpers") {
    Bytes b = build_ne("M", {{is::fmt::RT16_BITMAP, 1, "", Bytes(8, 0)}});
    CHECK(is_ne(b));
    CHECK(ne_description(b).empty());  // no nonresident name table
    // a nonresident name table at the end: "Hello" (NE +0x20 length, NE +0x2C offset)
    const auto at = static_cast<std::uint32_t>(b.size());
    b.insert(b.end(), {5, 'H', 'e', 'l', 'l', 'o', 0, 0, 0});
    put16(b, 0x40 + 0x20, 9);
    put16(b, 0x40 + 0x2C, static_cast<std::uint16_t>(at));
    put16(b, 0x40 + 0x2E, static_cast<std::uint16_t>(at >> 16));
    CHECK(ne_description(b) == "Hello");
    put16(b, 0x40 + 0x2C, 0xFFF0);  // past the end
    CHECK(ne_description(b).empty());

    Bytes notne = b;
    notne[0x40] = 'P';
    CHECK_FALSE(is_ne(notne));
    Bytes far = b;
    far[0x3C] = 0xFF;
    far[0x3D] = 0xFF;
    CHECK_FALSE(is_ne(far));
    CHECK_FALSE(is_ne(Bytes(0x3F, 0)));

    std::vector<ModuleInfo> mods(2);
    mods[0].name = "ENG_LANG.DLL";
    mods[1].name = "eng_fx.dll";
    CHECK(find_module(mods, "eng_lang.dll") == &mods[0]);
    CHECK(find_module(mods, "ENG_FX.DLL") == &mods[1]);
    CHECK(find_module(mods, "SND_FX.DLL") == nullptr);
}

TEST_CASE("isview wave: columns read only whole samples inside the data chunk") {
    // 8-bit mono: one column per two frames
    const Bytes w8 = riff_wave(1, 1, 8, 1, {0, 128, 255, 64});
    const Wave a = is::fmt::parse_wav(w8.data(), w8.size());
    REQUIRE(wave_drawable(a, w8.size()));
    auto cols = wave_columns(a, w8.data(), w8.size(), 2);
    REQUIRE(cols.size() == 2);
    CHECK(cols[0].lo == -32768);
    CHECK(cols[0].hi == 0);
    CHECK(cols[1].lo == -16384);
    CHECK(cols[1].hi == 32512);

    // 16-bit stereo: the first channel only
    const Bytes w16 = riff_wave(1, 2, 16, 4, {0x00, 0x80, 0xFF, 0x7F, 0xFF, 0x7F, 0x00, 0x80});
    const Wave c = is::fmt::parse_wav(w16.data(), w16.size());
    cols = wave_columns(c, w16.data(), w16.size(), 2);
    REQUIRE(cols.size() == 2);
    CHECK(cols[0].lo == -32768);
    CHECK(cols[0].hi == -32768);
    CHECK(cols[1].lo == 32767);
    CHECK(cols[1].hi == 32767);

    // more columns than frames: each column takes at least one frame
    cols = wave_columns(a, w8.data(), w8.size(), 10);
    REQUIRE(cols.size() == 10);
    CHECK(cols[9].lo == -16384);

    // 16 bits with block_align 1, as in a hand-edited SND_FX.DLL: parse_wav accepts it, but a
    // frame holds half a sample, and the last frame's sample would end past the data chunk
    const Bytes odd = riff_wave(1, 1, 16, 1, {1, 2, 3, 4, 5});
    const Wave o = is::fmt::parse_wav(odd.data(), odd.size());
    CHECK(o.frames() == 5);
    CHECK_FALSE(wave_drawable(o, odd.size()));
    CHECK(wave_columns(o, odd.data(), odd.size(), 300).empty());

    // not PCM, unusual sample sizes, no frame, no column, a data chunk past the file
    const Bytes adpcm = riff_wave(2, 1, 4, 1, {1, 2, 3});
    CHECK(wave_columns(is::fmt::parse_wav(adpcm.data(), adpcm.size()), adpcm.data(), adpcm.size(), 3).empty());
    const Bytes w24 = riff_wave(1, 1, 24, 3, {1, 2, 3, 4, 5, 6});
    CHECK(wave_columns(is::fmt::parse_wav(w24.data(), w24.size()), w24.data(), w24.size(), 3).empty());
    const Bytes nobits = riff_wave(1, 1, 0, 1, {1, 2, 3});
    CHECK(wave_columns(is::fmt::parse_wav(nobits.data(), nobits.size()), nobits.data(), nobits.size(), 3).empty());
    const Bytes empty = riff_wave(1, 1, 8, 1, {});
    CHECK(wave_columns(is::fmt::parse_wav(empty.data(), empty.size()), empty.data(), empty.size(), 3).empty());
    CHECK(wave_columns(a, w8.data(), w8.size(), 0).empty());
    Wave past = a;
    past.data_size = w8.size();
    CHECK_FALSE(wave_drawable(past, w8.size()));
    CHECK(wave_columns(past, w8.data(), w8.size(), 2).empty());
}

TEST_CASE("isview --dump golden: every NE module equals isre's index.json") {
    const fs::path installed = env_dir("IS_ORIGINAL_DIR");
    const fs::path assets = env_dir("IS_ASSETS_DIR");
    if (installed.empty() || assets.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR / IS_ASSETS_DIR not available");
        return;
    }
    // isre's modules: key -> package ("shipdisk/ENG_LANG.DLL" -> "shipdisk")
    const Json listed = read_json(assets / "modules.json");
    REQUIRE(listed.kind == Json::Arr);
    std::set<std::pair<std::string, std::string>> isre;  // (package, key)
    for (const Json& e : listed.arr) {
        const std::string source = e["source"].str;
        isre.insert({source.substr(0, source.find('/')), e["module"].str});
    }

    struct Tree {
        fs::path dir;
        std::string package;
    };
    const fs::path dist = dist_dir();
    std::vector<Tree> trees = {{installed, "shipdisk"}};
    if (!dist.empty()) trees.push_back({dist, "shipdisk"});
    else MESSAGE("original/shipdisk not found above IS_ASSETS_DIR; the distribution is not compared");
    for (const fs::path& sd : {installed, dist}) {
        const fs::path tw = theworks_beside(sd);
        if (!tw.empty()) trees.push_back({tw, "theworks"});
    }
    CHECK(trees.size() >= 2);  // the installed copy and at least one more tree

    for (const Tree& t : trees) {
        INFO("directory " << t.dir.u8string());
        const is::assets::AssetStore store(t.dir);
        const std::vector<ModuleInfo> mods = scan_modules(store);
        std::set<std::string> found, expected;
        for (const auto& [package, key] : isre)
            if (package == t.package) expected.insert(key);
        const std::string prefix = t.package == "shipdisk" ? "" : t.package + "-";  // re/isre/res.py module_key
        for (const ModuleInfo& m : mods) {
            REQUIRE_MESSAGE(m.ne, m.error);
            const Json got = parse_json(module_json(m));
            const std::string key = prefix + got["module"].str;
            found.insert(key);
            if (!expected.count(key)) continue;
            compare_module(got, read_json(assets / key / "index.json"), key);
        }
        CHECK(found == expected);
    }
}
