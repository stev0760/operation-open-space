// The RACE and DUEL boxes of the Navigation window (docs/re/nav.md "Race and duel boxes";
// docs/re/formats.md, the race and duel keys): the two group boxes and their seven drop-down
// combos that seg6:0x23d4 creates, fills from ENG_LANG.DLL and selects from INSPACE.INI, that
// the WM_SIZE case of seg6:0x1f22 places, that seg6:0x2dba shows for the selected row and that
// Enter (seg6:0x301a, src/nav/enter.h) reads with CB_GETCURSEL.
//
// Portable: the control table, the geometry and the INI selection. The window side (creation,
// placement, showing and reading the controls) is boxes_win32.cpp, declared at the end under
// _WIN32; src/nav/nav.cpp calls it from WM_CREATE, WM_SIZE and the selection handler.
#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "nav/nav_rules.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace is::settings {
class IniFile;
}

namespace is::nav {

// ---- control ids [BYTES seg6:0x25ab seg6:0x2609 seg6:0x26a0 seg6:0x27ab seg6:0x288a
// seg6:0x2969 seg6:0x2a48 seg6:0x2b27 seg6:0x2c73]
constexpr int kRaceBoxId = 0x8c;   // group box "RACE"
constexpr int kRacersId = 0x8d;    // INI Racers
constexpr int kRaceDiffId = 0x8e;  // INI RaceDiff
constexpr int kRaceLapsId = 0x8f;  // INI RaceLaps
constexpr int kRaceOppId = 0x90;   // INI RaceOpp
constexpr int kDuelersId = 0x91;   // INI Duelers
constexpr int kDuelDiffId = 0x92;  // INI DuelDiff
constexpr int kDuelBoxId = 0x93;   // group box "DUEL"
constexpr int kDuelOppId = 0x94;   // INI DuelOpp

// The group boxes' captions: 2145 "RACE", 2146 "DUEL" (LoadString 0x861 and 0x862, 40 bytes)
// [BYTES seg6:0x2587 seg6:0x25e5] [BYTES ENG_LANG.DLL STRING 135] ids 2145-2146.
constexpr int kStrRaceBox = 2145, kStrDuelBox = 2146;
// LoadString's buffer for every box string: 0x28 bytes [BYTES seg6:0x2441 seg6:0x26da].
constexpr int kBoxStringMax = 0x28;

// Styles: the group boxes WS_CHILD | WS_VISIBLE | BS_GROUPBOX, the combos WS_CHILD | WS_VISIBLE
// | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, unsorted [BYTES seg6:0x25b6 seg6:0x26ab].
constexpr unsigned long kGroupBoxStyle = 0x50000007ul;
constexpr unsigned long kComboStyle = 0x50210003ul;

// Which box seg6:0x2dba shows: 0 neither, 1 RACE (0x8c-0x90), 2 DUEL (0x93, 0x91, 0x92, 0x94),
// SW_SHOW (5) or SW_HIDE (0) per control [BYTES seg6:0x2dba].
enum class BoxKind { kNone = 0, kRace = 1, kDuel = 2 };

// One combo of the table.
struct Combo {
    int id;
    const char* key;  // the INI key, [Inner Space]
    int def;          // GetPrivateProfileInt's default
    std::vector<int> strings;  // CB_ADDSTRING (0x403) in this order
    BoxKind box;
};

// The seven combos in seg6:0x23d4's creation order, which is also the order of the INI reads
// [BYTES seg6:0x26ab seg6:0x2773 seg6:0x27b6 seg6:0x2851 seg6:0x2895 seg6:0x2930 seg6:0x2974
// seg6:0x2a0f seg6:0x2a53 seg6:0x2aee seg6:0x2b32 seg6:0x2c3a seg6:0x2c7e seg6:0x2d7f]
// [BYTES ENG_LANG.DLL STRING 135] ids 2147-2159 [BYTES ENG_LANG.DLL STRING 136] ids 2160-2169:
//   0x8d Racers 1: 2147 Solo, 2148 2 Ships, 2149 3 Ships, 2150 4 Ships
//   0x91 Duelers 0: 2151 1 Opponent, 2152 2 Opponents, 2153 3 Opponents
//   0x8e RaceDiff 0 and 0x92 DuelDiff 0: 2154 Normal, 2155 Difficult, 2156 Dangerous!
//   0x8f RaceLaps 1: 2157 1 Lap, 2158 5 Laps, 2159 10 Laps
//   0x90 RaceOpp 2: 2160 Fastest, 2161 Faster, 2162 Similar, 2163 Same Ships, 2164 Slower,
//        2165 Slowest
//   0x94 DuelOpp 2: 2166 Toughest, 2167 Tougher, 2162 Similar, 2163 Same Ships, 2168 Weaker,
//        2169 Weakest
const std::array<Combo, 7>& combos();
const Combo* combo(int id);

// The control ids of a box in seg6:0x2dba's ShowWindow order: RACE 0x8c, 0x8d, 0x8e, 0x8f,
// 0x90; DUEL 0x93, 0x91, 0x92, 0x94 [BYTES seg6:0x2dba].
std::vector<int> box_controls(BoxKind kind);
// seg6:0x2dba(kind): the ShowWindow command for one control, SW_SHOW 5 or SW_HIDE 0.
int show_command(BoxKind kind, int id);

// The index seg6:0x23d4 selects with CB_SETCURSEL (0x40e): the INI value as
// GetPrivateProfileInt returns it, the default when the key is absent [BYTES seg6:0x2762
// seg6:0x2773]. A value outside the list leaves no selection (CB_SETCURSEL fails), so Enter
// reads CB_ERR (-1) [REF Win16 SDK CB_SETCURSEL].
int initial_index(const settings::IniFile* ini, const Combo& c);

// ---- geometry

// DGROUP:0x94b8: the height of GetTextExtent("Test", 4) (DGROUP:0xd51) in the font DGROUP:0x90fe
// [BYTES seg6:0x2642 seg6:0x2658 DGROUP:0xd51].
constexpr const char* kExtentText = "Test";

// WM_CREATE's rectangles, client width cw: the group boxes at (cw - 0x5a, 0x78, 0x5a, 0x28), the
// combos at x cw - 0x3c, 0x3c wide, rows 0x78, 0x96, 0xb4, height (ext + 10) * 3, or
// (ext + 5) * 6 for 0x90 and 0x94 [BYTES seg6:0x25b6 seg6:0x2614 seg6:0x26ab seg6:0x27b6
// seg6:0x2895 seg6:0x2974 seg6:0x2a53 seg6:0x2b32 seg6:0x2c7e]. WM_SIZE moves them at once.
Box create_box(int id, int cw, int ext);
// WM_SIZE's rectangles for client cw x ch, with y0 = ch - 0x9b: the group boxes at (cw - 0x6e,
// y0, 0x66, 0x7a); RACE combos at x cw - 0x61, 0x50 wide, DUEL combos at x cw - 0x63, 0x53
// wide; rows y0 + 0x12, + 0x2b, + 0x44 and (0x90) + 0x5d; heights 0x8d ext * 4 + 0x1e, 0x90
// and 0x94 (ext + 5) * 6, the others (ext + 10) * 3 [BYTES seg6:0x211e seg6:0x2141 seg6:0x2163
// seg6:0x2194 seg6:0x21bc seg6:0x21ed seg6:0x2215 seg6:0x2246 seg6:0x2274 seg6:0x229c]
// [ORACLE newgame-0001: group boxes 442,161 102x122 and combos at x 455, 80 to 83 wide in a
// 552 x 316 client].
Box size_box(int id, int cw, int ch, int ext);

// ---- what Enter reads

// CB_GETCURSEL (0x407) of each combo, -1 for CB_ERR [BYTES seg6:0x32fa-0x34aa
// seg6:0x34de-0x365f].
struct BoxValues {
    int racers = 1, race_diff = 0, race_laps = 1, race_opp = 2;  // 0x8d, 0x8e, 0x8f, 0x90
    int duelers = 0, duel_diff = 0, duel_opp = 2;                // 0x91, 0x92, 0x94
    int& at(int id);
    int at(int id) const;
};
// The values seg6:0x23d4 leaves selected: initial_index() of every combo.
BoxValues initial_values(const settings::IniFile* ini);

#ifdef _WIN32
// ---- the window side (boxes_win32.cpp)

// seg6:0x23d4's part for the boxes, after the two list boxes: creates the RACE and DUEL group
// boxes (WM_SETFONT font, lParam 1), measures DGROUP:0x94b8 with GetTextExtentPoint32 of "Test"
// in `font`, creates the seven combos (WM_SETFONT font, lParam 0), adds their strings from
// `lang` (ENG_LANG.DLL, cp1252, cut to 39 bytes) and selects initial_index(). Returns
// DGROUP:0x94b8, which place_boxes() needs. The CTL3D subclass call after each control
// (seg2:0x1a7a) does nothing in the port, as for the other children (nav.cpp).
int create_boxes(HWND nav, HINSTANCE instance, HFONT font,
                 const std::function<std::string(int)>& lang, const settings::IniFile* ini);
// WM_SIZE: moves the nine controls to size_box().
void place_boxes(HWND nav, int cw, int ch, int ext);
// seg6:0x2dba(kind).
void show_boxes(HWND nav, BoxKind kind);
// Enter's CB_GETCURSEL of the seven combos.
BoxValues read_boxes(HWND nav);
#endif

}  // namespace is::nav
