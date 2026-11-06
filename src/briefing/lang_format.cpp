// seg10:0xa332 (lang_format.h).
#include "briefing/lang_format.h"

namespace is::briefing {

namespace {

// seg6:0xf1a / seg6:0xeb8: signed decimal.
std::string decimal(long v) { return std::to_string(v); }

}  // namespace

std::string lang_format(const std::string& f, const std::vector<FArg>& args, const LangLookup& lang,
                        int phase, const std::function<int()>& rand) {
    (void)phase;  // DGROUP:0x93aa: %N's test reads it but never changes the output (C076)
    std::string out;
    std::size_t next = 0;
    auto take = [&]() -> FArg { return next < args.size() ? args[next++] : FArg(0); };
    auto load = [&](long id) { return lang ? lang(static_cast<int>(id)) : std::string(); };
    bool on = true;  // [bp-0x138], 1 at the start [BYTES seg10:0xa33f]
    int c = 0;       // [bp-0xa], the enabled groups of the run
    const std::size_t n = f.size();
    for (std::size_t i = 0; i < n;) {
        const char ch = f[i];
        if (ch != '%') {
            // The caret codes ^P, ^F, ^M, ^N [BYTES seg10:0xa3d9-0xa40c].
            if (ch == '^' && i + 1 < n) {
                const char k = f[i + 1];
                if (k == 'P' || k == 'F' || k == 'M' || k == 'N') {
                    i += 2;
                    continue;
                }
            }
            if (on) out += ch;
            ++i;
            continue;
        }
        const char k = i + 1 < n ? f[i + 1] : '\0';
        switch (k) {
        case '#':
        case 'd': {  // [BYTES seg10:0xa558]
            const FArg a = take();
            if (on) out += decimal(static_cast<std::int16_t>(a.v));
            if (k == '#') {
                const FArg id = take();
                if (on) out += " " + load((static_cast<std::int16_t>(a.v) == 1 ? 0 : 10000) + id.v);
            }
            break;
        }
        case '%':
            if (on) out += '%';
            break;
        case '<': {  // [BYTES seg10:0xa5f8]
            on = take().v != 0;
            if (on) {
                for (std::size_t j = i + 2; j < n; ++j) {
                    if (f[j] != '%') continue;
                    const char m = j + 1 < n ? f[j + 1] : '\0';
                    if (m == '<') {
                        if (c > 0) out += ", ";
                    } else if (m == '>') {
                        if (c > 0) out += load(797) + " ";  // " AND" [BYTES seg10:0xa656]
                    } else {
                        continue;
                    }
                    ++c;
                    break;
                }
            }
            break;
        }
        case '>':  // [BYTES seg10:0xa5ec]
            on = true;
            c = 0;
            break;
        case '?': {  // [BYTES seg10:0xa77c]
            const FArg flag = take();
            const FArg id0 = take();
            const FArg id1 = take();
            // A negative flag indexes below the pair in the original [DESIGN: id0].
            const std::string s = load(static_cast<std::int16_t>(flag.v) >= 1 ? id1.v : id0.v);
            if (on) out += s;
            break;
        }
        case 'A':
        case 'S':
        case 'V': {  // [BYTES seg10:0xa4c9]
            // LoadString into the 80-byte [bp-0x68] [BYTES seg10:0xa4dd seg10:0xa4df]; the cut at
            // buf[len - 2] when that byte is '^' drops a trailing caret pair such as ^N [BYTES
            // seg10:0xa4f7 seg10:0xa50b] (M4 s4 fixer, C076). A string shorter than 2 reads the
            // bytes below the buffer in the original; the port leaves it whole [DESIGN].
            std::string s = load(take().v);
            if (s.size() > 0x4f) s.resize(0x4f);
            if (on) {
                if (s.size() >= 2 && s[s.size() - 2] == '^') s.resize(s.size() - 2);
                out += s;
            }
            break;
        }
        case 'D': {  // [BYTES seg10:0xa754]: the caller passes the name as the first argument
            const FArg name = take();
            take();
            take();
            if (on) out += name.s;
            break;
        }
        case 'N':
        case 'n':
            // CR LF whenever output is on: the phase-11 test also needs the byte [bp-0x6a] to be
            // 0, and that byte holds the directive letter [BYTES seg10:0xa3c9 seg10:0xa6e3
            // seg10:0xa6ec seg10:0xa6f3 seg10:0xa701] (M4 s4 fixer, C076).
            if (on) out += "\r\n";
            break;
        case 'R': {  // [BYTES seg10:0xa7b4]
            const FArg m = take();
            const FArg base = take();
            const int r = rand ? rand() : 0;
            const std::int16_t mm = static_cast<std::int16_t>(m.v);
            const long id = (mm != 0 ? static_cast<std::int16_t>(r) % mm : 0) + base.v;
            const std::string s = load(id);
            if (on) out += s;
            break;
        }
        case 'a':
        case 's':
        case 'v': {
            const FArg a = take();
            if (on) out += a.is_string ? a.s : decimal(a.v);
            break;
        }
        case 'l': {  // [BYTES seg10:0xa79c seg6:0xeb8]
            const FArg a = take();
            if (on) out += decimal(static_cast<std::int32_t>(a.v));
            break;
        }
        default:  // [BYTES seg10:0xa42f]
            i += 3;
            continue;
        }
        i += 2;
    }
    return out;
}

void lang_format_into(std::string& buf, int id, int mode, const std::vector<FArg>& args,
                      const LangLookup& lang, int phase, const std::function<int()>& rand) {
    const std::string fmt = lang ? lang(id) : std::string();
    const std::string s = lang_format(fmt, args, lang, phase, rand);
    if (mode == 0)
        buf += s;
    else
        buf = s;
}

}  // namespace is::briefing
