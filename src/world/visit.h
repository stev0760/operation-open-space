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
#include <optional>
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
    // DGROUP:0x3ca2, seg15:0x482e's vanishing point y (gfx/pad.h pad_geometry; 321 at 1024 x 768),
    // read by seg15:0x454c [BYTES seg15:0x4902-0x4975 seg15:0x45a5].
    int vy = 0;
    // The right monitor's text origin L = mon_dx + rx, T = mon_dy + ry.
    int L() const { return mon_dx + rx; }
    int T() const { return mon_dy + ry; }
    // seg10:0x6d52's bar k at (rx + 0x3b, ry + panel_h + 0x16 + 25k), panel_w - 0x50 x 10
    // [BYTES seg16:0x14c2-0x14ea].
    int bar_x() const { return rx + 0x3b; }
    int bar_y(int k) const { return ry + panel_h + 0x16 + 25 * k; }
    int bar_w() const { return panel_w - 0x50; }
};
// seg16:0x1f78 and seg16:0x27ea for the width W and the client height DGROUP:0x10 [BYTES
// seg16:0x1f78-0x2142 seg16:0x1fbc seg16:0x1ff5 seg16:0x27f8-0x2815] (verified claims 47-48) and
// seg15:0x482e's pad from the play height DGROUP:0x90ec and the client height (gfx/pad.h
// pad_geometry: DGROUP:0x3ce6, DGROUP:0x3c9a, DGROUP:0x3ca2) [BYTES seg15:0x483d-0x4975].
VisitLayout visit_layout(int W, int client_h, int play_h);
// The same with one height for both [DESIGN: the tests' and the callers' single-height form].
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
    std::int16_t flag = 0;         // DGROUP:0x17fc, the descent's flag (core.md "The descent", step 3)
    std::int16_t flag_prev = 0;    // DGROUP:0x17f8, the flag at the previous call (sound.md "The descent")
    // dword DGROUP:0x17f2, seg15:0x3590 sub-step 1's deadline: 0 from sub-step 0's end while the
    // descent runs, then DGROUP:0x1e50 + 1000 at sound 9; sub-step 1 holds until it lies below
    // DGROUP:0x1e50 (a deadline, no busy-wait) [BYTES seg15:0x35c1-0x35c6 seg15:0x36c3-0x370a].
    std::uint32_t descent_due = 0;
    int pad_off = 0;               // DGROUP:0x181c
    int pad_max = 0;               // seg15:0x4c76's [bp-0x26] = s4 + 0x3e
    bool monitor_saved = false;    // DGROUP:0x953a exists
    bool strip_saved = false;      // DGROUP:0x953c exists
    VisitLayout lay;
};
// The program's visit (DGROUP), as Shop is.
VisitState& visit_state();

// ---- the drawing (src/hud/visit_win32.cpp; the tests record) ------------------------------------
// seg15:0x302e's out block for scene 105: slot 0's bottom + 1 (word +6) and slot 1's top (word
// +0xa) [BYTES seg15:0x3248-0x325a].
struct LandingSlots {
    int bottom = 0, top = 0;
};

class VisitScreen {
public:
    virtual ~VisitScreen() = default;
    // seg15:0x302e's drawing in order (s6-scene.md section 2, verified claims 53-58) [BYTES
    // seg15:0x3079-0x3519]: PatBlt(screen, 0, 0, W, H, BLACKNESS) when `black` (DGROUP:0x93a6 =
    // 0), seg10:0x6da6(0, 1), the slow-machine line when slow_text is not empty, and when
    // `render` (DGROUP:0x928 = 0) IGRAPHIC.DLL #6600 105 through ordinal 32 at (W, H) into the
    // kept scene image DGROUP:0x929a, blitted whole to the screen. Returns the out block's two
    // words when it rendered.
    virtual std::optional<LandingSlots> landing_scene(const VisitLayout& l, const std::string& slow_text,
                                                      bool black, bool render) = 0;
    // seg15:0x3caa(first), the incoming-jet animation; non-zero when it has ended [OPEN: read in
    // summary only]. The visit does not call it while DGROUP:0x928 is set: seg15:0x3caa then
    // returns 1 before anything else [BYTES seg15:0x3cb9-0x3cc3].
    virtual int incoming(int first) = 0;
    // seg15:0x3590 sub-step 0's drawing between seg15:0x1eac and seg15:0x374c(1): the black screen
    // when `black` (DGROUP:0x93a6 = 0), ordinal 14 (3, 0xcc) and (2, 0x89) when `fade`
    // (DGROUP:0x928 = 0), then state 6's palette sequence around seg15:0x482e (the pad)
    // (s6-pad-verify.md claim 16, CORRECTED) [BYTES seg15:0x35d8-0x36b1].
    virtual void descent_start(const VisitLayout& l, bool black, bool fade) = 0;
    // seg15:0x374c's frame: the ship at y over the pad, its source row picked by the flag
    // DGROUP:0x17fc (DGROUP:0x3ce8 + 2·flag) [BYTES seg15:0x3b2b-0x3b31] [OPEN: the drawing is
    // summarised].
    virtual void descent_frame(const VisitLayout& l, int y, int s4, int flag) = 0;
    // seg16:0x2144, the whole visit screen (panels, monitors, labels, bars, the pad strip).
    virtual void redraw(const VisitLayout& l, int phase, int saved_phase, bool left_on) = 0;
    // Phase 2: the ship at (x, y), s4 x s4, SRCAND / SRCPAINT from the ship's DCs, and the pad
    // strip DGROUP:0x953c saved from the screen [BYTES seg16:0xdc1-0xf32].
    virtual void ship_on_pad(const VisitLayout& l, int x, int y, int s4, int strip_h) = 0;
    // seg15:0x4c76's strip at the pad's place, the ship lowered by `offset` pixels.
    virtual void pad_strip(const VisitLayout& l, int offset, int s4) = 0;
    // seg15:0x4c76(first, down)'s drawing at DGROUP:0x181c = offset [BYTES seg15:0x4c76-0x52d8]
    // (the PAD track of M4 stage 6); the default draws pad_strip [DESIGN: the fakes].
    virtual void pad_move(const VisitLayout& l, int first, int down, int offset, int s4) {
        (void)first, (void)down;
        pad_strip(l, offset, s4);
    }
    // seg15:0x4c76(1, 0) with the new body: the ship image rebuilt (seg15:0x2a08) [OPEN M8].
    virtual void rebuild_ship() = 0;
    // seg15:0x4c76's own s4 on every call: GetObject(DGROUP:0x92b0) and 4·trunc(bmHeight/k), k = 4
    // below 8 bpp else 2, after the new body's rebuild; the pad move's bound DGROUP:0x181c's
    // [bp-0x26] = s4 + 0x3e and the strip's blit height and y read it [BYTES seg15:0x4cec-0x4d1c
    // seg15:0x4d99-0x4db1]. `roster_s4` (phase 0's s4) when the screen has no body image [DESIGN:
    // the fakes].
    virtual int pad_s4(int roster_s4) { return roster_s4; }
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
    // DGROUP:0x10, the client height that seg16:0x1f78 reads (phase 0's layout); 0 takes the
    // play height DGROUP:0x90ec for it [DESIGN: the fakes] [BYTES seg16:0x1fbc seg16:0x1ff5].
    virtual int client_height() { return 0; }
    virtual bool limited() = 0;                // DGROUP:0x920c = 0
    // DialogBox(0xc2, ..., seg16:0x4104), modal; its result is ignored (verified claim 28).
    virtual void options_dialog() = 0;
    // seg10:0x82a2(6): the New Ship Body chooser of a dead ship [OPEN M8].
    virtual void body_chooser() = 0;
    // Phase 0's calls: seg10:0xa856 (the image-slot DCs freed), seg11:0x2f0 (the HUD windows),
    // seg13:0x65f2(3, 0, 0) and seg10:0x7480(-2, 1) of the scene.
    virtual void enter_calls() = 0;
    // seg7:0x158e, the wave's entry/vanish step (world/entry_effect.h entry_step), which
    // seg16:0x2144 calls 120 times while the back buffer DGROUP:0x9580 exists and DGROUP:0x93a6 = 0
    // [BYTES seg16:0x2764-0x2775] (review H3). The default does nothing [DESIGN: the fakes].
    virtual void entry_step() {}
    // seg10:0x6dd4(0), seg10:0x6e76(8, 0): the palette back for the wave.
    virtual void palette_back() = 0;
    // Phase 61's seg15:0xcb0 in state 6 (src/desk/desk.h Desk::enter_visit): the play area, the
    // pad's DGROUP:0x3ce6 and DGROUP:0x3c9a from `l` and DGROUP:0x92d6 = `end_y` (the descent's
    // end y) [BYTES seg16:0x1b77 seg15:0xf1a].
    virtual void launch_enter(const VisitLayout& l, int end_y) = 0;
    // Phase 63's seg15:0x10be (Desk::frame); true when step 9 ran, which raises DGROUP:0x914a in
    // state 6 [BYTES seg16:0x1b7f seg15:0x12e5-0x12ec].
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
// seg16:0x1f78's words at seg15:0x374c(1) in state 6 [BYTES seg15:0x3859 seg16:0x2052
// seg16:0x2056]: DGROUP:0x952c = seg16:0x619e() (DGROUP:0x9530 = 3, DGROUP:0x958a = 1) when
// DGROUP:0x93a6 = 0, then seg16:0x2144's tail clears DGROUP:0x958a (s6-pacing.md plan item 4).
void visit_panels(VisitCtx& c);
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
// DGROUP:0x9530 = 1) and the slow phase (DGROUP:0x914a = 1, DGROUP:0x92a2 = 0); no other phase
// changes the pace (ruling R1 of M4 stage 6) [BYTES seg7:0x126 seg7:0x2241-0x2267].
bool visit_pace_gate(const VisitState& v);
bool visit_pace_slow(const VisitState& v);

}  // namespace is::world
