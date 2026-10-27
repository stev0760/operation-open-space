// The soundfont synth (ADR 0011 decision 3): TinySoundFont rendering the port's MIDI player's
// channel messages with a General MIDI SoundFont. It stands in for no routine of the original:
// the original hands its music to the MCI sequencer [BYTES ISPACE.DLL seg9:0x3906], and this
// synth replaces the synth behind that sequencer (the Microsoft GS Wavetable synth on Windows,
// none under Wine). [DESIGN]
#include "audio/music_soundfont.h"

#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>

#include "audio/synth.h"

// The one translation unit that holds TinySoundFont's implementation (third_party/tsf).
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define TSF_IMPLEMENTATION
#include "tsf.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace is::audio::music {

namespace {

constexpr int kChannels = 16;
constexpr int kDrumChannel = 9;     // GM channel 10
constexpr int kDrumBank = 128;      // the SoundFont 2 percussion bank

// tsf_load reads through this stream so that a path the narrow fopen of tsf_load_filename
// cannot open (non-ANSI characters on Windows) still loads. [DESIGN]
int stream_read(void* data, void* ptr, unsigned int size) {
    auto* in = static_cast<std::ifstream*>(data);
    in->read(static_cast<char*>(ptr), static_cast<std::streamsize>(size));
    return static_cast<int>(in->gcount());
}

int stream_skip(void* data, unsigned int count) {
    auto* in = static_cast<std::ifstream*>(data);
    in->seekg(static_cast<std::streamoff>(count), std::ios::cur);
    return in->good() ? 1 : 0;
}

bool is_gm_or_gs_reset(const std::uint8_t* m, std::size_t n) {
    // GM System On: F0 7E <dev> 09 01 F7.
    if (n >= 6 && m[1] == 0x7e && m[3] == 0x09 && m[4] == 0x01) return true;
    // GS Reset: F0 41 <dev> 42 12 40 00 7F 00 41 F7.
    static const std::uint8_t gs[] = {0x42, 0x12, 0x40, 0x00, 0x7f, 0x00};
    return n >= 11 && m[1] == 0x41 && std::memcmp(m + 3, gs, sizeof gs) == 0;
}

class SoundfontSynth final : public Synth {
public:
    ~SoundfontSynth() override { close(); }

    bool open(const SynthConfig& cfg) override {
        std::lock_guard<std::mutex> lock(mu_);
        close_locked();
        if (cfg.soundfont.empty() || cfg.rate == 0 ||
            cfg.rate > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
            return false;
        std::ifstream in(cfg.soundfont, std::ios::binary);
        if (!in) return false;
        tsf_stream s{&in, &stream_read, &stream_skip};
        tsf_ = tsf_load(&s);
        if (!tsf_) return false;
        if (tsf_get_presetcount(tsf_) <= 0) {
            close_locked();
            return false;
        }
        tsf_set_output(tsf_, TSF_STEREO_INTERLEAVED, static_cast<int>(cfg.rate), kSoundfontGainDb);
        tsf_set_max_voices(tsf_, kSoundfontVoices);
        reset_locked();
        return true;
    }

    void reset() override {
        std::lock_guard<std::mutex> lock(mu_);
        if (tsf_) reset_locked();
    }

    void midi(const std::uint8_t* m, std::size_t n) override {
        if (!m || n == 0) return;
        std::lock_guard<std::mutex> lock(mu_);
        if (!tsf_) return;
        const std::uint8_t status = m[0];
        if (status == 0xf0) {
            if (is_gm_or_gs_reset(m, n)) reset_locked();
            return;  // every other SysEx is ignored [DESIGN]
        }
        if (status < 0x80 || status >= 0xf0) return;  // data without status, system messages
        const int ch = status & 0x0f;
        const int d1 = n > 1 ? (m[1] & 0x7f) : -1;
        const int d2 = n > 2 ? (m[2] & 0x7f) : -1;
        switch (status & 0xf0) {
        case 0x80:
            if (d1 >= 0) tsf_channel_note_off(tsf_, ch, d1);
            break;
        case 0x90:
            if (d2 > 0)
                tsf_channel_note_on(tsf_, ch, d1, static_cast<float>(d2) / 127.0f);
            else if (d2 == 0)
                tsf_channel_note_off(tsf_, ch, d1);
            break;
        case 0xb0:
            if (d2 >= 0) control(ch, d1, d2);
            break;
        case 0xc0:
            // The preset of the channel's selected bank, else bank 0's; on the drum channel the
            // kit of bank 128 (tsf_channel_set_presetnumber's lookup order). A program the
            // SoundFont lacks keeps the channel's preset.
            if (d1 >= 0) tsf_channel_set_presetnumber(tsf_, ch, d1, ch == kDrumChannel ? 1 : 0);
            break;
        case 0xe0:
            if (d2 >= 0) tsf_channel_set_pitchwheel(tsf_, ch, d1 | (d2 << 7));
            break;
        default:  // 0xa0 polyphonic and 0xd0 channel pressure: TinySoundFont has no target
            break;
        }
    }

    void render(std::int16_t* out, std::size_t frames) override {
        if (!out || frames == 0) return;
        std::lock_guard<std::mutex> lock(mu_);
        if (!tsf_) {
            std::memset(out, 0, frames * 2 * sizeof(std::int16_t));
            return;
        }
        constexpr std::size_t kChunk = 1u << 20;
        while (frames) {
            const std::size_t n = frames < kChunk ? frames : kChunk;
            tsf_render_short(tsf_, out, static_cast<int>(n), 0);  // 0: replace, do not mix
            out += n * 2;
            frames -= n;
        }
    }

    std::string name() const override { return "soundfont"; }

private:
    void close() {
        std::lock_guard<std::mutex> lock(mu_);
        close_locked();
    }

    void close_locked() {
        if (tsf_) tsf_close(tsf_);
        tsf_ = nullptr;
    }

    // GM reset: every voice ends at once, every channel back to its power-on state (volume
    // 100, pan centre, expression 127, pitch wheel centre, bend range 2 semitones, sustain off)
    // with program 0 of bank 0, and the standard kit of bank 128 on channel 10.
    void reset_locked() {
        tsf_reset(tsf_);  // ends the voices quickly and drops the channels
        for (int ch = 0; ch < kChannels; ++ch) {
            const int bank = ch == kDrumChannel ? kDrumBank : 0;
            if (!tsf_channel_set_bank_preset(tsf_, ch, bank, 0))
                tsf_channel_set_presetnumber(tsf_, ch, 0, ch == kDrumChannel ? 1 : 0);
            tsf_channel_midi_control(tsf_, ch, 7, 100);  // GM default channel volume
        }
    }

    // The controllers TinySoundFont implements; every other one is ignored. [DESIGN]
    void control(int ch, int cc, int value) {
        switch (cc) {
        case 0:    // bank select MSB
        case 32:   // bank select LSB (the bank applies at the next program change)
        case 6:    // data entry MSB, 38 LSB: RPN 0 pitch-bend range, 1 fine and 2 coarse tuning
        case 38:
        case 100:  // RPN LSB, 101 MSB; 98/99 (NRPN) deselect the RPN
        case 101:
        case 98:
        case 99:
        case 7:    // channel volume, 39 LSB
        case 39:
        case 10:   // pan, 42 LSB
        case 42:
        case 11:   // expression, 43 LSB
        case 43:
        case 64:   // sustain (damper) pedal
        case 120:  // all sound off
        case 123:  // all notes off
            tsf_channel_midi_control(tsf_, ch, cc, value);
            break;
        case 124:  // omni off, omni on, mono, poly: each also ends the notes (MIDI 1.0)
        case 125:
        case 126:
        case 127:
            tsf_channel_midi_control(tsf_, ch, 123, 0);
            break;
        case 121:
            reset_controllers(ch);
            break;
        default:
            break;
        }
    }

    // Reset All Controllers as GM RP-015 defines it: expression 127, pitch wheel centre,
    // sustain off, the RPN deselected; channel volume, pan, bank and the bend range stay.
    // TinySoundFont's own CC 121 also resets volume, pan and bank, which a file that sends
    // CC 121 after its volume settings would hear as a jump to full volume. [DESIGN]
    void reset_controllers(int ch) {
        tsf_channel_midi_control(tsf_, ch, 11, 127);
        tsf_channel_midi_control(tsf_, ch, 43, 0);
        tsf_channel_set_pitchwheel(tsf_, ch, 8192);
        tsf_channel_set_sustain(tsf_, ch, 0);
        tsf_channel_midi_control(tsf_, ch, 101, 127);
        tsf_channel_midi_control(tsf_, ch, 100, 127);
    }

    std::mutex mu_;
    tsf* tsf_ = nullptr;
};

}  // namespace

std::unique_ptr<Synth> make_soundfont() { return std::make_unique<SoundfontSynth>(); }

}  // namespace is::audio::music
