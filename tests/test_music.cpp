// The music state machine of ISPACE.DLL (src/audio/music_tracks.h) on a fake backend: the track
// tables and the section change pinned to docs/re/sound.md "Music", the control codes, the
// wait/notify flag, the MM_MCINOTIFY loops and chaining, Music = 0 and SoundSystem 0, the focus
// pause and the music volume formula.
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "audio/music_tracks.h"
#include "doctest.h"

namespace m = is::audio::music;

namespace {

// Records every backend call as one string.
struct Fake final : m::Backend {
    std::vector<std::string>* log;
    bool open_ok = true, seek_ok = true, play_ok = true;
    explicit Fake(std::vector<std::string>* l) : log(l) {}
    bool open(const std::filesystem::path& midi) override {
        const std::string p = midi.string();  // the host has no '\\' separator
        log->push_back("open " + p.substr(p.find_last_of("\\/") + 1));
        return open_ok;
    }
    bool seek_ms(unsigned ms, bool wait) override {
        log->push_back("seek " + std::to_string(ms) + (wait ? " wait" : " notify"));
        return seek_ok;
    }
    bool play_to_ms(unsigned ms) override {
        log->push_back("play " + std::to_string(ms));
        return play_ok;
    }
    void pause() override { log->push_back("pause"); }
    void resume() override { log->push_back("resume"); }
    void close() override { log->push_back("close"); }
    void set_volume(int percent) override { log->push_back("volume " + std::to_string(percent)); }
};

struct Rig {
    std::vector<std::string> calls;
    Fake* fake;
    m::Player player;
    explicit Rig(int sound_system = 1, int music = 1) {
        auto f = std::make_unique<Fake>(&calls);
        fake = f.get();
        player.set_backend(std::move(f));
        m::Config c;
        c.music = music;
        c.sound_system = sound_system;
        c.snd_path = "C:\\IS\\";
        player.configure(c);
    }
    std::vector<std::string> take() { return std::exchange(calls, {}); }
};

using V = std::vector<std::string>;

}  // namespace

TEST_CASE("music: the track table of sound.md") {
    // Track: file, start, length (100 ms) [BYTES ISPACE.DLL DGROUP:0xaa8 DGROUP:0xac0 DGROUP:0xad8]
    const int expect[12][3] = {{2, 0, 1810},    {0, 45, 1495},   {0, 1700, 1170}, {1, 0, 290},
                               {1, 340, 2550},  {2, 2000, 166},  {3, 190, 2030},  {0, 2870, 50},
                               {0, 2920, 90},   {3, 0, 89},      {3, 89, 101},    {2, 1810, 190}};
    for (int t = 0; t < 12; ++t) {
        CAPTURE(t);
        CHECK(m::kTracks[t].file == expect[t][0]);
        CHECK(m::kTracks[t].start == expect[t][1]);
        CHECK(m::kTracks[t].length == expect[t][2]);
    }
    CHECK(m::file_stem(0) == "main");
    CHECK(m::file_stem(1) == "race");
    CHECK(m::file_stem(2) == "anthem");
    CHECK(m::file_stem(3) == "demon");
    CHECK(m::end_ms(5) == 216600);  // past ANTHEM's 200.80 s [OPEN]
    CHECK(m::end_ms(8) == 301000);
    CHECK(m::end_ms(0) == 181000);
}

TEST_CASE("music: the section change between tracks 1 and 2") {
    CHECK(m::seek_target_ms(1, -1) == 4500);
    CHECK(m::seek_target_ms(1, 2) == 0);         // from 0 when entered from track 2
    CHECK(m::seek_target_ms(2, 1) == 155000);    // from 155 s when entered from track 1
    CHECK(m::seek_target_ms(2, 7) == 170000);    // 7 continues as 2 from 170 s
    CHECK(m::seek_target_ms(2, 2) == 170000);
    CHECK(m::seek_target_ms(1, -2) == 4500);     // after a pause the previous is -2
    CHECK(m::seek_target_ms(4, 3) == 34000);
    CHECK(m::seek_target_ms(-4, 0) == 170000);   // DGROUP:0xaac = 1700
}

TEST_CASE("music: the file names under SndPath") {
    CHECK(m::midi_path("C:\\IS\\", 0).native() == m::midi_path("C:\\IS", 0).native());
    const auto p = m::midi_path("", 2);
    CHECK(p.native().size() == std::string("\\anthem.mid").size());
}

TEST_CASE("music: the volume formula") {
    CHECK(m::music_level(100, 50) == 100);
    CHECK(m::music_level(100, 80) == 100);
    CHECK(m::music_level(100, 25) == 50);
    CHECK(m::music_level(80, 10) == 16);
    CHECK(m::music_level(100, 0) == 0);
    CHECK(m::music_volume(100) == 95);
    CHECK(m::music_volume(50) == 47);
    CHECK(m::music_volume(200) == 100);  // the clamp
    CHECK(m::music_volume(0) == 0);
    CHECK(m::volume_dword(95) == 0xF332F332u);
    CHECK(m::volume_dword(100) == 0xFFFFFFFFu);
    CHECK(m::config_from(1, -1, 100, 50, "").level == 100);
    CHECK(m::config_from(1, -1, 40, 50, "").level == 40);
    CHECK(m::config_from(1, -1, 100, 25, "").level == 50);
    CHECK(m::config_from(1, -1, 100, 50, "").sound_system == 3);
    CHECK(m::config_from(1, 7, 100, 50, "").sound_system == 3);
    CHECK(m::config_from(1, 1, 100, 50, "").sound_system == 1);
}

TEST_CASE("music: start track 2 from nothing") {
    Rig r;
    r.player.request(2, 1);
    CHECK(r.take() == V{"close", "open main.mid", "seek 170000 wait", "play 287000"});
    CHECK(r.player.state() == m::State::kPlaying);
    CHECK(r.player.previous() == 2);
    CHECK(r.player.file() == 0);
}

TEST_CASE("music: the title track 0 with the notify flag still seeks with MCI_WAIT") {
    Rig r;
    r.player.request(0, 0);  // a new file always seeks with flag 1 (seg9:0x38d3)
    CHECK(r.take() == V{"close", "open anthem.mid", "seek 0 wait", "play 181000"});
}

TEST_CASE("music: 1 -> 2 and 2 -> 1 stay in the open file") {
    Rig r;
    r.player.request(1, 1);
    CHECK(r.take() == V{"close", "open main.mid", "seek 4500 wait", "play 154000"});
    r.player.request(2, 1);
    CHECK(r.take() == V{"seek 155000 wait", "play 287000"});
    r.player.request(1, 0);  // the selector's (track, 0): play at the seek's notify
    CHECK(r.take() == V{"seek 0 notify"});
    CHECK(r.player.state() == m::State::kSeeked);
    r.player.notify(1);
    CHECK(r.take() == V{"play 154000"});
    CHECK(r.player.state() == m::State::kPlaying);
}

TEST_CASE("music: a track in another file closes and opens") {
    Rig r;
    r.player.request(1, 1);
    r.take();
    r.player.request(3, 1);
    CHECK(r.take() == V{"close", "open race.mid", "seek 0 wait", "play 29000"});
}

TEST_CASE("music: -2 then -3") {
    Rig r;
    r.player.request(6, 1);
    r.take();
    r.player.request(-2, 1);
    CHECK(r.take() == V{"pause"});
    CHECK(r.player.state() == m::State::kPaused);
    CHECK(r.player.previous() == -2);
    CHECK(r.player.resume_on_focus());
    r.player.request(-3, 1);
    CHECK(r.take() == V{"play 222000"});  // no MCI_FROM: from the paused position
    CHECK(r.player.state() == m::State::kPlaying);
    // -2 pauses only a playing track; -3 does nothing in state 1.
    r.player.request(-3, 1);
    CHECK(r.take().empty());
    r.player.request(-2, 1);
    r.player.request(-2, 1);
    CHECK(r.take() == V{"pause"});
}

TEST_CASE("music: -1 is a hard pause that only a track undoes") {
    Rig r;
    r.player.request(2, 1);
    r.take();
    r.player.request(-1, 1);
    CHECK(r.take() == V{"pause"});
    CHECK(r.player.state() == m::State::kHardPaused);
    for (int c : {-1, -2, -3, -5}) r.player.request(c, 1);
    CHECK(r.take().empty());
    // The same file in state 4: a re-seek; previous is -1, so no section change.
    r.player.request(1, 1);
    CHECK(r.take() == V{"seek 4500 wait", "play 154000"});
    r.player.request(-1, 1);
    r.take();
    r.player.request(3, 1);
    CHECK(r.take() == V{"close", "open race.mid", "seek 0 wait", "play 29000"});
}

TEST_CASE("music: -1 before anything still pauses (device 0)") {
    Rig r;
    r.player.request(-1, 1);
    CHECK(r.take() == V{"pause"});
    CHECK(r.player.state() == m::State::kHardPaused);
    r.player.request(-5, 1);
    CHECK(r.take().empty());
}

TEST_CASE("music: the notify loops, chains 7 and 8 into 2 and stops 5 and 11") {
    Rig r;
    r.player.request(7, 1);
    CHECK(r.take() == V{"close", "open main.mid", "seek 287000 wait", "play 292000"});
    r.player.notify(2);  // MCI_NOTIFY_SUPERSEDED: ignored
    r.player.notify(4);
    CHECK(r.take().empty());
    r.player.notify(1);
    CHECK(r.player.track() == 2);
    CHECK(r.take() == V{"seek 170000 notify"});
    r.player.notify(1);
    CHECK(r.take() == V{"play 287000"});
    r.player.notify(1);  // track 2 loops to its start
    CHECK(r.take() == V{"seek 170000 notify"});

    Rig e;
    e.player.request(5, 1);
    CHECK(e.take() == V{"close", "open anthem.mid", "seek 200000 wait", "play 216600"});
    e.player.notify(1);
    CHECK(e.take().empty());
    CHECK(e.player.state() == m::State::kPlaying);
    e.player.request(11, 1);
    e.take();
    e.player.notify(1);
    CHECK(e.take().empty());

    Rig l;
    l.player.request(0, 1);
    l.take();
    l.player.notify(1);
    CHECK(l.take() == V{"seek 0 notify"});
}

TEST_CASE("music: Music = 0 and SoundSystem 0 only store the track") {
    Rig r(1, 0);
    r.player.request(4, 1);
    CHECK(r.take().empty());
    CHECK(r.player.track() == 4);
    Rig z(0, 1);
    z.player.request(4, 1);
    CHECK(z.take().empty());
    // The control codes still run: -1 sends MCI_PAUSE whatever the settings.
    z.player.request(-1, 1);
    CHECK(z.take() == V{"pause"});
    // Options turning the sound system on restarts the stored track.
    m::Config before = z.player.config(), after = before;
    after.sound_system = 2;
    Rig o(0, 1);
    o.player.request(4, 1);
    o.player.restart_after_options(before, after);
    CHECK(o.take() == V{"volume 95", "close", "open race.mid", "seek 34000 wait", "play 289000"});
}

TEST_CASE("music: a play with Music turned off closes") {
    Rig r;
    r.player.request(6, 1);
    r.player.request(-2, 1);
    r.take();
    m::Config c = r.player.config();
    c.music = 0;
    r.player.configure(c);
    r.player.request(-3, 1);
    CHECK(r.take() == V{"close"});
    CHECK(r.player.state() == m::State::kClosed);
}

TEST_CASE("music: the failure modes") {
    Rig r;
    r.fake->open_ok = false;
    r.player.request(1, 1);
    CHECK(r.take() == V{"close", "open main.mid"});
    CHECK(r.player.state() == m::State::kClosed);
    // A failed waiting seek plays nothing and leaves state 3.
    r.fake->open_ok = true;
    r.fake->seek_ok = false;
    r.player.request(1, 1);
    CHECK(r.take() == V{"close", "open main.mid", "seek 4500 wait"});
    CHECK(r.player.state() == m::State::kSeeked);
    // A failed play closes.
    r.fake->seek_ok = true;
    r.fake->play_ok = false;
    r.player.request(-3, 1);
    CHECK(r.take() == V{"play 154000", "close"});
    CHECK(r.player.state() == m::State::kClosed);
}

TEST_CASE("music: the volume is sent above STANDARD and only when it changes") {
    Rig s1(1);
    s1.player.request(0, 1);
    CHECK(s1.take().front() == "close");  // STANDARD: no volume
    Rig r(3);
    r.player.request(0, 1);
    CHECK(r.take() == V{"volume 95", "close", "open anthem.mid", "seek 0 wait", "play 181000"});
    r.player.refresh_volume();
    CHECK(r.take().empty());
    m::Config c = r.player.config();
    c.level = 50;
    r.player.configure(c);
    r.player.refresh_volume();
    CHECK(r.take() == V{"volume 47"});
}

TEST_CASE("music: focus pauses a playing track and resumes it") {
    Rig r(3);
    r.player.request(6, 1);
    r.take();
    r.player.focus_lost();
    CHECK(r.take() == V{"pause", "volume -1"});
    CHECK(r.player.state() == m::State::kPaused);
    r.player.focus_gained();
    CHECK(r.take() == V{"volume 95", "play 222000"});
    // A paused track (-2) is not resumed by the focus: the flag clears at every pause.
    r.player.request(-2, 1);
    r.player.focus_lost();
    r.take();
    r.player.focus_gained();
    CHECK(r.take() == V{"volume 95"});
    // Sound system 0: nothing.
    Rig z(0);
    z.player.focus_lost();
    z.player.focus_gained();
    CHECK(z.take().empty());
}
