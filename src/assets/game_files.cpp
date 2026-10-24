#include "assets/game_files.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

#include "formats/bytes.h"
#include "formats/szdd.h"

namespace is::assets {

namespace fs = std::filesystem;

namespace {

using Native = fs::path::string_type;
using Char = fs::path::value_type;

// re/isre/szdd.py RESTORED_EXTENSIONS: distribution extension (lower case) -> installed one.
constexpr std::pair<const char*, const char*> kRestoredExtensions[] = {
    {"mi1", "MID"}, {"dl1", "DLL"}, {"hl1", "HLP"}, {"bo1", "BOD"}, {"sh1", "SHP"},
    {"in1", "INI"}, {"a1", "AD"},   {"sc1", "SCR"}, {"ex_", "EXE"},
};

Char ascii_lower(Char c) { return c >= Char('A') && c <= Char('Z') ? Char(c - 'A' + 'a') : c; }

// The upper-case partner of a lower-case cp1252 letter (name_key), else `c` itself.
char32_t fold_letter(char32_t c) {
    if (c >= U'a' && c <= U'z') return c - 0x20;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 0x20;  // a grave .. thorn, not division
    switch (c) {
    case 0x0161: return 0x0160;  // s caron
    case 0x0153: return 0x0152;  // oe
    case 0x017E: return 0x017D;  // z caron
    case 0x00FF: return 0x0178;  // y diaeresis
    default: return c;
    }
}

// name_key over UTF-16 (Windows): every letter above is one code unit. Only the overload for
// the platform's native string is used.
[[maybe_unused]] void fold_native(std::wstring& s) {
    for (wchar_t& c : s) c = static_cast<wchar_t>(fold_letter(static_cast<char16_t>(c)));
}

// name_key over UTF-8 (POSIX): a letter above is one byte (ASCII) or a two-byte sequence led
// by 0xC3 (U+00C0-U+00FF) or 0xC5 (U+0140-U+017F); the upper-case partner has the same
// length. Other bytes, valid UTF-8 or not, stay.
[[maybe_unused]] void fold_native(std::string& s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto b = static_cast<unsigned char>(s[i]);
        if (b < 0x80) {
            s[i] = static_cast<char>(fold_letter(b));
            continue;
        }
        if ((b != 0xC3 && b != 0xC5) || i + 1 >= s.size()) continue;
        const auto t = static_cast<unsigned char>(s[i + 1]);
        if ((t & 0xC0) != 0x80) continue;
        const char32_t c = (char32_t(b & 0x1F) << 6) | (t & 0x3F);
        const char32_t up = fold_letter(c);
        s[i] = static_cast<char>(0xC0 | (up >> 6));
        s[i + 1] = static_cast<char>(0x80 | (up & 0x3F));
        ++i;
    }
}

// s[pos..] equals the lower-case ASCII `lower`, ignoring ASCII case.
bool tail_equals(const Native& s, std::size_t pos, const char* lower) {
    for (; pos < s.size() && *lower; ++pos, ++lower)
        if (ascii_lower(s[pos]) != Char(*lower)) return false;
    return pos == s.size() && !*lower;
}

std::string message_path(const fs::path& p) { return p.u8string(); }

// The SZDD header's stored character of the file, 0 when it is not SZDD or unreadable.
char stored_last_char(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::uint8_t head[fmt::kSzddHeaderSize] = {};
    if (!f.read(reinterpret_cast<char*>(head), sizeof head)) return 0;
    if (!fmt::is_szdd(head, sizeof head)) return 0;
    return fmt::parse_szdd_header(head, sizeof head).last_char;
}

}  // namespace

fs::path restored_name(const fs::path& name, char last_char) {
    const Native& s = name.native();
    const auto dot = s.rfind(Char('.'));
    if (dot != Native::npos)
        for (const auto& [ext, installed] : kRestoredExtensions)
            if (tail_equals(s, dot + 1, ext)) {
                Native out = s.substr(0, dot + 1);
                for (const char* c = installed; *c; ++c) out.push_back(Char(*c));
                return fs::path(out);
            }
    const auto c = static_cast<unsigned char>(last_char);
    if (!s.empty() && s.back() == Char('_') && c > 0x20 && c < 0x7F) {
        Native out = s;
        out.back() = Char(c);
        return fs::path(out);
    }
    return name;
}

fs::path path_from_cp1252(const std::string& name) { return fs::path(fmt::cp1252_to_utf16(name)); }

Native name_key(const fs::path& name) {
    Native k = name.native();
    fold_native(k);
    return k;
}

GameFiles::GameFiles(fs::path dir) : dir_(std::move(dir)) {
    std::error_code ec;
    if (!fs::is_directory(dir_, ec)) throw NotFound(message_path(dir_) + " is not a directory");
    std::vector<fs::path> files;
    fs::directory_iterator it(dir_, ec);
    for (const fs::directory_iterator end; !ec && it != end; it.increment(ec)) {
        std::error_code fe;
        if (it->is_regular_file(fe)) files.push_back(it->path());
    }
    if (ec) throw NotFound("cannot list " + message_path(dir_) + ": " + ec.message());
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        return a.filename().native() < b.filename().native();
    });

    std::map<Native, Entry> chosen;
    for (const fs::path& p : files) {
        const fs::path file = p.filename();
        const Native& s = file.native();
        const char last = !s.empty() && s.back() == Char('_') ? stored_last_char(p) : 0;
        Entry e{p, restored_name(file, last), false};
        e.distribution_name = e.name.native() != s;
        const Native key = name_key(e.name);
        auto [at, fresh] = chosen.emplace(key, e);
        if (fresh) continue;
        // the file already under the installed name wins; else the first one stays
        if (at->second.distribution_name && !e.distribution_name) std::swap(at->second, e);
        shadowed_.push_back(e.path);
    }
    for (auto& [key, e] : chosen) {
        if (e.distribution_name) form_ = DirForm::kDistribution;
        index_.emplace(key, entries_.size());
        entries_.push_back(std::move(e));
    }
}

const GameFiles::Entry* GameFiles::find(const std::string& name) const {
    const auto it = index_.find(name_key(path_from_cp1252(name)));
    return it == index_.end() ? nullptr : &entries_[it->second];
}

const GameFiles::Entry& GameFiles::need(const std::string& name) const {
    const Entry* e = find(name);
    if (!e) throw NotFound(name + ": no such file in " + message_path(dir_));
    return *e;
}

std::vector<std::uint8_t> GameFiles::read(const Entry& e) const {
    std::vector<std::uint8_t> data;
    try {
        std::ifstream f(e.path, std::ios::binary);
        if (!f) throw NotFound("cannot open " + message_path(e.path));
        // istream::read turns a read error into badbit; it never throws while the stream's
        // exception mask is clear. (An istreambuf_iterator would let libstdc++'s filebuf
        // exception through.)
        std::vector<char> chunk(std::size_t{1} << 16);
        do {
            f.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
            const auto* p = reinterpret_cast<const std::uint8_t*>(chunk.data());
            data.insert(data.end(), p, p + f.gcount());
        } while (f);
        if (f.bad()) throw NotFound("cannot read " + message_path(e.path));
    } catch (const std::ios_base::failure& x) {  // in case a library throws all the same
        throw NotFound("cannot read " + message_path(e.path) + ": " + x.what());
    }
    if (fmt::is_szdd(data.data(), data.size())) {
        try {
            data = fmt::szdd_expand(data.data(), data.size()).data;
        } catch (const fmt::FormatError& x) {
            throw fmt::FormatError(message_path(e.path) + ": " + x.what());
        }
    }
    return data;
}

std::vector<std::uint8_t> GameFiles::read(const std::string& name) const { return read(need(name)); }

const std::vector<std::uint8_t>& GameFiles::bytes(const std::string& name) const {
    const Entry& e = need(name);
    const Native key = name_key(e.name);
    auto it = cache_.find(key);
    if (it == cache_.end()) it = cache_.emplace(key, read(e)).first;
    return it->second;
}

void GameFiles::forget(const std::string& name) { cache_.erase(name_key(path_from_cp1252(name))); }

bool holds_game(const fs::path& dir) {
    try {
        const GameFiles g(dir);
        return std::all_of(kGameModules.begin(), kGameModules.end(),
                           [&](const char* m) { return g.has(m); });
    } catch (const std::exception&) {
        return false;
    }
}

std::optional<fs::path> discover(const std::vector<fs::path>& candidates) {
    for (const fs::path& c : candidates)
        if (!c.empty() && holds_game(c)) return c;
    return std::nullopt;
}

}  // namespace is::assets
