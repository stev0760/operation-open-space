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

std::vector<std::uint8_t> read_file(const std::filesystem::path& path);

}  // namespace is::fmt
