// The port's sound system (ADR 0011, docs/re/sound.md): the effects engine of ISPACE.DLL
// (ordinals 7, 8 and 9), INSPACE's sound wrappers and the music requests, over one software
// mixer that writes to waveOut. The parts call only this header.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

// Settings lives in its own header so music.h's namespace is::audio::music and the function
// is::audio::music below never meet in one translation unit [DESIGN] (a C++ name cannot be both).
#include "audio/settings.h"

namespace is::assets {
class AssetStore;
}

namespace is::audio {

// Ordinal 7 command 0 with INSPACE's argument DGROUP:0x5a0 (3200, 3200); hwnd_notify is the
// main window (HWND) for the music notifications.
bool init(const Settings&, is::assets::AssetStore&, void* hwnd_notify);
void shutdown();                                                            // command 1
void reset_slots();                                                         // command 2
void focus(bool gained);                                                    // commands 3 and 4
void reread_settings(const Settings&);  // command 10 and the dialogs' reloads
int play(int id);  // INSPACE seg10:0x744c(id): the 0x69 reset then the id
// INSPACE seg10:0x754a(id, x, y, mode): the modes, p/q derivation and the two-step sequences.
int play_at(int id, int x, int y, int mode, std::uint32_t now);
// INSPACE seg10:0x7b48: the per-frame ordinal 8 update and the pending second sound.
void frame(int x, int y, std::uint32_t now);
// INSPACE seg10:0x7480(track, flag): tracks and control codes -1, -2, -3, -5.
void music(int track, int wait);
void on_mci_notify(unsigned wparam, unsigned long lparam);  // MM_MCINOTIFY from the main window
// [DESIGN] test hook: also write the mixed output stream to this WAV file (the IS_AUDIO_DUMP
// environment variable sets it at init).
void dump_to(const std::filesystem::path& wav);

// ---- M7 stage 1 fixer additions to the contract (one writer; listed in the stage report).
// The play area seg10:0x754a and seg10:0x7b48 pass to ordinals 9 and 8: DGROUP:0x90ea (w) and
// DGROUP:0x90ec (the play height) [BYTES seg10:0x7838 seg10:0x783b]. Kept across init.
void set_area(int w, int play_h);
// INSPACE's own MixChannels read DGROUP:0x920a (seg6:0xc9c, default 1), which seg10:0x7b48's
// follow-up rule tests [BYTES seg6:0xc9c seg6:0xcac]. Kept across init.
void set_inspace_mix_channels(int n);
// ISPACE.DLL seg9:0x3906(DGROUP:0xaa4, 1): the stored track, as seg9:0x220a restarts it after
// Options closes, outside INSPACE's wrapper seg10:0x7480 [BYTES ISPACE.DLL seg9:0x220a
// seg9:0x2225].
void music_restart_stored();
// ISPACE.DLL DGROUP:0x227e, the capability level command 0 found (0 before command 0).
int capability();

// ---- M7 stage 3 fixer additions to the contract.
// Audio OK (ISPACE.DLL seg9:0x331a steps 2-4): the settings re-read, the module loads and
// unloads, and the session reopen when the channel count differs from the session's
// DGROUP:0x219e [BYTES ISPACE.DLL seg9:0x34e4 seg9:0x3547 seg9:0x3582 seg9:0x3599].
void audio_ok(const Settings&);
// ISPACE.DLL DGROUP:0x219e, the session's channel count: 0 when command 0 opened no session
// (sound system 0), which Audio init selects as count - 1 [BYTES ISPACE.DLL seg9:0x2fdf].
int channels();

// ---- M4 stage 1 fixer additions to the contract.
// seg10:0x754a's sources in a wave (sound.md "INSPACE wrappers" steps 3 and 4): `source` gives
// the world point of mode 1 (pool a, record b: object words +4, +6) or mode 3 (the dword
// DGROUP:0x5144), false for none; `to_view` is seg8:0x29ea, a world point to the play area;
// `pool_count` is DGROUP:0x9324 (a mode-1 pool above it plays nothing). Empty functions unbind
// (the screens outside a wave play every source from the stand-in point). Kept across init.
void set_world_source(std::function<bool(int mode, int a, int b, int& x, int& y)> source,
                      std::function<void(int& x, int& y)> to_view, int pool_count);

}  // namespace is::audio
