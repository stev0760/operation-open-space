// The main window's command tables as data:
// - the WM_COMMAND handler seg2:0x13b2 (docs/re/globals.md "WM_COMMAND handler"), which turns
//   the ids of ENG_LANG.DLL's menus 101, 102, POPUPMENU and accelerator table MAINACCEL into
//   posted key events, private messages, WM_SYSCOMMAND codes or direct calls;
// - the WM_SYSCOMMAND dispatcher seg2:0xd78 (docs/re/shell.md "Mode switching").
// The window procedure executes the Action. Portable.
#pragma once

#include <cstdint>
#include <vector>

namespace is::shell {

// Message numbers the handler posts [BYTES seg2:0x13c9 seg2:0x13f4 seg2:0x1434 seg2:0x14ee
// seg2:0x14c2]. lParam is always 0 [BYTES seg2:0x13ce seg2:0x13d0].
constexpr std::uint32_t kMsgKeyUp = 0x101;      // WM_KEYUP
constexpr std::uint32_t kMsgSysCommand = 0x112; // WM_SYSCOMMAND
constexpr std::uint32_t kMsgAltKey = 0x99b;     // a key with Alt held (timing.md "Speed keys")
constexpr std::uint32_t kMsgCtrlKey = 0x99e;    // a key with Ctrl held
constexpr std::uint32_t kMsgUnhandled = 0x8e2;  // reaches DefWindowProc (shell.md step 24)

// System command codes (wParam & 0xFFF0) [BYTES seg2:0xd88].
constexpr std::uint16_t kScMainMenus = 0x60;   // "&Main Menus", string 2184
constexpr std::uint16_t kScGameOptions = 0xc0;
constexpr std::uint16_t kScMinimize = 0xF020;
constexpr std::uint16_t kScMaximize = 0xF030;
constexpr std::uint16_t kScClose = 0xF060;
constexpr std::uint16_t kScRestore = 0xF120;

// The routines the handler calls directly.
enum class Call : std::uint8_t {
    kOptions,    // seg10:0x7f18, the Options dialog
    kSave,       // seg10:0x8568(2) in state 1, seg10:0x8568(0) in state 2, nothing otherwise
    kHelpTopic,  // DGROUP:0x93c6 = arg, then seg2:0x88e (the IS_HELP window)
    kF1,         // as kHelpTopic 4 while DGROUP:0xa7c != 0, otherwise seg2:0x190a(0, 0)
    kHelpFile,   // seg2:0x190a(arg, 0), WinHelp
    kBossKey,    // the WS_OVERLAPPEDWINDOW bits while Full, then posts SC_MINIMIZE
};

enum class ActionKind : std::uint8_t {
    kNone,  // no case in the handler: nothing happens
    kPost,  // PostMessage(main, msg, wparam, 0)
    kCall,  // a direct call
};

struct Action {
    ActionKind kind = ActionKind::kNone;
    std::uint32_t msg = 0;     // kPost
    std::uint16_t wparam = 0;  // kPost
    Call call = Call::kOptions;  // kCall
    int arg = 0;               // kCall: the help topic or the seg2:0x190a argument
    const char* address = "";  // where the handler's case is

    bool operator==(const Action& o) const {
        return kind == o.kind && msg == o.msg && wparam == o.wparam &&
               (kind != ActionKind::kCall || (call == o.call && arg == o.arg));
    }
};

struct CommandEntry {
    std::uint16_t id;
    Action action;
};

// Every id with a case in seg2:0x13b2, in id order.
const std::vector<CommandEntry>& command_table();
// The action of a WM_COMMAND id (LOWORD of wParam); ActionKind::kNone for an id without a
// case, such as POPUPMENU's 208 "Options Menu" and the Strategy gap 0x72.
Action command_action(std::uint16_t id);
// seg10:0x8568's argument for Call::kSave in `state`, or -1 when the handler calls nothing
// [BYTES seg2:0x14f7-0x1512].
int save_argument(int state);

enum class SysAction : std::uint8_t {
    kToggleMenu,   // 0x60: flips Menu and attaches or removes the menu bar
    kGameOptions,  // 0xc0: posts WM_COMMAND 0xc0
    kPause,        // SC_MINIMIZE: seg2:0x9c4
    kFullScreen,   // SC_MAXIMIZE: seg2:0xfc4 when DGROUP:0x1e is set
    kRestore,      // SC_RESTORE: the windowed rectangle, then full screen when Full
    kClose,        // SC_CLOSE: the exit sequence
    kDefault,      // any other code: none
};

struct SysEntry {
    std::uint16_t code;
    SysAction action;
    // seg2:0xd78 returns non-zero, so WndProc also calls DefWindowProc [BYTES seg2:0x63f].
    // For 0x60 the original returns what DrawMenuBar leaves in AX; DefWindowProc ignores the
    // code either way, so the port calls it not at all [DESIGN].
    bool def_window_proc;
    const char* address;
};

const std::vector<SysEntry>& sys_command_table();
// The entry for a WM_SYSCOMMAND wParam (masked with 0xFFF0); kDefault with DefWindowProc for
// an unlisted code.
SysEntry sys_command(std::uint32_t wparam);

}  // namespace is::shell
