// The soundfont synth of ADR 0011 decision 3: TinySoundFont (third_party/tsf, MIT) behind the
// port's MIDI player. make_soundfont() is declared in audio/synth.h; this header carries the
// constants the tests and the player share. Portable: no Win32 calls.
#pragma once

namespace is::audio::music {

// The global gain handed to tsf_set_output, in dB. [DESIGN] TinySoundFont renders a forte
// note near full scale on its own, and the four ship-disk files sound up to 256 voices at once
// on all sixteen channels (channels 1-10 and 13-16 double each other, the dual-mode layout of
// docs/re/sound.md "Music"). Played end to end through GeneralUser GS 2.0.3 at 22,050 Hz and
// 0 dB, their sample peaks are MAIN +15.5, RACE +9.7, ANTHEM +20.7 and DEMON +13.7 dBFS (RMS
// -3.4, -6.1, -7.5 and -8.7 dBFS). -22 dB puts the loudest peak (ANTHEM at 174 s) at -1.3 dBFS,
// so the full mix never clips, and leaves the RMS of the mix between -25 and -31 dBFS. The
// player now plays channels 1-10 of these files only (midi_file.h dual_mode_skipped, M7 s2b
// fixer): at -22 dB their peaks are MAIN -10.7, RACE -17.3, ANTHEM -7.9 and DEMON -14.3 dBFS
// (RMS -31.0, -34.2, -34.3 and -35.6 dBFS), so 6 dB more fits: the gain is -16 dB, which puts
// ANTHEM's peak at -1.9 dBFS. The user judged the -22 dB rendering good but quiet next to the
// opl3 one, and the opl3 synth stays at unity (M7 s2b). tests/test_music_soundfont.cpp
// re-measures the peaks.
inline constexpr float kSoundfontGainDb = -16.0f;

// Voices preallocated at open(), so no note-on allocates on the output thread [DESIGN]. The
// output thread still allocates at a reset (each seek, loop, open and close): tsf_reset frees
// the channel table and the first message to each channel allocates it again, and a replaced
// MidiFile is freed there; neither matters at desktop block sizes (M7 s2 fixer). When every
// voice sounds, TinySoundFont steals the one furthest into its release and drops a new note
// that finds none. MAIN.MID reaches 256 sounding and releasing voices with GeneralUser GS
// (the layered presets sound two or more voices per note), so 256 keeps every note.
inline constexpr int kSoundfontVoices = 256;

}  // namespace is::audio::music
