#include "app/startup.h"

#include <windows.h>
#include <initguid.h>
#include <knownfolders.h>
#include <shlobj.h>

#include <algorithm>
#include <ctime>
#include <exception>
#include <system_error>
#include <vector>

#include "app/app.h"
#include "formats/bytes.h"
#include "formats/ne.h"
#include "settings/ini_location.h"
#include "settings/settings.h"

namespace is::app {

namespace fs = std::filesystem;

fs::path windows_directory() {
    wchar_t buf[MAX_PATH + 1];
    const UINT n = GetWindowsDirectoryW(buf, MAX_PATH + 1);
    if (n == 0 || n > MAX_PATH) return {};
    return fs::path(std::wstring(buf, n));
}

fs::path roaming_app_data() {
    PWSTR p = nullptr;
    fs::path r;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &p)) && p)
        r = fs::path(p);
    if (p) CoTaskMemFree(p);
    return r;
}

bool open_ini(const std::optional<fs::path>& override_path, const fs::path& exe_dir,
              std::string* why) {
    try {
        const settings::IniLocation loc =
            settings::resolve_ini_location(override_path, exe_dir, roaming_app_data(),
                                               g.asset_dir);
        g.ini_path = loc.path;
        if (loc.override_refused)
            log("--ini names a file in the game directory, which the port never writes (ADR "
                "0002 decision 5); using " + fmt::path_utf8(loc.path));
        const char* source = loc.source == settings::IniSource::kOverride   ? "--ini"
                             : loc.source == settings::IniSource::kPortable ? "portable"
                                                                             : "%APPDATA%";
        if (loc.source == settings::IniSource::kAppData) {
            std::error_code ec;
            fs::create_directories(loc.path.parent_path(), ec);  // ADR 0004 decision 2
            if (ec) log("INSPACE.INI folder: " + ec.message());
        }
        // ADR 0004 decision 3: the first run takes the Windows directory's file.
        const fs::path win_dir = windows_directory();
        if (!win_dir.empty() &&
            settings::first_run_copy(loc.path, win_dir / settings::kIniName))
            log("INSPACE.INI copied from " + fmt::path_utf8(win_dir / settings::kIniName));
        g.ini = std::make_unique<settings::IniFile>(loc.path);
        log("INI (" + std::string(source) + "): " + fmt::path_utf8(loc.path) +
            (g.ini->existed() ? "" : " (new)"));

        // seg6:0x6d8: an empty Path regenerates the keys [BYTES seg6:0x7e9 seg6:0x7f2]. Where
        // the original starts GRMSETUP.EXE for a missing file, the port regenerates as well
        // (ADR 0004 decision 7) [DESIGN]. Path is the asset directory (decision 4) and
        // LocalPath the INI's directory (decision 5) [DESIGN].
        if (g.ini->get_string(settings::kGameSection, "Path", "", 0x6d).empty()) {
            MessageBoxW(nullptr, widen(settings::kRegenerateMessage).c_str(), L"Inner Space",
                        MB_ICONEXCLAMATION);
            if (settings::regenerate(*g.ini, g.asset_dir, settings::ini_directory(loc.path))) {
                g.ini->save();
                log("INSPACE.INI regenerated: Path " +
                    g.ini->get_string(settings::kGameSection, "Path", "") + ", LocalPath " +
                    g.ini->get_string(settings::kGameSection, "LocalPath", ""));
            }
        }
        // DGROUP:0x9078: LocalPath, 0x6d bytes [BYTES seg6:0x6ef]. An empty value (which a
        // regenerated file never has) falls back to the INI's directory [DESIGN].
        g.local_path = settings::path_from_ini(
            g.ini->get_string(settings::kGameSection, "LocalPath", "", 0x6d));
        if (g.local_path.empty()) g.local_path = settings::ini_directory(loc.path);
        return true;
    } catch (const std::exception& x) {
        if (why) *why = x.what();
        return false;
    }
}

// ---- the edition check (ADR 0008 decision 2)

namespace {

// The first `n` bytes of a resource of INSPACE.EXE; empty when the module or the resource is
// missing. The resource table rounds the length up to 16 bytes, so the record is a prefix.
std::vector<std::uint8_t> exe_record(std::uint16_t type, std::uint16_t id, std::size_t n,
                                     bool* found) {
    if (found) *found = false;
    try {
        if (!g.store) return {};
        const fmt::NeModule& m = g.store->module(assets::kMainProgram);
        const fmt::NeResource* r = m.find(type, id);
        if (!r) return {};
        if (found) *found = true;
        const std::size_t len = std::min(n, m.size(*r));
        return std::vector<std::uint8_t>(m.data(*r), m.data(*r) + len);
    } catch (const std::exception& x) {
        log("INSPACE.EXE #" + std::to_string(type) + " " + std::to_string(id) + ": " + x.what());
        return {};
    }
}

shell::EditionInputs edition_inputs() {
    shell::EditionInputs in;
    // ISPACE.DLL seg10:0x4f9c and seg10:0x4c68 read <LocalPath>INSPACE.EXE; the port reads the
    // asset directory's INSPACE.EXE, since LocalPath is the INI's directory (ADR 0004 decision
    // 5, ADR 0008 decision 2) [DESIGN]. Resource types 5500 and 5000 are decimal.
    in.install_record = exe_record(5500, 110, 10, nullptr);  // #5500 110
    in.name_record = exe_record(5000, 143, 50, &in.name_found);  // #5000 143
    settings::RegistrationSettings reg;
    if (g.ini) reg.load(*g.ini);
    if (reg.present) {  // ADR 0008 decision 1: the port's own copy
        in.vidsp = reg.vidsp;
        in.bit32sp = reg.bit32sp;
        in.pdev = reg.pdev;
    } else {
        // GetWindowsDirectory + "\SYSTEM.INI" [386Enh] and WIN.INI [windows] [BYTES
        // seg10:0x89d0 seg10:0x8a26 seg10:0x8a3f seg10:0x8a52]. Win32 returns a 32-bit value
        // that edition_check cuts to the original's 16-bit word [DESIGN].
        const std::wstring sys = (windows_directory() / L"SYSTEM.INI").wstring();
        in.vidsp = static_cast<int>(GetPrivateProfileIntW(L"386Enh", L"vidsp", 0, sys.c_str()));
        in.bit32sp = static_cast<int>(GetPrivateProfileIntW(L"386Enh", L"32BitSP", 0, sys.c_str()));
        in.pdev = static_cast<int>(GetProfileIntW(L"windows", L"PDev", 0));
    }
    // WIN.INI [Inner Space] CDDrive (40 bytes) [BYTES seg10:0x88db]: read, logged when set, and
    // treated as empty (ADR 0008 decision 4) [DESIGN].
    static bool cd_logged = false;
    wchar_t cd[40] = {};
    GetProfileStringW(L"Inner Space", L"CDDrive", L"", cd, 40);
    if (cd[0] != 0 && !cd_logged) {
        cd_logged = true;
        log_todo("WIN.INI [Inner Space] CDDrive is set and treated as empty (ADR 0008)",
                 "seg10:0x88db");
    }
    const std::time_t t = std::time(nullptr);
    if (const std::tm* tm = std::localtime(&t)) {
        in.tm_year = tm->tm_year;
        in.tm_yday = tm->tm_yday;
    }
    return in;
}

}  // namespace

const shell::Edition& edition() { return g.edition; }

const shell::Edition& run_edition_check(int arg) {
    if (g.edition.edition == -1) return g.edition;  // seg10:0x8874: returns at once
    const shell::Edition before = g.edition;
    int ret = 0;
    g.edition = shell::edition_check(g.edition, arg, edition_inputs(), &ret);
    const shell::Edition& e = g.edition;
    if (e.edition != before.edition || e.plus != before.plus || e.invalid != before.invalid ||
        e.day != before.day || before.checked == 0) {
        // The name stays out of the log; its length shows that it was read [DESIGN].
        log("edition check (" + std::to_string(arg) + "): 920c " + std::to_string(e.edition) +
            ", 11aa " + std::to_string(e.plus) + ", 922a " + std::to_string(e.invalid) +
            ", 9224 " + std::to_string(e.day) + ", command 2 = " +
            std::to_string(e.record_result) + ", name " + std::to_string(e.name.size()) +
            " bytes, returns " + std::to_string(ret));
    }
    return g.edition;
}

bool post(UINT msg, WPARAM wparam, LPARAM lparam) {
    return g.main && PostMessageW(g.main, msg, wparam, lparam);
}

void request_state(int state) { g.game.request(state); }

}  // namespace is::app
