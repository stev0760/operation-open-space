// Standard MIDI File reader (midi_file.h) [REF Standard MIDI Files 1.0].
#include "audio/midi_file.h"

#include "assets/game_files.h"
#include "formats/bytes.h"  // path_utf8 for the log lines (M7 s3 fixer)

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

namespace is::audio::music {

namespace {

constexpr std::uint32_t kDefaultTempo = 500000;  // 120 bpm [REF Standard MIDI Files 1.0]

std::uint32_t be32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) << 24 | static_cast<std::uint32_t>(p[1]) << 16 |
           static_cast<std::uint32_t>(p[2]) << 8 | p[3];
}
std::uint16_t be16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] << 8 | p[1]);
}

bool fail(std::string* why, const std::string& s) {
    if (why) *why = s;
    return false;
}

struct Reader {
    const std::uint8_t* p;
    const std::uint8_t* end;
    bool ok = true;
    std::uint8_t byte() {
        if (p >= end) {
            ok = false;
            return 0;
        }
        return *p++;
    }
    // A variable-length quantity: at most four bytes [REF Standard MIDI Files 1.0].
    std::uint32_t vlq() {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            const std::uint8_t b = byte();
            v = (v << 7) | (b & 0x7f);
            if (!(b & 0x80)) return v;
        }
        ok = false;
        return v;
    }
    bool skip(std::uint32_t n) {
        if (static_cast<std::size_t>(end - p) < n) return ok = false;
        p += n;
        return true;
    }
};

int data_bytes(std::uint8_t status) {
    switch (status & 0xf0) {
    case 0xc0:
    case 0xd0: return 1;
    default: return 2;
    }
}

struct RawTempo {
    std::uint32_t tick, us;
};

}  // namespace

// A distribution directory (ADR 0002 decision 2) holds the file as ANTHEM.MI1 (SZDD): when
// the installed name is not on disk, the directory's GameFiles gives the expanded bytes
// under the installed name [DESIGN].
bool MidiFile::load(const std::filesystem::path& path, std::string* why) {
    std::ifstream in(path, std::ios::binary);
    if (in) {
        std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
        return parse(b.data(), b.size(), why);
    }
    try {
        const assets::GameFiles files(path.parent_path());
        const std::vector<std::uint8_t> b = files.read(fmt::path_utf8(path.filename()));
        return parse(b.data(), b.size(), why);
    } catch (const std::exception&) {
        return fail(why, "cannot open " + fmt::path_utf8(path));
    }
}

bool MidiFile::parse(const std::uint8_t* data, std::size_t size, std::string* why) {
    *this = MidiFile{};
    if (size < 14 || std::string(reinterpret_cast<const char*>(data), 4) != "MThd")
        return fail(why, "no MThd header");
    const std::uint32_t hlen = be32(data + 4);
    if (hlen < 6 || hlen > size - 8) return fail(why, "bad MThd length");
    format_ = be16(data + 8);
    tracks_ = be16(data + 10);
    division_ = be16(data + 12);
    if (format_ > 1) return fail(why, "SMF format " + std::to_string(format_) + " not played");
    if (division_ == 0) return fail(why, "division 0");
    if (division_ & 0x8000) {  // SMPTE: -frames per second in the high byte, ticks per frame
        const int fps = -static_cast<std::int8_t>(division_ >> 8);
        const int tpf = division_ & 0xff;
        if (fps <= 0 || tpf == 0) return fail(why, "bad SMPTE division");
        const double rate = (fps == 29 ? 29.97 : fps) * tpf;
        ms_per_tick_smpte_ = 1000.0 / rate;
    }

    std::vector<RawTempo> tempos;
    std::size_t pos = 8 + hlen;
    int found = 0;
    while (pos + 8 <= size && found < tracks_) {
        const std::uint32_t clen = be32(data + pos + 4);
        const bool is_track = std::equal(data + pos, data + pos + 4, "MTrk");
        const std::size_t body = pos + 8;
        if (clen > size - body) return fail(why, "chunk past the end of the file");
        pos = body + clen;
        if (!is_track) continue;  // unknown chunks are skipped
        Reader r{data + body, data + body + clen};
        std::uint32_t tick = 0;
        std::uint8_t running = 0;
        bool ended = false;
        while (r.ok && r.p < r.end && !ended) {
            tick += r.vlq();
            std::uint8_t b = r.byte();
            if (!r.ok) break;
            if (b == 0xff) {  // meta
                const std::uint8_t type = r.byte();
                const std::uint32_t len = r.vlq();
                const std::uint8_t* d = r.p;
                if (!r.skip(len)) break;
                if (type == 0x51 && len == 3)
                    tempos.push_back({tick, static_cast<std::uint32_t>(d[0] << 16 | d[1] << 8 | d[2])});
                else if (type == 0x2f)
                    ended = true;
                else if (type == 0x7f && len == 3 && d[0] == 0x00 && d[1] == 0x00 && d[2] == 0x41)
                    microsoft_authored_ = true;  // Microsoft's id (M7 s2b fixer)
                running = 0;  // meta events cancel running status [REF Standard MIDI Files 1.0]
                continue;
            }
            if (b == 0xf0 || b == 0xf7) {  // SysEx, or an escape whose bytes go out as they are
                const std::uint32_t len = r.vlq();
                const std::uint8_t* d = r.p;
                if (!r.skip(len)) break;
                MidiEvent e;
                e.tick = tick;
                e.kind = MidiEvent::kSysEx;
                e.offset = static_cast<std::uint32_t>(sysex_.size());
                if (b == 0xf0) sysex_.push_back(0xf0);
                sysex_.insert(sysex_.end(), d, d + len);
                e.size = static_cast<std::uint32_t>(sysex_.size()) - e.offset;
                if (e.size) events_.push_back(e);
                running = 0;
                continue;
            }
            MidiEvent e;
            e.tick = tick;
            if (b & 0x80) {
                if (b >= 0xf0) return fail(why, "system message in a track");
                running = b;
                e.bytes[0] = b;
                for (int i = 0; i < data_bytes(b); ++i) e.bytes[1 + i] = r.byte();
            } else {
                if (!running) return fail(why, "data byte without running status");
                e.bytes[0] = running;
                e.bytes[1] = b;
                if (data_bytes(running) == 2) e.bytes[2] = r.byte();
            }
            e.len = static_cast<std::uint8_t>(1 + data_bytes(e.bytes[0]));
            if (!r.ok) break;
            events_.push_back(e);
        }
        if (!r.ok) return fail(why, "track " + std::to_string(found) + " truncated");
        length_ticks_ = std::max(length_ticks_, tick);
        ++found;
    }
    if (found != tracks_)
        return fail(why, "MThd names " + std::to_string(tracks_) + " tracks, the file holds " +
                             std::to_string(found));

    // Merge: the tracks went in in order, so a stable sort by tick keeps track order and the
    // order within a track at one tick.
    std::stable_sort(events_.begin(), events_.end(),
                     [](const MidiEvent& a, const MidiEvent& b) { return a.tick < b.tick; });

    // The tempo map: at one tick the last set-tempo read wins.
    std::stable_sort(tempos.begin(), tempos.end(),
                     [](const RawTempo& a, const RawTempo& b) { return a.tick < b.tick; });
    tempo_.push_back({0, kDefaultTempo, 0.0});
    for (const RawTempo& t : tempos) {
        if (t.us == 0) continue;
        Tempo& last = tempo_.back();
        if (t.tick == last.tick) {
            last.us_per_quarter = t.us;
            continue;
        }
        const double ms = last.ms + (t.tick - last.tick) * (last.us_per_quarter / 1000.0) / division_;
        tempo_.push_back({t.tick, t.us, ms});
    }
    for (MidiEvent& e : events_) e.ms = tick_to_ms(e.tick);
    return true;
}

double MidiFile::tick_to_ms(std::uint32_t tick) const {
    if (ms_per_tick_smpte_ > 0) return tick * ms_per_tick_smpte_;
    if (tempo_.empty() || division_ <= 0) return 0;
    auto it = std::upper_bound(tempo_.begin(), tempo_.end(), tick,
                               [](std::uint32_t t, const Tempo& x) { return t < x.tick; });
    const Tempo& t = *(it - 1);
    return t.ms + (tick - t.tick) * (t.us_per_quarter / 1000.0) / division_;
}

std::uint32_t MidiFile::ms_to_tick(double ms) const {
    if (ms <= 0) return 0;
    if (ms_per_tick_smpte_ > 0) return static_cast<std::uint32_t>(std::ceil(ms / ms_per_tick_smpte_ - 1e-9));
    if (tempo_.empty() || division_ <= 0) return 0;
    auto it = std::upper_bound(tempo_.begin(), tempo_.end(), ms,
                               [](double m, const Tempo& x) { return m < x.ms; });
    const Tempo& t = *(it - 1);
    const double ticks = (ms - t.ms) * division_ / (t.us_per_quarter / 1000.0);
    return t.tick + static_cast<std::uint32_t>(std::ceil(ticks - 1e-9));
}

}  // namespace is::audio::music
