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
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "app/app.h"
#include "audio/audio.h"
#include "dialogs/audio_options.h"  // is::sound (M7 audio rewiring)
#include "shell/geometry.h"
#include "briefing/briefing.h"
#include "briefing/gdi_canvas.h"
#include "formats/ne.h"

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
bool g_handoff = false;         // state 3 is live: the next state-1 entry is seg9:0x60bc
bool g_loading_drawn = false;   // DGROUP:0x92e6 [BYTES seg10:0xa9 seg9:0x27b3]
unsigned long g_wave_frames = 0;  // state-1 frames since seg9:0x60bc
int g_logged_phase = -1;

void todo_once(const std::string& what, const char* address) {
    if (g_host) g_host->todo(what, address);
}

std::wstring widen(const std::string& s) { return app::widen(s); }
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

// The icon files of the directory: *.exe, then *.dll, files only (FINDFIRST attribute 0x01),
// as newgame-0002 records [ORACLE newgame-0002]. The wave set-up's icon records and cells are
// M3 (seg9:0x17d, seg9:0x256): the port lists the files for the count DGROUP:0x91fe and draws
// each file's first icon into its cell of DGROUP:0x3a02 [DESIGN].
constexpr int kMaxIcons = 100;  // [DESIGN] bounds the cell image; the original's limit is [OPEN]
std::vector<std::wstring> icon_files(const std::string& dir) {
    std::vector<std::wstring> out;
    std::wstring base = widen(dir);
    if (!base.empty() && base.back() != L'\\' && base.back() != L'/') base += L'\\';
    for (const wchar_t* pat : {L"*.exe", L"*.dll"}) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((base + pat).c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes &
                (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
                continue;
            if (static_cast<int>(out.size()) < kMaxIcons) out.push_back(base + fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return out;
}

gfx::ImagePtr icon_cells(const std::vector<std::wstring>& files, int b) {
    const int o = (6 << b) >> 1, s = (32 << b) >> 1;
    if (files.empty() || !g.gfx) return {};
    gfx::ImagePtr img;
    try {
        img = g.gfx->create_image((s + o) * static_cast<int>(files.size()) + o, s + 2 * o);
    } catch (const std::exception& x) {
        app::log(std::string("icon cells: ") + x.what());
        return {};
    }
    for (std::size_t k = 0; k < files.size(); ++k) {
        HICON ic = ExtractIconW(g.instance, files[k].c_str(), 0);
        if (reinterpret_cast<UINT_PTR>(ic) > 1) {
            DrawIconEx(img->hdc(), (s + o) * static_cast<int>(k) + o, o, ic, s, s, 0, nullptr,
                       DI_NORMAL);
            DestroyIcon(ic);
        }
    }
    GdiFlush();
    return img;
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
    const auto files = icon_files(st.directory);
    st.icons = static_cast<int>(files.size());
    st.gone.assign(files.size(), false);
    // The labels of seg17:0xc2e: the record's name, here the file name as FindFirstFile spells
    // it [OPEN: the record DGROUP:0x57b4 + 0x23k is the wave set-up's, M3].
    for (const auto& f : files) {
        const auto slash = f.find_last_of(L"\\/");
        st.names.push_back(narrow(slash == std::wstring::npos ? f : f.substr(slash + 1)));
    }
    if (g_canvas) g_canvas->set_icon_cells(icon_cells(files, st.size_level));
    todo_once("icon records and cells of the wave set-up", "seg9:0x17d");
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
    todo_once("seg9:0x8908 at the briefing entry", "seg9:0x8908");
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
    if (s.hooks().play_area) s.hooks().play_area(s);  // seg2:0xba4
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
    return std::string(1, letter) + (g_host ? g_host->lang(937) : ":") +
           shell::label_text(shell::label_as_find_first(name));
}

void state1_setup(shell::GameState&) {
    todo_once("state-1 set-up: palette scheme 8 and the wave's resources", "seg10:0x0");
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
    // The load's sound part: seg10:0x744c(0x6e) near its end stops the greeting's 0x6f
    // [BYTES seg10:0xad seg9:0x2768 seg9:0x276a] [ORACLE sound-0001 main: two stops 20 ms
    // after 0x6f] (M7 fixer).
    if (g_host) g_host->sound(0x6e);
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
}

// ---- the wave HUD of the state-1 repaint seg11:0xafe (shell.md "Window geometry", HUD)

HWND g_hud_scroll = nullptr;  // DGROUP:0xa7a

// seg11:0x2f0: the HUD child windows destroyed and zeroed [BYTES seg11:0x2f0].
void close_hud() {
    if (g_hud_scroll && IsWindow(g_hud_scroll)) DestroyWindow(g_hud_scroll);
    g_hud_scroll = nullptr;
}

// seg11:0xafe's HUD layout before its state dispatch, as far as it is documented: the PATCOPY
// fill of (0, DGROUP:0x8e5a, W, 85) with DGROUP:0x8e5a = H - 85, and the scrollbar (class
// "scrollbar", style 0x50000001, id 200) at (W/2 + 50, DGROUP:0x8e5a, 14, 85) while DGROUP:0x94e
// is 0 (a player game) [BYTES seg11:0xca2 seg11:0xcb1 seg11:0xcbe seg11:0xcc2 seg11:0xcd9
// seg11:0xd28 seg2:0x1b29]. The fill's brush is [OPEN]: the oracle's band is black. The IS_INFO
// panels 210 and 220 (DGROUP:0x93ba, DGROUP:0x5024) and what the panels draw are [OPEN] (shell.md
// "Open questions", the HUD drawing seg11:0xafe-0x15b0 and seg11:0x1774); the port leaves them
// out until M3 [OPEN]. Case 1 of the dispatch then draws the wave (seg7:0x11cc ...), M3.
void lay_out_hud() {
    if (!g.gfx || g.area.w <= 0 || g.area.h <= 0) return;
    const shell::HudLayout l = shell::hud_layout(g.area, {});
    PatBlt(g.gfx->dc(), l.fill.x, l.fill.y, l.fill.cx, l.fill.cy, BLACKNESS);
    if (g_hud_scroll && IsWindow(g_hud_scroll)) return;
    const int f = g.gfx->factor();
    g_hud_scroll = CreateWindowExW(0, L"scrollbar", nullptr, 0x50000001, l.scrollbar.x * f,
                                   l.scrollbar.y * f, l.scrollbar.cx * f, l.scrollbar.cy * f, g.main,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(200)),
                                   GetModuleHandleW(nullptr), nullptr);
    if (!g_hud_scroll) return;
    // The range 100-500 (shell.md "Open questions"); the start of DGROUP:0xa6c is [OPEN]: the
    // oracle's thumb sits at the bottom, 600 - 100 [OPEN seg11:0xafe].
    SetScrollRange(g_hud_scroll, SB_CTL, 100, 500, FALSE);
    SetScrollPos(g_hud_scroll, SB_CTL, 600 - 100, TRUE);
}

// ---- seg9:0x60bc: state 3 -> 1

void enter_wave(shell::GameState&) {
    g_handoff = false;
    todo_once("seg9:0x699e at the wave start", "seg9:0x699e");
    if (g.main) ShowWindow(g.main, SW_SHOW);
    PatBlt(g.gfx->dc(), 0, 0, g.area.w, g.area.h, BLACKNESS);  // seg9:0x60bc
    // DGROUP:0xa76 = 2, DGROUP:0x9e49 = 10, DGROUP:0x93a4 = 0 and the wave set-up are M3.
    todo_once("wave set-up (seg8:0x190e, seg13:0x2dcc, seg5:0xdc ...)", "seg9:0x60bc");
    g_wave_frames = 0;
    app::log("briefing: wave " + std::to_string(g_briefing->wave()) + " starts at " +
             std::to_string(app::now_tick()) + " ms");
}

}  // namespace

void set_selection(const std::string& dos_path) { g_selection = dos_path; }
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
    // State 1 before M3: a black play area under the HUD.
    // The first frame lays out the HUD as the state-1 repaint does; the wave itself is M3.
    h.frame[shell::kWave] = [](shell::GameState&) {
        // seg10:0x7b48 once per wave frame from seg7:0xcaf: the pending second sound and the
        // ordinal 8 update. The shell has no wave frame before M3; the port calls it from the
        // state-1 frame with the play-area centre [OPEN M3 seg7:0xcaf].
        audio::frame(g.area.w / 2, g.area.play_h / 2, app::now_tick());
        if (g_wave_frames++ == 0) {
            lay_out_hud();
            todo_once("wave frame and the HUD panels (IS_INFO 210, 220)", "seg11:0xafe");
        }
    };
    // The repaint: the black play area of the port before M3 (the wave of case 1 is M3), then
    // the HUD [BYTES seg11:0x15b3 seg11:0x15b9].
    h.repaint[shell::kWave] = [](shell::GameState&) {
        if (g.gfx && !g.gfx->surface().empty()) PatBlt(g.gfx->dc(), 0, 0, g.area.w, g.area.h, BLACKNESS);
        lay_out_hud();
    };
    auto prev_teardown = h.teardown;
    h.teardown = [prev_teardown](shell::GameState& s) {
        if (prev_teardown) prev_teardown(s);
        g_handoff = false;
        close_hud();
    };
    // Every other state's entry closes the HUD, as seg11:0x2f0's callers do on the state changes
    // out of the wave (shell.md "Closing the HUD") [BYTES seg11:0x2f0].
    for (int st = 0; st < shell::kStateCount; ++st) {
        if (st == shell::kWave) continue;
        auto prev = h.enter[static_cast<std::size_t>(st)];
        h.enter[static_cast<std::size_t>(st)] = [prev](shell::GameState& s) {
            close_hud();
            if (prev) prev(s);
        };
    }
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
