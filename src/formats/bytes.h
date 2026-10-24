// Bounds-checked little-endian reading and cp1252 text, shared by the format readers.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace is::fmt {

// Malformed or truncated input. Every reader throws this and nothing else for bad data.
struct FormatError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size, std::size_t pos = 0)
        : d_(data), n_(size), p_(pos) {
        if (pos > size) throw FormatError("start past end");
    }

    std::size_t pos() const { return p_; }
    std::size_t size() const { return n_; }
    std::size_t remaining() const { return n_ - p_; }
    const std::uint8_t* ptr() const { return d_ + p_; }

    void seek(std::size_t pos) {
        if (pos > n_) throw FormatError("seek past end");
        p_ = pos;
    }
    void skip(std::size_t k) {
        need(k);
        p_ += k;
    }

    std::uint8_t u8() {
        need(1);
        return d_[p_++];
    }
    std::uint16_t u16() {
        need(2);
        std::uint16_t v = static_cast<std::uint16_t>(d_[p_] | (d_[p_ + 1] << 8));
        p_ += 2;
        return v;
    }
    std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
    std::uint32_t u32() {
        std::uint32_t lo = u16();
        return lo | (std::uint32_t{u16()} << 16);
    }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }

    // NUL-terminated byte string (cp1252), without the NUL.
    std::string cstr() {
        std::string s;
        for (std::uint8_t c = u8(); c != 0; c = u8()) s.push_back(static_cast<char>(c));
        return s;
    }
    // Length-prefixed (one byte) string.
    std::string pstr() {
        std::size_t len = u8();
        need(len);
        std::string s(reinterpret_cast<const char*>(d_ + p_), len);
        p_ += len;
        return s;
    }

private:
    void need(std::size_t k) const {
        if (k > n_ - p_) throw FormatError("read past end");
    }

    const std::uint8_t* d_;
    std::size_t n_;
    std::size_t p_;
};

// Windows-1252 to UTF-16; bytes that cp1252 leaves undefined map to the same code point.
std::u16string cp1252_to_utf16(const std::string& s);

// UTF-16 to UTF-8. An unpaired surrogate becomes U+FFFD; nothing throws but std::bad_alloc.
std::string utf8_from_utf16(const std::u16string& w);

// A path for an exception message. On Windows: its native UTF-16 through utf8_from_utf16, so
// a name with an unpaired surrogate, which NTFS allows, gives U+FFFD where path::u8string
// throws. Elsewhere: its native bytes as they are, UTF-8 or not, as path::u8string gives
// them. Nothing throws but std::bad_alloc.
std::string path_utf8(const std::filesystem::path& p);

// ---- names
// File, module and resource names compare by one case fold (ADR 0002, decision 3): the
// lower-case letters that cp1252 holds in both cases become upper case, as Windows and Wine
// fold file names. They are ASCII a-z, U+00E0-U+00FE except U+00F7 (to U+00C0-U+00DE), and
// U+0161 U+0153 U+017E U+00FF (to U+0160 U+0152 U+017D U+0178). U+00DF and U+00B5 have no
// upper-case letter in cp1252 and stay; so does every other character. The three overloads
// apply this one rule to a code point, to cp1252 bytes and to UTF-16 code units. Two names
// are the same name when their folds are equal.

char32_t fold_name(char32_t c);
std::string fold_name(std::string cp1252);
std::u16string fold_name(std::u16string utf16);

// Throws FormatError when the file cannot be opened.
std::vector<std::uint8_t> read_file(const std::filesystem::path& path);

}  // namespace is::fmt
