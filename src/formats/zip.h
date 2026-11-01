// ZIP archive reading (PKWARE APPNOTE 6.3.x) for the install step: ADR 0008 decision 1 takes
// the install set as "a zip or a directory with INSTALL.INI", and a zip is unpacked into a
// directory of its own first [DESIGN]. The reader takes the central directory at the end of
// the archive, then each entry's local header for its data. Methods 0 (stored) and 8 (deflate,
// RFC 1951) are read; the inflater is this file's own. Every entry is CRC-32 checked.
// Not read: ZIP64, encryption (flag bit 0), multi-disk archives, other methods; each throws
// FormatError [REF APPNOTE 4.3, 4.4]. Portable: no Win32 calls.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "formats/bytes.h"

namespace is::fmt {

struct ZipEntry {
    std::string name;            // as stored, '/' separated (cp437 or UTF-8 by flag bit 11)
    bool utf8 = false;           // general purpose flag bit 11
    std::uint16_t method = 0;    // 0 stored, 8 deflate
    std::uint16_t flags = 0;
    std::uint32_t crc = 0;
    std::uint32_t csize = 0;     // compressed size
    std::uint32_t usize = 0;     // uncompressed size
    std::uint32_t local_offset = 0;
    std::uint16_t dos_time = 0, dos_date = 0;
    bool is_dir() const { return !name.empty() && name.back() == '/'; }
};

class ZipArchive {
public:
    static ZipArchive parse(std::vector<std::uint8_t> bytes);
    static ZipArchive load(const std::filesystem::path& path);

    const std::vector<ZipEntry>& entries() const { return entries_; }
    // The entry's bytes, decompressed and CRC-checked.
    std::vector<std::uint8_t> extract(const ZipEntry& e) const;

private:
    std::vector<std::uint8_t> data_;
    std::vector<ZipEntry> entries_;
};

// CRC-32 (IEEE 802.3, reflected polynomial 0xEDB88320), as ZIP and zlib compute it.
std::uint32_t crc32(const std::uint8_t* p, std::size_t n, std::uint32_t crc = 0);

// The largest archive ZipArchive::load reads, and the most an install set may unpack to
// (install.h unpack_zip): the shipped sets are a few MB [DESIGN].
constexpr std::uint64_t kZipMaxBytes = 256ull << 20;

// A raw deflate stream (RFC 1951, no zlib header) expanded. `size_hint` reserves the output;
// the output never grows past `limit` (a stream that would throws), so a small entry cannot
// expand without bound. Throws FormatError on a malformed or truncated stream.
std::vector<std::uint8_t> inflate(const std::uint8_t* p, std::size_t n, std::size_t size_hint = 0,
                                  std::size_t limit = static_cast<std::size_t>(-1));

// An entry name as a relative path under a target directory, or empty when it is unsafe
// (absolute, a drive letter, a ".." component, NUL or a control character, a character Win32
// file names cannot hold, a component ending in '.' or ' ', a DOS device name such as CON or
// LPT1.TXT) or names nothing. Backslashes count as
// separators, as some DOS zippers store them. A name without flag bit 11 is read as cp1252,
// the install sets' own code page, not APPNOTE's cp437; the sets' names are ASCII [DESIGN].
std::filesystem::path zip_entry_path(const std::string& name, bool utf8);

}  // namespace is::fmt
