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
#include "shell/shared_rules.h"

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
    // Whether the image slot DGROUP:0x39e8[slot] holds a picture the port can draw.
    virtual bool has_slot(int /*slot*/) { return false; }
    // Whether the program hands the ship sheets (slots 0x28 + i) to the canvas at all. Without
    // that link phase 11 draws the port's stand-in outline instead of skipping the ship as the
    // original does for an empty slot [DESIGN: a canvas with no slot source, as the unit tests'
    // canvases; the game's src/hud/wave_win32.cpp registers it].
    virtual bool ship_sheets_linked() { return false; }
    // SetStretchBltMode(3) and seg10:0x9962(0, dc, x, y, w, h, DGROUP:0x39e8[slot], sx, sy,
    // sw, sh, SRCCOPY).
    virtual void slot_blit(int /*slot*/, int /*x*/, int /*y*/, int /*w*/, int /*h*/, int /*sx*/,
                           int /*sy*/, int /*sw*/, int /*sh*/) {}
};

// The image slots the briefing draws from (DGROUP:0x39e8[n], shell.md "Image slots"): slot 5
// is the logo strip of seg10:0x39a8 (14 cells of 40 x 40 at x 6 + 46k), slot 7 the rocks and
// slot 0x16 the viruses' slot. A ship's sheet is slot 0x28 + ship word +0x50 (DGROUP:0x70e0 +
// 0x15a * ship), the ship's own index (seg14:0xd0a) [BYTES seg17:0x2109-0x211a seg14:0xd0a].
constexpr int kSlotStrip = 5, kSlotRocks = 7, kSlotViruses = 0x16, kSlotShipBase = 0x28;
// The colour of the placeholder outline the port draws where a slot's picture is not loaded
// yet [OPEN: the pools' images come with the wave set-up, M3].
constexpr Colour kPlaceholder = 0x02808080;

class Host : public Strings {
public:
    // seg10:0x744c (id) and seg10:0x754a (id, x, y, 4).
    virtual void sound(int id) = 0;
    // seg10:0x754a(id, x, y, mode); the default stands for hosts without positions (M7 audio
    // rewiring, implementer C).
    virtual void sound_at(int id, int x, int y, int mode) {
        (void)x, (void)y, (void)mode;
        sound(id);
    }
    // seg11:0xb09c(on, ship): the flame cells pasted into the ship's sheet (phase 11's calls,
    // [BYTES seg17:0x22fb seg17:0x25f2 seg17:0x2a32]). The default does nothing.
    virtual void flame(int on, int ship) { (void)on, (void)ship; }
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
// [BYTES seg17:0x1b6d seg17:0x1bc2 seg17:0x1bf9 seg17:0x1bd8]. Other kinds: 0.
int slide_in_frames(int kind);
// seg17:0x1b3e's constants per kind [BYTES seg17:0x1b68-0x1c10]: the image slot, the frame
// count, the cells and the base size, plus the source cell and gutter of the pool record
// DGROUP:0x1df8[kind] + 0x70r (+0x22, +0x24).
struct SlideKind {
    int slot = 0, frames = 0, cells = 0, base = 0;
    int cell = 0, gutter = 0;
};
SlideKind slide_kind(int kind);

// The CRT flash seg15:0x84b0(x, y) [BYTES seg15:0x84b0-0x8603]: sound 0x14 (state 3 is never
// state 0 step 7), a WHITE_PEN cross growing to half-length 19 and its erasure to black.
void crt_flash(Canvas& c, Host& h, int x, int y);

// ---- the load screen of seg10:0x0 (shell.md "Game-start dialogs" item 13, Greeting)

// seg10:0x9764(0), shared with the desk (shell/shared_rules.h).
using shell::day_part;
struct GreetingLine {
    int x = 0, y = 0;
    Colour colour = 0;
    std::string text;  // drawn in bitmap font 0, align 6
};
// seg15:0x6640's three lines for W = DGROUP:0x90ea, H = DGROUP:0x10, the local hour, the
// player DGROUP:0x380c and seg15:0x6558's drive text [BYTES seg15:0x6679-0x6810].
std::vector<GreetingLine> greeting_lines(Strings& s, int W, int H, int hour,
                                         const std::string& player, const std::string& drive);
// The sounds seg15:0x6640 plays: seg10:0x744c(0x6e) is 0x69 then 0x6e, then 0x6f [BYTES
// seg15:0x664e seg15:0x666f seg10:0x745f seg10:0x7465].
constexpr int kGreetingSounds[3] = {0x69, 0x6e, 0x6f};

// ---- the ship page (phase 11)

// The values of phase 11 for the player's ship (the ship records of the placement seg9:0x6624,
// src/world): seg17:0x33ee's 3035 page, seg17:0x4068's rank, seg17:0x3ba6's relation lines and
// the picture's measures. The defaults are start-0001's TRANSFORMER at wave 1.
struct ShipStats {
    long score = 0;      // dword DGROUP:0x1e58
    int captured = 0;    // word DGROUP:0x7162 + 0x15a * DGROUP:0x9646 (ship +0xd2)
    int destroyed = 0;   // word DGROUP:0x7160 + 0x15a * DGROUP:0x9646 (ship +0xd0)
    long credit = 150;   // dword DGROUP:0x716c + 0x15a * ship (+0xdc), in hundredths
    int resources = 0;   // word DGROUP:0x715e + 0x15a * ship (+0xce), in hundredths
    // The rating +0xd8 that seg13:0x33d6 gives with ship word +0x26 = 100 (seg17:0x4068)
    // [BYTES seg17:0x407b-0x4096]. The default is the shipped TRANSFORMER's (test_briefing_rating)
    // [DESIGN].
    int rating = 5900;
    int to_allies = 0;   // signed ship byte +0x5d (DGROUP:0x70ed)
    int to_enemies = 2;  // signed ship byte +0x5e (DGROUP:0x70ee)
    int sheet = 0;       // ship word +0x50: the picture is image slot 0x28 + sheet
    // The class size (roster byte +0x7a, or object +0x22 without a roster record) and the cell
    // side and gap (object +0x22, +0x24) [BYTES seg17:0x22b9-0x22e3 seg17:0x24ca-0x24e2].
    int size = 33, cell = 33, gap = 5;
};
// seg6:0xf54 (dword) / seg6:0x1010 (word) with (v, 100, digits): "-" and the negation for a
// negative v, v / 100 in decimal, ".", then v % 100 in decimal without leading zeros, cut to
// `digits` characters: 150 -> "1.50", 0 -> "0.0", 105 -> "1.5" [BYTES seg6:0xf54-0x100f
// seg6:0x1010-0x10a9 seg6:0x10ce-0x11e9].
std::string hundredths_text(long v, int digits);
// seg17:0x4068's rank string for a rating +0xd8 (signed, strictly greater than): 709 above
// 32600, 710 above 25000, then 711 ... 721 above 100 x the words DGROUP:0x96a down to
// DGROUP:0x956 (143, 114, 97, 80, 70, 63, 55, 48, 40, 34, 28), else 722 [BYTES
// seg17:0x409a-0x41c6 DGROUP:0x956-0x96b].
int rank_string(int rating);
// seg17:0x4068's sentence 3023 "%<YOUR%>%<THIS%> SHIP IS %S." with (ship = player, ship !=
// player, rank) [BYTES seg17:0x41c6-0x41f8]. `rank` is the string id (rank_string).
std::string ship_sentence(Strings& s, bool player, int rank);
// seg17:0x3ba6(0)'s two relation lines for the player: 2528 "%S %? ALLIES AND" with (246 + byte
// +0x5d, byte +0x5d > 2, 541, 542) and 2524 "%S %? ENEMIES. " with (250 + byte +0x5e, byte +0x5e
// > 2, 541, 542) [BYTES seg17:0x3e52-0x4027]. %? takes its second id when the flag is set
// [ORACLE start-0001] [OPEN: seg10:0xa332's %? code is unread].
std::vector<std::string> relation_lines(Strings& s, int to_allies, int to_enemies);
// seg17:0x33ee's page for the player with the help window closed: string 3035 formatted as
// seg10:0xa332 does [BYTES seg17:0x341c-0x347c], two seg17:0x42ea(1) breaks " \r\n" [BYTES
// seg17:0x42ea DGROUP:0xbb5 DGROUP:0xbb7] and the sentence of seg17:0x4068 [BYTES
// seg17:0x3482-0x349a seg17:0x371d-0x3745]. %N is a CR LF [OPEN: the Formatter's %N bytes].
std::string ship_page_text(Strings& s, int wave, const std::string& directory,
                           const ShipStats& st, int rank);

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
    // The label of each record (the 35-byte record DGROUP:0x57b4 + 0x23k, seg17:0xc2e).
    std::vector<std::string> names;
    WaveFlags flags;
    // Phase 11's player ship, and whether the program gave it (else the ShipStats defaults).
    ShipStats player;
    bool ship_known = false;
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
    int race(int init);                         // seg17:0x13ca
    int defenders(int init);                    // seg17:0x193c
    int ship(int init);                         // seg17:0x20fa(init, 0, 0)
    int ship_slot() const;                      // 0x28 + ship word +0x50
    void ship_setup();                          // seg17:0x22b0-0x23ee
    void ship_labels();                         // seg17:0x4354 and seg17:0x3ba6(0)
    void icon_label(int k, int x, int y);       // seg17:0xc2e
    void slide_draw(const SlideKind& k, int x, int y, int s, int sx, int sy, int sw);
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
    // seg17:0x1b3e: DGROUP:0x18b6, 0x18ba, 0x18bc, 0x18be and 0x18c0.
    Tick t18b6_ = 0;
    int n18ba_ = 0, n18bc_ = 0, n18be_ = 0, a18c0_ = 0;
    // seg17:0x13ca and seg17:0x193c: DGROUP:0x18a0, DGROUP:0x18b0 and the pulse DGROUP:0x18b4.
    Tick t18a0_ = 0, t18b0_ = 0;
    int t18b4_ = 0;
    // seg17:0x20fa: DGROUP:0x18c2 and the flight words DGROUP:0x18c6-0x1908.
    struct Flight {
        Tick end_at = 0;            // DGROUP:0x18c2
        long pass = 0;              // DGROUP:0x18c6
        long width0 = 0, width_d = 0;   // DGROUP:0x18ca, DGROUP:0x18ce (%)
        long px10 = 0, end = 0;     // DGROUP:0x18d2, DGROUP:0x18d6
        long frame = 0, frames = 0; // DGROUP:0x18da, DGROUP:0x18de
        long scale0 = 0, scale_d = 0;   // DGROUP:0x18e2, DGROUP:0x18e6 (%)
        long mid = 0, dy = 0;       // DGROUP:0x18ea, DGROUP:0x18ee
        long cell0 = 0, cell_d = 0; // DGROUP:0x18f2, DGROUP:0x18f6 (hundredths)
        long motion = 0;            // DGROUP:0x18fa
        long out = 0;               // DGROUP:0x18fe
        int cx = 0, cy = 0;         // DGROUP:0x1902, DGROUP:0x1904
        int x = 0, y = 0;           // DGROUP:0x1906, DGROUP:0x1908
        int h = 0;                  // the last drawn height
    } fl_;
    int ship_ = 0;  // DGROUP:0x9464
    // The phase routine's context: the phase DGROUP:0x93aa and its sub-step DGROUP:0x9462.
    int cur_phase_ = 0;
    shell::GameState* gs_ = nullptr;
    int sub() const { return gs_ ? gs_->briefing_skip() : 0; }
    void set_sub(int v) {
        if (gs_) gs_->set_briefing_skip(v);
    }
};

}  // namespace is::briefing
