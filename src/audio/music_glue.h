// The music player behind audio.cpp (music_tracks.h's Player and the mci backend). A separate
// translation unit, because the namespace is::audio::music and audio.h's function
// is::audio::music cannot meet in one [DESIGN].
#pragma once

#include "audio/settings.h"

namespace is::audio::glue {

void music_init(const Settings& s, int sound_system, void* hwnd_notify, const char* question);
void music_shutdown();
void music_request(int code, int flag);
void music_notify(unsigned wparam);
void music_focus(bool gained);
void music_refresh_volume();
void music_restart_stored();  // seg9:0x3906(DGROUP:0xaa4, 1) (M7 fixer)
void music_reconfigure(const Settings& s, int sound_system, bool options_restart);

}  // namespace is::audio::glue
