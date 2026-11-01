#include "shell/options.h"

#include <exception>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "assets/asset_store.h"
#include "assets/game_files.h"
#include "formats/bytes.h"

namespace is::shell {

namespace {
bool same(const std::filesystem::path& a, const char* b) {
    return a.native() == std::filesystem::path(b).native();
}

// An option's path in the form the INI keeps (ADR 0004 decision 4 writes it as `Path`,
// `SndPath` and the rest): a full DOS path with its drive. A relative or drive-less argument
// is completed by GetFullPathNameW; under Wine a Unix path (`/home/...`) goes through
// kernel32's `wine_get_dos_file_name`, which maps it to its drive (`Z:\home\...`), where
// GetFullPathNameW would put it on the current drive and the sound and music files would not
// open [DESIGN] [REF Wine dlls/kernel32/path.c wine_get_dos_file_name]. Host builds keep the
// argument as given.
std::filesystem::path full_path(const std::filesystem::path& p) {
#ifdef _WIN32
    const std::wstring w = p.wstring();
    if (w.empty()) return p;
    if (w[0] == L'/') {
        using Fn = LPWSTR(CDECL*)(LPCSTR);
        if (HMODULE k = GetModuleHandleW(L"kernel32.dll"))
            if (FARPROC raw = GetProcAddress(k, "wine_get_dos_file_name")) {
                const Fn fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(raw));
                const std::string u8 = fmt::path_utf8(p);
                if (LPWSTR dos = fn(u8.c_str())) {
                    std::filesystem::path r(dos);
                    HeapFree(GetProcessHeap(), 0, dos);
                    return r;
                }
            }
        // No drive maps the path (a prefix without `Z:`): keep it as given, which Wine's
        // file functions accept, rather than put it on the current drive.
        return p;
    }
    wchar_t buf[MAX_PATH * 4];
    const DWORD n = GetFullPathNameW(w.c_str(), MAX_PATH * 4, buf, nullptr);
    if (n == 0 || n >= MAX_PATH * 4) return p;
    return std::filesystem::path(buf);
#else
    return p;
#endif
}
}  // namespace

OptionsResult parse_options(const std::vector<std::filesystem::path>& args) {
    OptionsResult r;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        std::optional<std::filesystem::path>* slot = nullptr;
        const char* name = nullptr;
        if (same(a, "--game")) {
            slot = &r.options.game_dir;
            name = "--game";
        } else if (same(a, "--ini")) {
            slot = &r.options.ini;
            name = "--ini";
        } else if (same(a, "--install")) {
            slot = &r.options.install;
            name = "--install";
        } else if (same(a, "--to")) {
            slot = &r.options.to;
            name = "--to";
        } else {
            r.error = "unknown argument: " + fmt::path_utf8(a);
            return r;
        }
        if (i + 1 >= args.size()) {
            r.error = std::string(name) + " needs a value";
            return r;
        }
        if (slot->has_value()) {
            r.error = std::string(name) + " given twice";
            return r;
        }
        *slot = full_path(args[++i]);
    }
    if (r.options.to && !r.options.install) r.error = "--to needs --install";
    else if (r.options.install && r.options.game_dir) r.error = "--install and --game exclude each other";
    return r;
}

bool holds_game_modules(const assets::GameFiles& files) {
    return files.has(assets::kMainProgram) && files.has(assets::kGameDll) &&
           files.has(assets::kGraphicsDll);
}

std::optional<std::filesystem::path> choose_asset_dir(
    const std::filesystem::path& exe_dir, const std::optional<std::filesystem::path>& named,
    std::string* why) {
    std::string tried;
    const auto check = [&](const std::filesystem::path& dir) {
        try {
            if (holds_game_modules(assets::GameFiles(dir))) return true;
            tried += fmt::path_utf8(dir) + ": no INSPACE.EXE, ISPACE.DLL and IGRAPHIC.DLL\n";
        } catch (const std::exception& x) {
            tried += fmt::path_utf8(dir) + ": " + x.what() + "\n";
        }
        return false;
    };
    if (!exe_dir.empty() && check(exe_dir)) return exe_dir;
    if (named && check(*named)) return *named;
    if (why) *why = tried;
    return std::nullopt;
}

}  // namespace is::shell
