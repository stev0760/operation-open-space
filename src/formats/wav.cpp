#include "formats/wav.h"

#include <cstring>

#include "formats/bytes.h"

namespace is::fmt {

namespace {
bool tag_is(const std::uint8_t* p, const char* tag) { return std::memcmp(p, tag, 4) == 0; }
}  // namespace

Wave parse_wav(const std::uint8_t* p, std::size_t n) {
    Reader r(p, n);
    if (n < 12 || !tag_is(p, "RIFF") || !tag_is(p + 8, "WAVE")) throw FormatError("not RIFF WAVE");
    r.skip(4);
    const std::uint32_t riff = r.u32();
    // The RIFF size counts the "WAVE" tag, so the chunk walk below starts inside it.
    if (riff < 4) throw FormatError("RIFF size too small");
    if (riff > n - 8) throw FormatError("RIFF size overruns the data");
    Wave w;
    w.file_size = std::size_t{8} + riff;
    bool fmt = false, data = false;
    std::size_t pos = 12;
    while (w.file_size - pos >= 8) {
        r.seek(pos);
        const std::uint8_t* id = r.ptr();
        r.skip(4);
        const std::uint32_t len = r.u32();
        const std::size_t body = pos + 8;
        if (len > w.file_size - body) throw FormatError("WAVE chunk overruns the RIFF size");
        if (tag_is(id, "fmt ")) {
            if (len < 14) throw FormatError("WAVE fmt chunk too short");
            w.format_tag = r.u16();
            w.channels = r.u16();
            w.rate = r.u32();
            w.byte_rate = r.u32();
            w.block_align = r.u16();
            w.bits = len >= 16 ? r.u16() : 0;
            fmt = true;
        } else if (tag_is(id, "data") && !data) {
            w.data_offset = body;
            w.data_size = len;
            data = true;
        }
        pos = body + len + (len & 1);
        if (pos > w.file_size) break;  // the pad byte of the last chunk may be absent
    }
    if (!fmt || !data) throw FormatError("WAVE lacks a fmt or data chunk");
    return w;
}

std::vector<std::uint8_t> wav_file(const NeModule& m, const NeResource& r) {
    const std::uint8_t* p = m.data(r);
    const Wave w = parse_wav(p, m.size(r));
    return std::vector<std::uint8_t>(p, p + w.file_size);
}

}  // namespace is::fmt
