#include "shell/options.h"

#include <exception>

#include "assets/asset_store.h"
#include "assets/game_files.h"
#include "formats/bytes.h"

namespace is::shell {

namespace {
bool same(const std::filesystem::path& a, const char* b) {
    return a.native() == std::filesystem::path(b).native();
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
        *slot = args[++i];
    }
    if (!r.options.ini) r.error = "--ini <path> is required";
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
