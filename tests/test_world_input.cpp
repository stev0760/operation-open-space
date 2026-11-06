// The wave's input (src/world/input.h; ADR 0013 decision 6; docs/re/world.md "Input"): the key
// latch and its masks, the key remap of ENG_LANG strings 2900 and 2901, the keys of the switch
// seg11:0x6b64 this stage ports, the speed rule, and the joystick table and steering.
#include <cstdint>
#include <utility>
#include <vector>

#include "doctest.h"
#include "world/input.h"

using namespace is::world;

namespace {

class Recorder : public KeyActions {
public:
    std::vector<std::pair<int, int>> codes;  // (ctrl, code)
    int manual = 0;
    std::vector<std::int32_t> zooms;
    std::vector<std::uint32_t> posts;
    std::vector<int> speeds, scales;
    void target_code(bool ctrl, int code) override { codes.emplace_back(ctrl ? 1 : 0, code); }
    void toggle_manual_target() override { ++manual; }
    void zoom_changed(std::int32_t z) override { zooms.push_back(z); }
    void post_main(std::uint32_t m) override { posts.push_back(m); }
    void speed_step(int d) override { speeds.push_back(d); }
    void rescale_step(int d) override { scales.push_back(d); }
    std::vector<std::pair<int, int>> ambulance_keys;  // (ctrl, alt)
    void ambulance(bool ctrl, bool alt) override { ambulance_keys.emplace_back(ctrl ? 1 : 0, alt ? 1 : 0); }
};

KeyResult down(std::uint16_t key, const WaveGlobals& g, KeyState& ks, KeyActions& a,
               bool ctrl = false, bool alt = false, std::uint32_t lockout = 0,
               std::int16_t state = 0) {
    return key_message(kWmKeyDown, key, g, lockout, state, ctrl, alt, ks, a);
}

}  // namespace

TEST_CASE("input: a tap between passes fires once") {
    KeyLatch k;
    k.key_down(kVkSpace, false);
    k.key_up(kVkSpace);
    CHECK(k.test(kVkSpace, kFireMask));        // pressed since the last read
    CHECK_FALSE(k.test(kVkSpace, kFireMask));  // the read cleared it
    CHECK_FALSE(k.test(kVkSpace, kMaskDown));
}

TEST_CASE("input: the latch's down and pressed bits") {
    KeyLatch k;
    k.key_down(kVkRight, false);
    CHECK(k.async(kVkRight) == 0x8001);
    CHECK(k.async(kVkRight) == 0x8000);
    k.key_down(kVkRight, true);                // auto-repeat: no new press
    CHECK(k.async(kVkRight) == 0x8000);
    CHECK(k.down(kVkRight));
    k.key_up(kVkRight);
    CHECK(k.async(kVkRight) == 0);
    // Turning masks 0x8000: a tap whose key-up came first does not turn.
    k.key_down(kVkLeft, false);
    k.key_up(kVkLeft);
    CHECK_FALSE(k.test(kVkLeft, kMaskDown));
    // Up masks 0x8001: the same tap acts once.
    k.key_down(kVkUp, false);
    k.key_up(kVkUp);
    CHECK(k.test(kVkUp, kMaskDownOrPressed));
    CHECK_FALSE(k.test(kVkUp, kMaskDownOrPressed));
    // down() is GetKeyState: it leaves the pressed bit alone.
    k.key_down(kVkControl, false);
    CHECK(k.down(kVkControl));
    CHECK(k.pressed(kVkControl));
    k.reset();
    CHECK_FALSE(k.down(kVkControl));
}

// Alt+S: Alt's key-down reaches the main window, its key-up the Save Game dialog. At the resume
// the keys the keyboard reads up are released, so the arrows turn again instead of spinning
// (work/world/m4s4/accept/save.md DIFFERENT 1; seg2:0xa06 clears Shift, Ctrl, Alt, Caps Lock and
// Space [BYTES seg2:0xa1a-0xa66]) (M4 s4 fixer B).
TEST_CASE("input: the resume releases the keys the keyboard reads up") {
    KeyLatch k;
    const std::uint16_t alt = kVkMenu, s = 'S';
    k.key_down(alt, false);
    k.key_down(s, false);
    k.key_up(s);
    k.key_down(kVkLeft, false);  // still held on the keyboard
    CHECK(k.down(alt));
    k.release_unheld([&](std::uint16_t vk) { return vk == kVkLeft; });
    CHECK_FALSE(k.down(alt));
    CHECK(k.async(alt) == 0x0001);  // its press stays for the next read
    CHECK(k.down(kVkLeft));
    CHECK(k.async(kVkLeft) == 0x8001);
}

TEST_CASE("input: the key remap of strings 2900 and 2901") {
    const KeyRemap r = KeyRemap::from_strings("YHADTGSCDVPBWGXSIEFSCM", "NQADTGSCDVPBWGXSIEFSCM");
    CHECK(r.map('Q') == 'H');
    CHECK(r.map('S') == 'S');
    CHECK(r.map('T') == 'T');
    CHECK(r.map('Z') == 'Z');
    CHECK(r.map('N') == 'N');  // byte 0 is never compared
    const KeyRemap off = KeyRemap::from_strings("YHAD", "YQAD");
    CHECK(off.map('Q') == 'Q');  // 'Y' first: no remap
}

TEST_CASE("input: zoom keys and their limits") {
    WaveGlobals g;
    KeyState ks;
    Recorder a;
    CHECK(down(0x6b, g, ks, a) == KeyResult::kHandled);  // VK_ADD
    CHECK(ks.zoom == 128);
    ks.zoom = 496;
    down(0xbb, g, ks, a);  // VK_OEM_PLUS
    CHECK(ks.zoom == 500);
    ks.zoom = 104;
    down(0x6d, g, ks, a);  // VK_SUBTRACT
    CHECK(ks.zoom == 100);
    down(0xbd, g, ks, a);  // VK_OEM_MINUS
    CHECK(ks.zoom == 100);
    CHECK(a.zooms == std::vector<std::int32_t>{128, 500, 100, 100});
    CHECK(zoom_plus(120) == 128);
    CHECK(zoom_minus(100) == 100);
    // Alt and Ctrl: speed and scale; the message forms 0x99b and 0x99e count as the modifiers.
    down(0x6b, g, ks, a, false, true);
    down(0x6d, g, ks, a, true, false);
    key_message(kMsgAltKey, 0x6d, g, 0, 0, false, false, ks, a);
    key_message(kMsgCtrlKey, 0x6b, g, 0, 0, false, false, ks, a);
    CHECK(a.speeds == std::vector<int>{1, -1});
    CHECK(a.scales == std::vector<int>{-1, 1});
    CHECK(ks.zoom == 100);
}

TEST_CASE("input: target codes and the watch posts") {
    WaveGlobals g;
    KeyState ks;
    Recorder a;
    down('T', g, ks, a);
    down('R', g, ks, a);
    down('I', g, ks, a);
    down('Y', g, ks, a);
    down('V', g, ks, a);
    down('U', g, ks, a);
    down('R', g, ks, a, true);
    CHECK(a.codes == std::vector<std::pair<int, int>>{{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {1, 1}});
    down('T', g, ks, a, true);
    CHECK(a.manual == 1);
    down('I', g, ks, a, false, true);
    key_message(kMsgAltKey, 'R', g, 0, 0, false, false, ks, a);
    CHECK(a.posts == std::vector<std::uint32_t>{0x9aa, 0x9b4});
    CHECK(down('Q', g, ks, a) == KeyResult::kNotMine);
    CHECK(key_message(kWmKeyUp, 'T', g, 0, 0, false, false, ks, a) == KeyResult::kNotMine);
}

TEST_CASE("input: the gate and the demo") {
    WaveGlobals g;
    g.now = 1000;
    KeyState ks;
    Recorder a;
    CHECK(down('T', g, ks, a, false, false, 1001) == KeyResult::kGated);  // lock-out after now
    CHECK(down('T', g, ks, a, false, false, 0, 100) == KeyResult::kGated);  // player off-world
    CHECK(down('H', g, ks, a, false, false, 1001) == KeyResult::kHandled);  // H passes the gate (seg11:0x6f7a)
    CHECK(a.ambulance_keys == std::vector<std::pair<int, int>>{{0, 0}});
    CHECK(down('H', g, ks, a, true, false) == KeyResult::kHandled);
    CHECK(a.ambulance_keys.back() == std::pair<int, int>{1, 0});
    CHECK(a.codes.empty());
    g.demo = 0;  // DGROUP:0x95bc = 0: only + - W
    CHECK(down('T', g, ks, a) == KeyResult::kHandled);
    CHECK(a.codes.empty());
    down(0x6b, g, ks, a);
    CHECK(ks.zoom == 128);
}

TEST_CASE("input: the speed rule of seg10:0x7e1c") {
    CHECK(speed_step_rate(36, 1) == 38);
    CHECK(speed_step_rate(36, -1) == 34);
    CHECK(speed_step_rate(104, 1) == 1200);
    CHECK(speed_step_rate(1200, -1) == 104);
    CHECK(speed_step_rate(1200, 1) == 1200);
    CHECK(speed_step_rate(4, -1) == 4);
}

TEST_CASE("input: the joystick table and steering") {
    CHECK(stick_target(0, 0) == 14);
    CHECK(stick_target(0x8000, 0x8000) == -1);  // the centre
    CHECK(stick_target(0xffff, 0) == 2);
    CHECK(stick_target(0x8000, 0xffff) == 8);
    CHECK(stick_target(0, 0x8000) == 12);
    WaveGlobals g;
    steer(g, 0, 0, true);
    CHECK(g.joy_thrust == 1);
    g = WaveGlobals{};
    steer(g, 0, 2, true);   // |2 - 0| <= 8, heading < target: right
    CHECK(g.joy_right == 1);
    CHECK(g.joy_left == 0);
    g = WaveGlobals{};
    steer(g, 0, 14, true);  // the short way round: left
    CHECK(g.joy_left == 1);
    g = WaveGlobals{};
    steer(g, 14, 2, true);  // |2 - 14| = 12 > 8, heading > target: right
    CHECK(g.joy_right == 1);
    g = WaveGlobals{};
    steer(g, 5, -1, true);
    CHECK(g.joy_thrust == 0);
    CHECK(g.joy_left + g.joy_right == 0);
    g = WaveGlobals{};
    steer(g, 0, 2, false);  // not from the stick, with DGROUP:0x1034 = 1: nothing
    CHECK(g.joy_right == 0);
    // The buttons: 0 fire, 1 Enter up, 2 Tab down, 3 OverThruster; posted once per press.
    g = WaveGlobals{};
    g.joystick = 1;
    JoyState js;
    js.x = js.y = 0x8000;
    js.buttons = {1, 1, 1, 1};
    JoyButtons jb;
    jb.config = {0, 1, 2, 3};
    std::vector<std::pair<std::uint32_t, std::uint16_t>> posts;
    joystick_poll(g, js, jb, 0, [&posts](std::uint32_t m, std::uint16_t k) { posts.emplace_back(m, k); });
    CHECK(g.joy_fire == 1);
    CHECK(g.joy_overthrust == 1);
    CHECK(posts.size() == 2);
    posts.clear();
    joystick_poll(g, js, jb, 0, [&posts](std::uint32_t m, std::uint16_t k) { posts.emplace_back(m, k); });
    CHECK(posts.empty());   // held: configs 1 and 2 wait for a new press
    CHECK(g.joy_fire == 1); // configs 0 and 3 act while held
}
