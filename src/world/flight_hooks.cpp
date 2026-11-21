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
#include <cstring>
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
#include "world/aggression.h"
#include "world/ambulance.h"
#include "world/callin.h"
#include "world/capture.h"
#include "world/cheats.h"
#include "world/commands.h"
#include "world/death.h"
#include "world/fuel_balls.h"
#include "world/icon_effects.h"
#include "world/law.h"
#include "world/lightning.h"
#include "world/missile.h"
#include "world/pilot.h"
#include "world/pilot_orders.h"
#include "world/pilot_steer.h"
#include "world/pilot_think.h"
#include "world/pilot_tick.h"
#include "world/player_tick.h"
#include "world/reactions.h"
#include "world/relations.h"
#include "world/rescale.h"
#include "world/ring.h"
#include "world/roster_build.h"
#include "world/setup.h"
#include "world/shift.h"
#include "world/ship_react.h"
#include "world/shots.h"
#include "world/smoke.h"
#include "world/tank.h"
#include "world/tether.h"
#include "world/visibility.h"
#include "world/wildcard.h"
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
    // seg13:0x675e at the ship mover's head: the ship loop and the All Ships notice
    // (pilot_tick.h watch_refresh), whose tail calls the HUD's part (hud/wave.h pictures_tick)
    // through PilotHost::watch_tail [BYTES seg13:0x15 seg13:0x675e-0x688d] (M5 s3 integrate,
    // THINK binding 1).
    void seg13_675e() override;
    // ---- M5 s3 integrate: the computer ship's body in the ship mover seg13:0x0 (flight.cpp)
    // seg4:0xbde(i), the steer (pilot_steer.h), STEER binding B1.
    void seg4_bde(int ship) override;
    // seg4:0x78(i), the think, and seg4:0x6e70(i), the weapon choice (pilot_think.h), THINK
    // binding 1.
    void seg4_78(int ship) override;
    void seg4_6e70(int ship) override;
    // The design team word DGROUP:0x970a + 0xad·n (roster_build.h design_team): the speed cap's
    // SPEED DEMONS, the entry's Enforcers, the death and the captures read it [BYTES seg11:0x5ce2
    // seg7:0x1d89] (STEER B3, CONTACT binding 2).
    std::int16_t design_kind(int design) override;
    // DGROUP:0x9344 and the special places DGROUP:0x4f2c + 0x10·k (World::places) for the
    // computer speed cap [BYTES seg11:0x5d7e seg11:0x5daa seg11:0x5db9] (STEER B5).
    std::int16_t special_places() override;
    RingPlace special_place(int k) override;
    // seg7:0x1208 through the HUD's link; the link clears the frame's DGROUP:0x58a
    // (FrameGlobals::dg_58a, which seg7:0x5e04 sets) when the sprite is drawn [BYTES seg7:0x13a2]
    // (src/hud/wave_win32.cpp player_sprite) (M4 s5 EFFECTS).
    void player_sprite() override;
    // seg7:0x5b68(pool, rec, arg) on the program's World (smoke.h smoke_puff) (M4 s5 EFFECTS).
    void puff(int pool, int rec, int arg) override;
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
    // seg7:0x2128, the 10-second violence decay, on the program's World (aggression.h) (M4 s5 ICONS).
    void seg7_2128() override;
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
    // ---- M4 s7: the tethers and the bolt on the program's World
    // seg8:0x1e32(pool, rec) from the integrator seg8:0x1b54 (tether.h tether_follow) (M4 s7 TETHER).
    void follow_owner(int pool, int rec) override { tether_follow_program(pool, rec); }
    // seg13:0x6e5-0xa57, the throw (tether.h tether_throw) (M4 s7 TETHER).
    void seg13_6e5(int ship) override { tether_throw_program(ship); }
    // seg12:0x3778's erase and recount at seg7:0xbed (tether.h TetherLinkHooks) (M4 s7 TETHER).
    LinkHooks* link_hooks() override { return program_link_hooks(); }
    // seg11:0xa9ea(arg), the bolt's frame call (lightning.h bolt_frame) (M4 s7 RING).
    void seg11_a9ea(int arg) override;
    // seg10:0x7f88 at seg7:0xb7a while DGROUP:0x103c != 0 (world/rescale.h size_reread, the
    // program's RescaleHost) (M4 s8 integrate).
    void size_reread() override;
    // seg13:0xb93a(), the warrant, every dword frame# % 300 = 0 (law.h warrant_check) [BYTES
    // seg7:0xd60-0xd78 seg13:0xb93a-0xba03] (M5 s4 integrate, LAW binding 2).
    void seg13_b93a() override;
};

CollideLinks g_collide_links;
PilotLinks g_pilot_links;  // M5 s3 integrate
World* rt_world();

// seg13:0xb6f2(ship, crime) on the program's World (law.h), with a line in the log [DESIGN: a
// diagnostic of the port] (M5 s4 integrate).
void crime_report_logged(int ship, int code) {
    World* w = rt_world();
    if (!w) return;
    crime_report_program(*w, ship, code);
    char buf[96];
    std::snprintf(buf, sizeof buf, "law: seg13:0xb6f2(%d, %d): DGROUP:0x9648 %d, 0xa163 %d", ship, code,
                  contact_words().w9648, contact_words().ba163);
    app::log(buf);
}

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
    // seg8:0x2cd8's picture part: the debris entry over the torn wing with ordinal 15's strip, or
    // the cut alone (debris.h wing_picture) (M4 s5 SHEET).
    bool wing_picture(int ship, int cut, bool make_debris) override;
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
    // seg13:0xb6f2(ship, crime), the crime report (law.h) at every crime site (collide.cpp,
    // shots.cpp, icon_effects.cpp, capture.cpp, ship_react.cpp) [BYTES seg13:0xb6f2-0xb939]
    // (M5 s4 integrate, LAW binding 1).
    void seg13_b6f2(int ship, int code) override { crime_report_logged(ship, code); }
    // ---- M5 s3 integrate: the contact routines' calls (CONTACT binding 1)
    std::int16_t seg13_adbe(int ship, int kind, int msg) override {  // seg13:0xadbe (callin.h)
        return rt_world() ? call_in_program(*rt_world(), ship, kind, msg) : std::int16_t{0};
    }
    void seg3_f4(int ship, int pool, int rec) override;  // seg3:0xf4
    // seg13:0x915c(ship): the ship's answer to its new target, the challenge (commands.h)
    // [BYTES seg13:0x915c] (M5 s4 integrate, COMMANDS binding B3).
    void seg13_915c(int ship) override { command_challenge(ship); }
    void music_hold(std::uint32_t t) override;  // DGROUP:0x2384 (FrameGlobals::music_due)
    std::int16_t info_mode() override;          // DGROUP:0xa76, the frame's copy
    void info_paint() override {                // PostMessage(DGROUP:0xa7c, WM_PAINT, 0, 0L)
        if (g_pilot_links.info_paint) g_pilot_links.info_paint();
    }
    // seg8:0x5fdd: the holder of a hard-hit held ship lets go, seg11:0x947a(holder, 2) (tether.h)
    // (M4 s7 TETHER).
    void seg11_947a(int pool, int rec, int arg) override { tether_use_program(pool, rec, arg); }
    // seg8:0x641d: a pool-10 attacker meets its target, seg12:0x3498 (tether.h hook_meets)
    // (M4 s7 TETHER).
    void seg12_3498(int tp, int tr, int xr) override { hook_meets_program(tp, tr, xr); }
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
    // seg7:0x5b68 from the pod's expiry (pods.h, collide.cpp) through the frame host (M4 s5 EFFECTS).
    void puff(int pool, int rec, int arg) override { u->puff(pool, rec, arg); }
    // seg7:0x2e02's source DC of a break-up, by pool (effects.h EffectLinks::break_source).
    BreakSource break_source(int pool, int rec);
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
    // Ctrl +/- (seg11:0x7207, seg11:0x7266): seg10:0x9a2a(d) on the program's World (world/rescale.h
    // rescale_step) through the program's RescaleHost (M4 s8 integrate).
    void rescale_step(int d) override;
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
    // The wing debris DGROUP:0x915c (debris.h), reset at the wave start [DESIGN] (wave_started).
    // The smoke table is the World's (World::smoke, smoke.h).
    DebrisTable debris{};
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
    // ---- M4 s7: the weapon system's other routines (MissileHost's named calls); particle (M6)
    // and relation (M5) stay unbound.
    // seg11:0x8a42, RING OF DEATH (the weapon system's case 0) [BYTES seg11:0xad5a-0xad66] (M4 s7 RING).
    l.ring_of_death = [](int pool, int rec) {
        Runtime& r = rt();
        if (r.world) ring_of_death(*r.world, effect_state().sides, r.g.now, pool, rec);
    };
    // seg11:0x947a, cases 1 and 14 with (DGROUP:0x9324, DGROUP:0x9646, 2 or 0xf) (tether.h)
    // (M4 s7 TETHER).
    l.tether_use = [](int pool, int rec, int w) { tether_use_program(pool, rec, w); };
    // seg11:0x9fdc, LIGHTNING (case 2) and BATTERIES' seg12:0x66e3 (6, m) [BYTES seg11:0xad82-0xad8e]
    // (M4 s7 RING).
    l.lightning = [](int pool, int rec) {
        Runtime& r = rt();
        if (r.world) lightning_search_on(*r.world, r.g.now, pool, rec);
    };
    // seg13:0x1318 (fuel_balls.h) (M4 s7 FUEL).
    l.fuel_ball = [](int pool, int rec, int life, int kind) {
        Runtime& r = rt();
        if (!r.world) return;
        SoundFuelHost fh([](int id, int p, int q, int m) { rt().h().sound(id, p, q, m); });
        FuelCtx fc{*r.world, r.g, r.h().collide_globals(), fh};
        launch_fuel_ball(fc, pool, rec, lo16(life), lo16(kind));
    };
    // seg12:0x2db6, FIRE DEMONS' fire (fuel_balls.h) (M4 s7 FUEL).
    l.pool5_shot = [](int pool, int rec, int heading, int life, int kind) {
        Runtime& r = rt();
        if (!r.world) return;
        SoundFuelHost fh([](int id, int p, int q, int m) { rt().h().sound(id, p, q, m); });
        FuelCtx fc{*r.world, r.g, r.h().collide_globals(), fh};
        spawn_fuel_ball(fc, pool, rec, lo16(heading), lo16(life), lo16(kind));
    };
    // seg11:0x9078, WILDCARD DEFENSE (wildcard.h); its run-time W(27) stores go to
    // missile_state().words, the copy the pool-6 mover reads (M4 s7 FUEL, LAUNCHED binding 3).
    l.wildcard = [](int pool, int rec) {
        Runtime& r = rt();
        if (!r.world) return;
        wildcard(*r.world, r.g, missile_state().words, shell::game_rng(), linked_wildcard_host(), pool, rec);
    };
}

// The tethers' calls (tether.h TetherLinks) on the program's World (M4 s7 TETHER).
void bind_tether_links() {
    TetherLinks& t = tether_links();
    t.world = [] { return rt_world(); };
    t.globals = [] { return &rt().g; };
    t.play_size = [] { return std::make_pair(rt().view.play_w, rt().view.play_h); };
    t.sound = [](int id, int pool, int rec, int mode) { rt().h().sound(id, pool, rec, mode); };  // seg10:0x754a
    t.command = [](int id) { rt().h().play(id); };                                                // seg10:0x744c
    t.to_screen = [](std::int16_t x, std::int16_t y) {                                            // seg8:0x29ea
        const ScreenPoint p = flight_to_screen(x, y);
        return Point16{p.x, p.y};
    };
    t.blitter = [] { return rt().h().blitter(); };
    t.player_sprite = [] { rt().h().player_sprite(); };  // seg7:0x1208
    t.weapon_word = [](int w, int off) { return rt().h().weapon_word(w, off); };
    t.visible = [] { return rt().h().visible_list(); };  // DGROUP:0x507c
}

UpdateCtx make_ctx(Runtime& r);

// ---- M5 s3 integrate: the computer pilots on the program's World (pilot_think.h, pilot_tick.h,
// pilot_steer.h), the bindings of work/world/m5s3/think-report.md, steer-report.md and
// contact-report.md

// f(FlightCtx&) on the frame's flight context while the frame runs (UpdateHost::frame_ctx), else
// on one over the Runtime's words; r.world must be set.
template <typename F>
auto with_flight(F f) {
    Runtime& r = rt();
    UpdateHost& h = r.h();
    if (h.frame_ctx) return f(h.frame_ctx->flight);
    UpdateCtx local = make_ctx(r);
    return f(local.flight);
}

std::string hex_word(int v) {
    char b[16];
    std::snprintf(b, sizeof b, "%x", v & 0xffff);
    return b;
}

class ProgramSteerHost;
SteerHost& program_steer_host();

// The pilot's calls out on the program (pilot_think.h PilotHost); the subjects of later stages
// (the law, the commands, the reactions, Demon's Gate, duels) keep PilotHost's logged-once
// defaults (ruling R3).
class ProgramPilotHost final : public PilotHost {
public:
    // DGROUP:0x96d2 + 0xad·n + off: the words of rating.h DesignRating (World::design_ratings).
    std::int16_t design_word(int n, int off) override {
        const World* w = rt().world;
        if (!w || n < 0 || n >= kShipCount) return 0;  // [DESIGN ADR 0016 decision 2]
        const DesignRating& d = w->design_ratings[static_cast<std::size_t>(n)];
        switch (off) {
        case 0x10: return d.armor4a;   // DGROUP:0x96e2
        case 0x1e: return d.tank;      // DGROUP:0x96f0
        case 0x28: return d.special;   // DGROUP:0x96fa
        case 0x32: return d.mass;      // DGROUP:0x9704
        case 0x38: return d.team;      // DGROUP:0x970a
        case 0x42: return d.captures;  // DGROUP:0x9714
        case 0x4a: return d.cloak;     // DGROUP:0x971c
        case 0x4c: return d.repair;    // DGROUP:0x971e
        default:
            todo_once("the pilot's design word +0x" + hex_word(off), "DGROUP:0x96d2");
            return 0;
        }
    }
    std::int16_t relation(int a, int b) override {  // seg13:0x2688 (relations.h)
        return rt().world ? ::is::world::relation(*rt().world, a, b) : std::int16_t{0};
    }
    std::int16_t strength(int a, int b) override {  // seg4:0x5d9c (relations.h)
        return rt().world ? strength_ratio(*rt().world, a, b) : std::int16_t{100};
    }
    std::int16_t team_word(int x, int y) override { return ::is::world::team_word(x, y); }  // DGROUP:0x3824
    void message(int recipient, int pool, int rec, int code) override {  // seg3:0x6ec
        rt().collide_host.message(recipient, pool, rec, code);
    }
    void target(int ship, int pool, int rec) override {  // seg3:0xf4
        if (g_pilot_links.set_target) return g_pilot_links.set_target(ship, pool, rec);
        if (World* w = rt().world; w && ship >= 0 && ship < kShipCount) {
            w->ship(ship).target_pool = static_cast<std::int16_t>(pool);  // +0x3e [BYTES seg3:0x10a]
            w->ship(ship).target_rec = static_cast<std::int16_t>(rec);    // +0x40 [BYTES seg3:0x111]
        }
    }
    std::int16_t visible(int pool, int rec) override {  // seg3:0x0
        return g_pilot_links.visible && g_pilot_links.visible(pool, rec) ? std::int16_t{1} : std::int16_t{0};
    }
    // seg13:0x9a18(i, pool, rec), the defenders sent after (pool, rec) with the homing store
    // DGROUP:0x52f6 (commands.h send_defenders) (M5 s4 integrate, COMMANDS binding B2).
    void send_defenders(int i, int pool, int rec) override { command_send_defenders(i, pool, rec); }
    // ---- M5 s4 integrate: the commands and the reactions (commands.h, reactions.h), COMMANDS
    // binding B2
    void reactions(int i) override { command_reactions(i); }          // seg13:0x7ba2
    void command_report() override { command_request_reply(); }       // seg13:0xa65a
    void seg13_9ae8(int i) override { command_defenders_return(i); }  // seg13:0x9ae8
    void seg13_9ce0(int i) override { command_capture(i); }           // seg13:0x9ce0
    // seg11:0x4c72 / seg11:0x4b00 / seg3:0x332e inside the frame (update.h UpdateHost).
    std::int16_t cloak_toggle(int i) override { return rt().h().seg11_4c72(i); }
    std::int16_t shield_toggle(int i) override { return rt().h().seg11_4b00(i); }
    void notice(std::uint32_t ids) override { rt().h().seg3_332e(ids); }
    void sound(int id, int pool, int rec, int mode) override { rt().h().sound(id, pool, rec, mode); }  // seg10:0x754a
    std::string lang(int id) override { return g_pilot_links.lang ? g_pilot_links.lang(id) : std::string{}; }
    std::string pilot_name() override { return g_pilot_links.pilot ? g_pilot_links.pilot() : std::string{}; }
    std::string shp_name(int n) override { return g_pilot_links.shp_name ? g_pilot_links.shp_name(n) : std::string{}; }
    std::int16_t weapon_system(int i) override {  // seg4:0x5654 (pilot_steer.h)
        World* w = rt().world;
        if (!w) return 0;
        return with_flight([&](FlightCtx& f) { return weapon_decision_frame(*w, f, program_steer_host(), i); });
    }
    void weapon_use(int k, int s, int life) override {  // seg4:0x584c (pilot_steer.h)
        World* w = rt().world;
        if (!w) return;
        with_flight([&](FlightCtx& f) { use_weapon_frame(*w, f, program_steer_host(), k, s, life); });
    }
    void thrust(int i, int mode) override {  // seg11:0x5ab6(mode, i) (flight.h)
        if (!rt().world) return;
        with_flight([&](FlightCtx& f) { thrust_step(f, static_cast<std::int16_t>(mode), i); });
    }
    void gun(int i) override { rt().h().fire(i); }  // seg11:0x4e0a (shots.h through UpdateHost)
    std::int16_t call_in(int s, int kind, int msg) override {  // seg13:0xadbe (callin.h)
        return rt().world ? call_in_program(*rt().world, s, kind, msg) : std::int16_t{0};
    }
    void through_gate(int pool, int rec) override { run_gate_exit(pool, rec, rt().g.now); }  // seg12:0x2288
    std::int16_t ambulance_call(int i) override {  // seg16:0xb4e (ambulance.h)
        Runtime& r = rt();
        if (!r.world) return 0;
        const std::int16_t v = ambulance_call_program(*r.world, r.g.now, i);
        // The frame's copies of DGROUP:0x9e49 / 0x952e / 0x9e5b, as ambulance_frame keeps them [DESIGN].
        r.g.dg_9e49 = r.world->w9e49;
        r.g.dg_952e = r.world->w952e;
        r.g.dg_9e5b = ambulance_state().served_alive;
        return v;
    }
    void touched(int i) override {  // seg13:0x6716 (flight.h)
        if (!rt().world) return;
        with_flight([&](FlightCtx& f) { ship_touched(f, i); });
    }
    void weapon_display() override {  // seg11:0x2f8c
        if (g_pilot_links.weapon_display) g_pilot_links.weapon_display();
    }
    void set_manual_target() override {  // DGROUP:0xa62 = 1 [BYTES seg4:0x62b]
        if (g_pilot_links.manual_target) g_pilot_links.manual_target();
    }
    // DGROUP:0x8560 + 0x34·rec + off (effects.h PodRecord, the original's 0x34 bytes).
    std::int16_t pod_word(int rec, int off) override {
        if (rec < 0 || rec >= kPodRecords || off < 0 || off + 2 > 0x34) return 0;  // [DESIGN ADR 0016 decision 2]
        const PodRecord& p = effect_state().pods[static_cast<std::size_t>(rec)];
        std::int16_t v = 0;
        std::memcpy(&v, reinterpret_cast<const std::uint8_t*>(&p) + off, sizeof v);
        return v;
    }
    void clear_tallies() override {  // DGROUP:0x9680, 0x9682, 0x8e00, 0x8e04 [BYTES seg4:0x41f9-0x4213]
        ContactWords& c = contact_words();
        c.w9680 = c.w9682 = 0;
        c.d8e00 = c.d8e04 = 0;
    }
    std::int16_t weapon_word(int w, int off) override { return missile_state().words.word(w, off); }  // DGROUP:0x2f54
    // DGROUP:0x52fe + 0x24·k + off (collide.h CollideGlobals::defenders).
    std::int16_t defender_word(int k, int off) override {
        const auto& d = rt().h().collide_globals().defenders;
        if (k < 0 || k >= static_cast<int>(d.size())) return 0;  // [DESIGN ADR 0016 decision 2]
        const DefenderEntry& e = d[static_cast<std::size_t>(k)];
        switch (off) {
        case 0x00: return e.w00;
        case 0x06: return e.owner_pool;
        case 0x08: return e.owner_rec;
        case 0x0a: return e.killer_pool;
        case 0x0c: return e.killer_rec;
        default: todo_once("the defender word +0x" + hex_word(off), "DGROUP:0x52fe"); return 0;
        }
    }
    // DGROUP:0x9146 and the bytes DGROUP:0x507c / 0x50e0 of the last visibility pass.
    int visible_count() override {
        const VisibilityGlobals* v = last_visibility();
        return v ? v->count : 0;
    }
    void visible_entry(int k, std::int8_t& pool, std::int8_t& rec) override {
        const VisibilityGlobals* v = last_visibility();
        if (!v || k < 0 || k >= kVisibleMax) {
            pool = -1;
            rec = 0;
            return;
        }
        pool = static_cast<std::int8_t>(v->pool[static_cast<std::size_t>(k)]);
        rec = static_cast<std::int8_t>(v->rec[static_cast<std::size_t>(k)]);
    }
    void mark_6e52(std::int16_t& pool, std::int16_t& rec) override {  // DGROUP:0x6e54 / 0x6e52
        pool = -1;
        rec = -1;
        if (g_pilot_links.mark0) g_pilot_links.mark0(pool, rec);
    }
    void watch_ship(int i) override {  // seg13:0x6890(i) [BYTES seg13:0x6890-0x68ff]
        if (g_pilot_links.watch_ship) g_pilot_links.watch_ship(i);
    }
    // seg13:0x50b0(0, 0), seg13:0x58dc(0, 0), seg7:0x64ac(0, 0, -1) [BYTES seg13:0x67f2-0x6821]:
    // seg13:0x50b0 is the watch window's painter (hud/watch.h watch_tail, PilotLinks::watch_tail),
    // and the HUD's pictures_tick (WaveLinks::pictures) is the other two (M5 s4 integrate, WATCH
    // binding 6).
    void watch_tail() override {
        if (g_pilot_links.watch_tail) g_pilot_links.watch_tail();
        if (g_links.pictures) g_links.pictures();
    }
    // DGROUP:0x95fe != 0, the IS_SMART window (hud/watch.h smart_exists).
    bool smart_window() override { return g_pilot_links.smart_window && g_pilot_links.smart_window(); }
    // DGROUP:0x8f74 (INI SmWnd) and DGROUP:0x131c (INI ShipAnim, default 1) [BYTES seg6:0x13e
    // seg6:0xd0a].
    std::int16_t smwnd() override { return static_cast<std::int16_t>(g.keys.sm_wnd); }
    std::int16_t ship_anim() override {
        return static_cast<std::int16_t>(g.ini ? g.ini->get_int(settings::kGameSection, "ShipAnim", 1) : 1);
    }
    // DGROUP:0x95fa = n and PostMessage(main, 0x9b4) [BYTES seg13:0x6870-0x6881] (hud/watch.h
    // all_ships_notice).
    void all_ships_notice(int n) override {
        if (g_pilot_links.all_ships_notice) return g_pilot_links.all_ships_notice(n);
        todo_once("the All Ships window 0x9b4 (IS_SMART)", "seg13:0x6881");
    }
};
ProgramPilotHost g_pilot_host;

// The pilot's context on the program's World (pilot_think.h PilotCtx): the World's pools and
// ships, the flight globals (the frame's own, make_ctx), the wave's PilotGlobals, rand() and the
// host. r.world must be set.
PilotCtx program_pilot_ctx(Runtime& r) {
    World& w = *r.world;
    return PilotCtx{w.pools, w.ships, r.g, w, w.pilot, shell::game_rng(), g_pilot_host};
}

// The steer's calls into the think (pilot_steer.h SteerHost, STEER binding B1); the touch, the
// view and the launchers keep SteerHost's MissileLinks defaults.
class ProgramSteerHost final : public SteerHost {
public:
    void think(int ship) override {  // seg4:0x78
        Runtime& r = rt();
        if (!r.world) return;
        PilotCtx c = program_pilot_ctx(r);
        ::is::world::think(c, ship);
    }
    void hazard_step(int ship) override {  // seg4:0x4fc8
        Runtime& r = rt();
        if (!r.world) return;
        PilotCtx c = program_pilot_ctx(r);
        ::is::world::hazard_step(c, ship);
    }
    void weapon_choice(int ship) override {  // seg4:0x6e70
        Runtime& r = rt();
        if (!r.world) return;
        PilotCtx c = program_pilot_ctx(r);
        choose_weapon(c, ship);
    }
};
SteerHost& program_steer_host() {
    static ProgramSteerHost h;
    return h;
}

// The player's 1500 ms tick calls (player_tick.h PlayerTickLinks), the attitude change's calls
// out (relations.h AttitudeOut) and the MISSILE kind-5 aim's relation (missile.h MissileLinks)
// on the program's World (THINK binding 3, CONTACT bindings 6-8).
void bind_pilot_links() {
    PlayerTickLinks& t = player_tick_links();
    t.seg4_59bc = []() -> std::int16_t {  // seg4:0x59bc
        Runtime& r = rt();
        if (!r.world) return 0;
        PilotCtx c = program_pilot_ctx(r);
        return player_estimate(c);
    };
    t.seg4_6700 = [](int ship) {  // seg4:0x6700
        Runtime& r = rt();
        if (!r.world) return;
        PilotCtx c = program_pilot_ctx(r);
        rivals(c, ship);
    };
    t.seg13_3302 = [] {  // seg13:0x3302
        Runtime& r = rt();
        if (!r.world) return;
        PilotCtx c = program_pilot_ctx(r);
        claim_reset(c);
    };
    t.seg4_7e2a = [] {  // seg4:0x7e2a
        Runtime& r = rt();
        if (!r.world) return;
        PilotCtx c = program_pilot_ctx(r);
        // DGROUP:0x9648's one home is relations.h ContactWords::w9648 (the attack reaction adds
        // to it); the pilot's copy follows it before the read [DESIGN] (THINK binding 5).
        c.pg.crimes_wave = contact_words().w9648;
        callins(c);
    };
    t.seg4_83ae = [] {  // seg4:0x83ae
        Runtime& r = rt();
        if (!r.world) return;
        PilotCtx c = program_pilot_ctx(r);
        cloak_guess(c);
    };
    t.seg13_7b40 = [](int k) {  // seg13:0x7b40 (pilot_orders.h) [BYTES seg7:0xfae-0xfb4]
        Runtime& r = rt();
        if (!r.world) return;
        PilotCtx c = program_pilot_ctx(r);
        bidding_report(c, k);
    };
    t.seg13_2714 = [](int a, int b, int dt, int ds, int prop, int kind) {  // seg13:0x2714
        if (rt().world) attitude_change(*rt().world, a, b, dt, ds, prop, kind);
    };
    AttitudeOut& ao = attitude_links();
    ao.message = [](int recipient, int pool, int rec, int code) {  // seg3:0x6ec
        Runtime& r = rt();
        r.collide_host.u = &r.h();
        r.collide_host.message(recipient, pool, rec, code);
    };
    ao.info_mode = [] { return rt().f.info_mode; };  // DGROUP:0xa76
    ao.info_paint = [] {
        if (g_pilot_links.info_paint) g_pilot_links.info_paint();
    };
    missile_links().relation = [](int s, int rec) -> std::int16_t {  // seg13:0x2688
        return rt().world ? ::is::world::relation(*rt().world, s, rec) : std::int16_t{0};
    };
}

// seg13:0xadbe's program side (callin.h CallInLinks, ENTRY binding 1): the sounds, the music, the
// messages, the team words, DGROUP:0x3824, 0x2384, 0x1e50, 0x1e68, 0xa163 and the pilots' one
// DGROUP:0x964c / 0x8e2c (pilot.h PilotGlobals). The roster build is the drawing side's
// (src/hud/wave_win32.cpp).
void bind_callin_links() {
    CallInLinks& cl = callin_links();
    cl.play = [](int id) { rt().h().play(id); };                                                  // seg10:0x744c
    cl.sound = [](int id, int pool, int rec, int mode) { rt().h().sound(id, pool, rec, mode); };  // seg10:0x754a
    cl.music = [](int track, int flag) { is::sound::music(track, flag); };                        // seg10:0x7480
    cl.message = [](int recipient, int pool, int rec, int code) {                                 // seg3:0x6ec
        Runtime& r = rt();
        r.collide_host.u = &r.h();
        r.collide_host.message(recipient, pool, rec, code);
    };
    cl.team = [](int design) { return rt().world ? design_team(*rt().world, design) : std::int16_t{0}; };
    cl.team_table = [] { return roster_words().team_table.data(); };  // DGROUP:0x3824
    cl.dg_a163 = [] { return std::int16_t{contact_words().ba163}; };  // DGROUP:0xa163
    cl.music_due = [] { return &rt().f.music_due; };                  // DGROUP:0x2384
    cl.now = [] { return rt().g.now; };                               // DGROUP:0x1e50
    cl.frames = [] { return rt().g.game_frames; };                    // DGROUP:0x1e68
    cl.called_in = [] { return &current().pilot.called_in; };         // DGROUP:0x964c (World::pilot)
    cl.callin_frame = [] { return &current().pilot.callin_frame; };   // DGROUP:0x8e2c
    cl.todo = [](const std::string& what, const char* address) { todo_once(what, address); };
    // seg4:0x8346(k, 14, ship, 0xd) for a Pirates' call-in (pilot_orders.h ship_reaction), the
    // cheat key '4''s path [BYTES seg13:0xb2de-0xb2f3] (M5 s4 integrate).
    cl.seg4_8346 = [](int s, int pool, int rec, int kind) {
        Runtime& r = rt();
        if (!r.world) return;
        PilotCtx c = program_pilot_ctx(r);
        (void)ship_reaction(c, s, pool, rec, kind);
    };
}

// ---- M5 s4 integrate: the law, the cheat keys and the commands on the program's World
// (work/world/m5s4/law-report.md, keys-report.md, commands-report.md)

void DefaultHost::seg13_b93a() {
    if (World* w = rt_world()) warrant_check_program(*w);  // seg13:0xb93a() [BYTES seg7:0xd78]
}

// The commands' calls PilotHost does not carry (commands.h CommandHost, COMMANDS binding B1):
// the defender table DGROUP:0x52fe (collide.h CollideGlobals::defenders) and seg13:0x498e, the
// resource pack (death.h drop_pod on the program's EffectCtx, as effects.cpp's seg16:0xb3c).
// capture_count and attitude_change keep CommandHost's defaults (missile_links().capture_count,
// relations.h attitude_change); seg3:0x33b4 stays logged (M8).
template <typename F>
void with_effects(F f);
class ProgramCommandHost final : public CommandHost {
public:
    std::vector<DefenderEntry>* defenders() override { return &rt().h().collide_globals().defenders; }
    void drop_pod(int s, int flag) override {
        with_effects([&](EffectCtx& c) { ::is::world::drop_pod(c, s, flag); });  // seg13:0x498e(s, flag)
    }
};
ProgramCommandHost g_command_host;

// A command key's routine (input.h Command) in the bytes' order of seg11:0x6b64's letters: the
// decode (input.cpp key_message) has picked the routine by Ctrl and Alt, so each runs alone
// [BYTES seg11:0x6f20-0x7130 seg11:0x6f3a-0x6f55] (KEYS binding 2, COMMANDS binding B6).
void run_command(Command k, int P) {
    command_links().with_ctx([&](CommandCtx& c) {
        World& w = c.p.w;
        switch (k) {
        case Command::kCtrlA: attack_command(c, P); break;  // seg13:0x8570(P)
        case Command::kAltA:  // seg13:0x9a18(P, P's +0x3e, +0x40), read after Ctrl's call [BYTES seg11:0x707b]
            if (P >= 0 && P < kShipCount)
                send_defenders(c, P, w.ship(P).target_pool, w.ship(P).target_rec);
            break;
        case Command::kStrategyA: strategy_attack(c); break;   // seg13:0xa186
        case Command::kCtrlC: capture_command(c, P); break;    // seg13:0x9ce0(P)
        case Command::kStrategyC: strategy_capture(c); break;  // seg13:0xa234
        case Command::kCtrlD: defend_command(c, P); break;     // seg13:0x8e98(P)
        case Command::kStrategyD: strategy_help(c); break;     // seg13:0xa4f4
        case Command::kCtrlE: exit_command(c, P); break;       // seg13:0x9f3e(P)
        case Command::kAltE: defenders_leave(c); break;        // seg11:0x70bb-0x7113
        case Command::kStrategyE: strategy_exit(c); break;     // seg13:0xa3c8
        case Command::kCtrlF: challenge_command(c, P); break;  // seg13:0x915c(P)
        case Command::kCtrlG: gas_command(c, P); break;        // seg13:0xa01c(P)
        case Command::kStrategyG: strategy_gas(c); break;      // seg13:0xa47c
        case Command::kCtrlH: help_command(c, P); break;       // seg13:0x81c2(P)
        case Command::kAltH: defenders_return(c, P); break;    // seg13:0x9ae8(P)
        case Command::kCtrlS: preserve_command(c, P); break;   // seg13:0x9810(P)
        case Command::kCtrlAltS: arrest_command(c, P); break;  // seg13:0x4ac4(P)
        case Command::kStrategyS: strategy_needs(c); break;    // seg13:0xa568
        case Command::kSelfDestruct: {  // seg16:0x8e4(DGROUP:0x9324, P) [BYTES seg11:0x6f48-0x6f50]
            Runtime& r = rt();
            r.collide_host.u = &r.h();
            r.collide_host.ship_destroyed(r.g.ship_pool, P);
            break;
        }
        }
    });
}

void bind_law_links() {
    // The commands' context: the pilot's (program_pilot_ctx), the host above and the words
    // (COMMANDS binding B1).
    command_links().with_ctx = [](const std::function<void(CommandCtx&)>& f) {
        Runtime& r = rt();
        if (!r.world) return;
        PilotCtx pc = program_pilot_ctx(r);
        CommandCtx c{pc, g_command_host, command_words()};
        f(c);
    };
    // The key switch's slots (input.h KeyLinks; KEYS binding 2). The watch key W is the HUD's
    // (src/hud/hooks.cpp). DGROUP:0x104c is the one CollideGlobals::w104c damage.cpp reads, and
    // DGROUP:0x1048 the frame's FrameGlobals::dg_1048.
    KeyLinks& kl = key_links();
    kl.cheat_gate = [] {  // seg11:0x6ee7
        Runtime& r = rt();
        if (!r.world) return;
        const bool let = cheat_gate_program(*r.world, r.h().collide_globals().w104c, r.f.dg_1048);
        app::log("cheat gate seg11:0x6ee7: edition " + std::to_string(r.world->w920c) + (let ? ", " : " (refused), ") +
                 "DGROUP:0x951c = " + std::to_string(cheat_state().w951c));
    };
    kl.cheat_key = [](std::uint16_t vk) {  // seg11:0x72e0(vk)
        Runtime& r = rt();
        if (!r.world) return;
        char buf[64];
        std::snprintf(buf, sizeof buf, "cheat key seg11:0x72e0(0x%x)", vk);
        app::log(buf);
        cheat_key_program(*r.world, r.h().collide_globals().w104c, r.f.dg_1048, vk);
    };
    kl.command = [](Command k, int player) { run_command(k, player); };
    // The cheat keys' calls (cheats.h CheatLinks): play, sound and message fall back to
    // callin_links(); F6's crime is seg13:0xb6f2 (LAW binding 6); '1''s race finish stays logged
    // (ruling R3); the score redraw is the HUD's (src/hud/hooks.cpp).
    cheat_links().crime = [](int s, int code) { crime_report_logged(s, code); };
    // Ctrl+H and Alt+H through the ambulance's decode (ambulance.h AmbulanceHost::ctrl_h, alt_h)
    // are bound in ambulance.cpp (COMMANDS binding B4).
}

void DefaultHost::seg13_675e() {
    Runtime& r = rt();
    if (!r.world) {
        if (g_links.pictures) g_links.pictures();
        return;
    }
    PilotCtx c = program_pilot_ctx(r);
    watch_refresh(c);  // seg13:0x675e [BYTES seg13:0x675e-0x688d]
}

void DefaultHost::seg4_bde(int ship) {
    Runtime& r = rt();
    if (!r.world) return;
    with_flight([&](FlightCtx& f) { steer_frame(*r.world, f, program_steer_host(), ship); });  // seg4:0xbde(i)
}

void DefaultHost::seg4_78(int ship) {
    Runtime& r = rt();
    if (!r.world) return;
    PilotCtx c = program_pilot_ctx(r);
    think(c, ship);  // seg4:0x78(i)
}

void DefaultHost::seg4_6e70(int ship) {
    Runtime& r = rt();
    if (!r.world) return;
    PilotCtx c = program_pilot_ctx(r);
    choose_weapon(c, ship);  // seg4:0x6e70(i)
}

std::int16_t DefaultHost::design_kind(int design) {
    const World* w = rt().world;
    return w ? design_team(*w, design) : std::int16_t{0};  // DGROUP:0x970a + 0xad·n
}

std::int16_t DefaultHost::special_places() {
    const World* w = rt().world;
    return w ? w->w9344 : std::int16_t{0};  // DGROUP:0x9344
}

RingPlace DefaultHost::special_place(int k) {
    const World* w = rt().world;
    if (!w || k < 0 || k >= static_cast<int>(w->places.size())) return RingPlace{};  // [DESIGN ADR 0016 decision 2]
    const SpecialPlace& p = w->places[static_cast<std::size_t>(k)];
    return RingPlace{p.radius, p.x, p.y};  // DGROUP:0x4f30, 0x4f36 + 0x10·k
}

void ProgramCollideHost::seg3_f4(int ship, int pool, int rec) { g_pilot_host.target(ship, pool, rec); }
void ProgramCollideHost::music_hold(std::uint32_t t) { rt().f.music_due = t; }
std::int16_t ProgramCollideHost::info_mode() { return rt().f.info_mode; }

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
    // seg11:0xaece at the demo player's death (shift.h perspective_shift) [BYTES seg16:0xb2b]
    // (M5 s3 integrate, ENTRY binding 6).
    void seg11_aece() override {
        Runtime& r = rt();
        if (r.world) perspective_shift(*r.world, r.view, linked_shift_host(), &r.g);
    }
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
// the wave Blitter, DGROUP:0x1e50, DGROUP:0x951a and DGROUP:0x1318 (the World's word, the
// blast gate seg8:0x5aec reads it) [BYTES DGROUP:0x1318 seg8:0x5aec] (M4 s7 RING).
template <typename F>
void with_effects(F f) {
    Runtime& r = rt();
    if (!r.world) return;
    r.collide_host.u = &r.h();
    const EffectWords in{r.g.now, r.g.autopilot, r.world->w1318};
    EffectCtx c{*r.world,         effect_state(),
                in,               g_effect_host,
                shell::game_rng(), wave_blitter(),
                g_links.images ? g_links.images() : nullptr,
                effect_to_screen, &r.f.dg_943a,
                &r.f.dg_943c};
    c.dg_b0c = &r.h().collide_globals().w0b0c;  // DGROUP:0xb0c, one word (M4 s4 fixer, C006)
    c.shots = &r.h().shots();                    // DGROUP:0x6098 for seg8:0x9694 (M4 s5 EFFECTS)
    f(c);
}

// The smoke's context on the program's World (smoke.h; M4 s5 EFFECTS): DGROUP:0x58bc, 0x9456,
// the pools, the ship records and the muzzle tables, rand(), Detail, Size, DGROUP:0x9324,
// 0x9646, 0x9130, the window Blitter, slot 12, seg8:0x29ea, the visibility list and the frame's
// DGROUP:0x58a.
SmokeCtx program_smoke_ctx(Runtime& r) {
    World& w = *r.world;
    SmokeCtx c{w.smoke, w.w9456, w.pools, shell::game_rng()};
    c.ships = &w.ships;
    c.shots = &r.h().shots();
    c.detail = r.g.detail;
    c.size = r.g.size;
    c.ship_pool = r.g.ship_pool;
    c.player = r.g.player;
    c.dg_9130 = r.g.dg_9130;
    c.out = wave_blitter();
    c.images = g_links.images ? g_links.images() : nullptr;
    c.to_screen = effect_to_screen;
    c.visible = last_visibility();
    c.dg_58a = &r.f.dg_58a;
    c.sound = [](int id, int pool, int rec, int mode) { rt().h().sound(id, pool, rec, mode); };
    c.unported = [](const char* address) { todo_once("smoke in the seg18 mode", address); };
    return c;
}

void DefaultHost::puff(int pool, int rec, int arg) {
    Runtime& r = rt();
    if (!r.world) return;
    SmokeCtx c = program_smoke_ctx(r);
    smoke_puff(c, pool, rec, arg);
}

void DefaultHost::seg7_2128() {
    Runtime& r = rt();
    if (r.world) decay_violence(*r.world, r.g.now);  // [BYTES seg7:0x91d-0x922]
}

void DefaultHost::player_sprite() {
    if (g_links.player_sprite) {
        g_links.player_sprite();
    } else {
        todo_once("the player's sprite", "seg7:0x1208");
    }
}

// seg7:0x303f-0x306e and seg7:0x2e5c-0x2ec2 [BYTES seg7:0x303f-0x306e seg7:0x2e5c-0x2ec2]
// (s5-effects.md 1.7, verified claim 30).
BreakSource ProgramCollideHost::break_source(int pool, int rec) {
    World* w = rt_world();
    if (!w) return {};
    const ImageSlotTable* im = g_links.images ? g_links.images() : nullptr;
    auto slot = [im](int s) -> DcId {
        return im && s >= 0 && s < kImageSlots ? (*im)[static_cast<std::size_t>(s)] : kDcNone;
    };
    switch (pool) {
    case 1: {
        // CreateCompatibleDC(DGROUP:0x13dc) and SelectObject of the fragment's bitmap
        // DGROUP:0x4e94 + 2·rec [BYTES seg7:0x3046-0x306a]: the port binds the fragment's image to a
        // DC id of its own, which keeps that picture while a new shatter remakes the record's
        // (seg7:0x2768), and which the end deletes as the original deletes its DC [DESIGN ADR 0013].
        const DcId f = effect_links().fragment_dc ? effect_links().fragment_dc(rec) : kDcNone;
        if (f == kDcNone) {
            todo_once("the fragment's DC for the break-up", "seg7:0x3046");
            return {};
        }
        if (auto* gb = dynamic_cast<GdiBlitter*>(wave_blitter())) {
            if (std::shared_ptr<gfx::Surface> img = gb->image(f)) return BreakSource{gb->bind(img), true};
        }
        return BreakSource{f, false};
    }
    case 8:  // DGROUP:0x39e8[word DGROUP:0x3d6e + 0x34·rec] [BYTES seg7:0x2e5c-0x2e79]
        return rec >= 0 && rec < kPool8Max ? BreakSource{slot(w->pool8[static_cast<std::size_t>(rec)].at(0x1e)), false}
                                           : BreakSource{};
    case 6: {  // DGROUP:0x39e8[word DGROUP:0x2f50 + 0x50·w], w = DGROUP:0x66e4 + 0x1c·rec [BYTES seg7:0x2e7c-0x2e9e]
        if (rec < 0 || rec >= w->pools.records(6)) return {};
        const std::int16_t wpn = w->pools.missile(rec).weapon;
        return BreakSource{slot(missile_state().words.word(wpn, -4)), false};
    }
    case 4:  // DGROUP:0x39f0, slot 4 (the tank) [BYTES seg7:0x2ea8-0x2ec2]
        return BreakSource{slot(kSlotTank), false};
    default:  // no DC (seg7:0x2ea8 `jne seg7:0x2eca`) (A4)
        return {};
    }
}

// The break-up's links (effects.h EffectLinks); EffectLinks::fragment_dc is the HUD's
// (src/hud/wave_win32.cpp, g_fragment_dc).
void bind_effect_links() {
    EffectLinks& l = effect_links();
    l.break_source = [](int pool, int rec) {
        Runtime& r = rt();
        r.collide_host.u = &r.h();
        return r.collide_host.break_source(pool, rec);
    };
    l.release = [](DcId dc) {  // SelectObject back + DeleteDC [BYTES seg7:0x301f-0x3039 seg8:0x45b2-0x45c3]
        if (Blitter* b = wave_blitter()) b->delete_image(dc);
    };
}

// The LIGHTNING routines' calls out (lightning.h LightningLinks) on the program's World
// (M4 s7 RING).
void bind_lightning_links() {
    LightningLinks& l = lightning_links();
    l.sound = [](int id, int a, int b, int mode) { rt().h().sound(id, a, b, mode); };  // seg10:0x754a
    l.dispatch = [](int tp, int tr, int xp, int xr, std::int16_t a) -> std::int16_t {  // seg8:0x5df2
        Runtime& r = rt();
        if (!r.world) return 0;
        r.collide_host.u = &r.h();
        CollideCtx x = program_collide_ctx(r);
        return dispatch(x, tp, tr, xp, xr, a);
    };
    l.icon_overlay = [](int icon, int k) {  // seg7:0x2512 (icon_effects.h)
        Runtime& r = rt();
        if (!r.world) return;
        r.collide_host.u = &r.h();
        CollideCtx x = program_collide_ctx(r);
        icon_overlay(x, icon, k);
    };
    l.to_screen = effect_to_screen;  // seg8:0x29ea
    l.team = [](int design) -> std::int16_t {  // DGROUP:0x970a + 0xad·n, as the dispatcher reads it
        Runtime& r = rt();
        return r.world ? r.h().collide_host().design(*r.world, design).team : std::int16_t{0};
    };
    l.dg_943a = &rt().f.dg_943a;  // the words program_collide_ctx hands the dispatcher
    l.dg_943c = &rt().f.dg_943c;
    // USER.15 GetCurrentTime at the strike, the program's clock app::now_tick, which seeds the
    // bolt's srand [BYTES seg11:0xa4b5-0xa4bb] (lightning.h current_time; review L5).
    l.current_time = [] { return static_cast<std::uint32_t>(app::now_tick()); };
}

void ProgramEffectHost::destroy(int pool, int rec) {
    with_effects([&](EffectCtx& c) { call_on_destroy(c, pool, rec); });
}

void DefaultHost::seg11_a9ea(int arg) {
    Runtime& r = rt();
    if (r.world) bolt_frame_on(*r.world, r.g.now, arg);
}

void DefaultHost::effect(int k) {
    with_effects([k](EffectCtx& c) { effect_frame(c, k); });
}

void DefaultHost::seg13_447a() {
    Runtime& r = rt();
    if (!r.world) return;
    World& w = *r.world;
    // W, H: DGROUP:0x90ea, 0x90ec
    // The debris's puffs seg7:0x5d6a go to the World's smoke table DGROUP:0x58bc (smoke.h
    // smoke_start) (M4 s5 EFFECTS).
    DebrisCtx c{r.debris,       w.smoke,         w.w9456,           w.pools,
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

bool ProgramCollideHost::wing_picture(int ship, int cut, bool make_debris) {
    Runtime& r = rt();
    if (!r.world) return false;
    return ::is::world::wing_picture(*r.world, r.debris, wave_blitter(), g_links.images ? g_links.images() : nullptr,
                                     ship, cut, make_debris);
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
    // DGROUP:0x951a: the World's word is the one copy; seg11:0xaece and the pan write both
    // (shift.h, view.h) [DESIGN] (M5 s3 integrate, ENTRY binding 6).
    gl.autopilot = w.w951a;
    gl.game_frames = w.frames;
    gl.burst_count = w.w9510;
    r.f.race = w.handover.race;
    r.f.dg_950 = w.handover.duel;
    r.f.dg_9600 = w.w9600;
    r.f.entry_pending = w.w9140;
    r.f.info_mode = w.w0a76;
    // DGROUP:0x95fc, the watch window's handle, which the HUD's window code holds (M5 s4 integrate).
    r.f.dg_95fc = g_links.watch_window && g_links.watch_window() ? 1 : 0;
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
// ---- the Size change (world/rescale.h) on the program's World (M4 s8 integrate)

RescaleHost* g_rescale_host = nullptr;

// DGROUP:0x9156 and DGROUP:0x9150 are one word each in the original, so the rest of a frame
// that rescaled reads the new Size: the flight role's copies follow at once [DESIGN].
void size_sync(Runtime& r) {
    if (!r.world) return;
    r.g.size = r.world->size;
    r.view.shift = r.world->shift;
}

void DefaultActions::rescale_step(int d) {
    Runtime& r = rt();
    if (!r.world || !g_rescale_host) {
        todo_once("rescale " + std::to_string(d) + " without the drawing side", "seg10:0x9a2a");
        return;
    }
    world::rescale_step(*r.world, d, *g_rescale_host);  // seg10:0x9a2a(d) [BYTES seg11:0x7207 seg11:0x7266]
    size_sync(r);
}

void DefaultHost::size_reread() {
    Runtime& r = rt();
    if (!r.world || !g_rescale_host) {
        todo_once("the INI Size reread without the drawing side", "seg10:0x7f88");
        return;
    }
    world::size_reread(*r.world, *g_rescale_host);  // seg10:0x7f88 [BYTES seg7:0xb7a]
    size_sync(r);
}

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
    Runtime& r = rt();
    // seg12:0x3e98 returns at once outside a kind-6 wave [BYTES seg12:0x3ea8-0x3eaf]: only a race
    // wave misses its body (M4 s3 fixer, review L2).
    if (m == Mover::k12_3e98 && r.g.wave_kind != 6) return;
    // seg7:0x5e04, the smoke (smoke.h move_smoke); update.cpp calls it while DGROUP:0x9456 > 0
    // [BYTES seg7:0xd0a-0xd11] (M4 s5 EFFECTS).
    if (m == Mover::k7_5e04) {
        if (!r.world) return;
        SmokeCtx c = program_smoke_ctx(r);
        return move_smoke(c);
    }
    // seg8:0x8256: the head (DGROUP:0x9452, 0x186c, 0x1874; commands.h defender_mover_head)
    // [BYTES seg8:0x8277-0x82c0], then the colour cycle seg8:0x82c6-0x82f4 (the sparks'
    // DGROUP:0xb6a, effects.h effect_colour_cycle) before its first test of pool 2 (M4 s5
    // EFFECTS), then a defender in state 15 steers through commands.h defender_attack_step in
    // place of seg8:0x8f5b-0x9000 (COMMANDS binding B9). The entry loop's other states (0, 5, 10)
    // and its tail seg8:0x8484 are the defenders' own movers, out of M5 stage 4 (ruling R3): no
    // defender is launched while seg8:0x9464 is a host call, so the loop finds none.
    if (m == Mover::k8_8256) {
        bool steer = false;
        command_links().with_ctx([&](CommandCtx& c) { steer = defender_mover_head(c); });
        effect_colour_cycle(effect_state(), r.g.now);
        if (r.world) {
            const auto& d = r.h().collide_globals().defenders;
            for (int k = 0; k < r.world->pools.in_use(2) && k < static_cast<int>(d.size()); ++k) {
                // The loop head runs for every entry: the mark tests on DGROUP:0x1870 and the
                // heading from the velocity [BYTES seg8:0x8ecb-0x8f45] (the defenders' own mover,
                // ruling R3); then an entry whose pool-2 object is not in play goes to the next k
                // before the state switch (review L2) [BYTES seg8:0x8f4a-0x8f57].
                todo_once("the defenders' mover loop head (DGROUP:0x1870, the heading)", "seg8:0x8ecb");
                if (r.world->pools.records(2) > k && r.world->pools.rec(2, k).state != 0) continue;
                if (d[static_cast<std::size_t>(k)].w00 != 15) {
                    todo_once("the defenders' mover, states other than 15 and the tail", "seg8:0x8484");
                    continue;
                }
                command_links().with_ctx([&](CommandCtx& c) { defender_attack_step(c, k, steer); });
            }
        }
        return;
    }
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
    // The WM_KEYDOWN, 0x99b and 0x99e keys, and WM_KEYUP for the cheat keys '1' ... '9' and F3
    // ... F9 (seg11:0x6d19-0x6d69): key_message returns kNotMine for every other key-up, so ESC,
    // Enter, Z, CAPS and P go on to the previous route_input (M5 s4 integrate, KEYS binding 1).
    if (in.msg != kWmKeyDown && in.msg != kWmKeyUp && in.msg != kMsgAltKey && in.msg != kMsgCtrlKey) return false;
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
    // seg10:0x7896's player point, the Size wrap's centre and Size, and the activity rectangles
    // (audio/wave_view.h; M4 s8 sound track) [BYTES seg10:0x78d0-0x78e0 seg10:0x77e0-0x781d
    // seg10:0x7914-0x792b].
    if (w)
        audio::set_wave_view([](audio::WaveView& v) {
            Runtime& rr = rt();
            const World* ww = rr.world;
            if (!ww) return false;
            const int ships = ww->roles.ships;  // DGROUP:0x9324
            if (ww->player < 0 || ww->player >= ww->pools.records(ships)) return false;
            const ObjectRecord& o = ww->pools.rec(ships, ww->player);  // record DGROUP:0x9646
            v.x = o.x;
            v.y = o.y;
            v.scr_x = o.scr_x;
            v.scr_y = o.scr_y;
            v.size = ww->size;                    // DGROUP:0x9156
            v.rect_count = rr.view.active_count;  // DGROUP:0x9158
            for (int i = 0; i < 4; ++i) {
                const ViewRect& a = rr.view.active[static_cast<std::size_t>(i)];  // DGROUP:0x23e0
                v.rects[i][0] = a.left;
                v.rects[i][1] = a.top;
                v.rects[i][2] = a.right;
                v.rects[i][3] = a.bottom;
            }
            return true;
        });
    else
        audio::set_wave_view(nullptr);
    // The flight globals stand for DGROUP words and live for the run, as DGROUP does (the 0 ... 8
    // cycle DGROUP:0x95f6 runs on across waves); the INI keys are read again for the new wave
    // [DESIGN].
    if (w) r.settings_read = false;
}
void set_update_host(UpdateHost* host) { rt().host = host; }
void set_key_actions(KeyActions* actions) { rt().default_actions.hud = actions; }
void set_rescale_host(RescaleHost* host) { g_rescale_host = host; }
// seg10:0x7f88 on DGROUP:0x9156's one home, the program's World (world/hooks.h current()), for
// the Options dialog's seg10:0x7f58 and ordinal 10's tail seg10:0x8409 (src/dialogs/hooks.cpp).
void program_size_reread() {
    if (!g_rescale_host) {
        todo_once("the INI Size reread without the drawing side", "seg10:0x7f88");
        return;
    }
    world::size_reread(current(), *g_rescale_host);  // [BYTES seg10:0x7f58 seg10:0x8409]
    size_sync(rt());
}
void program_load_size_reread(int ini_size) {
    if (g_rescale_host) {
        world::load_size_reread(current(), *g_rescale_host);  // [BYTES seg9:0x23e4-0x2418]
    } else {
        // No drawing side (the host tests): the INI's Size and shift only, the table untouched.
        struct IniOnly final : RescaleHost {
            int v;
            explicit IniOnly(int s) : v(s) {}
            int ini_size() override { return v; }
        } h(ini_size);
        world::load_size_reread(current(), h);
    }
    size_sync(rt());
}
void program_new_game_logo() {
    if (g_rescale_host) world::new_game_logo(current(), *g_rescale_host);  // [BYTES seg9:0xd38-0xd5c]
}
void program_icon_overlays() {
    Runtime& r = rt();
    if (!r.world) return;
    r.collide_host.u = &r.h();
    CollideCtx x = program_collide_ctx(r);
    icon_overlay_pass(x);  // seg9:0x94ba [BYTES seg10:0x9fb3]
}
void program_icon_crack(int i) {
    Runtime& r = rt();
    if (!r.world) return;
    r.collide_host.u = &r.h();
    CollideCtx x = program_collide_ctx(r);
    icon_damage(x, 0, i, 0);  // seg7:0x233c(0, i, 0): no sound, no rand() [BYTES seg10:0x9fbf-0x9fd7]
}
WaveLinks& wave_links() { return g_links; }
PilotLinks& pilot_links() { return g_pilot_links; }
void program_think(int ship) {
    Runtime& r = rt();
    if (!r.world) return;
    PilotCtx c = program_pilot_ctx(r);
    think(c, ship);  // seg4:0x78(ship)
}
void set_zoom_source(std::function<std::int32_t()> zoom) { rt().zoom = std::move(zoom); }
void wave_started() {
    Runtime& r = rt();
    r.keys.clear_pressed();
    reset_collide_wave(r.collide);  // [DESIGN ADR 0015] decision 1
    r.repair.due = 0;               // [BYTES seg9:0x668d], the port at the wave start [DESIGN]
    // The debris table's reset at the wave start is the port's [DESIGN]; the smoke counters are
    // the placement's (setup.cpp smoke_place) [BYTES seg9:0x6747-0x675d].
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
    // The pan's words (shift.h, view.h pan_view) (M5 s3 integrate, ENTRY binding 4).
    vi.world = &w;
    vi.shift = &linked_shift_host();
    vi.frame = &r.g;
    vi.now = r.g.now;
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

void release_unheld_keys() {
    rt().keys.release_unheld([](std::uint16_t vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; });
}

}  // namespace is::world

namespace is::app {

void install_flight_hooks(App& app) {
    using namespace is::world;
    auto& h = app.game.hooks();
    Runtime& r = rt();
    r.default_host.prev_frame = h.frame[shell::kWave];
    bind_missile_links();
    bind_tether_links();     // M4 s7 TETHER
    bind_lightning_links();  // M4 s7 RING
    // The program's pool-5 tables for the set-up's seg9:0x6624 (fuel_balls.h) (M4 s7 FUEL).
    set_pool5_tables([]() -> CollideGlobals* { return &rt().h().collide_globals(); });
    bind_effect_links();
    bind_pilot_links();   // M5 s3 integrate
    bind_callin_links();  // M5 s3 integrate
    bind_law_links();     // M5 s4 integrate
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
