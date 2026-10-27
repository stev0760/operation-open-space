// The music player behind audio.cpp (music_glue.h): one Player per session, driven as
// music_tracks.h describes.
#include "audio/music_glue.h"

#include <memory>

#include "audio/music.h"
#include "audio/music_tracks.h"

namespace is::audio::glue {

namespace {
std::unique_ptr<music::Player> g_player;

music::Config config(const Settings& s, int sound_system) {
    return music::config_from(s.music, sound_system, s.sound_volume, s.fx_music, s.snd_path);
}
}  // namespace

void music_init(const Settings& s, int sound_system, void* hwnd_notify, const char* question) {
    g_player = std::make_unique<music::Player>();
    g_player->configure(config(s, sound_system));
    if (question) music::set_mapper_question(question);
    g_player->set_backend(music::make_mci(hwnd_notify));
}

void music_shutdown() { g_player.reset(); }

void music_request(int code, int flag) {
    if (g_player) g_player->request(code, flag);
}

void music_notify(unsigned wparam) {
    if (g_player) g_player->notify(wparam);
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
