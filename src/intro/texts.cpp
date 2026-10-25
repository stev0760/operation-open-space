#include "intro/texts.h"

#include <algorithm>
#include <cstring>

namespace is::intro {

std::string load_limit(const std::string& s, int max) {
    if (max <= 0) return {};
    std::string r = s.substr(0, std::min(s.size(), static_cast<std::size_t>(max - 1)));
    const auto nul = r.find('\0');
    if (nul != std::string::npos) r.resize(nul);
    return r;
}

std::string case_rule(std::string s, int mode) {
    // seg11:0x3fd4, step for step; `upper` is bVar1.
    bool upper = true;
    const int n = static_cast<int>(s.size());
    for (int i = 0; i < n; ++i) {
        if (mode == 0) upper = false;
        else if (mode == 1) upper = true;
        const auto c = static_cast<signed char>(s[i]);
        if (upper) {
            if (c < '{' && c > '`') s[i] = static_cast<char>(c - 0x20);
        } else if (c < '[' && c > '@') {
            s[i] = static_cast<char>(c + 0x20);
        }
        upper = mode == 2 && s[i] == ' ';
        if (i < n - 3 && s[i] == ' ' && s[i + 1] == 'I' && s[i + 2] == ' ') upper = true;
        if (mode == 3 && (s[i] == '.' || s[i] == '!' || s[i] == '\r' || s[i] == '\n')) {
            upper = true;
            for (int j = i + 1; j < n && s[j] == ' '; ++j) ++i;
        }
        if (i < n && s[i] == '(') ++i;
    }
    return s;
}

std::string title_name_line(const std::string& name) {
    return case_rule(load_limit(name, 0x2d), 0) + "\x99";
}

std::string version_string(const std::string& s167, const std::string& s233) {
    std::string v = "V0.00.00";  // DGROUP:0x1277
    const std::string a = load_limit(s167, 10), b = load_limit(s233, 10);
    // The original copies the buffer's bytes whatever they hold; a short string leaves its NUL
    // in the version, which ends it there [DESIGN: the port stops at the same NUL].
    const auto put = [&](std::size_t at, const std::string& from, std::size_t i) {
        v[at] = i < from.size() ? from[i] : '\0';
    };
    put(1, a, 0);
    put(3, a, 1);
    put(4, a, 2);
    put(6, b, 0);
    put(7, b, 1);
    const auto nul = v.find('\0');
    if (nul != std::string::npos) v.resize(nul);
    return v;
}

std::string edition_line(const std::string& edition, const std::string& version,
                         const std::string& s607) {
    return edition + " - " + version + " " + load_limit(s607, 0x14);
}

int day_value(int tm_year, int tm_yday) { return (tm_year - 93) * 365 + tm_yday; }

int trial_day(int dayval, int vidsp, int pdev) {
    const int k = (pdev - vidsp) ^ 0xff;
    const int day = dayval - k + 1;
    return day < 1 ? 200 : day;
}

std::string format_day(const std::string& format, int day) {
    const auto at = format.find("%d");
    if (at == std::string::npos) return format;
    return format.substr(0, at) + std::to_string(day) + format.substr(at + 2);
}

TrialLine trial_line(int edition, int invalid, int day, const std::string& s2341,
                     const std::string& s2342, const std::string& s2343) {
    TrialLine t;
    if (edition == 3 || edition == -1) {
        t.none = true;
        return t;
    }
    if (edition == 2) {
        t.text = format_day(s2342, -day);
        return t;
    }
    if (invalid != 0)
        t.text = s2343;
    else if (day < 0)
        t.text = format_day(s2342, -day);
    else
        t.text = format_day(s2341, day);
    t.order_line = true;
    return t;
}

const char* const kCopyright = "\xA9 1994-2000 Software Dynamics, inc. All rights reserved.";

std::array<std::string, kPageLines> text_page_lines(const std::vector<std::uint8_t>& res, int page) {
    // The resource up to its first NUL, CR turned into NUL as the original does in place.
    std::string data;
    for (std::uint8_t b : res) {
        if (b == 0) break;
        data.push_back(b == '\r' ? '\0' : static_cast<char>(b));
    }
    const auto line_at = [&](std::size_t off) -> std::string {
        if (off >= data.size()) return {};
        const std::size_t end = data.find('\0', off);
        return data.substr(off, end == std::string::npos ? std::string::npos : end - off);
    };
    std::size_t off = 0;
    for (int i = 0; i < page * kPageLines; ++i) off += line_at(off).size() + 2;
    std::array<std::string, kPageLines> lines;
    for (int i = 0; i < kPageLines; ++i) {
        lines[static_cast<std::size_t>(i)] = line_at(off);
        off += lines[static_cast<std::size_t>(i)].size() + 2;
    }
    return lines;
}

int text_page_resource(int edition) { return edition == 0 || edition == 1 ? 136 : 200; }

std::array<KeyRow, kKeyRows> key_rows() {
    std::array<KeyRow, kKeyRows> rows{};
    for (int i = 0; i < kKeyRows; ++i) {
        KeyRow& r = rows[static_cast<std::size_t>(i)];
        switch (i) {  // seg15:0x6b93 seg15:0x6c0c seg15:0x6c76
        case 8: r.key_id = 1209; r.desc_id = 1309; break;   // Alt G, Game Settings
        case 9: r.key_id = 1213; r.desc_id = 1313; break;   // Alt X, Exit Game
        case 10: r.key_id = 1207; r.desc_id = 1307; break;  // F1, F2, Online Help
        default: r.key_id = 1223 + i; r.desc_id = 1323 + i; break;
        }
        // red byte -(13 i + 1) = 255 - 13 i, blue byte 12 i + 11 [BYTES seg15:0x6c18 seg15:0x6c26]
        const std::uint32_t red = static_cast<std::uint8_t>(255 - 13 * i);
        const std::uint32_t blue = static_cast<std::uint8_t>(12 * i + 11);
        r.colour = red | (blue << 16);
        r.dy = 30 * (i - 5);
    }
    return rows;
}

TitleLayout title_layout(int w, int h, int d, int c) {
    const int cx = w / 2, cy = h / 2;
    TitleLayout t;
    t.operation = {cx - 90, cy + d - 245};
    t.name = {cx, cy + d - 220};
    t.edition = {cx, cy + d - 186};
    t.trial = {cx, cy + c + 125};
    t.order = {cx, cy + c + 150};
    t.hint = {cx, cy + c + 86};
    t.copyright = {cx, cy + c + 103};
    t.esc = {cx, cy + c + 185};
    t.picture = {cx - 138, cy + d - 155};
    return t;
}

int title_offset(int h) { return h < 440 ? 40 : h < 500 ? 30 : 0; }

}  // namespace is::intro
