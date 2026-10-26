// The start-up steps of stage 3 that need Win32: the INI location and regeneration (ADR 0004,
// step 6 of shell.md "Start-up") and the edition check's inputs (ADR 0008, step 8).
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace is::app {

// <Windows directory> (GetWindowsDirectoryW); empty when the call fails.
std::filesystem::path windows_directory();
// %APPDATA% (SHGetKnownFolderPath FOLDERID_RoamingAppData); empty when the call fails.
std::filesystem::path roaming_app_data();

// Step 6, seg6:0x6d8 with ADR 0004: resolves the file (--ini, portable, %APPDATA%), creates
// the %APPDATA% folder, copies <Windows directory>\INSPACE.INI on the first run, loads the
// file into g.ini and, when Path is empty, shows the original's message and regenerates the
// keys with the asset directory and the INI's directory. Sets g.ini_path and g.local_path.
// Returns false with a UTF-8 message in *why when the file cannot be read or written.
bool open_ini(const std::optional<std::filesystem::path>& override_path,
              const std::filesystem::path& exe_dir, std::string* why);

}  // namespace is::app
