// The music player behind audio.cpp (music_glue.h): one Player per session, driven as
// music_tracks.h describes, over the backend music_select.h picks.
#include "audio/music_glue.h"

#include <cstdio>
#include <memory>
#include <string>

#include "audio/midi_player.h"
#include "audio/mixer.h"
#include "audio/music.h"
#include "audio/music_select.h"
#include "audio/music_tracks.h"
#include "audio/synth.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

namespace is::audio::glue {

namespace {
constexpr std::uint32_t kSynthNotify = 0x80000000u;  // lParam tag of the port player's notifies
std::unique_ptr<music::Player> g_player;
Mixer* g_mixer = nullptr;
// The port's player when a synth backend runs, for the stale-notify check.
std::shared_ptr<music::MidiPlayer> g_midi;

void log(const std::string& s) {
    const std::string line = "inspace: " + s + "\n";
    std::fputs(line.c_str(), stderr);
    std::fflush(stderr);
#ifdef _WIN32
    OutputDebugStringA(line.c_str());
#endif
}

// A MIDI player over the synth of `kind`, its stream in the mixer and its notify posted to the
// main window as MM_MCINOTIFY (wParam MCI_NOTIFY_SUCCESSFUL, lParam kSynthNotify | the notify's
// sequence number), so on_mci_notify takes both backends' notifies [DESIGN]. MCI's own notifies
// carry the device id in lParam, a small number (M7 s2 fixer).
std::unique_ptr<music::Backend> make_synth_backend(music::Kind kind, const std::filesystem::path& sf2,
                                                   void* hwnd, std::uint32_t rate, int initial_volume) {
    std::unique_ptr<music::Synth> synth =
        kind == music::Kind::kOpl3 ? music::make_opl3() : music::make_soundfont();
    if (!synth) {
        log(std::string("music: synth ") + music::kind_name(kind) + " is not built in");
        return nullptr;
    }
    music::SynthConfig cfg;
    cfg.soundfont = sf2;
    cfg.rate = rate;
    if (!synth->open(cfg)) {
        log(std::string("music: synth ") + music::kind_name(kind) + " did not open" +
            (sf2.empty() ? std::string(" (no SoundFont)") : " (" + sf2.string() + ")"));
        return nullptr;
    }
    auto player = std::make_shared<music::MidiPlayer>(
        std::move(synth), [hwnd](unsigned wparam, std::uint32_t seq) {
#ifdef _WIN32
            PostMessageW(static_cast<HWND>(hwnd), MM_MCINOTIFY, wparam,
                         static_cast<LPARAM>(kSynthNotify | seq));
#else
            (void)hwnd;
            (void)wparam;
            (void)seq;
#endif
        });
    player->set_volume(initial_volume);  // M7 s2 fixer: see music_init
    if (g_mixer) g_mixer->set_music(player);
    g_midi = player;
    return music::backend_of(player);
}

music::Config config(const Settings& s, int sound_system) {
    return music::config_from(s.music, sound_system, s.sound_volume, s.fx_music, s.snd_path);
}
}  // namespace

void music_init(const Settings& s, int sound_system, void* hwnd_notify, const char* question,
                Mixer* mixer, std::uint32_t rate) {
    g_mixer = mixer;
    g_player = std::make_unique<music::Player>();
    g_player->configure(config(s, sound_system));
    if (question) music::set_mapper_question(question);
    music::SelectEnv env;
    env.wine = music::under_wine();
    env.make_mci = [hwnd_notify] { return music::make_mci(hwnd_notify); };
    // The synth starts at the session's music volume when Player::refresh_volume would send
    // one: its cache DGROUP:0xd06 starts at 0, so a volume of 0 at start is never sent, and the
    // mapper then keeps its own volume, where the port's synth has none to keep and would play
    // at 100 %. Below sound system 2 nothing is ever sent and the synth stays at 100 %
    // [DESIGN] (M7 s2 fixer).
    const int v0 = sound_system > 1 ? music::music_volume(g_player->config().level) : 100;
    env.make_synth = [hwnd_notify, rate, v0](music::Kind k, const std::filesystem::path& sf2) {
        return make_synth_backend(k, sf2, hwnd_notify, rate, v0);
    };
    env.log = log;
    g_player->set_backend(music::select_backend(s.port_music, s.port_soundfont, env));
}

void music_shutdown() {
    g_player.reset();
    if (g_mixer) g_mixer->set_music(nullptr);
    g_midi.reset();
    g_mixer = nullptr;
}

void music_request(int code, int flag) {
    if (g_player) g_player->request(code, flag);
}

void music_notify(unsigned wparam, unsigned long lparam) {
    if (!g_player) return;
    // The port's player posts kSynthNotify | its sequence number; one a later command
    // superseded is dropped, as MCI would have sent it as superseded or aborted (midi_player.h)
    // [DESIGN] (M7 s2 fixer).
    if (lparam & kSynthNotify) {
        if (!g_midi || !g_midi->accept_notify(static_cast<std::uint32_t>(lparam & ~kSynthNotify)))
            return;
    }
    g_player->notify(wparam);
}

void music_focus(bool gained) {
    if (!g_player) return;
    if (gained)
        g_player->focus_gained();
    else
        g_player->focus_lost();
}

void music_refresh_volume() {
    if (g_player) g_player->refresh_volume();
}

// seg9:0x220a's restart: seg9:0x3906(DGROUP:0xaa4, 1) [BYTES ISPACE.DLL seg9:0x220a
// seg9:0x2225] (M7 fixer).
void music_restart_stored() {
    if (g_player) g_player->request(g_player->track(), 1);
}

void music_reconfigure(const Settings& s, int sound_system, bool options_restart) {
    if (!g_player) return;
    const music::Config after = config(s, sound_system);
    if (options_restart)
        g_player->restart_after_options(g_player->config(), after);
    else
        g_player->configure(after);
}

}  // namespace is::audio::glue
