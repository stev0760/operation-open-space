// The waves of the effects engine (ADR 0011 decision 5): the #100 WAV resources of SND_FX.DLL
// and <Language>_FX.DLL through the asset store, and the loose WAV files of id -1 from the
// sound directory, any PCM converted to the mixer's 8-bit, 11,025 Hz mono. Portable.
#pragma once

#include <cstddef>
#include <cstdint>

#include "audio/engine.h"

namespace is::assets {
class AssetStore;
}

namespace is::audio {

// A RIFF WAVE file as the mixer's samples: PCM of 8, 16, 24 or 32 bits and any channel count
// (averaged) at any rate (linearly resampled). nullptr when the file is not PCM WAVE.
PcmPtr to_effect_pcm(const std::uint8_t* file, std::size_t n);

class StoreWaveSource : public WaveSource {
public:
    explicit StoreWaveSource(assets::AssetStore& store) : store_(store) {}
    bool has_module(int k) override;
    PcmPtr slot(int k, int resource) override;
    PcmPtr file(const std::string& name) override;

private:
    std::string module_name(int k) const;
    assets::AssetStore& store_;
};

}  // namespace is::audio
