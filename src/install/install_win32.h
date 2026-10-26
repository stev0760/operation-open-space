// The Win32 side of the install step (ADR 0008 decision 1): INSTALL.EXE's dialogs 300, 200,
// 1922, 1774, 135 and 100 from the set's own INSTALL.EXE resources, its message boxes, and the
// Open dialog that start-up offers when it finds no game. The logic is install.h's.
#pragma once

#ifdef _WIN32

#include <windows.h>

#include <filesystem>
#include <optional>

#include "install/install.h"

namespace is::install {

struct Win32Options {
    HINSTANCE instance = nullptr;
    std::filesystem::path source;                 // --install <source>
    std::optional<std::filesystem::path> to;      // --to <dir>
    std::filesystem::path ini_path;               // the port's INSPACE.INI (ADR 0004)
    std::filesystem::path work_root;              // where the set is copied (INI dir\INSTALL)
    bool open_readme = true;                      // dialog 100's OK runs notepad README.TXT
    Log log;
};

// Runs the step with the dialogs. The new game directory when the install completed, nullopt
// when it was cancelled or failed (the original's message was shown).
std::optional<std::filesystem::path> run_install_win32(const Win32Options& o);

// Start-up without a game [DESIGN]: asks whether to install, then lets the user choose the set's
// INSTALL.INI (GetOpenFileNameW, filter INSTALL.INI). False when the user declines or cancels.
bool choose_install_set(HWND owner, std::filesystem::path* ini);

}  // namespace is::install

#endif
