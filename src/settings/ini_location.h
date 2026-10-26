// Where the port keeps INSPACE.INI (ADR 0004 decisions 2, 3, 5 and 7) and the start-up rules
// around it. Portable: the caller supplies the Win32 folders (the executable's directory,
// %APPDATA% from SHGetKnownFolderPath(FOLDERID_RoamingAppData), the Windows directory).
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace is::settings {

// The original names the file without a path [BYTES seg6:0x6ef].
inline constexpr const char* kIniName = "INSPACE.INI";
// The folder under %APPDATA% (ADR 0004 decision 2).
inline constexpr const char* kAppDataFolder = "Inner Space";

enum class IniSource {
    kOverride,  // --ini <path>
    kPortable,  // an INSPACE.INI next to the executable
    kAppData,   // %APPDATA%\Inner Space\INSPACE.INI
};

struct IniLocation {
    std::filesystem::path path;
    IniSource source = IniSource::kAppData;
    // --ini named a file in the asset directory, which the port never writes (ADR 0002
    // decision 5); the override was dropped and the usual rule chose `path` [DESIGN].
    bool override_refused = false;
};

// ADR 0004 decision 2: --ini wins, unless it names a file in `asset_dir` (ADR 0002 decision
// 5, see IniLocation::override_refused); else an existing INSPACE.INI in `exe_dir` (portable mode);
// else `appdata_dir`\Inner Space\INSPACE.INI. The file need not exist.
// Portable mode is off when `exe_dir` is the asset directory `asset_dir`: the INSPACE.INI
// there is the distribution's own file, which the original never writes (its installer copies
// it to the Windows directory), and ADR 0002 decision 5 keeps the game directory read-only.
// Portable mode is also off when `exe_dir` holds INSTALL.INI: it is an install set, whose own
// INSPACE.INI the install step copies and must not write [DESIGN] (ADR 0008 decision 1).
// That file still reaches the port through the first-run copy only when it is the Windows
// directory's [DESIGN] (needs an ADR 0004 amendment).
IniLocation resolve_ini_location(const std::optional<std::filesystem::path>& override_path,
                                 const std::filesystem::path& exe_dir,
                                 const std::filesystem::path& appdata_dir,
                                 const std::filesystem::path& asset_dir = {});

// ADR 0004 decision 3: when `target` does not exist and `windows_ini` (<Windows
// directory>\INSPACE.INI) does, creates target's directory and copies the file. Returns true
// when it copied. Throws std::filesystem::filesystem_error when the copy fails.
bool first_run_copy(const std::filesystem::path& target,
                    const std::filesystem::path& windows_ini);

// LocalPath's default (ADR 0004 decision 5): the INI's own directory; "." for a bare name.
std::filesystem::path ini_directory(const std::filesystem::path& ini);

// A directory key of the INI (cp1252, backslashes) as a path; empty for an empty value.
std::filesystem::path path_from_ini(const std::string& cp1252);

// The message seg6:0x6d8 shows before it regenerates an INI with an empty Path:
// MessageBox(DGROUP:0x16, DGROUP:0x2d6, "Inner Space", MB_ICONEXCLAMATION). DGROUP:0x16 is
// still 0 at that point, so the box has no owner [BYTES seg6:0x7e9 seg6:0x7f2 DGROUP:0x2d6].
inline constexpr const char* kRegenerateMessage =
    "Some game settings are missing. Will attempt to regenerate INSPACE.INI. For best results, "
    "reinstall from installer.";

}  // namespace is::settings
