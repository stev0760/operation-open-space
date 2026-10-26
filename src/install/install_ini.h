// INSTALL.INI, the install set's file list, as INSTALL.EXE reads it (docs/re/install.md
// "Validation" and "INSTALL.INI"). Portable: no Win32 calls.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace is::settings {
class IniFile;
}

namespace is::install {

// One key of a group section: the file name (cp1252, as the INI spells it) and its destination
// code (GetPrivateProfileInt, INSTALL.EXE seg2:0x1482).
struct IniEntry {
    std::string name;
    int code = 0;
};

constexpr int kGroupCount = 8;  // sections [0] ... [7]

struct InstallIni {
    std::string v;             // [Vers] V as GetPrivateProfileString(..., 10) reads it
    std::int16_t version = 0;  // version_number(v); 0 makes the installer exit
    // [Dirs] key names in file order (SHIPS, SOUNDS, GAMES, RETIRED on the ship disk); code n
    // in 1-7 names dirs[n - 1].
    std::vector<std::string> dirs;
    // Sections [0] ... [7], keys in file order (GetPrivateProfileString(section, NULL),
    // INSTALL.EXE seg2:0x14ec).
    std::array<std::vector<IniEntry>, kGroupCount> groups;
};

// INSTALL.EXE seg2:0x4a92: every character of `v` (up to its length) adds (c - '0') * w to a
// 16-bit sum, w running 10000, 1000, 100, 10, 1, 0, ... (16-bit imul and idiv), so only the
// first five characters count and no character is checked for being a digit
// [BYTES INSTALL.EXE seg2:0x4af7 seg2:0x4b0e seg2:0x4b1b seg2:0x4b28 seg2:0x4b3a].
std::int16_t version_number(const std::string& v);
// "V0.00.00" with characters 0-4 of `v` at positions 1, 3, 4, 6 and 7 (INSTALL.EXE DGROUP:0x7bc,
// the splash's "V1.29.08") [BYTES INSTALL.EXE seg2:0x4b3f-0x4b72 DGROUP:0xc5e]. A shorter `v`
// puts its NUL there, as the original copies the buffer's bytes.
std::string version_label(const std::string& v);

InstallIni parse_install_ini(const settings::IniFile& ini);
// Reads the file; a missing file reads as an empty INI (version 0), as the profile calls do.
InstallIni load_install_ini(const std::filesystem::path& path);

// The source name probe INSTALL.EXE seg2:0x36c2 makes for key K: K with its last character
// replaced by '1' (read through LZ when present) and by '_' [BYTES INSTALL.EXE seg2:0x36f4
// seg2:0x371f seg2:0x3747].
std::string probe_name(const std::string& key, char last);

}  // namespace is::install
