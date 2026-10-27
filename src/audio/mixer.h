// The software mixer that replaces WAVEMIX.DLL and sndPlaySound (ADR 0011 decision 1):
// MixChannels channels of 8-bit, 11,025 Hz, mono PCM, each holding one wave at a time (every
// play clears the channel's queue at high priority, as ISPACE.DLL's WMIX_CLEARQUEUE |
// WMIX_HIPRIORITY plays do [BYTES ISPACE.DLL seg9:0x1383]), summed, clipped and scaled by the
// left and right volume words that the original hands to waveOutSetVolume (decision 2).
// Portable and thread-safe: the engine plays and stops from the program's thread, the output
// renders from its own.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace is::audio {

// The effects' native format [BYTES SND_FX.DLL 100] [BYTES ENG_FX.DLL 100].
inline constexpr std::uint32_t kEffectRate = 11025;

// 8-bit unsigned mono samples at kEffectRate.
using Pcm = std::vector<std::uint8_t>;
using PcmPtr = std::shared_ptr<const Pcm>;

// The most channels the records hold: the dialog offers 1 to 8 [BYTES ISPACE.DLL seg9:0x2fa1].
inline constexpr int kMaxChannels = 8;

class Mixer {
public:
    // plays = 0 loops until the next play or stop (sndPlaySound's SND_LOOP).
    static constexpr int kForever = 0;

    // Sets the channel count (1..kMaxChannels) and stops every channel.
    void configure(int channels);
    int channels() const;

    // Starts `wave` on `channel`, replacing what the channel played (clear queue, high
    // priority), `plays` times in a row (kForever: until replaced). `notify` = false keeps the
    // end silent (the single-channel path gets no completion message
    // [BYTES ISPACE.DLL seg9:0x2ac4 seg9:0x11b6]). Returns false for a bad channel or an
    // empty wave.
    bool play(int channel, PcmPtr wave, int plays, bool notify = true);
    void stop(int channel);
    void stop_all();
    bool busy(int channel) const;

    // The words of waveOutSetVolume(0, right << 16 | left) [BYTES ISPACE.DLL seg9:0xb8e],
    // applied to the mixed effects in software [DESIGN] ADR 0011 decision 2.
    void set_volume(std::uint16_t left, std::uint16_t right);
    std::uint32_t volume() const;  // right << 16 | left

    // The channels whose wave ended since the last call, in order (MM_WOM_DONE with
    // wParam = channel [BYTES ISPACE.DLL seg9:0x2adf]).
    std::vector<int> take_completed();

    // Mixes `frames` frames of 16-bit interleaved stereo at `rate` (an integer multiple of
    // kEffectRate; each effect sample is held rate / kEffectRate frames) into `out`.
    void render(std::int16_t* out, std::size_t frames, std::uint32_t rate = kEffectRate);

    // One mono effect frame before the volume: the clipped sum of every channel's sample,
    // centred on 0 (-128..127). Advances the channels. Exposed for the tests.
    int mix_one();

private:
    struct Channel {
        PcmPtr wave;
        std::size_t pos = 0;
        int plays_left = 0;  // kForever (0) with a wave: loops
        bool notify = true;
    };
    int mix_one_locked();

    mutable std::mutex mu_;
    std::vector<Channel> ch_ = std::vector<Channel>(1);
    std::vector<int> done_;
    std::uint16_t left_ = 0xFFFF, right_ = 0xFFFF;
    int hold_ = 0;      // frames left of the held effect sample at the output rate
    int held_ = 0;      // the held mono value
};

}  // namespace is::audio
