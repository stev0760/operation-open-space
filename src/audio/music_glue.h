// The music player behind audio.cpp (music_tracks.h's Player over the backend that
// music_select.h picks). A separate translation unit, because the namespace is::audio::music
// and audio.h's function is::audio::music cannot meet in one [DESIGN].
#pragma once

#include <cstdint>

#include "audio/settings.h"

namespace is::audio {
class Mixer;
}

namespace is::audio::glue {

// `mixer` takes a synth backend's stream, rendered at `rate` (the output's rate).
void music_init(const Settings& s, int sound_system, void* hwnd_notify, const char* question,
                Mixer* mixer, std::uint32_t rate);
void music_shutdown();
void music_request(int code, int flag);
// MM_MCINOTIFY: lParam is the MCI device id for the mci backend and 0 for the port's player.
void music_notify(unsigned wparam, unsigned long lparam);
void music_focus(bool gained);
void music_refresh_volume();
void music_restart_stored();  // seg9:0x3906(DGROUP:0xaa4, 1) (M7 fixer)
void music_reconfigure(const Settings& s, int sound_system, bool options_restart);

}  // namespace is::audio::glue
