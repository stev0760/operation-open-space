// State 3, the wave mission briefing (docs/re/shell.md "Screen states", State 3): the entry's
// text selection and drawing seg9:0x5cca / seg9:0x8934, the typewriter seg9:0x8d18, the
// 19-phase frame seg17:0x0 with the advance seg17:0x3b2, phase 1 seg17:0x600, the pages
// seg17:0x30b0 and the page typer seg17:0x2c5a. Portable: the drawing goes through Canvas, the
// clock, sounds, strings and resources through Host, and the phase (DGROUP:0x93aa), the Space
// flag (DGROUP:0x9462) and DGROUP:0x5ac live in shell::GameState.
//
// The globals keep the original's names in comments: L DGROUP:0x6084, T DGROUP:0x6086, R
// DGROUP:0x6088, the page bottom DGROUP:0x608a, the panel left DGROUP:0x607c and top
// DGROUP:0x607e.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shell/frame_scheduler.h"
#include "shell/game_state.h"

namespace is::briefing {

using shell::Tick;
using Colour = std::uint32_t;  // a COLORREF as the original passes it (0x02 = PALETTERGB)

// ---- seg9:0x8934's rectangles [BYTES seg9:0x8943-0x8a25 seg9:0x8b85-0x8be1]
struct Box {
    int L = 0, R = 0;      // DGROUP:0x6084 (= DGROUP:0x607c), DGROUP:0x6088 (= DGROUP:0x6080)
    int T = 0;             // DGROUP:0x6086, raised to 15 when H > 400
    int Y = 0;             // DGROUP:0x607e = T + bh - 180 with the unclamped T
    int bottom = 0;        // DGROUP:0x6082 = Y + 180
    int cL = 0, cR = 0;    // max(L, 0), min(R, W)
    // The page after the shrink: (L + 6, T + 11) to (R - 6, Y + 9), the clip region
    // DGROUP:0x93ae.
    int pl = 0, pt = 0, pr = 0, pb = 0;
};
// W = DGROUP:0x90ea, H = DGROUP:0x10.
Box box_for(int W, int H);

// The bevels seg11:0x1774(width, l, t, r, b) of the three bars [BYTES seg9:0x8a29-0x8a92].
struct Bevel {
    int width, l, t, r, b;
};
std::vector<Bevel> box_bevels(const Box& b);

// A label of seg9:0x8bec [BYTES seg9:0x8a95-0x8b82].
struct Label {
    int x, y, height, width;
    unsigned align;  // TA_LEFT 0, TA_RIGHT 2
    std::string text;
};

// The flags the entry and the phases test. They come from the wave set-up and the save
// loader (M3); a new game has them all 0.
struct WaveFlags {
    int f92e8 = 0;  // DGROUP:0x92e8: text #5000 102
    int f94e = 0;   // DGROUP:0x94e: text 103, phases 2 and 5
    int f950 = 0;   // DGROUP:0x950: text 104
    int f92f2 = 0;  // DGROUP:0x92f2: phase 4
    int f92fe = 0;  // DGROUP:0x92fe: phase 6
    int f92f0 = 0;  // DGROUP:0x92f0: phase 7
    int f4ee0 = 0;  // DGROUP:0x4ee0: phase 8
    int f4ee2 = 0;  // DGROUP:0x4ee2: phase 9
};

// seg9:0x5cca's text: the #5000 id (102, 103, 104 or 101), or 0 for the generated text
// seg9:0x91b0. `wave` is DGROUP:0x91f8 before the entry's increment [BYTES seg9:0x5ee7-0x5f46].
int text_resource(const WaveFlags& f, int wave);

// The inputs of seg9:0x91b0 (M3 globals, shell.md "Generated text").
struct GeneratedInputs {
    int kind = 0;          // DGROUP:0x9320
    int byte53 = 0;        // byte +0x53 of *DGROUP:0x1df8
    int v933a = 0;         // DGROUP:0x933a
    int v933c = 0;         // DGROUP:0x933c
    int v933e = 0;         // DGROUP:0x933e
    int v9340 = 0;         // DGROUP:0x9340
    int v4ee0 = 0, v4ee2 = 0, v4ee4 = 0, v4ee6 = 0;
    int v9318 = 0;         // DGROUP:0x9318
    int roster_count = 0;  // step 6's count of roster ships
};
// One %D argument (type, arg) with code 7.
struct Slot {
    int type, arg;
};
// seg9:0x91b0's plan: the string ids and slots in order (shell.md "Generated text" steps 1-6).
struct GeneratedPlan {
    int lead = 0;           // step 1's string id, 0 for none
    bool lead_space = false;
    int body = 0;           // 1189 or 3036
    std::vector<Slot> slots;  // for 3036, in the order 2, 1, 0
    int roster = 0;         // the 3037 count, 0 for none
};
GeneratedPlan generated_plan(const GeneratedInputs& in);

// Strings and resources for the texts.
class Strings {
public:
    virtual ~Strings() = default;
    virtual std::string lang(int id) = 0;  // ENG_LANG.DLL string, cp1252
};

// seg10:0xa332's %# (seg10:0xa558): n, a space, and string id for n = 1 or id + 10000
// otherwise [BYTES seg10:0xa558 seg10:0xa573 seg10:0xa5d5 seg10:0xa5e7].
std::string count_text(Strings& s, int n, int id);
// %D (seg10:0xa754) with code 7 for types 3 and 9 (10153 "Rocks", 10170 "Viruses"),
// upper-cased by seg3:0x1a1d. Type 8 names a ship class from M3 tables: empty, and `todo` is
// set [OPEN].
std::string d_text(Strings& s, const Slot& slot, bool* todo);
// The generated briefing text from the plan, NUL-free. The %< %> groups of 3036 are joined
// with ", " and string 797 " AND" before the last, as the Formatter joins consecutive enabled
// groups [OPEN: the join rule's exact spacing].
std::string generated_text(Strings& s, const GeneratedPlan& p, bool* todo);

// The four labels of seg9:0x8934 for the wave DGROUP:0x91f8 (after the increment), the
// directory DGROUP:0x38c8 and the count DGROUP:0x91fe.
std::vector<Label> box_labels(Strings& s, const Box& b, int wave, const std::string& directory,
                              int count);

// ---- drawing
struct Font {
    int height = 0, width = 0, weight = 0;
};
// TA_* values.
constexpr unsigned kTaLeft = 0, kTaRight = 2, kTaCenter = 6, kTaBaselineCenter = 6;

class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void black(int x, int y, int w, int h) = 0;                // PatBlt BLACKNESS
    virtual void fill(int x, int y, int w, int h, Colour brush) = 0;  // solid brush PATCOPY
    // MoveTo(x0, y0), LineTo(x1, y1) with a 1-px solid pen (the stock BLACK_PEN and WHITE_PEN
    // are 0x000000 and 0xFFFFFF).
    virtual void line(Colour pen, int x0, int y0, int x1, int y1) = 0;
    // Rectangle with a 1-px pen and NULL_BRUSH, or LTGRAY_BRUSH when `ltgray`.
    virtual void rectangle(Colour pen, bool ltgray, int l, int t, int r, int b) = 0;
    virtual void set_pixel(int x, int y, Colour c) = 0;
    // CreateFont(h, w, 0, 0, weight, 0, 0, 0, 0, 0, 0, 2, 0x22, "Arial"), background 0,
    // TRANSPARENT, SetTextAlign(align), SetTextColor(c), TextOut.
    virtual void text(const Font& f, Colour c, unsigned align, int x, int y,
                      const std::string& cp1252) = 0;
    // GetTextExtent's width in that font.
    virtual int text_width(const Font& f, const std::string& cp1252) = 0;
    // SelectClipRgn(DGROUP:0x93ae) for the rest of the frame (on), or no clip (off).
    virtual void clip(bool on, int l, int t, int r, int b) = 0;
    // seg10:0x9962(0, dc, x, y, w, h, DGROUP:0x3a02, sx, sy, sw, sh, SRCCOPY): a stretched
    // copy from the icon cells.
    virtual void icon(int x, int y, int w, int h, int sx, int sy, int sw, int sh) = 0;
};

class Host : public Strings {
public:
    // seg10:0x744c (id) and seg10:0x754a (id, x, y, 4).
    virtual void sound(int id) = 0;
    // An unimplemented path, once per address is the host's choice.
    virtual void todo(const std::string& what, const char* address) = 0;
    // ENG_LANG.DLL #5000 id; empty when absent.
    virtual std::vector<std::uint8_t> page(int id) = 0;
};

// seg11:0x1774(width, l, t, r, b) [BYTES seg11:0x1774-0x1a7b].
void draw_bevel(Canvas& c, int width, int l, int t, int r, int b);
// seg9:0x8bec(hdc, x, y, height, width, align, text) [BYTES seg9:0x8bec-0x8d17].
void draw_label(Canvas& c, const Label& l);

// The typewriter seg9:0x8d18 [BYTES seg9:0x8d26 seg9:0x8fc4 seg9:0x90e7].
class Typewriter {
public:
    Typewriter(Canvas& c, Host& h) : c_(c), h_(h) {}
    // The text (cp1252, cut at its NUL) and the panel: DGROUP:0x607c, DGROUP:0x607e,
    // DGROUP:0x6080, DGROUP:0x6082 and the wrap edge DGROUP:0x6088.
    void set_text(std::string text) { text_ = std::move(text); }
    void set_panel(int left, int top, int right, int bottom, int edge) {
        left_ = left, top_ = top, right_ = right, bottom_ = bottom, edge_ = edge;
    }
    // seg9:0x8d18(reset, instant): returns 'done' (DGROUP:0xa5a).
    int step(Tick now, int reset, int instant);
    bool done() const { return done_ != 0; }
    int index() const { return index_; }
    int x() const { return x_; }
    int y() const { return y_; }

    static constexpr Font kFont{17, 11, 520};
    static constexpr Colour kColour = 0x0000FFFF;

private:
    char at(int i) const {
        return i >= 0 && i < static_cast<int>(text_.size()) ? text_[i] : '\0';
    }
    Canvas& c_;
    Host& h_;
    std::string text_;
    int left_ = 0, top_ = 0, right_ = 0, bottom_ = 0, edge_ = 0;
    int x_ = 0, y_ = 0;          // DGROUP:0x1830, DGROUP:0x1832
    int cx_ = 0, cy_ = 0;        // DGROUP:0x1834, DGROUP:0x1836
    int index_ = 0;              // DGROUP:0x1838
    Tick gate_ = 0;              // DGROUP:0x183c
    int done_ = 1;               // DGROUP:0xa5a starts at 1 [BYTES DGROUP:0xa5a]
    int blink_ = 0;              // DGROUP:0xa5c
};

// The page typer seg17:0x2c5a [BYTES seg17:0x2c5a-0x306d].
class PageTyper {
public:
    PageTyper(Canvas& c, Host& h) : c_(c), h_(h) {}
    // seg17:0x30b0's block DGROUP:0x6078 (2000 bytes, zero-filled), its word-wrap flag
    // DGROUP:0x9478 and hold DGROUP:0x6090.
    void set_page(std::string text, bool wrap, Tick hold) {
        text_ = std::move(text);
        wrap_ = wrap;
        hold_ = hold;
    }
    // The origin DGROUP:0x5000 / DGROUP:0x5002 and the wrap edge DGROUP:0x6088.
    void set_origin(int x, int y, int edge) { ox_ = x, oy_ = y, edge_ = edge; }
    // seg17:0x2c5a(reset, instant) in phase `phase` (DGROUP:0x93aa): 0 before the gate, 1
    // while typing or holding, 2 once the hold has passed.
    int step(Tick now, int reset, int instant, int phase);
    bool done() const { return done_ != 0; }

    static constexpr Font kFont{13, 6, 0};
    static constexpr Colour kColour = 0x00FFFF00;

private:
    char at(int i) const {
        return i >= 0 && i < static_cast<int>(text_.size()) ? text_[i] : '\0';
    }
    Canvas& c_;
    Host& h_;
    std::string text_;
    bool wrap_ = false;
    Tick hold_ = 0;
    int ox_ = 0, oy_ = 0, edge_ = 0;
    int x_ = 0, y_ = 0;    // DGROUP:0x190a, DGROUP:0x190c
    int cx_ = 0, cy_ = 0;  // DGROUP:0x190e
    int index_ = 0;        // DGROUP:0x1912
    Tick gate_ = 0;        // DGROUP:0x1916
    Tick until_ = 0;       // DGROUP:0x191a
    int done_ = 0;         // DGROUP:0xba2
};

// seg17:0x30b0's #5000 id for kind `kind` in phase `phase` (state 3), or 0 for the generated
// kinds 2, 8, 9, 11, 12, 16 and 406 [BYTES seg17:0x30b0-0x32dd]. `meal` is DGROUP:0x9214,
// `ship_page` DGROUP:0x9468.
int page_resource(int kind, int meal, int ship_page);
// DGROUP:0x9478 for phase p: 1 for phases 2, 3, 8-13 and 15-17 [BYTES seg17:0x30d7].
bool page_wraps(int phase);

// The phases' frame counts of the object slide-in seg17:0x1b3e for kinds 3, 7, 8 and 9
// (shell.md "Open questions"). Other kinds: 0.
int slide_in_frames(int kind);

// What the entry gets from the program.
struct Setup {
    int W = 0, H = 0;         // DGROUP:0x90ea, DGROUP:0x10
    int quick_intro = 0;      // DGROUP:0x8f70
    int command_line = 0;     // DGROUP:0x90f8 (DEMO or PLAY)
    int play_mode = 0;        // DGROUP:0x9134
    int size_level = 1;       // DGROUP:0x9156
    std::string directory;    // DGROUP:0x38c8
    int icons = 0;            // DGROUP:0x91fe, the icon records
    std::vector<bool> gone;   // word +0x18 of each record != 0 (captured or destroyed)
    WaveFlags flags;
};

// The briefing: entry drawing, frame and repaint.
class Briefing {
public:
    Briefing(Canvas& c, Host& h) : c_(c), h_(h), typer_(c, h), pages_(c, h) {}

    // DGROUP:0x91f8, the wave: the entry increments it (seg9:0x5fbf).
    int wave() const { return wave_; }
    void set_wave(int w) { wave_ = w; }

    // seg9:0x5cca after its window work: picks the text (with the wave before the
    // increment), increments the wave and runs seg9:0x8934(1). Returns the #5000 id used, or
    // 0 for the generated text.
    int enter(shell::GameState& s, const Setup& setup);
    // The entry with a text in hand (tests, and the generated text).
    void enter_text(shell::GameState& s, const Setup& setup, std::string text);
    // seg9:0x8934(arg): the box, bevels, labels and clip, phase 0 and seg17:0x0(arg == 0, 1).
    void draw_box(shell::GameState& s, int arg);
    // seg11:0xafe case 3: seg9:0x8934(0) unless the phase is 18 [BYTES seg11:0x169f].
    void repaint(shell::GameState& s);
    // The tick's frame: seg17:0x0(0, 0).
    void frame(shell::GameState& s) { run(s, 0, 0); }

    const Box& box() const { return box_; }
    const Typewriter& typewriter() const { return typer_; }
    const PageTyper& page_typer() const { return pages_; }
    // Phase 1's state, for tests: DGROUP:0x1880, DGROUP:0x1882, DGROUP:0x187a.
    int icon_index() const { return i1880_; }
    int icon_size() const { return i1882_; }
    Tick phase1_end() const { return t187a_; }
    Tick next_advance() const { return t1876_; }

private:
    // seg17:0x0(a, b).
    void run(shell::GameState& s, int a, int b);
    // seg17:0x3b2: the advance's work after the increment.
    void advance(shell::GameState& s);
    // One phase routine of the table at seg17:0x355 (init 0) or seg17:0x5d1 (init 1); returns
    // non-zero to advance.
    int routine(shell::GameState& s, int phase, int init);
    int phase1(shell::GameState& s, int init);  // seg17:0x600
    int slide_in(int init, int kind);           // seg17:0x1b3e
    int held(int init, const char* what, const char* address);
    // seg17:0x30b0(kind, hold).
    void load_page(int phase, int kind, Tick hold);
    void black_page();
    int slot() const { return icons_left_ < 7 ? 80 : 70; }

    Canvas& c_;
    Host& h_;
    Typewriter typer_;
    PageTyper pages_;
    Setup setup_;
    Box box_;
    std::string text_;
    int wave_ = 0;
    int icons_left_ = 0;  // DGROUP:0x91fc
    Tick now_ = 0;
    Tick t1876_ = 0;  // DGROUP:0x1876
    // Phase 1: DGROUP:0x187a, 0x187e, 0x1880, 0x1882, 0x1884; DGROUP:0x9462 is its sub-step.
    Tick t187a_ = 0;
    int i187e_ = 0, i1880_ = 0, i1882_ = 0, i1884_ = 0;
    int pages_ox_ = 0, pages_oy_ = 0;  // DGROUP:0x5000, DGROUP:0x5002
    int frames_left_ = 0;  // the slide-in's frame count [DESIGN: see slide_in]
};

}  // namespace is::briefing
