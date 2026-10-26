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

// The control ids: Enter 1, Help 0x70, the directory list 0x65 [BYTES seg6:0x2305 seg6:0x2383]
// [ORACLE newgame-0001].
constexpr int kEnterId = 1, kHelpId = 0x70, kDirListId = 0x65;
// The directory list box's style: sort, owner-draw fixed, has-strings, notify, no integral
// height [ORACLE newgame-0001].
constexpr unsigned long kDirListStyle = 0x50b30153ul;

// The layout of WM_SIZE for the parts the port builds, in client coordinates: Enter and Help
// at (cw - 105, 5) and (cw - 105, 40), 90 x 26, the directory list at (0, 0), 160 x (ch - 26)
// [ORACLE newgame-0001: 447,5, 447,40 and 160 x 290 in a 552 x 316 client].
struct Box {
    int x = 0, y = 0, w = 0, h = 0;
    bool operator==(const Box& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};
Box enter_box(int cw, int ch);
Box help_box(int cw, int ch);
Box dir_list_box(int cw, int ch);

// The minimum track size of WM_GETMINMAXINFO: 560 x 350 [OPEN: from the decompilation of
// seg6:0x1f22, not in the docs].
constexpr int kMinTrackW = 560, kMinTrackH = 350;

// The root entry of drive 'C' + offset that the fixture-independent list shows: lower case
// with a backslash, as the oracle's tree root "d:\" [ORACLE newgame-0001].
std::string drive_root(int drive_offset);

// seg6:0x301a once it accepts the selection: state 1 is requested when DGROUP:0x91f8 = 0 or
// DGROUP:0x92e4 != 0; otherwise DGROUP:0x1e74 = 0 and DGROUP:0x1e70 = now [BYTES seg6:0x3686
// seg6:0x368c seg6:0x3693 seg6:0x369d seg6:0x36a0 seg6:0x36a5 seg6:0x36b2].
enum class EnterAction { kRefused, kRequestWave, kArmDebriefing };
EnterAction enter_action(bool armed, int wave, int restored);

}  // namespace is::nav
