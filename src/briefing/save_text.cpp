// The saved-game descriptions (save_text.h). Portable.
#include "briefing/save_text.h"

#include <cstring>

#include "intro/texts.h"

namespace is::briefing {

namespace {

std::int16_t w16(const std::uint8_t* h, int off) {
    return static_cast<std::int16_t>(h[off] | (h[off + 1] << 8));
}
std::uint32_t d32(const std::uint8_t* h, int off) {
    return static_cast<std::uint32_t>(static_cast<std::uint16_t>(w16(h, off))) |
           (static_cast<std::uint32_t>(static_cast<std::uint16_t>(w16(h, off + 2))) << 16);
}

struct Times {
    int min = 0, sec = 0;
};
// +0x18b / 36, a 32-bit unsigned divide (div ecx; seg5:0x2b4 in the desk), then min = (int16)t
// / 60 and sec = (int16)t - 60 min (cwd; idiv on the low word) [BYTES seg10:0xa12a-0xa13f
// seg15:0x54c0].
Times times(const std::uint8_t* h) {
    const std::uint32_t t = d32(h, 0x18b) / 36u;
    const std::int16_t lo = static_cast<std::int16_t>(static_cast<std::uint16_t>(t & 0xffff));
    Times r;
    r.min = lo / 60;
    r.sec = static_cast<std::int16_t>(lo - 60 * r.min);
    return r;
}

// The shape word: below 100 it stays (string 311 + value, ADR 0014 decision 1); else - 100
// [BYTES seg10:0xa184].
int shape_index(std::int16_t shape) { return shape < 100 ? shape : shape - 100; }

// The 3001 line, shared [BYTES seg10:0xa265 seg10:0xa26e seg10:0xa276].
void captured_line(std::string& dst, const std::uint8_t* h, const LangLookup& lang) {
    const std::int16_t cap = w16(h, 0x15a), des = w16(h, 0x15c);
    if (cap == 0 && des == 0) return;
    const int total = static_cast<std::int16_t>(w16(h, 0x156) + w16(h, 0x158));
    lang_format_into(dst, 3001, 0, {total, 2515, cap, cap, 2515, des, des, 2515}, lang);
}

}  // namespace

std::string completion_text(std::int16_t completed, std::int16_t dirs, const LangLookup& lang, bool desk) {
    if (completed == dirs) return lang(dirs == 1 ? 2503 : 2501);
    if (completed == 0) {
        if (desk && dirs <= 1) return lang(2529);  // [BYTES seg15:0x5545-0x555d]
        return lang(2502);
    }
    std::string s;
    lang_format_into(s, 2500, 1, {static_cast<int>(completed)}, lang);
    return s;
}

std::string restore_description(const std::uint8_t* h, const std::string& pilot, const LangLookup& lang,
                                bool help_window) {
    const Times t = times(h);
    std::int16_t v = w16(h, 0x162);
    const std::int16_t shape = w16(h, 0x15e);
    if (shape < 100) {
        // The old-file rule: v >= -15000 (0xc568) → v / 10 signed, else (uint16)v / 10
        // [BYTES seg10:0xa160-0xa17e].
        v = v >= -15000 ? static_cast<std::int16_t>(v / 10)
                        : static_cast<std::int16_t>(static_cast<std::uint16_t>(v) / 10u);
    }
    std::int16_t ships = 0;  // the 8 words +0x146, 16-bit [BYTES seg10:0xa18a-0xa1a8]
    for (int i = 0; i < 8; ++i) ships = static_cast<std::int16_t>(ships + w16(h, 0x146 + 2 * i));
    char name_buf[21] = {};
    std::memcpy(name_buf, h + 4, 20);
    const std::string name = std::string(name_buf);
    const bool same = name == pilot;  // lstrcmp = 0 [BYTES seg10:0xa1b6]
    const std::string done = completion_text(w16(h, 0x160), w16(h, 0x32), lang, false);
    std::string dst;
    lang_format_into(dst, 3000, 1, {1, w16(h, 0x24), t.min, 2505, t.sec, 2504, w16(h, 0x32), 318, done},
                     lang);  // [BYTES seg10:0xa255]
    captured_line(dst, h, lang);
    const int whole = v / 10, tenth = v % 10 < 0 ? -(v % 10) : v % 10;  // seg9:0x88ba abs
    lang_format_into(dst, 3002, 0, {whole, tenth, 311 + shape_index(shape)}, lang);  // [BYTES seg10:0xa2be-0xa2da]
    if (help_window) {
        // seg11:0x3fd4(name, 1) [BYTES seg10:0xa2f4-0xa321]
        lang_format_into(dst, 3027, 0,
                         {same ? 1 : 0, same ? 0 : 1, intro::case_rule(name, 1), ships == 0 ? 1 : 0,
                          static_cast<int>(ships), static_cast<int>(ships), 304},
                         lang);
    }
    return dst;
}

std::string desk_progress(const std::uint8_t* h, const LangLookup& lang) {
    const Times t = times(h);
    const std::int16_t shape = w16(h, 0x15e);
    const std::string done = completion_text(w16(h, 0x160), w16(h, 0x32), lang, true);
    std::string dst;
    lang_format_into(dst, 3000, 0, {0, w16(h, 0x24), t.min, 2505, t.sec, 2504, w16(h, 0x32), 318, done}, lang);
    captured_line(dst, h, lang);
    // 3031 with (+0x164 > 400, 311 + shape) [BYTES seg15:0x5601-0x562c]
    lang_format_into(dst, 3031, 0, {w16(h, 0x164) > 400 ? 1 : 0, 311 + shape_index(shape)}, lang);
    return dst;
}

}  // namespace is::briefing
