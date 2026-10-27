// The software mixer (mixer.h). [DESIGN] ADR 0011 decisions 1 and 2.
#include "audio/mixer.h"

#include <algorithm>

namespace is::audio {

void Mixer::configure(int channels) {
    std::lock_guard<std::mutex> lock(mu_);
    channels = std::clamp(channels, 1, kMaxChannels);
    ch_.assign(static_cast<std::size_t>(channels), Channel{});
    done_.clear();
}

int Mixer::channels() const {
    std::lock_guard<std::mutex> lock(mu_);
    return static_cast<int>(ch_.size());
}

bool Mixer::play(int channel, PcmPtr wave, int plays, bool notify) {
    std::lock_guard<std::mutex> lock(mu_);
    if (channel < 0 || channel >= static_cast<int>(ch_.size()) || !wave || wave->empty())
        return false;
    // WMIX_CLEARQUEUE: the channel's current wave goes without a completion [OPEN] whether
    // WaveMix posts MM_WOM_DONE for a wave it clears from the queue (sound.md open questions).
    ch_[static_cast<std::size_t>(channel)] = Channel{std::move(wave), 0, std::max(plays, 0), notify};
    return true;
}

void Mixer::stop(int channel) {
    std::lock_guard<std::mutex> lock(mu_);
    if (channel >= 0 && channel < static_cast<int>(ch_.size()))
        ch_[static_cast<std::size_t>(channel)] = Channel{};
}

void Mixer::stop_all() {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& c : ch_) c = Channel{};
    done_.clear();
}

bool Mixer::busy(int channel) const {
    std::lock_guard<std::mutex> lock(mu_);
    return channel >= 0 && channel < static_cast<int>(ch_.size()) &&
           ch_[static_cast<std::size_t>(channel)].wave != nullptr;
}

void Mixer::set_volume(std::uint16_t left, std::uint16_t right) {
    std::lock_guard<std::mutex> lock(mu_);
    left_ = left;
    right_ = right;
}

std::uint32_t Mixer::volume() const {
    std::lock_guard<std::mutex> lock(mu_);
    return static_cast<std::uint32_t>(right_) << 16 | left_;
}

std::vector<int> Mixer::take_completed() {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<int> out;
    out.swap(done_);
    return out;
}

int Mixer::mix_one() {
    std::lock_guard<std::mutex> lock(mu_);
    return mix_one_locked();
}

// Sums the channels' unsigned 8-bit samples about 128 and clips to the 8-bit range. WaveMix's
// own arithmetic is [OPEN] (sound.md open questions); ADR 0011 decision 1 sums and clips.
int Mixer::mix_one_locked() {
    int sum = 0;
    for (std::size_t i = 0; i < ch_.size(); ++i) {
        Channel& c = ch_[i];
        if (!c.wave) continue;
        sum += static_cast<int>((*c.wave)[c.pos]) - 128;
        if (++c.pos < c.wave->size()) continue;
        c.pos = 0;
        if (c.plays_left == kForever) continue;
        if (--c.plays_left > 0) continue;
        const bool notify = c.notify;
        c = Channel{};
        if (notify) done_.push_back(static_cast<int>(i));
    }
    return std::clamp(sum, -128, 127);
}

void Mixer::render(std::int16_t* out, std::size_t frames, std::uint32_t rate) {
    std::lock_guard<std::mutex> lock(mu_);
    const int step = std::max<int>(1, static_cast<int>(rate / kEffectRate));
    for (std::size_t f = 0; f < frames; ++f) {
        if (hold_ == 0) {
            held_ = mix_one_locked() * 256;
            hold_ = step;
        }
        --hold_;
        // The device volume scales the output linearly: 0xFFFF is unity. The Win32 docs say a
        // device may interpret waveOutSetVolume's words logarithmically [REF Microsoft
        // waveOutSetVolume], and how the original's devices mapped them is [OPEN]; linear is
        // the port's choice until ADR 0011 records one (M7 fixer).
        out[2 * f] = static_cast<std::int16_t>(static_cast<std::int64_t>(held_) * left_ / 0xFFFF);
        out[2 * f + 1] = static_cast<std::int16_t>(static_cast<std::int64_t>(held_) * right_ / 0xFFFF);
    }
}

}  // namespace is::audio
