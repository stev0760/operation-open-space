#include "shell/command_table.h"

#include <algorithm>

namespace is::shell {

namespace {

constexpr std::uint16_t kVkEscape = 0x1b, kVkF12 = 0x7b, kVkAdd = 0x6b, kVkSubtract = 0x6d;

Action post(std::uint32_t msg, std::uint16_t wparam, const char* address) {
    Action a;
    a.kind = ActionKind::kPost;
    a.msg = msg;
    a.wparam = wparam;
    a.address = address;
    return a;
}

Action call(Call c, int arg, const char* address) {
    Action a;
    a.kind = ActionKind::kCall;
    a.call = c;
    a.arg = arg;
    a.address = address;
    return a;
}

std::vector<CommandEntry> build() {
    std::vector<CommandEntry> t = {
        {0x67, post(kMsgKeyUp, kVkEscape, "seg2:0x14d2")},  // Abort
        {0x68, call(Call::kOptions, 0, "seg2:0x14ca")},     // Options..
        {0x69, post(kMsgKeyUp, kVkF12, "seg2:0x14de")},     // Action Mode
        {0x6a, post(kMsgSysCommand, kScClose, "seg2:0x14ea")},  // Exit
        {0x6b, call(Call::kSave, 0, "seg2:0x14f7")},        // Save..
        // Strategy (menu 102): Capture, Attack, Defend, Refuel, Self-Preserve, Exit Wave
        {0x6c, post(kMsgKeyUp, 'Q', "seg2:0x1516")},
        {0x6d, post(kMsgKeyUp, 'A', "seg2:0x1520")},
        {0x6e, post(kMsgKeyUp, 'D', "seg2:0x152a")},
        {0x6f, post(kMsgKeyUp, 'G', "seg2:0x1534")},
        {0x70, post(kMsgKeyUp, 'S', "seg2:0x153e")},
        {0x71, post(kMsgKeyUp, 'E', "seg2:0x1548")},
        {0x73, post(kMsgCtrlKey, 'F', "seg2:0x147a")},  // Challenge to Fight
        {0x74, post(kMsgAltKey, 'I', "seg2:0x13c5")},   // Watch: Icons
        {0x75, post(kMsgKeyUp, 'W', "seg2:0x13f0")},    // Watch: One Ship
        {0x76, post(kMsgAltKey, 'R', "seg2:0x13fb")},   // Watch: All Ships
        // Help: General, Scores, This Game, Target, Weapons
        {0x77, call(Call::kHelpTopic, 4, "seg2:0x1559")},
        {0x78, call(Call::kHelpTopic, 5, "seg2:0x1580")},
        {0x79, call(Call::kHelpTopic, 6, "seg2:0x1588")},
        {0x7a, call(Call::kHelpTopic, 1, "seg2:0x1590")},
        {0x7b, call(Call::kHelpTopic, 0, "seg2:0x1598")},
        // Ship Cmds: Capture, Attack, Defend, Refuel, Self-Preserve, Exit Wave, Help Me
        {0x7c, post(kMsgCtrlKey, 'Q', "seg2:0x1430")},
        {0x7d, post(kMsgCtrlKey, 'A', "seg2:0x143b")},
        {0x7e, post(kMsgCtrlKey, 'D', "seg2:0x1444")},
        {0x7f, post(kMsgCtrlKey, 'G', "seg2:0x1450")},
        {0x80, post(kMsgCtrlKey, 'S', "seg2:0x145c")},
        {0x81, post(kMsgCtrlKey, 'E', "seg2:0x1468")},
        {0x82, post(kMsgCtrlKey, 'H', "seg2:0x1471")},
        {0xb0, post(kMsgSysCommand, kScClose, "seg2:0x14ea")},  // Exit Game, Alt+X
        {0xc0, call(Call::kOptions, 0, "seg2:0x14ca")},         // Game Options.., Alt+G
        // Help: About Inner Space, User Manual, Playing Inner Space
        {0xc9, call(Call::kHelpFile, 0x1c, "seg2:0x1574")},
        {0xca, call(Call::kHelpFile, 0, "seg2:0x1566")},
        {0xcb, call(Call::kHelpFile, 5, "seg2:0x157a")},
        {0xe0, post(kMsgUnhandled, 0, "seg2:0x14be")},  // no menu or key in ENG_LANG.DLL
        {0xf0, call(Call::kF1, 0, "seg2:0x1552")},      // F1
        {0x100, call(Call::kHelpFile, 0, "seg2:0x1566")},  // F2
        {0x191, post(kMsgAltKey, kVkAdd, "seg2:0x13da")},       // Alt+=, Alt+keypad +
        {0x19b, post(kMsgAltKey, kVkSubtract, "seg2:0x13e5")},  // Alt+-, Alt+keypad -
        {0x19c, call(Call::kBossKey, 0, "seg2:0x1486")},        // Alt+B
        {0x1a6, call(Call::kSave, 0, "seg2:0x14f7")},           // Alt+S
        // Defenders: Attack, Help Me, Exit Wave; Alt+A, Alt+H, Alt+E
        {0x1b9, post(kMsgAltKey, 'A', "seg2:0x1406")},
        {0x1c3, post(kMsgAltKey, 'H', "seg2:0x1411")},
        {0x1cd, post(kMsgAltKey, 'E', "seg2:0x141c")},
        {0x1d7, post(kMsgAltKey, 'R', "seg2:0x13fb")},  // Alt+R
        {0x1e1, post(kMsgAltKey, 'W', "seg2:0x1427")},  // Alt+W
        {0x1eb, post(kMsgAltKey, 'I', "seg2:0x13c5")},  // Alt+I
    };
    return t;
}

}  // namespace

const std::vector<CommandEntry>& command_table() {
    static const std::vector<CommandEntry> t = build();
    return t;
}

Action command_action(std::uint16_t id) {
    const auto& t = command_table();
    const auto it = std::lower_bound(t.begin(), t.end(), id,
                                     [](const CommandEntry& e, std::uint16_t v) { return e.id < v; });
    if (it == t.end() || it->id != id) return Action{};
    return it->action;
}

int save_argument(int state) {
    if (state == 1) return 2;
    if (state == 2) return 0;
    return -1;
}

const std::vector<SysEntry>& sys_command_table() {
    static const std::vector<SysEntry> t = {
        {kScMainMenus, SysAction::kToggleMenu, false, "seg2:0xf4d"},
        {kScGameOptions, SysAction::kGameOptions, false, "seg2:0xfa7"},
        {kScMinimize, SysAction::kPause, true, "seg2:0xe70"},
        {kScMaximize, SysAction::kFullScreen, true, "seg2:0xe7a"},
        {kScClose, SysAction::kClose, false, "seg2:0xdb8"},
        {kScRestore, SysAction::kRestore, true, "seg2:0xe87"},
    };
    return t;
}

SysEntry sys_command(std::uint32_t wparam) {
    const auto code = static_cast<std::uint16_t>(wparam & 0xFFF0u);  // seg2:0xd88
    for (const SysEntry& e : sys_command_table())
        if (e.code == code) return e;
    return {code, SysAction::kDefault, true, "seg2:0xe74"};
}

}  // namespace is::shell
