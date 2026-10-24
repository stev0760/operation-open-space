// RIFF WAVE files, as SND_FX.DLL (46) and ENG_FX.DLL (32) store them in their #100
// resources: PCM, 11,025 Hz, 8-bit, mono (docs/re/resources.md#custom-types). The NE
// table pads each resource to 16 bytes, so the file's real length comes from the RIFF
// header, never from the resource length (docs/re/resources.md#win16-layouts).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "formats/ne.h"

namespace is::fmt {

// Resource type of the sound modules' WAV files.
constexpr std::uint16_t kWavResourceType = 100;

struct Wave {
    // fmt chunk (WAVEFORMAT; `bits` from PCMWAVEFORMAT, 0 when the chunk is shorter)
    std::uint16_t format_tag = 0;  // 1 = WAVE_FORMAT_PCM
    std::uint16_t channels = 0;
    std::uint32_t rate = 0;
    std::uint32_t byte_rate = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits = 0;
    // offsets into the parsed bytes
    std::size_t data_offset = 0;   // first sample byte of the data chunk
    std::size_t data_size = 0;     // the data chunk's length
    std::size_t file_size = 0;     // 8 + the RIFF size: the whole file, padding excluded

    std::size_t frames() const { return block_align ? data_size / block_align : 0; }
};

// Walks the chunks inside the RIFF size (each padded to an even length). Throws FormatError
// when the header is not RIFF WAVE, the RIFF size is below 4 (the "WAVE" tag) or overruns
// `n`, a chunk overruns the RIFF size, the fmt chunk is shorter than 14 bytes, or the fmt
// or data chunk is missing.
Wave parse_wav(const std::uint8_t* p, std::size_t n);

// The file bytes of a WAV resource of `m`, cut to the RIFF size.
std::vector<std::uint8_t> wav_file(const NeModule& m, const NeResource& r);

}  // namespace is::fmt
