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

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw FormatError("cannot open " + path.u8string());
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}

}  // namespace is::fmt
