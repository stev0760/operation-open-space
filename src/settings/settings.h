// INSPACE.INI for the port (ADR 0004): one file with the original's sections, keys, formats
// and defaults, read and written with the semantics of the Windows profile functions.
// Key meanings, defaults and readers: docs/re/formats.md#ini-files. Portable: no Win32 calls.
#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace is::settings {

// One INI file. Section and key names compare case-insensitively (cp1252 fold, as the profile
// functions do for ASCII). Values are cp1252 bytes (ADR 0003, decision 4). Unknown sections
// and keys survive a save; comments and blank lines may not.
class IniFile {
public:
    // Reads the file when it exists; a missing file is an empty INI with that path.
    explicit IniFile(std::filesystem::path path);

    const std::filesystem::path& path() const { return path_; }
    bool existed() const { return existed_; }  // the file was present when constructed

    // GetPrivateProfileString: the value without quotes, or nullopt when section or key is
    // absent. `nsize` truncates to nsize - 1 bytes as the Win16 call does (0 = no limit).
    std::optional<std::string> get(std::string_view section, std::string_view key,
                                   std::size_t nsize = 0) const;
    std::string get_string(std::string_view section, std::string_view key,
                           std::string_view def, std::size_t nsize = 0) const;
    // GetPrivateProfileInt: decimal, optional sign, parsing stops at the first other
    // character; an absent key or an empty value gives `def`.
    int get_int(std::string_view section, std::string_view key, int def) const;

    // WritePrivateProfileString: creates the section, replaces or appends the key.
    void set(std::string_view section, std::string_view key, std::string value);
    void set_int(std::string_view section, std::string_view key, int value);
    void remove(std::string_view section, std::string_view key);

    // Writes the file (cp1252, CRLF), creating directories as needed. Throws
    // std::runtime_error when the file cannot be written.
    void save() const;

    // Sections and keys in file order, for tests and diagnostics.
    std::vector<std::string> sections() const;
    std::vector<std::pair<std::string, std::string>> entries(std::string_view section) const;

private:
    struct Section {
        std::string name;
        std::vector<std::pair<std::string, std::string>> entries;
    };
    Section* find(std::string_view section);
    const Section* find(std::string_view section) const;

    std::filesystem::path path_;
    bool existed_ = false;
    std::vector<Section> sections_;
};

// The section of every game key.
constexpr std::string_view kGameSection = "Inner Space";

// The regeneration of seg6:0x6d8 (formats.md#files-and-sections), with the asset directory
// as the program directory (ADR 0004, decision 4) and `local_dir` as LocalPath (decision 5):
// when [Inner Space] Path is empty, writes Path, LocalPath, ShipPath, SndPath, RacePath, Dead,
// Games, Language, MainShip and ThePlayer and returns true. Otherwise changes nothing.
bool regenerate(IniFile& ini, const std::filesystem::path& asset_dir,
                const std::filesystem::path& local_dir);

}  // namespace is::settings
