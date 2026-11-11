// The contract (audio.h) over the engine, the mixer, the output and the music player.
#include "audio/audio.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <system_error>
#include <string>

#include "assets/asset_store.h"
#include "audio/engine.h"
#include "audio/mixer.h"
#include "audio/music_glue.h"
#include "audio/wav_source.h"
#include "audio/wave_out.h"

namespace is::audio {

namespace {

std::uint32_t steady_ms() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return static_cast<std::uint32_t>(
        duration_cast<milliseconds>(steady_clock::now() - t0).count());
}

struct System {
    explicit System(assets::AssetStore& store)
        : store(store), out(mixer), source(store), engine(mixer, source, [] { return steady_ms(); }) {}
    assets::AssetStore& store;
    Mixer mixer;
    WaveOut out;
    StoreWaveSource source;
    Engine engine;
    MusicWrapper wrapper;
    // DGROUP:0x1e50 for the calls that carry no time: the last `now` a caller gave, moved on by
    // the steady clock [DESIGN].
    std::int64_t offset = 0;
    std::uint32_t now() const {
        return static_cast<std::uint32_t>(static_cast<std::int64_t>(steady_ms()) + offset);
    }
    void sync(std::uint32_t game_now) {
        offset = static_cast<std::int64_t>(game_now) - static_cast<std::int64_t>(steady_ms());
    }
};

std::unique_ptr<System> g;

// The INSPACE globals the setters keep while no session exists (M7 fixer).
int g_area_w = 640, g_area_h = 480;  // DGROUP:0x90ea, DGROUP:0x90ec until the first set_area
int g_mix_920a = 1;                  // DGROUP:0x920a, default 1 [BYTES seg6:0xc9c]
// The wave's sources (set_world_source), kept across init (M4 s1 fixer).
std::function<bool(int, int, int, int&, int&)> g_source;
std::function<void(int&, int&)> g_to_view;
int g_pool_count = -1;
std::function<bool(WaveView&)> g_view;  // set_wave_view, kept across init (M4 s8)

void apply_world_source(World& w) {
    w.pool_count = g_pool_count;
    w.view = g_view;
    if (g_source) {
        w.source = [](int mode, int a, int b, Point& p) { return g_source(mode, a, b, p.x, p.y); };
    } else {
        w.source = nullptr;
    }
    if (g_to_view) {
        w.to_view = [](Point p) {
            g_to_view(p.x, p.y);
            return p;
        };
    } else {
        w.to_view = nullptr;
    }
}

// ISPACE.DLL seg9:0xba opens SndPath + the module name (with a `\` appended, seg9:0x162)
// [BYTES ISPACE.DLL seg9:0x152 seg9:0x162 seg9:0x1fe]: the asset store's sound directory.
// An empty SndPath, which the original resolves at the root of the current drive, keeps the
// game directory for the modules, the custom WAVs and the MIDI files alike [DESIGN] ADR 0011
// decision 5. A SndPath that becomes empty on a reread goes back to the game directory. The
// result carries the directory the music opens its files in (M7 s3 fixer).
// A SndPath that names a subdirectory which does not exist (a flat distribution directory's
// SOUNDS\) reads from the game directory instead, ADR 0004 decision 4 [DESIGN].
Settings use_snd_path(assets::AssetStore& store, const Settings& in) {
    Settings s = in;
    std::error_code ec;
    if (!s.snd_path.empty() && !std::filesystem::is_directory(s.snd_path, ec)) s.snd_path.clear();
    if (store.sound_dir() != s.snd_path) store.set_sound_dir(s.snd_path);
    if (s.snd_path.empty()) s.snd_path = store.files().dir();
    return s;
}

std::filesystem::path env_path(const char* name) {
#ifdef _WIN32
    std::wstring wname(name, name + std::char_traits<char>::length(name));
    const wchar_t* v = _wgetenv(wname.c_str());
#else
    const char* v = std::getenv(name);
#endif
    return v && *v ? std::filesystem::path(v) : std::filesystem::path();
}

}  // namespace

bool init(const Settings& in, assets::AssetStore& store, void* hwnd_notify) {
    if (g) return true;
    const Settings s = use_snd_path(store, in);
    g = std::make_unique<System>(store);
    const std::filesystem::path dump = env_path("IS_AUDIO_DUMP");
    const bool device = g->out.open();
    // A run with a dump and no device still counts one device, so the engine plays into the
    // dump [DESIGN] test hook.
    int devices = WaveOut::device_count();
    if (devices == 0 && !dump.empty()) devices = 1;
    {
        const std::string line = "inspace: audio: waveOut " +
            std::string(device ? "opened" : "did not open") + ", " + std::to_string(devices) +
            " device(s)" + (dump.empty() ? "" : ", dump on") + "\n";
        std::fputs(line.c_str(), stderr);
        std::fflush(stderr);
    }
    if (!dump.empty()) g->out.dump_to(dump);

    g->engine.on_music_volume = [](int) { glue::music_refresh_volume(); };
    g->engine.on_music_focus = [](bool gained) { glue::music_focus(gained); };
    g->engine.world().mix_channels = g_mix_920a;
    g->engine.world().area_w = g_area_w;
    g->engine.world().area_h = g_area_h;
    apply_world_source(g->engine.world());
    // INSPACE's argument DGROUP:0x5a0 (3200, 3200) [BYTES ISPACE.DLL seg9:0x455] (ispace.md).
    const bool ok = g->engine.command0(s, devices, 3200, 3200);

    const auto question = store.find_string(2219);  // [BYTES ENG_LANG.DLL STRING 139] id 2219
    glue::music_init(s, g->engine.sound_system(), hwnd_notify,
                     question ? question->c_str() : nullptr, &g->mixer, g->out.rate());
    return ok;
}

void shutdown() {
    if (!g) return;
    g->engine.command1();
    glue::music_shutdown();
    g->out.close();
    g.reset();
}

void reset_slots() {
    if (g) g->engine.command2();
}

void focus(bool gained) {
    if (!g) return;
    if (gained)
        g->engine.command4();
    else
        g->engine.command3();
}

namespace {
void reread(const Settings& in, bool audio_ok) {
    if (!g) return;
    const Settings s = use_snd_path(g->store, in);  // seg9:0xba re-reads SndPath at each load
    g->engine.reread(s, audio_ok);
    // A plain reconfigure: the music restart of seg9:0x220a runs once, from Options' close,
    // through music_restart_stored() (M7 fixer).
    glue::music_reconfigure(s, g->engine.sound_system(), false);
}
}  // namespace

void reread_settings(const Settings& s) { reread(s, false); }

void audio_ok(const Settings& s) { reread(s, true); }

int channels() { return g ? g->engine.channels() : 0; }

void set_area(int w, int play_h) {
    g_area_w = w;
    g_area_h = play_h;
    if (!g) return;
    g->engine.world().area_w = w;
    g->engine.world().area_h = play_h;
}

void set_world_source(std::function<bool(int mode, int a, int b, int& x, int& y)> source,
                      std::function<void(int& x, int& y)> to_view, int pool_count) {
    g_source = std::move(source);
    g_to_view = std::move(to_view);
    g_pool_count = pool_count;
    if (g) apply_world_source(g->engine.world());
}

void set_wave_view(std::function<bool(WaveView&)> view) {
    g_view = std::move(view);
    if (g) g->engine.world().view = g_view;
}

bool last_call(int& q, bool& played) {
    if (!g) return false;
    q = g->engine.last_call_q();
    played = g->engine.last_played();
    return true;
}

void set_inspace_mix_channels(int n) {
    g_mix_920a = n;
    if (g) g->engine.world().mix_channels = n;
}

void music_restart_stored() {
    if (g) glue::music_restart_stored();
}

int capability() { return g ? g->engine.capability() : 0; }

int play(int id) {
    if (!g) return 0;
    return g->engine.wrap_744c(id, g->now());
}

int play_at(int id, int x, int y, int mode, std::uint32_t now) {
    if (!g) return 0;
    g->sync(now);
    return g->engine.wrap_754a(id, x, y, mode, now);
}

void frame(int, int, std::uint32_t now) {
    if (!g) return;
    g->sync(now);
    g->engine.wrap_7b48(now);
}

void music(int track, int wait) {
    if (!g) return;
    int code = track;
    if (!g->wrapper.filter(code, g->now())) return;
    glue::music_request(code, wait);
}

void on_mci_notify(unsigned wparam, unsigned long lparam) {
    if (g) glue::music_notify(wparam, lparam);
}

void dump_to(const std::filesystem::path& wav) {
    if (g) g->out.dump_to(wav);
}

}  // namespace is::audio
