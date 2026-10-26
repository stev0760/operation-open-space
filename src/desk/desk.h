// The desk and launch scene: state 0 phase 6 (docs/re/shell.md "Game-start dialogs" items
// 6-10). The entry seg15:0xcb0, the frame seg15:0x10be with its steps 0-9, the monitor text
// seg15:0x52e4 with the formatter seg10:0xa332, the typing routine seg15:0x1584, the rise
// seg15:0x4c76 and the launch seg15:0x0 with its ordinal-14 fades.
//
// The logic is portable: it draws through Canvas (the GDI work on DGROUP:0x13dc and the scene
// DC DGROUP:0x929a, src/desk/gdi_desk.h on Win32) and reaches the program through Host. The
// intro (src/intro) owns the phase and its 90 s deadline and hands the scene its frames
// through intro::DeskScene (src/desk/hooks.cpp).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "desk/scene.h"
#include "shell/shared_rules.h"

namespace is::desk {

using Tick = std::uint32_t;
using Colour = std::uint32_t;  // a COLORREF as the original hands it to GDI

// localtime as seg10:0x9764 reads it.
struct Clock {
    int wday = 0;  // tm_wday, 0 = Sunday
    int hour = 0;
    int minute = 0;
};

class Canvas {
public:
    virtual ~Canvas() = default;
    // ---- palette (the seg10 wrappers of ordinal 4 and ordinal 14)
    virtual void palette_save(int set, int blank) = 0;    // seg10:0x6da6
    virtual void palette_scheme(int c, int mode) = 0;     // seg10:0x6e76
    virtual void palette_restore(int set) = 0;            // seg10:0x6dd4
    virtual void palette_fade(int mode, int percent) = 0; // seg10:0x993a
    // ---- the scene DC DGROUP:0x929a and its bitmap DGROUP:0x9294
    virtual void scene(const Render& r) = 0;              // ordinal 32 through seg10:0x9526
    virtual void monitor(const Rect& slot0) = 0;          // seg15:0x8604
    virtual void tower(const Rect& slot1) = 0;            // seg15:0x8ae6
    virtual void meal(int graphic, const Rect& slot4) = 0;  // seg15:0xf2e's drawing part
    virtual void scene_to_screen() = 0;                   // BitBlt of the whole scene
    // BitBlt SRCCOPY between the screen and the scene DC at the same rectangle.
    virtual void screen_to_scene(int x, int y, int w, int h) = 0;
    virtual void scene_to_screen(int x, int y, int w, int h) = 0;
    // ---- the screen DGROUP:0x13dc
    virtual void black(int x, int y, int w, int h) = 0;   // PatBlt BLACKNESS
    virtual void block(int x, int y, int w, int h, Colour brush) = 0;  // PatBlt PATCOPY
    // CreateFont(h, w, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0x22, "Arial"), background 0, OPAQUE,
    // TA_LEFT, colour 0x0000FF00 [BYTES seg15:0x15ec seg15:0x1605].
    virtual void text_font(int h, int w) = 0;
    virtual void text_char(int x, int y, char c) = 0;           // TextOut of one character
    virtual int text_extent(const std::string& cp1252) = 0;     // GetTextExtent's cx
    virtual void scroll_up(const Rect& r, int pixels) = 0;      // ScrollDC(0, -1) `pixels` times
    virtual void flash(int x, int y) = 0;                       // seg15:0x84b0's drawing
    // ---- launch [OPEN: seg15:0x4a2e, seg15:0x2a08 and seg15:0x3bfa are approximations]
    virtual void make_pad(int w, int h) = 0;   // seg15:0x482e: DGROUP:0x92a8 in DC 0x92b6
    virtual int make_ship() = 0;               // seg15:0x9e6: returns DGROUP:0x92da
    // seg15:0x4c76's strip: black, the pad at its bottom, the ship at (padW/2 - s4/2, 5), blitted
    // at (x, y).
    virtual void rise(int pad_w, int pad_h, int s4, int x, int y) = 0;
    virtual void free_rise() = 0;              // DGROUP:0x92ac and DGROUP:0x92b8
    virtual void pad_to_screen(int x, int y) = 0;
    // One launch frame: the previous ship rectangle back from the scene DC (and the pad), then
    // frame f (0-15) of size `size` at (x, y). Returns the ship rectangle.
    virtual Rect ship(int f, int size, int x, int y) = 0;
    virtual void free_all() = 0;               // seg15:0x1328(1) and seg15:0x14be
    virtual bool pad_alive() = 0;              // DGROUP:0x92ac still present
};

class Host {
public:
    virtual ~Host() = default;
    virtual Tick now() = 0;                                      // GetCurrentTime
    virtual void sound(int id) = 0;                              // seg10:0x744c
    virtual void sound_at(int id, int x, int y, int arg) = 0;    // seg10:0x754a
    virtual void music(int code) = 0;                            // seg10:0x7480(code, 1)
    virtual void pump() = 0;                                     // seg10:0x740c, once
    virtual void pump_until(Tick t) = 0;  // seg10:0x740c until GetCurrentTime reaches t
    virtual void busy_until(Tick t) = 0;  // a busy-wait on GetCurrentTime (no pumping)
    virtual std::string lang(int id) = 0;                        // ENG_LANG string
    virtual std::string lang_text(int type, int id) = 0;         // ENG_LANG #type id, text
    virtual std::string ini(const char* key, const char* def) = 0;
    virtual int ini_int(const char* key, int def) = 0;
    virtual Clock clock() = 0;
    virtual int rand() = 0;              // the C runtime's rand()
    virtual int calibrate_value() = 0;   // DGROUP:0x8ffc
    virtual Conditions conditions() = 0; // ISPACE.DLL DGROUP:0x22aa, 0x22ab, ...
    virtual Render render(int id, int W, int H) = 0;  // ordinal 32 for IGRAPHIC.DLL #6600 id
    virtual void todo(const std::string& what, const char* address) = 0;
};

// seg10:0x9672: the INI meal window (shell.md "Open questions"): t = hour × 100 + minute + 1,
// first of Lunch → 3, Breakfast → 4, Coffee → 1, Tea → 2 with K < t < K + 30, else 0.
int meal_of(const Clock& c, int lunch, int breakfast, int coffee, int tea);
// seg15:0xf2e's graphic for a meal value (0 for none); `r` is rand().
int meal_graphic(int meal, int r);
// seg10:0x9764(0), shared with the briefing (shell/shared_rules.h).
using shell::day_part;

// seg10:0xa332 for the desk's strings: %< %> groups with an enable flag each, %S a string id,
// %s a string, %N a CR LF; `args` are the varargs in order (ints and strings).
struct Arg {
    int i = 0;
    std::string s;
    bool is_string = false;
    Arg(int v) : i(v) {}
    Arg(std::string v) : s(std::move(v)), is_string(true) {}
    Arg(const char* v) : s(v), is_string(true) {}
};
using Lookup = std::function<std::string(int)>;
std::string format(const std::string& fmt, const std::vector<Arg>& args, const Lookup& lang);

// The scene's text cursor and typing state (seg15:0x1584, DGROUP:0x17e4-0x17ee).
struct Typing {
    std::string text;      // the monitor buffer, upper-cased
    std::size_t index = 0;
    int h = 0, w = 0;      // the font
    int x = 0, y = 0;      // the pen (DGROUP:0x17e4 is y)
    int cur_x = 0, cur_y = 0;  // DGROUP:0x17e6, 0x17e8
    int line = 0;          // DGROUP:0x17ec
    Tick next = 0;         // DGROUP:0x17ee
    bool started = false;
};

class Desk {
public:
    Desk(Canvas& canvas, Host& host) : canvas_(canvas), host_(host) {}

    // seg15:0xcb0 in state 0 with the play area W × H (DGROUP:0x90ea, DGROUP:0x10) and play
    // height DGROUP:0x90ec. The caller plays sound 0x6e and arms the 90 s phase deadline.
    void enter(Tick now, int W, int H, int play_h);
    // seg15:0x10be at DGROUP:0x1e50 = now. True when step 9 ran: the caller then sets phase 7
    // and runs seg6:0x1d9a and seg6:0x1c72(1) (seg15:0x12c3-0x12dd).
    bool frame(Tick now);
    void escape() { step_ = 9; }  // ESC in phase 6 [BYTES seg11:0x7a11]
    // Space at steps 1 [BYTES seg11:0x7b03-0x7b13]: seg15:0x1584(1).
    void space(Tick now);

    int step() const { return step_; }
    int meal() const { return meal_; }
    const Render& render() const { return render_; }
    const Typing& typing() const { return t_; }
    const std::string& text() const { return t_.text; }
    bool built() const { return built_; }
    int ship_y10() const { return pos10_; }
    int frame_index() const { return f_; }

    // seg15:0x52e4: the greeting, the mission text and the launch line (exposed for tests).
    std::string monitor_text();

private:
    void type(int arg);       // seg15:0x1584(arg)
    void newline();
    bool rise(bool first);    // seg15:0x4c76(first, 0); true when it advanced the step
    int launch(int arg);      // seg15:0x0(arg)
    void end();               // step 9

    Canvas& canvas_;
    Host& host_;
    Tick now_ = 0;
    int W_ = 0, H_ = 0, play_h_ = 0;
    int step_ = 9;            // DGROUP:0x92d4
    Tick wait_ = 0;           // DGROUP:0x3cc4
    bool built_ = false;
    Render render_;
    Rect monitor_;            // DGROUP:0x3ca8 (slot 0)
    Rect tower_;              // DGROUP:0x3cb8 (slot 1)
    Rect meal_rect_;          // DGROUP:0x3cd4 (slot 4)
    int desk_bottom_ = 0;     // DGROUP:0x92de
    int screen_h_ = 0;        // DGROUP:0x92d2 = B - T - 15
    int meal_ = 0;            // DGROUP:0x9214
    Typing t_;
    // launch
    int pad_w_ = 0, pad_c_ = 0;   // DGROUP:0x3ce6, DGROUP:0x3c9a
    int size_ = 0;                // DGROUP:0x92da
    int s4_ = 0;
    int rise_off_ = 0;            // DGROUP:0x181c
    bool rise_alive_ = false;
    int start_y_ = 0, end_y_ = 60;  // DGROUP:0x92d8, DGROUP:0x92d6
    int pos10_ = 0;               // DGROUP:0x17c2
    int speed_ = 150;             // DGROUP:0x17c8
    int f_ = 0;                   // DGROUP:0x17ca
    int lum_ = 100;               // DGROUP:0x17d0
    Rect ship_rect_;              // DGROUP:0x17da-0x17e0
    bool logged_rise_ = false;
};

}  // namespace is::desk
