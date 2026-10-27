// The Options/Setup dialog ENG_LANG.DLL DIALOG 100 (ISPACE ordinal 10 command 0, procedure
// ordinal 11, ISPACE.DLL seg9:0x15cc) and the Audio Options dialog DIALOG 999 (ordinal 38,
// ISPACE.DLL seg9:0x2f30) of docs/re/sound.md "Audio Options and Options", with the INSPACE
// side seg10:0x7f18 that opens them (menu 0x68 "Options..", popup 0xc0 "Game Options..").
//
// The first half is portable (no Win32): the list entries per capability level, the enable
// rules, the scroll arithmetic, the INI writes of OK in their order and the sound keys read
// into audio::Settings; the tests check it on the host. The second half (_WIN32) applies it to
// the controls of the templates (AssetStore::dialog_template).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "audio/audio.h"
#include "dialogs/rules.h"

namespace is::settings {
class IniFile;
}

namespace is::dialogs {

// The template ids [BYTES ENG_LANG.DLL DIALOG 100] [BYTES ENG_LANG.DLL DIALOG 999].
constexpr std::uint16_t kDlgOptions = 100;
constexpr std::uint16_t kDlgAudio = 999;

// The capability level ISPACE.DLL DGROUP:0x227e the port reports to its settings code: 3 (left
// and right volume), so every mode is offered (ADR 0011 decisions 2 and 5) [DESIGN].
constexpr int kPortCapability = 3;

// ---- Options #100 control ids [BYTES ENG_LANG.DLL DIALOG 100]
namespace opt {
constexpr int kOk = 1, kCancel = 2, kHelp = 106, kSpeedBar = 107, kNormalSpeed = 110,
              kVolumeBar = 111, kZoom = 114, kSoundSystem = 118, kQuick = 119, kVersion = 122,
              kName = 123, kSpeedLabel = 124, kDetail = 126, kEnhance = 130, kDisplay = 132,
              kRelease = 136, kDifficulty = 137, kAudio = 139, kLanguage = 142, kFxKind = 144,
              kVoiceKind = 146, kJoystick = 147, kBraking = 148, kIcon = 591;
}  // namespace opt

// ---- Audio Options #999 control ids [BYTES ENG_LANG.DLL DIALOG 999]
namespace aud {
constexpr int kOk = 1, kCancel = 2, kMusic = 101, kSoundFx = 102, kVoiceFx = 103, kHelp = 106,
              kMixChannels = 128, kBalanceBar = 1001, kKeep = 1003, kSwap = 1004,
              kStereoEffect = 1008;
}  // namespace aud

// Combo 118's string ids: 2233 "No Sound" always, then 2234, 2235 and 2236 while the level is
// at least 1, 2 and 3; the index is the SoundSystem value [BYTES ISPACE.DLL seg9:0x18ea
// seg9:0x1919 seg9:0x194b seg9:0x1981].
std::vector<int> sound_system_strings(int capability);

// The controls the Options init disables for the level: 111 below 2, 118 and 139 below 1
// [BYTES ISPACE.DLL seg9:0x19b7 seg9:0x19d0].
std::vector<int> options_disabled(int capability);

// The controls the Audio init disables: 1004 and 1008 below 3, 1001 below 2, and 101, 102,
// 103, 128 and 1003 below 1 [BYTES ISPACE.DLL seg9:0x30d6 seg9:0x313c seg9:0x3169].
std::vector<int> audio_disabled(int capability);

// Combo 128's entries "1" to "n": n = 8 with WaveMix present, else 1 [BYTES ISPACE.DLL
// seg9:0x2fa1 seg9:0x2fc8]. The port's mixer stands for WaveMix (ADR 0011 decision 1), so its
// dialog passes true [DESIGN].
std::vector<std::string> mix_channel_entries(bool wavemix);
// MixChannels from the combo index: index + 1 [BYTES ISPACE.DLL seg9:0x3367 seg9:0x3493], so
// a combo with no selection (CB_ERR) writes 0, as the original does after a start at sound
// system 0 [ORACLE sound-0002] (M7 s3 fixer).
int mix_channels_of_index(int index);
// The combo selection for the session's channel count DGROUP:0x219e (not the INI's
// MixChannels): count - 1 [BYTES ISPACE.DLL seg9:0x2fdf] [ORACLE sound-0002], so a count of 0
// (no session: a start at sound system 0) selects nothing (-1) (M7 s3 fixer). A count above
// the list selects its last entry [DESIGN].
int mix_channel_index(int channels, bool wavemix);

// The scroll arithmetic of the two sound bars (Master Volume 111, Effects vs. Music 1001):
// line and page codes 0-3 move by 1, SB_THUMBPOSITION and SB_THUMBTRACK go to `pos`, every
// other code leaves the value; the result is clamped to 0-100 [BYTES ISPACE.DLL seg9:0x1d6f
// seg9:0x1d86 seg9:0x31c1 seg9:0x31e2].
int sound_bar_step(int value, int code, int pos);
// Bar 111 plays the test sound 0x23 on SB_ENDSCROLL (8) [BYTES ISPACE.DLL seg9:0x1d4a].
constexpr int kVolumeTestSound = 0x23;

// The Play Speed bar 107: the position of FrameRateSet (1200 gives 1000; up to 36 gives
// (r - 4) * 500 / 32; above, (r - 36) * 500 / 70 + 500; a negative result gives 0)
// [BYTES ISPACE.DLL seg9:0x1a34-0x1aaa], its rate (timing.md "Rate settings", ISPACE.DLL
// seg9:0x2242), and its scroll step (codes 0-3 by 50, 4 and 5 to pos, 6 to 1000, 7 to 0,
// clamped to 0-1000) [BYTES ISPACE.DLL seg9:0x1d01-0x1d2a].
int speed_position(int frame_rate_set);
int speed_rate(int position);
int speed_bar_step(int value, int code, int pos);
// Label 124: "Play Speed  %s" with "Full Out" for the rate 1200, else "%d frames/sec"
// [BYTES ISPACE.DLL seg9:0x1ea8 DGROUP:0x85d DGROUP:0x866 DGROUP:0x874].
std::string speed_label(int position);

// What Options OK writes, in its order: Complex, Braking, Enhance, SoundVolume, SoundSystem,
// Quick, Detail (3 - index), Size and SetSize (2 - index), FrameRateSet (speed_rate)
// [BYTES ISPACE.DLL seg9:0x20b3 seg9:0x20d8 seg9:0x20fe seg9:0x2124 seg9:0x214a seg9:0x2170
// seg9:0x2196 seg9:0x21bb seg9:0x21d0 seg9:0x21fb].
struct OptionsValues {
    int complex = 2;        // combo 137 index
    int braking = 0;        // check 148
    int enhance = 1;        // check 130
    int sound_volume = 100; // ISPACE.DLL DGROUP:0x69e
    int sound_system = 3;   // combo 118 index
    int quick = 1;          // combo 119 index
    int detail_index = 0;   // combo 126 index
    int zoom_index = 1;     // combo 114 index
    int speed = 500;        // ISPACE.DLL DGROUP:0xcfe
};
std::vector<IniWrite> options_ok_writes(const OptionsValues& v);

// What Audio OK writes, in its order: StereoEffect (combo index), FXMusic, Music, SoundFX,
// VoiceFX, MixChannels (index + 1), PurgeSound (1 - Keep), StereoSwap [BYTES ISPACE.DLL
// seg9:0x33d6 seg9:0x33fc seg9:0x3422 seg9:0x3448 seg9:0x346e seg9:0x3493 seg9:0x34b9
// seg9:0x34df].
struct AudioValues {
    int stereo_effect = 2;
    int fx_music = 50;
    int music = 1, sound_fx = 1, voice_fx = 1;
    int mix_index = 3;
    int keep = 0;  // check 1003: PurgeSound = 1 - keep
    int stereo_swap = 0;
};
std::vector<IniWrite> audio_ok_writes(const AudioValues& v);

// After Options OK or Cancel, ISPACE.DLL seg9:0x220a restarts the stored track with
// seg9:0x3906(DGROUP:0xaa4, 1) when Music was off as the dialog opened and is on now, or (OK
// only) when the sound system went from 0 to non-zero [BYTES ISPACE.DLL seg9:0x220a
// seg9:0x221f seg9:0x222b].
bool options_restarts_music(bool ok, int music_at_open, int music_now, int old_system,
                            int new_system);

// The sound keys of [Inner Space] as ISPACE.DLL seg9:0x22ca reads them, with its defaults
// (sound.md "Settings"): SoundFX 1, VoiceFX 1, Music 1, SoundVolume 100, FXMusic 50,
// StereoEffect 2, PurgeSound 1, StereoSwap 0, SoundSystem -1, MixChannels 4, SaverVolume 0,
// ForceVol 0, AllowSound 0, SndPath "" (0x6d bytes) and Language (5 bytes).
audio::Settings audio_settings(const settings::IniFile& ini);

}  // namespace is::dialogs

#ifdef _WIN32
#include <windows.h>

namespace is::assets {
class AssetStore;
}

namespace is::dialogs {

// Everything the two dialogs read or do outside their controls.
struct OptionsEnv {
    HINSTANCE instance = nullptr;
    HWND owner = nullptr;  // the main window (ordinal 10's hwnd)
    const assets::AssetStore* store = nullptr;
    settings::IniFile* ini = nullptr;
    HICON game_icon = nullptr;
    int capability = kPortCapability;
    bool wavemix = true;  // the port's mixer stands for WAVEMIX.DLL [DESIGN ADR 0011]
    // ENG_LANG.DLL and INSPACE.EXE strings, cp1252; empty when missing.
    std::function<std::string(int)> lang;
    std::function<std::string(int)> exe_string;
    // The kind label of a sound module: ENG_LANG's 2314 "Compressed" when the module holds a
    // string 2314, ENG_LANG's 1148 "NONE" when it is not loaded, and empty (the template's
    // "Normal" stays) when it has no string 2314 [BYTES ISPACE.DLL seg9:0x1b36-0x1bf1]. Null:
    // empty.
    std::function<std::string(bool voice)> module_kind;
    // The INI changed: persist it.
    std::function<void()> ini_changed;
    // ISPACE.DLL seg9:0xa9e and the INI reader seg9:0x22ca: the sound settings in memory changed.
    std::function<void(const audio::Settings&)> reread;
    // Audio OK's steps 2-4 (audio::audio_ok); null: reread (M7 s3 fixer).
    std::function<void(const audio::Settings&)> audio_ok;
    // ISPACE.DLL DGROUP:0x219e, the session's channel count (audio::channels); null: the INI's
    // MixChannels, for a dialog without a sound system (M7 s3 fixer).
    std::function<int()> channels;
    // ISPACE.DLL seg9:0x298c(id): ordinal 9 directly, the test sound.
    std::function<void(int)> sound;
    // ISPACE.DLL seg9:0x3906(DGROUP:0xaa4, 1): the stored track again.
    std::function<void()> music_restart;
    // WinHelp(<Path><Language>_HELP.HLP, HELP_CONTEXT, 15) [BYTES ISPACE.DLL seg9:0x1e55].
    std::function<void()> help;
    std::function<void(const std::string&, const char*)> todo;
    // MessageBox(owner, text, caption, flags); the tests answer it.
    std::function<int(HWND, const std::string&, const std::string&, UINT)> message_box;
};

// The sound settings ISPACE.DLL keeps in memory between the dialogs (SoundVolume and FXMusic
// moved on a bar stay changed after Cancel, sound.md "Options OK and Cancel" item 4 and Audio
// OK item 5). load_live_settings reads them from the INI, as the INI reader does.
audio::Settings& live_settings();
void load_live_settings(const settings::IniFile& ini);

class OptionsDialogBase {
public:
    OptionsDialogBase(OptionsEnv& env, std::uint16_t id) : env_(env), template_(id) {}
    virtual ~OptionsDialogBase();
    OptionsDialogBase(const OptionsDialogBase&) = delete;
    OptionsDialogBase& operator=(const OptionsDialogBase&) = delete;

    int run_modal();
    HWND create_modeless();  // the tests
    HWND hwnd() const { return hwnd_; }
    int result() const { return result_; }
    bool ended() const { return ended_; }
    const std::vector<IniWrite>& writes() const { return writes_; }
    INT_PTR handle(HWND h, UINT msg, WPARAM w, LPARAM l);

protected:
    virtual void init() = 0;
    virtual void command(int id) = 0;
    virtual void hscroll(int id, int code, int pos) = 0;
    void end(int r);
    void write(const IniWrite& w);
    void enable(int id, bool on);
    void add(int id, const std::string& cp1252);
    void select(int id, int index);
    int selection(int id) const;
    void set_text(int id, const std::string& cp1252);
    std::string lang(int id) const { return env_.lang ? env_.lang(id) : std::string(); }

    OptionsEnv& env_;
    std::uint16_t template_;
    HWND hwnd_ = nullptr;
    HFONT normal_ = nullptr, bold_ = nullptr;
    bool modal_ = false;
    bool ended_ = false;
    int result_ = 0;
    std::vector<IniWrite> writes_;
};

// Audio Options #999 (ordinal 38, ISPACE.DLL seg9:0x2f30).
class AudioDialog final : public OptionsDialogBase {
public:
    explicit AudioDialog(OptionsEnv& env) : OptionsDialogBase(env, kDlgAudio) {}

protected:
    void init() override;
    void command(int id) override;
    void hscroll(int id, int code, int pos) override;

private:
    void ok();  // ISPACE.DLL seg9:0x331a
    int fx_music_shown_ = -1;  // ISPACE.DLL DGROUP:0xd04
};

// Options/Setup #100 (ordinal 11, ISPACE.DLL seg9:0x15cc).
class OptionsDialog final : public OptionsDialogBase {
public:
    explicit OptionsDialog(OptionsEnv& env) : OptionsDialogBase(env, kDlgOptions) {}
    ~OptionsDialog() override;
    // ISPACE.DLL DGROUP:0x69c: an Options dialog is open (the already-in-use test).
    static bool open();

protected:
    void init() override;
    void command(int id) override;
    void hscroll(int id, int code, int pos) override;

private:
    void close(bool ok);  // ISPACE.DLL seg9:0x1fae
    void show_speed(int position);  // ISPACE.DLL seg9:0x1ea8
    int music_at_open_ = 1;  // ISPACE.DLL DGROUP:0x2220
    int speed_ = 500;        // ISPACE.DLL DGROUP:0xcfe
    int speed_shown_ = -1;   // DGROUP:0xd00
    int volume_shown_ = -1;  // DGROUP:0xd02
};

// Ordinal 10 command 0 (ISPACE.DLL seg9:0x1546): string 2220 in a box when an Options dialog
// is open (returns 0), else sounds 0x6e and 0x13 through seg9:0x298c and DIALOG 100 [BYTES
// ISPACE.DLL seg9:0x1553 seg9:0x157a seg9:0x158f seg9:0x1596 seg9:0x15ba].
int options_ordinal10(OptionsEnv& env);

// seg10:0x7f18, the menu commands 0x68 and 0xc0 (src/dialogs/hooks.cpp): sound 0x6e, music
// (-2, 1), ordinal 10 command 0, seg6:0xb3e, seg10:0x7f88, DGROUP:0x1e50 = now and
// DGROUP:0x3b94 = now + 500, music (-3, 1) [BYTES seg10:0x7f22 seg10:0x7f2b seg10:0x7f34
// seg10:0x7f4e seg10:0x7f52 seg10:0x7f7e].
void options_command();
// The program's seg6:0xb3e (the rate settings and INSPACE's MixChannels), installed by
// src/app/main.cpp.
void set_rate_settings_reader(std::function<void()> reader);

}  // namespace is::dialogs

// ---- the program's sound calls: the audio call with the original's arguments, then one log
// line per call ("sound 0x<id> ..." as before, now played) for the headless comparisons. `now`
// is the program's GetCurrentTime sample (app::now_tick). Header-only so that every part that
// plays a sound (dialogs, briefing, desk, the intro host) calls the same code without linking
// another part [DESIGN].
#include <cstdio>

#include "app/app.h"

namespace is::sound {

inline void log_call(const char* text) { app::log(text); }

// seg10:0x744c(id): 0x69, then id [BYTES seg10:0x745f seg10:0x7475].
inline int play(int id) {
    const int r = audio::play(id);
    char buf[96];
    std::snprintf(buf, sizeof buf, "sound 0x%x seg10:0x744c -> %d at %lu ms", id, r,
                  static_cast<unsigned long>(app::now_tick()));
    log_call(buf);
    return r;
}

// seg10:0x754a(id, x, y, mode).
inline int play_at(int id, int x, int y, int mode) {
    const std::uint32_t now = app::now_tick();
    const int r = audio::play_at(id, x, y, mode, now);
    char buf[128];
    std::snprintf(buf, sizeof buf, "sound 0x%x seg10:0x754a(%d, %d, %d) -> %d at %lu ms", id, x,
                  y, mode, r, static_cast<unsigned long>(now));
    log_call(buf);
    return r;
}

// ISPACE.DLL seg9:0x298c(id) calls ordinal 9 directly with p = (-6000, -6000), q = 1, r = s = 0
// and the area (-6000, -6000) [BYTES ISPACE.DLL seg9:0x2999-0x29b5]. audio.h has no entry for
// ordinal 9 without INSPACE's wrapper; seg10:0x754a mode 0 passes the same p and q, so the
// port calls that [OPEN ISPACE.DLL seg9:0x298c: r, s and the area differ, and the wrapper's
// DGROUP:0x920c = 3 test and last-played words apply].
inline int play_ispace(int id) {
    const std::uint32_t now = app::now_tick();
    const int r = audio::play_at(id, 0, 0, 0, now);
    char buf[96];
    std::snprintf(buf, sizeof buf, "sound 0x%x ISPACE.DLL seg9:0x298c -> %d at %lu ms", id, r,
                  static_cast<unsigned long>(now));
    log_call(buf);
    return r;
}

// seg10:0x7480(code, flag).
inline void music(int code, int wait = 1) {
    audio::music(code, wait);
    char buf[96];
    std::snprintf(buf, sizeof buf, "music seg10:0x7480(%d, %d) at %lu ms", code, wait,
                  static_cast<unsigned long>(app::now_tick()));
    log_call(buf);
}

}  // namespace is::sound
#endif
