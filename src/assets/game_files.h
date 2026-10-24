// The user's original files (ADR 0002): an installed copy (expanded files under their
// installed names) or the distribution as shipped (14 files SZDD-compressed under changed
// names such as ENG_LANG.dl1, and the two executables stored plain as INSPACE.ex_ and
// FACTORY.ex_). GameFiles lists one directory and gives each file's bytes by installed
// name, whichever form the directory holds. Each file is handled by its own name and
// content, so a directory that mixes both forms works as well.
//
// Names are matched without regard to case (ADR 0002, decision 3), because the shipped names
// mix cases (ISPACE.dll, eng_fx.dll, Eng_help.hl1). Case folds for every letter that cp1252
// holds in both cases (is::fmt::fold_name, through name_key), as Windows and Wine fold file
// names. Paths stay
// std::filesystem::path; a name that comes from game data (a module name, a file name in
// MASTER.BOD) is a cp1252 string and converts to a path only for the comparison (ADR 0003).
//
// Nothing here writes to the directory. The class is not thread-safe.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace is::assets {

// A file or resource that the caller asked for is missing, or a file cannot be read.
// Malformed data throws is::fmt::FormatError instead.
struct NotFound : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class DirForm : std::uint8_t {
    kInstalled,     // every file is stored under its installed name
    kDistribution,  // at least one file is stored under a distribution name
};

// The installed name of a distribution file name, as re/isre/szdd.py restored_name gives it:
// a known distribution extension is replaced (.mi1 .dl1 .hl1 .bo1 .sh1 .in1 .a1 .sc1 .ex_,
// compared without case, become .MID .DLL .HLP .BOD .SHP .INI .AD .SCR .EXE; the stem keeps
// its case). Otherwise a trailing '_' becomes `last_char`, the SZDD header's stored
// character, when that is printable ASCII other than a space. `name` is a bare file name.
std::filesystem::path restored_name(const std::filesystem::path& name, char last_char = 0);

// A cp1252 name (from game data or the caller) as a path, through UTF-16.
std::filesystem::path path_from_cp1252(const std::string& name);

// The comparison key of a file name: its native characters folded by is::fmt::fold_name, which
// upper-cases the lower-case letters of cp1252: ASCII a-z, U+00E0-U+00FE except U+00F7 (to
// U+00C0-U+00DE), and U+0161 U+0153 U+017E U+00FF (to U+0160 U+0152 U+017D U+0178). Every
// other character stays as it is; on a system whose native paths are UTF-8, a byte sequence
// that is not one of these letters stays too.
std::filesystem::path::string_type name_key(const std::filesystem::path& name);

class GameFiles {
public:
    struct Entry {
        std::filesystem::path path;  // the file on disk
        std::filesystem::path name;  // its installed name
        bool distribution_name = false;  // stored under a name other than `name`
    };

    // Lists the regular files of `dir` (not its subdirectories). A file whose name ends in
    // '_' has its first 14 bytes read for the SZDD header's stored character; nothing else is
    // read until asked for. Throws NotFound when `dir` is not a readable directory.
    explicit GameFiles(std::filesystem::path dir);

    const std::filesystem::path& dir() const { return dir_; }
    DirForm form() const { return form_; }
    // One entry per installed name, sorted by name_key.
    const std::vector<Entry>& entries() const { return entries_; }
    // Files left out because another file has the same installed name. The file stored under
    // the installed name itself wins (ENG_LANG.DLL over ENG_LANG.dl1); otherwise the first in
    // native name order wins. `isre extract` refuses such a clash; here it is an anomaly that
    // identify() passes on for the report (ADR 0002, decision 4).
    const std::vector<std::filesystem::path>& shadowed() const { return shadowed_; }

    // The entry whose installed name equals `name` under name_key, or nullptr. `name` is
    // cp1252.
    const Entry* find(const std::string& name) const;
    bool has(const std::string& name) const { return find(name) != nullptr; }

    // The installed bytes: the file's content, SZDD-expanded when it starts with the SZDD
    // magic. Cached: the reference stays valid until forget(name) or destruction. Throws
    // NotFound for a missing file or one that cannot be opened or read, FormatError for a
    // bad SZDD stream, and nothing else but std::bad_alloc.
    const std::vector<std::uint8_t>& bytes(const std::string& name) const;
    // The same bytes, read afresh and not cached; the same errors.
    std::vector<std::uint8_t> read(const std::string& name) const;
    std::vector<std::uint8_t> read(const Entry& e) const;
    // Drops a cached file, so the next bytes() reads it again (after the Ship Factory, M8,
    // rewrites MASTER.BOD, for example).
    void forget(const std::string& name);

private:
    const Entry& need(const std::string& name) const;

    std::filesystem::path dir_;
    DirForm form_ = DirForm::kInstalled;
    std::vector<Entry> entries_;
    std::map<std::filesystem::path::string_type, std::size_t> index_;
    std::vector<std::filesystem::path> shadowed_;
    mutable std::map<std::filesystem::path::string_type, std::vector<std::uint8_t>> cache_;
};

// ---- discovery (ADR 0002, decision 1)

// The modules whose presence marks a directory as the game's [DESIGN]: the main program, the
// game DLL and the graphics DLL, by installed name. The language DLL is left out because the
// INI's Language key, which M2 reads, names it.
inline constexpr std::array<const char*, 3> kGameModules = {"INSPACE.EXE", "ISPACE.DLL",
                                                            "IGRAPHIC.DLL"};

// True when `dir` is a readable directory holding every kGameModules file, in either form.
bool holds_game(const std::filesystem::path& dir);

// The first candidate that holds the game, or nullopt. The caller passes the candidates in
// ADR 0002's order: the executable's directory, then the directory named on the command line
// or in the port's INI (M2). Empty paths are skipped.
std::optional<std::filesystem::path> discover(const std::vector<std::filesystem::path>& candidates);

}  // namespace is::assets
