// The port's command line and the choice of the asset directory. Portable.
//
//   inspace.exe [--game <dir>] --ini <path>
//
// --game names the asset directory; the directory of the executable wins when it holds the
// game's modules (ADR 0002, decision 1). --ini names INSPACE.INI; it is required until
// ADR 0004 (INI location) is accepted [DESIGN]. The original's own command line, DEMO or
// PLAY compared with the whole upper-cased line (shell.md "Start-up" step 20), is not read
// yet: TODO stage 3 with the DEMO and PLAY handlers 0x9f0 and 0x9fa.
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
};

struct OptionsResult {
    Options options;
    std::string error;  // empty when the line parsed; otherwise a UTF-8 message
};

// Parses the arguments after the program name. Each option takes the next argument as its
// value; an option given twice, an unknown argument or a missing value is an error, and so is
// a missing --ini.
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
