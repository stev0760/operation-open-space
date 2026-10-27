// The OPL3 instrument bank behind the opl3 synth (ADR 0011 decision 3): the Fat Man 2-operator
// General MIDI set (MIT, third_party/fatman-2op/), embedded at build time and parsed from its
// WOPL container at first use. Portable.
#pragma once
#include <array>
#include <cstddef>  // std::size_t (M7 s2b fixer)
#include <cstdint>
namespace is::audio::music {
struct OplOperator {
    std::uint8_t avekf;   // register 0x20: AM, VIB, EG type, KSR, multiplier
    std::uint8_t ksl_tl;  // register 0x40: key scale level, total level
    std::uint8_t ad;      // register 0x60: attack, decay
    std::uint8_t sr;      // register 0x80: sustain, release
    std::uint8_t wave;    // register 0xE0: waveform
};
struct OplInstrument {
    std::uint8_t feedconn = 0;      // register 0xC0 low nibble: feedback << 1 | connection
    std::int16_t note_offset = 0;   // semitones added to the note
    std::int8_t velocity_offset = 0;
    std::uint8_t percussion_key = 0;  // drums: the fixed key to play (0 = none)
    bool blank = false;             // no voice
    bool four_op = false;           // this bank has none; a 4-op entry is played with ops 0 and 1
    OplOperator op[2] = {};         // modulator, carrier
    std::uint16_t ms_sound_kon = 0, ms_sound_koff = 0;  // WOPL v3 delays, unused by the driver
};
struct OplBank {
    std::array<OplInstrument, 128> melodic;     // General MIDI programs 0..127
    std::array<OplInstrument, 128> percussion;  // General MIDI drum keys 0..127
};
// The embedded Fat Man bank, parsed once; the same object on every call.
const OplBank& fatman_bank();
// Parses a WOPL3 bank image (the first melodic and first percussion bank); false on a bad image.
bool parse_wopl(const std::uint8_t* data, std::size_t size, OplBank& out);
}  // namespace is::audio::music
