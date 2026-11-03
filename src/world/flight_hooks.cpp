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
#include "audio/audio.h"
#include "dialogs/audio_options.h"
#include "shell/rng.h"
#include "world/debris.h"
#include "world/effects.h"
#include "world/gate_exit.h"
#include "world/hooks.h"
#include "world/player_sheet.h"
#include "world/ambulance.h"
#include "world/capture.h"
#include "world/missile.h"
#include "world/shots.h"
#include "world/visibility.h"
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
    std::uint16_t joy_read() override;
    // Roster byte +0x33 from the World's roster, the record seg13:0x33d6 reads too (rating.h
    // RosterRecord::b33) [BYTES seg11:0x6644] (M5 s2 fixer, review M4).
    std::int16_t roster_byte_33(int roster) override;
    void mover(Mover m) override;
    // ---- M4 s3 fixer: the ambulance on the program's World (ambulance.h; review H2)
    // seg16:0xa2 at the pass's step 20 while pool 7 record 0 has `state` 0 [BYTES seg7:0xc61-0xc7a].
    void seg16_a2() override;
    // seg16:0x33a2 and seg16:0x3306 once the dword DGROUP:0x6da8 has passed [BYTES seg7:0xc7c-0xc8f].
    bool seg16_33a2() override;
    void seg16_3306() override;
    // seg13:0x3748(i), the served ship's part of the ship mover (flight.cpp) [BYTES seg13:0x3748].
    void seg13_3748(int ship) override;
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
    // seg11:0x4e0a and seg8:0x3aec are UpdateHost's defaults on the program's World (shots.h)
    // (M4 s2 fixer).
    // seg13:0x675e at the ship mover's head: the HUD's part (hud/wave.h pictures_tick) [BYTES
    // seg13:0x15].
    void seg13_675e() override {
        if (g_links.pictures) g_links.pictures();
    }
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
        // seg12:0x2556 + seg7:0x13b2, the entry materialise (src/hud wave_win32.cpp install_wave)
        if (sprite_links().ship_entered) sprite_links().ship_entered(ship);
    }
    void seg7_158e() override {
        if (sprite_links().entry_step) sprite_links().entry_step();
    }
    void flame_tail(int on, int ship) override {
        if (sprite_links().flame_tail) sprite_links().flame_tail(on, ship);
    }
    // seg10:0x754a(id, pool, rec, mode) through M7's engine (src/dialogs/audio_options.h
    // is::sound) (M4 s1 COLLIDE).
    void sound(int id, int pool, int rec, int mode) override { is::sound::play_at(id, pool, rec, mode); }
    // ---- M4 s2 CAPTURE: the touch routine's and the icon mover's calls
    // seg10:0x744c(id) through M7's engine.
    void play(int id) override { is::sound::play(id); }
    std::string lang(int id) override {
        try {
            if (g.store) return g.store->find_string(static_cast<std::uint16_t>(id)).value_or("");
        } catch (const std::exception&) {
        }
        return {};
    }
    void seg18_551a(int, int) override { todo_once("weapons 0x18/0x1a", "seg18:0x551a"); }
    void seg18_59ec(int, int) override { todo_once("weapons 0x19/0x1a", "seg18:0x59ec"); }
    void seg7_4d86(int) override { todo_once("icon word DGROUP:0x57a8", "seg7:0x4d86"); }
    void seg7_4ffe(int) override { todo_once("icon after DGROUP:0x57a8", "seg7:0x4ffe"); }
    void seg7_5660(int) override { todo_once("icon heat -2", "seg7:0x5660"); }
    void seg7_477e(int) override { todo_once("icon meets icon", "seg7:0x477e"); }
    void icon_pool5_hit(int, int) override { todo_once("icon meets pool 5", "seg7:0x4a8a"); }
    CollideHost& collide_host() override;
    CollideGlobals& collide_globals() override;
    // ---- M4 s1 fixer: the stage's calls on the program's World
    // seg8:0x3fbe(k) at the pass's step 5 (effects.h effect_frame).
    void effect(int k) override;
    // seg13:0x447a at the frame's end (debris.h move_debris).
    void seg13_447a() override;
    // seg12:0x2288(pool, rec): the gate exit (world/hooks.h run_gate_exit, gate_exit.h).
    void gate_passed(int pool, int rec) override;
    // seg12:0x274a(pool, rec) at the tumble's end of a dead ship [BYTES seg13:0x181] and from
    // CollideHost::destroy (effects.h object_destroyed).
    void seg12_274a(int pool, int rec) override;
    // The speed or message display's end [BYTES seg7:0xba7-0xbea]: seg3:0x6ec(player, 14,
    // player word +0x58, 2) while DGROUP:0x9608 != 0, else seg7:0x119c(2, 0).
    void speed_display_end_3_6ec() override;
    void speed_display_end_7_119c() override;
};

CollideLinks g_collide_links;
World* rt_world();

// The program's collision calls out (M4 s1 COLLIDE).
class ProgramCollideHost final : public CollideHost {
public:
    UpdateHost* u = nullptr;
    void sound(int id, int pool, int rec, int mode) override { u->sound(id, pool, rec, mode); }
    void message(int ship, int pool, int rec, int code) override {
        if (g_collide_links.message) return g_collide_links.message(ship, pool, rec, code);
        todo_once("message " + std::to_string(code), "seg3:0x6ec");
    }
    // seg12:0x274a and seg16:0x8e4 on the program's World (effects.h) (M4 s1 fixer).
    void destroy(int pool, int rec) override;
    void ship_destroyed(int pool, int ship) override;
    // seg8:0x2cd8's debris: the free entry of DGROUP:0x915c (debris.h debris_start). The wing's
    // strip image seg10:0x7370 and its mask are not read: the entry has no picture, so only its
    // erase squares show [OPEN seg10:0x7370].
    bool wing_debris(int ship, int side) override;
    // seg8:0x81f8(pool, rec): the hit history DGROUP:0x937a the 800 award searches (gate_exit.h
    // push_hit) [BYTES seg8:0x820f seg8:0x8240 seg8:0x8246 seg8:0x824c]; seg8:0x81f8's other
    // work, if any, is [OPEN].
    void seg8_81f8(int pool, int rec) override;
    void seg7_2080(int pool, int rec) override {
        if (g_collide_links.in_use) return g_collide_links.in_use(pool, rec);
        // seg7:0x2080 on the program's World (missile.h recount_in_use) (M4 s2 fixer).
        if (rt_world()) recount_in_use(rt_world()->pools, pool);
    }
    void flame_off(int ship) override;
    void erase(int x, int y, int w, int h) override {
        if (Blitter* out = u->blitter()) out->pat_blt(kDcWindow, x, y, w, h, kBlackness);
    }
    void seg13_b6f2(int, int) override { todo_once("pilot goal", "seg13:0xb6f2"); }
    void seg11_947a(int, int, int) override { todo_once("tether end", "seg11:0x947a"); }
    void wing_design(int) override { todo_once("wing design words", "seg14:0x3eda"); }
    void wing_taunts(int, int) override { todo_once("wing taunts", "seg8:0x2da6"); }
    // ---- M4 s2 fixer: the icon's on_damage seg7:0x233c and its shatter (damage.h)
    // seg7:0x5ae8(rec, arg) on the program's World (capture.h icon_counted).
    void icon_gone(int rec, int arg) override;
    // seg7:0x65aa(pool, rec): the IS_ICON column's row (hud refresh_object through WaveEndLinks).
    void column_row(int pool, int rec) override {
        if (wave_end_links().column_row) return wave_end_links().column_row(pool, rec);
        todo_once("the icon column's row", "seg7:0x65aa");
    }
    // The crack and the fragment pictures on the strip DC DGROUP:0x3a02 (the HUD's GDI side).
    void icon_crack(const CrackPlan& p) override {
        if (g_collide_links.icon_crack) return g_collide_links.icon_crack(p);
        todo_once("the icon crack", "seg7:0x23cf");
    }
    void fragment_bitmap(int r, std::int16_t side) override {
        if (g_collide_links.fragment_bitmap) g_collide_links.fragment_bitmap(r, side);
    }
    void fragment_pictures(const FragmentPlan& p) override {
        if (g_collide_links.fragment_pictures) return g_collide_links.fragment_pictures(p);
        todo_once("the icon fragments' pictures", "seg7:0x2a20");
    }
    // The far call through +0x66 from seg7:0x2652 (call_on_destroy).
    void on_destroy(int pool, int rec) override;
    std::int16_t other_damage(std::uint32_t id, int pool, int, std::int16_t) override {
        char buf[64];
        std::snprintf(buf, sizeof buf, "on_damage seg%u:0x%x pool %d", id >> 16, id & 0xffffu, pool);
        todo_once(buf, "seg8:0x647c");
        return 0;
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
    // WM_KEYDOWN 'H' (seg11:0x6f7a-0x6fe8): ambulance.h ambulance_key_program (M4 s3 fixer, review H1).
    void ambulance(bool ctrl, bool alt) override;
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
    ProgramCollideHost collide_host;
    CollideGlobals collide;
    RepairState repair;
    // The wing debris DGROUP:0x915c and the smoke table DGROUP:0x58c4 (debris.h); the placement
    // seg9:0x6624 frees them at the wave start (wave_started) [BYTES seg9:0x6624].
    DebrisTable debris{};
    SmokeTable smoke{};
    UpdateHost* host = nullptr;
    bool settings_read = false;
    std::function<std::int32_t()> zoom;  // DGROUP:0xa6c's owner (M3 s2 fixer)
    // The frames-per-second count of the wave tick (timing.md "Every 1000 ms"): the frames
    // DGROUP:0x586, the deadline DGROUP:0x1e8c and the last second's count DGROUP:0x59a.
    std::uint16_t frames_586 = 0;
    std::uint32_t due_1e8c = 0;
    std::int16_t actual_59a = 0;

    UpdateHost& h() { return host ? *host : default_host; }
    KeyActions& a() { return default_actions; }
};

Runtime& rt() {
    static Runtime r;
    return r;
}

KeyActions* default_actions_hud() { return rt().default_actions.hud; }
World* rt_world() { return rt().world; }

// The pool-6 mover's and the weapon system's calls (missile.h MissileLinks) on the program's
// World (M4 s2 fixer); paint_weapons is the HUD's (src/hud/wave_win32.cpp).
UpdateCtx make_ctx(Runtime& r);
void bind_missile_links() {
    MissileLinks& l = missile_links();
    l.sound = [](int id, int pool, int rec, int mode) { rt().h().sound(id, pool, rec, mode); };  // seg10:0x754a
    l.capture_count = [](int ship, int v) {  // seg13:0x4eec
        if (rt().world) bump_w82(ship_at(*rt().world, ship), static_cast<std::int16_t>(v));
    };
    l.destroy = [](int pool, int rec) { rt().h().seg12_274a(pool, rec); };  // seg12:0x274a
    l.on_destroy = [](int pool, int rec) {
        Runtime& r = rt();
        r.collide_host.u = &r.h();
        r.collide_host.on_destroy(pool, rec);
    };
    l.to_screen = [](std::int16_t x, std::int16_t y) {  // seg8:0x29ea
        const ScreenPoint p = flight_to_screen(x, y);
        return Point16{p.x, p.y};
    };
    l.touch = [](int ship, std::int16_t pr) {  // seg13:0x16ac
        Runtime& r = rt();
        if (!r.world) return;
        UpdateHost& h = r.h();
        UpdateCtx local = make_ctx(r);
        UpdateCtx& c = h.frame_ctx ? *h.frame_ctx : local;
        CollideCtx x = frame_collide_ctx(c, *r.world);
        CaptureCtx cc{x, c.flight, h.capture_host()};
        ::is::world::touch(cc, ship, pr);
    };
    l.ship_touched = [](int ship) {  // seg13:0x6716
        Runtime& r = rt();
        if (!r.world) return;
        UpdateCtx c = make_ctx(r);
        ship_touched(c.flight, ship);
    };
    l.unported = [](const char* address, int, int) { todo_once("missile branch", address); };
}

UpdateCtx make_ctx(Runtime& r);

CollideHost& DefaultHost::collide_host() {
    Runtime& r = rt();
    r.collide_host.u = &r.h();
    return r.collide_host;
}
CollideGlobals& DefaultHost::collide_globals() { return rt().collide; }

void ProgramCollideHost::flame_off(int ship) {
    Runtime& r = rt();
    if (!r.world) return;
    UpdateCtx c = make_ctx(r);
    ship_touched(c.flight, ship);  // seg13:0x6716
}

// ---- M4 s1 fixer: the effects, the death, the debris and the gate on the program's World

// The far call of a record's `on_destroy` (+0x66) [BYTES seg13:0x4a1d]: the two handlers the
// port has, seg16:0x8e4 (a ship) and seg12:0x274a (the generic one); any other logs [OPEN].
CollideCtx program_collide_ctx(Runtime& r);

void call_on_destroy(EffectCtx& c, int pool, int rec) {
    const std::uint32_t id = obj(c.w, pool, rec).on_destroy.get();
    if (id == kDestroyShip) return ship_destroyed(c, pool, rec);
    if (id == kDestroyGeneric) return object_destroyed(c, pool, rec);
    // seg7:0x2e02 (the pick-up) and seg12:0x6a60 (pool 11) through collide.h run_on_destroy
    // (M4 s3 fixer, review M5) [BYTES seg7:0x2e02 seg12:0x6a60].
    if (id == kDestroyPickup || id == kDestroyPool11) {
        Runtime& r = rt();
        if (!r.world) return;
        r.collide_host.u = &r.h();
        CollideCtx x = program_collide_ctx(r);
        return run_on_destroy(x, pool, rec);
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, "on_destroy seg%u:0x%x pool %d", id >> 16, id & 0xffffu, pool);
    todo_once(buf, "seg13:0x4a1d");
}

// The collision words of the program for a call outside the movers (the blast of seg8:0x5ab0
// runs in the pass): the pass's own view, visibility list and DGROUP:0x943a / 0x943c / 0x9454.
CollideCtx program_collide_ctx(Runtime& r) {
    return CollideCtx{*r.world, r.g,           r.h().collide_globals(), r.h().collide_host(),
                      r.view,   last_visibility(), shell::game_rng(),      r.f.dg_943a,
                      r.f.dg_943c, r.f.dg_9454};
}

class ProgramEffectHost final : public EffectHost {
public:
    void sound(int id, int pool, int rec, int mode) override { rt().h().sound(id, pool, rec, mode); }
    void player_sprite() override { rt().h().player_sprite(); }
    // seg8:0x5df2 from the blast (COLLIDE's dispatcher; a negative amount is its `neg` path).
    void hit(int pool, int rec, int sp, int sr, std::int16_t amount) override {
        Runtime& r = rt();
        if (!r.world) return;
        r.collide_host.u = &r.h();
        CollideCtx x = program_collide_ctx(r);
        dispatch(x, pool, rec, sp, sr, amount);
    }
    std::int16_t design_kind(int design) override { return rt().h().design_kind(design); }
    void destroy(int pool, int rec) override;
    void seg7_2080(int pool, int arg) override { rt().collide_host.seg7_2080(pool, arg); }
};

ProgramEffectHost g_effect_host;

// seg8:0x29ea for the effects and the debris.
Point16 effect_to_screen(std::int16_t x, std::int16_t y) {
    const ScreenPoint p = flight_to_screen(x, y);
    return Point16{p.x, p.y};
}

// Runs f on the effects' context of the program's World: the image table from the HUD's link,
// the wave Blitter, DGROUP:0x1e50, DGROUP:0x951a and DGROUP:0x1318 (0 in the image, its writer
// [OPEN]) [BYTES DGROUP:0x1318].
template <typename F>
void with_effects(F f) {
    Runtime& r = rt();
    if (!r.world) return;
    r.collide_host.u = &r.h();
    const EffectWords in{r.g.now, r.g.autopilot, 0};
    EffectCtx c{*r.world,         effect_state(),
                in,               g_effect_host,
                shell::game_rng(), wave_blitter(),
                g_links.images ? g_links.images() : nullptr,
                effect_to_screen, &r.f.dg_943a,
                &r.f.dg_943c};
    f(c);
}

void ProgramEffectHost::destroy(int pool, int rec) {
    with_effects([&](EffectCtx& c) { call_on_destroy(c, pool, rec); });
}

void DefaultHost::effect(int k) {
    with_effects([k](EffectCtx& c) { effect_frame(c, k); });
}

void DefaultHost::seg13_447a() {
    Runtime& r = rt();
    if (!r.world) return;
    World& w = *r.world;
    // W, H: DGROUP:0x90ea, 0x90ec
    DebrisCtx c{r.debris,       r.smoke,         w.w9456,           w.pools,
                shell::game_rng(), r.g.size,     r.g.dg_9130,       w.W,
                w.H,            r.g.detail,      wave_blitter(),    effect_to_screen,
                last_visibility(), [] { rt().h().player_sprite(); }};
    move_debris(c);
}

void DefaultHost::gate_passed(int pool, int rec) { run_gate_exit(pool, rec, rt().g.now); }

void DefaultHost::seg12_274a(int pool, int rec) {
    with_effects([&](EffectCtx& c) { object_destroyed(c, pool, rec); });
}

void DefaultHost::speed_display_end_3_6ec() {
    Runtime& r = rt();
    if (!r.world) return;
    const int p = r.g.player;
    r.collide_host.message(p, r.g.ship_pool, ship_at(*r.world, p).w58, 2);  // [BYTES seg7:0xbcc-0xbdf]
}

void DefaultHost::speed_display_end_7_119c() {
    if (g_links.info_target) return g_links.info_target();
    todo_once("the information display back to the target", "seg7:0x119c");
}

void ProgramCollideHost::icon_gone(int rec, int arg) {
    Runtime& r = rt();
    if (r.world) icon_counted(*r.world, rec, arg, r.g.ship_pool);  // seg7:0x5ae8
}

void ProgramCollideHost::on_destroy(int pool, int rec) {
    with_effects([&](EffectCtx& c) { call_on_destroy(c, pool, rec); });  // [BYTES seg7:0x26b1]
}

void ProgramCollideHost::destroy(int pool, int rec) {
    with_effects([&](EffectCtx& c) { object_destroyed(c, pool, rec); });  // seg12:0x274a
}

void ProgramCollideHost::ship_destroyed(int pool, int ship) {
    with_effects([&](EffectCtx& c) { ::is::world::ship_destroyed(c, pool, ship); });  // seg16:0x8e4
}

bool ProgramCollideHost::wing_debris(int ship, int side) {
    (void)side;  // the side picks the strip's offsets of seg8:0x2cd8 [OPEN]
    Runtime& r = rt();
    if (!r.world) return false;
    return debris_start(r.debris, obj(*r.world, r.g.ship_pool, ship)) >= 0;
}

void ProgramCollideHost::seg8_81f8(int pool, int rec) {
    if (rt().world) push_hit(*rt().world, pool, rec);
}

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
    // DGROUP:0x952e, 0x9e5b and 0x6da8: the ambulance's words (ambulance.h), copied in as
    // DGROUP:0x9e49 is (M4 s3 fixer, review H2) [DESIGN].
    gl.dg_952e = w.w952e;
    gl.dg_9e5b = ambulance_state().served_alive;
    r.f.dg_6da8 = ambulance_state().reentry_due;
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

// ISPACE.DLL ordinal 41 (seg10:0x6b54), which seg11:0x72b2 calls every 50 ms (world.md
// "Joystick") with joystick 0 and the far pointer DGROUP:0x1e4c [BYTES seg10:0x6b5e seg10:0x6b60
// seg10:0x6b65]. Ordinal 41 reads the PC game port itself: seg9:0x3ffe takes the buttons from
// `in al, 0x201` and times axis bits 0 and 1 of port 0x201 with interrupts off; a count that
// reaches 3 * 65536 (no stick, or no game port) returns 0, and ordinal 41 then calls
// MessageBeep(0) and returns 0xa7, so the caller clears `JoyStick` [BYTES ISPACE.DLL seg9:0x4010
// seg9:0x401f seg9:0x405d seg9:0x406f seg9:0x4079 seg9:0x4094 seg9:0x40c6 seg9:0x3ebf
// seg9:0x3ec4 seg9:0x3ec9]. On success it stores the words x, y at info+0, +2 and the four
// button bytes at info+4 ... +7; when ISPACE.DLL DGROUP:0x2294 = 3 the axes are rescaled to
// 0 ... 0xffff from the calibration [BYTES ISPACE.DLL seg9:0x3ed4 seg9:0x3f03 seg9:0x3f4d
// seg9:0x3f51 seg9:0x3f9b seg9:0x3fe8]. Win32 user mode cannot read port 0x201: the port reads
// joystick 0 with winmm's joyGetPos, whose 0 ... 65535 axes stand for the calibrated range, and
// any failure reads as the original's 0xa7 with its beep [DESIGN ADR 0013 decision 6,
// win16-translation.md "joystick"].
std::int16_t DefaultHost::roster_byte_33(int roster) {
    const World* w = rt().world;
    if (!w || roster < 0 || roster >= static_cast<int>(w->roster.size())) return 0;
    return w->roster[static_cast<std::size_t>(roster)].b33[0];
}

std::uint16_t DefaultHost::joy_read() {
    JOYINFO ji{};
    Runtime& r = rt();
    const MMRESULT rc = joyGetPos(JOYSTICKID1, &ji);
    if (rc != JOYERR_NOERROR) {
        r.joy = JoyState{};
        r.joy.x = r.joy.y = 0x8000;  // centred
        MessageBeep(0);              // [BYTES ISPACE.DLL seg9:0x3ec3 seg9:0x3ec4]
        return 0xa7;                 // [BYTES ISPACE.DLL seg9:0x3ec9]
    }
    r.joy.x = static_cast<std::uint16_t>(ji.wXpos);
    r.joy.y = static_cast<std::uint16_t>(ji.wYpos);
    for (int i = 0; i < 4; ++i)
        r.joy.buttons[static_cast<std::size_t>(i)] = (ji.wButtons & (1u << i)) ? 1 : 0;
    return 0;
}

// Alt + and Alt - : seg10:0x7e1c's rate rule, the scheduler's rate (ADR 0001 decision 2), the
// speed display (DGROUP:0xa76 = 4 for 5000 ms) and `FrameRateSet` in INSPACE.INI. The original
// sets the display's deadline and mode and posts its WM_PAINT only while the left display
// DGROUP:0xa7c exists [BYTES seg10:0x7e2a seg10:0x7e2f seg10:0x7e51]; in a wave it always
// exists, and the port's key switch runs only in a wave [DESIGN].
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

void DefaultActions::ambulance(bool ctrl, bool alt) {
    Runtime& r = rt();
    if (!r.world) return;
    const int code = ambulance_key_program(*r.world, r.g.now, ctrl, alt);
    if (code != 0) app::log("wave: H, the ambulance's reply " + std::to_string(code) + " (seg11:0x6fd1)");
    // The frame's copies of DGROUP:0x9e49 / 0x952e (the next sync_in copies them too) [DESIGN].
    r.g.dg_9e49 = r.world->w9e49;
    r.g.dg_952e = r.world->w952e;
}

void DefaultHost::mover(Mover m) {
    // seg12:0x3e98 returns at once outside a kind-6 wave [BYTES seg12:0x3ea8-0x3eaf]: only a race
    // wave misses its body (M4 s3 fixer, review L2).
    if (m == Mover::k12_3e98 && rt().g.wave_kind != 6) return;
    todo_once("wave mover", mover_address(m));
}

void DefaultHost::seg16_a2() {
    Runtime& r = rt();
    if (r.world && frame_ctx) ambulance_frame(*frame_ctx, *r.world);
}

bool DefaultHost::seg16_33a2() {
    Runtime& r = rt();
    return r.world && ambulance_reentry_clear_program(*r.world, r.g.now);
}

void DefaultHost::seg16_3306() {
    Runtime& r = rt();
    if (r.world) ambulance_reentry_program(*r.world, r.g, r.f);
}

void DefaultHost::seg13_3748(int ship) {
    Runtime& r = rt();
    if (r.world && frame_ctx) served_ship_move_program(frame_ctx->flight, *r.world, ship);
}

// The wave tick's frame count: every wave frame increments DGROUP:0x586; once the game clock
// DGROUP:0x1e50 passes the deadline DGROUP:0x1e8c (`jae` skips while deadline >= now, unsigned)
// the deadline becomes now + 1000, the count goes to DGROUP:0x59a whatever DGROUP:0x592 and the
// rate are, WM_PAINT goes to the left display DGROUP:0xa7c while DGROUP:0xa76 = 4 (the speed
// display) and the count restarts [BYTES seg7:0xa60 seg7:0xa6c seg7:0xa71 seg7:0xa7b seg7:0xad9
// seg7:0xadc seg7:0xadf seg7:0xaf2 seg7:0xaf7]. The threshold rule between (seg7:0xa7b-0xad5)
// is the scheduler's (ADR 0001) and stays there. The screen states' writer of the same words
// (seg7:0x232c) is not ported, so before the wave's first second DGROUP:0x59a reads 0
// [OPEN seg7:0x232c].
void frame_count(Runtime& r) {
    ++r.frames_586;  // [BYTES seg7:0xa60]
    if (r.due_1e8c >= r.g.now) return;  // [BYTES seg7:0xa6c seg7:0xa71]
    r.due_1e8c = r.g.now + 1000u;       // [BYTES seg7:0xa7b]
    r.actual_59a = static_cast<std::int16_t>(r.frames_586);  // [BYTES seg7:0xad9 seg7:0xadc]
    if (r.f.info_mode == 4 && default_actions_hud())          // [BYTES seg7:0xadf seg7:0xaf2]
        default_actions_hud()->info_repaint();
    r.frames_586 = 0;  // [BYTES seg7:0xaf7]
}

// The state-1 frame hook: the pass, then the frame.
void wave_hook(shell::GameState& s) {
    Runtime& r = rt();
    if (!r.world) {
        r.default_host.sound_frame();
        return;
    }
    if (!r.settings_read) read_settings(r);
    // The tick's deadline block seg7:0x5ce-0x707 before the pass: a deadline that fires ends
    // the tick there (the wave end, the history MessageBox, state 5) [DESIGN ADR 0001] (wave_end.h).
    if (run_wave_deadlines(s) == DeadlineResult::kEndPass) return;
    sync_in(r, s);
    UpdateCtx c = make_ctx(r);
    wave_pass(c, g.scheduler.rate() > shell::kMaxRateThreshold);
    frame_count(r);
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

// seg10:0x754a's mode-1, mode-2 and mode-3 points (sound.md "INSPACE wrappers" step 3): object
// words +4, +6 of record b of pool a; shot slot b's world head (+0x30, +0x48) + 12·row, row 1
// when the slot's word +0x84 (barrel 0 alive) is 0, else row 0 (shots.h) (M4 s2 fixer); and the
// dword DGROUP:0x5144 (effects.h EffectState::dg_5144). Step 4's seg8:0x29ea is the flight
// view's (M4 s1 fixer, review M14) [BYTES seg10:0x76d9-0x775c seg10:0x778b].
bool sound_source(int mode, int a, int b, int& x, int& y) {
    const World* w = rt().world;
    if (!w) return false;
    if (mode == 1) {
        if (a < 0 || a >= kPoolCount || b < 0 || b >= w->pools.records(a)) return false;
        const ObjectRecord& o = w->pools.rec(a, b);
        x = o.x;
        y = o.y;
        return true;
    }
    if (mode == 2) {
        if (b < 0 || b >= kShotSlots) return false;
        const ShotState& s = shot_state();
        const int row = s.word(b, shot::kAlive) == 0 ? 1 : 0;  // DGROUP:0x611c + 0x94·b
        x = s.word(b, shot::kWorldX + 12 * row);              // DGROUP:0x60c8
        y = s.word(b, shot::kWorldY + 12 * row);              // DGROUP:0x60e0
        return true;
    }
    if (mode == 3) {
        x = effect_state().dg_5144;
        y = effect_state().dg_5146;
        return true;
    }
    return false;
}

void bind_flight_world(World* w) {
    Runtime& r = rt();
    r.world = w;
    if (w)
        audio::set_world_source(
            sound_source,
            [](int& x, int& y) {
                const ScreenPoint p = flight_to_screen(static_cast<std::int16_t>(x), static_cast<std::int16_t>(y));
                x = p.x;
                y = p.y;
            },
            w->roles.ships);  // DGROUP:0x9324
    else
        audio::set_world_source(nullptr, nullptr, -1);
    // The flight globals stand for DGROUP words and live for the run, as DGROUP does (the 0 ... 8
    // cycle DGROUP:0x95f6 runs on across waves); the INI keys are read again for the new wave
    // [DESIGN].
    if (w) r.settings_read = false;
}
void set_update_host(UpdateHost* host) { rt().host = host; }
void set_key_actions(KeyActions* actions) { rt().default_actions.hud = actions; }
WaveLinks& wave_links() { return g_links; }
void set_zoom_source(std::function<std::int32_t()> zoom) { rt().zoom = std::move(zoom); }
void wave_started() {
    Runtime& r = rt();
    r.keys.clear_pressed();
    reset_collide_wave(r.collide);  // [DESIGN ADR 0015] decision 1
    r.repair.due = 0;               // [BYTES seg9:0x668d], the port at the wave start [DESIGN]
    // The placement seg9:0x6624 frees the smoke entries (DGROUP:0x58cc = -1) [BYTES seg9:0x6624];
    // the debris table's reset at the wave start is the port's [DESIGN].
    r.smoke = SmokeTable{};
    r.debris = DebrisTable{};
}
CollideLinks& collide_links() { return g_collide_links; }
void destroy_object(int pool, int rec) { rt().default_host.seg12_274a(pool, rec); }
CollideGlobals& collide_globals() { return rt().collide; }
RepairState& repair_state() { return rt().repair; }
void repair_tick() {
    Runtime& r = rt();
    if (!r.world) return;
    r.collide_host.u = &r.h();
    auto_repair(*r.world, r.repair, r.h().collide_host());  // seg11:0x1d7c
}
int frame_rate_actual() { return rt().actual_59a; }
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
    bind_missile_links();
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
