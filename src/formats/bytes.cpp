#include "formats/bytes.h"

#include <fstream>
#include <iterator>

namespace is::fmt {

namespace {
// cp1252 0x80-0x9F; 0 marks the five undefined bytes, which map to themselves.
constexpr char16_t kHigh[32] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178,
};
}  // namespace

std::u16string cp1252_to_utf16(const std::string& s) {
    std::u16string out;
    out.reserve(s.size());
    for (char ch : s) {
        auto b = static_cast<std::uint8_t>(ch);
        char16_t c = b;
        if (b >= 0x80 && b < 0xA0 && kHigh[b - 0x80] != 0) c = kHigh[b - 0x80];
        out.push_back(c);
    }
    return out;
}

std::string utf8_from_utf16(const std::u16string& w) {
    std::string o;
    for (std::size_t i = 0; i < w.size(); ++i) {
        char32_t c = w[i];
        if (c >= 0xD800 && c < 0xE000) {
            const bool pair = c < 0xDC00 && i + 1 < w.size() && w[i + 1] >= 0xDC00 && w[i + 1] < 0xE000;
            if (pair) {
                c = 0x10000 + ((c - 0xD800) << 10) + (w[i + 1] - 0xDC00u);
                ++i;
            } else {
                c = 0xFFFD;
            }
        }
        if (c < 0x80) {
            o += static_cast<char>(c);
        } else if (c < 0x800) {
            o += static_cast<char>(0xC0 | (c >> 6));
            o += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            o += static_cast<char>(0xE0 | (c >> 12));
            o += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            o += static_cast<char>(0xF0 | (c >> 18));
            o += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            o += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return o;
}

std::string path_utf8(const std::filesystem::path& p) {
#ifdef _WIN32
    const auto& n = p.native();  // UTF-16 code units, paired or not
    return utf8_from_utf16(std::u16string(n.begin(), n.end()));
#else
    return p.native();
#endif
}

char32_t fold_name(char32_t c) {
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

std::string fold_name(std::string s) {
    for (char& ch : s) {
        const auto b = static_cast<std::uint8_t>(ch);
        std::uint8_t up = b;
        if (b >= 'a' && b <= 'z') up = static_cast<std::uint8_t>(b - 0x20);
        else if (b >= 0xE0 && b <= 0xFE && b != 0xF7) up = static_cast<std::uint8_t>(b - 0x20);
        else if (b == 0x9A || b == 0x9C || b == 0x9E) up = static_cast<std::uint8_t>(b - 0x10);  // s caron, oe, z caron
        else if (b == 0xFF) up = 0x9F;                                                           // y diaeresis
        ch = static_cast<char>(up);
    }
    return s;
}

std::u16string fold_name(std::u16string s) {
    for (char16_t& c : s) c = static_cast<char16_t>(fold_name(static_cast<char32_t>(c)));
    return s;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw FormatError("cannot open " + path_utf8(path));
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}

}  // namespace is::fmt
