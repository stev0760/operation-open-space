// The waves (wav_source.h).
#include "audio/wav_source.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <vector>

#include "assets/asset_store.h"
#include "formats/wav.h"

namespace is::audio {

PcmPtr to_effect_pcm(const std::uint8_t* file, std::size_t n) {
    fmt::Wave w;
    try {
        w = fmt::parse_wav(file, n);
    } catch (const std::exception&) {
        return nullptr;
    }
    const unsigned bytes = w.bits / 8;
    if (w.format_tag != 1 || w.channels == 0 || w.rate == 0 || bytes == 0 || bytes > 4 ||
        w.block_align < bytes * w.channels)
        return nullptr;
    const std::size_t frames = w.frames();
    if (frames == 0) return nullptr;
    // Each frame as a signed value in -32768..32767, channels averaged.
    std::vector<int> mono(frames);
    const std::uint8_t* d = file + w.data_offset;
    for (std::size_t f = 0; f < frames; ++f) {
        long sum = 0;
        for (unsigned c = 0; c < w.channels; ++c) {
            const std::uint8_t* s = d + f * w.block_align + c * bytes;
            int v;
            if (bytes == 1) {
                v = (static_cast<int>(s[0]) - 128) * 256;
            } else {
                // little-endian signed; the top two bytes give 16 bits
                const std::uint8_t lo = s[bytes - 2], hi = s[bytes - 1];
                v = static_cast<std::int16_t>(static_cast<std::uint16_t>(hi << 8 | lo));
            }
            sum += v;
        }
        mono[f] = static_cast<int>(sum / static_cast<long>(w.channels));
    }
    auto out = std::make_shared<Pcm>();
    if (w.rate == kEffectRate && bytes == 1 && w.channels == 1) {
        out->assign(d, d + frames);
        return out;
    }
    const std::size_t n_out =
        static_cast<std::size_t>(static_cast<double>(frames) * kEffectRate / w.rate);
    out->resize(std::max<std::size_t>(n_out, 1));
    for (std::size_t i = 0; i < out->size(); ++i) {
        const double pos = static_cast<double>(i) * w.rate / kEffectRate;
        const std::size_t i0 = std::min(static_cast<std::size_t>(pos), frames - 1);
        const std::size_t i1 = std::min(i0 + 1, frames - 1);
        const double t = pos - static_cast<double>(i0);
        const double v = mono[i0] + (mono[i1] - mono[i0]) * t;
        const int s8 = static_cast<int>(std::lround(v / 256.0)) + 128;
        (*out)[i] = static_cast<std::uint8_t>(std::clamp(s8, 0, 255));
    }
    return out;
}

std::string StoreWaveSource::module_name(int k) const {
    return k == 0 ? std::string(assets::kSoundDll) : store_.fx_module();
}

bool StoreWaveSource::has_module(int k) {
    try {
        return store_.sound_files().has(module_name(k));
    } catch (const std::exception&) {
        return false;
    }
}

PcmPtr StoreWaveSource::slot(int k, int resource) {
    try {
        const assets::Sound s = store_.wav(module_name(k), static_cast<std::uint16_t>(resource));
        return to_effect_pcm(s.file.data(), s.file.size());
    } catch (const std::exception&) {
        return nullptr;  // FindResource fails: the four missing resources
    }
}

// SndPath + name + ".WAV" [BYTES ISPACE.DLL seg9:0xc72 seg9:0x1333].
PcmPtr StoreWaveSource::file(const std::string& name) {
    if (name.empty()) return nullptr;
    try {
        const std::vector<std::uint8_t> b = store_.sound_files().read(name + ".WAV");
        return to_effect_pcm(b.data(), b.size());
    } catch (const std::exception&) {
        return nullptr;
    }
}

}  // namespace is::audio
