// The portable rules of the Options and Audio Options dialogs (src/dialogs/audio_options.h,
// docs/re/sound.md "Audio Options and Options", timing.md "Rate settings").
#include <string>
#include <vector>

#include "dialogs/audio_options.h"
#include "doctest.h"
#include "settings/settings.h"

using namespace is::dialogs;

TEST_CASE("audio options: the sound-system list per capability level") {
    CHECK(sound_system_strings(0) == std::vector<int>{2233});
    CHECK(sound_system_strings(1) == std::vector<int>{2233, 2234});
    CHECK(sound_system_strings(2) == std::vector<int>{2233, 2234, 2235});
    CHECK(sound_system_strings(3) == std::vector<int>{2233, 2234, 2235, 2236});
    CHECK(sound_system_strings(kPortCapability).size() == 4);  // ADR 0011: every mode
}

TEST_CASE("audio options: the enable rules") {
    CHECK(options_disabled(3).empty());
    CHECK(options_disabled(2).empty());
    CHECK(options_disabled(1) == std::vector<int>{111});
    CHECK(options_disabled(0) == std::vector<int>{111, 118, 139});
    CHECK(audio_disabled(3).empty());
    CHECK(audio_disabled(2) == std::vector<int>{1004, 1008});
    CHECK(audio_disabled(1) == std::vector<int>{1004, 1008, 1001});
    CHECK(audio_disabled(0) == std::vector<int>{1004, 1008, 1001, 101, 102, 103, 128, 1003});
}

TEST_CASE("audio options: MixChannels 1-8") {
    const auto e = mix_channel_entries(true);
    REQUIRE(e.size() == 8);
    CHECK(e.front() == "1");
    CHECK(e.back() == "8");
    CHECK(mix_channel_entries(false) == std::vector<std::string>{"1"});
    for (int i = 0; i < 8; ++i) CHECK(mix_channels_of_index(i) == i + 1);
    CHECK(mix_channels_of_index(-1) == 0);  // CB_ERR writes 0 [ORACLE sound-0002]
    CHECK(mix_channels_of_index(12) == 8);
    CHECK(mix_channel_index(4, true) == 3);
    CHECK(mix_channel_index(0, true) == -1);  // no session: no selection [ORACLE sound-0002]
    CHECK(mix_channel_index(20, true) == 7);
    CHECK(mix_channel_index(4, false) == 0);
}

TEST_CASE("audio options: the sound bars and the Play Speed bar") {
    CHECK(sound_bar_step(50, 0, 0) == 49);
    CHECK(sound_bar_step(50, 2, 0) == 49);
    CHECK(sound_bar_step(50, 1, 0) == 51);
    CHECK(sound_bar_step(50, 3, 0) == 51);
    CHECK(sound_bar_step(50, 4, 77) == 77);
    CHECK(sound_bar_step(50, 5, 120) == 100);
    CHECK(sound_bar_step(0, 0, 0) == 0);
    CHECK(sound_bar_step(50, 6, 0) == 50);  // SB_TOP: nothing
    CHECK(sound_bar_step(50, 8, 0) == 50);  // SB_ENDSCROLL
    CHECK(speed_position(1200) == 1000);
    CHECK(speed_position(36) == 500);
    CHECK(speed_position(4) == 0);
    CHECK(speed_position(1) == 0);  // negative clamps to 0
    CHECK(speed_position(106) == 1000);
    CHECK(speed_rate(500) == 36);
    CHECK(speed_rate(0) == 4);
    CHECK(speed_rate(1000) == 1200);
    CHECK(speed_rate(999) == 1200);  // above 104
    CHECK(speed_rate(750) == 71);
    CHECK(speed_rate(speed_position(36)) == 36);
    CHECK(speed_bar_step(500, 0, 0) == 450);
    CHECK(speed_bar_step(500, 3, 0) == 550);
    CHECK(speed_bar_step(500, 6, 0) == 1000);
    CHECK(speed_bar_step(500, 7, 0) == 0);
    CHECK(speed_bar_step(980, 1, 0) == 1000);
    CHECK(speed_label(500) == "Play Speed  36 frames/sec");
    CHECK(speed_label(1000) == "Play Speed  Full Out");
}

TEST_CASE("audio options: Options OK writes in the original's order") {
    OptionsValues v;
    v.complex = 4, v.braking = 1, v.enhance = 0, v.sound_volume = 73, v.sound_system = 2;
    v.quick = 0, v.detail_index = 1, v.zoom_index = 0, v.speed = 500;
    const std::vector<IniWrite> want{{"Complex", "4"},     {"Braking", "1"}, {"Enhance", "0"},
                                     {"SoundVolume", "73"}, {"SoundSystem", "2"},
                                     {"Quick", "0"},       {"Detail", "2"},  {"Size", "2"},
                                     {"SetSize", "2"},     {"FrameRateSet", "36"}};
    CHECK(options_ok_writes(v) == want);
}

TEST_CASE("audio options: Audio OK writes in the original's order") {
    AudioValues v;
    v.stereo_effect = 3, v.fx_music = 40, v.music = 0, v.sound_fx = 1, v.voice_fx = 0;
    v.mix_index = 5, v.keep = 1, v.stereo_swap = 1;
    const std::vector<IniWrite> want{{"StereoEffect", "3"}, {"FXMusic", "40"},
                                     {"Music", "0"},        {"SoundFX", "1"},
                                     {"VoiceFX", "0"},      {"MixChannels", "6"},
                                     {"PurgeSound", "0"},   {"StereoSwap", "1"}};
    CHECK(audio_ok_writes(v) == want);
}

TEST_CASE("audio options: the music restart after Options") {
    CHECK(options_restarts_music(true, 0, 1, 3, 3));
    CHECK(options_restarts_music(false, 0, 1, 3, 3));
    CHECK_FALSE(options_restarts_music(true, 1, 1, 3, 3));
    CHECK(options_restarts_music(true, 1, 1, 0, 2));
    CHECK_FALSE(options_restarts_music(false, 1, 1, 0, 2));
    CHECK_FALSE(options_restarts_music(true, 0, 0, 3, 3));
}

TEST_CASE("audio options: the sound keys with ISPACE.DLL's defaults") {
    is::settings::IniFile ini{"unused.ini"};
    auto s = audio_settings(ini);
    CHECK(s.sound_fx == 1);
    CHECK(s.voice_fx == 1);
    CHECK(s.music == 1);
    CHECK(s.sound_volume == 100);
    CHECK(s.fx_music == 50);
    CHECK(s.stereo_effect == 2);
    CHECK(s.purge_sound == 1);
    CHECK(s.stereo_swap == 0);
    CHECK(s.sound_system == -1);
    CHECK(s.mix_channels == 4);
    CHECK(s.saver_volume == 0);
    ini.set(is::settings::kGameSection, "MixChannels", "6");
    ini.set(is::settings::kGameSection, "SndPath", "C:\\INSPACE\\SOUNDS\\");
    ini.set(is::settings::kGameSection, "Language", "ENGLISH");
    s = audio_settings(ini);
    CHECK(s.mix_channels == 6);
    CHECK(s.snd_path == std::filesystem::path("C:\\INSPACE\\SOUNDS\\"));
    CHECK(s.language == "ENGL");  // 5 bytes with the NUL
    // A cp1252 byte that is not UTF-8 decodes instead of throwing (M7 s2 fixer).
    ini.set(is::settings::kGameSection, "SndPath", "C:\\B\xf6se\\");
    s = audio_settings(ini);
    CHECK(s.snd_path == std::filesystem::path(u"C:\\B\u00f6se\\"));
}
