// The port's own MIDI file player (ADR 0011 decision 3, `soundfont` and `opl3`): a
// music::Backend over a Synth whose stream the mixer's output thread pulls as a MusicSource.
// It stands in for the MCI sequencer that ISPACE.DLL drives (docs/re/sound.md "MCI sequence"):
// one file open at a time, a seek to a millisecond, a play to an end millisecond from the
// current position with a notify at the end, a pause, a close. Portable: no Win32 calls.
//
// Threads and locking [DESIGN]:
// - The message thread calls the Backend functions. Each one validates what it can (a file is
//   open, the seek is inside the file), queues a command under cmd_mu_ and returns at once; no
//   Backend call waits for the output thread, and cmd_mu_ is never held across a synth call.
// - The output thread calls render_music(). It takes the queued commands under cmd_mu_, lets go
//   of the lock, applies them (the synth reset and the seek's fast-forward run here, so only
//   this thread ever touches the synth after construction), then renders. The playing cursor,
//   the position and the synth are this thread's alone.
// - The volume is an atomic, read once per block.
// - The notify callback runs on the output thread when a notify seek has been applied and when
//   a play reaches its end; audio.cpp's glue posts MM_MCINOTIFY (wParam 1) to the main window.
//   A notify whose command a later command has replaced is not posted, and accept_notify() lets
//   the receiver drop one that was already posted when a later command came (MCI's
//   MCI_NOTIFY_SUPERSEDED and MCI_NOTIFY_ABORTED, which Player::notify ignores as wParam != 1).
//   Each notify carries its sequence number (1, 2, ...), which the glue posts in lParam, so a
//   lost or stale post never shifts the check of the later ones (M7 s2 fixer).
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio/midi_file.h"
#include "audio/mixer.h"
#include "audio/music.h"
#include "audio/music_tracks.h"
#include "audio/synth.h"

namespace is::audio::music {

class MidiPlayer final : public Backend, public MusicSource {
public:
    using Notify = std::function<void(unsigned wparam, std::uint32_t seq)>;

    // `synth` is open at the output rate; `notify` receives MCI_NOTIFY_SUCCESSFUL (1) and the
    // notify's sequence number (below 0x80000000; it wraps there).
    MidiPlayer(std::unique_ptr<Synth> synth, Notify notify);
    ~MidiPlayer() override;

    // ---- Backend (message thread)
    bool open(const std::filesystem::path& midi) override;
    bool seek_ms(unsigned ms, bool wait) override;
    bool play_to_ms(unsigned ms) override;
    void pause() override;
    void resume() override;
    void close() override;
    void set_volume(int percent) override;  // linear amplitude; kSystemVolume is ignored

    // The message thread's check of a delivered notify by its sequence number: false when a
    // command came after it was posted (M7 s2 fixer).
    bool accept_notify(std::uint32_t seq) const;

    // ---- MusicSource (output thread)
    void render_music(std::int16_t* out, std::size_t frames, std::uint32_t rate) override;

    // ---- state, for the tests and the log (output thread's values)
    double position_ms() const;
    bool playing() const;
    std::string synth_name() const { return synth_ ? synth_->name() : std::string(); }

private:
    enum class Op { kOpen, kClose, kSeek, kPlay, kPause, kResume };
    struct Command {
        explicit Command(Op o) : op(o) {}
        Op op;
        std::shared_ptr<const MidiFile> file;  // kOpen
        double ms = 0;                         // kSeek target, kPlay end
        bool notify = false;                   // kSeek without wait
        std::uint64_t epoch = 0;
    };
    void queue(Command c);
    void apply(const Command& c);
    void post(std::uint64_t epoch);
    void all_notes_off();
    void send(const MidiEvent& e);

    std::unique_ptr<Synth> synth_;
    Notify notify_;

    // message thread
    std::shared_ptr<const MidiFile> msg_file_;

    // shared
    std::mutex cmd_mu_;
    std::vector<Command> cmds_;
    std::atomic<std::uint64_t> epoch_{0};      // bumped by every Backend command
    std::atomic<std::uint32_t> posted_{0};       // the last notify's sequence number
    std::atomic<std::uint32_t> cancel_upto_{0};  // posted_ when the last command came
    std::atomic<int> volume_{100};

    // output thread
    std::atomic<double> pub_pos_ms_{0};  // published copies for the accessors
    std::atomic<bool> pub_playing_{false};
    std::shared_ptr<const MidiFile> file_;
    std::size_t cursor_ = 0;
    double pos_ms_ = 0;
    double end_ms_ = 0;
    bool playing_ = false;
    bool has_play_ = false;
    std::uint64_t play_epoch_ = 0;
    std::vector<Command> work_;
};

// The Backend the Player owns over a player the mixer also holds (Player keeps a unique_ptr,
// the mixer a shared_ptr) [DESIGN].
std::unique_ptr<Backend> backend_of(std::shared_ptr<MidiPlayer> player);

}  // namespace is::audio::music
