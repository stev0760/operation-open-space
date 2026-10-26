#include "install/install_ini.h"

#include "settings/settings.h"

namespace is::install {

std::int16_t version_number(const std::string& v) {
    // [BYTES INSTALL.EXE seg2:0x4af7 seg2:0x4b02 seg2:0x4b0e seg2:0x4b3d]
    std::int16_t sum = 0;
    std::int16_t w = 10000;
    for (char c : v) {
        if (c == 0) break;  // lstrlen
        const std::int16_t d = static_cast<std::int16_t>(static_cast<signed char>(c) - 0x30);
        sum = static_cast<std::int16_t>(sum + static_cast<std::int16_t>(d * w));  // imul, low word
        w = static_cast<std::int16_t>(w / 10);
    }
    return sum;
}

std::string version_label(const std::string& v) {
    std::string out = "V0.00.00";  // DGROUP:0xc5e
    char b[5] = {0, 0, 0, 0, 0};
    for (std::size_t i = 0; i < 5 && i < v.size(); ++i) b[i] = v[i];
    // A NUL that lands in the label ends it there, as the original's string does.
    const std::size_t pos[5] = {1, 3, 4, 6, 7};
    for (int i = 0; i < 5; ++i) out[pos[i]] = b[i];
    const std::size_t nul = out.find('\0');
    if (nul != std::string::npos) out.resize(nul);
    return out;
}

InstallIni parse_install_ini(const settings::IniFile& ini) {
    InstallIni r;
    // GetPrivateProfileString("Vers", "V", "", buf, 10) [BYTES INSTALL.EXE seg2:0x4af2].
    r.v = ini.get_string("Vers", "V", "", 10);
    r.version = version_number(r.v);
    for (const auto& kv : ini.entries("Dirs")) r.dirs.push_back(kv.first);
    for (int g = 0; g < kGroupCount; ++g) {
        const std::string section = std::to_string(g);
        for (const auto& kv : ini.entries(section))
            r.groups[static_cast<std::size_t>(g)].push_back(
                {kv.first, ini.get_int(section, kv.first, 0)});
    }
    return r;
}

InstallIni load_install_ini(const std::filesystem::path& path) {
    return parse_install_ini(settings::IniFile(path));
}

std::string probe_name(const std::string& key, char last) {
    if (key.empty()) return key;
    std::string s = key;
    s.back() = last;
    return s;
}

}  // namespace is::install
