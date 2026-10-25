// The WM_COMMAND table of seg2:0x13b2 (docs/re/globals.md "WM_COMMAND handler") and the
// WM_SYSCOMMAND table of seg2:0xd78 (docs/re/shell.md "Mode switching"). With the original
// files present, every id of ENG_LANG.DLL's menus 101, 102, POPUPMENU and MAINACCEL is checked
// against the table.
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "shell/command_table.h"

using namespace is::shell;

namespace {

Action post(std::uint32_t msg, std::uint16_t w) {
    Action a;
    a.kind = ActionKind::kPost;
    a.msg = msg;
    a.wparam = w;
    return a;
}
Action call(Call c, int arg = 0) {
    Action a;
    a.kind = ActionKind::kCall;
    a.call = c;
    a.arg = arg;
    return a;
}

// globals.md's table, row by row.
struct Row {
    std::uint16_t id;
    Action want;
};
std::vector<Row> doc_rows() {
    return {
        {0x67, post(0x101, 0x1b)},           // Abort: WM_KEYUP VK_ESCAPE
        {0x68, call(Call::kOptions)},        // Options..
        {0xc0, call(Call::kOptions)},        // Game Options.., Alt+G
        {0x69, post(0x101, 0x7b)},           // Action Mode: WM_KEYUP VK_F12
        {0x6a, post(0x112, 0xF060)},         // Exit: SC_CLOSE
        {0xb0, post(0x112, 0xF060)},         // Exit Game, Alt+X
        {0x6b, call(Call::kSave)},           // Save..
        {0x1a6, call(Call::kSave)},          // Alt+S
        {0x6c, post(0x101, 'Q')}, {0x6d, post(0x101, 'A')}, {0x6e, post(0x101, 'D')},
        {0x6f, post(0x101, 'G')}, {0x70, post(0x101, 'S')}, {0x71, post(0x101, 'E')},
        {0x73, post(0x99e, 'F')},            // Challenge to Fight
        {0x74, post(0x99b, 'I')},            // Icons
        {0x1eb, post(0x99b, 'I')},           // Alt+I
        {0x75, post(0x101, 'W')},            // One Ship
        {0x1e1, post(0x99b, 'W')},           // Alt+W
        {0x76, post(0x99b, 'R')},            // All Ships
        {0x1d7, post(0x99b, 'R')},           // Alt+R
        {0x77, call(Call::kHelpTopic, 4)}, {0x78, call(Call::kHelpTopic, 5)},
        {0x79, call(Call::kHelpTopic, 6)}, {0x7a, call(Call::kHelpTopic, 1)},
        {0x7b, call(Call::kHelpTopic, 0)},
        {0xf0, call(Call::kF1)},             // F1
        {0x100, call(Call::kHelpFile, 0)},   // F2: seg2:0x190a(0, 0)
        {0x7c, post(0x99e, 'Q')}, {0x7d, post(0x99e, 'A')}, {0x7e, post(0x99e, 'D')},
        {0x7f, post(0x99e, 'G')}, {0x80, post(0x99e, 'S')}, {0x81, post(0x99e, 'E')},
        {0x82, post(0x99e, 'H')},
        {0xc9, call(Call::kHelpFile, 0x1c)}, {0xca, call(Call::kHelpFile, 0)},
        {0xcb, call(Call::kHelpFile, 5)},
        {0x191, post(0x99b, 0x6b)},          // Alt+=: VK_ADD
        {0x19b, post(0x99b, 0x6d)},          // Alt+-: VK_SUBTRACT
        {0x19c, call(Call::kBossKey)},       // Alt+B
        {0x1b9, post(0x99b, 'A')}, {0x1c3, post(0x99b, 'H')}, {0x1cd, post(0x99b, 'E')},
        {0xe0, post(0x8e2, 0)},
    };
}

}  // namespace

TEST_CASE("commands: every row of globals.md's WM_COMMAND table") {
    const auto rows = doc_rows();
    for (const Row& r : rows) {
        CAPTURE(r.id);
        CHECK(command_action(r.id) == r.want);
    }
    // and the table holds nothing else
    CHECK(command_table().size() == rows.size());
    std::set<std::uint16_t> ids;
    for (const auto& e : command_table()) {
        CHECK(ids.insert(e.id).second);
        CHECK(std::string(e.action.address).rfind("seg2:0x", 0) == 0);
    }
}

TEST_CASE("commands: ids without a case do nothing") {
    for (std::uint16_t id : {0, 0x66, 0x72, 0x83, 0xd0, 0xff, 0x1ec, 0xffff}) {
        CAPTURE(id);
        CHECK(command_action(id).kind == ActionKind::kNone);
    }
}

TEST_CASE("commands: Save's argument by state") {
    CHECK(save_argument(1) == 2);
    CHECK(save_argument(2) == 0);
    for (int s : {0, 3, 4, 5, 6, 7, 8, 9}) CHECK(save_argument(s) == -1);
}

TEST_CASE("syscommands: seg2:0xd78's table") {
    struct R {
        std::uint32_t w;
        SysAction a;
        bool def;
    };
    for (const R& r : {R{0x60, SysAction::kToggleMenu, false}, R{0xc0, SysAction::kGameOptions, false},
                       R{0xF020, SysAction::kPause, true}, R{0xF030, SysAction::kFullScreen, true},
                       R{0xF120, SysAction::kRestore, true}, R{0xF060, SysAction::kClose, false},
                       R{0xF100, SysAction::kDefault, true}, R{0xF010, SysAction::kDefault, true}}) {
        CAPTURE(r.w);
        const SysEntry e = sys_command(r.w);
        CHECK(e.action == r.a);
        CHECK(e.def_window_proc == r.def);
    }
    // the low four bits are masked off (the mouse-drag forms of the codes)
    CHECK(sys_command(0xF062).action == SysAction::kClose);
    CHECK(sys_command(0xF032).action == SysAction::kFullScreen);
    CHECK(sys_command(0x6f).action == SysAction::kToggleMenu);
}

TEST_CASE("commands: every id of ENG_LANG.DLL's menus and MAINACCEL has a case") {
    const char* dir = std::getenv("IS_ORIGINAL_DIR");
    if (!dir || !*dir || !std::filesystem::is_directory(dir)) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    is::assets::AssetStore store{std::filesystem::path(dir)};
    std::set<std::uint16_t> ids;
    const auto walk = [&](const auto& self, const std::vector<is::fmt::MenuItem16>& items) -> void {
        for (const auto& m : items) {
            if (m.flags & is::fmt::kMfPopup)
                self(self, m.children);
            else if (m.id != 0)
                ids.insert(m.id);
        }
    };
    walk(walk, store.menu(101));
    walk(walk, store.menu(102));
    walk(walk, store.menu("POPUPMENU"));
    for (const auto& a : store.accelerators("MAINACCEL")) ids.insert(a.cmd);
    CHECK(ids.size() == 45);
    for (std::uint16_t id : ids) {
        CAPTURE(id);
        // POPUPMENU's 208 "Options Menu" is the popup's title line; seg2:0x13b2 has no case
        // for it [BYTES seg2:0x15a0-0x1696].
        if (id == 208)
            CHECK(command_action(id).kind == ActionKind::kNone);
        else
            CHECK(command_action(id).kind != ActionKind::kNone);
    }
}
