#include "settings/ini_location.h"

#include "formats/bytes.h"

namespace is::settings {

namespace fs = std::filesystem;

IniLocation resolve_ini_location(const std::optional<fs::path>& override_path,
                                 const fs::path& exe_dir, const fs::path& appdata_dir,
                                 const fs::path& asset_dir) {
    std::error_code ec;
    bool refused = false;
    if (override_path) {
        // The game directory stays read-only (ADR 0002 decision 5), so an --ini inside it is
        // dropped, as portable mode is below [DESIGN].
        refused = !asset_dir.empty() &&
                  fs::equivalent(ini_directory(*override_path), asset_dir, ec) && !ec;
        ec.clear();
        if (!refused) return {*override_path, IniSource::kOverride};
    }
    const fs::path portable = exe_dir / kIniName;
    // Not in the asset directory: that INSPACE.INI is the game's own file [DESIGN].
    const bool in_assets = !asset_dir.empty() && fs::equivalent(exe_dir, asset_dir, ec) && !ec;
    ec.clear();
    // Nor in an install set (a directory holding INSTALL.INI): the game carries its own
    // INSPACE.INI, which is the user's file and the install step's source (ADR 0002 decision
    // 5, ADR 0008 decision 1) [DESIGN].
    const bool in_set = !exe_dir.empty() && fs::is_regular_file(exe_dir / "INSTALL.INI", ec);
    ec.clear();
    if (!exe_dir.empty() && !in_assets && !in_set && fs::is_regular_file(portable, ec))
        return {portable, IniSource::kPortable, refused};
    return {appdata_dir / kAppDataFolder / kIniName, IniSource::kAppData, refused};
}

bool first_run_copy(const fs::path& target, const fs::path& windows_ini) {
    std::error_code ec;
    if (fs::exists(target, ec) || windows_ini.empty() || !fs::is_regular_file(windows_ini, ec))
        return false;
    const fs::path dir = target.parent_path();
    if (!dir.empty()) fs::create_directories(dir);
    fs::copy_file(windows_ini, target);
    return true;
}

fs::path ini_directory(const fs::path& ini) {
    const fs::path dir = ini.parent_path();
    return dir.empty() ? fs::path(".") : dir;
}

fs::path path_from_ini(const std::string& cp1252) {
    if (cp1252.empty()) return {};
    std::u16string w = fmt::cp1252_to_utf16(cp1252);
#ifndef _WIN32
    for (char16_t& c : w)
        if (c == u'\\') c = u'/';
#endif
    return fs::path(w);
}

}  // namespace is::settings
