// Standard MIDI Files, formats 0 and 1, for the port's own MIDI player (ADR 0011 decision 3):
// the header, the tracks merged into one time-ordered event list, the tempo map and the
// tick <-> millisecond conversion [REF Standard MIDI Files 1.0]. Portable: no Win32 calls.
// The four files of the original are format 1, division 480 (docs/re/sound.md "Music").
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace is::audio::music {

struct MidiEvent {
    enum Kind : std::uint8_t {
        kChannel,  // bytes[0..len): status and one or two data bytes
        kSysEx,    // sysex_[offset, offset + size): the whole message, F0 .. F7, or an F7 escape's bytes
    };
    std::uint32_t tick = 0;
    double ms = 0;  // tick_to_ms(tick), filled by the parser
    Kind kind = kChannel;
    std::uint8_t len = 0;
    std::uint8_t bytes[3] = {};
    std::uint32_t offset = 0, size = 0;  // kSysEx
};

class MidiFile {
public:
    // Parses a whole file; false with `why` on a malformed header or track. Unknown chunks are
    // skipped [REF Standard MIDI Files 1.0, "chunks"]. Format 2 is refused (the player plays
    // one sequence) [DESIGN].
    bool parse(const std::uint8_t* data, std::size_t size, std::string* why = nullptr);
    bool load(const std::filesystem::path& path, std::string* why = nullptr);

    int format() const { return format_; }
    int tracks() const { return tracks_; }
    int division() const { return division_; }  // ticks per quarter note, or the raw SMPTE word

    // Every channel and SysEx event of every track, ordered by tick and, at one tick, by track
    // number and then by the order within the track. Meta events are not in the list: the tempo
    // changes are in the tempo map and the end of track sets length_ticks.
    const std::vector<MidiEvent>& events() const { return events_; }
    const std::uint8_t* sysex(const MidiEvent& e) const { return sysex_.data() + e.offset; }

    // The tick of the last end-of-track (or last event) of any track, and its time.
    std::uint32_t length_ticks() const { return length_ticks_; }
    double length_ms() const { return tick_to_ms(length_ticks_); }

    // The tempo map: FF 51 set-tempo metas of every track (format 1 keeps them in track 0, but
    // the map takes them from any track), 500,000 us per quarter note before the first.
    double tick_to_ms(std::uint32_t tick) const;
    // The first tick at or after `ms` (ceil), so tick_to_ms(ms_to_tick(x)) >= x.
    std::uint32_t ms_to_tick(double ms) const;

    struct Tempo {
        std::uint32_t tick;
        std::uint32_t us_per_quarter;
        double ms;  // the time of `tick`
    };
    const std::vector<Tempo>& tempo_map() const { return tempo_; }

private:
    int format_ = 0, tracks_ = 0, division_ = 0;
    std::vector<MidiEvent> events_;
    std::vector<std::uint8_t> sysex_;
    std::vector<Tempo> tempo_;
    std::uint32_t length_ticks_ = 0;
    double ms_per_tick_smpte_ = 0;  // > 0 for an SMPTE division
};

}  // namespace is::audio::music
