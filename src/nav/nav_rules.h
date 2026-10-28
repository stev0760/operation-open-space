// The portable rules of the Navigation window IS_NAV (docs/re/shell.md "From ESC to the wave",
// steps 11 and 12; docs/re/formats.md "INI files", the Nav keys): where it opens, what its
// close writes, its child layout and what Enter does. The window itself is src/nav/nav.cpp.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace is::nav {

// DGROUP:0x6af0-0x6af6: NavLeft (30), NavTop (30), NavRight (560) and NavBottom (350), which
// seg6:0x1c72 passes to CreateWindow as x, y, width and height [BYTES seg6:0x1cb6 seg6:0x1ccc
// seg6:0x1ce3 seg6:0x1cfa seg6:0x1d10].
struct Placement {
    int x = 30, y = 30, w = 560, h = 350;
};
constexpr int kDefaultLeft = 30, kDefaultTop = 30, kDefaultRight = 560, kDefaultBottom = 350;

// The style 0x10cc0000: WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME [BYTES seg6:0x1d10]
// [ORACLE newgame-0001].
constexpr unsigned long kStyle = 0x10cc0000ul;
constexpr int kTitleString = 2140;  // "Inner Space Navigation"

// The window rectangle kept in DGROUP:0x6af0 (WM_MOVE and WM_SIZE) as seg6:0x2ebc writes it, in
// this order: NavLeft = left, NavRight = right - left, NavTop = top, NavBottom = bottom - top
// [BYTES seg6:0x2f07 seg6:0x2f32 seg6:0x2f59 seg6:0x2f84] [ORACLE newgame-0001].
std::vector<std::pair<std::string, int>> close_writes(int left, int top, int right, int bottom);

// The control ids: Enter 1, Help 0x70, the directory list 0x65, the file list 0x6b [BYTES
// seg6:0x2305 seg6:0x2383 seg6:0x23d4] [ORACLE newgame-0001].
constexpr int kEnterId = 1, kHelpId = 0x70, kDirListId = 0x65, kFileListId = 0x6b;
// The directory list box's style: sort, owner-draw fixed, has-strings, notify, no integral
// height, both scroll bars [BYTES seg6:0x24d1] [ORACLE newgame-0001].
constexpr unsigned long kDirListStyle = 0x50b30153ul;
// The file list box's style: sort, owner-draw fixed, has-strings, notify, no integral height,
// multicolumn, extended selection, vertical scroll bar [BYTES seg6:0x23d4] [ORACLE newgame-0001].
constexpr unsigned long kFileListStyle = 0x50a30b53ul;
// The button styles: Enter BS_DEFPUSHBUTTON with WS_GROUP | WS_TABSTOP, Help BS_PUSHBUTTON with
// WS_TABSTOP [BYTES seg6:0x23d4].
constexpr unsigned long kEnterStyle = 0x50030000ul, kHelpStyle = 0x50010000ul;
// LB_SETHORIZONTALEXTENT 200 to the directory list and LB_SETCOLUMNWIDTH 95 to the file list
// [BYTES seg6:0x2507 seg6:0x2527].
constexpr int kDirExtent = 200, kFileColumn = 95;
// The labels of Enter and Help: strings 2143 "&Enter" and 2144 "&Help", 0x28 bytes [BYTES
// seg6:0x23d4] [BYTES ENG_LANG.DLL STRING 134] ids 2143-2144.
constexpr int kEnterString = 2143, kHelpString = 2144;

struct Box {
    int x = 0, y = 0, w = 0, h = 0;
    bool operator==(const Box& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};
// The children as seg6:0x23d4 creates them in a cw x ch client: Enter (cw - 90, 15) and Help
// (cw - 90, 51), 90 x 26; the directory list (0, 0) 160 x (ch - 26); the file list (164, 0)
// (cw - 264) x (ch - 26) [BYTES seg6:0x23d4].
Box enter_create_box(int cw, int ch);
Box help_create_box(int cw, int ch);
Box file_list_create_box(int cw, int ch);
// The layout of WM_SIZE in client coordinates: Enter and Help at (cw - 105, 5) and (cw - 105,
// 40), 90 x 26, the directory list at (0, 0), 160 x (ch - 26), the file list at (164, 0),
// (cw - 284) x (ch - 26) [BYTES seg6:0x1f22] [ORACLE newgame-0001: 447,5, 447,40, 160 x 290 and
// 164,0 268 x 290 in a 552 x 316 client]. The directory list is created at its WM_SIZE box.
Box enter_box(int cw, int ch);
Box help_box(int cw, int ch);
Box dir_list_box(int cw, int ch);
Box file_list_box(int cw, int ch);

// The minimum track size of WM_GETMINMAXINFO: 560 x 350 [OPEN: from the decompilation of
// seg6:0x1f22, not in the docs].
constexpr int kMinTrackW = 560, kMinTrackH = 350;

// DGROUP:0xd18, the drive root seg6:0x1c72 builds: 'C' + DGROUP:0x94ac, ":\\" [BYTES
// seg6:0x1c97]; the tree draws it lower case ("d:\\", newgame-0001).
std::string drive_root(int drive_offset);

// seg6:0x301a once it accepts the selection: state 1 is requested when DGROUP:0x91f8 = 0 or
// DGROUP:0x92e4 != 0; otherwise DGROUP:0x1e74 = 0 and DGROUP:0x1e70 = now [BYTES seg6:0x3686
// seg6:0x368c seg6:0x3693 seg6:0x369d seg6:0x36a0 seg6:0x36a5 seg6:0x36b2].
enum class EnterAction { kRefused, kRequestWave, kArmDebriefing };
EnterAction enter_action(bool armed, int wave, int restored);

}  // namespace is::nav
