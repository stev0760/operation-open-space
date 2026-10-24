// The table of known original files and the identification of a game directory against it
// (ADR 0002, decision 4). The table holds one row per file of each package, generated from
// original.sha256 into known_files_table.cpp by tools/gen_known_files.py. A file that matches
// no row is reported as unknown and still used: ships edited with the Ship Factory, or an
// INSPACE.INI the game has written, differ from the shipped bytes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/game_files.h"

namespace is::assets {

enum class Package : std::uint8_t {
    kShipdisk,  // the distribution disk of v1.29.08 (original/shipdisk)
    kTheWorks,  // the THEWORKS add-on: the Enhanced Ship Factory, its DIBs and WAVs
                // (original/theworks)
};

// "shipdisk v1.29.08" or "theworks". 1.29.08 is INSPACE.EXE's FileVersion string
// [BYTES INSPACE.EXE VERSION 1].
const char* package_label(Package p);

struct KnownFile {
    const char* name;          // installed name, as `isre extract` spells it
    Package package;
    std::uint32_t size;        // installed (expanded) size
    const char* sha256;        // lowercase hex SHA-256 of the installed bytes
    const char* dist_name;     // the name as shipped (equal to `name` for unchanged files)
    std::uint32_t dist_size;   // the size as shipped
    const char* dist_sha256;   // the SHA-256 as shipped: original.sha256's value
};

struct KnownFiles {
    const KnownFile* first = nullptr;
    std::size_t count = 0;
    const KnownFile* begin() const { return first; }
    const KnownFile* end() const { return first + count; }
    std::size_t size() const { return count; }
};

// The generated table, sorted by package and then by installed name.
KnownFiles known_files();

// The row with this installed name (compared by name_key, the case fold of GameFiles), size
// and hash, or nullptr.
const KnownFile* find_known(const std::filesystem::path& name, std::uint64_t size,
                            const std::string& sha256);
// True when some row has this installed name (by name_key), whatever its content.
bool known_name(const std::filesystem::path& name);

struct FileReport {
    std::filesystem::path path;        // the file on disk
    std::filesystem::path name;        // its installed name
    std::uint64_t size = 0;            // installed size
    std::string sha256;                // hex of the installed bytes; empty when unreadable
    const KnownFile* known = nullptr;  // the matching row; nullptr for an unknown file
    bool name_known = false;           // a row has this name (a changed copy of a known file)
    std::string error;                 // why the file could not be read or expanded
};

struct Identification {
    DirForm form = DirForm::kInstalled;
    std::vector<FileReport> files;   // in GameFiles::entries() order
    std::vector<Package> packages;   // packages with a known file, in enum order
    // GameFiles::shadowed(): files left out because another file restores to the same
    // installed name. Not hashed and not in `files`; an anomaly to report all the same.
    std::vector<std::filesystem::path> shadowed;

    // Files in use that match no row; `shadowed` does not count.
    std::size_t unknown_count() const;
    bool all_known() const { return unknown_count() == 0; }
};

// Hashes every file of `files` (installed bytes, uncached) and looks it up in the table.
// Never throws for a file's content: an unreadable file or a bad SZDD stream is reported
// as unknown with `error` set. A report of the result (isview, the port's log) owes the
// `shadowed` files a line too (ADR 0002, decision 4).
Identification identify(const GameFiles& files);

}  // namespace is::assets
