// The hook point of src/hall (app.h install_hall_hooks: binding request): state 8's frame
// (seg7:0x197's seg16:0x6928(0), one per scheduler frame) and repaint (seg11:0xafe case 8:
// seg16:0x6928(1)), and the state-8 entry of world/wave_end.cpp (hall_entry_slot). Win32 only:
// the host build compiles this file empty.
#ifdef _WIN32
#include <windows.h>

#include <ctime>
#include <memory>
#include <set>
#include <string>

#include "app/app.h"
#include "assets/asset_store.h"
#include "desk/scene_gdi.h"
#include "dialogs/audio_options.h"  // is::sound
#include "hall/hall.h"
#include "hall/hall_gdi.h"
#include "settings/settings.h"
#include "shell/game_state.h"
#include "shell/rng.h"
#include "shell/window_keys.h"
#include "world/hooks.h"
#include "world/wave_end.h"
#include "world/world.h"

namespace is::hall {

namespace {

using app::g;

void hall_todo(const std::string& what, const char* address) {
    static std::set<std::string> seen;
    if (seen.insert(std::string(address) + what).second) app::log_todo("hall: " + what, address);
}

std::string lang_text(int id) {
    try {
        if (g.store)
            if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) return *s;
    } catch (const std::exception& x) {
        app::log("string " + std::to_string(id) + ": " + x.what());
    }
    return {};
}

class ProgramHallHost final : public Host {
public:
    Tick now() override { return app::now_tick(); }
    Tick pass_now() override { return g.game.now(); }
    void sound(int id) override { is::sound::play(id); }
    void sound_at(int id, int a, int b, int mode) override { is::sound::play_at(id, a, b, mode); }
    void music(int code, int arg) override { is::sound::music(code, arg); }
    // seg10:0x740c: the port shows the surface and lets the time pass without dispatching, as the
    // desk's and the visit's hosts do [DESIGN].
    void pump_until(Tick t) override {
        app::present_frame();
        while (static_cast<std::int32_t>(t - app::now_tick()) > 0) Sleep(1);
    }
    void pump() override { app::present_frame(); }
    bool ctrl_held() override { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }
    int quick_intro() override { return 0; }  // DGROUP:0x8f70, as the visit's host [OPEN: its writer]
    // DGROUP:0x8ffc: the program's one Calibrate (ADR 0001 decision 9).
    int calibrate() override { return shell::kCalibrate; }
    int depth() override { return g.depth; }
    std::string lang(int id) override { return lang_text(id); }
    std::string pilot() override {
        return g.ini ? g.ini->get_string(settings::kGameSection, "ThePlayer", "", 12) : std::string();
    }
    Clock clock() override {
        const std::time_t t = std::time(nullptr);  // seg5:0x5be
        const std::tm* tm = std::localtime(&t);    // seg5:0x4c0
        return tm ? Clock{tm->tm_hour, tm->tm_min, tm->tm_mon, tm->tm_mday, tm->tm_year} : Clock{};
    }
    std::string main_ship() override {
        return g.ini ? g.ini->get_string(settings::kGameSection, "MainShip", "EAGLE", 20) : std::string("EAGLE");
    }
    void close_hud() override {
        if (world::wave_end_links().close_hud) world::wave_end_links().close_hud();  // seg11:0x2f0
        else hall_todo("seg11:0x2f0 has no slot", "seg11:0x2f0");
    }
    void watch_windows(int code) override {
        // seg13:0x65f2(3) [BYTES seg15:0x3045]: the watch windows hidden (hud/watch.h
        // watch_windows through WaveEndLinks::watch_windows) (M5 s4 integrate).
        if (world::wave_end_links().watch_windows) world::wave_end_links().watch_windows(code);
        else hall_todo("seg13:0x65f2(" + std::to_string(code) + ") at the Hall's landing", "seg13:0x65f2");
    }
    void debriefing() override {
        if (world::debrief_entry_slot()) world::debrief_entry_slot()(g.game, 1);  // seg9:0x6cc4(1)
        else hall_todo("seg9:0x6cc4(1) has no slot (world/hooks.cpp binding)", "seg9:0x6cc4");
    }
    void set_state(int s) override { g.game.set(s); }
    void todo(const std::string& what, const char* address) override { hall_todo(what, address); }
};

std::unique_ptr<HallGdi> g_screen;
std::unique_ptr<ProgramHallHost> g_host;
std::unique_ptr<Hall> g_hall;

// The log's trace of the phases DGROUP:0x929c, and the text of seg16:0x6afe once it is made
// (phase 3), with the pilot's name masked [DESIGN: a diagnostic of the port] (M5 s4 integrate).
void trace_phase(int before) {
    const int now = g_hall->words().phase;
    if (now == before) return;
    app::log("hall: phase " + std::to_string(before) + " -> " + std::to_string(now) + " at " +
             std::to_string(app::now_tick()) + " ms");
    if (now != 3) return;
    std::string t = g_hall->words().text;
    std::string p = g_host->pilot();
    for (char& ch : p) ch = static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch);
    if (!p.empty())
        for (std::size_t k = t.find(p); k != std::string::npos; k = t.find(p, k + 7)) t.replace(k, p.size(), "<pilot>");
    for (char& ch : t)
        if (ch == '\r' || ch == '\n') ch = '|';
    app::log("hall: text (seg16:0x6afe) " + t);
}

}  // namespace

}  // namespace is::hall

namespace is::app {

void install_hall_hooks(App& app) {
    using namespace is::hall;
    g_screen = std::make_unique<HallGdi>([] { return g.gfx.get(); }, hall_todo);
    // seg10:0x9526 for scenes 110 and 115 (ISPACE.DLL ordinal 32, src/desk/scene_gdi.h).
    g_screen->scene = [](int id, int W, int H) -> desk::Render {
        try {
            if (g.store) return desk::render_scene(*g.store, id, W, H, g.screen_cx, g.screen_cy, lang_text);
        } catch (const std::exception& x) {
            hall_todo(std::string("#6600 ") + std::to_string(id) + ": " + x.what(), "ISPACE.DLL seg8:0x5b76");
        }
        desk::Render r;
        r.proj.W = W;
        r.proj.H = H;
        return r;
    };
    g_host = std::make_unique<ProgramHallHost>();
    g_hall = std::make_unique<Hall>(world::current(), *g_screen, g_screen->desk(), *g_host, shell::game_rng());
    // seg9:0x6c6a-0x6c6e: the play area DGROUP:0x90ea x DGROUP:0x10 and DGROUP:0x90ec.
    world::hall_entry_slot() = [] {
        g_hall->player_game = world::current().player_game != 0;  // DGROUP:0x95bc
        app::log("state 8: seg9:0x6c62, the Hall of Justice");
        g_hall->enter(g.area.w, g.area.h, g.area.play_h);
    };
    auto& h = app.game.hooks();
    h.frame[shell::kJustice] = [](shell::GameState&) {  // seg7:0x1b7
        const int before = g_hall->words().phase;
        g_hall->frame(0);
        trace_phase(before);
    };
    h.repaint[shell::kJustice] = [](shell::GameState&) { g_hall->frame(1); };  // seg11:0x1686
}

}  // namespace is::app
#endif
