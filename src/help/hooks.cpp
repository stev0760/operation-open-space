// The Win32 side of the manual (ADR 0005): the help file from the asset store, the export
// directory under the INI's directory (ADR 0004), and ShellExecuteW. The portable export is
// src/help/export.cpp. Win32 only: the host build compiles this file empty.
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "app/app.h"
#include "assets/game_files.h"
#include "formats/bytes.h"
#include "help/export.h"
#include "help/help.h"

namespace is::help {

namespace fs = std::filesystem;
using app::g;
using app::log;

namespace {

// <INI directory>, or %APPDATA%\Inner Space when the INI's location is unknown (ADR 0004
// decision 2).
fs::path base_directory() {
    if (!g.ini_path.empty()) return g.ini_path.parent_path();
    if (!g.local_path.empty()) return g.local_path;
    PWSTR p = nullptr;
    fs::path r;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &p)) && p)
        r = fs::path(p) / L"Inner Space";
    if (p) CoTaskMemFree(p);
    return r;
}

void fail(const std::string& why) {
    log("help: " + why);
    // WinHelp reported a missing file in its own message box; the port's text [DESIGN].
    std::wstring text(static_cast<std::size_t>(why.size()), L'\0');
    text.resize(static_cast<std::size_t>(MultiByteToWideChar(
        CP_UTF8, 0, why.data(), static_cast<int>(why.size()), text.data(), static_cast<int>(text.size()))));
    MessageBoxW(g.main, (L"The manual cannot be opened.\n\n" + text).c_str(), L"Inner Space",
                MB_OK | MB_ICONEXCLAMATION);
}

}  // namespace

bool open_manual(std::uint32_t ctx) {
    char head[96];
    std::snprintf(head, sizeof head, "help: seg2:0x190a(%u): %s", static_cast<unsigned>(ctx),
                  ctx ? "HELP_CONTEXT" : "HELP_INDEX");
    log(head);
    if (!g.store) {
        fail("no game directory");
        return false;
    }
    // Path + Language + "_HELP.HLP" [BYTES seg2:0x1927 seg2:0x1934 seg2:0x194c]; the asset
    // store expands the distribution's Eng_help.hl1 (ADR 0002).
    const std::string name = g.store->language() + "_HELP.HLP";
    std::vector<std::uint8_t> hlp;
    try {
        hlp = g.store->files().read(name);
    } catch (const std::exception& e) {
        fail(name + ": " + e.what());
        return false;
    }
    const fs::path base = base_directory();
    if (base.empty()) {
        fail("no directory for the exported manual");
        return false;
    }
    const fs::path dir = (base / L"help" / assets::path_from_cp1252(g.store->language())).make_preferred();
    const ExportResult r = ensure_export(hlp, dir, ctx);
    if (!r.ok) {
        fail(name + ": " + r.error);
        return false;
    }
    if (r.regenerated)
        log("help: exported " + name + " to " + fmt::path_utf8(dir) + " (" + std::to_string(r.files_written) +
            " files written, " + std::to_string(r.files_removed) + " removed)");
    else
        log("help: the export in " + fmt::path_utf8(dir) + " is current");
    const fs::path page = fs::path(r.page).make_preferred();
    // ShellExecuteW returns a value greater than 32 on success.
    auto rc = reinterpret_cast<INT_PTR>(
        ShellExecuteW(g.main, L"open", page.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    log("help: ShellExecuteW(\"open\", " + fmt::path_utf8(page) + ") = " + std::to_string(rc));
    if (rc == SE_ERR_NOASSOC) {
        // No program is registered for .html (a Wine prefix made without mshtml lacks the
        // mapping): the page opens through the htmlfile class instead [DESIGN].
        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof info;
        info.fMask = SEE_MASK_CLASSNAME;
        info.hwnd = g.main;
        info.lpVerb = L"open";
        info.lpFile = page.c_str();
        info.lpClass = L"htmlfile";
        info.nShow = SW_SHOWNORMAL;
        const BOOL ok = ShellExecuteExW(&info);
        rc = reinterpret_cast<INT_PTR>(info.hInstApp);
        log("help: ShellExecuteExW(\"open\", class htmlfile) = " + std::to_string(rc));
        if (ok) rc = 33;
    }
    if (rc <= 32) {
        fail("no program opens " + fmt::path_utf8(page) + " (ShellExecuteW " + std::to_string(rc) + ")");
        return false;
    }
    return true;
}

}  // namespace is::help

namespace is::app {

// The help commands reach is::help::open_manual from the window procedure's WM_COMMAND
// cases (Call::kHelpFile, Call::kF1); nothing else needs installing.
void install_help_hooks(App&) {}

}  // namespace is::app
#endif
