// The game-start session of INSPACE.EXE around ISPACE ordinal 10 (docs/re/shell.md "Game-start
// dialogs", "From ESC to the wave" and "Exit"): the wrapper seg10:0x82a2 and its result tail,
// the Save & Exit step seg10:0x8568, the SC_CLOSE exit sequence of seg2:0xd78, and the DGROUP
// words they keep until M3 gives the game state a home. src/dialogs/hooks.cpp installs it into
// the program (install_dialog_hooks). Win32 only.
#pragma once

#ifdef _WIN32
#include <array>
#include <cstdint>
#include <string>

namespace is::dialogs {

// The INSPACE.EXE words the game-start flow writes [DESIGN: kept here until M3].
struct Session {
    int wave = 0;            // DGROUP:0x91f8: the wave of a restored game, 0 for a new one
    int restored = 0;        // DGROUP:0x92e4: seg6:0x930a restored a game
    std::uint32_t game_id = 0;     // DGROUP:0x939a: seg10:0x804c(1), time()
    std::uint32_t game_date = 0;   // DGROUP:0x939e: seg10:0x804c(0), the packed local date
    int drive = 0;           // DGROUP:0x94ac: INI Drive - 'C'
    std::string pilot;       // DGROUP:0x380c: INI ThePlayer after command 1
    int demo_restore = 0;    // DGROUP:0x9516
    int player = 1;          // DGROUP:0x95bc outside state 0 (the intro keeps it in state 0)
    int size = 1;            // DGROUP:0x9156: INI Size as the program applied it
    int order_count = 0;     // DGROUP:0x921e: Order dialogs this run
    std::uint32_t key_hold = 0;  // DGROUP:0x3b94: keys are dropped until then
    bool keep_once = false;  // the kept ESC passes the hold once
    bool table_loaded = false;
    // DGROUP:0x3824, the 8 x 8 word table of ISPACE.DLL #5600 135 (seg10:0x38e0).
    std::array<std::array<std::int16_t, 8>, 8> table{};
};
Session& session();

// seg10:0x82a2(cmd): ordinal 10 with its arguments and result tail; returns the result.
int dialog(int cmd);
// seg10:0x8568(arg): the Save Game step of Save & Exit, Save.. and the abort dialog.
int save_step(int arg);
// seg2:0xd78's SC_CLOSE: the exit sequence of shell.md "Exit".
void exit_sequence();
// The private message 0x991, abort to the intro (shell.md "Exit", **Abort to the intro**).
void abort_sequence();

}  // namespace is::dialogs
#endif
