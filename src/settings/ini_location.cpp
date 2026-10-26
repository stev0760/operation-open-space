#include "settings/ini_location.h"

#include "formats/bytes.h"

namespace is::settings {

namespace fs = std::filesystem;

IniLocation resolve_ini_location(const std::optional<fs::path>& override_path,
                                 const fs::path& exe_dir, const fs::path& appdata_dir,
                                 const fs::path& asset_dir) {
    if (override_path) return {*override_path, IniSource::kOverride};
    std::error_code ec;
    const fs::path portable = exe_dir / kIniName;
    // Not in the asset directory: that INSPACE.INI is the game's own file [DESIGN].
    const bool in_assets = !asset_dir.empty() && fs::equivalent(exe_dir, asset_dir, ec) && !ec;
    ec.clear();
    if (!exe_dir.empty() && !in_assets && fs::is_regular_file(portable, ec))
        return {portable, IniSource::kPortable};
    return {appdata_dir / kAppDataFolder / kIniName, IniSource::kAppData};
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
