// The soundfont synth (src/audio/music_soundfont.*, ADR 0011 decision 3). Portable. The core
// cases build a tiny SoundFont in the test; the GM cases use GeneralUser GS under work/sf2 (or
// the .sf2 that IS_SF2 names) and skip without it; the full-mix peak case also reads the
// game's MIDI files through IS_ORIGINAL_DIR.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "audio/midi_file.h"
#include "audio/music_soundfont.h"
#include "audio/synth.h"
#include "doctest.h"
#include "formats/szdd.h"

namespace m = is::audio::music;
namespace fs = std::filesystem;

namespace {

const char* env(const char* name) {
    const char* v = std::getenv(name);
    return v && *v ? v : nullptr;
}

// --- A SoundFont 2 file built in memory [REF SoundFont 2.04 specification] -------------------

struct Bytes {
    std::vector<std::uint8_t> b;
    void u8(unsigned v) { b.push_back(static_cast<std::uint8_t>(v)); }
    void u16(unsigned v) { u8(v & 0xff); u8(v >> 8); }
    void u32(std::uint32_t v) { u16(v & 0xffff); u16(v >> 16); }
    void tag(const char* t) { b.insert(b.end(), t, t + 4); }
    void name(const char* n) {
        char buf[20] = {};
        std::strncpy(buf, n, 19);
        b.insert(b.end(), buf, buf + 20);
    }
    void append(const Bytes& o) { b.insert(b.end(), o.b.begin(), o.b.end()); }
};

Bytes chunk(const char* id, const Bytes& body) {
    Bytes c;
    c.tag(id);
    c.u32(static_cast<std::uint32_t>(body.b.size()));
    c.append(body);
    if (body.b.size() & 1) c.u8(0);
    return c;
}

Bytes list(const char* type, const std::vector<Bytes>& chunks) {
    Bytes body;
    body.tag(type);
    for (const auto& c : chunks) body.append(c);
    return chunk("LIST", body);
}

constexpr int kCycle = 100;          // samples per cycle at 44,100 Hz: 441 Hz
constexpr std::uint32_t kLoopCycles = 10;
constexpr unsigned kRootKey = 69;    // A4, 440 Hz: key 69 plays the sample at its own pitch

// Two looped instruments: 0 a sine, 1 a square, each one cycle repeated. Presets: bank 0
// program 0 = sine, bank 0 program 1 = square, bank 128 program 0 (a drum kit) = square.
std::vector<std::uint8_t> tiny_sf2() {
    const std::uint32_t len = kCycle * kLoopCycles;
    Bytes smpl;
    for (int wave = 0; wave < 2; ++wave) {
        for (std::uint32_t i = 0; i < len; ++i) {
            const double ph = 2.0 * 3.14159265358979 * (i % kCycle) / kCycle;
            const double v = wave == 0 ? std::sin(ph) : (std::sin(ph) >= 0 ? 1.0 : -1.0);
            smpl.u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(v * 16000)));
        }
        for (int i = 0; i < 46; ++i) smpl.u16(0);  // the 46 zero samples after each sample
    }
    const std::uint32_t stride = len + 46;

    Bytes ifil;
    ifil.u16(2);
    ifil.u16(1);
    Bytes isng;
    isng.b = {'E', 'M', 'U', '8', '0', '0', '0', 0};
    Bytes inam;
    inam.b = {'t', 'i', 'n', 'y', 0, 0};

    Bytes phdr, pbag, pmod, pgen, inst, ibag, imod, igen, shdr;
    const struct { const char* n; unsigned preset, bank, instrument; } presets[] = {
        {"sine", 0, 0, 0}, {"square", 1, 0, 1}, {"kit", 0, 128, 1}};
    unsigned bag = 0;
    for (const auto& p : presets) {
        phdr.name(p.n);
        phdr.u16(p.preset);
        phdr.u16(p.bank);
        phdr.u16(bag);
        phdr.u32(0);
        phdr.u32(0);
        phdr.u32(0);
        pbag.u16(bag);  // one generator per bag: the instrument
        pbag.u16(0);
        pgen.u16(41);
        pgen.u16(p.instrument);
        ++bag;
    }
    phdr.name("EOP");
    for (int i = 0; i < 3; ++i) phdr.u16(i == 2 ? bag : 0);
    phdr.u32(0);
    phdr.u32(0);
    phdr.u32(0);
    pbag.u16(bag);
    pbag.u16(0);
    pmod.b.assign(10, 0);
    pgen.u32(0);

    const char* inames[] = {"sine", "square"};
    unsigned igens = 0;
    for (unsigned i = 0; i < 2; ++i) {
        inst.name(inames[i]);
        inst.u16(i);
        ibag.u16(igens);
        ibag.u16(0);
        igen.u16(54);  // sampleModes: loop continuously
        igen.u16(1);
        igen.u16(53);  // sampleID, last in the zone
        igen.u16(i);
        igens += 2;
    }
    inst.name("EOI");
    inst.u16(2);
    ibag.u16(igens);
    ibag.u16(0);
    imod.b.assign(10, 0);
    igen.u32(0);

    for (std::uint32_t i = 0; i < 2; ++i) {
        shdr.name(inames[i]);
        shdr.u32(i * stride);
        shdr.u32(i * stride + len);
        shdr.u32(i * stride);
        shdr.u32(i * stride + len);
        shdr.u32(44100);
        shdr.u8(kRootKey);
        shdr.u8(0);
        shdr.u16(0);
        shdr.u16(1);  // monoSample
    }
    shdr.name("EOS");
    for (int i = 0; i < 6; ++i) shdr.u32(0);
    shdr.u16(0);

    Bytes body;
    body.tag("sfbk");
    body.append(list("INFO", {chunk("ifil", ifil), chunk("isng", isng), chunk("INAM", inam)}));
    body.append(list("sdta", {chunk("smpl", smpl)}));
    body.append(list("pdta", {chunk("phdr", phdr), chunk("pbag", pbag), chunk("pmod", pmod),
                              chunk("pgen", pgen), chunk("inst", inst), chunk("ibag", ibag),
                              chunk("imod", imod), chunk("igen", igen), chunk("shdr", shdr)}));
    return chunk("RIFF", body).b;
}

fs::path write_temp(const std::string& name, const std::vector<std::uint8_t>& data) {
    const fs::path p = fs::temp_directory_path() / name;
    std::ofstream(p, std::ios::binary)
        .write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return p;
}

struct TinySf2 {
    fs::path path = write_temp("is_test_tiny.sf2", tiny_sf2());
    ~TinySf2() {
        std::error_code ec;
        fs::remove(path, ec);
    }
};

// --- Measurements --------------------------------------------------------------------------

std::vector<std::int16_t> render(m::Synth& s, std::size_t frames) {
    std::vector<std::int16_t> out(frames * 2, 0x5a5a);
    s.render(out.data(), frames);
    return out;
}

int peak(const std::vector<std::int16_t>& v) {
    int p = 0;
    for (auto x : v) p = std::max(p, std::abs(static_cast<int>(x)));
    return p;
}

double rms(const std::vector<std::int16_t>& v) {
    double s = 0;
    for (auto x : v) s += double(x) * x;
    return v.empty() ? 0 : std::sqrt(s / double(v.size()));
}

// Rising zero crossings of the left channel per second.
double frequency(const std::vector<std::int16_t>& v, std::uint32_t rate) {
    int n = 0;
    for (std::size_t i = 2; i < v.size(); i += 2)
        if (v[i - 2] < 0 && v[i] >= 0) ++n;
    return n * double(rate) / double(v.size() / 2);
}

void send(m::Synth& s, std::initializer_list<std::uint8_t> msg) {
    const std::vector<std::uint8_t> b(msg);
    s.midi(b.data(), b.size());
}

std::unique_ptr<m::Synth> open_tiny(const TinySf2& sf, std::uint32_t rate = 22050) {
    auto s = m::make_soundfont();
    REQUIRE(s);
    m::SynthConfig cfg;
    cfg.soundfont = sf.path;
    cfg.rate = rate;
    REQUIRE(s->open(cfg));
    return s;
}

// The GM SoundFont of the GM cases: IS_SF2, else work/sf2/GeneralUser-GS.sf2 of the
// repository that holds IS_ORIGINAL_DIR (original/ or work/extracted/game) or this file.
fs::path gm_sf2() {
    if (const char* p = env("IS_SF2")) return p;
    std::vector<fs::path> roots;
    if (const char* o = env("IS_ORIGINAL_DIR"))
        for (fs::path d = fs::path(o); d.has_relative_path(); d = d.parent_path()) roots.push_back(d);
    roots.push_back(fs::path(__FILE__).parent_path().parent_path());
    for (const auto& r : roots) {
        const fs::path p = r / "work" / "sf2" / "GeneralUser-GS.sf2";
        std::error_code ec;
        if (fs::exists(p, ec)) return p;
    }
    return {};
}

// One of the four MIDI files: <dir>/<NAME>.MID as isre extracts it, or the game's
// SZDD-packed <NAME>.MI1 in <dir> (IS_ORIGINAL_DIR=original/game) or <dir>/game
// (IS_ORIGINAL_DIR=original) (M7 s2 fixer). Empty when none is there.
std::vector<std::uint8_t> ship_midi(const fs::path& dir, const std::string& name) {
    for (const fs::path& p :
         {dir / (name + ".MID"), dir / (name + ".MI1"), dir / "game" / (name + ".MI1")}) {
        std::ifstream in(p, std::ios::binary);
        if (!in) continue;
        std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(in)), {});
        if (is::fmt::is_szdd(b.data(), b.size())) return is::fmt::szdd_expand(b.data(), b.size()).data;
        return b;
    }
    return {};
}

}  // namespace

TEST_CASE("soundfont synth: the factory and refused files") {
    auto s = m::make_soundfont();
    REQUIRE(s);
    CHECK(s->name() == "soundfont");
    m::SynthConfig cfg;
    CHECK_FALSE(s->open(cfg));  // no path
    cfg.soundfont = fs::temp_directory_path() / "is_test_no_such.sf2";
    CHECK_FALSE(s->open(cfg));
    const fs::path junk = write_temp("is_test_junk.sf2", {'R', 'I', 'F', 'F', 4, 0, 0, 0, 'W', 'A', 'V', 'E'});
    cfg.soundfont = junk;
    CHECK_FALSE(s->open(cfg));
    fs::remove(junk);
    // Not open: silence, replacing what the buffer held.
    const auto out = render(*s, 256);
    CHECK(peak(out) == 0);
    send(*s, {0x90, 60, 100});  // ignored, no crash
}

TEST_CASE("soundfont synth: a SoundFont path outside the ANSI code page opens") {
    const auto data = tiny_sf2();
    const fs::path p = fs::temp_directory_path() / fs::u8path(u8"is_test_\u00fcn\u00efc\u00f8d\u00e9_\u043c\u0443\u0437.sf2");
    std::ofstream(p, std::ios::binary)
        .write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    auto s = m::make_soundfont();
    m::SynthConfig cfg;
    cfg.soundfont = p;
    CHECK(s->open(cfg));
    std::error_code ec;
    fs::remove(p, ec);
}

TEST_CASE("soundfont synth: note on sounds, all notes off decays to silence") {
    TinySf2 sf;
    auto s = open_tiny(sf);
    CHECK(peak(render(*s, 2205)) == 0);  // silence before a note, and render replaces
    send(*s, {0x90, kRootKey, 127});
    const auto on = render(*s, 22050);
    CHECK(rms(on) > 100);
    CHECK(peak(on) < 32767);  // no clipping from one voice at the gain
    send(*s, {0xb0, 123, 0});  // all notes off
    render(*s, 22050);         // the release (the default volume-envelope release is ~1 ms)
    CHECK(peak(render(*s, 2205)) == 0);
}

TEST_CASE("soundfont synth: note off, velocity 0 and the GM reset end notes") {
    TinySf2 sf;
    auto s = open_tiny(sf);
    send(*s, {0x90, 60, 100});
    CHECK(rms(render(*s, 2205)) > 0);
    send(*s, {0x80, 60, 0});
    render(*s, 2205);
    CHECK(peak(render(*s, 2205)) == 0);
    send(*s, {0x91, 64, 100});
    CHECK(rms(render(*s, 2205)) > 0);
    send(*s, {0x91, 64, 0});  // note on with velocity 0 is a note off
    render(*s, 2205);
    CHECK(peak(render(*s, 2205)) == 0);
    send(*s, {0x92, 67, 100});
    CHECK(rms(render(*s, 2205)) > 0);
    send(*s, {0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7});  // GM System On
    render(*s, 2205);
    CHECK(peak(render(*s, 2205)) == 0);
    send(*s, {0x92, 67, 100});
    CHECK(rms(render(*s, 2205)) > 0);
    s->reset();
    render(*s, 2205);
    CHECK(peak(render(*s, 2205)) == 0);
}

TEST_CASE("soundfont synth: program change changes the output") {
    TinySf2 sf;
    auto s = open_tiny(sf);
    send(*s, {0x90, kRootKey, 100});
    const auto sine = render(*s, 22050);
    send(*s, {0xb0, 123, 0});
    render(*s, 4410);
    send(*s, {0xc0, 1});
    send(*s, {0x90, kRootKey, 100});
    const auto square = render(*s, 22050);
    // Same pitch, same velocity: the square's RMS is the sine's times sqrt(2).
    CHECK(rms(square) > rms(sine) * 1.25);
    CHECK(frequency(square, 22050) == doctest::Approx(frequency(sine, 22050)).epsilon(0.02));
    // A program the SoundFont lacks keeps the channel's preset.
    send(*s, {0xb0, 123, 0});
    render(*s, 4410);
    send(*s, {0xc0, 57});
    send(*s, {0x90, kRootKey, 100});
    CHECK(rms(render(*s, 22050)) == doctest::Approx(rms(square)).epsilon(0.05));
}

TEST_CASE("soundfont synth: channel 10 plays the bank 128 kit") {
    TinySf2 sf;
    auto s = open_tiny(sf);
    send(*s, {0x90, kRootKey, 100});
    const auto sine = render(*s, 22050);
    send(*s, {0xb0, 123, 0});
    render(*s, 4410);
    send(*s, {0x99, kRootKey, 100});  // the kit is the square
    CHECK(rms(render(*s, 22050)) > rms(sine) * 1.25);
}

TEST_CASE("soundfont synth: render at 22050 and 11025 Hz keeps the pitch") {
    TinySf2 sf;
    for (std::uint32_t rate : {22050u, 11025u}) {
        CAPTURE(rate);
        auto s = open_tiny(sf, rate);
        send(*s, {0x90, kRootKey, 100});
        const auto out = render(*s, rate);  // one second
        CHECK(rms(out) > 100);
        CHECK(frequency(out, rate) == doctest::Approx(441.0).epsilon(0.02));
        send(*s, {0x90, kRootKey + 12, 100});  // an octave up joins it: the sum is louder
        CHECK(rms(render(*s, rate)) > rms(out));
    }
}

TEST_CASE("soundfont synth: volume, expression, pan, pitch bend and Reset All Controllers") {
    TinySf2 sf;
    auto s = open_tiny(sf);
    send(*s, {0x90, kRootKey, 100});
    const auto base = render(*s, 22050);
    send(*s, {0xb0, 7, 50});
    const double quiet = rms(render(*s, 22050));
    CHECK(quiet < rms(base) * 0.5);
    send(*s, {0xb0, 11, 64});
    CHECK(rms(render(*s, 22050)) < quiet);
    // Reset All Controllers (GM RP-015): expression back to 127, volume stays.
    send(*s, {0xb0, 121, 0});
    CHECK(rms(render(*s, 22050)) == doctest::Approx(quiet).epsilon(0.05));
    send(*s, {0xb0, 7, 100});
    // Pan hard left: the right channel falls silent.
    send(*s, {0xb0, 10, 0});
    const auto left = render(*s, 22050);
    double l = 0, r = 0;
    for (std::size_t i = 0; i < left.size(); i += 2) {
        l += std::abs(left[i]);
        r += std::abs(left[i + 1]);
    }
    CHECK(l > 1000 * r + 1);
    send(*s, {0xb0, 10, 64});
    // Pitch bend fully up: two semitones (the default bend range).
    send(*s, {0xe0, 0x7f, 0x7f});
    CHECK(frequency(render(*s, 22050), 22050) ==
          doctest::Approx(441.0 * std::pow(2.0, 2.0 / 12.0)).epsilon(0.02));
    send(*s, {0xe0, 0x00, 0x40});
    CHECK(frequency(render(*s, 22050), 22050) == doctest::Approx(441.0).epsilon(0.02));
    // Sustain holds a released note until the pedal lifts.
    send(*s, {0xb0, 64, 127});
    send(*s, {0x80, kRootKey, 0});
    render(*s, 2205);
    CHECK(rms(render(*s, 2205)) > 0);
    send(*s, {0xb0, 64, 0});
    render(*s, 2205);
    CHECK(peak(render(*s, 2205)) == 0);
    // Unhandled messages are ignored.
    send(*s, {0xa0, kRootKey, 10});
    send(*s, {0xd0, 10});
    send(*s, {0xb0, 1, 127});
    send(*s, {0xf0, 0x43, 0x10, 0x4c, 0xf7});
    send(*s, {0xfe});
    CHECK(peak(render(*s, 2205)) == 0);
}

TEST_CASE("soundfont synth: a General MIDI SoundFont (GeneralUser GS)") {
    const fs::path sf2 = gm_sf2();
    if (sf2.empty()) {
        MESSAGE("skipped: no GM SoundFont (IS_SF2 or work/sf2/GeneralUser-GS.sf2)");
        return;
    }
    auto s = m::make_soundfont();
    m::SynthConfig cfg;
    cfg.soundfont = sf2;
    cfg.rate = 22050;
    REQUIRE(s->open(cfg));
    send(*s, {0x90, 60, 100});  // acoustic grand piano
    const auto piano = render(*s, 11025);
    CHECK(rms(piano) > 100);
    send(*s, {0xb0, 123, 0});
    render(*s, 22050 * 3);
    CHECK(peak(render(*s, 2205)) == 0);
    send(*s, {0xc0, 80});  // square lead
    send(*s, {0x90, 60, 100});
    const auto lead = render(*s, 11025);
    CHECK(rms(lead) > 100);
    CHECK(std::abs(rms(lead) - rms(piano)) > 0.05 * rms(piano));
    send(*s, {0xb0, 123, 0});
    render(*s, 22050 * 3);
    send(*s, {0x99, 38, 120});  // acoustic snare on channel 10
    CHECK(rms(render(*s, 4410)) > 100);
}

// The gain: each of the four ship-disk files played end to end through GeneralUser GS stays
// below full scale. Reports the peak and RMS in dBFS.
TEST_CASE("soundfont synth: the game's music does not clip at the gain") {
    const fs::path sf2 = gm_sf2();
    const char* orig = env("IS_ORIGINAL_DIR");
    if (sf2.empty() || !orig) {
        MESSAGE("skipped: needs IS_ORIGINAL_DIR and a GM SoundFont");
        return;
    }
    auto s = m::make_soundfont();
    m::SynthConfig cfg;
    cfg.soundfont = sf2;
    cfg.rate = 22050;
    REQUIRE(s->open(cfg));
    for (const std::string name : {"MAIN", "RACE", "ANTHEM", "DEMON"}) {
        CAPTURE(name);
        const auto smf = ship_midi(orig, name);
        if (smf.empty()) {
            MESSAGE("skipped: no ", name, ".MID in IS_ORIGINAL_DIR");
            continue;
        }
        m::MidiFile f;
        std::string why;
        REQUIRE_MESSAGE(f.parse(smf.data(), smf.size(), &why), why);
        s->reset();
        int pk = 0;
        double sum = 0;
        std::size_t frames = 0;
        std::vector<std::int16_t> buf;
        auto advance_to = [&](double ms) {
            const auto target = static_cast<std::size_t>(ms * cfg.rate / 1000.0);
            if (target <= frames) return;
            buf.resize((target - frames) * 2);
            s->render(buf.data(), target - frames);
            for (auto x : buf) {
                pk = std::max(pk, std::abs(int(x)));
                sum += double(x) * x;
            }
            frames = target;
        };
        CHECK(f.microsoft_authored());
        for (const auto& e : f.events()) {
            advance_to(e.ms);
            if (m::dual_mode_skipped(f, e)) continue;  // as the player plays it (M7 s2b fixer)
            if (e.kind == m::MidiEvent::kChannel)
                s->midi(e.bytes, e.len);
            else
                s->midi(f.sysex(e), e.size);
        }
        advance_to(f.length_ms() + 2000);
        const double peak_db = 20 * std::log10(std::max(pk, 1) / 32768.0);
        const double rms_db = 20 * std::log10(std::sqrt(sum / double(frames * 2)) / 32768.0);
        MESSAGE(name, ": ", f.length_ms() / 1000.0, " s, peak ", peak_db, " dBFS, RMS ", rms_db, " dBFS");
        CHECK(pk < 32767);
        CHECK(rms_db > -40.0);
    }
}
