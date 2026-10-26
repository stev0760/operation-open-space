// Golden tests of the WinHelp reader (src/formats/hlp) against `isre hlp` (re/isre/hlp.py):
// a port of the reference's MarkdownWriter renders each topic and the index from the C++
// reader, and every file must equal the one in work/manual/2000 (Eng_help.hl1, the manual
// v1.29.08 opens) and work/manual/1998 (theworks ENG_HELP.HLP), the pictures included.
// The help files come from IS_ORIGINAL_DIR (the shipdisk) and its sibling theworks/, read
// through is::assets::GameFiles as the port reads them; the manual directory is
// IS_MANUAL_DIR or, by default, the sibling manual/ of IS_ASSETS_DIR. Each test skips when
// a file is missing.
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "assets/game_files.h"
#include "doctest.h"
#include "formats/bytes.h"
#include "formats/hlp.h"

namespace fs = std::filesystem;
using namespace is::fmt;

namespace {

using U = std::u32string;

U dec(const std::string& cp1252) {
    const std::u16string w = cp1252_to_utf16(cp1252);
    return U(w.begin(), w.end());
}

std::string enc(const U& s) {
    std::string o;
    for (char32_t c : s) {
        if (c < 0x80) {
            o.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            o.push_back(static_cast<char>(0xC0 | (c >> 6)));
            o.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            o.push_back(static_cast<char>(0xE0 | (c >> 12)));
            o.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            o.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return o;
}

U u(const char* ascii) { return U(ascii, ascii + std::char_traits<char>::length(ascii)); }
U u(const std::string& ascii) { return U(ascii.begin(), ascii.end()); }
U num(long long v) { return u(std::to_string(v)); }

bool py_space(char32_t c) {
    return (c >= 9 && c <= 13) || (c >= 0x1C && c <= 0x20) || c == 0x85 || c == 0xA0 ||
           c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 ||
           c == 0x202F || c == 0x205F || c == 0x3000;
}
U lstrip(const U& s) {
    std::size_t a = 0;
    while (a < s.size() && py_space(s[a])) ++a;
    return s.substr(a);
}
U rstrip(const U& s) {
    std::size_t z = s.size();
    while (z > 0 && py_space(s[z - 1])) --z;
    return s.substr(0, z);
}
U strip(const U& s) { return rstrip(lstrip(s)); }
U strip_chars(const U& s, const U& chars, bool left = true, bool right = true) {
    std::size_t a = 0, z = s.size();
    if (left)
        while (a < z && chars.find(s[a]) != U::npos) ++a;
    if (right)
        while (z > a && chars.find(s[z - 1]) != U::npos) --z;
    return s.substr(a, z - a);
}
U replace_all(U s, const U& from, const U& to) {
    for (std::size_t p = 0; (p = s.find(from, p)) != U::npos; p += to.size()) s.replace(p, from.size(), to);
    return s;
}
std::vector<U> split(const U& s, char32_t sep) {
    std::vector<U> out(1);
    for (char32_t c : s) {
        if (c == sep)
            out.emplace_back();
        else
            out.back().push_back(c);
    }
    return out;
}
U join(const std::vector<U>& v, const U& sep) {
    U o;
    for (std::size_t i = 0; i < v.size(); ++i) o += (i ? sep : U()) + v[i];
    return o;
}
char32_t lower(char32_t c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    return c;
}
U lower(U s) {
    for (auto& c : s) c = lower(c);
    return s;
}
bool alnum_ascii(char32_t c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
U hex_lower(unsigned long long v, int min_digits = 1) {
    static const char* d = "0123456789abcdef";
    U o;
    do {
        o.insert(o.begin(), static_cast<char32_t>(d[v & 15]));
        v >>= 4;
    } while (v);
    while (static_cast<int>(o.size()) < min_digits) o.insert(o.begin(), '0');
    return o;
}
U py_hex(long long v) { return v < 0 ? u("-0x") + hex_lower(-v) : u("0x") + hex_lower(v); }

// _md_escape
U md_escape(const U& s0) {
    U s;
    const auto punct = [](char32_t c) {
        return (c >= 0x21 && c <= 0x2F) || (c >= 0x3A && c <= 0x40) || (c >= 0x5B && c <= 0x60) ||
               (c >= 0x7B && c <= 0x7E);
    };
    for (std::size_t i = 0; i < s0.size(); ++i) {
        if (s0[i] == '\\' && i + 1 < s0.size() && punct(s0[i + 1]))
            s += u("\\\\");
        else
            s.push_back(s0[i]);
    }
    U a;
    for (char32_t c : s) {
        if (c == '*' || c == '`') a.push_back('\\');
        a.push_back(c);
    }
    U b;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] == '[') {
            const std::size_t close = a.find(']', i + 1);
            if (close != U::npos && close + 1 < a.size() &&
                (a[close + 1] == '(' || a[close + 1] == '[' || a[close + 1] == ':'))
                b.push_back('\\');
        }
        b.push_back(a[i]);
    }
    U c;
    for (std::size_t i = 0; i < b.size(); ++i) {
        if (b[i] == '<' && i + 1 < b.size() &&
            ((b[i + 1] >= 'A' && b[i + 1] <= 'Z') || (b[i + 1] >= 'a' && b[i + 1] <= 'z') ||
             b[i + 1] == '/' || b[i + 1] == '!' || b[i + 1] == '?'))
            c.push_back('\\');
        c.push_back(b[i]);
    }
    if (!c.empty() && (c[0] == '>' || c[0] == '#' || c[0] == '+' || c[0] == '-')) c.insert(c.begin(), '\\');
    U d;
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (c[i] == '_') {
            const bool prev_ok = i > 0 && alnum_ascii(c[i - 1]);
            const bool next_ok = i + 1 < c.size() && alnum_ascii(c[i + 1]);
            if (!prev_ok || !next_ok) d.push_back('\\');
        }
        d.push_back(c[i]);
    }
    return d;
}

U slugify(const U& s) {
    U o;
    bool dash = false;
    for (char32_t c : lower(s)) {
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')) {
            o.push_back(c);
            dash = false;
        } else if (!dash) {
            o.push_back('-');
            dash = true;
        }
    }
    o = strip_chars(o, u("-"));
    return o.empty() ? u("topic") : o;
}

U zpad2(int v) {
    U s = num(v);
    while (s.size() < 2) s.insert(s.begin(), '0');
    return s;
}

const U kSep = U{' ', 0xB7, ' '};

U block_plain(const HlpBlock& b) {
    if (const auto* p = std::get_if<HlpParagraph>(&b)) return dec(p->plain());
    return dec(std::get<HlpTableRow>(b).plain());
}

// MarkdownWriter
class Writer {
public:
    explicit Writer(const HlpFile& hf) : hf_(hf) {
        for (const auto& t : hf.topics())
            files_[t.index] = zpad2(t.index) + u("-") + slugify(display_title(t)) + u(".md");
    }

    const std::map<int, U>& files() const { return files_; }

    static U display_title(const HlpTopic& t) {
        if (!t.title.empty()) return dec(t.title);
        for (const auto& b : t.blocks) {
            const U line = strip(split(strip(block_plain(b)), '\n')[0]);
            if (!line.empty()) return line.substr(0, 60);
        }
        return u("untitled");
    }

    U topic_markdown(const HlpTopic& t) const {
        std::vector<U> lines{u("# ") + (t.title.empty() ? u("(untitled topic) ") + display_title(t)
                                                        : dec(t.title)),
                             U()};
        std::vector<U> meta{u("Topic ") + num(t.index) + u(" of ") + num(static_cast<long long>(hf_.topics().size()))};
        if (!t.contexts.empty()) {
            std::vector<U> c;
            for (const auto& x : t.contexts) c.push_back(u("`") + dec(x) + u("`"));
            meta.push_back(u("context ") + join(c, u(", ")));
        }
        if (!t.map_ids.empty()) {
            std::vector<U> c;
            for (auto m : t.map_ids) c.push_back(num(m));
            meta.push_back(u("help map id ") + join(c, u(", ")));
        }
        lines.push_back(u("*") + join(meta, kSep) + u("*"));
        if (!t.keywords.empty()) {
            std::vector<U> k;
            for (const auto& x : t.keywords) k.push_back(md_escape(dec(x)));
            lines.push_back(U());
            lines.push_back(u("*Keywords: ") + join(k, u("; ")) + u("*"));
        }
        std::vector<U> nav;
        const std::pair<const char*, std::int32_t> browse[] = {{"previous", t.browse_prev},
                                                               {"next", t.browse_next}};
        for (const auto& [label, off] : browse) {
            const HlpTopic* o = off != -1 ? hf_.topic_at(off) : nullptr;
            if (o && o != &t)
                nav.push_back(u("browse ") + u(label) + u(": [") + md_escape(dec(o->title)) + u("](") +
                              files_.at(o->index) + u(")"));
        }
        if (!nav.empty()) {
            lines.push_back(U());
            lines.push_back(u("*") + join(nav, kSep) + u("*"));
        }
        if (!t.macros.empty()) {
            std::vector<U> m;
            for (const auto& x : t.macros) m.push_back(u("`") + dec(x) + u("`"));
            lines.push_back(U());
            lines.push_back(u("*Entry macros: ") + join(m, u("; ")) + u("*"));
        }
        lines.push_back(U());
        for (auto& l : blocks_markdown(t)) lines.push_back(l);
        U text = rstrip(join(lines, u("\n"))) + u("\n");
        U out;
        for (std::size_t i = 0; i < text.size();) {  // re.sub(r"\n{3,}", "\n\n", text)
            std::size_t j = i;
            while (j < text.size() && text[j] == '\n') ++j;
            if (j - i >= 3) {
                out += u("\n\n");
                i = j;
            } else if (j > i) {
                out += text.substr(i, j - i);
                i = j;
            } else {
                out.push_back(text[i++]);
            }
        }
        return out;
    }

    U index_markdown(const std::string& name) const {
        const HlpSystem& si = hf_.system();
        std::vector<U> L{u("# ") + (si.title.empty() ? dec(name) : dec(si.title)), U()};
        L.push_back(u("Extracted from `") + dec(name) + u("` by `isre hlp`."));
        L.push_back(U());
        L.push_back(u("## |SYSTEM"));
        L.push_back(U());
        L.push_back(u("| Field | Value |"));
        L.push_back(u("|---|---|"));
        L.push_back(u("| Format | WinHelp, minor ") + num(si.minor) + u(", major ") + num(si.major) +
                    u(" (minor 21 = HC31) |"));
        L.push_back(u("| Flags | ") + py_hex(si.flags) + u(" (") +
                    u(hf_.lz77() ? "LZ77" : "not compressed") + u(", ") + num(hf_.block_size()) +
                    u("-byte topic blocks) |"));
        L.push_back(u("| Build date | ") + (si.gen_date ? build_date(si.gen_date) : u("(none)")) + u(" |"));
        L.push_back(u("| Title | ") + md_escape(dec(si.title)) + u(" |"));
        if (!si.copyright.empty()) L.push_back(u("| Copyright | ") + md_escape(dec(si.copyright)) + u(" |"));
        if (!si.citation.empty()) L.push_back(u("| Citation | ") + md_escape(dec(si.citation)) + u(" |"));
        if (si.contents) {
            const HlpTopic* ct = hf_.topic_at(*si.contents);
            L.push_back(u("| Contents topic | ") + (ct ? topic_ref(ct) : py_hex(*si.contents)) + u(" |"));
        }
        for (const auto& m : si.config) L.push_back(u("| Config macro | `") + dec(m) + u("` |"));
        for (const auto& w : si.windows) L.push_back(u("| Window | ") + window_text(w) + u(" |"));
        for (const auto& [k, v] : si.other)
            L.push_back(u("| Record type ") + num(k) + u(" | ") + num(static_cast<long long>(v.size())) + u(" bytes |"));
        const auto n = static_cast<long long>(hf_.topics().size());
        if (hf_.topic_headers() != n)
            L.push_back(u("| Topics | ") + num(n) + u(" (") + num(hf_.topic_headers()) +
                        u(" topic-header records; the last is the empty terminal record) |"));
        else
            L.push_back(u("| Topics | ") + num(n) + u(" |"));
        L.push_back(u("| Pictures | ") + num(static_cast<long long>(hf_.pictures().size())) + u(" |"));
        L.push_back(u("| Parse errors | ") + num(static_cast<long long>(hf_.errors().size())) + u(" |"));
        L.push_back(U());
        L.push_back(u("## Topics"));
        L.push_back(U());
        L.push_back(u("| # | Topic | Context ids | Help map ids |"));
        L.push_back(u("|---|---|---|---|"));
        for (const auto& t : hf_.topics()) {
            std::vector<U> c, m;
            for (const auto& x : t.contexts) c.push_back(u("`") + dec(x) + u("`"));
            for (auto x : t.map_ids) m.push_back(num(x));
            L.push_back(u("| ") + num(t.index) + u(" | ") + topic_ref(&t) + u(" | ") + join(c, u(", ")) +
                        u(" | ") + join(m, u(", ")) + u(" |"));
        }
        L.push_back(U());
        if (!hf_.ctxomap().empty()) {
            L.push_back(u("## Help context map (|CTXOMAP)"));
            L.push_back(U());
            L.push_back(u("Numeric ids an application passes to `WinHelp(..., HELP_CONTEXT, id)`."));
            L.push_back(U());
            L.push_back(u("| Map id | Topic |"));
            L.push_back(u("|---|---|"));
            auto sorted = hf_.ctxomap();
            std::sort(sorted.begin(), sorted.end());
            for (const auto& [mid, off] : sorted) {
                const HlpTopic* t = hf_.topic_at(off);
                L.push_back(u("| ") + num(mid) + u(" | ") +
                            (t ? topic_ref(t) : u("(no topic: offset ") + num(off) + u(")")) + u(" |"));
            }
            L.push_back(U());
        }
        if (!hf_.keywords().empty()) {
            L.push_back(u("## Keyword index (|KWBTREE)"));
            L.push_back(U());
            for (const auto& kw : hf_.keywords()) {
                std::vector<U> refs;
                for (auto off : kw.offsets) {
                    const HlpTopic* t = hf_.topic_at(off);
                    refs.push_back(t ? topic_ref(t) : u("offset ") + py_hex(off));
                }
                L.push_back(u("- ") + md_escape(dec(kw.text)) + u(": ") + join(refs, u(", ")));
            }
            L.push_back(U());
        }
        if (!hf_.pictures().empty()) {
            L.push_back(u("## Pictures"));
            L.push_back(U());
            L.push_back(u("| Name | Type | Size | Bits | Packing | Used in | Note |"));
            L.push_back(u("|---|---|---|---|---|---|---|"));
            std::vector<std::string> names;
            for (const auto& kv : hf_.pictures()) names.push_back(kv.first);
            std::sort(names.begin(), names.end(), natural_less);
            for (const auto& nm : names) {
                const HlpPicture& p = hf_.pictures().at(nm);
                std::vector<U> users;
                for (const auto& t : hf_.topics())
                    if (std::find(t.bitmaps.begin(), t.bitmaps.end(), nm) != t.bitmaps.end())
                        users.push_back(topic_ref(&t));
                const U ptype = p.ptype == 5 ? u("DDB") : p.ptype == 6 ? u("DIB") : p.ptype == 8 ? u("metafile") : num(p.ptype);
                const char* packs[] = {"none", "RunLen", "LZ77", "LZ77+RunLen"};
                const U pack = u(p.packing < 4 ? packs[p.packing] : "?");
                const U link = p.ptype == 5 || p.ptype == 6 ? u("[") + u(nm) + u(".bmp](") + u(nm) + u(".bmp)") : u(nm);
                L.push_back(u("| ") + link + u(" | ") + ptype + u(" | ") + num(p.width) + u("x") + num(p.height) +
                            u(" | ") + num(p.bit_count) + u(" | ") + pack + u(" | ") + join(users, u(", ")) +
                            u(" | ") + dec(p.note) + u(" |"));
            }
            L.push_back(U());
        }
        L.push_back(u("## Internal files"));
        L.push_back(U());
        L.push_back(u("| Name | Offset | Size |"));
        L.push_back(u("|---|---|---|"));
        for (const auto& [fname, off] : hf_.internal_files()) {
            L.push_back(u("| `") + replace_all(dec(fname), u("|"), u("\\|")) + u("` | ") + py_hex(off) +
                        u(" | ") + num(static_cast<long long>(hf_.file(fname).size())) + u(" |"));
        }
        L.push_back(U());
        if (!hf_.errors().empty()) {
            L.push_back(u("## Parse errors"));
            L.push_back(U());
            for (const auto& e : hf_.errors()) L.push_back(u("- ") + md_escape(dec(e)));
            L.push_back(U());
        }
        return rstrip(join(L, u("\n"))) + u("\n");
    }

private:
    static bool natural_less(const std::string& a, const std::string& b) {
        // re.split(r"(\d+)"): alternate text and numbers
        std::size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) {
            const bool da = std::isdigit(static_cast<unsigned char>(a[i])) != 0;
            const bool db = std::isdigit(static_cast<unsigned char>(b[j])) != 0;
            if (da && db) {
                std::size_t i2 = i, j2 = j;
                while (i2 < a.size() && std::isdigit(static_cast<unsigned char>(a[i2]))) ++i2;
                while (j2 < b.size() && std::isdigit(static_cast<unsigned char>(b[j2]))) ++j2;
                const long long x = std::stoll(a.substr(i, i2 - i)), y = std::stoll(b.substr(j, j2 - j));
                if (x != y) return x < y;
                i = i2;
                j = j2;
            } else {
                if (a[i] != b[j]) return a[i] < b[j];
                ++i;
                ++j;
            }
        }
        return a.size() - i < b.size() - j;
    }

    static U build_date(std::uint32_t t) {
        long long days = t / 86400, rem = t % 86400;
        // civil_from_days (Howard Hinnant)
        days += 719468;
        const long long era = days / 146097;
        const long long doe = days - era * 146097;
        const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        long long y = yoe + era * 400;
        const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const long long mp = (5 * doy + 2) / 153;
        const long long d = doy - (153 * mp + 2) / 5 + 1;
        const long long m = mp < 10 ? mp + 3 : mp - 9;
        if (m <= 2) ++y;
        return num(y) + u("-") + zpad2(static_cast<int>(m)) + u("-") + zpad2(static_cast<int>(d)) + u(" ") +
               zpad2(static_cast<int>(rem / 3600)) + u(":") + zpad2(static_cast<int>(rem / 60 % 60)) + u(":") +
               zpad2(static_cast<int>(rem % 60)) + u(" UTC");
    }

    static U window_text(const HlpWindow& w) {
        const unsigned f = w.flags;
        std::vector<U> parts;
        if (f & 0x01) parts.push_back(u("type \"") + md_escape(dec(w.type)) + u("\""));
        if (f & 0x02) parts.push_back(u("name \"") + md_escape(dec(w.name)) + u("\""));
        if (f & 0x04) parts.push_back(u("caption \"") + md_escape(dec(w.caption)) + u("\""));
        if (f & 0x08) parts.push_back(u("x ") + num(w.x));
        if (f & 0x10) parts.push_back(u("y ") + num(w.y));
        if (f & 0x20) parts.push_back(u("width ") + num(w.width));
        if (f & 0x40) parts.push_back(u("height ") + num(w.height));
        if (f & 0x80) parts.push_back(u("maximize ") + py_hex(w.maximize));
        if (f & 0x100) parts.push_back(u("rgb 0x") + hex_lower(w.rgb, 6));
        if (f & 0x200) parts.push_back(u("rgb_nsr 0x") + hex_lower(w.rgb_nsr, 6));
        if (f & 0x400) parts.push_back(u("on top"));
        if (f & 0x800) parts.push_back(u("auto-size height"));
        return u("flags ") + py_hex(f) + u(": ") + join(parts, u(", "));
    }

    U topic_ref(const HlpTopic* t) const {
        if (!t) return u("(none)");
        return u("[") + md_escape(t->title.empty() ? u("(untitled)") : dec(t->title)) + u("](") +
               files_.at(t->index) + u(")");
    }

    const HlpFont* font(int i) const { return hf_.font(i); }

    struct Seg {
        U text;
        bool bold, ital;
        int link;
    };

    std::vector<Seg> segments(const std::vector<HlpRun>& runs) const {
        std::vector<Seg> segs;
        for (const auto& r : runs) {
            const HlpFont* f = font(r.font);
            const bool bold = f && f->bold(), ital = f && f->italic();
            U txt;
            switch (r.kind) {
            case HlpRunKind::kText: txt = dec(r.text); break;
            case HlpRunKind::kTab: txt = u("\t"); break;
            case HlpRunKind::kBreak: txt = u("\n"); break;
            case HlpRunKind::kImage: txt = U(1, 0) + u("IMG") + dec(r.text) + U(1, 0); break;
            case HlpRunKind::kEmbed: txt = U(1, 0) + u("EMB") + dec(r.text) + U(1, 0); break;
            }
            if (!segs.empty() && segs.back().bold == bold && segs.back().ital == ital &&
                segs.back().link == r.link)
                segs.back().text += txt;
            else
                segs.push_back({txt, bold, ital, r.link});
        }
        return segs;
    }

    static U inline_text(const U& text) {
        const auto parts = split(text, 0);
        U out;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const U& part = parts[i];
            if (i % 2 == 0)
                out += md_escape(part);
            else if (part.compare(0, 3, u("IMG")) == 0)
                out += u("![") + part.substr(3) + u("](") + part.substr(3) + u(".bmp)");
            else
                out += u("`{") + part.substr(3) + u("}`");
        }
        return out;
    }

    std::pair<U, U> link_href(const HlpTopic& t, int li) const {
        const HlpLink& link = t.link_table[static_cast<std::size_t>(li)];
        if (link.kind == HlpLinkKind::kMacro || link.kind == HlpLinkKind::kExternal) return {U(), dec(link.target)};
        const HlpTopic* to = link.hash ? hf_.topic_for_hash(*link.hash) : nullptr;
        const U title = link.kind == HlpLinkKind::kPopup ? u(" \"popup\"") : U();
        if (!to) return {u("#") + dec(link.target), title + (title.empty() ? u(" \"unresolved\"") : U())};
        return {files_.at(to->index), title};
    }

    U render_inline(const HlpTopic& t, const std::vector<HlpRun>& runs, bool plain_bold, bool plain_ital) const {
        U out;
        for (const auto& s : segments(runs)) {
            const U body = inline_text(s.text);
            const U ls = lstrip(body), rs = rstrip(body);
            const U lead = body.substr(0, body.size() - ls.size());
            const U trail = body.substr(rs.size());
            if (s.link >= 0) {
                U core = strip(body);
                if (core.empty()) {
                    out += body;
                    continue;
                }
                const auto [href, title] = link_href(t, s.link);
                core = replace_all(replace_all(core, u("["), u("\\[")), u("]"), u("\\]"));
                const HlpLink& link = t.link_table[static_cast<std::size_t>(s.link)];
                if (!href.empty())
                    out += lead + u("[") + core + u("](") + href + title + u(")") + trail;
                else if (link.kind == HlpLinkKind::kMacro)
                    out += lead + core + u(" (macro `") + dec(link.target) + u("`)") + trail;
                else
                    out += lead + core + u(" (external `") + dec(link.target) + u("`)") + trail;
                continue;
            }
            U mark;
            if (s.bold && !plain_bold) mark += u("**");
            if (s.ital && !plain_ital) mark += u("*");
            if (!mark.empty() && !strip(body).empty() && s.text.find(char32_t(0)) == U::npos) {
                U rev(mark.rbegin(), mark.rend());
                out += lead + mark + strip(body) + rev + trail;
            } else {
                out += body;
            }
        }
        return out;
    }

    struct Style {
        int size = 0;
        bool bold = false, ital = false;
    };

    Style para_style(const HlpParagraph& para) const {
        Style st;
        bool any = false, all_bold = true, all_ital = true;
        int mn = 0;
        for (const auto& r : para.runs) {
            if (r.kind != HlpRunKind::kText || strip(dec(r.text)).empty()) continue;
            const HlpFont* f = font(r.font);
            const int sz = f ? f->half_points : 0;
            mn = any ? std::min(mn, sz) : sz;
            any = true;
            all_bold = all_bold && f && f->bold();
            all_ital = all_ital && f && f->italic();
        }
        if (!any) return st;
        st.size = mn;
        st.bold = all_bold;
        st.ital = all_ital;
        return st;
    }

    int body_size(const HlpTopic& t) const {
        std::vector<std::pair<int, std::size_t>> count;  // insertion order, as a dict
        for (const HlpParagraph* p : t.paragraphs()) {
            for (const auto& r : p->runs) {
                if (r.kind != HlpRunKind::kText) continue;
                const HlpFont* f = font(r.font);
                const int hp = f ? f->half_points : 0;
                auto it = std::find_if(count.begin(), count.end(), [&](auto& e) { return e.first == hp; });
                if (it == count.end()) count.emplace_back(hp, 0), it = count.end() - 1;
                it->second += strip(dec(r.text)).size();
            }
        }
        if (count.empty()) return 0;
        auto best = count.begin();
        for (auto it = count.begin(); it != count.end(); ++it)
            if (it->second > best->second) best = it;
        return best->first;
    }

    std::vector<U> blocks_markdown(const HlpTopic& t) const {
        const int bsize = body_size(t);
        std::vector<U> out;
        std::vector<std::vector<U>> table;
        const auto flush_table = [&] {
            if (table.empty()) return;
            std::size_t width = 0;
            for (const auto& r : table) width = std::max(width, r.size());
            out.push_back(U());
            U h1 = u("|"), h2 = u("|");
            for (std::size_t i = 0; i < width; ++i) h1 += u(" |"), h2 += u("---|");
            out.push_back(h1);
            out.push_back(h2);
            for (const auto& r : table) {
                std::vector<U> cells;
                for (const auto& c : r) cells.push_back(replace_all(c, u("|"), u("\\|")));
                cells.resize(width);
                out.push_back(u("| ") + join(cells, u(" | ")) + u(" |"));
            }
            out.push_back(U());
            table.clear();
        };
        const auto has_link = [](const HlpParagraph& p) {
            return std::any_of(p.runs.begin(), p.runs.end(), [](const HlpRun& r) { return r.link >= 0; });
        };
        for (const auto& b : t.blocks) {
            if (const auto* row = std::get_if<HlpTableRow>(&b)) {
                flush_table();
                std::vector<U> cells;
                for (const auto& cell : row->cells) {
                    std::vector<U> parts;
                    for (const auto& p : cell)
                        if (!p.runs.empty()) parts.push_back(strip(render_inline(t, p.runs, false, false)));
                    cells.push_back(replace_all(replace_all(join(parts, u("<br>")), u("\t"), u(" ")), u("\n"), u("<br>")));
                }
                table.push_back(cells);
                continue;
            }
            const auto& para = std::get<HlpParagraph>(b);
            const U plain = dec(para.plain());
            const bool has_pic = std::any_of(para.runs.begin(), para.runs.end(), [](const HlpRun& r) {
                return r.kind == HlpRunKind::kImage || r.kind == HlpRunKind::kEmbed;
            });
            if (strip(plain).empty() && !has_pic) {
                flush_table();
                continue;
            }
            const Style st = para_style(para);
            const U text = render_inline(t, para.runs, st.bold, st.ital);
            if (bsize && st.size > bsize && strip(plain).size() < 60 && plain.find('\t') == U::npos &&
                !has_link(para)) {
                flush_table();
                const U head = replace_all(strip(text), u("\n"), u(" "));
                if (out.empty() && lower(strip(plain)) == lower(strip(dec(t.title)))) continue;
                const U level = st.size >= 28 || para.nonscroll ? u("##") : u("###");
                out.push_back(U());
                out.push_back(level + u(" ") + head);
                out.push_back(U());
                continue;
            }
            const auto lines = split(text, '\n');
            bool tabbed = false;
            for (const auto& ln : lines)
                if (strip_chars(ln, u("\t ")).find('\t') != U::npos) tabbed = true;
            if (tabbed) {
                for (const auto& ln : lines) {
                    if (strip(ln).empty()) continue;
                    std::vector<U> cells;
                    U cur;
                    const U body = strip_chars(ln, u(" \t"));
                    for (std::size_t i = 0; i < body.size(); ++i) {
                        if (body[i] == '\t') {
                            cells.push_back(strip(cur));
                            cur.clear();
                            while (i + 1 < body.size() && body[i + 1] == '\t') ++i;
                        } else {
                            cur.push_back(body[i]);
                        }
                    }
                    cells.push_back(strip(cur));
                    table.push_back(cells);
                }
                continue;
            }
            flush_table();
            U mark;
            if (!has_link(para)) mark = u(st.bold ? "**" : "") + u(st.ital ? "*" : "");
            const U rev(mark.rbegin(), mark.rend());
            std::vector<U> md_lines;
            for (U ln : lines) {
                const std::size_t lead_tabs = ln.size() - strip_chars(ln, u("\t"), true, false).size();
                ln = rstrip(strip_chars(ln, u("\t ")));
                if (!mark.empty() && !ln.empty()) ln = mark + ln + rev;
                if (lead_tabs) {
                    U pre;
                    for (std::size_t k = 0; k < lead_tabs; ++k) pre += u("&emsp;");
                    ln = pre + ln;
                }
                md_lines.push_back(ln);
            }
            out.push_back(strip(join(md_lines, u("  \n"))));
            out.push_back(U());
        }
        flush_table();
        return out;
    }

    const HlpFile& hf_;
    std::map<int, U> files_;
};

fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v || !fs::is_directory(v)) return {};
    return fs::path(v);
}

fs::path manual_dir() {
    fs::path d = env_dir("IS_MANUAL_DIR");
    if (!d.empty()) return d;
    d = env_dir("IS_ASSETS_DIR");
    if (d.empty()) return {};
    d = d.parent_path() / "manual";
    return fs::is_directory(d) ? d : fs::path();
}

std::string slurp(const fs::path& p) {
    const auto b = read_file(p);
    return std::string(b.begin(), b.end());
}

// Renders every topic, the index and the pictures, and compares them with `golden`.
void check_against(const HlpFile& hf, const std::string& name, const fs::path& golden) {
    const Writer w(hf);
    std::size_t files = 0;
    for (const auto& t : hf.topics()) {
        const fs::path p = golden / fs::path(enc(w.files().at(t.index)));
        INFO("topic " << t.index << " " << p.u8string());
        REQUIRE(fs::is_regular_file(p));
        CHECK(enc(w.topic_markdown(t)) == slurp(p));
        ++files;
    }
    for (const auto& [pname, pic] : hf.pictures()) {
        INFO("picture " << pname);
        const auto bmp = pic.to_bmp();
        REQUIRE(!bmp.empty());
        CHECK(bmp == read_file(golden / (pname + ".bmp")));
        ++files;
    }
    CHECK(enc(w.index_markdown(name)) == slurp(golden / "index.md"));
    ++files;
    // No other topic or picture files in the golden directory.
    std::size_t golden_files = 0;
    for (const auto& e : fs::directory_iterator(golden))
        if (e.is_regular_file()) ++golden_files;
    CHECK(golden_files == files);
}

}  // namespace

TEST_CASE("golden: isre hlp output for the 2000 manual (Eng_help.hl1)") {
    const fs::path orig = env_dir("IS_ORIGINAL_DIR");
    const fs::path man = manual_dir();
    if (orig.empty() || man.empty() || !fs::is_directory(man / "2000")) {
        MESSAGE("skipped: IS_ORIGINAL_DIR or work/manual not available");
        return;
    }
    const is::assets::GameFiles gf(orig);
    if (!gf.has("ENG_HELP.HLP")) {
        MESSAGE("skipped: no ENG_HELP.HLP in IS_ORIGINAL_DIR");
        return;
    }
    const HlpFile hf(gf.read("ENG_HELP.HLP"));
    CHECK(hf.errors().empty());
    CHECK(hf.topics().size() == 37);
    CHECK(hf.topic_headers() == 38);
    check_against(hf, "Eng_help.hl1", man / "2000");
}

TEST_CASE("golden: isre hlp output for the 1998 manual (theworks ENG_HELP.HLP)") {
    const fs::path orig = env_dir("IS_ORIGINAL_DIR");
    const fs::path man = manual_dir();
    if (orig.empty() || man.empty() || !fs::is_directory(man / "1998") ||
        !fs::is_directory(orig.parent_path() / "theworks")) {
        MESSAGE("skipped: theworks or work/manual not available");
        return;
    }
    const is::assets::GameFiles gf(orig.parent_path() / "theworks");
    if (!gf.has("ENG_HELP.HLP")) {
        MESSAGE("skipped: no theworks ENG_HELP.HLP");
        return;
    }
    const HlpFile hf(gf.read("ENG_HELP.HLP"));
    CHECK(hf.errors().empty());
    CHECK(hf.topics().size() == 40);
    CHECK(hf.pictures().size() == 5);
    check_against(hf, "ENG_HELP.HLP", man / "1998");
}
