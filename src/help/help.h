// The manual (ADR 0005): the port's seg2:0x190a. The original calls
// WinHelp(main window, Path + Language + "_HELP.HLP", HELP_INDEX when ctx is 0 and
// HELP_CONTEXT otherwise, ctx) (docs/re/shell.md "Help"). Its callers:
// - the WM_COMMAND handler seg2:0x13b2: 0xc9 About Inner Space (ctx 0x1c, Credits), 0xca
//   User Manual (0, Contents), 0xcb Playing Inner Space (5, General), F1 while the
//   targeting display DGROUP:0xa7c is 0 (command 0xf0, ctx 0) and F2 (command 0x100, ctx 0)
//   [BYTES seg2:0x1552 seg2:0x1566 seg2:0x1574 seg2:0x157a];
// - the Navigation window's Help button 0x70 (ctx 5, General) [BYTES seg6:0x2379].
// Win32 only.
#pragma once

#include <cstdint>

namespace is::help {

// Exports the help file of the asset directory to <INI directory>\help\<Language>\ when its
// SHA-256 changed (src/help/export.h) and opens the page for `ctx` with
// ShellExecuteW("open"). A missing or unreadable help file, or a failed export, shows a
// message box instead (WinHelp showed its own) [DESIGN]. Returns true when the browser was
// started.
bool open_manual(std::uint32_t ctx);

}  // namespace is::help
