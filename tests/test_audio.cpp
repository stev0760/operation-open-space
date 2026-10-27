// The effects engine and the mixer (src/audio/engine.h, mixer.h, wav_source.h) against
// docs/re/sound.md and re/artifacts/sounds.tsv. Portable; the WAV checks read IS_ORIGINAL_DIR.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "audio/engine.h"
#include "audio/mixer.h"
#include "audio/wav_source.h"
#include "doctest.h"

namespace a = is::audio;
namespace fs = std::filesystem;

namespace {

const char* env(const char* name) {
    const char* v = std::getenv(name);
    return v && *v ? v : nullptr;
}

// The catalogue: re/artifacts/sounds.tsv beside the repository's work/assets, or found from
// this source file.
std::vector<std::map<std::string, std::string>> catalogue() {
    std::vector<fs::path> tries;
    if (const char* d = env("IS_ASSETS_DIR"))
        tries.push_back(fs::path(d).parent_path().parent_path() / "re/artifacts/sounds.tsv");
    tries.push_back(fs::path(__FILE__).parent_path().parent_path() / "re/artifacts/sounds.tsv");
    std::ifstream in;
    for (const auto& p : tries) {
        std::error_code ec;
        if (fs::exists(p, ec)) {
            in.open(p);
            break;
        }
    }
    std::vector<std::map<std::string, std::string>> rows;
    if (!in) return rows;
    std::string line;
    std::vector<std::string> head;
    auto split = [](const std::string& l) {
        std::vector<std::string> f;
        std::string cur;
        for (char c : l) {
            if (c == '\t') {
                f.push_back(cur);
                cur.clear();
            } else if (c != '\r') {
                cur += c;
            }
        }
        f.push_back(cur);
        return f;
    };
    std::getline(in, line);
    head = split(line);
    while (std::getline(in, line)) {
        const auto f = split(line);
        std::map<std::string, std::string> row;
        for (std::size_t i = 0; i < head.size() && i < f.size(); ++i) row[head[i]] = f[i];
        rows.push_back(row);
    }
    return rows;
}

// Every slot holds a short wave of `len` samples of `value`; the four missing resources
// are absent.
struct FakeSource : a::WaveSource {
    int len = 100;
    std::uint8_t value = 200;
    int slot_calls = 0;
    bool has_module(int) override { return true; }
    a::PcmPtr slot(int, int resource) override {
        ++slot_calls;
        if (resource == 112 || resource == 142 || resource == 165 || resource == 181)
            return nullptr;
        return std::make_shared<a::Pcm>(static_cast<std::size_t>(len), value);
    }
    a::PcmPtr file(const std::string&) override { return nullptr; }
};

a::Settings defaults() {
    a::Settings s{};
    s.sound_fx = 1;
    s.voice_fx = 1;
    s.music = 1;
    s.sound_volume = 100;
    s.fx_music = 50;
    s.stereo_effect = 2;
    s.purge_sound = 1;
    s.stereo_swap = 0;
    s.sound_system = -1;
    s.mix_channels = 4;
    s.saver_volume = 0;
    s.force_vol = 0;
    s.allow_sound = 0;
    return s;
}

struct Rig {
    a::Mixer mixer;
    FakeSource src;
    std::uint32_t clock = 10000;
    a::Engine eng{mixer, src, [this] { return clock; }};
    explicit Rig(a::Settings s = defaults()) { eng.command0(s, 1); }
    // Plays every channel to its end and delivers the completions.
    void finish() {
        for (int i = 0; i < 100000 && (mixer.busy(0) || mixer.busy(1) || mixer.busy(2) ||
                                       mixer.busy(3));
             ++i)
            mixer.mix_one();
        eng.pump();
    }
};

}  // namespace

TEST_CASE("audio: slot flags, resources and holds match the catalogue") {
    const auto rows = catalogue();
    if (rows.empty()) {
        MESSAGE("re/artifacts/sounds.tsv not found; skipped");
        return;
    }
    int slots = 0;
    for (const auto& r : rows) {
        const int id = static_cast<int>(std::stol(r.at("id"), nullptr, 0));
        if (id < 0 || id >= a::kSlots) continue;
        ++slots;
        CAPTURE(id);
        CHECK(a::slot_flag(id) == std::stoi(r.at("flag")));
        CHECK(a::slot_resource(id) == std::stoi(r.at("resource")));
        CHECK((r.at("module") == "SND_FX.DLL") == ((a::slot_flag(id) & 1) == 0));
        const std::string hold = r.at("hold");
        CHECK(a::hold_ms(id) == (hold.empty() ? 0 : std::stoi(hold)));
    }
    CHECK(slots == 82);
    // Specials carry no hold either.
    CHECK(a::hold_ms(0x6f) == 0);
}

TEST_CASE("audio: the WAV resources load as the catalogue says (IS_ORIGINAL_DIR)") {
    const char* dir = env("IS_ORIGINAL_DIR");
    const auto rows = catalogue();
    if (!dir || rows.empty()) {
        MESSAGE("IS_ORIGINAL_DIR or the catalogue missing; skipped");
        return;
    }
    is::assets::AssetStore store{fs::path(dir)};
    a::StoreWaveSource src(store);
    if (!src.has_module(0) || !src.has_module(1)) {
        MESSAGE("no SND_FX.DLL or ENG_FX.DLL in IS_ORIGINAL_DIR; skipped");
        return;
    }
    for (const auto& r : rows) {
        const int id = static_cast<int>(std::stol(r.at("id"), nullptr, 0));
        if (id < 0 || id >= a::kSlots) continue;
        CAPTURE(id);
        const a::PcmPtr w = src.slot(a::slot_flag(id) & 1, a::slot_resource(id));
        if (r.at("ms") == "absent") {
            CHECK(w == nullptr);
        } else {
            REQUIRE(w != nullptr);
            CHECK(static_cast<long>(w->size() * 1000 / a::kEffectRate) == std::stol(r.at("ms")));
        }
    }
}

TEST_CASE("audio: the volume routine") {
    a::Settings s = defaults();
    {
        Rig rig(s);
        CHECK(rig.eng.effects_volume() == 100);
        CHECK(rig.eng.music_volume() == 100);
        CHECK(rig.eng.music_volume_percent() == 95);
    }
    s.fx_music = 25;
    {
        Rig rig(s);
        CHECK(rig.eng.effects_volume() == 100);
        CHECK(rig.eng.music_volume() == 50);
    }
    s.fx_music = 75;
    {
        Rig rig(s);
        CHECK(rig.eng.effects_volume() == 50);
        CHECK(rig.eng.music_volume() == 100);
    }
    s.fx_music = 50;
    s.sound_volume = 60;
    {
        Rig rig(s);
        CHECK(rig.eng.effects_volume() == 60);
        rig.eng.command10(30);  // SaverVolume
        CHECK(rig.eng.effects_volume() == 30);
        CHECK(rig.eng.music_volume() == 30);
    }
}

TEST_CASE("audio: the device test and the sound system") {
    a::Settings s = defaults();
    {
        Rig rig(s);
        CHECK(rig.eng.capability() == 3);
        CHECK(rig.eng.sound_system() == 3);
        CHECK(rig.eng.channels() == 4);
    }
    {
        a::Mixer m;
        FakeSource src;
        a::Engine e(m, src, [] { return 0u; });
        e.command0(s, 0);
        CHECK(e.capability() == 0);
        CHECK(e.sound_system() == 0);
        CHECK(e.ordinal9(0x13, {0, 0}, 0, 1, 1, 640, 480, 100) == 0);
        CHECK(e.load_slot(0x13, 1) == -1);
    }
}

namespace {
// Ordinal 8's words for a point, with the listener at the centre of 640x480 plus 1.
std::uint32_t words(a::Settings s, int x, int y) {
    Rig rig(s);
    rig.eng.ordinal8(640, 480, 1, 1, {x, y});
    return rig.mixer.volume();
}
std::uint32_t pack(unsigned left, unsigned right) { return right << 16 | left; }
}  // namespace

TEST_CASE("audio: ordinal 8 at the breakpoints") {
    const a::Settings s = defaults();
    const int cx = 321, cy = 241;  // half(640) + 1, half(480) + 1
    // -6000: A = B = 80, V = 100 -> 0xcccc both (the oracle's WAVEOUTSETVOLUME(0, cccccccc)).
    CHECK(words(s, -6000, 0) == 0xccccccccu);
    // d = 0: A = B = 95.
    CHECK(words(s, cx, cy) == pack(62258, 62258));
    // d = 299: A = 99, B = 2990 / -35 + 95 = 10; dx > 0 puts A on the right.
    CHECK(words(s, cx + 299, cy) == pack(6553, 64879));
    // d = 300: A = 100, B = 10.
    CHECK(words(s, cx + 300, cy) == pack(6553, 65535));
    // d = 600 on the left: A = 50, B = 0.
    CHECK(words(s, cx - 600, cy) == pack(32767, 0));
    // d = 900: A = 15.
    CHECK(words(s, cx - 900, cy) == pack(9830, 0));
    // The 3200 wrap: dx = 1700 > 1600 moves p.x by -3200: dx = -1500, A = 15 on the left.
    CHECK(words(s, cx + 1700, cy) == pack(9830, 0));
    // V: e = 300 -> 75, e = 600 -> 20, e = 900 -> 15 (A = B = 95 at d = 0).
    CHECK(words(s, cx, cy + 300) == pack(46529, 46529));   // 95 * 75 / 100 = 71
    CHECK(words(s, cx, cy - 600) == pack(12451, 12451));   // 95 * 20 / 100 = 19
    CHECK(words(s, cx, cy + 900) == pack(9174, 9174));     // 95 * 15 / 100 = 14
    // StereoEffect 0 quarters the distance: 1200 -> 300.
    a::Settings s0 = s;
    s0.stereo_effect = 0;
    CHECK(words(s0, cx + 1200, cy) == pack(6553, 65535));
    // StereoEffect 3: d + d / 2: 200 -> 300.
    a::Settings s3 = s;
    s3.stereo_effect = 3;
    CHECK(words(s3, cx + 200, cy) == pack(6553, 65535));
    // StereoSwap swaps the sides.
    a::Settings sw = s;
    sw.stereo_swap = 1;
    CHECK(words(sw, cx + 300, cy) == pack(65535, 6553));
    // FXMusic 75: E = 50 halves each side.
    a::Settings fx = s;
    fx.fx_music = 75;
    CHECK(words(fx, cx + 300, cy) == pack(3276, 32767));
    // STANDARD SOUND: ordinal 8 returns at once and the volume stays at unity.
    a::Settings st = s;
    st.sound_system = 1;
    CHECK(words(st, cx + 300, cy) == 0xFFFFFFFFu);
}

TEST_CASE("audio: busy-until holds and admission with a fake clock") {
    Rig rig;
    auto& e = rig.eng;
    // A held voice: q = 1 with nothing held stops all and takes the priority.
    CHECK(e.ordinal9(0x2c, {-6000, -6000}, 1, 1, 1, 640, 480, 1000) == 1);
    CHECK(e.busy_until() == 1730);
    CHECK(e.priority_id() == 0x2c);
    CHECK(e.held_count() == 1);
    CHECK(e.active_count() == 1);
    // Inside the hold nothing plays.
    CHECK(e.ordinal9(0x13, {-6000, -6000}, 0, 1, 1, 640, 480, 1500) == 0);
    // After the hold a q = 0 sound is refused while the priority record holds q.
    CHECK(e.ordinal9(0x13, {-6000, -6000}, 0, 1, 1, 640, 480, 1731) == 0);
    CHECK(e.busy_until() == 1730);
    // A loop word skips the busy check, but rule 1 still refuses it.
    CHECK(e.ordinal9(0x6f, {-6000, -6000}, 0, 1, 1, 640, 480, 1100) == 0);
    // The voice ends: the priority resets and plays go on.
    rig.finish();
    CHECK(e.priority_id() == 0);
    CHECK(e.priority_channel() == 8);
    CHECK(e.active_count() == 0);
    CHECK(e.ordinal9(0x13, {-6000, -6000}, 0, 1, 1, 640, 480, 1731) == 1);
    CHECK(e.busy_until() == 1731);  // q = 0 resets busy-until to now
    // 0x69 resets the priority and clears busy-until.
    CHECK(e.ordinal9(0x69, {0, 0}, 0, 1, 1, 640, 480, 1800) == 1);
    CHECK(e.priority_id() == 0);
    CHECK(e.busy_until() == 0);
    // Rule 7: id >= cur takes the priority; a lower id plays without it.
    CHECK(e.ordinal9(0x20, {-6000, -6000}, 0, 1, 1, 640, 480, 1900) == 1);
    CHECK(e.ordinal9(0x15, {-6000, -6000}, 0, 1, 1, 640, 480, 1900) == 0);
    CHECK(e.priority_id() == 0x20);
    CHECK(e.active_count() == 3);  // 0x13 still plays: 0x69 stops nothing with 4 channels
    // 0x6e stops everything.
    CHECK(e.ordinal9(0x6e, {0, 0}, 0, 1, 1, 640, 480, 2000) == 1);
    CHECK(e.active_count() == 0);
    CHECK_FALSE(rig.mixer.busy(0));
    // Effects volume 0 refuses plays without a loop word.
    a::Settings mute = defaults();
    mute.sound_volume = 0;
    Rig quiet(mute);
    CHECK(quiet.eng.ordinal9(0x13, {-6000, -6000}, 0, 1, 1, 640, 480, 100) == 0);
    CHECK_FALSE(quiet.mixer.busy(0));
}

TEST_CASE("audio: the player's channel choice") {
    Rig rig;
    auto& e = rig.eng;
    // The same id within 80 ms reuses its channel.
    e.ordinal9(0x15, {-6000, -6000}, 0, 1, 1, 640, 480, 100);
    rig.clock += 50;
    e.ordinal9(0x15, {-6000, -6000}, 0, 1, 1, 640, 480, 101);
    CHECK(e.record(0).id == 0x15);
    CHECK(e.record(1).id == 0);
    // Past 80 ms it takes the next free channel.
    rig.clock += 100;
    e.ordinal9(0x15, {-6000, -6000}, 0, 1, 1, 640, 480, 102);
    CHECK(e.record(1).id == 0x15);
    // 0x14 never reuses.
    e.ordinal9(0x14, {-6000, -6000}, 0, 1, 1, 640, 480, 103);
    e.ordinal9(0x14, {-6000, -6000}, 0, 1, 1, 640, 480, 104);
    CHECK(e.record(2).id == 0x14);
    CHECK(e.record(3).id == 0x14);
    // All busy: the oldest record that is not the priority channel.
    const int prio = e.priority_channel();
    rig.clock += 10;
    e.ordinal9(0x16, {-6000, -6000}, 0, 1, 1, 640, 480, 105);
    int found = -1;
    for (int c = 0; c < 4; ++c)
        if (e.record(c).id == 0x16) found = c;
    CHECK(found >= 0);
    CHECK(found != prio);
}

TEST_CASE("audio: remaps, variants and missing resources") {
    Rig rig;
    auto& e = rig.eng;
    // 0x0c plays slot 0x0b (resource 112 is missing).
    e.ordinal9(0x0c, {-6000, -6000}, 0, 1, 1, 640, 480, 100);
    CHECK(e.record(0).id == 0x0b);
    // 0x2a, 0x41, 0x51 never play.
    e.ordinal9(0x6e, {}, 0, 1, 1, 640, 480, 100);
    CHECK(e.ordinal9(0x2a, {-6000, -6000}, 0, 1, 1, 640, 480, 100) == 0);
    CHECK(e.ordinal9(0x51, {-6000, -6000}, 0, 1, 1, 640, 480, 100) == 0);
    CHECK_FALSE(rig.mixer.busy(0));
    // 0x64 plays nothing.
    CHECK(e.ordinal9(0x64, {-6000, -6000}, 0, 1, 1, 640, 480, 100) == 0);
    // 0x66 never picks its last variant twice; 0x67 alternates 0x49 and 0x2e.
    int last = -1;
    for (int i = 0; i < 30; ++i) {
        e.ordinal9(0x6e, {}, 0, 1, 1, 640, 480, 100);
        e.ordinal9(0x66, {-6000, -6000}, 0, 1, 1, 640, 480, 100);
        const int id = e.record(0).id;
        CHECK(id >= 0x47);
        CHECK(id <= 0x49);
        CHECK(id != last);
        last = id;
    }
    std::vector<int> alt;
    for (int i = 0; i < 4; ++i) {
        e.ordinal9(0x6e, {}, 0, 1, 1, 640, 480, 100);
        e.ordinal9(0x67, {-6000, -6000}, 0, 1, 1, 640, 480, 100);
        alt.push_back(e.record(0).id);
    }
    CHECK(alt == std::vector<int>{0x49, 0x2e, 0x49, 0x2e});
    // 0x26 plays 0x23 while VoiceFX is 0.
    a::Settings nv = defaults();
    nv.voice_fx = 0;
    Rig r2(nv);
    r2.eng.ordinal9(0x26, {-6000, -6000}, 0, 1, 1, 640, 480, 100);
    CHECK(r2.eng.record(0).id == 0x23);
    // A preload (p.x = -11111) loads a deferred slot and plays nothing.
    Rig r3;
    CHECK_FALSE(r3.eng.loaded(0x13));
    CHECK(r3.eng.ordinal9(0x13, {-11111, 0}, 0, 1, 1, 640, 480, 100) == 0);
    CHECK(r3.eng.loaded(0x13));
    CHECK_FALSE(r3.mixer.busy(0));
}

TEST_CASE("audio: slot loading and purging") {
    a::Settings s = defaults();
    Rig rig(s);
    auto& e = rig.eng;
    // Command 0 preloads flag-0 slots and defers bit 1.
    CHECK(e.loaded(0x00));
    CHECK(e.loaded(0x26));   // flag 1
    CHECK_FALSE(e.loaded(0x01));  // flag 2
    CHECK(e.loaded(0x28));        // flag 4: bit 1 clear, so preloaded
    CHECK(e.load_slot(0x29, 1) == 1);
    CHECK(e.load_slot(0x29, 1) == -3);
    CHECK(e.load_slot(0x13, 1) == 1);
    CHECK(e.load_slot(0x43, 1) == 1);  // flag 7
    // Command 2 purges bit-1 slots while PurgeSound is on, never 0x28.
    e.command2();
    CHECK_FALSE(e.loaded(0x13));
    CHECK_FALSE(e.loaded(0x43));
    CHECK(e.loaded(0x28));
    CHECK(e.loaded(0x00));
    // PurgeSound 0 keeps bit 1 without bit 2, bit 2 still goes.
    s.purge_sound = 0;
    Rig keep(s);
    keep.eng.load_slot(0x13, 1);
    keep.eng.load_slot(0x05, 1);  // flag 6
    keep.eng.command2();
    CHECK(keep.eng.loaded(0x13));
    CHECK_FALSE(keep.eng.loaded(0x05));
    // SoundFX off refuses module-0 slots with -4.
    a::Settings nf = defaults();
    nf.sound_fx = 0;
    Rig off(nf);
    CHECK(off.eng.load_slot(0x13, 1) == -4);
    CHECK(off.eng.load_slot(0x4b, 1) == 1);
}

TEST_CASE("audio: the mixer sums and clips") {
    a::Mixer m;
    m.configure(2);
    m.play(0, std::make_shared<a::Pcm>(a::Pcm{200, 100, 255, 0}), 1);
    m.play(1, std::make_shared<a::Pcm>(a::Pcm{100, 100, 255, 0}), 1);
    CHECK(m.mix_one() == 72 - 28);
    CHECK(m.mix_one() == -56);
    CHECK(m.mix_one() == 127);   // 254 clipped
    CHECK(m.mix_one() == -128);  // -256 clipped
    CHECK(m.take_completed() == std::vector<int>{0, 1});
    CHECK(m.mix_one() == 0);
    // render: 16-bit stereo with the volume words; 22,050 Hz holds each sample twice.
    m.play(0, std::make_shared<a::Pcm>(a::Pcm{192}), 1);
    m.set_volume(0xFFFF, 0x8000);
    std::int16_t out[4] = {};
    m.render(out, 2, 22050);
    CHECK(out[0] == 64 * 256);
    CHECK(out[1] == static_cast<std::int16_t>(64 * 256 * 0x8000L / 0xFFFF));
    CHECK(out[2] == out[0]);
    CHECK(out[3] == out[1]);
}

TEST_CASE("audio: id 0x6f loops 20 times (21 plays) on WaveMix, forever on one channel") {
    Rig rig;
    rig.src.len = 100;
    REQUIRE(rig.eng.ordinal9(0x6f, {-6000, -6000}, 0, 1, 1, 640, 480, 100) == 1);
    CHECK(rig.eng.record(0).id == 0x17);
    int frames = 0;
    while (rig.mixer.busy(0) && frames < 100000) {
        rig.mixer.mix_one();
        ++frames;
    }
    CHECK(frames == 21 * 100);
    CHECK(rig.mixer.take_completed() == std::vector<int>{0});

    a::Settings one = defaults();
    one.mix_channels = 1;
    Rig single(one);
    REQUIRE(single.eng.ordinal9(0x6f, {-6000, -6000}, 0, 1, 1, 640, 480, 100) == 1);
    for (int i = 0; i < 10000; ++i) single.mixer.mix_one();
    CHECK(single.mixer.busy(0));
    CHECK(single.mixer.take_completed().empty());
    // The next play replaces it (sndPlaySound's one voice) after 0x69 stops it.
    single.eng.ordinal9(0x69, {}, 0, 1, 1, 640, 480, 100);
    CHECK_FALSE(single.mixer.busy(0));
}

TEST_CASE("audio: the two-step sequences of seg10:0x754a and seg10:0x7b48") {
    Rig rig;
    auto& e = rig.eng;
    e.wrap_754a(0x6a, 0, 0, 0, 1000);  // 0x20, then 0x21 after 1000 ms
    CHECK(e.priority_id() == 0x20);
    CHECK(e.pending_id() == 0x21);
    CHECK(e.pending_deadline() == 2000);
    e.wrap_7b48(2000);  // not yet past
    CHECK(e.pending_id() == 0x21);
    e.wrap_7b48(2001);
    CHECK(e.pending_id() == 0);
    CHECK(e.pending_deadline() == 0);
    CHECK(e.priority_id() == 0x21);

    // Another sound in between drops a follow-up below 0x38 with one INSPACE channel.
    Rig r2;
    r2.eng.wrap_754a(0x71, 0, 0, 0, 1000);  // 0x24 then 0x25
    r2.eng.wrap_744c(0x13, 1100);
    r2.eng.wrap_7b48(1700);
    CHECK(r2.eng.pending_id() == 0);
    bool played25 = false;
    for (int c = 0; c < 4; ++c) played25 |= r2.eng.record(c).id == 0x25;
    CHECK_FALSE(played25);

    // A follow-up of 0x66 always plays.
    Rig r3;
    r3.eng.wrap_754a(0x74, 0, 0, 0, 1000);  // 0x42 then 0x66
    r3.eng.wrap_744c(0x13, 1100);
    r3.eng.wrap_7b48(1401);
    bool voice = false;
    for (int c = 0; c < 4; ++c) {
        const int id = r3.eng.record(c).id;
        voice |= id >= 0x47 && id <= 0x49;
    }
    CHECK(voice);

    // 0x6e and the looping ids clear a pending follow-up.
    Rig r4;
    r4.eng.wrap_754a(0x6b, 0, 0, 0, 1000);
    r4.eng.wrap_754a(0x6e, 0, 0, 0, 1001);
    CHECK(r4.eng.pending_deadline() == 0);

    // seg10:0x744c: 0x69 first, so every sound resets the priority.
    Rig r5;
    r5.eng.wrap_744c(0x20, 100);
    CHECK(r5.eng.priority_id() == 0x20);
    r5.eng.wrap_744c(0x13, 100);
    CHECK(r5.eng.priority_id() == 0x13);
}

TEST_CASE("audio: INSPACE's music wrapper seg10:0x7480") {
    a::MusicWrapper w;
    int code = 0;
    CHECK(w.filter(code, 100));
    CHECK(w.current == 0);
    CHECK(w.hold == 9100);
    code = 7;  // 7 goes out as 7 and is stored as 2
    CHECK(w.filter(code, 200));
    CHECK(code == 7);
    CHECK(w.current == 2);
    code = -2;
    CHECK(w.filter(code, 300));
    CHECK(code == -2);
    CHECK(w.saved[0] == 2);
    code = 1;
    w.filter(code, 400);
    code = -3;  // becomes the saved 2, which differs from the current 1
    CHECK(w.filter(code, 500));
    CHECK(code == 2);
    code = 10;  // 10 while 1 plays becomes 9; 9 or 10 while 9 or 10 plays do nothing
    w.current = 1;
    CHECK(w.filter(code, 600));
    CHECK(code == 9);
    code = 10;
    CHECK_FALSE(w.filter(code, 700));
}

TEST_CASE("audio: any PCM WAV converts to 8-bit 11,025 Hz mono") {
    // 16-bit stereo at 22,050 Hz: 4 frames of (+16384, -16384) and (32767, 32767).
    std::vector<std::uint8_t> f;
    auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) f.push_back(static_cast<std::uint8_t>(v >> (8 * i))); };
    auto u16 = [&](std::uint16_t v) { f.push_back(static_cast<std::uint8_t>(v)); f.push_back(static_cast<std::uint8_t>(v >> 8)); };
    const std::uint32_t data = 4 * 4;
    f.insert(f.end(), {'R', 'I', 'F', 'F'});
    u32(36 + data);
    f.insert(f.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    u32(16); u16(1); u16(2); u32(22050); u32(22050 * 4); u16(4); u16(16);
    f.insert(f.end(), {'d', 'a', 't', 'a'});
    u32(data);
    for (int i = 0; i < 2; ++i) { u16(16384); u16(static_cast<std::uint16_t>(-16384)); }
    for (int i = 0; i < 2; ++i) { u16(32767); u16(32767); }
    const a::PcmPtr p = a::to_effect_pcm(f.data(), f.size());
    REQUIRE(p != nullptr);
    REQUIRE(p->size() == 2);
    CHECK((*p)[0] == 128);  // the channels cancel
    CHECK((*p)[1] == 128 + 128 - 1);  // 32767 / 256 rounds to 128, clamped to 255
    CHECK(a::to_effect_pcm(f.data(), 10) == nullptr);
}

// M7 fixer: ordinal 9 returns 0 while the slot stays empty after the loader, before the
// admission can stop anything [BYTES ISPACE.DLL seg9:0xe37-0xe68].
TEST_CASE("audio: an effect refused by the loader stops no voice") {
    a::Settings nf = defaults();
    nf.sound_fx = 0;
    Rig rig(nf);
    auto& e = rig.eng;
    CHECK(e.ordinal9(0x2c, {-6000, -6000}, 0, 1, 1, 640, 480, 1000) == 1);  // a voice, q = 0
    CHECK(e.active_count() == 1);
    const auto busy = e.busy_until();
    // 0x13 with q = 1 would stop all through the admission's rule 2; its slot cannot load.
    CHECK(e.ordinal9(0x13, {-6000, -6000}, 1, 1, 1, 640, 480, 1100) == 0);
    CHECK(e.active_count() == 1);
    CHECK(e.record(0).id == 0x2c);
    CHECK(e.busy_until() == busy);
}

// M7 fixer: Audio OK's seg9:0x4e0(1, 0) rebuilds the session whatever the reference count
// [BYTES ISPACE.DLL seg9:0x3547 seg9:0x3582].
TEST_CASE("audio: Audio OK opens a session after a start with SoundSystem 0") {
    a::Settings s0 = defaults();
    s0.sound_system = 0;
    Rig rig(s0);
    CHECK(rig.eng.sound_system() == 0);
    a::Settings s1 = defaults();
    s1.sound_system = 3;
    s1.mix_channels = 2;
    rig.eng.reread(s1);
    CHECK(rig.eng.sound_system() == 3);
    CHECK(rig.eng.channels() == 2);
    CHECK(rig.eng.ordinal9(0x13, {-6000, -6000}, 0, 1, 1, 640, 480, 100) == 1);
}
