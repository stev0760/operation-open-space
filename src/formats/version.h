// VERSION resources (VS_VERSION_INFO) in the Win16 layout: a tree of nodes, each a length
// WORD, a value-length WORD, an ASCIIZ key and the value, each part padded to a DWORD, then
// the children up to the node's length. Unlike Win32 there is no wType and the text is
// cp1252. docs/re/resources.md#win16-layouts; re/isre/res_formats.py parse_version is the
// reference that writes work/assets/<module>/version.json.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "formats/ne.h"

namespace is::fmt {

struct VersionNode {
    std::string key;                  // cp1252
    std::vector<std::uint8_t> value;  // value-length bytes
    std::vector<VersionNode> children;
};

// VS_FIXEDFILEINFO, the root node's value.
struct FixedFileInfo {
    static constexpr std::uint32_t kSignature = 0xFEEF04BD;
    std::uint32_t signature = 0;
    std::uint32_t struc_version = 0;
    std::uint32_t file_version_ms = 0;
    std::uint32_t file_version_ls = 0;
    std::uint32_t product_version_ms = 0;
    std::uint32_t product_version_ls = 0;
    std::uint32_t file_flags_mask = 0;
    std::uint32_t file_flags = 0;
    std::uint32_t file_os = 0;
    std::uint32_t file_type = 0;
    std::uint32_t file_subtype = 0;
    std::uint32_t file_date_ms = 0;
    std::uint32_t file_date_ls = 0;

    // "a.b.c.d" from the four version words (INSPACE.EXE: "1.29.0.0"; the "1.29.08" that
    // the game shows is the FileVersion string).
    std::string file_version() const;
    std::string product_version() const;
};

// One StringFileInfo block, such as "040904E4" (U.S. English, code page 1252).
struct VersionStringTable {
    std::string key;
    std::vector<std::pair<std::string, std::string>> strings;  // stored order; cp1252,
                                                               // value up to its first NUL
};

struct VersionInfo {
    VersionNode root;                          // key "VS_VERSION_INFO"
    std::optional<FixedFileInfo> fixed;        // when the root value holds 52 bytes or more
    std::vector<VersionStringTable> string_tables;
    // VarFileInfo entries as WORD arrays, e.g. "Translation" {0x0409, 0x04E4}.
    std::vector<std::pair<std::string, std::vector<std::uint16_t>>> vars;
    std::size_t size = 0;                      // the root node's length

    // The value of `key` in the first string table that has it, or nullptr.
    const std::string* find_string(const std::string& key) const;
};

// Throws FormatError on a node that overruns its parent or the data, a key or a non-empty
// value that overruns its node, nesting deeper than 8 levels, a root key other than
// VS_VERSION_INFO, or a bad VS_FIXEDFILEINFO signature. A node with an empty value may end
// right after its key's NUL, without the DWORD padding, as res_formats.py accepts it.
VersionInfo parse_version16(const std::uint8_t* p, std::size_t n);

// The module's first VERSION resource, or nullopt when it has none.
std::optional<VersionInfo> load_version(const NeModule& m);

}  // namespace is::fmt
