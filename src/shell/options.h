// The port's command line and the choice of the asset directory. Portable.
//
//   inspace.exe [--game <dir>] [--ini <path>]
//   inspace.exe --install <source> [--to <dir>] [--ini <path>]
//
// --game names the asset directory; the directory of the executable wins when it holds the
// game's modules (ADR 0002, decision 1). --ini names INSPACE.INI and wins over ADR 0004's
// other locations (decision 2) [DESIGN]. The original's own command line, DEMO or
// PLAY compared with the whole upper-cased line (shell.md "Start-up" step 20), is not read
// yet: TODO stage 3 with the DEMO and PLAY handlers 0x9f0 and 0x9fa.
// --install runs the port's install step (ADR 0008 decision 1, src/install) from the install
// set <source> (a directory with INSTALL.INI, or that file) and exits; --to gives the
// directory the components dialog proposes (the original proposes C:\SPACE\) [DESIGN].
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace is::assets {
class GameFiles;
}

namespace is::shell {

struct Options {
    std::optional<std::filesystem::path> game_dir;
    std::optional<std::filesystem::path> ini;
    std::optional<std::filesystem::path> install;  // --install <source>
    std::optional<std::filesystem::path> to;       // --to <dir>, only with --install
};

struct OptionsResult {
    Options options;
    std::string error;  // empty when the line parsed; otherwise a UTF-8 message
};

// Parses the arguments after the program name. Each option takes the next argument as its
// value; an option given twice, an unknown argument or a missing value is an error. Every
// option is optional; --to needs --install, and --install excludes --game.
OptionsResult parse_options(const std::vector<std::filesystem::path>& args);

// The files the installer puts in the game directory, by which ADR 0002 decision 1 recognises
// one: INSPACE.EXE, ISPACE.DLL and IGRAPHIC.DLL.
bool holds_game_modules(const assets::GameFiles& files);

// ADR 0002 decision 1: the executable's directory when it holds the game's modules, else the
// named directory. nullopt when neither does (an unreadable directory counts as not holding
// them); `why` then says what was tried, in UTF-8.
std::optional<std::filesystem::path> choose_asset_dir(
    const std::filesystem::path& exe_dir, const std::optional<std::filesystem::path>& named,
    std::string* why = nullptr);

}  // namespace is::shell
