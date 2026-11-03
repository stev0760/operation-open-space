// The wave's input (input.h).
#include "world/input.h"

#include "world/collide.h"
#include "world/world.h"

namespace is::world {

// ---- KeyLatch [DESIGN ADR 0013 decision 6]

void KeyLatch::key_down(std::uint16_t vk, bool repeat) {
    const std::size_t k = vk & 0xffu;
    down_[k] = true;
    if (!repeat) pressed_[k] = true;
}

void KeyLatch::key_up(std::uint16_t vk) { down_[vk & 0xffu] = false; }

void KeyLatch::reset() {
    down_.fill(false);
    pressed_.fill(false);
}

std::uint16_t KeyLatch::async(std::uint16_t vk) {
    const std::size_t k = vk & 0xffu;
    std::uint16_t r = down_[k] ? 0x8000 : 0;
    if (pressed_[k]) r |= 0x0001;
    pressed_[k] = false;
    return r;
}

// ---- seg11:0xb306

KeyRemap KeyRemap::from_strings(const std::string& s2900, const std::string& s2901) {
    // One 45-byte window from DGROUP:0x94e4: LoadString(2900) then LoadString(2901), each at
    // most 39 characters plus the NUL (cchMax 0x28) [BYTES seg6:0x1785-0x17a4].
    std::array<std::uint8_t, 0x16 + 0x28> mem{};
    auto load = [&mem](std::size_t at, const std::string& s) {
        std::size_t n = s.size() < 0x27 ? s.size() : 0x27;
        for (std::size_t i = 0; i < n; ++i) mem[at + i] = static_cast<std::uint8_t>(s[i]);
        mem[at + n] = 0;
    };
    load(0, s2900);
    load(0x16, s2901);
    KeyRemap r;
    for (std::size_t i = 0; i < r.to.size(); ++i) r.to[i] = mem[i];
    for (std::size_t i = 0; i < r.from.size(); ++i) r.from[i] = mem[0x16 + i];
    return r;
}

std::uint16_t KeyRemap::map(std::uint16_t key) const {
    if (from[0] == 'Y') return key;  // [BYTES seg11:0xb313]
    for (std::size_t j = 1; j < 0x16; ++j) {
        // `mov al, byte; cwde; cmp ax, key`: the byte sign-extended [BYTES seg11:0xb32d-0xb335]
        const auto b = static_cast<std::int16_t>(static_cast<std::int8_t>(from[j]));
        if (b == static_cast<std::int16_t>(key))
            return static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(to[j])));
    }
    return key;
}

// ---- seg11:0x6b64

std::int32_t zoom_plus(std::int32_t zoom) {
    const std::int32_t z = zoom + 8;
    return z > 500 ? 500 : z;
}

std::int32_t zoom_minus(std::int32_t zoom) {
    const std::int32_t z = zoom - 8;
    return z >= 100 ? z : 100;
}

// [BYTES seg10:0x7e56-0x7e86]
int speed_step_rate(int rate, int d) {
    std::int16_t r = add16(static_cast<std::int16_t>(rate), static_cast<std::int16_t>(2 * d));
    if (d < 0 && r > 1000) r = 104;  // stepping down from above 1000
    if (r < 4) r = 4;
    if (r > 104) r = 1200;
    return r;
}

namespace {

constexpr std::uint16_t kVkAdd = 0x6b, kVkSubtract = 0x6d, kVkOemPlus = 0xbb, kVkOemMinus = 0xbd;

}  // namespace

// ---- seg11:0x4db0 (M4 s2 GUN)

bool fire_poll(const WaveGlobals& g, KeyLatch& keys, std::uint32_t player_lockout) {
    if (g.play_mode != 0) return false;            // [BYTES seg11:0x4dba-0x4dbf]
    if (!(player_lockout < g.now)) return false;   // `ja` / `jae` on the dword [BYTES seg11:0x4dc1-0x4dda]
    return keys.test(kVkShift, kFireMask) ||       // [BYTES seg11:0x4ddc-0x4de6]
           keys.test(kVkSpace, kFireMask) ||       // [BYTES seg11:0x4de8-0x4df2]
           g.joy_fire != 0;                        // DGROUP:0x1042 [BYTES seg11:0x4df4-0x4df9]
}

KeyResult key_message(std::uint32_t msg, std::uint16_t key, const WaveGlobals& g,
                      std::uint32_t player_lockout, std::int16_t player_state, bool ctrl_down,
                      bool alt_down, KeyState& ks, KeyActions& act) {
    // The gate [BYTES seg11:0x6c75-0x6cd1]
    const bool exempt = (msg == kWmKeyUp && key == 'P') || (msg == kWmKeyDown && key == 'H');
    if ((player_lockout > g.now || player_state != 0) && !exempt) return KeyResult::kGated;
    const bool ctrl = msg == kMsgCtrlKey || ctrl_down;  // [BYTES seg11:0x6cd5-0x6cef]
    const bool alt = msg == kMsgAltKey || alt_down;     // [BYTES seg11:0x6cf4-0x6d0e]
    // The message switch at seg11:0x7288: WM_KEYDOWN, 0x99b and 0x99e share seg11:0x6e19.
    if (msg != kWmKeyDown && msg != kMsgAltKey && msg != kMsgCtrlKey) return KeyResult::kNotMine;
    if (g.demo == 0 && key != kVkAdd && key != kVkSubtract && key != 'W' && key != kVkOemMinus &&
        key != kVkOemPlus)
        return KeyResult::kHandled;  // the demo takes only these [BYTES seg11:0x6e19-0x6e3e]
    switch (key) {
    case kVkAdd:
    case kVkOemPlus:  // seg11:0x71ef
        if (alt) {
            act.speed_step(1);  // seg10:0x7e1c(1)
        } else if (ctrl) {
            act.rescale_step(1);  // seg10:0x9a2a(1)
        } else {
            ks.zoom = zoom_plus(ks.zoom);
            act.zoom_changed(ks.zoom);
        }
        return KeyResult::kHandled;
    case kVkSubtract:
    case kVkOemMinus:  // seg11:0x724e
        if (alt) {
            act.speed_step(-1);
        } else if (ctrl) {
            act.rescale_step(-1);
        } else {
            ks.zoom = zoom_minus(ks.zoom);
            act.zoom_changed(ks.zoom);
        }
        return KeyResult::kHandled;
    case 'T':  // seg11:0x7132
        if (ctrl)
            act.toggle_manual_target();
        else
            act.target_code(ctrl, 0);
        return KeyResult::kHandled;
    case 'R':  // seg11:0x718b
        if (alt)
            act.post_main(0x9b4);
        else
            act.target_code(ctrl, 1);
        return KeyResult::kHandled;
    case 'I':  // seg11:0x716c
        if (alt)
            act.post_main(0x9aa);
        else
            act.target_code(ctrl, 2);
        return KeyResult::kHandled;
    case 'Y':  // seg11:0x717f
        act.target_code(ctrl, 3);
        return KeyResult::kHandled;
    case 'V':  // seg11:0x7187
        act.target_code(ctrl, 4);
        return KeyResult::kHandled;
    case 'U':  // seg11:0x7183
        act.target_code(ctrl, 5);
        return KeyResult::kHandled;
    case 'B':  // seg11:0x71d4: the frame-rate display [BYTES seg11:0x6e6a] (M3 s2 fixer)
        act.show_frame_rate();
        return KeyResult::kHandled;
    case 'H':  // seg11:0x6f7a: the ambulance (ambulance.h ambulance_key) [BYTES seg11:0x6f7a-0x6fe8] (M4 s3 fixer)
        act.ambulance(ctrl, alt);
        return KeyResult::kHandled;
    case 'M':  // seg11:0x71a9: the message display [BYTES seg11:0x6e9a] (M3 s2 fixer)
        act.show_messages();
        return KeyResult::kHandled;
    default:
        break;
    }
    return KeyResult::kNotMine;
}

// ---- Joystick

std::int16_t stick_target(std::uint16_t x, std::uint16_t y) {
    std::uint16_t cx = static_cast<std::uint16_t>(x / 0x3444u);
    if (cx > 4) cx = 4;
    std::uint16_t cy = static_cast<std::uint16_t>(y / 0x3444u);
    if (cy > 4) cy = 4;
    return kStickTable[static_cast<std::size_t>(cy * 5 + cx)];
}

void steer(WaveGlobals& g, std::int16_t heading, std::int16_t target, bool from_stick) {
    if (g.play_mode != 0) return;                      // [BYTES seg11:0x765e]
    if (!from_stick && g.dg_1034 != 0) return;         // [BYTES seg11:0x7667-0x7672]
    if (target > -1 && heading == target) {            // [BYTES seg11:0x7676-0x7695]
        g.joy_thrust = 1;
        return;
    }
    g.joy_thrust = 0;
    if (target <= -1) return;
    const std::int16_t d = abs16(sub16(heading, target));  // -(target - heading) [BYTES seg11:0x76d3]
    if (d > 8) {
        if (heading < target)
            g.joy_left = 1;
        else
            g.joy_right = 1;
    } else if (heading < target) {
        g.joy_right = 1;
    } else {
        g.joy_left = 1;
    }
}

JoyPost joystick_button(WaveGlobals& g, const JoyState& js, JoyButtons& jb, int i) {
    const auto idx = static_cast<std::size_t>(i);
    const std::int16_t b = static_cast<std::int8_t>(js.buttons[idx]);
    const std::int16_t cfg = jb.config[idx];
    if (b == jb.previous[idx] && cfg != 0 && cfg != 3) return {};  // [BYTES seg11:0x75a3-0x75b5]
    jb.previous[idx] = b;
    if (b == 0) return {};
    switch (cfg) {  // [BYTES seg11:0x762c-0x7645]
    case 0: g.joy_fire = 1; return {};
    case 1: return {kWmKeyUp, 0x0d};
    case 2: return {kWmKeyDown, 0x09};
    case 3: g.joy_overthrust = 1; return {};
    case 4: return {kWmKeyDown, 'H'};
    case 5: return {kWmKeyDown, 'R'};
    case 6: return {kWmKeyUp, 0x14};
    default: return {};
    }
}

// ---- the cloak and the shield (M4 s3 FUEL)

namespace {
SystemsHost g_null_systems;
SystemsHost* g_systems = nullptr;
std::int16_t g_9522 = 0;

// `sub/sbb` of frame# DGROUP:0x1e68 - dword +0x152, then hi = 0 and lo < 0x96 (`jb`): the
// 150-frame wait [BYTES seg11:0x4c9d-0x4cb8 seg11:0x4b45-0x4b60].
bool within_150(std::uint32_t frames, std::uint32_t last) { return frames - last < 150u; }
}  // namespace

SystemsHost& systems_host() { return g_systems ? *g_systems : g_null_systems; }
void set_systems_host(SystemsHost* h) { g_systems = h; }
std::int16_t& dg_9522() { return g_9522; }

// seg11:0x4c72 [BYTES seg11:0x4c72-0x4d16]
std::int16_t cloak_toggle(SystemsCtx& c, int i) {
    World& w = c.w;
    if (i < 0 || i >= kShipCount || i >= w.pools.records(w.roles.ships)) return 0;  // [DESIGN] bounds
    ShipRecord& s = w.ship(i);
    ObjectRecord& o = w.ship_object(i);
    const bool action_player = i == w.player && w.play_mode == 0;  // [BYTES seg11:0x4c8e-0x4c9b]
    if (s.be7 == 0) return 0;                                       // [BYTES seg11:0x4c82]
    if (!action_player && within_150(c.frames, ship_dword(s, 0x152))) return 0;
    if (s.be8 == 0 && s.fuel < 120) return 0;                       // [BYTES seg11:0x4cbf-0x4ccb]
    if (s.beb != 0) return 0;                                       // [BYTES seg11:0x4cd2-0x4cd7]
    const int pool = w.roles.ships;                                 // DGROUP:0x9324
    if (s.be8 != 0) {  // off [BYTES seg11:0x4cd9-0x4d08]
        c.h.vanish_buffer(pool, i);
        s.w2e = 2;
        c.h.vanish_start(2, pool, i);
        s.be8 = 0;
    } else {  // on [BYTES seg11:0x4d1c-0x4da0]
        if (s.w2c == 0) return 0;
        if (!action_player && s.w2c <= 40) return 0;                // jle 0x28 [BYTES seg11:0x4d3e]
        set_ship_dword(s, 0x14a, static_cast<std::uint32_t>(static_cast<std::uint16_t>(o.x)) |
                                     (static_cast<std::uint32_t>(static_cast<std::uint16_t>(o.y)) << 16));
        set_ship_dword(s, 0x14e, c.now);                            // [BYTES seg11:0x4d6f-0x4d73]
        c.h.vanish_buffer(pool, i);
        s.w2e = 2;
        c.h.vanish_start(1, pool, i);
        s.be8 = 1;
    }
    set_ship_dword(s, 0x152, c.frames);  // [BYTES seg11:0x4d0d-0x4d11]
    return 1;
}

// seg11:0x4b00 [BYTES seg11:0x4b00-0x4c66]
std::int16_t shield_toggle(SystemsCtx& c, int i) {
    World& w = c.w;
    if (i < 0 || i >= kShipCount || i >= w.pools.records(w.roles.ships)) return 0;  // [DESIGN] bounds
    ShipRecord& s = w.ship(i);
    const ObjectRecord& o = w.ship_object(i);
    if (s.w2c < 50 && s.bea == 0) return 0;     // [BYTES seg11:0x4b0b-0x4b1e]
    if (s.type >= 1 && s.type <= 6) return 0;   // [BYTES seg11:0x4b23-0x4b34]
    // The 150-frame wait (s3-fuel-verify.md claim 11, CORRECTED) [BYTES seg11:0x4b36-0x4b60].
    const bool action_player = i == w.player && w.play_mode == 0;
    if (!action_player && within_150(c.frames, ship_dword(s, 0x152))) return 0;
    if (s.bea != 0) {  // down [BYTES seg11:0x4b6e-0x4ba1]
        s.type = 6;
        g_9522 = -1;
        s.w20 = sub16(o.size, 2);
        s.w22 = 1;
        s.bea = 0;
    } else {  // up [BYTES seg11:0x4ba8-0x4bdf]
        s.type = 1;
        g_9522 = 1;
        s.w20 = sub16(o.size, 12);
        s.w22 = 7;
        s.bea = 1;
    }
    c.h.shield_render(i);                 // seg10:0x7328 [BYTES seg11:0x4be4-0x4c37]
    set_ship_dword(s, 0x152, c.frames);   // [BYTES seg11:0x4c3a-0x4c3e]
    if (s.thrusting != 0) c.h.flame(1, i);  // seg11:0xb09c(i, 1) [BYTES seg11:0x4c43-0x4c50]
    c.h.sound(5, w.roles.ships, i, 1);    // SHIELD.WAV [BYTES seg11:0x4c53-0x4c63]
    return 1;
}

// seg3:0x332e [BYTES seg3:0x332e-0x33b1]
void strategy_notice(SystemsCtx& c, std::uint32_t ids) {
    auto cut = [](std::string t) {  // LoadString's cchMax 40 [BYTES seg3:0x3355-0x3392]
        if (t.size() > 39) t.resize(39);
        return t;
    };
    World& w = c.w;
    w.w8630 = c.h.pilot() + ": " + cut(c.h.lang(static_cast<int>(ids >> 16))) +
              cut(c.h.lang(static_cast<int>(ids & 0xffffu)));  // DGROUP:0x13d6 ": "
    w.w058c = 1;                 // [BYTES seg3:0x3397]
    w.w05c0 = c.now + 1200u;     // [BYTES seg3:0x339d-0x33a7]
    c.h.strategy_line(w.w8630);  // seg7:0x1f38 draws it only while PlayMode = 1 (M8) [BYTES seg7:0x1f45]
}

CapsAction capslock_action(bool gated, std::int16_t demo, std::int16_t play_mode, std::uint8_t has_shield,
                           std::uint8_t has_cloak) {
    if (gated || demo == 0) return CapsAction::kNone;  // [BYTES seg11:0x6c9b-0x6cd1 seg11:0x6d19]
    if (play_mode != 0) return CapsAction::kNone;      // [BYTES seg11:0x6dd6-0x6ddb]
    if (has_shield != 0) return CapsAction::kShield;   // [BYTES seg11:0x6ddf-0x6df1]
    if (has_cloak != 0) return CapsAction::kCloak;     // [BYTES seg11:0x6df6-0x6e0a]
    return CapsAction::kNone;
}

}  // namespace is::world
