// owner: M4 stage 3, track VISIT.
// The ambulance visit, state 6: the phase machine seg16:0xcfe(repaint) with its 65 phases on
// DGROUP:0x914a, the monitor typing seg16:0x2e3c, the panel layout seg16:0x1f78 / seg16:0x27ea,
// the pad's lower/raise seg15:0x4c76 in state 6, the descent seg15:0x3590 / seg15:0x374c as far
// as read, the hand-back of phase 64 (the free-spot search) and the computer ship's service
// seg16:0x32a0. Sources: work/analysis/m4/s3-visit.md sections 1, 2 and 5 with the verdicts of
// s3-visit-verify.md; docs/re/core.md "The visit"; docs/re/shell.md "State 6"; the oracle's
// core-0003 (fuel, a full visit) and wave-0001 ("Ambulance"). Portable: the drawing goes
// through VisitScreen (src/hud/visit_win32.cpp on Win32), the program's calls through
// VisitHost (src/hud/hooks.cpp).
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "world/purchases.h"

namespace is::world {

class World;

// ---- the layout of seg16:0x1f78 and seg16:0x27ea -------------------------------------------------
struct VisitLayout {
    int W = 0, H = 0;      // DGROUP:0x90ea, DGROUP:0x10
    int pad_w = 0;         // DGROUP:0x3ce6, seg15:0x482e's width k/36 + 164, k = 4·H/3 - 1100
    int pad_c = 0;         // DGROUP:0x3c9a, k/30 + 80; the pad bitmap is pad_c + 52 high
    int panel_w = 0;       // DGROUP:0x3cac = min(260, (W - pad_w)/2 - 20)
    int panel_h = 200;     // DGROUP:0x3cae
    int rx = 0, ry = 0;    // DGROUP:0x3d48 (= 0x6db0), DGROUP:0x3d4a (= 0x6db6): the right panel
    int lx = 0;            // DGROUP:0x6db4: the left panel's x (its y is ry)
    int panel_bg_h = 0;    // DGROUP:0x6dae = max(H/2, 370): the panels' background height
    int mon_dx = 12, mon_dy = 10;  // DGROUP:0x6e18, DGROUP:0x6e1a
    int mon_w = 0, mon_h = 0;      // DGROUP:0x6e14 = panel_w - 30, DGROUP:0x6e16 = panel_h - 36
    // The right monitor's text origin L = mon_dx + rx, T = mon_dy + ry.
    int L() const { return mon_dx + rx; }
    int T() const { return mon_dy + ry; }
    // seg10:0x6d52's bar k at (rx + 0x3b, ry + panel_h + 0x16 + 25k), panel_w - 0x50 x 10
    // [BYTES seg16:0x14c2-0x14ea].
    int bar_x() const { return rx + 0x3b; }
    int bar_y(int k) const { return ry + panel_h + 0x16 + 25 * k; }
    int bar_w() const { return panel_w - 0x50; }
};
// seg16:0x1f78 and seg16:0x27ea for the play area W x H [BYTES seg16:0x1f78-0x2142
// seg16:0x27f8-0x2815] (verified claims 47-48) and seg15:0x482e's pad [BYTES seg15:0x483d
// seg15:0x48c2].
VisitLayout visit_layout(int W, int H);

// ---- the visit's DGROUP words ----------------------------------------------------------------------
struct VisitState {
    std::int16_t phase = 0;        // DGROUP:0x914a
    std::int16_t descent = 0;      // DGROUP:0x92a2, seg15:0x3590's sub-step
    std::int16_t fast = 0;         // DGROUP:0x928: Ctrl held, or DGROUP:0x8f70 > 1
    std::int16_t left_on = 0;      // DGROUP:0x952c: the left monitor's record exists
    std::int16_t left_mode = 2;    // DGROUP:0x9530
    std::int16_t clicked = 0;      // DGROUP:0x958a
    std::int16_t saved_phase = 0;  // DGROUP:0x958c
    std::int16_t wings_refit = 0;  // DGROUP:0x9598
    std::int16_t w959a = 0;        // DGROUP:0x959a
    std::int16_t w9662 = 0;        // DGROUP:0x9662
    std::int16_t demon = 0;        // DGROUP:0x93a6, the Inner Demon visit (M6)
    std::uint32_t d1c44 = 0;       // DGROUP:0x1c44, the transfer's 2500 ms
    std::uint32_t d1c48 = 0;       // DGROUP:0x1c48, the 50 ms ticks and phase 60's wait
    std::int16_t weapon = 1;       // DGROUP:0x1c4c, w of phases 48-53
    std::int16_t power = 0;        // DGROUP:0x1c4e, k of phases 45-47
    std::int16_t blank_w = 0;      // DGROUP:0x1c50, the blank-line flag of phases 49 / 52
    std::int16_t blank_p = 0;      // DGROUP:0x1c52, of phase 46
    std::string text;              // DGROUP:0x6db8, the monitor text (90 bytes for LoadString)
    // seg16:0x2e3c's words.
    std::int16_t index = 0;        // DGROUP:0x1c5c
    std::int16_t lines = 0;        // DGROUP:0x1c5e
    int pen_x = 0, pen_y = 0;      // DGROUP:0x1c54, DGROUP:0x1c56
    int cur_x = 0, cur_y = 0;      // DGROUP:0x1c58, DGROUP:0x1c5a
    std::uint32_t type_due = 0;    // DGROUP:0x1c60
    // seg15:0x374c's and seg15:0x4c76's words.
    int s4 = 0;                    // 4·(roster +0x7a + (roster +0x7b - 6)/2)
    int start_y = 0, end_y = 0;    // DGROUP:0x92d8, DGROUP:0x92d6
    int pos10 = 0;                 // DGROUP:0x17fa
    int speed = 150;               // DGROUP:0x17f6
    std::uint32_t descent_due = 0; // seg15:0x3590 sub-step 1's 1000 ms [OPEN: busy or not]
    int pad_off = 0;               // DGROUP:0x181c
    int pad_max = 0;               // seg15:0x4c76's [bp-0x26] = s4 + 0x3e
    bool monitor_saved = false;    // DGROUP:0x953a exists
    bool strip_saved = false;      // DGROUP:0x953c exists
    std::uint32_t launch_due = 0;  // the port's stand-in launch [OPEN]
    VisitLayout lay;
};
// The program's visit (DGROUP), as Shop is.
VisitState& visit_state();

// ---- the drawing (src/hud/visit_win32.cpp; the tests record) ------------------------------------
class VisitScreen {
public:
    virtual ~VisitScreen() = default;
    // seg15:0x302e's drawing: the scene (IGRAPHIC #6600 id 0x69) and, on a slow machine, the
    // "Incoming Jet / Approaching Landing Pad.." line [BYTES seg15:0x302e-0x358d] [OPEN: the
    // render].
    virtual void landing_scene(const VisitLayout& l, const std::string& slow_text) = 0;
    // seg15:0x3caa(first), the incoming-jet animation; non-zero when it has ended [OPEN: read in
    // summary only].
    virtual int incoming(int first) = 0;
    // seg15:0x3590 sub-step 0's tail: the black screen, the pad (seg15:0x482e) and the panels
    // (seg16:0x1f78) [BYTES seg15:0x3590-0x374a] [OPEN].
    virtual void descent_start(const VisitLayout& l) = 0;
    // seg15:0x374c's frame: the ship at y over the pad [OPEN: summarised].
    virtual void descent_frame(const VisitLayout& l, int y, int s4, bool braking) = 0;
    // seg16:0x2144, the whole visit screen (panels, monitors, labels, bars, the pad strip).
    virtual void redraw(const VisitLayout& l, int phase, int saved_phase, bool left_on) = 0;
    // Phase 2: the ship at (x, y), s4 x s4, SRCAND / SRCPAINT from the ship's DCs, and the pad
    // strip DGROUP:0x953c saved from the screen [BYTES seg16:0xdc1-0xf32].
    virtual void ship_on_pad(const VisitLayout& l, int x, int y, int s4, int strip_h) = 0;
    // seg15:0x4c76's strip at the pad's place, the ship lowered by `offset` pixels.
    virtual void pad_strip(const VisitLayout& l, int offset, int s4) = 0;
    // seg15:0x4c76(1, 0) with the new body: the ship image rebuilt (seg15:0x2a08) [OPEN M8].
    virtual void rebuild_ship() = 0;
    // seg10:0x6d52(screen, x, y, w, h, v), ISPACE ordinal 35.
    virtual void bar(int x, int y, int w, int h, int v) = 0;
    // ---- seg16:0x2e3c's monitor
    // SelectClipRgn(DGROUP:0x9534), CreateFont(h - 1, 5, ..., 2, 0x22, "Arial"), SetBkColor 0,
    // TRANSPARENT, TA_LEFT, SetTextColor 0x0000FF00 [BYTES seg16:0x2f07-0x2f6f]; end restores.
    virtual void monitor_begin(int font_h) = 0;
    virtual void monitor_end() = 0;
    virtual int text_extent(const std::string& cp1252) = 0;  // GetTextExtent's cx
    virtual void glyph(int x, int y, char c) = 0;            // TextOut of one character
    virtual void black(int x, int y, int w, int h) = 0;      // PatBlt BLACKNESS
    virtual void block(int x, int y, int w, int h, std::uint32_t colour) = 0;  // PATCOPY
    // ScrollDC(screen, 0, -1, R, R) `n` times on R = (l, t, r, b).
    virtual void scroll_up(int l, int t, int r, int b, int n) = 0;
    virtual void flash(int x, int y) = 0;  // seg15:0x84b0, the CRT flash [OPEN]
    // Phase 15: the monitor's text area into DGROUP:0x953a, for seg16:0x2144 [BYTES
    // seg16:0x125a-0x12c8].
    virtual void save_monitor(int x, int y, int w, int h) = 0;
    // seg16:0x64fa, the left monitor (the Limited edition's "ORDER FULL GAME TO SEE ..." text).
    virtual void left_monitor(const VisitLayout& l, bool limited) = 0;
    virtual void black_screen() = 0;  // PatBlt(screen, 0, 0, W, H, BLACKNESS)
    // seg15:0x1328(0) and seg16:0x1f24: the scene's bitmaps freed (DGROUP:0x9590, 0x95a0, the
    // strip and the saved monitor).
    virtual void free_scene() = 0;
};

// ---- the program's calls -------------------------------------------------------------------------
class VisitHost {
public:
    virtual ~VisitHost() = default;
    virtual std::uint32_t now() = 0;           // DGROUP:0x1e50, the pass's tick
    virtual std::uint32_t current_time() = 0;  // GetCurrentTime
    virtual void busy_wait(int ms) = 0;        // a GetCurrentTime loop without pumping
    virtual void pump_wait(int ms) = 0;        // seg10:0x740c until GetCurrentTime + ms
    virtual void sound(int id) = 0;            // seg10:0x744c
    virtual void sound_at(int id, int a, int b, int mode) = 0;  // seg10:0x754a
    virtual std::string lang(int id) = 0;
    // seg3:0x1542(type 1) of the served ship for %D: "THE TRANSFORMER" [ORACLE core-0003]
    // (string 2702 "THE " with the ship's name) [OPEN: seg3:0x1542 itself].
    virtual std::string ship_name(int ship) = 0;
    virtual std::string pilot() = 0;           // DGROUP:0x380c
    virtual bool ctrl_held() = 0;              // GetKeyState(VK_CONTROL) & 0x8000
    virtual int w8f70() = 0;                   // DGROUP:0x8f70
    virtual int calibrate() = 0;               // DGROUP:0x8ffc
    virtual int depth() = 0;                   // DGROUP:0x90ee
    virtual bool limited() = 0;                // DGROUP:0x920c = 0
    // DialogBox(0xc2, ..., seg16:0x4104), modal; its result is ignored (verified claim 28).
    virtual void options_dialog() = 0;
    // seg10:0x82a2(6): the New Ship Body chooser of a dead ship [OPEN M8].
    virtual void body_chooser() = 0;
    // Phase 0's calls: seg10:0xa856 (the image-slot DCs freed), seg11:0x2f0 (the HUD windows),
    // seg13:0x65f2(3, 0, 0) and seg10:0x7480(-2, 1) of the scene.
    virtual void enter_calls() = 0;
    // seg10:0x6dd4(0), seg10:0x6e76(8, 0): the palette back for the wave.
    virtual void palette_back() = 0;
    // seg15:0xcb0 then seg15:0x10be per frame in state 6: the launch; true when it ended
    // (seg15:0x10be advances the phase at its end) [OPEN: the desk's launch is not reachable from
    // here; the program's stand-in].
    virtual void launch_enter() = 0;
    virtual bool launch_frame() = 0;
    virtual void recreate_dcs() = 0;           // seg10:0xa90c
    virtual void seg18_7b2() = 0;              // with DGROUP:0x92e4 = 1 (DGROUP:0x9662)
    virtual void to_screen(std::int16_t x, std::int16_t y, std::int16_t& sx, std::int16_t& sy) = 0;  // seg8:0x29ea
    virtual void watch_windows(int code) = 0;  // seg13:0x65f2(2)
    virtual void cursor_home() = 0;            // SetCursorPos(DGROUP:0x1d48, DGROUP:0x10)
    virtual void music_back() = 0;             // seg10:0x7480(DGROUP:0x9220 == 2 ? 2 : 1, 1)
    virtual void message(int ship, int code) = 0;  // seg3:0x6ec(ship, 7, 0, code)
    virtual void repaint_wave() = 0;           // seg11:0xafe
    virtual void set_state(int s) = 0;         // DGROUP:0x90f4
    virtual void demon_end() = 0;              // seg16:0x7210(1) (state 9, M6)
    virtual void demon_click() = 0;            // the full edition's Inner Demon subgame (M6)
    virtual void box_2322() = 0;               // seg16:0x68d6(DGROUP:0x9532, 2322, -1)
    virtual int rand() = 0;                    // seg5:0xdc
    virtual void todo(const std::string& what, const char* address) = 0;
};

struct VisitCtx {
    VisitCtx(World& w_, Shop& s_, VisitState& v_, VisitHost& h_, VisitScreen& scr_, PurchaseHost& ph_,
             const WeaponWords& W_)
        : w(w_), s(s_), v(v_), h(h_), scr(scr_), ph(ph_), W(W_) {}
    World& w;
    Shop& s;
    VisitState& v;
    VisitHost& h;
    VisitScreen& scr;
    PurchaseHost& ph;
    const WeaponWords& W;
    bool player_game = true;    // DGROUP:0x95bc
    std::int16_t restored = 0;  // DGROUP:0x92e4
    std::int16_t plus = 0;      // DGROUP:0x11aa
};

// The entry at seg7:0x5fa-0x608: DGROUP:0x90f4 = 6 (the caller's), DGROUP:0x914a = 0, and
// seg16:0xcfe(1) once [BYTES seg7:0x5fa seg7:0x600 seg7:0x608].
void visit_begin(VisitCtx& c);
// seg16:0xcfe(repaint): repaint != 0 is WM_PAINT / seg11:0xafe's case 6, 0 the state's frame.
void visit(VisitCtx& c, int repaint);
// seg16:0x2e3c(init, blank): the monitor typing.
void visit_type(VisitCtx& c, int init, int blank);
// seg15:0x4c76(first, down) in state 6 [BYTES seg15:0x4c76-0x52d8] (verified claims 31-33).
void visit_pad(VisitCtx& c, int first, int down);
// seg16:0x638a(flags, x, y, esc): a click (WM_LBUTTONDOWN) or ESC (WM_KEYUP VK_ESCAPE with
// DGROUP:0x93a6 = 0) on the visit screen [BYTES seg16:0x638a-0x64c6].
void visit_click(VisitCtx& c, int x, int y, int esc);
// The free spot of phase 64 for the ambulance's point (x, y): the candidate moves by
// (rand() % 150 + 100)·8 in x and then y while any record lies in its rectangle or it is out
// of bounds [BYTES seg16:0x1c4c-0x1d56].
void visit_free_spot(VisitCtx& c, std::int16_t& x, std::int16_t& y);
// seg16:0x872 on the visit's hosts: ambulance.h ambulance_repairs_done (the one port) with
// seg16:0x3784 through c.ph and seg3:0x6ec through c.h [BYTES seg16:0x87e-0x893]. seg16:0x32a0
// (a ship served without the visit) is ambulance.h ambulance_drop.
void visit_purchases(VisitCtx& c);
// The program's visit for the other tracks' callers (src/hud/hooks.cpp fills the slots on
// Win32; an empty slot does nothing). The tick's seg7:0x5fa calls `enter` (it sets state 6,
// DGROUP:0x914a = 0 and runs seg16:0xcfe(1)); ROSTER's seg14:0x0 calls `wave_setup(i)` for seg14:0xd32's seg16:0x3784(i, 1);
// the window procedure's WM_LBUTTONDOWN in state 6 calls `click(x, y)` (seg16:0x638a)
// [DESIGN: the slots stand for the far calls across the port's libraries].
struct VisitLinks {
    std::function<void()> enter;
    std::function<void(int ship)> wave_setup;
    std::function<void(int x, int y)> click;
};
VisitLinks& visit_links();

// The state-6 pace inputs of timing.md (shell::PaceInputs): the gate (DGROUP:0x952c set,
// DGROUP:0x9530 = 1) and the slow phase (DGROUP:0x914a = 1, DGROUP:0x92a2 = 0).
bool visit_pace_gate(const VisitState& v);
bool visit_pace_slow(const VisitState& v);

}  // namespace is::world
