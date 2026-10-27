// The port's MIDI player (src/audio/midi_file.h, midi_player.h) on a fake synth: the SMF
// parser and tempo map on a hand-built file [REF Standard MIDI Files 1.0], the seek
// fast-forward, the play to an end and its single notify, the pause, the volume, the
// superseded notify, and the mixer's music stream. With IS_ORIGINAL_DIR: the four MIDI files of
// the ship disk and the twelve track sections of docs/re/sound.md "Music".
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "audio/midi_file.h"
#include "audio/midi_player.h"
#include "audio/mixer.h"
#include "audio/music_tracks.h"
#include "audio/synth.h"
#include "doctest.h"

namespace m = is::audio::music;
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;

namespace {

constexpr std::int16_t kTone = 1000;

// Records every message and renders a constant `kTone` on both sides.
struct FakeSynth final : m::Synth {
    std::vector<Bytes>* msgs;
    int* resets;
    FakeSynth(std::vector<Bytes>* m, int* r) : msgs(m), resets(r) {}
    bool open(const m::SynthConfig&) override { return true; }
    void reset() override { ++*resets; }
    void midi(const std::uint8_t* msg, std::size_t len) override { msgs->emplace_back(msg, msg + len); }
    void render(std::int16_t* out, std::size_t frames) override {
        for (std::size_t i = 0; i < frames * 2; ++i) out[i] = kTone;
    }
    std::string name() const override { return "fake"; }
};

void be32(Bytes& b, std::uint32_t v) {
    for (int i = 3; i >= 0; --i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}
void chunk(Bytes& out, const char* id, const Bytes& body) {
    out.insert(out.end(), id, id + 4);
    be32(out, static_cast<std::uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
}

// Format 1, division 96. Track 0: tempo 500,000 us at tick 0 and 250,000 at tick 192 (1000 ms).
// Track 1: program 5 and a note at 0, its note-off at 96 (500 ms, running status), CC 7 at 96,
// pitch bend at 192, a SysEx at 240 (1125 ms), a note at 288 (1250 ms) ending at 384 (1500 ms).
Bytes tiny_smf() {
    Bytes f;
    chunk(f, "MThd", {0, 1, 0, 2, 0, 96});
    chunk(f, "MTrk", {0x00, 0xff, 0x51, 3, 0x07, 0xa1, 0x20,   //
                      0x81, 0x40, 0xff, 0x51, 3, 0x03, 0xd0, 0x90,  // delta 192
                      0x00, 0xff, 0x2f, 0x00});
    chunk(f, "MTrk", {0x00, 0xc0, 0x05,               // program 5
                      0x00, 0x90, 0x3c, 0x64,         // note on
                      0x60, 0x3c, 0x00,               // delta 96, running status: note off
                      0x00, 0xb0, 0x07, 0x64,         // CC 7
                      0x60, 0xe0, 0x00, 0x40,         // delta 96: pitch bend
                      0x30, 0xf0, 0x03, 0x7e, 0x7f, 0xf7,  // delta 48: SysEx
                      0x30, 0x90, 0x40, 0x64,         // delta 48: note on at 288
                      0x60, 0x80, 0x40, 0x00,         // delta 96: note off at 384
                      0x00, 0xff, 0x2f, 0x00});
    return f;
}

fs::path scratch_file(const std::string& name, const Bytes& b) {
    const fs::path p = fs::temp_directory_path() / name;
    std::FILE* f = std::fopen(p.string().c_str(), "wb");
    REQUIRE(f);
    std::fwrite(b.data(), 1, b.size(), f);
    std::fclose(f);
    return p;
}

struct Rig {
    std::vector<Bytes> msgs;
    int resets = 0;
    int notifies = 0;
    std::uint32_t last_seq = 0;
    std::shared_ptr<m::MidiPlayer> p;
    fs::path file;
    static constexpr std::uint32_t kRate = 1000;  // one frame per millisecond
    Rig() {
        p = std::make_shared<m::MidiPlayer>(std::make_unique<FakeSynth>(&msgs, &resets),
                                            [this](unsigned w, std::uint32_t seq) {
                                                notifies += w == 1;
                                                last_seq = seq;
                                            });
        // A name of this process's own, so parallel test runs never share it (M7 s2 fixer).
        file = scratch_file("is_test_midi_tiny_" + std::to_string(std::random_device{}()) + "_" +
                                std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".mid",
                            tiny_smf());
    }
    ~Rig() {
        std::error_code ec;
        fs::remove(file, ec);
    }
    std::vector<std::int16_t> render(std::size_t frames) {
        std::vector<std::int16_t> out(frames * 2);
        p->render_music(out.data(), frames, kRate);
        return out;
    }
    void run(std::size_t ms, std::size_t block = 23) {
        for (std::size_t t = 0; t < ms; t += block) render(block);
    }
    int count(std::uint8_t status_hi) const {
        int n = 0;
        for (const auto& x : msgs) n += !x.empty() && (x[0] & 0xf0) == status_hi && x[0] < 0xf0;
        return n;
    }
    int note_ons() const {
        int n = 0;
        for (const auto& x : msgs) n += x.size() == 3 && (x[0] & 0xf0) == 0x90 && x[2] != 0;
        return n;
    }
};

const char* env(const char* name) {
    const char* v = std::getenv(name);
    return v && *v ? v : nullptr;
}

// A file of `dir` by name, ignoring case (the ship disk's names are upper case).
fs::path find_ci(const fs::path& dir, const std::string& name) {
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        std::string a = e.path().filename().string(), b = name;
        if (a.size() != b.size()) continue;
        bool same = true;
        for (std::size_t i = 0; i < a.size() && same; ++i)
            same = std::tolower(static_cast<unsigned char>(a[i])) == std::tolower(static_cast<unsigned char>(b[i]));
        if (same) return e.path();
    }
    return {};
}

}  // namespace

TEST_CASE("midi: the SMF parser merges the tracks and keeps the tempo map") {
    const Bytes b = tiny_smf();
    m::MidiFile f;
    std::string why;
    REQUIRE(f.parse(b.data(), b.size(), &why));
    CHECK(f.format() == 1);
    CHECK(f.tracks() == 2);
    CHECK(f.division() == 96);
    REQUIRE(f.events().size() == 8);
    CHECK(f.events()[0].bytes[0] == 0xc0);
    CHECK(f.events()[2].tick == 96);
    CHECK(f.events()[2].bytes[0] == 0x90);  // running status
    CHECK(f.events()[2].bytes[2] == 0);
    CHECK(f.events()[5].kind == m::MidiEvent::kSysEx);
    CHECK(f.events()[5].size == 4);  // F0 7E 7F F7
    CHECK(f.sysex(f.events()[5])[0] == 0xf0);
    CHECK(f.tempo_map().size() == 2);
    CHECK(f.tick_to_ms(96) == doctest::Approx(500));
    CHECK(f.tick_to_ms(192) == doctest::Approx(1000));
    CHECK(f.tick_to_ms(288) == doctest::Approx(1250));
    CHECK(f.events()[6].ms == doctest::Approx(1250));
    CHECK(f.length_ticks() == 384);
    CHECK(f.length_ms() == doctest::Approx(1500));
    CHECK(f.ms_to_tick(1250) == 288);
    CHECK(f.ms_to_tick(600) == 116);  // ceil(115.2)
    CHECK(f.ms_to_tick(1125) == 240);
}

// A dual-mode file (M7 s2b fixer): Microsoft's FF 7F 03 00 00 41 in track 0, notes on MIDI
// channels 1, 10, 13 and 16. The player plays channels 1-10 only; without the marker, all.
Bytes dual_smf(bool marked) {
    Bytes f;
    chunk(f, "MThd", {0, 1, 0, 2, 0, 96});
    Bytes t0 = {0x00, 0xff, 0x51, 3, 0x07, 0xa1, 0x20};
    if (marked) t0.insert(t0.end(), {0x00, 0xff, 0x7f, 0x03, 0x00, 0x00, 0x41});
    t0.insert(t0.end(), {0x00, 0xff, 0x2f, 0x00});
    chunk(f, "MTrk", t0);
    chunk(f, "MTrk", {0x00, 0x90, 60, 100, 0x00, 0x99, 36, 100, 0x00, 0x9c, 60, 100,
                      0x00, 0x9f, 36, 100, 0x00, 0xbf, 7, 100, 0x60, 0x80, 60, 0,
                      0x00, 0x89, 36, 0, 0x00, 0x8c, 60, 0, 0x00, 0x8f, 36, 0,
                      0x60, 0xff, 0x2f, 0x00});  // the end at 1000 ms, after the note offs
    return f;
}

TEST_CASE("midi: a Microsoft-authored dual-mode file plays channels 1-10 only (M7 s2b fixer)") {
    for (bool marked : {true, false}) {
        CAPTURE(marked);
        const Bytes b = dual_smf(marked);
        m::MidiFile f;
        REQUIRE(f.parse(b.data(), b.size()));
        CHECK(f.microsoft_authored() == marked);
        int skipped = 0;
        for (const auto& e : f.events()) skipped += m::dual_mode_skipped(f, e);
        CHECK(skipped == (marked ? 5 : 0));

        Rig r;
        const fs::path p = scratch_file("is_test_midi_dual_" + std::to_string(std::random_device{}()) +
                                            "_" + std::to_string(marked) + ".mid",
                                        b);
        REQUIRE(r.p->open(p));
        REQUIRE(r.p->seek_ms(0, true));
        REQUIRE(r.p->play_to_ms(1000));
        r.run(1100);
        std::error_code ec;
        fs::remove(p, ec);
        int high = 0;
        for (const auto& x : r.msgs)
            if ((x[0] & 0xf0) != 0xb0 || x[1] != 123) high += (x[0] & 0x0f) >= 10;  // not the all notes off
        CHECK(r.note_ons() == (marked ? 2 : 4));
        CHECK(high == (marked ? 0 : 5));
    }
}

TEST_CASE("midi: malformed files are refused") {
    Bytes b = tiny_smf();
    m::MidiFile f;
    CHECK_FALSE(f.parse(b.data(), 10));
    Bytes cut(b.begin(), b.end() - 6);
    CHECK_FALSE(f.parse(cut.data(), cut.size()));
    Bytes fmt2 = b;
    fmt2[9] = 2;
    CHECK_FALSE(f.parse(fmt2.data(), fmt2.size()));
}

TEST_CASE("midi: a seek fast-forwards the controllers silently and a notify seek notifies") {
    Rig r;
    REQUIRE(r.p->open(r.file));
    CHECK(r.p->seek_ms(1200, false));
    r.render(10);
    CHECK(r.notifies == 1);
    CHECK(r.resets >= 1);
    CHECK(r.count(0xc0) == 1);  // program 5
    CHECK(r.count(0xb0) == 1);  // CC 7
    CHECK(r.count(0xe0) == 1);  // pitch bend
    CHECK(r.note_ons() == 0);
    CHECK(r.msgs.size() == 4);  // and the SysEx
    CHECK_FALSE(r.p->playing());
    CHECK(r.p->position_ms() == doctest::Approx(1200));
    // A waiting seek posts nothing.
    CHECK(r.p->seek_ms(0, true));
    r.render(10);
    CHECK(r.notifies == 1);
    // Past the file's end fails, as MCIERR_OUTOFRANGE.
    CHECK_FALSE(r.p->seek_ms(1600, true));
}

TEST_CASE("midi: play_to_ms plays in real time, stops at the end and notifies once") {
    Rig r;
    REQUIRE(r.p->open(r.file));
    REQUIRE(r.p->seek_ms(0, true));
    REQUIRE(r.p->play_to_ms(1200));
    r.run(1100);
    CHECK(r.notifies == 0);
    CHECK(r.p->playing());
    CHECK(r.note_ons() == 1);
    r.run(400);
    CHECK(r.notifies == 1);
    CHECK_FALSE(r.p->playing());
    CHECK(r.note_ons() == 1);  // the note at 1250 ms lies past the end
    CHECK(r.p->position_ms() == doctest::Approx(1200).epsilon(0.001));
    r.run(1000);
    CHECK(r.notifies == 1);
    // The end: all notes off on the sixteen channels.
    int ano = 0;
    for (const auto& x : r.msgs) ano += x.size() == 3 && (x[0] & 0xf0) == 0xb0 && x[1] == 123;
    CHECK(ano == 16);
}

TEST_CASE("midi: an end past the file plays to the file's end, then notifies (tracks 5 and 8)") {
    Rig r;
    REQUIRE(r.p->open(r.file));
    REQUIRE(r.p->seek_ms(1000, true));
    REQUIRE(r.p->play_to_ms(5000));
    r.run(400);
    CHECK(r.notifies == 0);
    r.run(200);
    CHECK(r.notifies == 1);
    CHECK(r.note_ons() == 1);  // the note at 1250 ms
    CHECK(r.p->position_ms() == doctest::Approx(1500).epsilon(0.001));
}

TEST_CASE("midi: pause sends all notes off and the play resumes from there") {
    Rig r;
    REQUIRE(r.p->open(r.file));
    REQUIRE(r.p->seek_ms(0, true));
    REQUIRE(r.p->play_to_ms(1500));
    r.run(300);
    r.msgs.clear();
    r.p->pause();
    r.render(10);
    REQUIRE(r.msgs.size() == 16);
    for (int ch = 0; ch < 16; ++ch) {
        CHECK(r.msgs[static_cast<std::size_t>(ch)] == Bytes{static_cast<std::uint8_t>(0xb0 | ch), 123, 0});
    }
    const double at = r.p->position_ms();
    r.run(500);
    CHECK(r.p->position_ms() == doctest::Approx(at));  // a pause holds the position
    CHECK(r.notifies == 0);                            // and aborts the play's notify
    // The Player plays again with play_to_ms from the paused position (seg9:0x3b5c).
    REQUIRE(r.p->play_to_ms(1500));
    r.run(1400);
    CHECK(r.notifies == 1);
}

TEST_CASE("midi: a later command supersedes a pending notify") {
    Rig r;
    REQUIRE(r.p->open(r.file));
    r.p->seek_ms(500, false);
    r.p->seek_ms(0, true);  // before the output thread applied the first
    r.render(10);
    CHECK(r.notifies == 0);
    // One posted and then superseded before its delivery: accept_notify drops it.
    r.p->seek_ms(500, false);
    r.render(10);
    CHECK(r.notifies == 1);
    const std::uint32_t first = r.last_seq;
    CHECK(first == 1);
    CHECK(r.p->accept_notify(first));
    r.p->pause();
    CHECK_FALSE(r.p->accept_notify(first));
    r.p->seek_ms(500, false);
    r.render(10);
    CHECK(r.notifies == 2);
    CHECK(r.last_seq == first + 1);
    CHECK(r.p->accept_notify(r.last_seq));
    // A notify that never arrives does not shift the check of the next one (M7 s2 fixer).
    r.p->seek_ms(0, false);
    r.render(10);
    CHECK(r.notifies == 3);  // seq 3: lost on its way
    r.p->seek_ms(500, false);
    r.render(10);
    CHECK(r.notifies == 4);
    CHECK(r.p->accept_notify(r.last_seq));
    CHECK_FALSE(r.p->accept_notify(r.last_seq - 3));  // an earlier play's notify, stale
}

TEST_CASE("midi: the music volume scales the stream linearly; the system volume is ignored") {
    Rig r;
    REQUIRE(r.p->open(r.file));
    auto out = r.render(4);
    CHECK(out[0] == kTone);
    r.p->set_volume(95);
    out = r.render(4);
    CHECK(out[0] == kTone * 95 / 100);
    CHECK(out[1] == kTone * 95 / 100);
    r.p->set_volume(m::kSystemVolume);
    out = r.render(4);
    CHECK(out[0] == kTone * 95 / 100);
    r.p->set_volume(0);
    out = r.render(4);
    CHECK(out[0] == 0);
}

TEST_CASE("midi: close and a missing file") {
    Rig r;
    CHECK_FALSE(r.p->play_to_ms(100));
    CHECK_FALSE(r.p->open(r.file.parent_path() / "is_no_such_file.mid"));
    REQUIRE(r.p->open(r.file));
    r.p->close();
    CHECK_FALSE(r.p->seek_ms(0, true));
    CHECK_FALSE(r.p->play_to_ms(100));
    r.render(10);
    CHECK_FALSE(r.p->playing());
}

TEST_CASE("midi: the mixer adds the music after the effects' volume and clips") {
    is::audio::Mixer mx;
    mx.configure(1);
    mx.set_volume(0x8000, 0x8000);
    auto wave = std::make_shared<is::audio::Pcm>(100, std::uint8_t{228});  // +100 about 128
    REQUIRE(mx.play(0, wave, 1));
    std::vector<Bytes> msgs;
    int resets = 0;
    auto p = std::make_shared<m::MidiPlayer>(std::make_unique<FakeSynth>(&msgs, &resets), nullptr);
    mx.set_music(p);
    std::vector<std::int16_t> out(8);
    mx.render(out.data(), 4, is::audio::kEffectRate);
    const int effect = 100 * 256 * 0x8000 / 0xFFFF;
    CHECK(out[0] == effect + kTone);
    CHECK(out[1] == effect + kTone);
    mx.set_music(nullptr);
    mx.render(out.data(), 4, is::audio::kEffectRate);
    CHECK(out[0] == effect);
}

TEST_CASE("midi: the ship disk's four files and the twelve track sections (IS_ORIGINAL_DIR)") {
    const char* dir = env("IS_ORIGINAL_DIR");
    if (!dir) {
        MESSAGE("IS_ORIGINAL_DIR unset; skipped");
        return;
    }
    struct Expect {
        const char* name;
        int tracks;
        double seconds;
    };
    // docs/re/sound.md "Music": format 1, division 480, the track counts and lengths.
    const Expect expect[m::kFileCount] = {
        {"MAIN.MID", 15, 295.52}, {"RACE.MID", 13, 295.26}, {"ANTHEM.MID", 14, 200.80}, {"DEMON.MID", 13, 223.98}};
    m::MidiFile files[m::kFileCount];
    for (int i = 0; i < m::kFileCount; ++i) {
        const fs::path p = find_ci(dir, expect[i].name);
        if (p.empty()) {
            MESSAGE("no " << expect[i].name << " in IS_ORIGINAL_DIR; skipped");
            return;
        }
        std::string why;
        INFO(expect[i].name);
        REQUIRE(files[i].load(p, &why));
        CHECK(files[i].format() == 1);
        CHECK(files[i].division() == 480);
        CHECK(files[i].tracks() == expect[i].tracks);
        CHECK(files[i].length_ms() / 1000.0 == doctest::Approx(expect[i].seconds).epsilon(0.0001));
        int sysex = 0;
        for (const auto& e : files[i].events()) sysex += e.kind == m::MidiEvent::kSysEx;
        CHECK(sysex == 0);  // sound.md: no SysEx
        CHECK(files[i].microsoft_authored());  // FF 7F 03 00 00 41 (M7 s2b fixer)
    }
    for (int t = 0; t < m::kTrackCount; ++t) {
        INFO("track " << t);
        const auto& tr = m::kTracks[static_cast<std::size_t>(t)];
        const double len = files[tr.file].length_ms();
        CHECK(m::seek_target_ms(t, -1) <= len);
        if (t == 5 || t == 8)
            CHECK(m::end_ms(t) > len);  // sound.md: these two end past their files
        else
            CHECK(m::end_ms(t) <= len);
    }
    // Track 5 (anthem 200-216.6 s) plays to the file's end and notifies once there.
    std::vector<Bytes> msgs;
    int resets = 0, notifies = 0;
    auto p = std::make_shared<m::MidiPlayer>(std::make_unique<FakeSynth>(&msgs, &resets),
                                             [&](unsigned w, std::uint32_t) { notifies += w == 1; });
    REQUIRE(p->open(find_ci(dir, "ANTHEM.MID")));
    REQUIRE(p->seek_ms(m::seek_target_ms(5, -1), true));
    REQUIRE(p->play_to_ms(m::end_ms(5)));
    std::vector<std::int16_t> out(2 * 512);
    for (int i = 0; i < 200 && notifies == 0; ++i) p->render_music(out.data(), 512, 22050);
    CHECK(notifies == 1);
    CHECK(p->position_ms() == doctest::Approx(files[2].length_ms()).epsilon(0.0002));
    // Track 8 (main 292-301 s) likewise.
    REQUIRE(p->open(find_ci(dir, "MAIN.MID")));
    REQUIRE(p->seek_ms(m::seek_target_ms(8, -1), true));
    REQUIRE(p->play_to_ms(m::end_ms(8)));
    for (int i = 0; i < 400 && notifies == 1; ++i) p->render_music(out.data(), 512, 22050);
    CHECK(notifies == 2);
    CHECK(p->position_ms() == doctest::Approx(files[0].length_ms()).epsilon(0.0002));
}

// The Player over the port's MIDI player, end to end on the ship disk's files with a fake synth
// and a simulated message queue (posted notifies delivered between output blocks through
// accept_notify, as audio.cpp's glue does): the loop of track 1, the notify-driven play of a
// flag-0 request, the focus pause, the control codes, the 7/8 -> 2 chaining and the stop of
// track 5 (seg9:0x2ac4, docs/re/sound.md "Music") (M7 s2 fixer).
TEST_CASE("midi: the Player over the MIDI player, end to end (IS_ORIGINAL_DIR)") {
    const char* dir = env("IS_ORIGINAL_DIR");
    if (!dir) {
        MESSAGE("IS_ORIGINAL_DIR unset; skipped");
        return;
    }
    // The files under the names midi_path() gives (SndPath\\<stem>.mid), in a scratch directory
    // of this process's own, so a case-sensitive file system finds them.
    const fs::path snd = fs::temp_directory_path() /
                         ("is_test_midi_e2e_" + std::to_string(std::random_device{}()));
    fs::create_directories(snd);
    struct Cleanup {
        fs::path d;
        ~Cleanup() {
            std::error_code ec;
            fs::remove_all(d, ec);
        }
    } cleanup{snd};
    for (int f = 0; f < m::kFileCount; ++f) {
        const fs::path to = m::midi_path(snd / "", f);
        const fs::path from = find_ci(dir, to.filename().string());
        if (from.empty()) {
            MESSAGE("no " << to.filename().string() << " in IS_ORIGINAL_DIR; skipped");
            return;
        }
        fs::copy_file(from, to);
    }
    std::vector<Bytes> msgs;
    int resets = 0;
    std::deque<std::uint32_t> queue;
    auto mp = std::make_shared<m::MidiPlayer>(std::make_unique<FakeSynth>(&msgs, &resets),
                                              [&](unsigned w, std::uint32_t seq) {
                                                  if (w == 1) queue.push_back(seq);
                                              });
    m::Player pl;
    pl.set_backend(m::backend_of(mp));
    m::Config c;
    c.music = 1;
    c.sound_system = 3;
    c.level = 100;
    c.snd_path = snd / "";  // with its separator, which midi_path keeps
    pl.configure(c);
    int dropped = 0;
    std::vector<std::int16_t> out(2 * 512);
    auto run = [&](double ms) {
        const int blocks = static_cast<int>(ms * 22.05 / 512) + 1;
        for (int i = 0; i < blocks; ++i) {
            mp->render_music(out.data(), 512, 22050);
            while (!queue.empty()) {
                const std::uint32_t seq = queue.front();
                queue.pop_front();
                if (mp->accept_notify(seq))
                    pl.notify(1);
                else
                    ++dropped;
            }
        }
    };
    auto near_seek = [&](double from) {
        const double pos = mp->position_ms();
        return pos >= from && pos < from + 1000;
    };

    pl.request(1, 1);
    run(50);
    CHECK(pl.state() == m::State::kPlaying);
    CHECK(mp->playing());
    run(m::end_ms(1) - m::seek_target_ms(1, -1) + 200);  // past the end: the loop
    CHECK(pl.track() == 1);
    CHECK(pl.state() == m::State::kPlaying);
    CHECK(near_seek(m::seek_target_ms(1, pl.previous())));

    pl.request(2, 0);  // MCI_NOTIFY seek, the play at its notify
    const unsigned target2 = m::seek_target_ms(2, pl.previous());
    run(50);
    CHECK(pl.state() == m::State::kPlaying);
    CHECK(mp->playing());
    CHECK(near_seek(target2));

    pl.focus_lost();
    run(300);
    CHECK(pl.state() == m::State::kPaused);
    CHECK_FALSE(mp->playing());
    const double held = mp->position_ms();
    pl.focus_gained();
    run(50);
    CHECK(pl.state() == m::State::kPlaying);
    CHECK(mp->position_ms() >= held);

    pl.request(-2, 1);
    run(50);
    CHECK(pl.state() == m::State::kPaused);
    pl.request(-3, 1);
    run(50);
    CHECK(pl.state() == m::State::kPlaying);
    pl.request(-1, 1);
    run(50);
    CHECK(pl.state() == m::State::kHardPaused);
    pl.request(-3, 1);
    run(50);
    CHECK(pl.state() == m::State::kHardPaused);
    CHECK_FALSE(mp->playing());

    for (int t : {7, 8}) {
        INFO("track " << t);
        pl.request(t, 1);
        run(50);
        CHECK(pl.state() == m::State::kPlaying);
        run(m::end_ms(t) - m::seek_target_ms(t, -1) + 500);
        CHECK(pl.track() == 2);  // seg9:0x2b44
        CHECK(pl.state() == m::State::kPlaying);
        CHECK(mp->playing());
    }

    pl.request(5, 1);  // ANTHEM from 200 s: the file's end, then nothing (seg9:0x2b4a)
    run(2000);
    CHECK(pl.track() == 5);
    CHECK(pl.state() == m::State::kPlaying);
    CHECK_FALSE(mp->playing());
    CHECK(dropped == 0);
}
