// The music backends behind one interface (ADR 0011 decision 3). The track tables, sections,
// control codes and notify handling sit above this interface; a backend only opens, seeks,
// plays, pauses and closes one MIDI file.
#pragma once

#include <filesystem>
#include <memory>

namespace is::audio::music {

struct Backend {
    virtual ~Backend();
    virtual bool open(const std::filesystem::path& midi) = 0;
    virtual bool seek_ms(unsigned ms, bool wait) = 0;
    virtual bool play_to_ms(unsigned ms) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void close() = 0;
    virtual void set_volume(int percent) = 0;
};

// The MCI sequencer backend (music_mci.cpp); hwnd_notify receives MM_MCINOTIFY.
std::unique_ptr<Backend> make_mci(void* hwnd_notify);

}  // namespace is::audio::music
