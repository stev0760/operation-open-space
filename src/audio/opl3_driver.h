// The opl3 synth's General MIDI voice driver (ADR 0011 decision 3): MIDI channel messages in,
// YMF262 (OPL3) register writes out, over the bank of audio/opl3_bank.h. It stands in for the
// Windows FM synth driver behind the MIDI mapper that the original's MCI sequence plays through
// [BYTES ISPACE.DLL seg9:0x3906]; the driver itself is the port's own. [DESIGN] Portable: no
// Win32 calls, no chip; the writes go to a sink (the Nuked OPL3 chip in music_opl3.cpp, a
// recorder in the tests). Not thread-safe: the synth serialises the calls.
//
// The register model [REF Yamaha YMF262 data sheet, "Register map" and "Channel and operator
// assignment"]: two register banks (0x000-0x0FF and 0x100-0x1FF), 9 two-operator channels in
// each. Channel c (0..8) of bank b uses the operator slot offsets kOperatorOffset[c] (the
// modulator) and kOperatorOffset[c] + 3 (the carrier) under the operator registers 0x20, 0x40,
// 0x60, 0x80 and 0xE0, and the channel registers 0xA0 + c (F-number low 8 bits), 0xB0 + c
// (key on bit 5, block bits 4-2, F-number bits 9-8) and 0xC0 + c (right output bit 5, left
// output bit 4, feedback bits 3-1, connection bit 0). 0x105 bit 0 is NEW (OPL3 mode), 0x104 the
// four-operator connection select (all 0: eighteen 2-op channels), 0x01 bit 5 the OPL2
// waveform-select enable, 0xBD the depths and the rhythm mode (kOplDepthBits).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "audio/opl3_bank.h"

namespace is::audio::music {

inline constexpr int kOplVoices = 18;            // two-operator channels: 9 per register bank
inline constexpr int kOplChannelsPerBank = 9;
inline constexpr std::uint8_t kOperatorOffset[kOplChannelsPerBank] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
inline constexpr double kOplClockHz = 49716.0;   // the YMF262's sample clock (14.318 MHz / 288)

// The reset's 0xBD value: bit 7 the AM (tremolo) depth 4.8 dB instead of 1 dB, bit 6 the
// vibrato depth 14 cents instead of 7 [REF YMF262 data sheet, register BD]; bits 5-0 0, melodic
// mode. The Fat Man bank's WOPL header flags byte is 3, deep tremolo and deep vibrato
// [REF libADLMIDI v1.6.2.1 src/wopl/wopl_file.h WOPL_FLAG_DEEP_TREMOLO, WOPL_FLAG_DEEP_VIBRATO],
// and audio/opl3_bank.h keeps no flags, so the depths are this constant (M7 s2b fixer).
inline constexpr std::uint8_t kOplDepthBits = 0xc0;

// The F-number of each note of the octave from middle C (MIDI 60..71) at block 4:
// round(f * 2^(20 - 4) / 49716) with f the equal-tempered frequency at A4 = 440 Hz
// [REF YMF262 data sheet, "F-Number": f = F-Num * fs * 2^(block - 20)]. Note n plays at block
// n / 12 - 1 with kFnum[n % 12]; notes below the first octave halve the F-number at block 0,
// notes above block 7 double it at block 7 (capped at 1023).
inline constexpr std::uint16_t kFnum[12] = {345, 365, 387, 410, 435, 460, 488, 517, 547, 580, 615, 651};

// The register writes for one sounding pitch.
struct OplPitch {
    std::uint16_t fnum = 0;  // 0..1023
    std::uint8_t block = 0;  // 0..7
};
// `semitones` is a MIDI note number with a fractional part (the pitch bend); the result keeps
// the F-number of the note's own block, scaled by 2^(fraction / 12).
OplPitch opl_pitch(double semitones);

// The attenuation, in OPL total-level steps of 0.75 dB, of a MIDI value 0..127 on the
// 40 log10(v / 127) dB law (the GM Level 2 law for channel volume and expression, and the DLS
// Level 1 default for velocity) [REF GM Level 2 1.1, "Channel Volume" / "Expression"]; 0 for
// 127, 63 or more for 0. Rounded to the nearest step.
int opl_attenuation_steps(int value);

class Opl3Driver {
public:
    using Write = std::function<void(std::uint16_t reg, std::uint8_t value)>;

    Opl3Driver(const OplBank& bank, Write write);

    // GM reset: every voice silent at once, every channel's controllers and program back to
    // the power-on state, and the chip's start state written (OPL3 mode, waveform select,
    // every operator at full attenuation with the fastest release, every key off).
    void reset();

    // One channel message (status byte first) or one SysEx (F0 .. F7; a GM System On or a GS
    // Reset resets, every other SysEx is ignored).
    void midi(const std::uint8_t* msg, std::size_t len);

    // The synth's gain, as total-level steps (0.75 dB each) added to every operator the volume
    // law scales; applied from the next level write. Attenuating at the operators keeps the
    // chip's own 16-bit mix from clipping, which a gain after the chip cannot undo.
    void set_gain_steps(int steps) { gain_steps_ = steps < 0 ? 0 : steps; }
    int gain_steps() const { return gain_steps_; }

    // A GM System On (F0 7E <dev> 09 01 F7) or a GS Reset (F0 41 <dev> 42 12 40 00 7F 00 41 F7).
    static bool is_reset_sysex(const std::uint8_t* msg, std::size_t len);

    // ---- state, for the tests
    struct VoiceState {
        int channel = -1;        // MIDI channel 0..15, -1 idle
        int key = -1;            // the MIDI key that started it
        bool on = false;         // keyed on
        bool sustained = false;  // note off seen while the pedal held it
        std::uint64_t on_stamp = 0, off_stamp = 0;
    };
    const VoiceState& voice(int v) const { return voices_[v]; }
    // The register offset of voice v's channel (0..8, or 0x100..0x108) and of its operators.
    static std::uint16_t channel_reg(int v);
    static std::uint16_t operator_reg(int v, int op);  // op 0 modulator, 1 carrier

private:
    struct Channel {
        int program = 0;
        int volume = 100, expression = 127, pan = 64;
        bool sustain = false;
        int bend = 8192;           // 14-bit wheel
        int bend_range = 200;      // cents (RPN 0)
        int rpn_msb = 127, rpn_lsb = 127;
    };
    struct Voice : VoiceState {
        const OplInstrument* ins = nullptr;
        int velocity = 0;
        int note = 0;              // the played note (key, drum key and note offset applied)
    };

    void note_on(int ch, int key, int velocity);
    void note_off(int ch, int key);
    void control(int ch, int cc, int value);
    void reset_controllers(int ch);
    void key_off(int v);
    void silence(int v);
    int allocate();
    void write_levels(int v);
    void write_pitch(int v, bool key_on);
    void write_output(int v);
    void w(std::uint16_t reg, std::uint8_t value) { write_(reg, value); }

    const OplBank& bank_;
    Write write_;
    Channel channels_[16];
    Voice voices_[kOplVoices];
    int gain_steps_ = 0;
    std::uint64_t clock_ = 0;   // bumped by every note on and note off: the voices' age
};

}  // namespace is::audio::music
