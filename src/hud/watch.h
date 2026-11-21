// The ship watch windows (docs/re/shell.md "The ship watch windows"; work/analysis/m5/s4-commands.md
// 3.1-3.5 with s4-commands-verify.md claims 57-65 and its Omissions; M5 stage 4, track WATCH):
//  - the Ship Watch IS_WATCH that W opens: window DGROUP:0x95fc, the key routine seg13:0x563c,
//    the creation seg13:0x5878 (the 0x9a0 handler), the title seg13:0x57ac, the window procedure
//    seg13:0x4f4e and the painter seg13:0x50b0;
//  - the All Ships window IS_SMART: window DGROUP:0x95fe, the creation seg13:0x638a (the 0x9b4
//    handler), the procedure seg13:0x613a and the rows seg13:0x6450;
//  - seg13:0x65f2(m), the three watch windows together, and seg13:0x6890's refresh of a ship.
// The decisions and the painting are portable (WatchHost, over hud::Canvas); the windows are
// Win32 (watch_win32.cpp). Both windows are owned top-level windows with a caption, a system
// menu and a sizing frame (style 0x10CC0000, parent DGROUP:0x16), as IS_ICON is: the Wine oracle
// under Xvfb has no window manager and so shows no caption (ships-0005, newgame-0001), the
// Windows 98 photograph shows it (win98-0001).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "hud/activity.h"
#include "hud/canvas.h"
#include "hud/info.h"
#include "hud/target.h"

namespace is::hud {

// ---- the program's entry points (watch_win32.cpp; the host build has no window: they do
// nothing and report no window)

// seg13:0x563c(ctrl, alt): the W key (seg11:0x712b passes the Ctrl and Alt flags). Returns 1
// when a watch is shown or refreshed, else 0 [BYTES seg13:0x563c-0x57ab].
int watch_key(bool ctrl, bool alt);
// seg13:0x5878, the main window's 0x9a0 handler (seg2:0x4e1): CreateWindow("IS_WATCH") when the
// left IS_INFO display exists and no watch does, then SetActiveWindow(main) [BYTES seg13:0x5878-0x58da].
void watch_create();
// seg13:0x5850: DestroyWindow(DGROUP:0x95fc), DGROUP:0x95fc = 0 [BYTES seg13:0x5850-0x5876].
void watch_destroy();
bool watch_exists();  // DGROUP:0x95fc != 0
// seg13:0x638a, the creation half of the 0x9b4 toggle (seg2:0x513) [BYTES seg13:0x638a-0x644e].
void smart_create();
// seg13:0x6362: DestroyWindow(DGROUP:0x95fe), DGROUP:0x95fe = 0.
void smart_destroy();
bool smart_exists();  // DGROUP:0x95fe != 0
// seg13:0x6890(i)'s windows part (from seg13:0x675e's 5000 ms ship loop): seg13:0x6450(0, 2, i)
// with an IS_SMART window and i != the player, then seg13:0x50b0(0, 2) when the watch window
// exists and its mark is (ship pool, i) [BYTES seg13:0x6890-0x68da]. The left display's
// seg13:0x58dc(0, 2) that follows is the caller's.
void watch_ship(int i);
// seg13:0x6450(0, mode, i) alone: seg7:0x65aa's call for a ship i != the player, mode 1 [BYTES
// seg7:0x65cb-0x65e2]; nothing without an IS_SMART window [BYTES seg13:0x645d-0x6462].
void smart_row(int i, int mode);
// seg13:0x50b0(0, 0) of seg13:0x675e's tail [BYTES seg13:0x67f2-0x67fa].
void watch_tail();
// seg13:0x65f2(m) [BYTES seg13:0x65f2-0x6715]: 0 posts 0x9aa / 0x9b4 for IconWnd / SmWnd
// windows that do not exist and returns 1 when it posted; 1 closes all three; 2 shows them; 3
// hides them.
int watch_windows(int m);
// seg13:0x6824-0x6881's notice: DGROUP:0x95fa = n and PostMessage(main, 0x9b4) [BYTES seg13:0x6870-0x6881].
void all_ships_notice(int n);
// seg3:0x1542(caps, pool, rec, buf, mode) on the program's World and strings (activity.h
// object_name over the watch windows' ActivityHost), for the message dispatcher's %D (M5 s4
// integrate, WATCH binding 11); the host build returns "".
std::string program_object_name(int caps, int pool, int rec, int mode);
// seg11:0x3ed8(2)'s drawing: GetDC(DGROUP:0x95fc) and seg11:0x2ce6(hdc, 2), the direction mark
// round the watch's cell (Info::blink_mark through Links::draw_mark2).
void watch_mark2();
// The INI keys of the two windows' positions for the write-back at exit (seg6:0x33e): WatchLeft,
// WatchTop, WatchRight (DGROUP:0x1ddc, 0x1dde, 0x1de0) while DGROUP:0x8fea is set and SmLeft,
// SmTop (DGROUP:0x1dec, 0x1dee) while DGROUP:0x8ff0 is set [BYTES seg6:0x424-0x470 seg6:0x588
// seg6:0x5ae]. The flags are cleared by the call.
struct WatchKeys {
    bool watch_dirty = false;  // DGROUP:0x8fea
    int watch_left = 0, watch_top = 0, watch_right = 0;
    bool sm_dirty = false;  // DGROUP:0x8ff0
    int sm_left = 0, sm_top = 0;
};
WatchKeys take_watch_keys();

// What the program binds: seg4:0x78(s), the think the demo's give-up of seg3:0x1d0a calls
// (unbound: logged once) [BYTES seg3:0x2223-0x2226]; and the design byte +0x44 of design n
// (body byte +9 bit 0x20, [BYTES seg14:0x1efd-0x1f09]; unbound: 0).
struct WatchLinks {
    std::function<void(int ship)> think;
    std::function<int(int design)> design_flags44;
};
WatchLinks& watch_links();

// ---- the portable logic

// The words of the watch windows that are not the direction mark's (one set per program, as
// DGROUP is).
struct WatchState {
    std::uint32_t text_due = 0;  // dword DGROUP:0x1c8a, the text's 500 ms throttle
    // IS_SMART: DGROUP:0x95fa the rows, DGROUP:0x95f8 the half scale (INI SScale), and the rows'
    // mark entries 11 + k of DGROUP:0x6e38 (+0x14 the glint, +0x16 x, +0x18 y, +0x1a rec, +0x1c
    // pool: DGROUP:0x6f96 ... 0x6f9e + 0x1e·k).
    int rows = 0;
    int half = 0;
    struct Row {
        int glint = 0, x = 2, y = 2, rec = 0, pool = 14;
    };
    std::array<Row, 9> row{};
};

class WatchHost {
public:
    virtual ~WatchHost() = default;
    virtual bool window() const = 0;          // DGROUP:0x95fc != 0
    virtual int ship_pool() const { return 14; }  // DGROUP:0x9324
    virtual int player() const { return 0; }      // DGROUP:0x9646
    virtual int last_ship() const { return 0; }   // DGROUP:0x9316
    // The player's target words +0x3e / +0x40 (DGROUP:0x70ce / 0x70d0 + 0x15a·P).
    virtual int player_target_pool() const { return -1; }
    virtual int player_target_rec() const { return 0; }
    virtual int defenders() const { return 0; }  // DGROUP:0x940a
    virtual DefenderWords defender(int /*k*/) const { return {}; }
    virtual int object_state(int /*pool*/, int /*rec*/) const { return 1; }  // object +0x18
    virtual bool target_ok(int /*pool*/, int /*rec*/) const { return false; }  // seg3:0x0
    // The watch mark, entry 2 of DGROUP:0x6e38 (Info::mark(2)): DGROUP:0x6e90 pool, 0x6e8e rec,
    // 0x6e8a / 0x6e8c the cell origin, 0x6e88 the glint.
    virtual Mark& mark() = 0;
    virtual void next_mark() {}    // seg3:0x45a(6)
    virtual void post_create() {}  // PostMessage(DGROUP:0x16, 0x9a0, 0, 0)
    virtual void post_paint() {}   // PostMessage(DGROUP:0x95fc, WM_PAINT, 0, 0)
    virtual void set_title(const std::string&) {}  // SetWindowText(DGROUP:0x95fc, text)
    virtual void destroy() {}      // seg13:0x5850
    // The painter's reads.
    virtual std::uint32_t now() const { return 0; }  // DGROUP:0x1e50
    virtual int attitude(int /*a*/, int /*b*/) const { return 0; }  // seg13:0x2688
    virtual int team(int /*ship*/) const { return 0; }  // design word DGROUP:0x970a
    virtual std::string team_name(int /*team*/) { return {}; }  // seg13:0x218a
    virtual std::string attitude_words(int /*v*/) { return {}; }  // seg11:0x3f2e into DGROUP:0x93e0
    virtual std::string lang(int /*id*/) { return {}; }  // LoadString(DGROUP:0x9212, id, buf, 0x50)
    virtual bool monochrome() const { return false; }   // DGROUP:0x90ee == 1
    // seg3:0x1d0a(a, r, 3) and seg3:0x304c(P, r, 3): they write the lines DGROUP:0x8694/0x86d0.
    virtual void activity(int /*a*/, int /*r*/) {}
    virtual void defender_activity(int /*r*/) {}
    virtual const std::string& line1() const = 0;  // DGROUP:0x8694
    virtual const std::string& line2() const = 0;  // DGROUP:0x86d0
    virtual void clear_lines() {}                  // lstrcpy from DGROUP:0x1376 / 0x1377
    // seg3:0x1542(caps, pool, rec, buf, mode) and seg17:0x8e60(rec, buf).
    virtual std::string name(int /*caps*/, int /*pool*/, int /*rec*/, int /*mode*/) { return {}; }
    virtual std::string pilot_name(int /*ship*/) { return {}; }
    // seg13:0x5a6a's record fields (Host::cell); it also puts the cell's images in the canvas.
    virtual bool cell(int /*pool*/, int /*rec*/, CellInputs& /*out*/) { return false; }
    virtual bool gate_picture() { return false; }
    virtual bool fragment(int /*k*/) { return false; }
    virtual int client_width() const { return 0; }  // GetClientRect(DGROUP:0x95fc).right
};

// seg13:0x563c(ctrl, alt) [BYTES seg13:0x563c-0x57ab].
int watch_key_563c(WatchHost& h, bool ctrl, bool alt);
// seg13:0x57ac's text: seg3:0x1542(0, pool, rec, buf, 0) cased by seg11:0x3fd4(buf, 2), and for a
// ship " - " and seg17:0x8e60(rec) cased the same way [BYTES seg13:0x57ac-0x584f].
std::string watch_title(WatchHost& h);
// seg13:0x50b0(hdc, mode) on `c` (Img::kScreen is the window's client) [BYTES seg13:0x50b0-0x545f].
void watch_paint(WatchHost& h, WatchState& ws, Canvas& c, int mode);
// seg13:0x5a6a(hdc, x, y, pool, rec, mark, flag, scale)'s drawing on `c` from a plan, as the left
// display's Info::target_picture draws it (the glint is not drawn [OPEN]).
void draw_cell(Canvas& c, WatchHost& h, const CellPlan& p);
// seg13:0x6450(hdc, mode, i) [BYTES seg13:0x6450-0x65f1]. Returns 0 after drawing, 1 when the row
// count became 0 (the caller runs seg13:0x6362) and 2 when it changed (the caller moves the
// window to its own rectangle so that WM_GETMINMAXINFO resizes it); nothing is drawn then.
int smart_rows(WatchHost& h, WatchState& ws, Canvas* c, int mode, int i);
// WM_GETMINMAXINFO of IS_SMART: the fixed client width 50 >> half and the window height cyCaption
// + 46n + 6, or 23n + 3 at half scale [BYTES seg13:0x628d-0x62e1].
int smart_width(int half);
int smart_height(int caption, int rows, int half);

}  // namespace is::hud
