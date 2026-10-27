// The opl3 synth's General MIDI voice driver (ADR 0011 decision 3); the model and the tables
// are in audio/opl3_driver.h. The port's own driver: the original sends its music to the MCI
// sequencer [BYTES ISPACE.DLL seg9:0x3906] and has no FM code of its own. [DESIGN]
#include "audio/opl3_driver.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace is::audio::music {

namespace {

constexpr int kDrumChannel = 9;  // GM channel 10

int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Pan: the OPL3 has only an on/off switch per output (0xC0 bit 4 left, bit 5 right), so CC 10
// splits in three [DESIGN]: 0..42 left only, 43..84 both, 85..127 right only.
std::uint8_t pan_bits(int pan) {
    if (pan < 43) return 0x10;
    if (pan > 84) return 0x20;
    return 0x30;
}

}  // namespace

OplPitch opl_pitch(double semitones) {
    if (semitones < 0) semitones = 0;
    if (semitones > 140) semitones = 140;
    const double whole = std::floor(semitones);
    const int n = static_cast<int>(whole);
    double fnum = kFnum[n % 12] * std::pow(2.0, (semitones - whole) / 12.0);
    int block = n / 12 - 1;
    if (block < 0) {
        fnum /= double(1 << -block);
        block = 0;
    } else if (block > 7) {
        fnum *= double(1 << (block - 7));
        block = 7;
    }
    OplPitch p;
    p.fnum = static_cast<std::uint16_t>(clamp(static_cast<int>(std::lround(fnum)), 0, 1023));
    p.block = static_cast<std::uint8_t>(block);
    return p;
}

int opl_attenuation_steps(int value) {
    static const auto table = [] {
        std::array<std::uint8_t, 128> t{};
        t[0] = 63;
        for (int v = 1; v < 128; ++v) {
            const double db = -40.0 * std::log10(v / 127.0);
            t[v] = static_cast<std::uint8_t>(std::min(63L, std::lround(db / 0.75)));
        }
        return t;
    }();
    return table[clamp(value, 0, 127)];
}

Opl3Driver::Opl3Driver(const OplBank& bank, Write write) : bank_(bank), write_(std::move(write)) {}

bool Opl3Driver::is_reset_sysex(const std::uint8_t* m, std::size_t n) {
    if (!m || n == 0 || m[0] != 0xf0) return false;
    // GM System On: F0 7E <dev> 09 01 F7.
    if (n >= 6 && m[1] == 0x7e && m[3] == 0x09 && m[4] == 0x01) return true;
    // GS Reset: F0 41 <dev> 42 12 40 00 7F 00 41 F7.
    static const std::uint8_t gs[] = {0x42, 0x12, 0x40, 0x00, 0x7f, 0x00};
    return n >= 11 && m[1] == 0x41 && std::memcmp(m + 3, gs, sizeof gs) == 0;
}

std::uint16_t Opl3Driver::channel_reg(int v) {
    return static_cast<std::uint16_t>((v / kOplChannelsPerBank) * 0x100 + v % kOplChannelsPerBank);
}

std::uint16_t Opl3Driver::operator_reg(int v, int op) {
    return static_cast<std::uint16_t>((v / kOplChannelsPerBank) * 0x100 +
                                      kOperatorOffset[v % kOplChannelsPerBank] + (op ? 3 : 0));
}

void Opl3Driver::reset() {
    for (auto& c : channels_) c = Channel{};
    for (auto& v : voices_) v = Voice{};
    clock_ = 0;
    w(0x105, 0x01);  // NEW: OPL3 mode, so the 0x100 bank and the eight waveforms are live
    w(0x104, 0x00);  // no four-operator pairs: eighteen two-operator channels
    w(0x001, 0x20);  // waveform select enable (the OPL2 bit; OPL3 mode implies it)
    w(0x008, 0x00);  // CSM off, note select 0
    w(0x0bd, kOplDepthBits);  // deep tremolo and vibrato, melodic mode (M7 s2b fixer)
    for (int v = 0; v < kOplVoices; ++v) {
        const std::uint16_t c = channel_reg(v);
        w(0xb0 + c, 0x00);  // key off
        w(0xa0 + c, 0x00);
        w(0xc0 + c, 0x00);
        for (int op = 0; op < 2; ++op) {
            const std::uint16_t o = operator_reg(v, op);
            w(0x20 + o, 0x00);
            w(0x40 + o, 0x3f);  // full attenuation
            w(0x60 + o, 0xff);
            w(0x80 + o, 0xff);  // fastest release
            w(0xe0 + o, 0x00);
        }
    }
}

void Opl3Driver::midi(const std::uint8_t* m, std::size_t n) {
    if (!m || n == 0) return;
    const std::uint8_t status = m[0];
    if (status == 0xf0) {
        if (is_reset_sysex(m, n)) reset();
        return;  // every other SysEx is ignored [DESIGN]
    }
    if (status < 0x80 || status >= 0xf0) return;
    const int ch = status & 0x0f;
    const int d1 = n > 1 ? (m[1] & 0x7f) : -1;
    const int d2 = n > 2 ? (m[2] & 0x7f) : -1;
    switch (status & 0xf0) {
    case 0x80:
        if (d1 >= 0) note_off(ch, d1);
        break;
    case 0x90:
        if (d2 > 0)
            note_on(ch, d1, d2);
        else if (d2 == 0)
            note_off(ch, d1);
        break;
    case 0xb0:
        if (d2 >= 0) control(ch, d1, d2);
        break;
    case 0xc0:
        if (d1 >= 0) channels_[ch].program = d1;  // GM bank 0 only: bank select is ignored
        break;
    case 0xe0:
        if (d2 >= 0) {
            channels_[ch].bend = d1 | (d2 << 7);
            for (int v = 0; v < kOplVoices; ++v)
                if (voices_[v].channel == ch) write_pitch(v, voices_[v].on);
        }
        break;
    default:  // 0xa0 polyphonic and 0xd0 channel pressure: no target [DESIGN]
        break;
    }
}

// Free voice first, else the one longest in release, else the one keyed on longest. "Free"
// is a voice no note has used since the reset or an all-sound-off: the driver has no view of
// the chip's envelopes, so a released voice counts as releasing until it is reused. [DESIGN]
int Opl3Driver::allocate() {
    int released = -1, oldest = -1;
    for (int v = 0; v < kOplVoices; ++v) {
        const Voice& x = voices_[v];
        if (x.channel < 0) return v;
        if (!x.on) {
            if (released < 0 || x.off_stamp < voices_[released].off_stamp) released = v;
        } else if (oldest < 0 || x.on_stamp < voices_[oldest].on_stamp) {
            oldest = v;
        }
    }
    return released >= 0 ? released : oldest;
}

void Opl3Driver::note_on(int ch, int key, int velocity) {
    const bool drum = ch == kDrumChannel;
    const OplInstrument& ins = drum ? bank_.percussion[key] : bank_.melodic[channels_[ch].program];
    if (ins.blank) return;
    const int played = (drum && ins.percussion_key ? ins.percussion_key : key) + ins.note_offset;

    const int v = allocate();
    Voice& x = voices_[v];
    if (x.on) key_off(v);  // a stolen voice: the key-off write lets the chip retrigger
    x.channel = ch;
    x.key = key;
    x.on = true;
    x.sustained = false;
    x.on_stamp = ++clock_;
    x.ins = &ins;
    x.velocity = clamp(velocity + ins.velocity_offset, 1, 127);
    x.note = played;

    for (int op = 0; op < 2; ++op) {
        const std::uint16_t o = operator_reg(v, op);
        const OplOperator& p = ins.op[op];
        w(0x20 + o, p.avekf);
        w(0x40 + o, p.ksl_tl);  // the level follows from write_levels below
        w(0x60 + o, p.ad);
        w(0x80 + o, p.sr);
        w(0xe0 + o, p.wave);
    }
    write_levels(v);
    write_output(v);
    write_pitch(v, true);
}

// A note off ends one voice: the oldest sounding one of the channel and key, so a key struck
// twice before its first note off keeps its second note to its own note off [DESIGN]
// (M7 s2b fixer).
void Opl3Driver::note_off(int ch, int key) {
    int oldest = -1;
    for (int v = 0; v < kOplVoices; ++v) {
        const Voice& x = voices_[v];
        if (x.channel != ch || x.key != key || !x.on || x.sustained) continue;
        if (oldest < 0 || x.on_stamp < voices_[oldest].on_stamp) oldest = v;
    }
    if (oldest < 0) return;
    if (channels_[ch].sustain)
        voices_[oldest].sustained = true;
    else
        key_off(oldest);
}

void Opl3Driver::key_off(int v) {
    Voice& x = voices_[v];
    x.on = false;
    x.sustained = false;
    x.off_stamp = ++clock_;
    write_pitch(v, false);
}

void Opl3Driver::silence(int v) {
    Voice& x = voices_[v];
    for (int op = 0; op < 2; ++op) {
        const std::uint16_t o = operator_reg(v, op);
        w(0x40 + o, 0x3f);
        w(0x80 + o, static_cast<std::uint8_t>((x.ins ? x.ins->op[op].sr : 0) | 0x0f));
    }
    if (x.channel >= 0) write_pitch(v, false);
    x = Voice{};
}

// The volume law: the carrier's attenuation is the instrument's total level plus the steps of
// the velocity, CC 7 and CC 11, each on the 40 log10 law of opl_attenuation_steps, capped at 63
// (silent). The modulator is scaled the same way when the connection is additive (feedconn
// bit 0 = 1: both operators reach the output); in FM it shapes the timbre and keeps its level.
void Opl3Driver::write_levels(int v) {
    const Voice& x = voices_[v];
    if (!x.ins) return;
    const Channel& c = channels_[x.channel];
    const int atten = opl_attenuation_steps(x.velocity) + opl_attenuation_steps(c.volume) +
                      opl_attenuation_steps(c.expression) + gain_steps_;
    for (int op = 0; op < 2; ++op) {
        const std::uint8_t r = x.ins->op[op].ksl_tl;
        const bool scaled = op == 1 || (x.ins->feedconn & 0x01);
        const int tl = scaled ? std::min(63, (r & 0x3f) + atten) : (r & 0x3f);
        w(0x40 + operator_reg(v, op), static_cast<std::uint8_t>((r & 0xc0) | tl));
    }
}

void Opl3Driver::write_output(int v) {
    const Voice& x = voices_[v];
    if (!x.ins) return;
    w(0xc0 + channel_reg(v),
      static_cast<std::uint8_t>((x.ins->feedconn & 0x0f) | pan_bits(channels_[x.channel].pan)));
}

void Opl3Driver::write_pitch(int v, bool key_on) {
    const Voice& x = voices_[v];
    const Channel& c = channels_[x.channel];
    const double bend = (c.bend - 8192) / 8192.0 * (c.bend_range / 100.0);
    const OplPitch p = opl_pitch(x.note + bend);
    const std::uint16_t r = channel_reg(v);
    w(0xa0 + r, static_cast<std::uint8_t>(p.fnum & 0xff));
    w(0xb0 + r, static_cast<std::uint8_t>((key_on ? 0x20 : 0) | (p.block << 2) | (p.fnum >> 8)));
}

void Opl3Driver::control(int ch, int cc, int value) {
    Channel& c = channels_[ch];
    auto each = [&](auto fn) {
        for (int v = 0; v < kOplVoices; ++v)
            if (voices_[v].channel == ch) fn(v);
    };
    switch (cc) {
    case 7:
        c.volume = value;
        each([&](int v) { write_levels(v); });
        break;
    case 11:
        c.expression = value;
        each([&](int v) { write_levels(v); });
        break;
    case 10:
        c.pan = value;
        each([&](int v) { write_output(v); });
        break;
    case 64:
        c.sustain = value >= 64;
        if (!c.sustain)
            each([&](int v) {
                if (voices_[v].sustained) key_off(v);
            });
        break;
    case 101: c.rpn_msb = value; break;
    case 100: c.rpn_lsb = value; break;
    case 98:   // an NRPN selection deselects the RPN
    case 99:
        c.rpn_msb = c.rpn_lsb = 127;
        break;
    case 6:    // data entry MSB: RPN 0 is the pitch-bend range in semitones
        if (c.rpn_msb == 0 && c.rpn_lsb == 0) {
            c.bend_range = value * 100 + c.bend_range % 100;
            each([&](int v) { write_pitch(v, voices_[v].on); });
        }
        break;
    case 38:   // data entry LSB: its cents
        if (c.rpn_msb == 0 && c.rpn_lsb == 0) {
            c.bend_range = c.bend_range / 100 * 100 + std::min(value, 99);
            each([&](int v) { write_pitch(v, voices_[v].on); });
        }
        break;
    case 120:  // all sound off
        each([&](int v) { silence(v); });
        break;
    case 121:
        reset_controllers(ch);
        break;
    case 123:  // all notes off; 124-127 (omni and mono/poly) also end the notes (MIDI 1.0)
    case 124:
    case 125:
    case 126:
    case 127:
        each([&](int v) {
            if (voices_[v].on) key_off(v);
        });
        break;
    default:   // bank select (0, 32) and every other controller: ignored [DESIGN]
        break;
    }
}

// Reset All Controllers as GM RP-015 defines it (the subset music_soundfont.cpp applies):
// expression 127, pitch wheel centre, sustain off, the RPN deselected; channel volume, pan,
// program and the bend range stay.
void Opl3Driver::reset_controllers(int ch) {
    Channel& c = channels_[ch];
    c.expression = 127;
    c.bend = 8192;
    c.sustain = false;
    c.rpn_msb = c.rpn_lsb = 127;
    for (int v = 0; v < kOplVoices; ++v) {
        if (voices_[v].channel != ch) continue;
        if (voices_[v].sustained) key_off(v);
        write_levels(v);
        write_pitch(v, voices_[v].on);
    }
}

}  // namespace is::audio::music
