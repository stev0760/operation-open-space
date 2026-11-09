// The hook point of src/desk (declared in src/app/app.h, called by WinMain after
// install_state_hooks): the desk and launch scene of state 0 phase 6, registered with the intro
// as its intro::DeskScene. Win32 only: the host build compiles this file empty.
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "app/app.h"
#include "assets/asset_store.h"
#include "desk/main_ship.h"
#include "desk/desk.h"
#include "dialogs/audio_options.h"  // is::sound (M7 audio rewiring)
#include "desk/gdi_desk.h"
#include "desk/scene_gdi.h"
#include "formats/ne.h"
#include "formats/records.h"
#include "gfx/ship_flame.h"
#include "intro/intro.h"
#include "settings/settings.h"
#include "shell/rng.h"
#include "shell/window_keys.h"
#include "world/hooks.h"
#include "world/world.h"

namespace is::desk {

namespace {

using app::g;

class DeskHost final : public Host {
public:
    Tick now() override { return app::now_tick(); }
    // The sound wrappers seg10:0x744c and seg10:0x754a and the music selector seg10:0x7480.
    void sound(int id) override { is::sound::play(id); }
    void sound_at(int id, int x, int y, int mode) override { is::sound::play_at(id, x, y, mode); }
    void music(int code) override { is::sound::music(code, 1); }
    // seg10:0x740c pumps messages; the port shows the surface and lets the time pass without
    // dispatching, so no key reaches the intro inside its own frame (as present_wait in
    // src/app/main.cpp) [DESIGN].
    void pump() override { app::present_frame(); }
    void pump_until(Tick t) override { wait_until(t); }
    void busy_until(Tick t) override { wait_until(t); }
    std::string lang(int id) override {
        try {
            if (g.store)
                if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) return *s;
        } catch (const std::exception& x) {
            app::log("string " + std::to_string(id) + ": " + x.what());
        }
        return {};
    }
    std::string lang_text(int type, int id) override {
        try {
            if (g.store) {
                const fmt::NeModule& m = g.store->language_dll();
                if (const fmt::NeResource* r = m.find(static_cast<std::uint16_t>(type), static_cast<std::uint16_t>(id))) {
                    std::string s(reinterpret_cast<const char*>(m.data(*r)), m.size(*r));
                    if (auto z = s.find('\0'); z != std::string::npos) s.resize(z);
                    return s;
                }
            }
        } catch (const std::exception& x) {
            app::log("#" + std::to_string(type) + " " + std::to_string(id) + ": " + x.what());
        }
        return {};
    }
    std::string ini(const char* key, const char* def) override {
        return g.ini ? g.ini->get_string(settings::kGameSection, key, def) : std::string(def);
    }
    int ini_int(const char* key, int def) override {
        return g.ini ? g.ini->get_int(settings::kGameSection, key, def) : def;
    }
    // LocalPath + Games + Restore + ".sav", Games at most 10 characters with '\' appended
    // (seg6:0x6d8, as dialogs/save_game.h games_dir) [BYTES seg15:0x53e2-0x5405 seg15:0x54af]
    // (M4 s4 fixer B).
    std::optional<std::vector<std::uint8_t>> saved_header(const std::string& restore) override {
        std::string games = g.ini ? g.ini->get_string(settings::kGameSection, "Games", "", 11) : std::string();
        if (!games.empty() && games.back() != '\\') games += '\\';
        const std::string rel = games + restore + ".sav";
        // cp1252 to UTF-16 (ADR 0003 decisions 1 and 2) [DESIGN].
        const int n = MultiByteToWideChar(1252, 0, rel.data(), static_cast<int>(rel.size()), nullptr, 0);
        std::wstring w(static_cast<std::size_t>(n > 0 ? n : 0), L'\0');
        if (n > 0) MultiByteToWideChar(1252, 0, rel.data(), static_cast<int>(rel.size()), w.data(), n);
        const std::filesystem::path p = g.local_path / std::filesystem::path(w);
        HANDLE f = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) return std::nullopt;
        std::vector<std::uint8_t> out(0x1a9);
        DWORD got = 0;
        if (!ReadFile(f, out.data(), static_cast<DWORD>(out.size()), &got, nullptr)) got = 0;
        CloseHandle(f);
        out.resize(got);
        return out;
    }
    Clock clock() override {
        const std::time_t t = std::time(nullptr);
        const std::tm* tm = std::localtime(&t);
        return tm ? Clock{tm->tm_wday, tm->tm_hour, tm->tm_min} : Clock{};
    }
    int rand() override { return shell::game_rng().rand(); }
    int calibrate_value() override { return shell::kCalibrate; }
    // ISPACE.DLL seg8:0x5c86 on the system (ruling R3 of M4 stage 6) [BYTES ISPACE.DLL
    // seg8:0x5c95-0x5fa5].
    Conditions conditions() override { return desk::read_devices(); }
    Render render(int id, int W, int H) override {
        try {
            if (g.store) {
                // seg10:0x9526 with LoadString(ENG_LANG.DLL) for the record strings [BYTES
                // seg10:0x9530-0x955d] [BYTES ISPACE.DLL seg8:0x1b45-0x1b54].
                Render r = desk::render_scene(*g.store, id, W, H, g.screen_cx, g.screen_cy,
                                              [this](int s) { return lang(s); });
                char buf[160];
                for (std::size_t i = 0; i < r.slots.size(); ++i) {
                    const Rect& s = r.slots[i];
                    std::snprintf(buf, sizeof buf, "desk: slot %zu (%d, %d, %d, %d)", i, s.l, s.t, s.r, s.b);
                    app::log(buf);
                }
                return r;
            }
        } catch (const std::exception& x) {
            app::log_todo(std::string("#6600 ") + std::to_string(id) + ": " + x.what(), "ISPACE.DLL seg8:0x5b76");
        }
        Render r;
        r.proj.W = W;
        r.proj.H = H;
        return r;
    }
    void todo(const std::string& what, const char* address) override {
        if (todos_.insert(what + address).second) app::log_todo(what, address);
    }

private:
    static void wait_until(Tick t) {
        if (g.gfx && g.main) g.gfx->present(g.main, true);
        GdiFlush();
        for (Tick n = app::now_tick(); static_cast<std::int32_t>(t - n) > 0; n = app::now_tick())
            MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>(t - n), 0, 0);
    }
    std::set<std::string> todos_;
};

class SceneHook final : public intro::DeskScene {
public:
    SceneHook(Canvas& c, Host& h) : desk_(c, h) {}
    void enter(Tick now, const intro::Area& a) override {
        app::log("desk: entry seg15:0xcb0");
        desk_.enter(now, a.w, a.h, a.play_h);
    }
    bool frame(Tick now) override {
        const int before = desk_.step();
        const bool done = desk_.frame(now);
        if (desk_.step() != before) app::log("desk: step " + std::to_string(desk_.step()));
        return done;
    }
    void escape() override { desk_.escape(); }
    void space(Tick now) override { desk_.space(now); }

private:
    Desk desk_;
};

// INI ShipPath resolved as ADR 0004 decision 4 (the same rule as src/dialogs/hooks.cpp): the
// key's directory when it holds MASTER.SHP, else the asset directory's SHIPS\, else the asset
// directory [DESIGN].
std::filesystem::path ship_dir() {
    std::error_code ec;
    const std::string s = g.ini ? g.ini->get_string(settings::kGameSection, "ShipPath", "") : std::string();
    if (!s.empty()) {
        const int n = MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
        const std::filesystem::path p(w);
        if (std::filesystem::exists(p / L"MASTER.SHP", ec)) return p;
    }
    if (std::filesystem::exists(g.asset_dir / L"SHIPS" / L"MASTER.SHP", ec)) return g.asset_dir / L"SHIPS";
    return g.asset_dir;
}

// seg15:0x2a08's record lookup: MainShip against SHP + 0x0a by exact lstrcmp, then the body
// record SHP + 0x21 and its graphic BOD + 0x21 [BYTES seg15:0x2a2f seg15:0x2a50 seg15:0x2b8f]. A
// name no record matches reads record DGROUP:0x95aa, one past the table; the port reads the zeroed
// scratch record (body 0, SHP +0x07 = +0x08 = +0x09 = 0) and logs that read once [DESIGN ADR 0016
// decision 2] (ruling R4 of M4 stage 6) [BYTES seg15:0x2a55-0x2a8e] (M4 s6 PAD, binding 5).
std::optional<ShipArt> ship_art(const std::string& name) {
    if (!g.store) return std::nullopt;
    try {
        const assets::Ships ships = g.store->ships(ship_dir());
        const fmt::ShipRecord* r = main_ship_record(ships, name, [](const std::string& w, const char* a) {
            app::log_todo(w, a);
        });  // desk/main_ship.h
        const fmt::BodyRecord* b = ships.body(*r);
        if (!b || b->bitmap_id == fmt::kBodyBitmapFromFile) return std::nullopt;  // [OPEN] a .DIB file
        ShipArt a;
        a.body = g.store->sprite(b->bitmap_id);
        a.id = b->bitmap_id;
        a.a = r->byte_07;
        a.b = r->byte_08;
        // sext BOD +0x19 and +0x1a, BOD +0x20, SHP +0x09 and graphic 280 [BYTES seg15:0x2abf-0x2af0
        // seg15:0x2b5c-0x2b72] (gfx/launch_ship.h LaunchShipArt).
        a.fy = static_cast<std::int8_t>(b->byte_19);
        a.band = static_cast<std::int8_t>(b->byte_1a);
        a.body_flags = b->flags;
        a.ship_flags = r->flags;
        a.flame = g.store->sprite(gfx::kFlameGraphic);
        // A kept or restored game (DGROUP:0x92e4 != 0): the wing bytes +0x72 / +0x73 of roster
        // record DGROUP:0x931c for seg15:0x2a08's cuts; otherwise both wings stay on [BYTES
        // seg15:0x2edf-0x2f8c] (verified claim 30, CORRECTED).
        const world::World& w = world::current();
        if (w.restored != 0) {
            a.keep = true;
            if (w.player_roster >= 0 && static_cast<std::size_t>(w.player_roster) < w.roster.size()) {
                const world::RosterRecord& R = w.roster[static_cast<std::size_t>(w.player_roster)];
                a.left_wing = R.wing0 != 0;
                a.right_wing = R.wing1 != 0;
            }
        }
        return a;
    } catch (const std::exception& x) {
        app::log_todo(std::string("launch ship: ") + x.what(), "seg15:0x2a08");
        return std::nullopt;
    }
}

std::unique_ptr<DeskHost> g_host;
std::unique_ptr<GdiDesk> g_canvas;
std::unique_ptr<SceneHook> g_scene;

}  // namespace

}  // namespace is::desk

namespace is::app {
void install_desk_hooks(App&) {
    using namespace is::desk;
    g_host = std::make_unique<DeskHost>();
    g_canvas = std::make_unique<GdiDesk>([] { return g.gfx.get(); },
                                         [](const std::string& w, const char* a) { log_todo(w, a); },
                                         ship_art);
    program_ships() = ship_art;  // the visit's launch and new body (src/hud/hooks.cpp)
    g_scene = std::make_unique<SceneHook>(*g_canvas, *g_host);
    intro::set_desk_scene(g_scene.get());
}
}  // namespace is::app
#endif
