// isview's text conversions (ADR 0003). Free of Win32, so that tests/test_isview_dump.cpp
// can check them; main.cpp wraps them for std::wstring, which is UTF-16 on Windows.
//
// Game text is cp1252 and converts with is::fmt::cp1252_to_utf16. Exception texts are mixed:
// the asset store writes paths into its messages as UTF-8 (path::u8string) and names from game
// data or the caller as cp1252, so message_to_utf16 decodes each valid UTF-8 sequence as
// UTF-8 and every other byte as cp1252.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "formats/bytes.h"

namespace is::isview {

// UTF-16 to UTF-8. An unpaired surrogate becomes U+FFFD.
inline std::string utf8_from_utf16(const std::u16string& w) {
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

// UTF-16 to cp1252; nullopt when a character has no cp1252 byte. File names only.
inline std::optional<std::string> cp1252_from_utf16(const std::u16string& w) {
    static const std::u16string high = [] {
        std::string b;
        for (int i = 0x80; i < 0x100; ++i) b.push_back(static_cast<char>(i));
        return fmt::cp1252_to_utf16(b);
    }();
    std::string out;
    for (char16_t c : w) {
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            continue;
        }
        const std::size_t at = high.find(c);
        if (at == std::u16string::npos) return std::nullopt;
        out.push_back(static_cast<char>(0x80 + at));
    }
    return out;
}

// An exception's what() as UTF-16: each well-formed UTF-8 sequence (no overlong form, no
// surrogate, at most U+10FFFF) decodes as UTF-8, and every other byte as cp1252.
inline std::u16string message_to_utf16(const std::string& s) {
    std::u16string out;
    std::size_t i = 0;
    while (i < s.size()) {
        const auto b = static_cast<unsigned char>(s[i]);
        if (b < 0x80) {
            out.push_back(b);
            ++i;
            continue;
        }
        std::size_t len = 0;
        char32_t cp = 0, least = 0;
        if (b >= 0xC2 && b <= 0xDF) {
            len = 2;
            cp = b & 0x1Fu;
            least = 0x80;
        } else if (b >= 0xE0 && b <= 0xEF) {
            len = 3;
            cp = b & 0x0Fu;
            least = 0x800;
        } else if (b >= 0xF0 && b <= 0xF4) {
            len = 4;
            cp = b & 0x07u;
            least = 0x10000;
        }
        bool ok = len > 0 && len <= s.size() - i;
        for (std::size_t k = 1; ok && k < len; ++k) {
            const auto c = static_cast<unsigned char>(s[i + k]);
            if ((c & 0xC0) != 0x80) ok = false;
            else cp = (cp << 6) | (c & 0x3Fu);
        }
        if (ok && (cp < least || cp > 0x10FFFF || (cp >= 0xD800 && cp < 0xE000))) ok = false;
        if (!ok) {
            out += fmt::cp1252_to_utf16(std::string(1, s[i]));
            ++i;
            continue;
        }
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<char16_t>(cp));
        }
        i += len;
    }
    return out;
}

// A character the views draw as itself. Not printable: the C0 controls, DEL and U+0080-U+009F,
// which a widened cp1252 string holds only for the five bytes cp1252 leaves undefined (0x81,
// 0x8D, 0x8F, 0x90, 0x9D; is::fmt::cp1252_to_utf16 maps them to themselves).
inline bool printable(char16_t c) { return c >= 0x20 && !(c >= 0x7F && c <= 0x9F); }

// One line of a list: tab, line feed and carriage return as \t \n \r, and every other
// character that is not printable as \xNN.
inline std::u16string escape_controls(const std::u16string& s) {
    static const char hex[] = "0123456789ABCDEF";
    std::u16string o;
    for (char16_t c : s) {
        if (c == u'\t') o += u"\\t";
        else if (c == u'\n') o += u"\\n";
        else if (c == u'\r') o += u"\\r";
        else if (!printable(c)) {
            o += u"\\x";
            o.push_back(static_cast<char16_t>(hex[(c >> 4) & 0xF]));
            o.push_back(static_cast<char16_t>(hex[c & 0xF]));
        } else {
            o.push_back(c);
        }
    }
    return o;
}

}  // namespace is::isview
