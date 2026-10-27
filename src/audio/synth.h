// A MIDI synthesizer behind the port's own MIDI player (ADR 0011 decision 3): takes channel
// messages and renders 16-bit interleaved stereo. Portable: no Win32 calls. The player calls
// midi() and render() from the output thread, open()/reset() from any thread before play.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
namespace is::audio::music {
struct SynthConfig {
    std::filesystem::path soundfont;  // the .sf2 for the soundfont synth; ignored by opl3
    std::uint32_t rate = 22050;       // the output rate render() is called at
};
struct Synth {
    virtual ~Synth();
    virtual bool open(const SynthConfig&) = 0;     // false: not usable (no file, bad file)
    virtual void reset() = 0;                      // GM reset: all notes off, controllers reset
    // One channel message (status byte first, 1 to 3 bytes) or one SysEx (F0 .. F7).
    virtual void midi(const std::uint8_t* msg, std::size_t len) = 0;
    // Renders 'frames' stereo frames at the configured rate, REPLACING 'out' (not adding).
    virtual void render(std::int16_t* out, std::size_t frames) = 0;
    virtual std::string name() const = 0;          // "soundfont" or "opl3"
};
// Factories: nullptr when the synth is not built in (opl3 behind the CMake option IS_OPL3).
std::unique_ptr<Synth> make_soundfont();   // music_soundfont.cpp (TinySoundFont)
std::unique_ptr<Synth> make_opl3();        // music_opl3.cpp (Nuked OPL3) or a stub
}  // namespace is::audio::music
