// The Options #100 and Audio Options #999 dialogs under Win32 (Wine in CTest): each one created
// modeless from its ENG_LANG.DLL template with its ported procedure, its fills checked against
// sound.md "Audio Options and Options" and OK's INI writes in their order. Skips without the
// original files (IS_ORIGINAL_DIR).
#ifdef _WIN32
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "dialogs/audio_options.h"
#include "dialogs/dialogs.h"
#include "doctest.h"
#include "formats/res16.h"
#include "settings/settings.h"

using namespace is::dialogs;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

struct Rig {
    std::filesystem::path dir = original_dir();
    std::unique_ptr<is::assets::AssetStore> store;
    is::settings::IniFile ini{std::filesystem::temp_directory_path() / "is_audio_options_test.ini"};
    OptionsEnv env;
    std::vector<is::audio::Settings> rereads;
    std::vector<int> sounds;
    int restarts = 0;

    bool ok() const { return !dir.empty(); }
    Rig() {
        if (dir.empty()) return;
        store = std::make_unique<is::assets::AssetStore>(dir);
        env.instance = GetModuleHandleW(nullptr);
        env.store = store.get();
        env.ini = &ini;
        env.lang = [this](int id) {
            auto s = store->find_string(static_cast<std::uint16_t>(id));
            return s ? *s : std::string();
        };
        env.exe_string = [this](int id) {
            auto s = is::fmt::load_string(store->module(is::assets::kMainProgram),
                                          static_cast<std::uint16_t>(id));
            return s ? *s : std::string();
        };
        env.reread = [this](const is::audio::Settings& s) { rereads.push_back(s); };
        env.sound = [this](int id) { sounds.push_back(id); };
        env.music_restart = [this] { ++restarts; };
        const auto set = [&](const char* k, const char* v) {
            ini.set(is::settings::kGameSection, k, v);
        };
        set("MixChannels", "6");
        set("StereoEffect", "1");
        set("FXMusic", "30");
        set("Music", "0");
        set("SoundFX", "1");
        set("VoiceFX", "0");
        set("PurgeSound", "0");
        set("StereoSwap", "1");
        set("SoundVolume", "80");
        set("SoundSystem", "-1");
        set("FrameRateSet", "36");
        set("Detail", "3");
        set("Quick", "2");
        set("Complex", "1");
        set("Size", "1");
        load_live_settings(ini);
    }
};

int count(HWND d, int id) { return static_cast<int>(SendDlgItemMessageW(d, id, CB_GETCOUNT, 0, 0)); }
int sel(HWND d, int id) { return static_cast<int>(SendDlgItemMessageW(d, id, CB_GETCURSEL, 0, 0)); }
std::wstring item(HWND d, int id, int i) {
    wchar_t buf[64] = L"";
    SendDlgItemMessageW(d, id, CB_GETLBTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(buf));
    return buf;
}
std::wstring text(HWND d, int id) {
    wchar_t buf[128] = L"";
    GetDlgItemTextW(d, id, buf, 128);
    return buf;
}

}  // namespace

TEST_CASE("audio options win32: Audio Options #999 fills and OK") {
    Rig r;
    if (!r.ok()) return;
    AudioDialog d(r.env);
    HWND h = d.create_modeless();
    REQUIRE(h != nullptr);
    CHECK(count(h, aud::kMixChannels) == 8);
    CHECK(item(h, aud::kMixChannels, 7) == L"8");
    CHECK(sel(h, aud::kMixChannels) == 5);
    CHECK(count(h, aud::kStereoEffect) == 4);
    CHECK(item(h, aud::kStereoEffect, 0) == L"Minimal");
    CHECK(item(h, aud::kStereoEffect, 3) == L"More");
    CHECK(sel(h, aud::kStereoEffect) == 1);
    CHECK(IsDlgButtonChecked(h, aud::kSoundFx) == BST_CHECKED);
    CHECK(IsDlgButtonChecked(h, aud::kVoiceFx) == BST_UNCHECKED);
    CHECK(IsDlgButtonChecked(h, aud::kMusic) == BST_UNCHECKED);
    CHECK(IsDlgButtonChecked(h, aud::kKeep) == BST_CHECKED);  // PurgeSound 0
    CHECK(IsDlgButtonChecked(h, aud::kSwap) == BST_CHECKED);
    CHECK(GetScrollPos(GetDlgItem(h, aud::kBalanceBar), SB_CTL) == 30);
    for (int id : {aud::kSwap, aud::kStereoEffect, aud::kBalanceBar, aud::kMusic, aud::kMixChannels})
        CHECK(IsWindowEnabled(GetDlgItem(h, id)));  // capability 3

    // The balance bar moves FXMusic in memory and recomputes the volumes.
    SendMessageW(h, WM_HSCROLL, MAKEWPARAM(SB_LINEDOWN, 0),
                 reinterpret_cast<LPARAM>(GetDlgItem(h, aud::kBalanceBar)));
    CHECK(live_settings().fx_music == 31);
    REQUIRE(!r.rereads.empty());
    CHECK(r.rereads.back().fx_music == 31);

    // Mix Channels 3, Music on, OK.
    SendDlgItemMessageW(h, aud::kMixChannels, CB_SETCURSEL, 2, 0);
    CheckDlgButton(h, aud::kMusic, BST_CHECKED);
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(aud::kOk, BN_CLICKED), 0);
    CHECK(d.ended());
    CHECK(d.result() == 1);
    const std::vector<IniWrite> want{{"StereoEffect", "1"}, {"FXMusic", "31"},
                                     {"Music", "1"},        {"SoundFX", "1"},
                                     {"VoiceFX", "0"},      {"MixChannels", "3"},
                                     {"PurgeSound", "0"},   {"StereoSwap", "1"}};
    CHECK(d.writes() == want);
    CHECK(r.ini.get_int(is::settings::kGameSection, "MixChannels", 0) == 3);
    CHECK(r.rereads.back().mix_channels == 3);
    CHECK(r.rereads.back().music == 1);
}

TEST_CASE("audio options win32: Options #100 fills, bars and OK") {
    Rig r;
    if (!r.ok()) return;
    OptionsDialog d(r.env);
    HWND h = d.create_modeless();
    REQUIRE(h != nullptr);
    CHECK(OptionsDialog::open());
    CHECK(count(h, opt::kSoundSystem) == 4);
    CHECK(item(h, opt::kSoundSystem, 0) == L"No Sound");
    CHECK(item(h, opt::kSoundSystem, 3) == L"3D Dynamic Sound");
    CHECK(sel(h, opt::kSoundSystem) == 3);  // -1 resolves to the level
    CHECK(GetScrollPos(GetDlgItem(h, opt::kVolumeBar), SB_CTL) == 80);
    CHECK(GetScrollPos(GetDlgItem(h, opt::kSpeedBar), SB_CTL) == 500);
    CHECK(text(h, opt::kSpeedLabel) == L"Play Speed  36 frames/sec");
    CHECK(count(h, opt::kDetail) == 4);
    CHECK(sel(h, opt::kDetail) == 0);
    CHECK(sel(h, opt::kQuick) == 2);
    CHECK(count(h, opt::kDifficulty) == 5);
    CHECK(sel(h, opt::kDifficulty) == 1);
    CHECK(sel(h, opt::kZoom) == 1);
    CHECK(text(h, opt::kLanguage) == L"English");
    CHECK(text(h, opt::kFxKind) == L"Normal");  // no module_kind hook: the template's text
    CHECK(IsWindowEnabled(GetDlgItem(h, opt::kAudio)));
    CHECK(IsWindowEnabled(GetDlgItem(h, opt::kVolumeBar)));

    // Master Volume: a line step, then SB_ENDSCROLL plays the test sound 0x23.
    SendMessageW(h, WM_HSCROLL, MAKEWPARAM(SB_LINEUP, 0),
                 reinterpret_cast<LPARAM>(GetDlgItem(h, opt::kVolumeBar)));
    SendMessageW(h, WM_HSCROLL, MAKEWPARAM(SB_ENDSCROLL, 0),
                 reinterpret_cast<LPARAM>(GetDlgItem(h, opt::kVolumeBar)));
    CHECK(live_settings().sound_volume == 79);
    CHECK(r.sounds == std::vector<int>{0x23});
    // Play Speed: SB_TOP is Full Out.
    SendMessageW(h, WM_HSCROLL, MAKEWPARAM(SB_TOP, 0),
                 reinterpret_cast<LPARAM>(GetDlgItem(h, opt::kSpeedBar)));
    CHECK(text(h, opt::kSpeedLabel) == L"Play Speed  Full Out");

    SendDlgItemMessageW(h, opt::kSoundSystem, CB_SETCURSEL, 2, 0);
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(opt::kOk, BN_CLICKED), 0);
    CHECK(d.ended());
    const std::vector<IniWrite> want{{"Complex", "1"},      {"Braking", "0"},
                                     {"Enhance", "1"},      {"SoundVolume", "79"},
                                     {"SoundSystem", "2"},  {"Quick", "2"},
                                     {"Detail", "3"},       {"Size", "1"},
                                     {"SetSize", "1"},      {"FrameRateSet", "1200"}};
    CHECK(d.writes() == want);
    CHECK(r.restarts == 0);  // Music stays 0 and the sound system stays non-zero
}
#endif
