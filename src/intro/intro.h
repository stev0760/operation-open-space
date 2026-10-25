// State 0, the intro, phases 0-5 (docs/re/shell.md "Intro and title screen"): the state-0 entry
// seg15:0x5786, the frame seg15:0x5962 with its phase table, deadlines and transitions, the logo
// (seg15:0x5e22), the title (seg15:0x6cdc), the credits (seg15:0x7b34, seg15:0x7cda and their
// helpers), the text pages (seg15:0x6890), Basic Key Commands (seg15:0x6b52), the repaint rule
// of seg11:0xafe and the key handler seg11:0x7902.
//
// The logic is portable: it draws through Canvas, a thin layer over the GDI calls the original
// makes on DGROUP:0x13dc, and reaches the rest of the program through Host. The Win32 canvas is
// src/intro/gdi_canvas.h; the tests drive the logic with recording fakes. Field comments name
// the DGROUP word each member stands for.
#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "intro/bolt.h"
#include "intro/texts.h"
#include "shell/frame_scheduler.h"
#include "shell/rng.h"

namespace is::intro {

using shell::Tick;
// A COLORREF as the original hands it to GDI: plain RGB (high byte 0) or PALETTERGB (2).
using Colour = std::uint32_t;
// A font handle of the canvas; 0 is none.
using FontId = std::uintptr_t;

// CreateFont(height, width, 0, 0, weight, italic, 0, 0, 0, 0, 0, 2, 0x22, face): the only form
// the intro uses (charset 0, precisions 0, quality 2, VARIABLE_PITCH | FF_SWISS).
struct FontSpec {
    int height = 0;
    int width = 0;
    int weight = 0;
    bool italic = false;
    const char* face = "Arial";
};

// The images the intro draws from.
enum class Image {
    kStrip,        // the logo strip, memory DC DGROUP:0x39f2 (ordinal 2 id -1000)
    kSdLogo,       // SD_LOGO, DGROUP:0x95f4 (monochrome)
    kCredits,      // the credits' logo cells, image slot 0x16 (DC DGROUP:0x3a14)
    kTitle,        // graphic 300, ordinal 1 (DGROUP:0x95be in DC DGROUP:0x95c2)
    kTitleReveal,  // the ordinal-12 picture the title reveals row by row (DGROUP:0x95c2)
};

// The GDI work on the screen DC DGROUP:0x13dc, and the images and palette calls behind it.
class Canvas {
public:
    virtual ~Canvas() = default;

    // ---- images
    virtual void build_strip() = 0;                 // seg10:0x39a8(0), when not built yet
    virtual void load_sd_logo() = 0;                // seg15:0x5914 -> seg10:0x6d38("SD_LOGO")
    virtual void build_credit_cells(int mode) = 0;  // seg10:0x552(0x16, 1), seg10:0x58ce(0)
    virtual void load_title_picture() = 0;          // seg10:0x6c02(300, 0, -1)
    virtual void make_title_reveal() = 0;           // seg10:0x73a6(300, ...): ordinal 12
    virtual void free_image(Image image) = 0;       // DeleteObject of the bitmap

    // ---- drawing on DGROUP:0x13dc
    virtual void black(int x, int y, int w, int h) = 0;  // PatBlt BLACKNESS
    // BitBlt SRCCOPY of (sx, sy, w, h).
    virtual void blit(Image src, int dx, int dy, int w, int h, int sx, int sy) = 0;
    // SetStretchBltMode(COLORONCOLOR) and StretchBlt SRCCOPY.
    virtual void stretch(Image src, int dx, int dy, int dw, int dh, int sx, int sy, int sw,
                         int sh) = 0;
    // seg10:0x9962 with a destination twice the source: an exact pixel doubling.
    virtual void double_copy(Image src, int dx, int dy, int sx, int sy, int sw, int sh) = 0;
    // A solid brush of `brush` selected and BitBlt MERGECOPY (0x00C000CA) of (sx, sy, w, h).
    virtual void merge(Image src, int dx, int dy, int w, int h, int sx, int sy, Colour brush) = 0;
    // CreatePen(PS_SOLID, width, pen), MoveTo(x0, y0), LineTo(x1, y1).
    virtual void line(Colour pen, int width, int x0, int y0, int x1, int y1) = 0;

    // ---- text state (each returns what it replaced, as the GDI calls do)
    virtual FontId create_font(const FontSpec& spec) = 0;
    virtual FontId select_font(FontId font) = 0;
    virtual void delete_font(FontId font) = 0;
    virtual Colour text_colour(Colour c) = 0;
    virtual Colour bk_colour(Colour c) = 0;
    virtual void bk_mode(int mode) = 0;           // 1 TRANSPARENT, 2 OPAQUE
    virtual void text_align(unsigned align) = 0;  // 1 TA_UPDATECP, 6 TA_CENTER | TA_TOP
    virtual void text_out(int x, int y, const std::string& cp1252) = 0;
    virtual void move_to(int x, int y) = 0;
    virtual Point current_position() = 0;
    virtual int text_width(const std::string& cp1252) = 0;  // GetTextExtent's cx
    // ISPACE.DLL ordinal 33 through seg10:0x94c4.
    virtual void font_text(int font, int align, Colour colour, int x, int y,
                           const std::string& cp1252) = 0;

    // ---- palette (ordinal 4 through the seg10 wrappers)
    virtual void palette_scheme(int c, int mode) = 0;  // seg10:0x6e76
    virtual void palette_ramp(int c) = 0;              // seg10:0x6ea0
    virtual void palette_animate() = 0;                // seg10:0x6e4c

    // The end of the pass: the original releases DGROUP:0x13dc after every frame and takes a
    // fresh DC with its defaults for the next (timing.md "Message loop and idle step").
    virtual void release_dc() = 0;
};

// The play-area globals the intro reads.
struct Area {
    int w = 0;       // DGROUP:0x90ea
    int h = 0;       // DGROUP:0x10
    int play_h = 0;  // DGROUP:0x90ec
};

// localtime(time()) as seg10:0x885e reads it.
struct LocalDay {
    int tm_year = 0;
    int tm_yday = 0;
};

// The rest of the program as the intro sees it.
class Host {
public:
    virtual ~Host() = default;
    virtual Tick now() = 0;  // GetCurrentTime
    virtual Area area() = 0;
    // seg10:0x744c / seg10:0x754a (sound), seg10:0x7480 (music), and the actions later stages
    // port: none of them is ported yet.
    virtual void sound(int id) = 0;
    virtual void music(int code) = 0;
    virtual void todo(const std::string& what, const char* address) = 0;
    virtual void show_window() = 0;  // ShowWindow(main, SW_SHOW) [BYTES seg15:0x5840]
    // Calibrate (timing.md "Calibrate", ADR 0001 decision 9).
    virtual void calibrate_start(Tick now) = 0;  // seg15:0x58fb, only while DGROUP:0x1212 = 0
    virtual bool calibrating() = 0;              // DGROUP:0x120e != 0
    virtual bool calibrated() = 0;               // DGROUP:0x1212
    // One frame of the measurement in logo step 0; true when it ended in this call
    // (seg15:0x6009 sets DGROUP:0x1212, seg6:0xa6c runs).
    virtual bool calibrate_step(Tick now) = 0;
    virtual int calibrate_value() = 0;  // DGROUP:0x8ffc
    virtual int mix_channels() = 0;     // DGROUP:0x920a
    // The credits' busy waits on GetCurrentTime: the drawing so far is shown, then the time
    // passes.
    virtual void present_wait(Tick ms) = 0;
    // The demo start (seg15:0x5a64) is not ported: the host restarts state 0.
    virtual void demo_start() = 0;
    virtual LocalDay local_day() = 0;
    // Resources, cp1252: an ENG_LANG.DLL string, an INSPACE.EXE string (DGROUP:0x8e66) and a
    // raw ENG_LANG.DLL resource; empty when missing.
    virtual std::string lang(int id) = 0;
    virtual std::string exe_string(int id) = 0;
    virtual std::vector<std::uint8_t> lang_resource(int type, int id) = 0;
};

// The display depth the intro tests (DGROUP:0x90ee): the port behaves as a 256-colour display
// (ADR 0007 decision 1), so every "8 bpp and above" branch applies and the hint never shows.
constexpr int kDepth = 8;

class Intro {
public:
    Intro(Canvas& canvas, Host& host, shell::Rng& rng);

    // seg15:0x5786, the state-0 entry. `now` is GetCurrentTime, which it also stores in
    // DGROUP:0x1e50.
    void enter(Tick now);
    // seg15:0x5962(redraw) with DGROUP:0x1e50 = now: the tick passes redraw = false.
    void frame(Tick now, bool redraw);
    // seg11:0xafe case 0 with DGROUP:0x1e50 = now, ending with seg15:0x5962(1).
    void repaint(Tick now);
    // seg11:0x7902 with a WM_KEYUP key (the input path already holds back every key before
    // Calibrate ends, shell.md "Input").
    void key_up(std::uint32_t vk, Tick now);

    // ---- state, for the tests and the logs
    int phase() const { return phase_; }
    int step() const { return step_; }
    Tick deadline() const { return deadline_; }
    Tick timer2() const { return timer2_; }
    int spin() const { return spin_; }
    int speed() const { return speed_; }
    int title_rows() const { return rows_; }
    int title_c() const { return c_; }
    int title_d() const { return d_; }
    int page() const { return page_; }
    int credit_entry() const { return entry_; }
    int credit_line() const { return which_; }
    int typed() const { return typed_; }
    Tick next_entry() const { return next_entry_; }
    int pending_bolt() const { return pending_; }
    int trial_day_value() const { return day_; }
    bool attract() const { return attract_; }

private:
    // seg15:0x5962's parts
    void phase_work(bool redraw);
    void transition();
    void title_enter();
    // phase 0
    void logo(bool redraw);
    void logo_cell(int cell);
    void sweep();
    void presents();
    // phase 2
    void title();
    void title_first_frame();
    void emboss(int x, int y, const std::string& text);  // seg15:0x7970
    void registration_check();                            // seg10:0x885e(0)
    // phase 3
    void credits_enter();  // seg15:0x7b34
    void credits();        // seg15:0x7cda
    void credits_logo(int s);
    void load_entry();                  // seg15:0x811a
    int type_line(int which, int c);    // seg15:0x8174
    void emboss_char(const std::string& ch);  // seg15:0x7a62
    void aim_bolt(int x1, int y1, int x2, int y2);  // seg15:0x80b0
    void finish_bolt();                 // seg15:0x80da
    void draw_bolt(int x1, int y1, int x2, int y2);  // seg11:0xa4a6
    void bolt_pass();                   // seg11:0xa9ea(0)
    void bolt_lines(Colour main, Colour branch, bool main_too);
    void clipped_line(Colour pen, int width, Point a, Point b);  // seg12:0x7256
    void erase(int s);
    // phases 4 and 5
    void text_page();  // seg15:0x6890
    void keys();       // seg15:0x6b52

    void sound(int id) { host_.sound(id); }
    void todo_once(const std::string& what, const char* address);

    Canvas& canvas_;
    Host& host_;
    shell::Rng& rng_;
    Area area_;
    Tick now_ = 0;  // DGROUP:0x1e50

    int phase_ = 0;          // DGROUP:0x95b8
    bool attract_ = false;   // DGROUP:0x95bc
    Tick deadline_ = 0;      // DGROUP:0x6e28
    Tick timer2_ = 0;        // DGROUP:0x6e2c: the title scene, the credits' hold and tail
    int step_ = 0;           // DGROUP:0x95cc
    int speed_ = 0;          // DGROUP:0x95d0
    int spin_ = 0;           // DGROUP:0x95d2
    int last_cell_ = 0;      // the logo cell last drawn (for the redraw, see logo())
    // title
    bool scene_off_ = true;  // DGROUP:0x94c4
    int d_ = 0;              // DGROUP:0x95de
    int c_ = 0;              // DGROUP:0x95ce
    int c6_ = 0;             // DGROUP:0x95c6 (sound position only)
    int c8_ = 0, ca_ = 0;    // DGROUP:0x95c8, DGROUP:0x95ca
    int rows_ = 0;           // DGROUP:0x95d4
    std::uint8_t row_map_[18] = {};  // DGROUP:0x95e0
    // edition (stage 3 ports the registration check)
    int edition_ = 0;  // DGROUP:0x920c
    int plus_ = 0;     // DGROUP:0x11aa
    int invalid_ = 0;  // DGROUP:0x922a
    int day_ = 0;      // DGROUP:0x9224
    // credits
    int which_ = 0;          // DGROUP:0x95dc: 0 heading, 1 name (the title clears it too)
    int typed_ = 0;          // DGROUP:0x95da
    int entry_ = 0;          // DGROUP:0x95d8
    Tick next_entry_ = 0;    // DGROUP:0x130c
    int line_width_ = 0;     // DGROUP:0x1c70
    Point bolt_end_;         // DGROUP:0x3b98
    std::string heading_;    // DGROUP:0x87e0
    std::string name_;       // DGROUP:0x881c
    Bolt bolt_;              // DGROUP:0x6b5c, 0x6bbc, 0x6cfc, counts 0x9526 and 0x9528
    int pending_ = 0;        // DGROUP:0x1016
    int flashed_ = 0;        // DGROUP:0x1036
    // text pages
    int page_ = 0;  // DGROUP:0x95d6

    std::set<std::string> logged_;
};

}  // namespace is::intro
