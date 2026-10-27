// The sound keys of INSPACE.INI [Inner Space] that the audio contract takes (audio.h). Separate
// from audio.h so that the music code (namespace is::audio::music) can use them without seeing
// the function is::audio::music [DESIGN].
#pragma once

#include <filesystem>
#include <string>

namespace is::audio {

// The sound keys of INSPACE.INI [Inner Space] (docs/re/sound.md "Settings",
// [BYTES ISPACE.DLL seg9:0x22ca]); the caller reads the INI.
struct Settings {
    int sound_fx, voice_fx, music, sound_volume, fx_music, stereo_effect, purge_sound,
        stereo_swap, sound_system, mix_channels, saver_volume, force_vol, allow_sound;
    std::filesystem::path snd_path;
    std::string language;
    // [Port] Music and [Port] SoundFont as written (settings::PortSettings), read at init only
    // (ADR 0011 decision 3) [DESIGN].
    std::string port_music;
    std::filesystem::path port_soundfont;
};

}  // namespace is::audio
