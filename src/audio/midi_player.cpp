// The MIDI file player (midi_player.h): the MCI sequencer's open, seek, play-to, pause and
// close for the synth backends (ADR 0011 decision 3), command for command as ISPACE.DLL sends
// them (docs/re/sound.md "MCI sequence").
#include "audio/midi_player.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace is::audio::music {

namespace {

void log(const std::string& s) {
    const std::string line = "inspace: music: " + s + "\n";
    std::fputs(line.c_str(), stderr);
    std::fflush(stderr);
}

// The messages a seek replays while it fast-forwards: everything but the notes and their
// polyphonic pressure, so the programs, controllers, channel pressure, pitch bend and SysEx
// stand as they would at the target [DESIGN]. A note struck before the target and still held
// there is not struck again, so a section that starts inside held notes opens on silence until
// its next note-on: track 4 (RACE at 34 s) 764 ms, track 11 (ANTHEM at 181 s) 1797 ms, and
// track 5 (ANTHEM at 200 s) none at all. Whether the Windows or Wine sequencer sounds held
// notes after MCI_SEEK is [OPEN] (M7 s2 fixer).
bool chased(const MidiEvent& e) {
    if (e.kind == MidiEvent::kSysEx) return true;
    const std::uint8_t hi = e.bytes[0] & 0xf0;
    return hi != 0x80 && hi != 0x90 && hi != 0xa0;
}

class Adapter final : public Backend {
public:
    explicit Adapter(std::shared_ptr<MidiPlayer> p) : p_(std::move(p)) {}
    bool open(const std::filesystem::path& m) override { return p_->open(m); }
    bool seek_ms(unsigned ms, bool wait) override { return p_->seek_ms(ms, wait); }
    bool play_to_ms(unsigned ms) override { return p_->play_to_ms(ms); }
    void pause() override { p_->pause(); }
    void resume() override { p_->resume(); }
    void close() override { p_->close(); }
    void set_volume(int percent) override { p_->set_volume(percent); }

private:
    std::shared_ptr<MidiPlayer> p_;
};

}  // namespace

std::unique_ptr<Backend> backend_of(std::shared_ptr<MidiPlayer> player) {
    if (!player) return nullptr;
    return std::make_unique<Adapter>(std::move(player));
}

MidiPlayer::MidiPlayer(std::unique_ptr<Synth> synth, Notify notify)
    : synth_(std::move(synth)), notify_(std::move(notify)) {}

MidiPlayer::~MidiPlayer() = default;

// ---------------------------------------------------------------- message thread

void MidiPlayer::queue(Command c) {
    c.epoch = ++epoch_;
    // Every notify posted before this command is superseded (MCI_NOTIFY_SUPERSEDED or
    // MCI_NOTIFY_ABORTED in MCI's terms).
    cancel_upto_ = posted_.load();
    std::lock_guard<std::mutex> lock(cmd_mu_);
    cmds_.push_back(std::move(c));
}

// Sequence numbers are 31-bit and compared modulo 2^31, so the wrap after 2^31 notifies keeps
// the order (M7 s2 fixer).
bool MidiPlayer::accept_notify(std::uint32_t seq) const {
    const std::uint32_t d = (seq - cancel_upto_.load()) & 0x7fffffffu;
    return d != 0 && d < 0x40000000u;
}

// MCI_OPEN of the file (seg9:0x37ea): the file is read and parsed here, so a missing or broken
// file fails the open as MCI_OPEN would [BYTES ISPACE.DLL seg9:0x380c].
bool MidiPlayer::open(const std::filesystem::path& midi) {
    auto f = std::make_shared<MidiFile>();
    std::string why;
    if (!f->load(midi, &why)) {
        log("open " + midi.string() + " failed: " + why);
        return false;
    }
    char buf[160];
    std::snprintf(buf, sizeof buf, "open %s: SMF %d, %d tracks, division %d, %.2f s through %s",
                  midi.filename().string().c_str(), f->format(), f->tracks(), f->division(),
                  f->length_ms() / 1000.0, synth_name().c_str());
    log(buf);
    msg_file_ = f;
    Command c(Op::kOpen);
    c.file = std::move(f);
    queue(std::move(c));
    return true;
}

// MCI_SEEK with MCI_TO and MCI_WAIT or MCI_NOTIFY (seg9:0x3a94). A target past the file's end
// fails, as the sequencer's MCIERR_OUTOFRANGE [DESIGN].
bool MidiPlayer::seek_ms(unsigned ms, bool wait) {
    if (!msg_file_ || ms > msg_file_->length_ms() + 0.5) return false;
    Command c(Op::kSeek);
    c.ms = ms;
    c.notify = !wait;
    queue(std::move(c));
    return true;
}

// MCI_PLAY with MCI_TO and MCI_NOTIFY from the current position (seg9:0x3b5c). An end past
// the file's end plays to the file's end and then notifies: tracks 5 and 8 ask for one, and
// what the sequencer does with it is [OPEN] (docs/re/sound.md "Music").
bool MidiPlayer::play_to_ms(unsigned ms) {
    if (!msg_file_) return false;
    Command c(Op::kPlay);
    c.ms = std::min<double>(ms, msg_file_->length_ms());
    queue(std::move(c));
    return true;
}

// MCI_PAUSE (seg9:0x3a48): the play stops where it is with all notes off; nothing open, it does
// nothing (the sequencer's error for device 0 or 0xFFFF).
void MidiPlayer::pause() {
    if (!msg_file_) return;
    queue(Command(Op::kPause));
}

// Play on from the paused position to the last end. The Player plays again with play_to_ms
// instead, as the original does (music_mci.cpp) [DESIGN].
void MidiPlayer::resume() {
    if (!msg_file_) return;
    queue(Command(Op::kResume));
}

// MCI_CLOSE (seg9:0x3a16).
void MidiPlayer::close() {
    msg_file_.reset();
    queue(Command(Op::kClose));
}

void MidiPlayer::set_volume(int percent) {
    if (percent == kSystemVolume) return;  // no system volume to hand back [DESIGN]
    volume_ = std::clamp(percent, 0, 100);
}

// ---------------------------------------------------------------- output thread

double MidiPlayer::position_ms() const { return pub_pos_ms_.load(); }
bool MidiPlayer::playing() const { return pub_playing_.load(); }

void MidiPlayer::post(std::uint64_t epoch) {
    // A command queued after the one this notify ends has replaced it.
    if (epoch != epoch_.load()) return;
    std::uint32_t seq = (posted_.load() + 1) & 0x7fffffffu;
    if (seq == 0) seq = 1;
    posted_ = seq;
    if (notify_) notify_(1, seq);  // MCI_NOTIFY_SUCCESSFUL
}

void MidiPlayer::all_notes_off() {
    if (!synth_) return;
    for (std::uint8_t ch = 0; ch < 16; ++ch) {
        const std::uint8_t m[3] = {static_cast<std::uint8_t>(0xb0 | ch), 123, 0};  // All Notes Off
        synth_->midi(m, 3);
    }
}

void MidiPlayer::send(const MidiEvent& e) {
    if (!synth_) return;
    if (e.kind == MidiEvent::kSysEx)
        synth_->midi(file_->sysex(e), e.size);
    else
        synth_->midi(e.bytes, e.len);
}

void MidiPlayer::apply(const Command& c) {
    switch (c.op) {
    case Op::kOpen:
        file_ = c.file;
        if (synth_) synth_->reset();
        cursor_ = 0;
        pos_ms_ = 0;
        playing_ = has_play_ = false;
        break;
    case Op::kClose:
        file_.reset();
        if (synth_) synth_->reset();
        cursor_ = 0;
        pos_ms_ = 0;
        playing_ = has_play_ = false;
        break;
    case Op::kSeek: {
        playing_ = false;
        if (!file_) break;
        // From a reset synth, every chased message before the target, in order; the notes
        // stay silent [DESIGN].
        if (synth_) synth_->reset();
        const auto& ev = file_->events();
        std::size_t i = 0;
        for (; i < ev.size() && ev[i].ms < c.ms; ++i)
            if (chased(ev[i])) send(ev[i]);
        cursor_ = i;
        pos_ms_ = c.ms;
        if (c.notify) post(c.epoch);
        break;
    }
    case Op::kPlay:
        if (!file_) break;
        end_ms_ = c.ms;
        playing_ = has_play_ = true;
        play_epoch_ = c.epoch;
        break;
    case Op::kPause:
        if (playing_) all_notes_off();
        playing_ = false;
        break;
    case Op::kResume:
        if (file_ && has_play_) {
            playing_ = true;
            play_epoch_ = c.epoch;
        }
        break;
    }
}

void MidiPlayer::render_music(std::int16_t* out, std::size_t frames, std::uint32_t rate) {
    work_.clear();
    {
        std::lock_guard<std::mutex> lock(cmd_mu_);
        work_.swap(cmds_);
    }
    for (const Command& c : work_) apply(c);

    if (!synth_ || rate == 0) {
        std::fill(out, out + frames * 2, std::int16_t{0});
        return;
    }
    const double ms_per_frame = 1000.0 / rate;
    std::size_t done = 0;
    while (done < frames) {
        std::size_t n = frames - done;
        if (playing_ && file_) {
            const auto& ev = file_->events();
            // The events due at the current position, up to (not at) the end of the play.
            while (cursor_ < ev.size() && ev[cursor_].ms <= pos_ms_ + 1e-9 && ev[cursor_].ms < end_ms_)
                send(ev[cursor_++]);
            if (pos_ms_ + 1e-9 >= end_ms_) {
                // The play's end: the sequencer stops there and notifies (seg9:0x2ac4's
                // MM_MCINOTIFY); the notes still sounding are released [DESIGN].
                all_notes_off();
                playing_ = false;
                post(play_epoch_);
                continue;
            }
            double next = end_ms_;
            if (cursor_ < ev.size()) next = std::min(next, ev[cursor_].ms);
            const double until = std::ceil((next - pos_ms_) / ms_per_frame - 1e-9);
            n = std::min<std::size_t>(n, static_cast<std::size_t>(std::max(1.0, until)));
        }
        synth_->render(out + 2 * done, n);
        if (playing_) pos_ms_ += n * ms_per_frame;
        done += n;
    }
    pub_pos_ms_ = pos_ms_;
    pub_playing_ = playing_;

    // The music volume as linear amplitude, as the effects' volume words (ADR 0011 decision 2).
    const int v = volume_.load();
    if (v >= 100) return;
    for (std::size_t i = 0; i < frames * 2; ++i)
        out[i] = static_cast<std::int16_t>(out[i] * v / 100);
}

}  // namespace is::audio::music
