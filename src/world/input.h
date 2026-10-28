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
