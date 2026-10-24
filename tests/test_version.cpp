// VERSION resources: a synthetic Win16 node tree, then every VERSION resource of the
// distribution against isre's version.json (work/assets/<module>/version.json).
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "doctest.h"
#include "formats/version.h"
#include "ne_builder.h"

using namespace is::fmt;
namespace fs = std::filesystem;

namespace {

// ---- building Win16 nodes: length, value length, ASCIIZ key, value, DWORD padding

using Bytes = std::vector<std::uint8_t>;

void pad4(Bytes& b) {
    while (b.size() % 4) b.push_back(0);
}

// `length` overrides the computed node length (to build broken nodes).
Bytes node(const std::string& key, const Bytes& value, const std::vector<Bytes>& children = {},
           int length = -1) {
    Bytes b(4, 0);
    b.insert(b.end(), key.begin(), key.end());
    b.push_back(0);
    pad4(b);
    b.insert(b.end(), value.begin(), value.end());
    for (const auto& c : children) {
        pad4(b);
        b.insert(b.end(), c.begin(), c.end());
    }
    put16(b, 0, static_cast<std::uint16_t>(length < 0 ? static_cast<int>(b.size()) : length));
    put16(b, 2, static_cast<std::uint16_t>(value.size()));
    return b;
}

// A node with no value whose length ends right after its key's NUL, with no DWORD padding.
Bytes unpadded_leaf(const std::string& key) {
    Bytes b = node(key, {});
    b.resize(4 + key.size() + 1);
    put16(b, 0, static_cast<std::uint16_t>(b.size()));
    return b;
}

Bytes text(const std::string& s) {
    Bytes b(s.begin(), s.end());
    b.push_back(0);
    return b;
}

Bytes dwords(std::initializer_list<std::uint32_t> v) {
    Bytes b;
    for (std::uint32_t x : v)
        for (int k = 0; k < 4; ++k) b.push_back(static_cast<std::uint8_t>(x >> (8 * k)));
    return b;
}

Bytes sample(std::uint32_t signature = FixedFileInfo::kSignature,
             const std::string& root = "VS_VERSION_INFO") {
    const Bytes fixed = dwords({signature, 0x10000, 0x1001d, 0x20003, 0x1001d, 0, 0x3f, 0xa, 1,
                                1, 0, 0, 0});
    return node(root, fixed,
                {node("StringFileInfo", {},
                      {node("040904E4", {},
                            {node("CompanyName", text("Software Dynamics, inc. (SDi)")),
                             node("FileVersion", text("1.29.08")),
                             node("LegalCopyright", text("\xA9 SDi")),
                             node("Odd", Bytes{'a', 0, 'b', 0})})}),
                 node("VarFileInfo", {}, {node("Translation", {0x09, 0x04, 0xE4, 0x04})})});
}

// ---- a minimal JSON reader for isre's output; strings decode to UTF-16

struct Json {
    enum Kind { Null, Bool, Num, Str, Arr, Obj } kind = Null;
    double num = 0;
    std::u16string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::u16string, Json>> obj;

    // The member `k`, or a null value when there is none.
    const Json& operator[](const std::u16string& k) const {
        static const Json none;
        for (const auto& kv : obj)
            if (kv.first == k) return kv.second;
        return none;
    }
};

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
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) ++i_;
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
    unsigned hex4() {
        if (i_ + 4 > s_.size()) fail();
        const unsigned v = static_cast<unsigned>(std::stoul(s_.substr(i_, 4), nullptr, 16));
        i_ += 4;
        return v;
    }
    std::u16string string() {
        expect('"');
        std::u16string out;
        while (i_ < s_.size() && s_[i_] != '"') {
            const auto c = static_cast<unsigned char>(s_[i_]);
            if (c == '\\') {
                ++i_;
                if (i_ >= s_.size()) fail();
                const char e = s_[i_++];
                switch (e) {
                    case 'n': out.push_back(u'\n'); break;
                    case 'r': out.push_back(u'\r'); break;
                    case 't': out.push_back(u'\t'); break;
                    case 'b': out.push_back(u'\b'); break;
                    case 'f': out.push_back(u'\f'); break;
                    case 'u': out.push_back(static_cast<char16_t>(hex4())); break;
                    default: out.push_back(static_cast<char16_t>(e));
                }
                continue;
            }
            // UTF-8 to UTF-16
            const int len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
            if (i_ + static_cast<std::size_t>(len) > s_.size()) fail();
            char32_t cp = len == 1 ? c : len == 2 ? (c & 0x1F) : len == 3 ? (c & 0x0F) : (c & 0x07);
            for (int k = 1; k < len; ++k) cp = (cp << 6) | (static_cast<unsigned char>(s_[++i_]) & 0x3F);
            ++i_;
            if (cp >= 0x10000) {
                cp -= 0x10000;
                out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
                out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
            } else {
                out.push_back(static_cast<char16_t>(cp));
            }
        }
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
                std::u16string k = string();
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
        v.num = std::stod(s_.substr(i_), &used);
        i_ += used;
        return v;
    }

    const std::string& s_;
    std::size_t i_ = 0;
};

Json read_json(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + p.u8string());
    const std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return JsonReader(s).parse();
}

std::u16string u16(const std::string& ascii) { return std::u16string(ascii.begin(), ascii.end()); }

std::u16string hex(std::uint32_t v, int digits) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%0*x", digits, v);
    return u16(buf);
}

fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v || !fs::is_directory(v)) return {};
    return fs::path(v);
}

// Every field version.json holds for one resource.
void compare(const VersionInfo& v, const Json& j, const std::string& what) {
    INFO(what);
    REQUIRE(j.kind == Json::Obj);
    CHECK(cp1252_to_utf16(v.root.key) == j[u"key"].str);
    CHECK(static_cast<double>(v.size) == j[u"size"].num);

    const Json& jf = j[u"fixed"];
    REQUIRE(v.fixed.has_value() == (jf.kind == Json::Obj));
    if (v.fixed) {
        const FixedFileInfo& f = *v.fixed;
        const std::pair<const char16_t*, std::uint32_t> fields[] = {
            {u"signature", f.signature},
            {u"struc_version", f.struc_version},
            {u"file_version_ms", f.file_version_ms},
            {u"file_version_ls", f.file_version_ls},
            {u"product_version_ms", f.product_version_ms},
            {u"product_version_ls", f.product_version_ls},
            {u"file_flags_mask", f.file_flags_mask},
            {u"file_flags", f.file_flags},
            {u"file_os", f.file_os},
            {u"file_type", f.file_type},
            {u"file_subtype", f.file_subtype},
            {u"file_date_ms", f.file_date_ms},
            {u"file_date_ls", f.file_date_ls},
        };
        for (const auto& [name, value] : fields) {
            CHECK(hex(value, 8) == jf[name].str);
        }
        CHECK(u16(f.file_version()) == jf[u"file_version"].str);
        CHECK(u16(f.product_version()) == jf[u"product_version"].str);
    }

    const Json& sfi = j[u"string_file_info"];
    REQUIRE(v.string_tables.size() == sfi.obj.size());
    for (std::size_t t = 0; t < v.string_tables.size(); ++t) {
        const VersionStringTable& ours = v.string_tables[t];
        const auto& theirs = sfi.obj[t];
        CHECK(cp1252_to_utf16(ours.key) == theirs.first);
        REQUIRE(ours.strings.size() == theirs.second.obj.size());
        for (std::size_t k = 0; k < ours.strings.size(); ++k) {
            CHECK(cp1252_to_utf16(ours.strings[k].first) == theirs.second.obj[k].first);
            CHECK(cp1252_to_utf16(ours.strings[k].second) == theirs.second.obj[k].second.str);
        }
    }

    const Json& vfi = j[u"var_file_info"];
    REQUIRE(v.vars.size() == vfi.obj.size());
    for (std::size_t k = 0; k < v.vars.size(); ++k) {
        CHECK(cp1252_to_utf16(v.vars[k].first) == vfi.obj[k].first);
        const auto& words = vfi.obj[k].second.arr;
        REQUIRE(v.vars[k].second.size() == words.size());
        for (std::size_t w = 0; w < words.size(); ++w)
            CHECK(hex(v.vars[k].second[w], 4) == words[w].str);
    }
}

}  // namespace

TEST_CASE("VERSION: the Win16 node tree, fixed info, strings and translations") {
    const Bytes b = sample();
    const VersionInfo v = parse_version16(b.data(), b.size());
    CHECK(v.root.key == "VS_VERSION_INFO");
    CHECK(v.size == b.size());
    REQUIRE(v.fixed);
    CHECK(v.fixed->signature == FixedFileInfo::kSignature);
    CHECK(v.fixed->file_version_ms == 0x1001d);
    CHECK(v.fixed->file_flags == 0xa);
    CHECK(v.fixed->file_version() == "1.29.2.3");
    CHECK(v.fixed->product_version() == "1.29.0.0");
    REQUIRE(v.root.children.size() == 2);
    REQUIRE(v.string_tables.size() == 1);
    const VersionStringTable& t = v.string_tables[0];
    CHECK(t.key == "040904E4");
    REQUIRE(t.strings.size() == 4);
    CHECK(t.strings[0].first == "CompanyName");
    CHECK(t.strings[0].second == "Software Dynamics, inc. (SDi)");
    REQUIRE(v.find_string("FileVersion"));
    CHECK(*v.find_string("FileVersion") == "1.29.08");
    REQUIRE(v.find_string("LegalCopyright"));
    CHECK(*v.find_string("LegalCopyright") == "\xA9 SDi");  // cp1252, unconverted
    REQUIRE(v.find_string("Odd"));
    CHECK(*v.find_string("Odd") == "a");  // up to the first NUL
    CHECK(v.find_string("ProductName") == nullptr);
    REQUIRE(v.vars.size() == 1);
    CHECK(v.vars[0].first == "Translation");
    CHECK(v.vars[0].second == std::vector<std::uint16_t>{0x0409, 0x04E4});

    // trailing padding after the root (NE resource lengths are rounded) is ignored
    Bytes padded = b;
    padded.resize(b.size() + 12, 0xCC);
    CHECK(parse_version16(padded.data(), padded.size()).size == b.size());

    // a root value shorter than VS_FIXEDFILEINFO gives no fixed info
    const Bytes nofixed = node("VS_VERSION_INFO", Bytes(8, 0));
    CHECK_FALSE(parse_version16(nofixed.data(), nofixed.size()).fixed);

    // an empty value takes no room: a leaf may end right after its key's NUL, unpadded
    const Bytes comments = unpadded_leaf("Comments");
    REQUIRE(comments.size() == 13);
    const Bytes bare = node("VS_VERSION_INFO", {}, {comments});  // 33 bytes, not a DWORD multiple
    const VersionInfo e = parse_version16(bare.data(), bare.size());
    CHECK(e.size == 33);
    REQUIRE(e.root.children.size() == 1);
    CHECK(e.root.children[0].key == "Comments");
    CHECK(e.root.children[0].value.empty());
    const Bytes in_table = node(
        "VS_VERSION_INFO", {},
        {node("StringFileInfo", {},
              {node("040904E4", {}, {node("FileVersion", text("1.29.08")), comments})})});
    const VersionInfo et = parse_version16(in_table.data(), in_table.size());
    REQUIRE(et.find_string("Comments"));
    CHECK(et.find_string("Comments")->empty());
    REQUIRE(et.find_string("FileVersion"));
    CHECK(*et.find_string("FileVersion") == "1.29.08");
}

TEST_CASE("VERSION: malformed trees throw FormatError") {
    const Bytes badsig = sample(0x12345678);
    CHECK_THROWS_AS(parse_version16(badsig.data(), badsig.size()), FormatError);
    const Bytes badroot = sample(FixedFileInfo::kSignature, "VS_VERSION");
    CHECK_THROWS_AS(parse_version16(badroot.data(), badroot.size()), FormatError);
    const Bytes b = sample();
    CHECK_THROWS_AS(parse_version16(b.data(), b.size() - 1), FormatError);  // root overruns
    const Bytes tiny = node("X", {}, {}, 3);
    CHECK_THROWS_AS(parse_version16(tiny.data(), tiny.size()), FormatError);
    // a child longer than its parent
    Bytes child = node("Child", text("v"));
    Bytes parent = node("VS_VERSION_INFO", {}, {child});
    put16(parent, 20, static_cast<std::uint16_t>(child.size() + 8));
    CHECK_THROWS_AS(parse_version16(parent.data(), parent.size()), FormatError);
    // a value past its node
    Bytes overv = node("VS_VERSION_INFO", Bytes(4, 0));
    put16(overv, 2, 40);
    CHECK_THROWS_AS(parse_version16(overv.data(), overv.size()), FormatError);
    // a one-byte value in a node that ends right after its key
    Bytes leaf1 = unpadded_leaf("Comments");
    put16(leaf1, 2, 1);
    const Bytes overleaf = node("VS_VERSION_INFO", {}, {leaf1});
    CHECK_THROWS_AS(parse_version16(overleaf.data(), overleaf.size()), FormatError);
    // a key without its NUL inside the node
    Bytes nokey = {6, 0, 0, 0, 'A', 'B', 'C', 'D'};
    CHECK_THROWS_AS(parse_version16(nokey.data(), nokey.size()), FormatError);
    // ten levels of nesting
    Bytes deep = node("leaf", {});
    for (int k = 0; k < 9; ++k) deep = node(k == 8 ? "VS_VERSION_INFO" : "n", {}, {deep});
    CHECK_THROWS_AS(parse_version16(deep.data(), deep.size()), FormatError);
}

TEST_CASE("VERSION: load_version finds the module's resource") {
    const Bytes b = sample();
    NeModule m(build_ne("INSPACE", {{RT16_VERSION, 1, "", b}}));
    const auto v = load_version(m);
    REQUIRE(v);
    REQUIRE(v->find_string("FileVersion"));
    CHECK(*v->find_string("FileVersion") == "1.29.08");
    NeModule none(build_ne("EMPTY", {{RT16_STRING, 1, "", Bytes(16, 0)}}));
    CHECK_FALSE(load_version(none));
}

TEST_CASE("VERSION golden: every module's resource equals isre's version.json") {
    const fs::path dir = env_dir("IS_ORIGINAL_DIR");
    const fs::path assets = env_dir("IS_ASSETS_DIR");
    if (dir.empty() || assets.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR / IS_ASSETS_DIR not available");
        return;
    }
    // INSPACE.EXE: FileVersion 1.29.08 (docs/re/resources.md, Inventory)
    const NeModule ins = NeModule::load(dir / "INSPACE.EXE");
    const auto v = load_version(ins);
    REQUIRE(v);
    REQUIRE(v->find_string("FileVersion"));
    CHECK(*v->find_string("FileVersion") == "1.29.08");
    REQUIRE(v->find_string("OriginalFilename"));
    CHECK(*v->find_string("OriginalFilename") == "INSPACE.EXE");
    REQUIRE(v->fixed);
    CHECK(v->fixed->file_version() == "1.29.0.0");

    // The theworks copy of FACTORY.EXE sits in the sibling directory of the extracted tree.
    fs::path base = dir.lexically_normal();
    if (base.filename().empty()) base = base.parent_path();
    const fs::path theworks = base.parent_path() / "theworks" / "FACTORY.EXE";
    struct Mod {
        fs::path file;
        const char* assets_dir;
    };
    std::vector<Mod> mods = {{dir / "INSPACE.EXE", "inspace.exe"},
                             {dir / "FACTORY.EXE", "factory.exe"},
                             {dir / "CTL3D.DLL", "ctl3d.dll"},
                             {dir / "WAVEMIX.DLL", "wavemix.dll"}};
    if (fs::is_regular_file(theworks)) mods.push_back({theworks, "theworks-factory.exe"});
    else MESSAGE("theworks/FACTORY.EXE not found next to IS_ORIGINAL_DIR; not compared");
    for (const auto& mod : mods) {
        const NeModule m = NeModule::load(mod.file);
        const Json j = read_json(assets / mod.assets_dir / "version.json");
        const auto res = m.all(RT16_VERSION);
        REQUIRE_MESSAGE(res.size() == j.arr.size(), mod.assets_dir);
        for (std::size_t k = 0; k < res.size(); ++k) {
            CHECK(static_cast<double>(res[k]->id) == j.arr[k][u"id"].num);
            compare(parse_version16(m.data(*res[k]), m.size(*res[k])), j.arr[k], mod.assets_dir);
        }
    }
}
