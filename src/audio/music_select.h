// The choice of music backend (ADR 0011 decision 3): [Port] Music = mci | soundfont | opl3 and
// [Port] SoundFont = <path to a .sf2>. Portable except under_wine(), which is false off Windows.
//
// The rule [DESIGN]:
// - an empty value: mci, except under Wine (Wine's MCI sequencer opens a "Midi Through" port
//   with no synth behind it), where it is opl3 when that synth is built in and opens, else
//   soundfont when a SoundFont resolves and opens, else mci.
// - mci: the MCI sequencer, always.
// - soundfont / opl3: the port's MIDI player over that synth; when the synth is not built in or
//   does not open, a log line and mci.
// - an unknown value: logged, then mci.
// - SoundFont resolution: the key's path as written; when the key is empty,
//   Z:\usr\share\soundfonts\default.sf2 under Wine when it exists (Arch's soundfont-fluid),
//   else none.
#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "audio/music.h"

namespace is::audio::music {

enum class Kind { kMci, kSoundFont, kOpl3 };

// "mci", "soundfont", "opl3", ignoring case and surrounding blanks; an empty value is mci and
// known; any other value is mci with *known = false.
Kind parse_kind(std::string_view value, bool* known = nullptr);
const char* kind_name(Kind k);

// ntdll's wine_get_version is exported (GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
// "wine_get_version")).
bool under_wine();

// The Wine default when the key is empty.
inline constexpr std::string_view kWineSoundFont = "Z:\\usr\\share\\soundfonts\\default.sf2";
// `exists` is the file test (std::filesystem::exists by default; the tests pass their own).
std::filesystem::path resolve_soundfont(
    const std::filesystem::path& key, bool wine,
    const std::function<bool(const std::filesystem::path&)>& exists = {});

struct SelectEnv {
    bool wine = false;
    std::function<std::unique_ptr<Backend>()> make_mci;
    // A MIDI player backend over the synth of `kind` (kSoundFont or kOpl3) with that SoundFont,
    // or nullptr when the synth is not built in or does not open.
    std::function<std::unique_ptr<Backend>(Kind kind, const std::filesystem::path& soundfont)> make_synth;
    std::function<bool(const std::filesystem::path&)> exists;  // empty: std::filesystem::exists
    std::function<void(const std::string&)> log;
};

// The session's backend for the [Port] values as written.
std::unique_ptr<Backend> select_backend(const std::string& music_key,
                                        const std::filesystem::path& soundfont_key,
                                        const SelectEnv& env);

}  // namespace is::audio::music
