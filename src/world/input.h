// The wave's input (docs/re/world.md "Input"; ADR 0013 decision 6): the key latch that stands
// for GetAsyncKeyState at the polled sites, the key remap seg11:0xb306, the state-1 key-message
// switch seg11:0x6b64 for the keys of M3 stage 2, the target-code call seg11:0x772e's shape, the
// joystick table DGROUP:0x1018 and the steering seg11:0x7504 / seg11:0x7650.
//
// Portable, no Win32. The hooks (flight_hooks.cpp) feed the latch from WM_KEYDOWN and WM_KEYUP
// and implement KeyActions with the other parts of the port.
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace is::world {

struct WaveGlobals;
class FlightHost;

// --- The key latch (ADR 0013 decision 6) ------------------------------------------------------
// Per virtual key: `down`, the key's current state (GetAsyncKeyState bit 0x8000), and
// `pressed`, set by a key-down that is not an auto-repeat and cleared by the next read of that
// key (bit 0x0001, "pressed since the last call"; the first reader clears it, as
// GetAsyncKeyState's bit is shared by every caller). [DESIGN ADR 0013]
class KeyLatch {
public:
    static constexpr int kKeys = 256;

    // WM_KEYDOWN / WM_SYSKEYDOWN: `repeat` is lParam bit 30 (the key was already down).
    void key_down(std::uint16_t vk, bool repeat);
    // WM_KEYUP / WM_SYSKEYUP.
    void key_up(std::uint16_t vk);
    // All keys up, nothing pressed (a new wave, or the window's keyboard state reset).
    void reset();
    // Every `pressed` bit cleared, the `down` bits kept: the wave start, so a tap made in the
    // briefing does not fire through the port's 0x8001 fire mask, which the original's 0x8000
    // mask would not see [DESIGN ADR 0013 decision 6] (M3 s2 fixer).
    void clear_pressed() { pressed_.fill(false); }
    // Every key the latch holds down that `held(vk)` reports up is released (its `pressed` bit
    // kept): the window's activation comes back (seg2:0xa06, resume) after a key-up went to
    // another window, as the Alt of Alt+S does to the Save Game dialog. The original's
    // GetAsyncKeyState reads the keyboard, so its Alt reads up there; a latch fed only by the
    // main window's messages kept Alt down and turned every arrow into Alt+arrow, the spin of
    // world.md "Turning" (work/world/m4s4/accept/save.md DIFFERENT 1) [DESIGN ADR 0013 decision 6]
    // (M4 s4 fixer B).
    template <class Held>
    void release_unheld(Held held) {
        for (int vk = 0; vk < kKeys; ++vk)
            if (down_[static_cast<std::size_t>(vk)] && !held(static_cast<std::uint16_t>(vk)))
                down_[static_cast<std::size_t>(vk)] = false;
    }

    // The GetAsyncKeyState word of vk: 0x8000 while down, 0x0001 when pressed since the last
    // read, which this read clears.
    std::uint16_t async(std::uint16_t vk);
    // (async(vk) & mask) != 0, the test the original's sites make.
    bool test(std::uint16_t vk, std::uint16_t mask) { return (async(vk) & mask) != 0; }
    // GetKeyState(vk) & 0x8000: the down bit without touching `pressed` (seg11:0x6b64's Ctrl
    // and Alt tests) [DESIGN ADR 0013].
    bool down(std::uint16_t vk) const { return down_[vk & 0xff]; }
    bool pressed(std::uint16_t vk) const { return pressed_[vk & 0xff]; }

private:
    std::array<bool, kKeys> down_{};
    std::array<bool, kKeys> pressed_{};
};

// The masks of the polled sites [BYTES seg11:0x5978 seg11:0x5f81 seg11:0x5fb9 seg11:0x4db0].
constexpr std::uint16_t kMaskDown = 0x8000;
constexpr std::uint16_t kMaskDownOrPressed = 0x8001;
// The fire poll seg11:0x4db0 masks 0x8000; the port reads 0x8001 so a tap between passes fires
// once at its one pass per frame [DESIGN ADR 0013 decision 6].
constexpr std::uint16_t kFireMask = kMaskDownOrPressed;

// --- The fire poll seg11:0x4db0 (M4 s2 GUN) -------------------------------------------------
// seg11:0x4db0: true when the gun seg11:0x4e0a(player) is due: PlayMode DGROUP:0x9134 = 0, the
// player's lock-out dword +0x84 before now (unsigned), then GetAsyncKeyState(VK_SHIFT) & 0x8000,
// else VK_SPACE (read only when Shift is up), else the joystick word DGROUP:0x1042
// [BYTES seg11:0x4dba-0x4e00]. The port reads kFireMask (0x8001) [DESIGN ADR 0013 decision 6].
// The frame calls it at seg7:0x881 only while the player's object +0x18 = 0 [BYTES
// seg7:0x871-0x881]; update.cpp's step 3 calls it.
bool fire_poll(const WaveGlobals& g, KeyLatch& keys, std::uint32_t player_lockout);

// --- The key remap seg11:0xb306 ---------------------------------------------------------------
// DGROUP:0x94e4 holds ENG_LANG string 2900 and DGROUP:0x94fa string 2901, both loaded at start-up
// (seg6:0x178d, seg6:0x179f). Unless DGROUP:0x94fa[0] is 'Y', a key code equal to byte
// DGROUP:0x94fa + j (j = 1 ... 21) becomes byte DGROUP:0x94e4 + j, the first match winning
// [BYTES seg11:0xb313-0xb341]. English: "NQAD..." over "YHAD...", so Q becomes H.
struct KeyRemap {
    std::array<std::uint8_t, 23> to{};    // DGROUP:0x94e4
    std::array<std::uint8_t, 23> from{};  // DGROUP:0x94fa
    // From the two strings; LoadString copies at most 39 characters and DGROUP:0x94e4's NUL
    // lands on DGROUP:0x94fa[0] before string 2901 overwrites it [BYTES seg6:0x1785-0x17a4].
    static KeyRemap from_strings(const std::string& s2900, const std::string& s2901);
    std::uint16_t map(std::uint16_t key) const;
};

// --- The key-message switch seg11:0x6b64 (the keys of this stage) ----------------------------
// What the switch calls in other parts of the port. Each names the original's routine.
class KeyActions {
public:
    virtual ~KeyActions() = default;
    // seg11:0x772e(ctrl, code): the target code of T, R, I, Y, V, U (C's target display).
    virtual void target_code(bool ctrl, int code) { (void)ctrl, (void)code; }
    // Ctrl+T at seg11:0x7138: DGROUP:0xa62 toggled; on calls seg13:0x2206 and seg3:0xf4, then
    // WM_PAINT to DGROUP:0xa7c (C's target display).
    virtual void toggle_manual_target() {}
    // The zoom keys' tail: SetScrollPos(DGROUP:0xa7a, SB_CTL, 600 - zoom, TRUE) and the radar
    // repaint seg11:0x33cc [BYTES seg11:0x723a-0x7247].
    virtual void zoom_changed(std::int32_t zoom) { (void)zoom; }
    // PostMessage(main DGROUP:0x16, msg, 0, 0): 0x9aa (Alt+I), 0x9b4 (Alt+R).
    virtual void post_main(std::uint32_t msg) { (void)msg; }
    // seg10:0x7e1c(d): the speed step (Alt +/-) and seg10:0x9a2a(d): the rescale (Ctrl +/-).
    virtual void speed_step(int d) { (void)d; }
    virtual void rescale_step(int d) { (void)d; }
    // B (seg11:0x71d4) and M (seg11:0x71a9): the left display's frame-rate and message modes
    // (C's Info::show_frame_rate, show_messages) [BYTES seg11:0x6e6a seg11:0x6e9a].
    virtual void show_frame_rate() {}
    virtual void show_messages() {}
    // seg10:0x7e1c's PostMessage(DGROUP:0xa7c, WM_PAINT) after it set DGROUP:0xa76 = 4: the
    // left display repaints in its new mode [BYTES seg10:0x7e3f].
    virtual void info_repaint() {}
    // WM_KEYDOWN 'H' (seg11:0x6f7a-0x6fe8): Ctrl / Alt / the call seg16:0xb4e with its reply
    // (ambulance.h ambulance_key) (M4 s3 fixer).
    virtual void ambulance(bool ctrl, bool alt) { (void)ctrl, (void)alt; }
    // A key of the switch that a later stage ports: wParam and the case's address.
    virtual void unported(std::uint32_t msg, std::uint16_t key, const char* address) {
        (void)msg, (void)key, (void)address;
    }
};

// The state the switch reads and writes beyond WaveGlobals.
struct KeyState {
    std::int32_t zoom = 120;  // DGROUP:0xa6c (dword), the radar zoom; the file holds 120
};

// Messages the switch tests (Win16 and Win32 share the numbers).
constexpr std::uint32_t kWmKeyDown = 0x100;
constexpr std::uint32_t kWmKeyUp = 0x101;
constexpr std::uint32_t kMsgAltKey = 0x99b;
constexpr std::uint32_t kMsgCtrlKey = 0x99e;

// The outcome of one call of the switch, for the router and the tests.
enum class KeyResult {
    kGated,      // the lock-out or the player's state kept the key out [BYTES seg11:0x6c9b-0x6cd1]
    kHandled,    // a key of this stage acted
    kNotMine,    // a key the switch handles that this stage does not port (passed on)
};

// seg11:0x6b64(msg, wParam, lParam) for WM_KEYDOWN, 0x99b and 0x99e with the keys of this
// stage: + and - (zoom, or Alt / Ctrl speed and scale), T, R, I, Y, V, U, Alt+I, Alt+R. The
// gate: the player's lock-out dword +0x84 > now or `state` != 0 keeps every key out but
// WM_KEYUP 'P' and WM_KEYDOWN 'H'. While DGROUP:0x95bc = 0 (the demo) WM_KEYDOWN takes only
// VK_ADD, VK_SUBTRACT, 'W', VK_OEM_MINUS and VK_OEM_PLUS [BYTES seg11:0x6e19-0x6e3e].
// `player_lockout` is the player's ship dword +0x84 and `player_state` its object +0x18.
KeyResult key_message(std::uint32_t msg, std::uint16_t key, const WaveGlobals& g,
                      std::uint32_t player_lockout, std::int16_t player_state, bool ctrl_down,
                      bool alt_down, KeyState& ks, KeyActions& act);

// The zoom step of the plus and minus keys: zoom + 8 capped at 500, or zoom - 8 floored at 100,
// as 32-bit compares [BYTES seg11:0x7215-0x7233 seg11:0x7268-0x7286].
std::int32_t zoom_plus(std::int32_t zoom);
std::int32_t zoom_minus(std::int32_t zoom);

// seg10:0x7e1c's rate rule (timing.md "Speed keys"): rate + 2d, stepping down from above 1000
// gives 104, at least 4, anything above 104 becomes 1200 [BYTES seg10:0x7e5b seg10:0x7e86
// seg10:0x7f0b].
int speed_step_rate(int rate, int d);

// --- The cloak and the shield (work/analysis/m4/s3-fuel.md 1.2-1.4 with s3-fuel-verify.md) ------
// M4 stage 3, track FUEL. The toggles work on the program's World (its ship records and the
// ship pool's objects); the routines they call in other parts go through SystemsHost.

class World;

// The calls of the two toggles and of seg3:0x332e out of this file; the defaults do nothing.
class SystemsHost {
public:
    virtual ~SystemsHost() = default;
    // seg12:0x2556(DGROUP:0x9324, i) then seg7:0x13b2(mode, DGROUP:0x9324, i), the vanish of
    // core.md "The vanish" (entry_effect.h entry_buffer, entry_start) [BYTES seg11:0x4ce9
    // seg11:0x4cfe seg11:0x4d81 seg11:0x4d96]. The caller writes ship word +0x2e = 2 between them.
    virtual void vanish_buffer(int pool, int ship) { (void)pool, (void)ship; }
    virtual void vanish_start(int mode, int pool, int ship) { (void)mode, (void)pool, (void)ship; }
    // seg10:0x7328(design +0x36, far design +0x84, dword design +0xa9, byte design +0x18, byte
    // design +0x1a, object +0x28, DGROUP:0x3a38[i], 1, 0x63), ISPACE.DLL ordinal 17's wrapper, on
    // both the raise and the lowering: body frames 1 and 2 at the ship's heading into column 16
    // of the sheet DC, rows 0 and 1 [BYTES seg11:0x4ba6 seg11:0x4be4-0x4c37 seg10:0x7335-0x7365]
    // (work/analysis/m4/s5-sheet.md 4.1-4.3, verified claims 22-34). The program's host runs
    // player_sheet.h shield_render_program (gfx/ship_quick.h).
    virtual void shield_render(int ship) { (void)ship; }
    // seg11:0xb09c(i, on): the flame (world.h briefing_flame outside the frame) [BYTES seg11:0x4c50].
    virtual void flame(int on, int ship) { (void)on, (void)ship; }
    // seg10:0x754a(id, pool, rec, mode).
    virtual void sound(int id, int pool, int rec, int mode) { (void)id, (void)pool, (void)rec, (void)mode; }
    // LoadString(DGROUP:0x9212, id, buf, 40) and the pilot name DGROUP:0x380c (seg3:0x332e).
    virtual std::string lang(int id) { return (void)id, std::string{}; }
    virtual std::string pilot() { return {}; }
    // seg7:0x1f38's draw of DGROUP:0x8630 in Strategy mode (PlayMode 1): M8 [OPEN].
    virtual void strategy_line(const std::string& text) { (void)text; }
};
// The program's SystemsHost (src/hud/wave_win32.cpp sets it); a do-nothing host before.
SystemsHost& systems_host();
void set_systems_host(SystemsHost* h);

// DGROUP:0x9522: 1 at a shield raise, -1 at a lower (seg11:0x4b74, seg11:0x4bb4); its readers
// are [OPEN] for this track.
std::int16_t& dg_9522();

// What the toggles read beyond the World: now DGROUP:0x1e50 and the frame count DGROUP:0x1e68.
struct SystemsCtx {
    World& w;
    std::uint32_t now = 0;
    std::uint32_t frames = 0;
    SystemsHost& h;
};

// seg11:0x4c72(i) [BYTES seg11:0x4c72-0x4d16] (verified claims 6-9): the cloak toggle; 1 when it
// toggled. 0 without `has_cloak` +0xe7; for any ship but the player in PlayMode 0 within 150
// frames (unsigned 32-bit frame# - dword +0x152) of the last toggle; switching on with fuel < 120
// (signed); while byte +0xeb != 0. Off: the vanish mode 2, +0xe8 = 0. On: system 3 (+0x2c) != 0
// and, unless the action player, > 40; dword +0x14a = the object's (x, y), +0x14e = now, the
// vanish mode 1, +0xe8 = 1. Both store frame# in +0x152.
std::int16_t cloak_toggle(SystemsCtx& c, int i);
// seg11:0x4b00(i) [BYTES seg11:0x4b00-0x4c66] (verified claims 11 CORRECTED, 12, 13): the shield
// toggle; 1 when it toggled. 0 when system 3 < 50 (signed) and +0xea = 0, while the animation word
// +0x10 is 1 ... 6, and for any ship but the action player within 150 frames of +0x152. Down:
// +0x10 = 6, DGROUP:0x9522 = -1, ship word +0x20 = object size - 2, +0x22 = 1, +0xea = 0. Up:
// +0x10 = 1, DGROUP:0x9522 = 1, +0x20 = size - 12, +0x22 = 7, +0xea = 1. Then seg10:0x7328 (the
// quick cells into column 16; both branches reach it at seg11:0x4be4, s5-sheet-verify.md claim
// 22), +0x152 = frame#, seg11:0xb09c(i, 1) while ship word +0x02 (`thrusting`) != 0 (its pastes
// go to column 16 at the heading while +0x10 is 1 ... 6), sound 5 (SHIELD.WAV) [BYTES
// seg11:0x4c3a-0x4c63]. The step +0x10 moves one per ship-mover pass on screen (flight.cpp,
// seg13:0x37f) and the sprite shows column 16 row 0 for steps 1-3, row 1 for 4-6 [ORACLE
// core-0005].
std::int16_t shield_toggle(SystemsCtx& c, int i);
// seg3:0x332e(a, b), Pascal [BYTES seg3:0x332e-0x33b1] (verified claims 3-5): DGROUP:0x8630 =
// pilot + ": " + string a + string b (each LoadString at most 39 characters), DGROUP:0x58c = 1,
// dword DGROUP:0x5c0 = now + 1200 (a dead store: the frame's gate reads DGROUP:0x5bc). `ids` is
// the pushed dword, a in the high word (0x04090418: 1033 "Uncloaking ", 1048 "Because I'm Low
// On Gas.").
void strategy_notice(SystemsCtx& c, std::uint32_t ids);

// WM_KEYUP VK_CAPITAL in seg11:0x6b64 (seg11:0x6dd6) [BYTES seg11:0x6d19 seg11:0x6dd6-0x6e0a]
// (verified claim 10): the key switch's gate (the player's lock-out or `state`) and the WM_KEYUP
// demo filter DGROUP:0x95bc != 0 come first; then nothing while PlayMode != 0; else the shield
// toggle when the player has `has_shield` +0xe9, else the cloak toggle with `has_cloak` +0xe7.
enum class CapsAction : std::uint8_t { kNone, kShield, kCloak };
constexpr std::uint16_t kVkCapital = 0x14;
CapsAction capslock_action(bool gated, std::int16_t demo, std::int16_t play_mode, std::uint8_t has_shield,
                           std::uint8_t has_cloak);

// --- Joystick (world.md "Joystick") -----------------------------------------------------------
// DGROUP:0x1018: the 5 x 5 stick table, y * 5 + x, a heading or -1 [BYTES DGROUP:0x1018].
constexpr std::array<std::int8_t, 25> kStickTable = {
    14, 15, 0, 1, 2, 13, -1, -1, -1, 3, 12, -1, -1, -1, 4, 11, -1, -1, -1, 5, 10, 9, 8, 7, 6};
// The record at far DGROUP:0x1e4c: axes as unsigned words, then 4 button bytes.
struct JoyState {
    std::uint16_t x = 0, y = 0;
    std::array<std::uint8_t, 4> buttons{};
};
// What a joystick button posts or sets (seg11:0x7588's cases by DGROUP:0x1dcc + 2i).
struct JoyButtons {
    std::array<std::int16_t, 4> config{};   // DGROUP:0x1dcc + 2i
    std::array<std::int16_t, 4> previous{}; // DGROUP:0x6b54 + 2i
};
// The stick's target heading: axes / 0x3444 (unsigned, each clamped to 4) through kStickTable
// [BYTES seg11:0x753d-0x7570].
std::int16_t stick_target(std::uint16_t x, std::uint16_t y);
// seg11:0x7650(target, from_stick): the steering flags DGROUP:0x1040, 0x1044, 0x1046
// [BYTES seg11:0x7650-0x772c].
void steer(WaveGlobals& g, std::int16_t heading, std::int16_t target, bool from_stick);
// A message seg11:0x7588 posts to the main window for a button.
struct JoyPost {
    std::uint32_t msg = 0;
    std::uint16_t key = 0;
};
// seg11:0x7504: clears the joystick flags, reads the 4 buttons (seg11:0x7588; posts go to
// `post`, at most one per button) and steers by the stick. Runs only while `JoyStick` is on.
template <typename Post>
void joystick_poll(WaveGlobals& g, const JoyState& js, JoyButtons& jb, std::int16_t heading,
                   Post&& post);
// One button of seg11:0x7588 [BYTES seg11:0x7588-0x764e]; returns the post (msg 0: none).
JoyPost joystick_button(WaveGlobals& g, const JoyState& js, JoyButtons& jb, int i);

}  // namespace is::world

#include "world/flight.h"

namespace is::world {

template <typename Post>
void joystick_poll(WaveGlobals& g, const JoyState& js, JoyButtons& jb, std::int16_t heading,
                   Post&& post) {
    if (g.joystick == 0) return;  // [BYTES seg11:0x7512]
    g.joy_overthrust = g.joy_fire = g.joy_left = g.joy_right = 0;  // [BYTES seg11:0x7519-0x7524]
    for (int i = 0; i < 4; ++i) {
        const JoyPost p = joystick_button(g, js, jb, i);
        if (p.msg != 0) post(p.msg, p.key);
    }
    steer(g, heading, stick_target(js.x, js.y), true);  // seg11:0x7575: push 1
}

}  // namespace is::world
