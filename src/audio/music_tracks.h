// The music of ISPACE.DLL above the backend (docs/re/sound.md "Music", ADR 0011 decision 3):
// the twelve tracks as sections of four MIDI files, the section change between tracks 1 and 2,
// the control codes, the wait-or-notify flag, the MM_MCINOTIFY loops and chaining, the music
// volume and the focus pause. Portable: the Player drives a music::Backend and makes no Win32
// call, so the host tests run it against a fake backend.
//
// How audio.cpp drives it (one Player per session):
//   ordinal 7 command 0      -> player.configure(config_from(...)); player.set_backend(make_mci(hwnd))
//   command 11 (seg10:0x7480's tail) -> player.request(code, flag)
//   MM_MCINOTIFY             -> player.notify(wparam)
//   command 3 / command 4    -> player.focus_lost() / player.focus_gained()
//   ordinal 8 while the sound system is above 1 -> player.refresh_volume()
//   command 10, Audio OK, Options OK -> player.configure(config_from(...)); Options OK also
//                               player.restart_after_options(before, after)
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "audio/music.h"

namespace is::audio::music {

// ---- the tables [BYTES ISPACE.DLL DGROUP:0xaa8 DGROUP:0xac0 DGROUP:0xad8]

constexpr int kTrackCount = 12;
// The files, by the byte of DGROUP:0xad8: 0 main, 1 race, 2 anthem, 3 demon.
constexpr int kFileCount = 4;

struct TrackInfo {
    int file;    // DGROUP:0xad8[t]
    int start;   // DGROUP:0xaa8[t], in 100 ms
    int length;  // DGROUP:0xac0[t], in 100 ms
};
extern const std::array<TrackInfo, kTrackCount> kTracks;

// "main", "race", "anthem", "demon" (ISPACE.DLL DGROUP:0xaef, 0xaf4, 0xaf9, 0xb00); "" for any
// other file number, which seg9:0x39b8 leaves without a name.
std::string_view file_stem(int file);
// SndPath + stem + ".mid" (seg9:0x39a3-0x39d1): a `\` joins them unless SndPath already ends in
// one (seg9:0x162), so an empty SndPath gives "\main.mid" at the root of the current drive.
// A '/' also counts as the separator on the host [DESIGN].
std::filesystem::path midi_path(const std::filesystem::path& snd_path, int file);

// seg9:0x3a94's seek target in ms: 100 * start, less 4500 when track 1 follows track 2 and
// 15000 when track 2 follows track 1 (`previous` is DGROUP:0xaa6) [BYTES ISPACE.DLL
// seg9:0x3ad3 seg9:0x3af9 seg9:0x3b0f].
unsigned seek_target_ms(int track, int previous);
// seg9:0x3b5c's MCI_TO in ms: 100 * (start + length) [BYTES ISPACE.DLL seg9:0x3b86-0x3ba6].
unsigned end_ms(int track);

// ---- the volume

// The music volume Mm (ISPACE.DLL DGROUP:0x2288) of seg9:0xa9e: M when F >= 50, else
// F * 2 * M / 100, 16-bit imul and idiv [BYTES ISPACE.DLL seg9:0xacd-0xae8]. M is SoundVolume,
// or SaverVolume after command 10.
int music_level(int master, int fx_music);
// seg9:0x3bf4 for the -6000 point: v = min(100, 95 * Mm / 100), unsigned 16-bit arithmetic
// [BYTES ISPACE.DLL seg9:0x3c14 seg9:0x3dbb-0x3ddb].
int music_volume(int level);
// The midiOutSetVolume dword: v * 0xFFFF / 100 in both words (0xF332F332 for v = 95)
// [BYTES ISPACE.DLL seg9:0x3ddd-0x3e25].
std::uint32_t volume_dword(int v);

// ---- the state machine

// ISPACE.DLL DGROUP:0x2292 [BYTES ISPACE.DLL seg9:0x3a34 seg9:0x3a88 seg9:0x3ab3 seg9:0x3acd
// seg9:0x3bd6].
enum class State { kClosed = 0, kPlaying = 1, kPaused = 2, kSeeked = 3, kHardPaused = 4 };

// The music side of the settings: `Music` (DGROUP:0x221e), the sound system (DGROUP:0x2280), Mm
// and SndPath.
struct Config {
    int music = 1;
    int sound_system = 0;
    int level = 100;  // Mm
    std::filesystem::path snd_path;
};
// From the INI settings: Mm from `master` (SoundVolume, or SaverVolume after command 10) and
// FXMusic. This header does not take audio.h's Settings: the function is::audio::music of
// audio.h and the namespace is::audio::music cannot meet in one translation unit.
Config config_from(int music, int sound_system, int master, int fx_music,
                   const std::filesystem::path& snd_path);

// The set_volume argument that hands the mapper back its own volume (the original's focus-loss
// write of the saved system value, seg9:0xb2c) [DESIGN]; a backend without such a volume
// ignores it.
constexpr int kSystemVolume = -1;

class Player {
public:
    Player() = default;
    explicit Player(std::unique_ptr<Backend> backend) : backend_(std::move(backend)) {}

    void set_backend(std::unique_ptr<Backend> backend) { backend_ = std::move(backend); }
    Backend* backend() const { return backend_.get(); }
    void configure(const Config& c) { config_ = c; }
    const Config& config() const { return config_; }

    // seg9:0x3906(code, flag), ordinal 7 command 11: a track 0-11 or a control code -1 (hard
    // pause), -2 (pause), -3 (resume), -5 (nothing). `flag` 1 seeks with MCI_WAIT and plays at
    // once, 0 seeks with MCI_NOTIFY and plays at the notify.
    void request(int code, int flag);
    // seg9:0x2ac4's MM_MCINOTIFY branch: MCI_NOTIFY_SUCCESSFUL (1) only.
    void notify(unsigned wparam);
    // The music part of command 3 (seg9:0xaf2) and command 4 (seg9:0xba6); both do nothing while
    // the sound system is 0.
    void focus_lost();
    void focus_gained();
    // seg9:0x38e2 -> seg9:0x3bf4 -> seg9:0x3e46: sends the volume when the sound system is above
    // 1 and the value differs from the last one sent (DGROUP:0xd06, initially 0).
    void refresh_volume();
    // seg9:0x220a's music restart after Options OK: request(current track, 1) when Music was off
    // as the dialog opened and is now on, or the sound system went from 0 to non-zero
    // [BYTES ISPACE.DLL seg9:0x220a seg9:0x2225]. Applies `after` first.
    void restart_after_options(const Config& before, const Config& after);

    State state() const { return state_; }
    int track() const { return track_; }        // DGROUP:0xaa4
    int previous() const { return previous_; }  // DGROUP:0xaa6
    int file() const { return file_; }          // DGROUP:0xaa2
    bool resume_on_focus() const { return resume_; }  // DGROUP:0x229a

private:
    void close();         // seg9:0x3a16
    void open_file();     // seg9:0x37ea
    void pause(int code); // seg9:0x3a48
    void seek(int flag);  // seg9:0x3a94
    void play();          // seg9:0x3b5c

    std::unique_ptr<Backend> backend_;
    Config config_;
    State state_ = State::kClosed;
    int track_ = -1;     // DGROUP:0xaa4, initially -1
    int previous_ = -1;  // DGROUP:0xaa6, initially -1
    int file_ = -1;      // DGROUP:0xaa2, initially -1
    bool resume_ = false;
    std::uint32_t sent_ = 0;  // DGROUP:0xd06
    int sent_v_ = -1;         // the percent behind sent_, for focus_gained
};

// ---- the mci backend (music_mci.cpp; Win32 only, declared here so music.h stays as agreed)

// String 2219 "Midi Mapper is not available. Continue?" (cp1252), from the language module's
// string table [BYTES ENG_LANG.DLL STRING 139] id 2219; audio.cpp sets it at init.
void set_mapper_question(std::string cp1252);

// The test seam of the mci backend: when set, its calls go here instead of to winmm and user32.
// `params` points at the MCI_*_PARMS structure of the call; `ask` stands for the MessageBox and
// returns its IDYES/IDNO answer.
struct MciSeam {
    std::uint32_t (*send)(std::uint32_t device, std::uint32_t msg, std::uint32_t flags,
                          void* params) = nullptr;
    int (*ask)(void* hwnd, const std::string& text) = nullptr;
    bool (*get_volume)(std::uint32_t* value) = nullptr;
    void (*set_volume)(std::uint32_t value) = nullptr;
};
void set_mci_seam(const MciSeam* seam);  // nullptr restores winmm

}  // namespace is::audio::music
