// The hook point of src/hud (install_hud_hooks, called by WinMain after the other modules'):
// the wave HUD on the surface (ADR 0013), the repaint of states 1 and 7 (seg11:0xafe), the HUD's
// part of the state-1 entry (seg11:0x0), the IS_INFO panels' paint after each wave frame, the
// scrollbar's relayout and seg11:0x2f0 at the end of the wave states. Win32 only: the host build
// compiles this file empty.
#ifdef _WIN32
#include <windows.h>

#include <exception>
#include <memory>
#include <set>
#include <string>

#include "app/app.h"
#include "hud/gdi_canvas.h"
#include "hud/hooks.h"
#include "hud/hud.h"
#include "hud/program.h"
#include "hud/scrollbar.h"
#include "hud/wave.h"
#include "shell/rng.h"
#include "world/flight_hooks.h"
#include "world/hooks.h"
#include "world/world.h"

namespace is::hud {

namespace {

using app::g;

template <class T, class F, class... A>
T call(const F& f, T fallback, A... a) {
    return f ? f(a...) : fallback;
}

// hud::Host for the program: the shell's globals from app::g, the rest through links().
class AppHost final : public Host {
public:
    int state() const override { return g.game.state(); }
    shell::PlayArea area() const override { return g.area; }
    bool border() const override { return g.keys.border != 0; }
    std::uint32_t now() const override { return g.game.now(); }
    std::string lang(int id) override {
        try {
            if (g.store)
                if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) return *s;
        } catch (const std::exception& x) {
            app::log("hud: string " + std::to_string(id) + ": " + x.what());
        }
        return {};
    }
    bool monochrome() const override { return g.depth == 1; }  // DGROUP:0x90ee
    void log(const std::string& s) override { app::log(s); }
    // srand(GetCurrentTime()): the low word of the port's tick (ADR 0001 decision 5) [DESIGN].
    void seed_rng() override {
        shell::game_rng().srand(static_cast<std::uint16_t>(app::now_tick() & 0xffff));
    }

    // The World (src/world/hooks.h) holds the DGROUP words the set-up writes; a links() slot,
    // when filled, takes precedence.
    bool race() const override { return call(links().race, w().handover.race != 0); }
    bool demo_gate() const override { return call(links().demo_gate, w().w9130 != 0); }
    bool alt_view() const override { return call(links().alt_view, false); }
    int player() const override { return call(links().player, int{w().player}); }
    int ship_pool() const override { return call(links().ship_pool, int{w().roles.ships}); }
    int watch_ship() const override { return call(links().watch_ship, int{w().w952e}); }
    int view_mode() const override { return call(links().view_mode, int{w().w9e49}); }
    bool blink() const override { return call(links().blink, false); }

    std::uint32_t score() const override { return call(links().score, std::uint32_t{0}); }
    bool score_alt() const override { return call(links().score_alt, false); }
    bool score_alt_hi() const override { return call(links().score_alt_hi, false); }
    std::uint32_t wave_ticks() const override { return call(links().wave_ticks, w().frames); }
    void set_wave_seconds(int s) override { w().w934e = static_cast<std::int16_t>(s); }
    // The ship record's fields; the design table (DGROUP:0x96f0 + 0xad·class) has no port yet,
    // so its words keep ShipView's defaults until links().ship fills them [OPEN].
    ShipView ship(int i) const override {
        if (links().ship) return links().ship(i);
        ShipView v;
        if (i < 0 || i >= world::kShipCount) return v;
        world::ShipRecord& r = w().ship(i);
        for (int k = 0; k < 4; ++k) v.health[k] = r.roster_word(k);
        v.b_e7 = r.be7 != 0;
        v.b_e9 = r.be9 != 0;
        v.fuel = r.fuel;
        return v;
    }
    // The weapon table DGROUP:0x2f54 and the count byte DGROUP:0x713e (world.md "Weapon
    // display" step 2) have no port yet: unlinked, the line reads "FUEL - " and a default name
    // [OPEN weapon table, a later stage (M4)].
    WeaponView weapon() const override { return call(links().weapon, WeaponView{}); }
    void message(int ship, int id) override {
        if (links().message) links().message(ship, id);
    }
    int frame_rate_set() const override {
        return links().frame_rate_set ? links().frame_rate_set() : g.scheduler.rate();
    }
    // DGROUP:0x59a, the frames of the last second (timing.md "Every 1000 ms") has no port in the
    // scheduler yet: unlinked, the B display's ACTUAL line reads 0 [OPEN frame count, ADR 0001].
    int frame_rate_actual() const override { return call(links().frame_rate_actual, 0); }

    // seg11:0x3f2e's attitude, seg3:0x0 and seg13:0x371c are not ported: unlinked, attitude 0,
    // every target acceptable and kind 0 [OPEN target tests, a later stage].
    int attitude(int a, int b) const override { return call(links().attitude, 0, a, b); }
    bool target_ok(int p, int r) const override { return call(links().target_ok, true, p, r); }
    int target_kind(int p, int r) const override { return call(links().target_kind, 0, p, r); }
    void bump_bad_index() override {
        if (links().bump_bad_index) links().bump_bad_index();
    }
    std::string text_9628() const override { return call(links().text_9628, t9628_); }
    std::string text_8718() const override { return call(links().text_8718, t8718_); }
    std::string text_960a() const override { return call(links().text_960a, t960a_); }
    std::string text_8740() const override { return call(links().text_8740, std::string()); }
    std::string text_8768() const override { return call(links().text_8768, std::string()); }
    std::string text_87b8() const override { return call(links().text_87b8, std::string()); }
    // seg11:0x2ce6 / seg13:0x58dc, the target's picture (world.md "Target display" step 3):
    // unlinked, no icon is drawn [OPEN target icon, a later stage].
    void draw_target_icon(Canvas& c) override {
        if (links().draw_target_icon) links().draw_target_icon(c);
    }
    void draw_race(Canvas& c) override {
        if (links().draw_race) links().draw_race(c);
    }

    int player_target_pool() const override {
        return call(links().player_target_pool, int{w().ship(w().player).target_pool});
    }
    int player_target_rec() const override {
        return call(links().player_target_rec, int{w().ship(w().player).target_rec});
    }
    // seg13:0x2206 is not ported: unlinked, -1 makes Ctrl+T and the retarget call
    // seg3:0xf4(player, 0xff, 0xff) (world.md "Key messages", Ctrl+T) [OPEN seg13:0x2206].
    int next_target() override { return call(links().next_target, -1); }
    // seg3:0xf4(ship, pool, rec) as far as the HUD knows it, until links().set_target holds the
    // whole routine: the ship's target words, the target display's object and texts, and the
    // repaint [BYTES seg3:0x102-0x1ae seg3:0x259-0x26c seg3:0x41f-0x440]. The texts that
    // seg3:0x1542, seg13:0x218a and seg17:0x8e60 make, and seg7:0x65aa, are [OPEN].
    void set_target(int s, int p, int r) override {
        if (links().set_target) return links().set_target(s, p, r);
        if (s < 0 || s >= world::kShipCount) return;
        world::ShipRecord& ship = w().ship(s);
        ship.target_pool = static_cast<std::int16_t>(p);  // seg3:0x10a: +0x3e
        ship.target_rec = static_cast<std::int16_t>(r);   // seg3:0x111: +0x40
        if (s != w().player) return;                       // seg3:0x115-0x11b
        Hud* h = hud();
        if (!h) return;
        h->info().mark(0).pool = p;  // seg3:0x122: DGROUP:0x6e54
        h->info().mark(0).rec = r;   // seg3:0x128: DGROUP:0x6e52
        if (!target_ok(p, r) && target_kind(p, r) == 0) {  // seg3:0x12c-0x14d
            bump_bad_index();
            return;
        }
        t960a_.clear();  // seg3:0x150-0x165: DGROUP:0x13be, DGROUP:0x13bf
        t9628_.clear();
        t8718_.clear();
        if (p == -1) {  // seg3:0x16a-0x1ae: strings 2205 and 2206
            t9628_ = lang(0x89d);
            t8718_ = lang(0x89e);
        } else if (p == 0 && r >= 0 && r < world::kIconRecords) {
            // seg3:0x259-0x2c6: the icon's file name (DGROUP:0x57b4 + 0x23·rec), then
            // seg3:0x1542's text [OPEN] with string 2208 or 2209 by byte +0x53.
            t9628_ = w().icons[static_cast<std::size_t>(r)].name;
        } else {
            log_once("the target texts of pool " + std::to_string(p) + " (seg3:0x1542)");
        }
        h->info().invalidate(kInfoTarget);  // seg3:0x42d-0x440
    }
    Point screen_point(int p, int r) const override {
        return call(links().screen_point, Point{}, p, r);
    }
    Point player_point() const override {
        if (links().player_point) return links().player_point();
        const world::ObjectRecord& p = w().pools.rec(w().roles.ships, w().player);
        return Point{p.scr_x, p.scr_y};  // +0x00, +0x02 [BYTES seg13:0x25b0-0x25cb]
    }
    int direction16(int dx, int dy) const override {
        return call(links().direction16, 0, dx, dy);
    }
    int in_use(int p) const override {
        if (links().in_use) return links().in_use(p);
        return p >= 0 && p < world::kPoolCount ? w().pools.in_use(p) : 0;
    }
    // seg8:0x285a on the two records' world positions (world::distance_cells).
    int distance(int pa, int ra, int pb, int rb) const override {
        if (links().distance) return links().distance(pa, ra, pb, rb);
        const world::ObjectRecord& a = w().pools.rec(pa, ra);
        const world::ObjectRecord& b = w().pools.rec(pb, rb);
        std::int16_t dx = 0, dy = 0;
        return world::distance_cells(a.x, a.y, b.x, b.y, dx, dy);
    }
    // seg3:0x65e code 3 on the World's pool-8 side table (M3 s3 fixer) [BYTES seg3:0x6a2-0x6b8].
    bool gate_post_live(int r) const override {
        return call(links().gate_post_live, world::gate_post_live(w(), r), r);
    }
    int record_bound_9422() const override { return call(links().record_bound_9422, 0); }
    bool mark2_window() const override { return call(links().mark2_window, false); }
    void draw_mark2() override {
        if (links().draw_mark2) links().draw_mark2();
    }

    void demo_mark() override { run(links().demo_mark); }
    void pin_player() override { run(links().pin_player); }
    void draw_player() override { run(links().draw_player); }
    void set_58c() override { run(links().set_58c); }
    void draw_1f38() override { run(links().draw_1f38); }
    void radar() override { run(links().radar); }
    void radar_centre() override { run(links().radar_centre); }

    // DGROUP:0xa76 is the World's w0a76, which the flight role shares.
    int info_mode() const override { return w().w0a76; }
    void set_info_mode(int m) override { w().w0a76 = static_cast<std::int16_t>(m); }

private:
    static world::World& w() { return world::current(); }
    void log_once(const std::string& what) {
        if (logged_.insert(what).second) app::log_todo(what, "seg3:0xf4");
    }
    std::string t9628_, t8718_, t960a_;  // DGROUP:0x9628, 0x8718, 0x960a (the fallback's)
    std::set<std::string> logged_;
    static void run(const std::function<void()>& f) {
        if (f) f();
    }
};

AppHost g_host;

// The HUD's part of the key switch seg11:0x6b64 (world::KeyActions, set_key_actions).
class HudKeyActions final : public world::KeyActions {
public:
    void target_code(bool ctrl, int code) override {
        if (Hud* h = hud()) h->info().target_code(ctrl, code);  // seg11:0x772e
    }
    void toggle_manual_target() override {
        if (Hud* h = hud()) h->info().toggle_manual();  // seg11:0x7138
    }
    void zoom_changed(std::int32_t zoom) override {
        if (links().zoom_changed) return links().zoom_changed(zoom);
        // seg11:0x723a-0x7247: SetScrollPos(600 - zoom), then the radar.
        set_scroll_pos(static_cast<std::int16_t>(600 - static_cast<std::int16_t>(zoom)));
        if (links().radar) links().radar();
    }
    // B, M and seg10:0x7e1c's repaint (M3 s2 fixer).
    void show_frame_rate() override {
        if (Hud* h = hud()) h->info().show_frame_rate();  // seg11:0x71d4
    }
    void show_messages() override {
        if (Hud* h = hud()) h->info().show_messages();  // seg11:0x71a9
    }
    void info_repaint() override {
        if (Hud* h = hud()) h->info().invalidate(kInfoTarget);  // PostMessage(DGROUP:0xa7c, WM_PAINT)
    }
};
HudKeyActions g_actions;
std::unique_ptr<GdiCanvas> g_canvas;
std::unique_ptr<ScrollBar> g_scroll;
std::unique_ptr<Hud> g_hud;
bool g_load_failed = false;

// seg11:0x0's HUD part, once (while DGROUP:0x5042 = 0 in the original) [BYTES seg11:0xd].
void ensure_loaded() {
    if (!g_hud || g_hud->is_loaded() || g_load_failed || !g.gfx || g.gfx->surface().empty())
        return;
    std::string why;
    if (!g_canvas->load(&why)) {
        g_load_failed = true;
        app::log("hud: the HUD bitmaps did not load: " + why);
        return;
    }
    g_hud->loaded();
    app::log("hud: seg11:0x0 loaded METAL, 250, 240, 45, 260, DIGITAL, SYSTEMS");
}

}  // namespace

HBRUSH metal_brush() { return g_canvas ? g_canvas->metal_brush() : nullptr; }

namespace {

void close_hud() {
    if (g_scroll) g_scroll->destroy();       // seg11:0x2fa-0x30a: DGROUP:0xa7a
    if (g_hud) g_hud->info().close();         // seg11:0x310-0x336: DGROUP:0xa7c, DGROUP:0xa7e
}

}  // namespace

}  // namespace is::hud

namespace is::app {

void install_hud_hooks(App& app) {
    using namespace is::hud;
    g_canvas = std::make_unique<GdiCanvas>(*app.gfx, [] { return g.font; });
    // The position 600 - zoom comes from the radar (links().scroll_pos); 600 - 120 before it
    // is linked [BYTES DGROUP:0xa6c].
    g_scroll = std::make_unique<ScrollBar>(
        [] { return links().scroll_pos ? links().scroll_pos() : 600 - 120; },
        [] { return g.main; }, [] { return g.gfx ? g.gfx->factor() : 1; });
    g_hud = std::make_unique<Hud>(*g_canvas, g_host, g_scroll.get());
    bind_program(
        g_hud.get(), [](int pos) { g_scroll->set_pos(pos); }, [] { close_hud(); });

    world::set_key_actions(&g_actions);
    install_wave(app);  // the wave's integration, src/hud/wave_win32.cpp (M3 s2 fixer)

    auto& h = app.game.hooks();
    // seg11:0xafe for states 1 and 7 (shell::GameState::repaint runs entry 1 for both): the HUD
    // and then the state dispatch; the IS_INFO panels paint after it, as the new child windows'
    // first WM_PAINT did [DESIGN ADR 0013 decision 4].
    h.repaint[shell::kWave] = [](shell::GameState&) {
        ensure_loaded();
        g_hud->repaint();
        g_hud->info().flush();
    };
    // The state-1 entry (seg10:0x0, whose seg10:0x112 runs seg11:0x0 once) [BYTES seg10:0x112].
    auto prev_enter = h.enter[shell::kWave];
    h.enter[shell::kWave] = [prev_enter](shell::GameState& s) {
        if (prev_enter) prev_enter(s);
        ensure_loaded();
        load_wave_resources();  // the rock images and the radar (M3 s2 fixer)
    };
    // After each wave frame: the IS_INFO panels a PostMessage(WM_PAINT) marked (ADR 0013
    // decision 4).
    auto prev_frame = h.frame[shell::kWave];
    h.frame[shell::kWave] = [prev_frame](shell::GameState& s) {
        if (prev_frame) prev_frame(s);
        if (s.state() == shell::kWave || s.state() == shell::kHelp) g_hud->info().flush();
    };
    // The play area changed (seg2:0x1ac8): the scrollbar follows the scale factor [DESIGN].
    auto prev_area = h.play_area;
    h.play_area = [prev_area](shell::GameState& s) {
        if (prev_area) prev_area(s);
        if (g_scroll) g_scroll->relayout();
    };
    // seg11:0x2f0 at the wave teardown (seg10:0x259) and at the entries of the states that
    // leave the wave (shell.md "Closing the HUD"); state 7 keeps the HUD.
    auto prev_teardown = h.teardown;
    h.teardown = [prev_teardown](shell::GameState& s) {
        if (prev_teardown) prev_teardown(s);
        close_hud();
    };
    for (int st = 0; st < shell::kStateCount; ++st) {
        if (st == shell::kWave || st == shell::kHelp) continue;
        auto prev = h.enter[static_cast<std::size_t>(st)];
        h.enter[static_cast<std::size_t>(st)] = [prev](shell::GameState& s) {
            close_hud();
            if (prev) prev(s);
        };
    }
}

}  // namespace is::app
#endif
