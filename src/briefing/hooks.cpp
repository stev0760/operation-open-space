// The hook point of src/briefing (declared in src/app/app.h, called by WinMain after
// install_state_hooks): state 3, the wave mission briefing, and the state-1 hand-off before M3
// (docs/re/shell.md "Screen states"). Win32 only: the host build compiles this file empty.
#ifdef _WIN32
#include "briefing/hooks.h"

#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "app/app.h"
#include "formats/icon_source.h"
#include "audio/audio.h"
#include "dialogs/audio_options.h"  // is::sound (M7 audio rewiring)
#include "shell/geometry.h"
#include "briefing/briefing.h"
#include "briefing/gdi_canvas.h"
#include "formats/ne.h"
#include "world/hooks.h"

namespace is::briefing {

namespace {

using app::g;

// ---- the program's side of the briefing

class BriefingHost final : public Host {
public:
    std::string lang(int id) override {
        try {
            if (g.store)
                if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) return *s;
        } catch (const std::exception& x) {
            app::log("string " + std::to_string(id) + ": " + x.what());
        }
        return {};
    }
    // seg10:0x744c(id); the positional calls go through sound_at (briefing.h).
    void sound(int id) override { is::sound::play(id); }
    void sound_at(int id, int x, int y, int mode) override { is::sound::play_at(id, x, y, mode); }
    // seg11:0xb09c(on, ship) of phase 11 on the program's World (src/world/hooks.h).
    void flame(int on, int ship) override { world::briefing_flame(world::current(), on, ship); }
    void todo(const std::string& what, const char* address) override {
        if (todos_.insert(what + address).second) app::log_todo(what, address);
    }
    std::vector<std::uint8_t> page(int id) override {
        try {
            if (g.store) {
                const fmt::NeModule& m = g.store->language_dll();
                if (const fmt::NeResource* r = m.find(5000, static_cast<std::uint16_t>(id)))
                    return std::vector<std::uint8_t>(m.data(*r), m.data(*r) + m.size(*r));
            }
        } catch (const std::exception& x) {
            app::log("#5000 " + std::to_string(id) + ": " + x.what());
        }
        app::log("#5000 " + std::to_string(id) + " missing");
        return {};
    }

private:
    std::set<std::string> todos_;
};

std::unique_ptr<BriefingHost> g_host;
std::unique_ptr<GdiCanvas> g_canvas;
std::unique_ptr<Briefing> g_briefing;
std::string g_selection;        // DGROUP:0x38c8 as the game-start flow set it
WaveFlags g_handover;           // DGROUP:0x92e8, 0x94e, 0x950 as Navigation's Enter set them
bool g_handoff = false;         // state 3 is live: the next state-1 entry is seg9:0x60bc
bool g_loading_drawn = false;   // DGROUP:0x92e6 [BYTES seg10:0xa9 seg9:0x27b3]
unsigned long g_wave_frames = 0;  // state-1 frames since seg9:0x60bc
int g_logged_phase = -1;

void todo_once(const std::string& what, const char* address) {
    if (g_host) g_host->todo(what, address);
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(1252, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0,
                                      nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n > 0 ? n : 0), '\0');
    if (n > 0)
        WideCharToMultiByte(1252, 0, w.c_str(), static_cast<int>(w.size()), &out[0], n, nullptr,
                            nullptr);
    return out;
}

// The INI Drive root: the first character of Drive (default "C", 10 bytes) + ":\" [DESIGN:
// stands for the Navigation window's selection until it exists].
std::string drive_root() {
    std::string d = g.ini ? g.ini->get_string(settings::kGameSection, "Drive", "C", 10) : "C";
    char c = d.empty() ? 'C' : d[0];
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return std::string(1, c) + ":\\";
}

// The briefing's icon cells: the wave set-up's strip DGROUP:0x3a02 (src/world/hooks.h, built from
// the icon records of seg9:0x0 through the Walker, ADR 0012), each 32 x 32 cell drawn at the
// briefing's (32 << Size) / 2 with (6 << Size) / 2 gaps [OPEN: world.md "Icon strip scale"].
gfx::ImagePtr icon_cells(int slots, int b) {
    const int o = (6 << b) >> 1, s = (32 << b) >> 1;
    const gfx::ImagePtr strip = world::icon_strip();
    if (slots <= 0 || !g.gfx || !strip) return {};
    gfx::ImagePtr img;
    try {
        img = g.gfx->create_image((s + o) * slots + o, s + 2 * o);
    } catch (const std::exception& x) {
        app::log(std::string("icon cells: ") + x.what());
        return {};
    }
    PatBlt(img->hdc(), 0, 0, (s + o) * slots + o, s + 2 * o, BLACKNESS);
    for (int k = 0; k < slots; ++k)
        StretchBlt(img->hdc(), (s + o) * k + o, o, s, s, strip->hdc(), world::strip_x(k), 6, 32, 32,
                   SRCCOPY);
    GdiFlush();
    return img;
}

// Phase 11's values for the player's ship from the ship records of the placement seg9:0x6624,
// which the world's part of the entry has just run (src/world/hooks.cpp briefing_entry).
void player_stats(Setup& st) {
    world::World& w = world::current();
    const int p = w.player;  // DGROUP:0x9646
    if (p < 0 || p >= world::kShipCount || w.pools.records(w.roles.ships) <= p) return;
    world::ShipRecord& s = w.ship(p);
    const world::ObjectRecord& o = w.ship_object(p);
    ShipStats& ps = st.player;
    ps.score = static_cast<long>(w.score);                        // dword DGROUP:0x1e58
    ps.captured = world::word_at(s, 0xd2);                        // DGROUP:0x7162
    ps.destroyed = world::word_at(s, 0xd0);                       // DGROUP:0x7160
    ps.credit = static_cast<long>(static_cast<std::uint16_t>(s.wdc)) |
                (static_cast<long>(s.wde) << 16);                 // dword +0xdc DGROUP:0x716c
    ps.resources = world::word_at(s, 0xce);                       // DGROUP:0x715e
    // seg17:0x4068: ship word +0x26 = 100 around seg13:0x33d6(ship), which stores +0xd8, and
    // +0x26 back [BYTES seg17:0x407b-0x4096]. The original runs it when the page is typed; the
    // records do not change during the briefing, so the port runs it at the entry [DESIGN].
    const std::int16_t saved = s.w26;
    s.w26 = 100;
    ps.rating = world::rate_ship(w, p);
    s.w26 = saved;
    ps.to_allies = world::sbyte_at(s, 0x5d);   // DGROUP:0x70ed
    ps.to_enemies = world::sbyte_at(s, 0x5e);  // DGROUP:0x70ee
    ps.sheet = s.sprite_slot;                  // ship word +0x50 (seg14:0xd0a)
    // size: roster byte +0x7a, or object +0x22 when +0xd6 is -1; cell and gap: object +0x22 and
    // +0x24 [BYTES seg17:0x22b9-0x22e3 seg17:0x24ca-0x24e2].
    const world::RosterRecord* r = world::roster_of(w, p);
    ps.size = r != nullptr ? r->size : o.size;
    ps.cell = o.size;
    ps.gap = o.gap;
    st.ship_known = true;
    app::log("briefing: phase 11 ship " + std::to_string(p) + ": rating " + std::to_string(ps.rating) +
             ", credit " + std::to_string(ps.credit) + ", resources " + std::to_string(ps.resources) +
             ", +0x5d " + std::to_string(ps.to_allies) + ", +0x5e " + std::to_string(ps.to_enemies) +
             ", size " + std::to_string(ps.size) + ", cell " + std::to_string(ps.cell) + ", gap " +
             std::to_string(ps.gap) + ", slot " + std::to_string(kSlotShipBase + ps.sheet));
}

Setup make_setup() {
    Setup st;
    st.W = g.area.w;
    st.H = g.area.h;
    if (g.ini) {
        st.quick_intro = g.ini->get_int(settings::kGameSection, "QuickIntro", 0);  // DGROUP:0x8f70
        st.size_level = g.ini->get_int(settings::kGameSection, "Size", 1);         // DGROUP:0x9156
    }
    if (st.size_level < 0 || st.size_level > 2) st.size_level = 1;
    // DGROUP:0x90f8: the DEMO and PLAY command lines are not ported yet (0x9f0, 0x9fa).
    st.command_line = 0;
    st.play_mode = g.play_mode;  // DGROUP:0x9134
    st.directory = g_selection.empty() ? drive_root() : g_selection;
    st.flags.f92e8 = g_handover.f92e8;  // Navigation's Enter (src/nav/enter.h)
    st.flags.f94e = g_handover.f94e;
    st.flags.f950 = g_handover.f950;
    // The icon records DGROUP:0x57b4 + 0x23k of the wave set-up (src/world/setup.h) that ran
    // just before: DGROUP:0x91fe records, word +0x18 of each pool-0 record, the found names.
    const world::World& w = world::current();
    st.icons = w.icons_c;
    for (int k = 0; k < st.icons && k < world::kIconRecords; ++k) {
        st.gone.push_back(w.pools.records(world::kPoolIcons) > k &&
                          w.pools.rec(world::kPoolIcons, k).state != 0);
        st.names.push_back(w.icons[static_cast<std::size_t>(k)].name);
    }
    if (g_canvas) g_canvas->set_icon_cells(icon_cells(st.icons, st.size_level));
    player_stats(st);
    return st;
}

void trace_phase(const shell::GameState& s) {
    if (s.briefing_phase() == g_logged_phase) return;
    g_logged_phase = s.briefing_phase();
    app::log("briefing: phase " + std::to_string(g_logged_phase) + " at " +
             std::to_string(app::now_tick()) + " ms");
}

// ---- seg9:0x5cca: the state-3 entry

void enter_briefing(shell::GameState& s) {
    // seg9:0x8908 frees the previous page block DGROUP:0x948e (seg17:0x306e) and deletes the
    // page clip region DGROUP:0x93ae, and draws nothing; the port's page is a std::string and its
    // clip a value, so nothing is left to do [BYTES seg9:0x8908-0x8933 seg17:0x306e-0x30ae].
    if (g.main) ShowWindow(g.main, SW_SHOW);
    PatBlt(g.gfx->dc(), 0, 0, g.area.w, g.area.h, BLACKNESS);
    // DGROUP:0x95bc: the port has no demo yet, so every game is a player game.
    if (g.main) {
        // SetCursorPos(DGROUP:0x1d48, H): the client's bottom in screen coordinates [DESIGN:
        // the original's full-screen window has its client at the screen origin].
        POINT p{0, g.area.h * (g.gfx ? g.gfx->factor() : 1)};
        ClientToScreen(g.main, &p);
        SetCursorPos(g.start_cursor.x, p.y);
    }
    // seg2:0xba4 at seg9:0x5d14. The port runs seg2:0x1ac8's play-area hook for its client
    // part, then seg2:0xca4 at seg2:0xc52: the clip region DGROUP:0x12 and the play rectangle
    // DGROUP:0x1d30 with the Border and PlayMode insets, (68, 78, 956, 663) at 1024 x 768
    // [BYTES seg2:0xc52 seg2:0xcb8-0xd69 seg9:0x5d14] (M3 s4 fixer, review M9). seg2:0xba4's
    // player-square blackout with seg7:0x11cc / seg7:0x1208 (seg2:0xc08-0xc3d), the blackout of
    // DGROUP:0x1d28 (seg2:0xc55-0xc86, under the PatBlt above) and its PostMessage(WM_PAINT)
    // (seg2:0xc97) are not ported [OPEN].
    if (s.hooks().play_area) s.hooks().play_area(s);
    g.clip = shell::clip_rect(g.area, g.keys.border != 0, g.play_mode != 0);
    // seg9:0x22b4(1) draws nothing here: seg10:0x0's call has set DGROUP:0x92e6 = 1.
    todo_once("seg9:0x22b4(1) graphics and the wave resets of seg9:0x5cca", "seg9:0x5cca");
    // Its sound part runs whatever DGROUP:0x92e6 holds: seg10:0x744c(0x6e) [BYTES seg9:0x2768
    // seg9:0x276a] (M7 fixer).
    if (g_host) g_host->sound(0x6e);
    g_logged_phase = -1;
    const Setup setup = make_setup();
    const int id = g_briefing->enter(s, setup);
    app::log("briefing: wave " + std::to_string(g_briefing->wave()) + ", text #5000 " +
             std::to_string(id) + ", directory " + setup.directory + ", " +
             std::to_string(setup.icons) + " icons, QuickIntro " +
             std::to_string(setup.quick_intro));
    g_handoff = true;
    trace_phase(s);
}

// ---- seg10:0x0 for state 1: the state-1 set-up before the briefing

// seg15:0x6558(DGROUP:0x94ac): 'C' + the INI Drive offset, string 937 and the volume label as
// the Win16 find-first gives it, "Unknown_HD" when there is none: shell/shared_rules.h
// label_text and label_as_find_first, as the dialogs use them [BYTES seg15:0x656b-0x6628].
std::string drive_text() {
    const char letter = drive_root()[0];
    const wchar_t root[] = {static_cast<wchar_t>(letter), L':', L'\\', 0};
    wchar_t label[MAX_PATH + 1] = L"";
    std::string name;
    if (GetVolumeInformationW(root, label, MAX_PATH + 1, nullptr, nullptr, nullptr, nullptr, 0))
        name = narrow(label);
    const std::optional<std::string> found = shell::label_as_find_first(name);
    // The volume-label find-first's line, in the form of the Navigation walk's (M3 s1 fixer).
    app::log(std::string("drive fs seg15:0x6558 findfirst \"") + letter + ":\\*.*\" 0x08 -> " +
             (found ? "\"" + *found + "\"" : std::string("end")));
    return std::string(1, letter) + (g_host ? g_host->lang(937) : ":") + shell::label_text(found);
}

void state1_setup(shell::GameState&) {
    // seg10:0x0's palette scheme 8 (seg10:0x37, seg10:0x3f) and the load run in src/world/hooks.cpp
    // around this part (M3 s4).
    // DGROUP:0x380c: INI ThePlayer (default "", 12 bytes) outside the demo [BYTES seg10:0x8c];
    // the port has no demo yet.
    const std::string player =
        g.ini ? g.ini->get_string(settings::kGameSection, "ThePlayer", "", 12) : std::string();
    const shell::Tick start = app::now_tick();  // GetCurrentTime + 2000 [BYTES seg10:0x91]
    // The greeting seg15:0x6640: seg10:0x744c(0x6e) plays 0x69 and 0x6e, then
    // seg10:0x754a(0x6f, W/2, H/2 - 240, 4) [BYTES seg15:0x6650 seg15:0x666f].
    if (g_host) {
        g_host->sound(0x6e);
        g_host->sound_at(0x6f, g.area.w / 2, g.area.h / 2 - 240, 4);
    }
    if (g.gfx && g_host) {
        PatBlt(g.gfx->dc(), 0, 0, g.area.w, g.area.h, BLACKNESS);
        std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &now);
        for (const GreetingLine& l :
             greeting_lines(*g_host, g.area.w, g.area.h, tm.tm_hour, player, drive_text()))
            g.gfx->font_text(0, 6, l.colour, l.x, l.y, l.text);  // seg10:0x94c4, align 6
    }
    // The load seg9:0x22b4(0) draws LOADING GRAPHICS and the bar and erases both before it
    // returns (DGROUP:0x92e6 = 1 at its end); no screenshot ever shows them, and the port's
    // load is the asset store's, so the port draws neither [BYTES seg9:0x22e5-0x27b3]
    // [ORACLE start-0001] [DESIGN: shell.md "Game-start dialogs" item 13, On screen].
    g_loading_drawn = true;
    if (g.gfx && g.main) {
        g.gfx->present(g.main, true);
        GdiFlush();
    }
    app::log("state 1: greeting drawn at " + std::to_string(start) + " ms" +
             (g.gfx && g.gfx->alphabet() ? "" : " (no ALPHABET)"));
    // The busy wait to start + 2000 ms [BYTES seg10:0xb7-0xc6]: the time passes without
    // dispatching messages, as the original's loop on GetCurrentTime. The wait goes through
    // MsgWaitForMultipleObjectsEx with an empty wake mask, which removes no message but lets
    // the display driver show the surface (Wine flushes window surfaces there) [DESIGN]. Both
    // editions show the greeting: the unregistered oracle's black shots are Wine not refreshing
    // during the CPU-bound wait (shell.md, item 13) [DESIGN].
    while (app::now_tick() - start < 2000)
        MsgWaitForMultipleObjectsEx(0, nullptr, 10, 0, 0);
    // The load's sound part: seg10:0x744c(0x6e) near the end of seg9:0x22b4 stops the
    // greeting's looping bubbles 0x6f [BYTES seg10:0xad seg9:0x2768 seg9:0x276a]. In the
    // original the bubbles run for as long as the disk load takes; on a modern disk and in
    // the oracle that is 20 ms [ORACLE sound-0001 main], so the port plays the stop after
    // the 2000 ms window instead, and the bubbles last the load screen [DESIGN ADR 0009].
    if (g_host) g_host->sound(0x6e);
}

// The wave HUD of the state-1 repaint seg11:0xafe, its scrollbar and seg11:0x2f0 are src/hud's
// (ADR 0013 decisions 1 and 5): the briefing's stand-ins are gone (M3 s2 fixer).

// ---- seg9:0x60bc: state 3 -> 1

void enter_wave(shell::GameState&) {
    g_handoff = false;
    todo_once("seg9:0x699e at the wave start", "seg9:0x699e");
    if (g.main) ShowWindow(g.main, SW_SHOW);
    PatBlt(g.gfx->dc(), 0, 0, g.area.w, g.area.h, BLACKNESS);  // seg9:0x60bc
    // The ships of seg9:0x60bc are src/world's start_wave (with the stars seg8:0x190e, M3 s4) and
    // its HUD repaint src/hud's (M3 s2 fixer); the radar brushes seg13:0x2dcc are not ported
    // [OPEN].
    todo_once("wave start: the radar brushes seg13:0x2dcc", "seg9:0x60bc");
    g_wave_frames = 0;
    app::log("briefing: wave " + std::to_string(g_briefing->wave()) + " starts at " +
             std::to_string(app::now_tick()) + " ms");
}

}  // namespace

void set_selection(const std::string& dos_path) { g_selection = dos_path; }
void set_handover(int demons_gate, int race, int duel) {
    g_handover.f92e8 = demons_gate;
    g_handover.f94e = race;
    g_handover.f950 = duel;
}
void set_wave(int wave) {
    if (g_briefing) g_briefing->set_wave(wave);
}
int wave() { return g_briefing ? g_briefing->wave() : 0; }

}  // namespace is::briefing

namespace is::app {

void install_briefing_hooks(App& app) {
    using namespace is::briefing;
    auto& h = app.game.hooks();
    g_host = std::make_unique<BriefingHost>();
    g_canvas = std::make_unique<GdiCanvas>(*app.gfx);
    g_briefing = std::make_unique<Briefing>(*g_canvas, *g_host);

    h.enter[shell::kBriefing] = [](shell::GameState& s) { enter_briefing(s); };
    h.frame[shell::kBriefing] = [](shell::GameState& s) {
        g_briefing->frame(s);
        trace_phase(s);
    };
    // seg11:0xafe case 3: seg9:0x8934(0) unless the phase is 18; a WM_PAINT replays the
    // briefing from phase 0 [BYTES seg11:0x1696-0x169f seg9:0x8bcb].
    h.repaint[shell::kBriefing] = [](shell::GameState& s) {
        g_briefing->repaint(s);
        trace_phase(s);
    };
    // The state-1 entry hook is both seg10:0x0 (the state-change step, before the briefing)
    // and seg9:0x60bc (the briefing's end).
    h.enter[shell::kWave] = [](shell::GameState& s) {
        if (g_handoff)
            enter_wave(s);
        else
            state1_setup(s);
    };
    // The state-1 frame hook: seg10:0x7b48 alone, which the wave frame (src/world/flight_hooks.cpp)
    // calls at seg7:0xcaf as UpdateHost::sound_frame, with the play-area centre [OPEN seg7:0xcaf].
    // The HUD and its scrollbar are src/hud's repaint and seg11:0x2f0 (M3 s2 fixer, ADR 0013
    // decisions 1 and 5).
    h.frame[shell::kWave] = [](shell::GameState&) {
        audio::frame(g.area.w / 2, g.area.play_h / 2, app::now_tick());
        ++g_wave_frames;
    };
    auto prev_teardown = h.teardown;
    h.teardown = [prev_teardown](shell::GameState& s) {
        if (prev_teardown) prev_teardown(s);
        g_handoff = false;
    };
    // ESC in a player game posts 0x991 (seg11:0x6c54) once DGROUP:0x934e > 2. DGROUP:0x934e
    // comes with the wave (M3); the port counts state-1 frames in its place [OPEN].
    auto prev_route = h.route_input;
    h.route_input = [prev_route](shell::GameState& s, const shell::Input& in) {
        if (s.state() == shell::kWave && in.msg == shell::kWmKeyUp &&
            in.wparam == shell::kVkEscape) {
            if (g_wave_frames > 2) {
                log("state 1: ESC posts 0x991");
                if (!post(kMsgAbort)) log("state 1: the 0x991 post failed");
            }
            return;
        }
        if (prev_route) prev_route(s, in);
    };

    // A test hook for the headless checks: IS_TEST_BRIEFING set to a directory (or "1" for the
    // INI Drive root) requests state 1 from the first state-0 frame after the Calibrate
    // measurement, as Navigation Enter does [DESIGN].
    const char* t = std::getenv("IS_TEST_BRIEFING");
    if (t && *t) {
        if (std::string(t) != "1") g_selection = t;
        auto prev_frame = h.frame[shell::kIntro];
        h.frame[shell::kIntro] = [prev_frame](shell::GameState& s) {
            if (prev_frame) prev_frame(s);
            static bool once = false;
            if (!once && g.calibrated) {
                once = true;
                log("test hook: IS_TEST_BRIEFING requests state 1");
                s.request(shell::kWave);
            }
        };
    }
}

}  // namespace is::app
#endif
