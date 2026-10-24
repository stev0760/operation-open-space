// SZDD expansion: synthetic streams, then every compressed file of the distribution against
// the tree that `isre extract` writes (work/extracted/shipdisk, IS_ORIGINAL_DIR).
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "formats/szdd.h"

using namespace is::fmt;
namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> make_szdd(const std::vector<std::uint8_t>& stream, std::uint32_t size,
                                    char last = 0, std::uint8_t mode = 'A') {
    std::vector<std::uint8_t> b = {'S', 'Z', 'D', 'D', 0x88, 0xF0, 0x27, 0x33, mode,
                                   static_cast<std::uint8_t>(last)};
    for (int k = 0; k < 4; ++k) b.push_back(static_cast<std::uint8_t>(size >> (8 * k)));
    b.insert(b.end(), stream.begin(), stream.end());
    return b;
}

std::vector<std::uint8_t> bytes_of(const std::string& s) { return {s.begin(), s.end()}; }

std::vector<std::uint8_t> cat(std::vector<std::uint8_t> a, const std::vector<std::uint8_t>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

std::string expand_str(const std::vector<std::uint8_t>& f) {
    const SzddFile x = szdd_expand(f.data(), f.size());
    return std::string(x.data.begin(), x.data.end());
}

fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v || !fs::is_directory(v)) return {};
    return fs::path(v);
}

// The distribution disk: IS_DIST_DIR, else original/shipdisk in a directory above
// IS_ORIGINAL_DIR (the repository layout: work/extracted/shipdisk next to original/).
fs::path dist_dir(const fs::path& extracted) {
    if (fs::path d = env_dir("IS_DIST_DIR"); !d.empty()) return d;
    fs::path p = extracted.lexically_normal();
    if (p.filename().empty()) p = p.parent_path();
    for (; p.has_relative_path(); p = p.parent_path())
        if (fs::is_directory(p / "original" / "shipdisk")) return p / "original" / "shipdisk";
    return {};
}

// Distribution name -> installed name, as re/isre/szdd.py restored_name gives it: a known
// distribution extension is replaced (RESTORED_EXTENSIONS; the stem keeps its case, and
// INSTALL.INI lists the installed names), else a trailing '_' becomes the SZDD header's
// stored last character when it is printable.
std::string restored_name(const std::string& name, char last_char = 0) {
    static const std::map<std::string, std::string> ext = {
        {"mi1", "MID"}, {"dl1", "DLL"}, {"hl1", "HLP"}, {"bo1", "BOD"}, {"sh1", "SHP"},
        {"in1", "INI"}, {"a1", "AD"},   {"sc1", "SCR"}, {"ex_", "EXE"},
    };
    const auto dot = name.rfind('.');
    if (dot != std::string::npos) {
        std::string e = name.substr(dot + 1);
        for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const auto it = ext.find(e);
        if (it != ext.end()) return name.substr(0, dot + 1) + it->second;
    }
    const auto c = static_cast<unsigned char>(last_char);
    if (!name.empty() && name.back() == '_' && c > 0x20 && c < 0x7F)
        return name.substr(0, name.size() - 1) + last_char;
    return name;
}

}  // namespace

TEST_CASE("SZDD: literals, the header's size and last character") {
    const auto f = make_szdd(cat({0xFF}, bytes_of("ABCDEFGH")), 8, 'X');
    CHECK(is_szdd(f.data(), f.size()));
    const SzddHeader h = parse_szdd_header(f.data(), f.size());
    CHECK(h.mode == 'A');
    CHECK(h.last_char == 'X');
    CHECK(h.size == 8);
    const SzddFile x = szdd_expand(f.data(), f.size());
    CHECK(std::string(x.data.begin(), x.data.end()) == "ABCDEFGH");
    CHECK(x.last_char == 'X');
    // input after the declared size is ignored
    CHECK(expand_str(make_szdd(cat({0xFF}, bytes_of("ABCD")), 2)) == "AB");
    CHECK(expand_str(make_szdd({}, 0)) == "");
}

TEST_CASE("SZDD: back-references into the window") {
    // three literals, then a match at 0xFF0 (the start position), length 3
    CHECK(expand_str(make_szdd(cat(cat({0x07}, bytes_of("abc")), {0xF0, 0xF0}), 6)) == "abcabc");
    // the window starts as spaces: offset 0, length 5
    CHECK(expand_str(make_szdd({0x00, 0x00, 0x02}, 5)) == "     ");
    // a match that overlaps the bytes it writes: offset 0xFF0, length 6
    CHECK(expand_str(make_szdd({0x01, 'x', 0xF0, 0xF3}, 7)) == "xxxxxxx");
    // the longest match, 18 bytes, cut at the declared size
    CHECK(expand_str(make_szdd({0x00, 0x00, 0x0F}, 10)) == std::string(10, ' '));
}

TEST_CASE("SZDD: the write position and match offsets wrap at 4096") {
    // 16 literals fill 0xFF0-0xFFF, 4 more land at 0x000-0x003; a match at 0xFFE of
    // length 6 reads across the wrap
    std::vector<std::uint8_t> s = {0xFF};
    s = cat(s, bytes_of("01234567"));
    s.push_back(0xFF);
    s = cat(s, bytes_of("89ABCDEF"));
    s.push_back(0x0F);  // four literals, then a match
    s = cat(s, bytes_of("WXYZ"));
    s = cat(s, {0xFE, 0xF3});
    CHECK(expand_str(make_szdd(s, 26)) == "0123456789ABCDEFWXYZEFWXYZ");

    // 4096 literals run the write position all the way round to 0xFF0, which still holds
    // literal 0, the oldest byte in the window; the match copies literals 0-2
    std::vector<std::uint8_t> big;
    std::string want;
    for (int i = 0; i < 4096; ++i) {
        if (i % 8 == 0) big.push_back(0xFF);
        const char c = static_cast<char>('a' + (i * 7) % 26);
        big.push_back(static_cast<std::uint8_t>(c));
        want.push_back(c);
    }
    big = cat(big, {0x00, 0xF0, 0xF0});  // match at 0xFF0, length 3
    want += want.substr(0, 3);
    CHECK(expand_str(make_szdd(big, 4099)) == want);
}

TEST_CASE("SZDD: bad input throws FormatError") {
    // the stream ends before the declared size
    auto shortf = make_szdd(cat({0xFF}, bytes_of("ABC")), 8);
    CHECK_THROWS_AS(szdd_expand(shortf.data(), shortf.size()), FormatError);
    // half a match
    auto half = make_szdd({0x00, 0xF0}, 3);
    CHECK_THROWS_AS(szdd_expand(half.data(), half.size()), FormatError);
    // a flag byte and nothing else
    auto flag = make_szdd({0xFF}, 1);
    CHECK_THROWS_AS(szdd_expand(flag.data(), flag.size()), FormatError);
    // an unknown mode
    auto mode = make_szdd(cat({0xFF}, bytes_of("A")), 1, 0, 'B');
    CHECK_THROWS_AS(szdd_expand(mode.data(), mode.size()), FormatError);
    // not SZDD, or a header cut short
    std::vector<std::uint8_t> mz(40, 0);
    mz[0] = 'M';
    mz[1] = 'Z';
    CHECK_FALSE(is_szdd(mz.data(), mz.size()));
    CHECK_THROWS_AS(szdd_expand(mz.data(), mz.size()), FormatError);
    auto cut = make_szdd({}, 0);
    CHECK(is_szdd(cut.data(), 8));
    CHECK_THROWS_AS(parse_szdd_header(cut.data(), 13), FormatError);
    CHECK_FALSE(is_szdd(cut.data(), 7));
}

TEST_CASE("SZDD: the golden test restores names as isre's restored_name does") {
    CHECK(restored_name("MASTER.BO1") == "MASTER.BOD");
    CHECK(restored_name("Eng_help.hl1") == "Eng_help.HLP");
    CHECK(restored_name("INSPACE.ex_", 'X') == "INSPACE.EXE");  // the table comes first
    CHECK(restored_name("README.TX_", 'T') == "README.TXT");
    CHECK(restored_name("README.TX_") == "README.TX_");
    CHECK(restored_name("README.TX_", ' ') == "README.TX_");
    CHECK(restored_name("README.TXT", 'T') == "README.TXT");
}

TEST_CASE("SZDD golden: the distribution expands to work/extracted/shipdisk") {
    const fs::path extracted = env_dir("IS_ORIGINAL_DIR");
    const fs::path dist = extracted.empty() ? fs::path() : dist_dir(extracted);
    if (extracted.empty() || dist.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR or original/shipdisk (IS_DIST_DIR) not available");
        return;
    }
    std::size_t files = 0, compressed = 0;
    for (const auto& e : fs::directory_iterator(dist)) {
        if (!e.is_regular_file()) continue;
        const std::string name = e.path().filename().u8string();
        const auto src = read_file(e.path());
        const char last =
            is_szdd(src.data(), src.size()) ? parse_szdd_header(src.data(), src.size()).last_char : 0;
        const fs::path target = extracted / restored_name(name, last);
        REQUIRE_MESSAGE(fs::is_regular_file(target), name << " -> " << target.u8string());
        const auto want = read_file(target);
        ++files;
        if (is_szdd(src.data(), src.size())) {
            ++compressed;
            const SzddFile x = szdd_expand(src.data(), src.size());
            CHECK_MESSAGE(x.data.size() == parse_szdd_header(src.data(), src.size()).size, name);
            CHECK_MESSAGE(x.data == want, name);
            CHECK_MESSAGE(x.last_char == 0, name);
        } else {
            CHECK_MESSAGE(src == want, name);  // copied unchanged, INSPACE.ex_ among them
        }
    }
    CHECK(files == 25);
    CHECK(compressed == 14);
}
