// The waveform of isview's WAV view: per pixel column, the lowest and highest sample of the
// first channel. Free of Win32, so that tests/test_isview_dump.cpp checks its bounds.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "formats/wav.h"

namespace is::isview {

struct WaveColumn {
    int lo = 0, hi = 0;  // samples scaled to -32768..32767
};

// True when the view can read the first channel of every frame: PCM (format tag 1) with
// samples of 1-8 bits (one unsigned byte) or 16 bits (signed, little-endian), a block_align
// that holds at least one whole sample, and a data chunk inside the `file_size` bytes.
// parse_wav accepts other fmt chunks, such as 16 bits with block_align 1, whose frames hold
// part of a sample; reading those would run past the data chunk.
inline bool wave_drawable(const fmt::Wave& w, std::size_t file_size) {
    if (w.format_tag != 1) return false;
    const bool byte = w.bits >= 1 && w.bits <= 8;
    if (!byte && w.bits != 16) return false;
    if (w.block_align < (byte ? 1u : 2u)) return false;
    return w.data_offset <= file_size && w.data_size <= file_size - w.data_offset;
}

// `columns` columns of the first channel's samples, each over its share of the frames, or
// nothing when the wave is not drawable or has no frame. `file` holds `file_size` bytes, the
// RIFF file that `w` was parsed from.
inline std::vector<WaveColumn> wave_columns(const fmt::Wave& w, const std::uint8_t* file,
                                            std::size_t file_size, int columns) {
    const std::size_t frames = w.frames();
    if (columns <= 0 || frames == 0 || !file || !wave_drawable(w, file_size)) return {};
    // frames * step <= data_size and a sample fits in a step, so the last sample's last byte
    // is inside the data chunk
    const bool byte = w.bits <= 8;
    const std::size_t step = w.block_align;
    const std::uint8_t* data = file + w.data_offset;
    const auto n = static_cast<std::uint64_t>(columns);
    std::vector<WaveColumn> out(static_cast<std::size_t>(columns));
    for (std::uint64_t x = 0; x < n; ++x) {
        // a < frames for every x < n; each column covers at least one frame
        const auto a = static_cast<std::size_t>(frames * x / n);
        const auto b = std::max(a + 1, static_cast<std::size_t>(frames * (x + 1) / n));
        int lo = 0x7FFF, hi = -0x8000;
        for (std::size_t i = a; i < b && i < frames; ++i) {
            const std::uint8_t* s = data + i * step;
            const int v = byte ? (s[0] - 128) * 256 : static_cast<std::int16_t>(s[0] | (s[1] << 8));
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        out[static_cast<std::size_t>(x)] = {lo, hi};
    }
    return out;
}

}  // namespace is::isview
