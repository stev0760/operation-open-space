// State 8, the Hall of Justice (docs/re/law.md "The Hall of Justice (state 8)";
// work/analysis/m5/s4-hall.md sections 1-4 with the verdicts and omissions of s4-hall-verify.md,
// which win where the two differ; M5 stage 4 part B, track HALL):
//   - the way in seg9:0x6c62-0x6cb3 (world/wave_end.cpp) and the phase machine seg16:0x6928(arg)
//     with its eleven phases on DGROUP:0x929c (Hall::frame);
//   - the landing in state 8: seg15:0x302e (scene 110), seg15:0x3590 / seg15:0x374c (the jet, the
//     two ship frames, the descent onto the disc) and seg15:0x760 (scene 115 and the monitor);
//   - the typewriter seg15:0x1584 in state 8 (the phase rises at the CR-space pairs and the NUL);
//   - the text and the punishment seg16:0x6afe (hall_text), with its helpers seg17:0x7ba6,
//     seg6:0x1010, seg6:0xf54, seg10:0x97ce, seg17:0x8e60 and seg16:0x71a2;
//   - the launch seg15:0xcb0, seg15:0x10be steps 5-9 and seg15:0x0 in state 8.
// Portable: the pictures go through Screen (src/hall/hall_gdi.h on Win32) and desk::Canvas (the
// scene DC DGROUP:0x929a, the typer's glyphs and the launch frames: desk::GdiDesk), the program
// through Host (src/hall/hooks.cpp). The original's widths: 16-bit words, the fine and the credit
// signed 32-bit dwords, every `rand()` from the one generator in the original's order (ruling R1).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "desk/desk.h"
#include "shell/rng.h"

namespace is::world {
class World;
struct ShipRecord;
}  // namespace is::world

namespace is::hall {

using Tick = std::uint32_t;
using Lang = std::function<std::string(int)>;

// ---- the text's helpers ----------------------------------------------------------------------
// DGROUP:0x116a, the crime weights 0 ... 9 (signed words) [BYTES DGROUP:0x116a].
constexpr std::array<std::int16_t, 10> kCrimeWeights{700, 400, 550, 550, 500, 300, 300, 450, 500, 0};

// seg17:0x7ba6(n, buf): n in decimal (seg6:0xf1a) and 2095 "ST" for n % 10 = 1 and n != 11, 2096
// "ND" for 2 and != 12, 2097 "RD" for 3 and != 13, else 2098 "TH"; the remainder is the signed
// 16-bit `cwd; idiv` [BYTES seg17:0x7bb7-0x7c1d].
std::string ordinal(std::int16_t n, const Lang& lang);
// seg6:0x1010(v, 100, 2, buf): "-" (DGROUP:0x491) and -v for v < 0, v / 100, "." (DGROUP:0x493)
// and v % 100 in decimal without padding, cut at 2 characters [BYTES seg6:0x101e-0x10a2
// seg6:0x10ce-0x11e2]: 150 "1.50", 105 "1.5", 100 "1.0".
std::string fixed_word(std::int16_t v);
// seg6:0xf54(v, 100, 2, buf): the same for a dword (DGROUP:0x48d "-", DGROUP:0x48f ".") [BYTES
// seg6:0xf63-0x1007]: 0 "0.0".
std::string fixed_dword(std::int32_t v);

// localtime's fields as seg10:0x97ce reads them: tm_hour, tm_min, tm_mon (0-11), tm_mday and
// tm_year (years since 1900).
struct Clock {
    int hour = 0, minute = 0, mon = 0, mday = 1, year = 0;
};
// seg10:0x97ce(buf) [BYTES seg10:0x97db-0x992e]: h:MM then "PM" for tm_hour > 11 ("AM" else, no
// space), hour 0 and 12 as 12, " ", tm_mon + 1, "/", tm_mday, "/" and tm_year raw (pitfalls.md:
// 2026 reads 126).
std::string hall_clock(const Clock& t);

// seg17:0x8e60(i, buf) [BYTES seg17:0x8e60-0x8f1c]: ship i's roster name (+0xd6 >= 0, signed) or
// string 788 "Officer", " " (DGROUP:0xced) and lo16(2·lo16(lo16(lo16((2i + 3)(c + 2))(i + 6))(i +
// 7))) / (c + 3) + 956 (16-bit `imul`, CWD, `idiv`), c = sbyte +0x5e [BYTES seg17:0x8e98-0x8f0f].
std::string officer_name(const world::World& w, int i, const Lang& lang);

// seg16:0x71a2(s, k) [BYTES seg16:0x71af-0x7205]: w = sext(byte +0xb8 + k); bytes +0xb8 + k and
// +0xae + k = 0; for j = k ... 8 +0xb8 + j = +0xb8 + j + 1 and +0xae + j = +0xae + j + 1; slot 9 of
// both = 0; byte +0xcb -= 1; returns w. The selected-slot word +0x18 is not touched.
int confiscate(world::ShipRecord& s, int k);

// ---- the punishment (section 3) [BYTES seg16:0x6ebf-0x6f94] -----------------------------------
struct Sentence {
    std::int16_t total = 0;      // [bp-0xd0], sum of the signed bytes DGROUP:0xa14d
    std::int16_t served = 0;     // [bp-0xd6], sum of the signed bytes DGROUP:0xa157
    std::int32_t severity = 0;   // [bp-0xda] after the charge loop
    std::int32_t fine = 0;       // [bp-0xda] after the punishment
    bool confiscate = false;     // seg16:0x71a2(P, 1) runs, the fine 0
};
// The charge loop's sum [BYTES seg16:0x6dec-0x6e1a]: for each c with d = sbyte a14d[c] - sbyte
// a157[c] != 0 (16-bit), S += sext(w_c) and, for d > 1 (signed), S += sext16(lo16((w_c / 5)·(d -
// 1))).
std::int32_t severity(const std::array<std::uint8_t, 10>& a14d, const std::array<std::uint8_t, 10>& a157);
// The punishment on S, the served sum, the credit (after the resources merge) and the signed slot
// count +0xcb: a first offence (served 0) with S > 0 pays max(S / 3, 100), lowered to a positive
// credit; then with credit + 100 < S (signed 32) and S != 0 a ship of more than one slot pays 0
// and loses slot 1, else S / 2 (32-bit signed, toward 0).
Sentence punish(std::int32_t S, std::int16_t served, std::int32_t credit, std::int8_t slots);

// What seg16:0x6afe reads besides the World: the pilot DGROUP:0x380c, the clock, DGROUP:0xa162
// (law.h LawWords::ba162, the arresting ship) and the formatter's strings.
struct TextInputs {
    std::string pilot;
    Clock now;
    std::uint8_t a162 = 0;
};
struct HallText {
    std::string text;  // the 2000-byte block DGROUP:0x93a8 / far DGROUP:0x4fcc, upper-cased
    Sentence s;
    int conf = 0;      // [bp-0xd2]: the confiscated weapon, or ADR 0016's 0
    bool conf_unset = false;  // the fine is 0 with no confiscation: the stale stack word
    bool drew = false;        // the one rand() of seg16:0x7139 was drawn
};
// seg16:0x6afe without its first and last steps (DGROUP:0x92d4 = 0, the GlobalAlloc, the
// typewriter's init): the text paragraph by paragraph and its writes on the World: the player's
// credit +0xdc += sext(+0xce), +0xce = 0, the punishment, the confiscation, DGROUP:0xa157 =
// DGROUP:0xa14d [BYTES seg16:0x6b2e-0x7190].
HallText hall_text(world::World& w, const TextInputs& in, const Lang& lang, shell::Rng& rng);

// ---- the state-8 DGROUP words ----------------------------------------------------------------
// Words state 8 shares with states 0 and 6 in the original (DGROUP:0x928, 0x92a2, 0x92d6, 0x92d8,
// 0x17f2 ... 0x17fc) are world/visit.h VisitState's, their one home [DESIGN]; the rest are here.
struct Words {
    std::int16_t phase = 0;   // DGROUP:0x929c
    std::uint32_t hold = 0;   // dword DGROUP:0x1c64, phase 6's 4000 ms deadline
    std::int16_t done = 0;    // DGROUP:0x92a, the typewriter's end flag
    std::int16_t step = 9;    // DGROUP:0x92d4, the typewriter's init test and the launch's step
    desk::Rect monitor;       // DGROUP:0x3ca8 ... 0x3cae (slot 0 of scene 110 or 115)
    int mon_x = 0, mon_y = 0; // DGROUP:0x3ca4 / 0x3ca6 = DGROUP:0x3d48 / 0x3d4a (scene 115 words 0, 1)
    int w92ba = 0;            // DGROUP:0x92ba, scene 115 word 5 (the disc)
    int w92de = 0;            // DGROUP:0x92de
    int w92d2 = 0;            // DGROUP:0x92d2 = monitor bottom - top - 15 (seg15:0x8604)
    // seg15:0x1584's words.
    std::string text;         // the block DGROUP:0x4fcc
    std::size_t index = 0;    // DGROUP:0x17ea
    int line = 0;             // DGROUP:0x17ec
    int x = 0, y = 0;         // DGROUP:0x17e2, DGROUP:0x17e4
    int cur_x = 0, cur_y = 0; // DGROUP:0x17e6, DGROUP:0x17e8
    std::uint32_t next = 0;   // dword DGROUP:0x17ee
    // seg15:0x0's words (the launch).
    int size = 0;             // DGROUP:0x92da (s of seg15:0x9e6)
    int pos10 = 0;            // DGROUP:0x17c2
    int speed = 150;          // DGROUP:0x17c8
    int f = 0;                // DGROUP:0x17ca
    int lum = 100;            // DGROUP:0x17d0
    int prev_top = 0;         // DGROUP:0x17cc
    desk::Rect ship;          // DGROUP:0x17da ... 0x17e0
    std::uint32_t wait = 0;   // dword DGROUP:0x3cc4
    bool scene = false;       // DGROUP:0x929a != 0, the scene DC
};

// The drawing of the landing and the Hall's scene (src/hall/hall_gdi.h).
class Screen {
public:
    virtual ~Screen() = default;
    // seg15:0x302e's drawing in state 8 [BYTES seg15:0x3079-0x3519]: PatBlt black, seg10:0x6da6(0,
    // 1), the slow-machine line when `slow` is not empty (seg10:0x94c4 at W/2, DGROUP:0x90ec/2), and
    // when `render` scene 110 (IGRAPHIC.DLL #6600 0x6e) into the scene DC DGROUP:0x929a, blitted to
    // the screen. Returns slot 0 (the out block's words 0 ... 3) when it rendered.
    virtual std::optional<desk::Rect> landing(int W, int H, int play_h, const std::string& slow, bool render) = 0;
    // seg15:0x1a42 outside state 9: seg15:0x20ca (the player's ship; seg15:0x28b2 when `dead`,
    // DGROUP:0x9e5b = 0, with its 1000 rand() draws from `rng`) and the jet's 16 frames.
    virtual void jet_frames(bool dead, shell::Rng& rng) = 0;
    // seg15:0x3caa(first) with the jet's words DGROUP:0x92d6, DGROUP:0x92d8 and DGROUP:0x92de; non-
    // zero when the jet has landed. The caller handles the fast flag's early return.
    virtual int incoming(int first, int d92d6, int d92d8, int d92de) = 0;
    // seg15:0x1328(1) in state 8 (the jet's frames freed, the scene DC kept) and seg15:0x1eac (the
    // two ship frames, 0 with the flame, 1 without) [BYTES seg15:0x35c9-0x35d0].
    virtual void two_frames() = 0;
    virtual void black(int W, int H) = 0;  // PatBlt(DGROUP:0x13dc, 0, 0, W, H, BLACKNESS)
    // seg15:0x760's render branch [BYTES seg15:0x863-0x9d8]: the old scene DC freed, scene 115
    // (0x73) rendered at W x H into a new one; returns its out words 0 ... 3 (slot 0, the monitor)
    // and 5 (slot 1's top). The monitor (seg15:0x8604) and the blit are the caller's, through the
    // desk canvas that holds the scene DC.
    struct HallScene {
        desk::Rect slot0;
        int w5 = 0;
    };
    virtual HallScene hall_scene(int W, int H) = 0;
    // seg15:0x760's redraw branch's ship: frame 1 (without the flame) of the two frames at (x, y),
    // s4 x s4, through the mask (0x008800C6) and the image (0x00EE0086) [BYTES seg15:0x796-0x860].
    virtual void ship_frame(int x, int y, int s4) = 0;
    // seg15:0x374c's drawing in state 8 [BYTES seg15:0x39b6-0x3b92]: the ship rectangle (half(W) -
    // half(s4), y, s4, s4 + 40) restored from the scene DC (not the pad DC), the shadow at every
    // height (depth <= 8) from frame 1's mask at (0, half(hgt) + 9), frame `flag` at (0, 5).
    virtual void descent_frame(int W, int y, int s4, int flag, int hgt) = 0;
    // The pictures seg15:0x9e6 reads at the launch's step 5 (DGROUP:0x92b0 exists): the desk
    // canvas's visit_ship takes them [BYTES seg15:0x11fe seg15:0xa07-0xa32].
    virtual void lend_ship() = 0;
    virtual void free_scene() = 0;  // DGROUP:0x929a and 0x9294 deleted, both 0
};

// The program's calls.
class Host {
public:
    virtual ~Host() = default;
    virtual Tick now() = 0;          // USER.15 GetCurrentTime
    virtual Tick pass_now() = 0;     // DGROUP:0x1e50, the pass's sample
    virtual void sound(int id) = 0;  // seg10:0x744c
    virtual void sound_at(int id, int a, int b, int mode) = 0;  // seg10:0x754a
    virtual void music(int code, int arg) = 0;                  // seg10:0x7480
    virtual void pump_until(Tick t) = 0;  // seg10:0x740c until GetCurrentTime reaches t (unsigned)
    virtual void pump() = 0;              // seg10:0x740c once
    virtual bool ctrl_held() = 0;    // GetKeyState(VK_CONTROL) & 0x8000
    virtual int quick_intro() = 0;   // DGROUP:0x8f70
    virtual int calibrate() = 0;     // DGROUP:0x8ffc
    virtual int depth() = 0;         // DGROUP:0x90ee
    virtual std::string lang(int id) = 0;
    virtual std::string pilot() = 0;  // DGROUP:0x380c
    virtual Clock clock() = 0;        // time() and localtime() (seg5:0x5be, seg5:0x4c0)
    virtual std::string main_ship() = 0;  // INI MainShip in seg15:0x2a08's 20-byte buffer
    virtual void close_hud() = 0;         // seg11:0x2f0
    virtual void watch_windows(int code) = 0;  // seg13:0x65f2(code)
    // seg9:0x6cc4(1): state 2, the wave's teardown and the box drawn at once (wave_end.h
    // debrief_entry(c, 1)).
    virtual void debriefing() = 0;
    virtual void set_state(int s) = 0;  // DGROUP:0x90f4 (the demo's phase 10)
    virtual void todo(const std::string& what, const char* address) = 0;
};

// The machine. W x H is the play area DGROUP:0x90ea x DGROUP:0x10, play_h DGROUP:0x90ec.
class Hall {
public:
    Hall(world::World& w, Screen& scr, desk::Canvas& desk, Host& h, shell::Rng& rng)
        : w_(w), scr_(scr), desk_(desk), h_(h), rng_(rng) {}

    // seg9:0x6c62-0x6c6e: DGROUP:0x929c = 0 and seg16:0x6928(0), which runs phase 0 at once. The
    // state store DGROUP:0x90f4 = 8 is the caller's (it comes first); DGROUP:0x9346 and the alive
    // loop after it are wave_end.cpp's.
    void enter(int W, int H, int play_h);
    // seg16:0x6928(arg) [BYTES seg16:0x6928-0x6afc]: arg 0 the state-8 frame (seg7:0x1b7), 1 the
    // repaint (seg11:0x1686).
    void frame(int arg);

    const Words& words() const { return k_; }
    Words& words() { return k_; }
    int W() const { return W_; }
    int H() const { return H_; }
    bool player_game = true;  // DGROUP:0x95bc

    // Exposed for the tests.
    void type(int arg);       // seg15:0x1584(arg) in state 8
    int descent_step(bool first);  // seg15:0x374c(first) in state 8
    int launch(int arg);      // seg15:0x0(arg) in state 8
    void launch_enter();      // seg15:0xcb0 in state 8
    bool launch_frame();      // seg15:0x10be from step 5; true when step 9 ran
    void hall_scene();        // seg15:0x760
    void landing();           // seg15:0x302e in state 8
    void descent();           // seg15:0x3590 in state 8
    void phase_text();        // seg16:0x6afe with its seg15:0x1584(1)
    const HallText& last_text() const { return text_; }

private:
    int s4() const;           // 4·(sext(R +0x7a) + (sext(R +0x7b) - 6)/2) of the player's roster record
    void newline();

    world::World& w_;
    Screen& scr_;
    desk::Canvas& desk_;
    Host& h_;
    shell::Rng& rng_;
    Words k_;
    HallText text_;
    int W_ = 0, H_ = 0, play_h_ = 0;
    int font_h_ = 0, font_w_ = 0;
};

}  // namespace is::hall
