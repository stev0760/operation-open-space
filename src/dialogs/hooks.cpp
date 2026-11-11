// The hook point of src/dialogs (declared in src/app/app.h, called by WinMain after
// install_state_hooks): the game-start session (hooks.h), the intro's GameStart, the
// Navigation window's host and the state hooks this part owns. Win32 only: the host build
// compiles this file empty.
#ifdef _WIN32
#include "dialogs/hooks.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <exception>
#include <set>

#include "app/app.h"
#include "briefing/hooks.h"
#include "dialogs/audio_options.h"
#include "dialogs/dialogs.h"
#include "dialogs/save_game.h"
#include "help/help.h"
#include "formats/bytes.h"
#include "formats/res16.h"
#include "intro/intro.h"
#include "nav/enter.h"
#include "nav/nav.h"
#include "nav/pilot.h"
#include "shell/edition.h"
#include "shell/rng.h"
#include "world/flight_hooks.h"
#include "world/hooks.h"
#include "world/rescale.h"
#include "world/world.h"

namespace is::dialogs {

namespace {

using app::g;

Session g_session;
std::set<std::string> g_logged;
int g_cache = -1;  // ISPACE.DLL DGROUP:0x1ac

void todo_once(const std::string& what, const char* address) {
    if (g_logged.insert(what).second) app::log_todo(what, address);
}

// seg10:0x744c(id): the program's sound wrapper (audio_options.h is::sound).
void sound(int id) { is::sound::play(id); }

// seg10:0x7480(code, 1): the music pause (-2) before and resume (-3) after the dialogs.
void music(int code) { is::sound::music(code, 1); }

std::string lang(int id) {
    try {
        if (g.store)
            if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) return *s;
    } catch (const std::exception& x) {
        app::log("string " + std::to_string(id) + ": " + x.what());
    }
    return {};
}

std::string exe_string(int id) {
    try {
        if (g.store)
            if (auto s = fmt::load_string(g.store->module(assets::kMainProgram),
                                          static_cast<std::uint16_t>(id)))
                return *s;
    } catch (const std::exception& x) {
        app::log("INSPACE.EXE string " + std::to_string(id) + ": " + x.what());
    }
    return {};
}

void save_ini() {
    try {
        if (g.ini) g.ini->save();
    } catch (const std::exception& x) {
        app::log(std::string("INSPACE.INI: ") + x.what());
    }
}

std::string ini_string(const char* key, const char* def, std::size_t n) {
    return g.ini ? g.ini->get_string(settings::kGameSection, key, def, n) : std::string(def);
}
int ini_int(const char* key, int def) {
    return g.ini ? g.ini->get_int(settings::kGameSection, key, def) : def;
}

std::filesystem::path path_of(const std::string& cp1252) { return std::filesystem::path(wide(cp1252)); }

// INI ShipPath resolved as ADR 0004 decision 4: the key's directory when it holds MASTER.SHP,
// else the asset directory's SHIPS\ (a key written for another machine or prefix), else the
// asset directory itself (a flat distribution) [DESIGN].
std::filesystem::path ship_dir() {
    std::error_code ec;
    const std::string s = ini_string("ShipPath", "", 0x6d);
    if (!s.empty()) {
        const auto p = path_of(s);
        if (std::filesystem::exists(p / L"MASTER.SHP", ec)) return p;
    }
    if (std::filesystem::exists(g.asset_dir / L"SHIPS" / L"MASTER.SHP", ec)) return g.asset_dir / L"SHIPS";
    return g.asset_dir;
}

// ISPACE.DLL DGROUP:0x1ac, computed once with CDDrive empty (rules.h availability_cache):
// command 2's result of the edition check and command 10 on the asset directory's #5000 143.
int availability() {
    if (g_cache != -1) return g_cache;
    int command10 = 0;
    try {
        if (g.store) {
            const fmt::NeModule& m = g.store->module(assets::kMainProgram);
            if (const fmt::NeResource* r = m.find(5000, 143)) {
                const std::size_t n = std::min<std::size_t>(50, m.size(*r));
                std::string name;
                command10 = shell::read_name(std::vector<std::uint8_t>(m.data(*r), m.data(*r) + n),
                                             true, &name);
            }
        }
    } catch (const std::exception& x) {
        app::log(std::string("INSPACE.EXE #5000 143: ") + x.what());
    }
    const int command2 = g.edition.record_result == -1 ? 1 : g.edition.record_result;
    g_cache = availability_cache(command2, command10);
    app::log("ship availability cache " + std::to_string(g_cache));
    return g_cache;
}

std::tm local_now() {
    const std::time_t t = std::time(nullptr);
    const std::tm* tm = std::localtime(&t);
    return tm ? *tm : std::tm{};
}

// DGROUP:0x95bc: the intro's in state 0, the session's otherwise.
int player() {
    if (g.game.state() == shell::kIntro)
        if (const intro::Intro* in = intro::Intro::current()) return in->attract() ? 1 : 0;
    return g_session.player;
}

Env make_env(int x4, int x5) {
    Env e;
    e.instance = g.instance;
    e.owner = g.main;
    e.store = g.store.get();
    e.ini = g.ini.get();
    e.local_path = g.local_path;
    e.text_dir = g.asset_dir;
    e.ship_dir = ship_dir();
    e.game_icon = g.icon;
    e.screen_cx = GetSystemMetrics(SM_CXSCREEN);
    e.screen_cy = GetSystemMetrics(SM_CYSCREEN);
    e.x4 = x4;
    e.x5 = x5;
    const std::tm t = local_now();  // command 15's date [BYTES seg10:0x8303 seg10:0x8314]
    e.tm_mday = t.tm_mday;
    e.tm_mon = t.tm_mon;
    e.tm_year = t.tm_year;
    e.availability_cache = availability();
    e.lang = lang;
    e.exe_string = exe_string;
    // The ISPACE.DLL dialogs play through seg9:0x298c (Benefits seg10:0x3af6 ...).
    e.sound = [](int id) { is::sound::play_ispace(id); };
    e.todo = [](const std::string& what, const char* address) { todo_once(what, address); };
    e.ini_changed = save_ini;
    e.open_text = [](const std::filesystem::path& file) {
        // WinExec("NOTEPAD " + file) becomes the system's text editor (ADR 0008 decision 3).
        std::filesystem::path p = file.lexically_normal();
        p.make_preferred();
        auto r = reinterpret_cast<INT_PTR>(
            ShellExecuteW(g.main, L"open", p.c_str(), nullptr, nullptr, SW_SHOW));
        if (r == SE_ERR_NOASSOC) {
            // No editor is registered for the type: NOTEPAD, as the original runs [DESIGN].
            const std::wstring arg = L"\"" + p.wstring() + L"\"";
            r = reinterpret_cast<INT_PTR>(
                ShellExecuteW(g.main, L"open", L"notepad.exe", arg.c_str(), nullptr, SW_SHOW));
        }
        app::log("open " + fmt::path_utf8(p) + (r > 32 ? "" : " failed (" + std::to_string(r) + ")"));
    };
    return e;
}

// seg10:0x7f88: INI Size against DGROUP:0x9156 [BYTES seg10:0x7fa9 seg10:0x7fb1], on the
// program's World with the program's RescaleHost (world/flight_hooks.h program_size_reread,
// world/rescale.h size_reread) (M4 s8 integrate). The session's word follows the World's.
void size_reread() {
    world::program_size_reread();
    g_session.size = world::current().size;
}

void write_log(int cmd, int result) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "dialog: ordinal 10 command %d returned %d", cmd, result);
    app::log(buf);
}

// The intro's side of the game-start flow.
class Flow final : public intro::GameStart {
public:
    int dialog(int cmd) override { return dialogs::dialog(cmd); }
    int order(bool* opened) override {  // seg10:0x80a8
        // The original returns its uninitialised [bp-4] after 20 dialogs; the port returns 0
        // (shell.md "Input") [DESIGN]. It saves and restores the file times of SYSTEM.INI,
        // WIN.INI and IGRAPHIC.DLL around the dialog, which the port never writes [DESIGN].
        *opened = false;
        if (g_session.order_count >= kOrderLimit) return 0;
        ++g_session.order_count;
        *opened = true;
        return dialogs::dialog(kCmdOrder);
    }
    int accept() override { return ini_int("Accept", 0); }
    int restore(int edition, int player) override {  // seg6:0x930a
        if (restore_refused(edition, player)) {
            // String 2325 through seg16:0x68d6 [BYTES seg6:0x9329 seg6:0x9330].
            MessageBoxW(g.main, wide(lang(2325)).c_str(), L"Inner Space", MB_ICONEXCLAMATION);
            return 0;
        }
        // The reader with ADR 0014 decisions 1, 2, 7 and 9 (save_game.h): step 1's id and date,
        // the file, the blocks into the World, nav::records() and the session, DGROUP:0x92e4 = 1
        // in both the session and the World (setup.cpp's new-game reset skips) [BYTES
        // seg6:0x933f seg6:0x98c4] (M4 s3 SAVE).
        return program_restore(player);
    }
    void load_table() override {  // seg10:0x38e0 [BYTES seg10:0x38ff seg10:0x3963]
        try {
            if (!g.store) return;
            g_session.table = g.store->table5600().rows;
            g_session.table_loaded = true;
            app::log("ISPACE.DLL #5600 135 copied to DGROUP:0x3824");
        } catch (const std::exception& x) {
            app::log(std::string("ISPACE.DLL #5600 135: ") + x.what());
        }
    }
    void delete_demo_save() override {
        // seg6:0x92b8 deletes <LocalPath><Games>demogame.sav; the port never deletes the
        // user's files (ADR 0002) [DESIGN].
        todo_once("delete demogame.sav", "seg6:0x92b8");
    }
    void stamp_dates() override {
        // seg10:0x804c(1) = time(), seg10:0x804c(0) = DX tm_year, AX tm_mday | tm_mon << 8
        // [BYTES seg10:0x805d-0x8091].
        const std::tm t = local_now();
        g_session.game_id = static_cast<std::uint32_t>(std::time(nullptr));
        g_session.game_date = (static_cast<std::uint32_t>(t.tm_year & 0xffff) << 16) |
                              static_cast<std::uint32_t>((t.tm_mday & 0xff) | ((t.tm_mon & 0xff) << 8));
    }
    void set_demo_restore(int v) override { g_session.demo_restore = v; }
    int quick_intro() override { return ini_int("QuickIntro", 0); }
    void clear_restored() override { g_session.restored = 0; }
    void clear_wave_if_new() override {
        if (g_session.restored == 0) {
            g_session.wave = 0;
            briefing::set_wave(0);
        }
    }
    std::string drive_text() override {  // seg15:0x6558
        const char letter = static_cast<char>('C' + g_session.drive);
        const wchar_t root[] = {static_cast<wchar_t>(letter), L':', L'\\', 0};
        wchar_t label[MAX_PATH + 1] = L"";
        std::optional<std::string> name;
        if (GetVolumeInformationW(root, label, MAX_PATH + 1, nullptr, nullptr, nullptr, nullptr, 0))
            name = label_as_find_first(narrow(label));
        // The volume-label find-first's line, in the form of the Navigation walk's (M3 s1 fixer).
        app::log(std::string("drive fs seg15:0x6558 findfirst \"") + letter + ":\\*.*\" 0x08 -> " +
            (name ? "\"" + *name + "\"" : std::string("end")));
        return dialogs::drive_text(g_session.drive, lang(937), name);
    }
    void open_navigation() override {
        sync_records_to_nav();  // the one byte store's counters into Navigation's view (M4 s3 SAVE)
        nav::attach(g.main, g.font);
        nav::open(1, g_session.drive);
    }
    void keep_key() override {
        g_session.keep_once = true;
        g.game.post_input({shell::kWmKeyUp, shell::kVkEscape, 0});
    }
    bool keys_held(intro::Tick now) override {
        if (g_session.keep_once) {
            g_session.keep_once = false;
            return false;
        }
        return now < g_session.key_hold;
    }
    void present() override { app::present_frame(); }
};

Flow g_flow;

}  // namespace

// DGROUP:0x9156 has one home, the World's (world.h World::size): the session's copy follows it,
// so a save after Ctrl +/- writes the Size in force [BYTES seg6:0x8f32-0x8f38] (M4 s8 integrate).
Session& session() {
    g_session.size = world::current().size;
    return g_session;
}

int dialog(int cmd) {  // seg10:0x82a2
    music(-2);  // [BYTES seg10:0x82bb]
    int x4 = 0, x5 = 0;
    switch (cmd) {
    case kCmdGameStart:
    case kCmdNewDemo: x5 = x5_for_edition(g.edition.edition); break;
    case kCmdExit:
    case kCmdAbort:  // [BYTES seg10:0x8369 seg10:0x836f]
        x4 = g.game.state();
        x5 = player();
        break;
    case kCmdOrder: {  // [BYTES seg10:0x8380 seg10:0x8300]
        x4 = order_x4(g.edition.invalid, g.edition.day);
        const std::tm t = local_now();
        x5 = date_number(t.tm_year, t.tm_yday);
        break;
    }
    default: break;
    }
    Env env = make_env(x4, x5);
    const int r = ordinal10(env, cmd);
    write_log(cmd, r);
    const shell::Tick now = app::now_tick();  // DGROUP:0x1e50 [BYTES seg10:0x83ef]
    g_session.key_hold = now + 500;     // DGROUP:0x3b94 [BYTES seg10:0x8401]
    size_reread();                      // seg10:0x7f88 [BYTES seg10:0x8409]
    if (cmd == kCmdBenefits) {          // [BYTES seg10:0x841d seg10:0x8429]
        if (r != 0) {
            sound(0x13);
            bool opened = false;
            g_flow.order(&opened);  // seg10:0x80a8
        }
        return r;
    }
    if (r != 0) {  // [BYTES seg10:0x845b-0x8534]
        g.hud_dirty = false;  // DGROUP:0x8f64
        // DGROUP:0x104e, 0x104c and 0x951c belong to the wave (M3).
        g_session.demo_restore = 0;  // DGROUP:0x9516
        const std::string drive = ini_string("Drive", "C", 20);  // [BYTES seg10:0x851f]
        g_session.drive = drive.empty() ? 0 : drive[0] - 'C';  // DGROUP:0x94ac
        if (cmd == kCmdGameStart) {
            g.play_mode = ini_int("PlayMode", 0);       // DGROUP:0x9134
            g_session.pilot = ini_string("ThePlayer", "", 20);  // DGROUP:0x380c [BYTES seg10:0x8490]
            // Restore into a 9-byte buffer [BYTES seg10:0x84ac]
            if (ini_string("Restore", "", 9).find(' ') != std::string::npos) {
                g_flow.stamp_dates();
                save_new_game();  // a new game's granularity is 32 (ADR 0014 decision 1) [DESIGN]
            }
        }
    }
    music(-3);  // [BYTES seg10:0x853d]
    return r;
}

int save_step(int arg) {  // seg10:0x8568
    music(-2);  // [BYTES seg10:0x857f]
    int r = 1;
    if (save_dialog_opens(arg, ini_int("SavePrompt", 1), ini_string("Restore", "", 9))) {
        sound(0x6e);
        sound(0x13);
        Env env = make_env(0, 0);
        r = ordinal10(env, kCmdSaveGame);
        write_log(kCmdSaveGame, r);
        g_session.key_hold = app::now_tick() + 500;  // [BYTES seg10:0x8612 seg10:0x8624]
    }
    if (r != 0) {
        // seg10:0x863d: arguments 0 and -1 call seg6:0x885c(1, 0) directly, 1 ... 3 the
        // roster update seg6:0x99b6, which ends in seg6:0x885c(1, 0); the trial's refusal (2324)
        // is the writer's, after the roster update (verify claims 24, 25) [BYTES seg10:0x8657
        // seg6:0x9e56 seg6:0x8887]. The writer's result is ignored [BYTES seg10:0x865c].
        if (arg == -1 || arg == 0)
            program_save(1, 0, player());
        else
            program_roster_save(player());
        if (save_destroys_window(arg) && g.main) DestroyWindow(g.main);  // seg10:0x864c
    }
    music(-3);  // [BYTES seg10:0x8679]
    return r;
}

// seg10:0x7f18: the menu commands 0x68 and 0xc0 (audio_options.h).
namespace {
std::function<void()> g_rate_settings;
}  // namespace

void set_rate_settings_reader(std::function<void()> reader) { g_rate_settings = std::move(reader); }

void options_command() {
    // DGROUP:0x592 = 2 holds the adaptive thresholds, which the port does not have (ADR 0001
    // decision 7) [BYTES seg10:0x7f22].
    sound(0x6e);  // [BYTES seg10:0x7f2b]
    music(-2);    // [BYTES seg10:0x7f34]
    OptionsEnv env;
    env.instance = g.instance;
    env.owner = g.main;
    env.store = g.store.get();
    env.ini = g.ini.get();
    env.game_icon = g.icon;
    env.lang = lang;
    env.exe_string = exe_string;
    env.module_kind = [](bool voice) {
        try {
            if (g.store) {
                const std::string name = voice ? g.store->fx_module() : std::string(assets::kSoundDll);
                // LoadString(module, 2314) succeeds: ENG_LANG's 2314; fails: no SetWindowText.
                if (fmt::load_string(g.store->sound_module(name), 2314)) return lang(2314);
                return std::string();
            }
        } catch (const std::exception&) {
        }
        return lang(1148);  // the module is not loaded [BYTES ISPACE.DLL seg9:0x1b3d]
    };
    env.ini_changed = save_ini;
    env.reread = [](const audio::Settings& s) { audio::reread_settings(s); };
    env.audio_ok = [](const audio::Settings& s) { audio::audio_ok(s); };  // (M7 s3 fixer)
    env.channels = [] { return audio::channels(); };                     // DGROUP:0x219e
    env.sound = [](int id) { is::sound::play_ispace(id); };
    // seg9:0x3906(DGROUP:0xaa4, 1) plays the track ISPACE.DLL stored while the music was off,
    // outside INSPACE's wrapper [BYTES ISPACE.DLL seg9:0x220a seg9:0x2225] (M7 fixer).
    env.music_restart = [] {
        app::log("Options: music restart seg9:0x3906(DGROUP:0xaa4, 1)");
        audio::music_restart_stored();
    };
    // DGROUP:0x227e as command 0 found it: 3 with a waveOut device, 0 without [DESIGN ADR 0011
    // decision 2] (M7 fixer).
    env.capability = audio::capability();
    // WinHelp(<Path><Language>_HELP.HLP, HELP_CONTEXT, 15): the port's manual window [DESIGN].
    env.help = [] { help::open_manual(15); };
    env.todo = [](const std::string& what, const char* address) { todo_once(what, address); };
    const int r = options_ordinal10(env);  // ordinal 10 command 0 [BYTES seg10:0x7f4e]
    write_log(kCmdOptions, r);
    if (g_rate_settings) g_rate_settings();  // seg6:0xb3e [BYTES seg10:0x7f52]
    size_reread();                           // seg10:0x7f88
    const shell::Tick now = app::now_tick();  // DGROUP:0x1e50 = now [BYTES seg10:0x7f60]
    g_session.key_hold = now + 500;           // DGROUP:0x3b94
    music(-3);  // [BYTES seg10:0x7f7e]
}

void exit_sequence() {  // seg2:0xd78, SC_CLOSE
    app::log_window("at SC_CLOSE");
    // 1: sounds 0x6e and 11 + rand() % 3, a 450 ms busy wait and sound 0x50 [BYTES seg2:0xdb8
    // seg2:0xdc0 seg2:0xdcf seg2:0xdda seg2:0xde6 seg2:0xdff]. The port sleeps [DESIGN].
    sound(0x6e);                               // seg10:0x744c [BYTES seg2:0xdba]
    sound(11 + shell::game_rng().rand() % 3);  // seg10:0x744c [BYTES seg2:0xdcf]
    Sleep(450);
    is::sound::play_at(0x50, 0, 0, 0);  // seg10:0x754a(0x50, 0, 0, 0) [BYTES seg2:0xdff]
    g.game.set_suspended(true);  // 2: DGROUP:0x1a = 1 [BYTES seg2:0xe07]
    const int r = dialog(kCmdExit);
    switch (r) {
    case kExitExit: {  // [BYTES seg2:0xe1c-0xe44]
        g.game.set(shell::kEnding);  // state 9
        if (benefits_on_exit(g.edition.edition)) dialog(kCmdBenefits);
        // seg10:0x87de [BYTES seg10:0x87eb-0x8852]
        const PlusCountStep s =
            plus_count_step(g.edition.plus, g.edition.edition, ini_int("PlusCount", 13));
        if (s.acts) {
            if (s.plus_pack) {
                g.game.set(shell::kEnding);
                dialog(kCmdPlusPack);
            }
            if (g.ini) {
                g.ini->set_int(settings::kGameSection, "PlusCount", s.count);
                save_ini();
            }
        }
        if (g.main) DestroyWindow(g.main);  // seg2:0xe44
        break;
    }
    case kExitSave:  // seg10:0x8568(-1) in state 2, (1) otherwise [BYTES seg2:0xe5d]
        save_step(save_exit_argument(g.game.state()));
        break;
    default:  // Cancel: DGROUP:0x1a cleared again [BYTES seg2:0xe65]
        g.game.set_suspended(false);
        break;
    }
}

void abort_sequence() {  // seg2:0x536
    sound(0x6e);  // [BYTES seg2:0x334 seg2:0x337]
    sound(0x13);
    const int r = dialog(kCmdAbort);  // [BYTES seg2:0x548]
    if (r == kExitSave) save_step(3);  // [BYTES seg2:0x55e]
    if (r != 0) g.game.set_abort(true);  // DGROUP:0x8f6c = 1 [BYTES seg2:0x564]
}

}  // namespace is::dialogs

namespace is::app {

namespace {
// DGROUP:0x90f8: the DEMO and PLAY command lines are not ported yet (shell.md "Start-up"), so 0.
int nav_command_line() { return 0; }
// DGROUP:0x95ba, INI DemoType (default 519).
int nav_demo_type() { return is::dialogs::ini_int("DemoType", 519); }
}  // namespace

void install_dialog_hooks(App& app) {
    using namespace is::dialogs;
    // DGROUP:0x9156 and DGROUP:0x9150 = 4 - Size from the start-up's read, so the Restore before
    // any wave compares the saved Size with the INI's [BYTES seg6:0xaf8-0xb07 seg6:0x95b4]; a value
    // outside 0 ... 2 is refused and logged once, as the load's reread refuses it, and recorded
    // for seg10:0x7f88's compare (world/rescale.h size_from_ini, ruling R2) [DESIGN ADR 0016
    // decision 2] (M4 s8 integrate; M4 s8 fixer, review L3, M3).
    const std::int16_t start_size =
        world::size_from_ini(app.ini ? app.ini->get_int(settings::kGameSection, "Size", 1) : 1, "seg6:0xaf8");
    // The world's guard log (world.h guard_todo) has no sink before install_world_hooks, so the
    // start-up's refusal is logged here (M4 s8 fixer: the L3 log line was lost).
    if (world::size_session().refused)
        todo_once("an INI Size outside 0 ... 2 (the port's tables would be indexed outside their storage): Size 1",
                  "seg6:0xaf8");
    g_session.size = start_size;
    world::current().size = start_size;
    world::current().shift = static_cast<std::int16_t>(4 - start_size);
    intro::set_game_start(&g_flow);
    nav::Host h;
    h.instance = app.instance;
    h.icon = app.icon;
    h.ini = app.ini.get();
    h.ini_changed = save_ini;
    h.lang = lang;
    h.sound = is::dialogs::sound;  // seg10:0x744c (nav: 0x6e, 0x4d)
    h.todo = [](const std::string& what, const char* address) { todo_once(what, address); };
    h.log = [](const std::string& s) { log(s); };
    h.state = [] { return g.game.state(); };
    h.black_play_area = [] {
        if (g.gfx && !g.gfx->surface().empty()) {
            PatBlt(g.gfx->dc(), 0, 0, g.area.w, g.area.h, BLACKNESS);
            present_frame();
        }
    };
    // DGROUP:0x91f8 is the briefing's counter (briefing::wave), which seg9:0x5cca raises at each
    // briefing [BYTES seg9:0x5fbf]: Navigation in state 2 after a played wave reads it, arms the
    // debriefing's deadline and keeps the game (M4 s1 fixer, review H7).
    h.wave = [] { return briefing::wave(); };
    h.restored = [] { return g_session.restored; };
    // seg9:0x6cc4's DGROUP:0x92e4 = 0 [BYTES seg9:0x6ceb] in the game-start flow's copy (M4 s1
    // fixer).
    world::wave_end_links().clear_restored = [] { g_session.restored = 0; };
    // LASTGAME at the wave end, seg6:0x885c(0, 0) [BYTES seg9:0x31f0] [DESIGN ADR 0014] (M4 s3 fixer).
    world::wave_end_links().save_lastgame = [] { program_save_lastgame(); };
    h.play_mode = [] { return g.play_mode; };
    h.player = [] { return player(); };
    h.request_state = [](int s) { request_state(s); };
    h.arm_debriefing = [] {
        g.game.deadlines().debrief_nav = 0;
        g.game.deadlines().debrief_next = now_tick();
    };
    // Enter hands the directory DGROUP:0x38c8 and the wave DGROUP:0x91f8 to the briefing.
    // The wave reached the briefing when the game-start flow wrote it (the restore, the new
    // game's clear) (M4 s1 fixer).
    h.entered = [](const std::string& dir) {
        sync_records_from_nav();  // Enter's record into the one byte store (M4 s3 SAVE)
        briefing::set_selection(dir);
    };
    // seg2:0x190a(5, 0): the main window's command 0xcb runs it (shell::command_table) [DESIGN].
    h.help = [] { post(WM_COMMAND, 0xcb, 0); };
    h.torn_down = [] { return g.torn_down; };
    h.reset_sounds = [] {  // seg10:0x7db8 (M7 s3 fixer)
        audio::reset_slots();
        app::log("sound reset: ISPACE ordinal 7 command 2 (seg10:0x7db8)");
    };
    // M3 s1 fixer: the edition DGROUP:0x920c (shell.md "Editions"), the asset store (FLDR, FILES,
    // GROUP_ICON 105-107) and the DEMO/PLAY command line and DemoType, from one source shared with
    // EnterHost below [BYTES seg6:0x3c43 seg6:0x3c4c].
    h.edition = [] { return g.edition.edition; };
    h.store = [] { return static_cast<const assets::AssetStore*>(g.store.get()); };
    h.command_line = [] { return nav_command_line() != 0; };
    h.demo_type = [] { return nav_demo_type(); };
    if (!nav::install(h)) log("IS_NAV: RegisterClass failed");
    // The race and duel boxes, Enter's checks and hand-over and the demo pilot (src/nav/enter.h).
    nav::EnterHost eh;
    eh.lang = lang;
    eh.ini = app.ini.get();
    eh.ini_changed = save_ini;
    eh.log = [](const std::string& s) { log(s); };
    // DGROUP:0x90f8 and DGROUP:0x8e5e: the DEMO and PLAY command lines are not ported yet
    // (shell.md "Start-up"), so both are 0.
    eh.command_line = [] { return nav_command_line(); };
    eh.demo = [] { return 0; };
    eh.demo_type = [] { return nav_demo_type(); };  // DGROUP:0x95ba
    eh.rand = [] { return shell::game_rng().rand(); };       // seg5:0xdc
    eh.now = [] { return static_cast<std::uint32_t>(now_tick()); };
    eh.tick = [] { return static_cast<std::uint32_t>(g.game.now()); };  // DGROUP:0x1e50
    // Enter's values for the wave set-up: the briefing picks its text by DGROUP:0x92e8, 0x94e
    // and 0x950 (seg9:0x5ee7-0x5f46).
    eh.handover = [](const nav::Handover& v) {
        briefing::set_handover(v.demons_gate, v.race, v.duel);
    };
    nav::install_parts(eh);
    // seg7:0x233: while the Navigation window exists in the demo with PlayMode set, the state-0
    // frame is the demo pilot seg6:0x1e68 instead of the intro [BYTES seg7:0x233 seg7:0x248].
    auto intro_frame = g.game.hooks().frame[shell::kIntro];
    g.game.hooks().frame[shell::kIntro] = [intro_frame](shell::GameState& s) {
        if (nav::window() && g.play_mode != 0 && player() == 0 &&
            nav::pilot_frame(static_cast<std::uint32_t>(s.now())))
            return;
        if (intro_frame) intro_frame(s);
    };
    // seg6:0x1c72(1) from the debriefing's deadline (seg7:0x3ac) and the other callers.
    g.game.hooks().open_navigation = [](shell::GameState&) {
        sync_records_to_nav();  // (M4 s3 SAVE)
        nav::attach(g.main, g.font);
        nav::open(1, g_session.drive);
    };
    // State 5's score list, .ded save and table (save_game.h, M4 s3 SAVE).
    install_high_scores();
}

}  // namespace is::app
#endif
