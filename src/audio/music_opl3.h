// The opl3 music synth of ADR 0011 decision 3: the Nuked OPL3 chip emulator
// (third_party/nuked-opl3, LGPL-2.1-or-later) played by the port's General MIDI voice driver
// (audio/opl3_driver.h) with the Fat Man 2-operator bank (audio/opl3_bank.h), behind the port's
// MIDI player. make_opl3() is declared in audio/synth.h. Built when the CMake option IS_OPL3 is
// on (it defines IS_OPL3=1); with it off make_opl3() returns nullptr and the session keeps the
// mci or soundfont backend.
#pragma once

namespace is::audio::music {

#if defined(IS_OPL3) && IS_OPL3
inline constexpr bool kOpl3BuiltIn = true;
#else
inline constexpr bool kOpl3BuiltIn = false;
#endif

// The synth's gain, in OPL total-level steps of 0.75 dB added to every operator the driver's
// volume law scales (Opl3Driver::set_gain_steps). [DESIGN] The chip sums its eighteen channels
// into a 16-bit output and clips there, so any cut is taken before the mix, at the operators.
// At 0 (unity: the chip's output as it comes) the four ship-disk files played end to end as the
// player plays them (channels 1-10 of the dual-mode files, midi_file.h dual_mode_skipped)
// through the Fat Man bank at 22,050 Hz peak at MAIN -7.4, RACE -8.7, ANTHEM -7.9 and DEMON
// -9.2 dBFS with no clipped sample (RMS -26.9, -24.6, -24.0 and -25.3 dBFS): the OPL3's own
// mix leaves headroom for an 18-voice load and the files never reach it (M7 s2b fixer).
// The music then sits about 8 to 10 dB (RMS) above the soundfont synth's at its -22 dB, and
// the mixer's sum of this music and a loud effect burst can reach full scale and clip
// (decision 1's sum-and-clip); the level against the effects and against the soundfont synth
// is [OPEN] until listened to, as for the soundfont.
// tests/test_opl3_driver.cpp re-measures the peaks.
inline constexpr int kOpl3GainSteps = 0;

}  // namespace is::audio::music
