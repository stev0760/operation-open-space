// The Navigation window IS_NAV (docs/re/shell.md "Game-start dialogs", items 11 and 12;
// docs/re/nav.md): seg6:0x1c72 opens it, its procedure seg6:0x1f22 handles the messages,
// seg6:0x23d4 builds the children and fills the tree (seg6:0x37ca) and the selection
// (seg6:0x5318, seg6:0x5524), the owner-draw routines seg6:0x47e8 and seg6:0x49c0 draw the rows,
// seg6:0x5b4c the status line and the preview, seg6:0x2ebc closes it and writes the Nav keys,
// and seg6:0x301a is Enter. The portable rules are nav_rules.h, nav_draw.h, tree.h, files.h and
// fs.h. The race and duel boxes, Enter's checks and values and the demo pilot plug in through
// Parts. Win32 only.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "nav/fs.h"
#include "nav/nav_draw.h"
#include "nav/tree.h"

namespace is::settings {
class IniFile;
}
namespace is::assets {
class AssetStore;
}

namespace is::nav {

// What the window needs from the program.
struct Host {
    HINSTANCE instance = nullptr;
    HWND main = nullptr;  // DGROUP:0x16, the parent
    HICON icon = nullptr;  // the class icon (GROUP_ICON GAME)
    HFONT font = nullptr;  // DGROUP:0x90fe, the children's font
    settings::IniFile* ini = nullptr;
    std::function<void()> ini_changed;
    std::function<std::string(int)> lang;  // ENG_LANG.DLL, cp1252
    std::function<void(int)> sound;
    std::function<void(const std::string&, const char*)> todo;
    std::function<void(const std::string&)> log;
    std::function<int()> state;  // DGROUP:0x90f4
    // PatBlt(DGROUP:0x13dc, 0, 0, W, H, BLACKNESS) and the present [BYTES seg6:0x1d4a].
    std::function<void()> black_play_area;
    // DGROUP:0x91f8 and DGROUP:0x92e4 for Enter's rule.
    std::function<int()> wave;
    std::function<int()> restored;
    // DGROUP:0x9134 (PlayMode) and DGROUP:0x95bc (player game), for the demo pilot.
    std::function<int()> play_mode;
    std::function<int()> player;
    // A state request: DGROUP:0x13de = DGROUP:0x968e = s [BYTES seg6:0x369a seg6:0x36a0].
    std::function<void(int)> request_state;
    // DGROUP:0x1e74 = 0 and DGROUP:0x1e70 = now [BYTES seg6:0x36a5 seg6:0x36b2].
    std::function<void()> arm_debriefing;
    // Enter's accepted directory, for DGROUP:0x38c8 ("D:\GAMES\": the path in on-disk case with
    // its trailing backslash, or a built-in level's name).
    std::function<void(const std::string&)> entered;
    // seg2:0x190a(5, 0), the Help button's WinHelp call [BYTES seg6:0x2305].
    std::function<void()> help;
    // DGROUP:0x49e: the teardown has run.
    std::function<bool()> torn_down;
    // seg10:0x7db8: ISPACE ordinal 7 command 2, the purge of the deferred sound slots
    // [BYTES seg10:0x7dc7 seg10:0x7dc9] (M7 s3 fixer).
    std::function<void()> reset_sounds;

    // ---- M3 stage 1: the tree, the file pane and the preview
    // DGROUP:0x920c, the edition flag (shell.md "Edition check"); 0 when unset.
    std::function<int()> edition;
    // DGROUP:0x90f8 (a DEMO or PLAY command line) and DemoType DGROUP:0x95ba (default 519), for
    // the initial selection [BYTES seg6:0x3c43 seg6:0x3c4c].
    std::function<bool()> command_line;
    std::function<int()> demo_type;
    // The asset store: IGRAPHIC.DLL BITMAP FLDR and FILES, ISPACE.DLL GROUP_ICON 105-107. Without
    // it the rows are drawn without their glyphs and the built-in previews stay black.
    std::function<const assets::AssetStore*()> store;
    // The filesystem the walk reads; null: Win32Fs [DESIGN ADR 0012 decision 1]. Tests pass a MemFs.
    std::shared_ptr<Fs> fs;
    // The port's ExtractIcon reader for a DOS path; null: fmt::IconSource::load of the native
    // path Win32Fs gives (no icons with another Fs) [DESIGN ADR 0012 decision 5].
    IconReader::Load read_module;
    // The directory records (DGROUP:0x69f4, count DGROUP:0x94a4); null: the window's own list.
    Records* records = nullptr;
};

// Registers the class IS_NAV with the procedure (CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, the
// GAME icon, IDC_ARROW, LTGRAY_BRUSH: start-up step 12's class), replacing the placeholder
// registration of src/app/main.cpp [DESIGN].
bool install(const Host& host);
// The inputs install() logs as unset: lang, edition, player, store, command_line, demo_type,
// wave, restored and entered (M3 s1 fixer).
std::vector<std::string> unset_inputs(const Host& host);

// The main window and its font, once they exist (WM_CREATE comes after install()).
void attach(HWND main, HFONT font);

// seg6:0x1c72(arg) with the drive offset DGROUP:0x94ac.
void open(int arg, int drive_offset);
// DGROUP:0xd1e: the window, or null.
HWND window();
// seg6:0x2ebc: writes the Nav keys from DGROUP:0x6af0, frees the glyphs and the preview and
// destroys the window.
void close();
// seg6:0x301a: 1 when it accepted Enter (and closed the window), 0 otherwise. Without
// Parts::enter it accepts an ordinary directory only (step 7: a *.exe and a state below 2).
int enter();

// ---- the parts implementer D plugs in (nav.md "Race and duel boxes", "Enter", "Demo pilot")
struct Parts {
    // seg6:0x23d4 after listbox 0x6b: the RACE and DUEL group boxes 0x8c and 0x93 and the
    // combos 0x8d-0x92 and 0x94, in a client of cw x ch, with the font DGROUP:0x90fe.
    std::function<void(HWND nav, int cw, int ch, HFONT font)> create_boxes;
    // WM_SIZE for the same controls [BYTES seg6:0x1f22].
    std::function<void(HWND nav, int cw, int ch)> layout_boxes;
    // seg6:0x2dba(kind): 0 neither box, 1 RACE, 2 DUEL [BYTES seg6:0x2dba].
    std::function<void(HWND nav, int kind)> show_box;
    // seg6:0x301a after its DGROUP:0xd20 and DGROUP:0x90f4 tests: 1 when it accepted the row
    // (by calling accept()), 0 when it refused it (the window then expands or collapses it).
    std::function<int()> enter;
    // seg6:0x1e68(arg), the demo pilot; open() calls it with 1 when DGROUP:0x9134 != 0 and
    // DGROUP:0x95bc = 0 [BYTES seg6:0x1d6c seg6:0x1d82].
    std::function<void(int arg)> demo_pilot;
    // The end of seg6:0x37ca: the initial selection, with DGROUP:0x94e, DGROUP:0x950 and
    // DGROUP:0x94be of a DEMO or PLAY command line [BYTES seg6:0x3c43-0x3d8e].
    std::function<void(const InitialSelection&)> initial;
};
void set_parts(const Parts& parts);

// ---- the window's state for the parts (valid while window() is non-null unless noted)
int selected();              // DGROUP:0x94b4
int row_count();             // DGROUP:0x94b6
std::string& directory();    // DGROUP:0x6990, kept across openings
int drive_offset();          // DGROUP:0x94ac as open() got it
bool armed();                // DGROUP:0xd20
Tree* tree();                // the rows of listbox 0x65
Walker* walker();            // every find goes through it (and is logged)
Records& records();          // the directory records
Strings& strings();          // ENG_LANG.DLL through Host::lang
Game game();                 // DGROUP:0x920c, DGROUP:0x95bc, INI `Tree`
const Selected& selection(); // the last seg6:0x5524 result
// seg6:0x3f4c(path) with the window's game, records and strings [BYTES seg6:0x3f4c].
int state_of(const std::string& path);
// LB_SETCURSEL(index) and seg6:0x5318 (the demo pilot's step 4) [BYTES seg6:0x1ed8 seg6:0x1eff].
void select_row(int index);
// seg6:0x5260 on the selection and seg6:0x4f46 (sub 1) or seg6:0x511c (sub 2): the window's
// LBN_DBLCLK and refused-Enter path [BYTES seg6:0x2329 seg6:0x239f].
void expand_or_collapse();
// seg6:0x3686, Enter's acceptance: finds or adds the record of DGROUP:0x6990 (seg6:0x36b6),
// DGROUP:0xd1c = DGROUP:0x94b4, hands `label` (DGROUP:0x38c8) to Host::entered, requests state 1
// or arms the debriefing, closes the window and returns 1 [BYTES seg6:0x3686-0x37bd].
int accept(const std::string& label);
// Whether accept() finds or can add the record of directory(): false at Records::kMax records,
// where accept() refuses Enter (M3 s1 fixer) [OPEN: ADR 0012 line owed].
bool record_room();

}  // namespace is::nav
#endif
