// The flight role's hooks (flight_hooks.h). Win32 only: the host build compiles this file empty.
//
// The frame hook of state 1 runs, in the original's order, the wave pass (update.h wave_pass)
// and the wave frame (wave_frame) on the pass that precedes each due frame [DESIGN ADR 0001].
// The previous state-1 frame hook (the briefing's stand-in, which calls M7's seg10:0x7b48) runs
// at the frame's seg7:0xcaf, where the original calls seg10:0x7b48.
#ifdef _WIN32
#include "world/flight_hooks.h"

#include <windows.h>
#include <mmsystem.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <set>
#include <string>

#include "app/app.h"
#include "shell/rng.h"
#include "world/world.h"

namespace is::world {

namespace {

using app::g;

// ---- the logging defaults

void todo_once(const std::string& what, const char* address) {
    static std::set<std::string> seen;
    if (seen.insert(what + address).second) app::log_todo(what, address);
}

WaveLinks g_links;

class DefaultHost final : public UpdateHost {
public:
    shell::GameState::Hook prev_frame;  // seg10:0x7b48's stand-in
    shell::GameState* state = nullptr;

    void sound_frame() override {
        if (prev_frame && state) prev_frame(*state);
    }
    void joy_read() override;
    void mover(Mover m) override { todo_once("wave mover", mover_address(m)); }
    // The calls src/hud's integration links (WaveLinks, M3 s2 fixer); a missing link logs.
    void fuel_mass() override {
        if (g_links.fuel_mass) return g_links.fuel_mass();
        todo_once("fuel and mass", "seg11:0x1bcc");
    }
    void radar_refresh() override {
        if (g_links.radar_refresh) return g_links.radar_refresh();
        todo_once("radar refresh", "seg11:0x3396");
    }
    void turn_followup() override {
        if (g_links.turn_followup) g_links.turn_followup();
    }
    void fire(int) override { todo_once("fire", "seg11:0x4e0a"); }
    void player_sprite() override {
        if (g_links.player_sprite) return g_links.player_sprite();
        todo_once("the player's sprite", "seg7:0x1208");
    }
    void visibility() override {
        if (g_links.visibility) return g_links.visibility();
        todo_once("visibility pass", "seg8:0x4e14");
    }
    void direction_mark(int d) override {
        if (g_links.direction_mark) return g_links.direction_mark(d);
        todo_once("direction mark " + std::to_string(d), "seg13:0x24aa");
    }
    void draw_sprite(std::int16_t pr, int kind, int slot, int masked) override {
        if (g_links.draw_sprite) return g_links.draw_sprite(pr, kind, slot, masked);
        todo_once("sprite", "seg8:0xb76");
    }
    void dissolve(int i) override {
        if (g_links.dissolve) g_links.dissolve(i);
    }
    void stars(int arg) override {
        if (g_links.stars) g_links.stars(arg);
    }
    void lines() override {
        if (g_links.lines) g_links.lines();
    }
    void select_clip() override {
        if (g_links.select_clip) g_links.select_clip();
    }
    void post_key(std::uint32_t msg, std::uint16_t key) override {
        if (!app::post(static_cast<UINT>(msg), key)) app::log("wave: the joystick post failed");
    }
    void ship_entered(int ship) override {
        char buf[64];
        std::snprintf(buf, sizeof buf, "wave: ship %d enters", ship);
        app::log(buf);
    }
};

// The switch's actions: the posts, the speed and the rescale are the flight role's; the target
// codes, Ctrl+T and the zoom go to the HUD's KeyActions when one is set (set_key_actions).
class DefaultActions final : public KeyActions {
public:
    KeyActions* hud = nullptr;
    void target_code(bool ctrl, int code) override {
        if (hud) return hud->target_code(ctrl, code);
        todo_once("target code " + std::to_string(code) + (ctrl ? " ctrl" : ""), "seg11:0x772e");
    }
    void toggle_manual_target() override {
        if (hud) return hud->toggle_manual_target();
        todo_once("Ctrl+T", "seg11:0x7138");
    }
    void zoom_changed(std::int32_t zoom) override {
        if (hud) return hud->zoom_changed(zoom);
        app::log("radar zoom " + std::to_string(zoom) + " (seg11:0x723a)");
    }
    void show_frame_rate() override {
        if (hud) return hud->show_frame_rate();
        todo_once("B", "seg11:0x71d4");
    }
    void show_messages() override {
        if (hud) return hud->show_messages();
        todo_once("M", "seg11:0x71a9");
    }
    void post_main(std::uint32_t msg) override {
        if (!app::post(static_cast<UINT>(msg))) app::log("wave: the post failed");
    }
    void speed_step(int d) override;
    void rescale_step(int d) override {
        todo_once("rescale " + std::to_string(d), "seg10:0x9a2a");
    }
    void unported(std::uint32_t msg, std::uint16_t key, const char* address) override {
        char buf[64];
        std::snprintf(buf, sizeof buf, "key msg 0x%x key 0x%x", msg, key);
        todo_once(buf, address);
    }
};

KeyActions* default_actions_hud();

struct Runtime {
    World* world = nullptr;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    KeyState ks;
    KeyRemap remap;
    JoyState joy;
    JoyButtons jb;
    DefaultHost default_host;
    DefaultActions default_actions;
    UpdateHost* host = nullptr;
    bool settings_read = false;
    std::function<std::int32_t()> zoom;  // DGROUP:0xa6c's owner (M3 s2 fixer)

    UpdateHost& h() { return host ? *host : default_host; }
    KeyActions& a() { return default_actions; }
};

Runtime& rt() {
    static Runtime r;
    return r;
}

KeyActions* default_actions_hud() { return rt().default_actions.hud; }

// The INI keys the flight model reads (seg6:0xb3e: `Braking` DGROUP:0x8ff6, `JoyStick`
// DGROUP:0x8ff4, `JBtn0` ... `JBtn3` DGROUP:0x1dcc) and the remap strings 2900 and 2901
// (seg6:0x178d) [BYTES seg6:0xcf4 seg6:0xc4a seg6:0xbeb seg6:0x1785-0x17a4].
void read_settings(Runtime& r) {
    try {
        if (g.ini) {
            r.g.braking = static_cast<std::int16_t>(g.ini->get_int(settings::kGameSection, "Braking", 0));
            r.g.joystick = static_cast<std::int16_t>(g.ini->get_int(settings::kGameSection, "JoyStick", 0));
            for (int i = 0; i < 4; ++i)
                r.jb.config[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(
                    g.ini->get_int(settings::kGameSection, "JBtn" + std::to_string(i), i));
        }
        if (g.store) {
            const auto a = g.store->find_string(2900);
            const auto b = g.store->find_string(2901);
            r.remap = KeyRemap::from_strings(a.value_or(""), b.value_or(""));
            if (!a || !b) r.remap.from[0] = 'Y';  // no strings: no remap [DESIGN]
        } else {
            r.remap.from[0] = 'Y';
        }
    } catch (const std::exception& x) {
        app::log(std::string("flight settings: ") + x.what());
        r.remap.from[0] = 'Y';
    }
    r.settings_read = true;
}

// The World's set-up values into the flight globals, before each frame [DESIGN]: the set-up
// owns these DGROUP words; the flight role keeps its own and writes back the shared ones.
void sync_in(Runtime& r, shell::GameState& s) {
    World& w = *r.world;
    WaveGlobals& gl = r.g;
    gl.now = s.now();
    gl.player = w.player;
    gl.last_ship = w.last_ship;
    gl.ship_pool = w.roles.ships;
    gl.rock_pool = w.roles.rocks;
    gl.pool5 = w.roles.pool5;
    gl.gates = w.gates;
    gl.play_mode = w.play_mode;
    gl.demo = w.player_game;
    gl.wave_kind = w.kind;
    gl.size = w.size;
    gl.detail = w.detail;
    gl.demons_gate = w.handover.demons_gate;
    gl.dg_9e49 = w.w9e49;
    gl.dg_9eb4 = static_cast<std::uint8_t>(w.w9eb4);
    gl.dg_94e2 = w.w94e2;
    gl.game_frames = w.frames;
    gl.burst_count = w.w9510;
    r.f.race = w.handover.race;
    r.f.dg_950 = w.handover.duel;
    r.f.dg_9600 = w.w9600;
    r.f.entry_pending = w.w9140;
    r.f.info_mode = w.w0a76;
    r.view.play_w = w.W;
    r.view.play_h = w.H;
    r.view.shift = w.shift;
}

void sync_out(Runtime& r) {
    World& w = *r.world;
    w.frames = r.g.game_frames;
    w.w9510 = r.g.burst_count;
    w.w9140 = r.f.entry_pending;
    w.w0a76 = r.f.info_mode;
    w.play_mode = r.g.play_mode;
}

UpdateCtx make_ctx(Runtime& r) {
    World& w = *r.world;
    return UpdateCtx{FlightCtx{w.pools, w.ships, r.g, r.keys, r.h(), shell::game_rng()},
                     r.f, r.view, r.h(), r.joy, r.jb};
}

// seg11:0x72b2 reads the stick into far DGROUP:0x1e4c every 50 ms; the port reads joystick 1
// with joyGetPos (Win16's coordinates, 0 ... 65535) [DESIGN] [OPEN: seg11:0x72b2 is not read].
void DefaultHost::joy_read() {
    JOYINFO ji{};
    Runtime& r = rt();
    if (joyGetPos(JOYSTICKID1, &ji) != JOYERR_NOERROR) {
        r.joy = JoyState{};
        r.joy.x = r.joy.y = 0x8000;  // centred
        return;
    }
    r.joy.x = static_cast<std::uint16_t>(ji.wXpos);
    r.joy.y = static_cast<std::uint16_t>(ji.wYpos);
    for (int i = 0; i < 4; ++i)
        r.joy.buttons[static_cast<std::size_t>(i)] = (ji.wButtons & (1u << i)) ? 1 : 0;
}

// Alt + and Alt - : seg10:0x7e1c's rate rule, the scheduler's rate (ADR 0001 decision 2), the
// speed display (DGROUP:0xa76 = 4 for 5000 ms) and `FrameRateSet` in INSPACE.INI.
void DefaultActions::speed_step(int d) {
    Runtime& r = rt();
    const int rate = speed_step_rate(g.scheduler.rate(), d);
    g.scheduler.set_rate(rate, app::now_tick());
    r.f.speed_shown = r.g.now + 5000u;  // DGROUP:0x870c [BYTES seg10:0x7e35]
    r.f.info_mode = 4;                  // [BYTES seg10:0x7e3f]
    // DGROUP:0xa76 is the World's word; the frame's sync would otherwise undo the 4, and the
    // left display repaints (M3 s2 fixer).
    if (r.world) r.world->w0a76 = 4;
    if (default_actions_hud()) default_actions_hud()->info_repaint();
    app::log("FrameRateSet " + std::to_string(rate) + " (seg10:0x7e1c)");
    try {
        if (g.ini) {
            g.ini->set_int(settings::kGameSection, "FrameRateSet", rate);
            g.ini->save();
        }
    } catch (const std::exception& x) {
        app::log(std::string("INSPACE.INI: ") + x.what());
    }
}

// The state-1 frame hook: the pass, then the frame.
void wave_hook(shell::GameState& s) {
    Runtime& r = rt();
    if (!r.world) {
        r.default_host.sound_frame();
        return;
    }
    if (!r.settings_read) read_settings(r);
    sync_in(r, s);
    UpdateCtx c = make_ctx(r);
    wave_pass(c, g.scheduler.rate() > shell::kMaxRateThreshold);
    wave_frame(c);
    sync_out(r);
}

// State 1's input: the router seg11:0x6aa8 (the remap seg11:0xb306 of WM_KEYDOWN and WM_KEYUP)
// and the switch seg11:0x6b64 for the keys of this stage. Everything else goes on to the
// previous route_input (the briefing's ESC).
bool route_wave(shell::GameState& s, const shell::Input& in) {
    Runtime& r = rt();
    if (s.state() != shell::kWave || !r.world) return false;
    if (r.g.dg_9130 != 0) return false;  // seg18:0xd4e [OPEN]
    // This stage ports the WM_KEYDOWN, 0x99b and 0x99e keys; WM_KEYUP (ESC, R, the strategy
    // keys) goes on to the previous route_input.
    if (in.msg != kWmKeyDown && in.msg != kMsgAltKey && in.msg != kMsgCtrlKey) return false;
    if (!r.settings_read) read_settings(r);
    std::uint16_t key = static_cast<std::uint16_t>(in.wparam);
    if (in.msg == kWmKeyDown || in.msg == kWmKeyUp) key = r.remap.map(key);
    const World& w = *r.world;
    const int p = w.player;
    const std::uint32_t lockout = w.ships[static_cast<std::size_t>(p)].lockout.get();
    const std::int16_t pstate = w.pools.records(w.roles.ships) > p ? w.pools.rec(w.roles.ships, p).state : 0;
    r.g.now = s.now();
    if (r.zoom) r.ks.zoom = r.zoom();  // DGROUP:0xa6c is the radar's (M3 s2 fixer)
    const KeyResult res = key_message(in.msg, key, r.g, lockout, pstate, r.keys.down(kVkControl),
                                      r.keys.down(kVkMenu), r.ks, r.a());
    return res != KeyResult::kNotMine;  // the others go on (the shell logs them)
}

}  // namespace

void bind_flight_world(World* w) {
    Runtime& r = rt();
    r.world = w;
    // The flight globals stand for DGROUP words and live for the run, as DGROUP does (the 0 ... 8
    // cycle DGROUP:0x95f6 runs on across waves); the INI keys are read again for the new wave
    // [DESIGN].
    if (w) r.settings_read = false;
}
void set_update_host(UpdateHost* host) { rt().host = host; }
void set_key_actions(KeyActions* actions) { rt().default_actions.hud = actions; }
WaveLinks& wave_links() { return g_links; }
void set_zoom_source(std::function<std::int32_t()> zoom) { rt().zoom = std::move(zoom); }
void wave_started() { rt().keys.clear_pressed(); }
WaveGlobals& flight_globals() { return rt().g; }
FrameGlobals& frame_globals() { return rt().f; }
ViewState& flight_view() { return rt().view; }
KeyLatch& key_latch() { return rt().keys; }
KeyState& key_state() { return rt().ks; }

void recentre_view(World& w) {
    Runtime& r = rt();
    r.view.play_w = w.W;
    r.view.play_h = w.H;
    r.view.shift = w.shift;
    ViewInputs vi;
    const ObjectRecord& p = w.pools.rec(w.roles.ships, w.player);
    vi.player_x = p.x;
    vi.player_y = p.y;
    vi.autopilot = r.g.autopilot;
    vi.shake = r.g.dg_238c;
    vi.wave_kind = w.kind;
    vi.dg_9eb5 = r.g.dg_9eb5;
    recentre(r.view, vi);
}

ScreenPoint flight_to_screen(std::int16_t x, std::int16_t y) {
    Runtime& r = rt();
    return world_to_screen(r.view, x, y, r.g.wave_kind, r.g.dg_9eb5, r.g.dg_9130);
}

void feed_key(std::uint32_t msg, std::uintptr_t wparam, std::intptr_t lparam) {
    const auto vk = static_cast<std::uint16_t>(wparam & 0xff);
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
        rt().keys.key_down(vk, (static_cast<std::uintptr_t>(lparam) & (1u << 30)) != 0);
    else if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
        rt().keys.key_up(vk);
}

}  // namespace is::world

namespace is::app {

void install_flight_hooks(App& app) {
    using namespace is::world;
    auto& h = app.game.hooks();
    Runtime& r = rt();
    r.default_host.prev_frame = h.frame[shell::kWave];
    h.frame[shell::kWave] = [](shell::GameState& s) {
        rt().default_host.state = &s;
        wave_hook(s);
    };
    auto prev_route = h.route_input;
    h.route_input = [prev_route](shell::GameState& s, const shell::Input& in) {
        if (route_wave(s, in)) return;
        if (prev_route) prev_route(s, in);
    };
}

}  // namespace is::app

#endif  // _WIN32
