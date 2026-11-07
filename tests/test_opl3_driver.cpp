// The opl3 synth's General MIDI voice driver (src/audio/opl3_driver.*, ADR 0011 decision 3).
// Portable. The register cases drive the driver into a recording sink with a bank built here;
// the sound cases drive it into the Nuked OPL3 chip with the Fat Man bank; the full-mix case
// reads the game's MIDI files through IS_ORIGINAL_DIR and skips without them. Built with
// the opl3 synth only (IS_OPL3=1).
#include "audio/music_opl3.h"

#if defined(IS_OPL3) && IS_OPL3

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "audio/midi_file.h"
#include "audio/opl3_bank.h"
#include "audio/opl3_driver.h"
#include "audio/synth.h"
#include "doctest.h"
#include "formats/szdd.h"
#include "opl3.h"

namespace m = is::audio::music;
namespace fs = std::filesystem;

namespace {

struct Recorder {
    std::vector<std::pair<std::uint16_t, std::uint8_t>> writes;
    std::map<std::uint16_t, std::uint8_t> regs;
    m::Opl3Driver::Write sink() {
        return [this](std::uint16_t r, std::uint8_t v) {
            writes.emplace_back(r, v);
            regs[r] = v;
        };
    }
    bool wrote(std::uint16_t r) const {
        return std::any_of(writes.begin(), writes.end(), [&](auto& w) { return w.first == r; });
    }
    std::uint8_t at(std::uint16_t r) const {
        auto it = regs.find(r);
        REQUIRE(it != regs.end());
        return it->second;
    }
};

// Program 0: FM (connection 0), modulator TL 0x12, carrier TL 0x08 with KSL 1.
// Program 1: additive (connection 1), both TL 0x04.
// Drum key 36: percussion_key 48, note_offset 0. Drum key 37: blank.
m::OplBank test_bank() {
    m::OplBank b{};
    for (auto& i : b.melodic) i = m::OplInstrument{};
    for (auto& i : b.percussion) i = m::OplInstrument{};
    auto& p0 = b.melodic[0];
    p0.feedconn = 0x0c;  // feedback 6, FM
    p0.op[0] = {0x21, 0x12, 0xf2, 0x54, 0x01};
    p0.op[1] = {0x31, 0x48, 0xf3, 0x65, 0x02};
    auto& p1 = b.melodic[1];
    p1.feedconn = 0x01;
    p1.op[0] = {0x01, 0x04, 0xf0, 0x0f, 0x00};
    p1.op[1] = {0x01, 0x04, 0xf0, 0x0f, 0x00};
    auto& d36 = b.percussion[36];
    d36.feedconn = 0x0e;
    d36.percussion_key = 48;
    d36.op[0] = {0x02, 0x0a, 0xfa, 0x07, 0x00};
    d36.op[1] = {0x03, 0x05, 0xfb, 0x08, 0x00};
    b.percussion[37].blank = true;
    return b;
}

void send(m::Opl3Driver& d, std::initializer_list<std::uint8_t> msg) {
    const std::vector<std::uint8_t> b(msg);
    d.midi(b.data(), b.size());
}

int carrier_tl(const Recorder& r, int v) { return r.at(0x40 + m::Opl3Driver::operator_reg(v, 1)) & 0x3f; }
int modulator_tl(const Recorder& r, int v) { return r.at(0x40 + m::Opl3Driver::operator_reg(v, 0)) & 0x3f; }

const char* env(const char* name) {
    const char* v = std::getenv(name);
    return v && *v ? v : nullptr;
}

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

// The driver over a real chip, the way music_opl3.cpp wires it.
struct Chip {
    explicit Chip(std::uint32_t rate, int gain = m::kOpl3GainSteps)
        : chip(std::make_unique<opl3_chip>()),
          drv(m::fatman_bank(), [this](std::uint16_t r, std::uint8_t v) {
              if (direct)
                  OPL3_WriteReg(chip.get(), r, v);
              else
                  OPL3_WriteRegBuffered(chip.get(), r, v);
          }),
          rate(rate) {
        drv.set_gain_steps(gain);
        reset();
    }
    void reset() {
        OPL3_Reset(chip.get(), rate);
        direct = true;
        drv.reset();
        direct = false;
    }
    std::vector<std::int16_t> render(std::size_t frames) {
        std::vector<std::int16_t> out(frames * 2);
        OPL3_GenerateStream(chip.get(), out.data(), static_cast<std::uint32_t>(frames));
        return out;
    }
    std::unique_ptr<opl3_chip> chip;
    bool direct = false;
    m::Opl3Driver drv;
    std::uint32_t rate;
};

// The chip's floor once a voice has sounded: an operator at full attenuation outputs 0 in the
// positive half of its wave and -1 in the negative half (the sign is a one's complement
// [REF Nuked OPL3 opl3.c, OPL3_EnvelopeCalcSin0]), so a silent chip still shows a few LSB
// while the phase generators run. Silence is a peak at or under this (-72 dBFS).
constexpr int kChipFloor = 8;

int peak(const std::vector<std::int16_t>& v) {
    int p = 0;
    for (auto x : v) p = std::max(p, std::abs(static_cast<int>(x)));
    return p;
}

}  // namespace

TEST_CASE("opl3 driver: the reset puts the chip in OPL3 mode with waveform select") {
    const m::OplBank bank = test_bank();
    Recorder r;
    m::Opl3Driver d(bank, r.sink());
    d.reset();
    REQUIRE(!r.writes.empty());
    CHECK(r.writes.front() == std::make_pair<std::uint16_t, std::uint8_t>(0x105, 0x01));
    CHECK((r.at(0x001) & 0x20) == 0x20);
    CHECK(r.at(0x104) == 0x00);
    CHECK(r.at(0x0bd) == 0xc0);  // deep tremolo and vibrato, the Fat Man bank's flags (M7 s2b fixer)
    CHECK(m::kOplDepthBits == 0xc0);
    for (int v = 0; v < m::kOplVoices; ++v) {
        CHECK((r.at(0xb0 + m::Opl3Driver::channel_reg(v)) & 0x20) == 0);  // key off
        CHECK((r.at(0x40 + m::Opl3Driver::operator_reg(v, 1)) & 0x3f) == 0x3f);
        CHECK(d.voice(v).channel == -1);
    }
}

TEST_CASE("opl3 driver: the operator and channel offsets of the 18 channels") {
    // [REF YMF262 data sheet]: channel c of a bank uses the slots {0,1,2,8,9,10,16,17,18}[c]
    // and +3; the second bank repeats the map at 0x100.
    const std::uint16_t mod[18] = {0x000, 0x001, 0x002, 0x008, 0x009, 0x00a, 0x010, 0x011, 0x012,
                                   0x100, 0x101, 0x102, 0x108, 0x109, 0x10a, 0x110, 0x111, 0x112};
    for (int v = 0; v < 18; ++v) {
        CAPTURE(v);
        CHECK(m::Opl3Driver::operator_reg(v, 0) == mod[v]);
        CHECK(m::Opl3Driver::operator_reg(v, 1) == mod[v] + 3);
        CHECK(m::Opl3Driver::channel_reg(v) == (v < 9 ? v : 0x100 + v - 9));
    }
}

TEST_CASE("opl3 driver: a note on writes the instrument, the output bits and the pitch") {
    const m::OplBank bank = test_bank();
    Recorder r;
    m::Opl3Driver d(bank, r.sink());
    d.reset();
    r.writes.clear();
    send(d, {0x90, 60, 127});  // middle C, program 0, centre pan, volume 100
    CHECK(d.voice(0).on);
    CHECK(d.voice(0).channel == 0);
    const auto& ins = bank.melodic[0];
    for (int op = 0; op < 2; ++op) {
        const std::uint16_t o = m::Opl3Driver::operator_reg(0, op);
        CHECK(r.at(0x20 + o) == ins.op[op].avekf);
        CHECK(r.at(0x60 + o) == ins.op[op].ad);
        CHECK(r.at(0x80 + o) == ins.op[op].sr);
        CHECK(r.at(0xe0 + o) == ins.op[op].wave);
        CHECK((r.at(0x40 + o) & 0xc0) == (ins.op[op].ksl_tl & 0xc0));  // KSL kept
    }
    CHECK(modulator_tl(r, 0) == 0x12);                          // FM: the modulator keeps its TL
    CHECK(carrier_tl(r, 0) == 0x08 + m::opl_attenuation_steps(100));  // + volume 100's steps
    CHECK(r.at(0xc0) == (0x0c | 0x30));                          // feedconn, both outputs
    // Middle C: F-number 345 (0x159) at block 4, key on.
    CHECK(r.at(0xa0) == 0x59);
    CHECK(r.at(0xb0) == (0x20 | (4 << 2) | 0x01));
    // The key-on is the last write.
    CHECK(r.writes.back().first == 0xb0);

    // Bend fully down: two semitones under C4 is A#3 (F-number 615, block 3).
    send(d, {0xe0, 0x00, 0x00});
    CHECK(r.at(0xa0) == (615 & 0xff));
    CHECK(r.at(0xb0) == (0x20 | (3 << 2) | (615 >> 8)));
    // Bend fully up (8191 = 2 * 8191/8192 semitones): D4 at block 4, F-number 387 less a hair.
    send(d, {0xe0, 0x7f, 0x7f});
    const auto up = m::opl_pitch(60 + 2.0 * 8191.0 / 8192.0);
    CHECK(up.block == 4);
    CHECK(std::abs(int(up.fnum) - 387) <= 1);
    CHECK(r.at(0xa0) == (up.fnum & 0xff));
    CHECK(r.at(0xb0) == (0x20 | (4 << 2) | (up.fnum >> 8)));
    // RPN 0 sets the range to 12 semitones: fully up is C5 less a hair.
    send(d, {0xb0, 101, 0});
    send(d, {0xb0, 100, 0});
    send(d, {0xb0, 6, 12});
    const auto oct = m::opl_pitch(60 + 12.0 * 8191.0 / 8192.0);
    CHECK(oct.block == 4);  // B4 + 0.99 semitone at B's block
    CHECK(r.at(0xa0) == (oct.fnum & 0xff));
}

TEST_CASE("opl3 driver: the pitch table") {
    CHECK(m::opl_pitch(60).fnum == 345);
    CHECK(m::opl_pitch(60).block == 4);
    CHECK(m::opl_pitch(69).fnum == 580);  // A4 440 Hz: 580 * 49716 / 2^16 = 440.0
    CHECK(m::opl_pitch(72).block == 5);
    CHECK(m::opl_pitch(12).block == 0);
    CHECK(m::opl_pitch(0).block == 0);
    CHECK(m::opl_pitch(0).fnum == 173);   // C-1: half of 345 at block 0
    CHECK(m::opl_pitch(127).block == 7);
    CHECK(m::opl_pitch(127).fnum == 1023);  // G9 is past the chip's top: capped
    for (int n = 12; n < 108; ++n) {
        const auto p = m::opl_pitch(n);
        const double hz = p.fnum * m::kOplClockHz * std::pow(2.0, p.block - 20.0);
        CHECK(std::abs(1200 * std::log2(hz / (440.0 * std::pow(2.0, (n - 69) / 12.0)))) < 5.0);
    }
}

TEST_CASE("opl3 driver: the volume law") {
    CHECK(m::opl_attenuation_steps(127) == 0);
    CHECK(m::opl_attenuation_steps(0) == 63);
    CHECK(m::opl_attenuation_steps(64) == 16);  // 40 log10(64/127) = -11.9 dB
    CHECK(m::opl_attenuation_steps(1) == 63);
    for (int v = 1; v < 127; ++v) CHECK(m::opl_attenuation_steps(v) >= m::opl_attenuation_steps(v + 1));

    const m::OplBank bank = test_bank();
    Recorder r;
    m::Opl3Driver d(bank, r.sink());
    d.reset();
    send(d, {0x90, 60, 1});
    const int soft = carrier_tl(r, 0);
    send(d, {0x90, 62, 127});
    const int loud = carrier_tl(r, 1);
    CHECK(loud < soft);  // a lower TL is louder
    CHECK(soft == 63);

    // CC 7 and CC 11 move the sounding voice's carrier.
    send(d, {0xb0, 7, 127});
    CHECK(carrier_tl(r, 1) == 0x08);
    send(d, {0xb0, 7, 64});
    CHECK(carrier_tl(r, 1) == 0x08 + 16);
    send(d, {0xb0, 11, 64});
    CHECK(carrier_tl(r, 1) == 0x08 + 32);
    CHECK(modulator_tl(r, 1) == 0x12);  // FM: the modulator is not scaled
    send(d, {0xb0, 121, 0});            // reset all controllers: expression 127, volume stays
    CHECK(carrier_tl(r, 1) == 0x08 + 16);

    // Additive: both operators scale.
    send(d, {0xc1, 1});
    send(d, {0xb1, 7, 64});
    send(d, {0x91, 60, 127});
    CHECK(carrier_tl(r, 2) == 0x04 + 16);
    CHECK(modulator_tl(r, 2) == 0x04 + 16);

    // The gain adds its steps to the scaled operators.
    d.set_gain_steps(4);
    send(d, {0xb1, 7, 64});
    CHECK(carrier_tl(r, 2) == 0x04 + 20);
}

TEST_CASE("opl3 driver: pan") {
    const m::OplBank bank = test_bank();
    Recorder r;
    m::Opl3Driver d(bank, r.sink());
    d.reset();
    send(d, {0xb0, 10, 0});
    send(d, {0x90, 60, 100});
    CHECK((r.at(0xc0) & 0x30) == 0x10);  // left only
    send(d, {0xb0, 10, 127});
    CHECK((r.at(0xc0) & 0x30) == 0x20);  // right only, the sounding voice follows
    send(d, {0xb0, 10, 64});
    CHECK((r.at(0xc0) & 0x30) == 0x30);
    send(d, {0xb0, 10, 42});
    CHECK((r.at(0xc0) & 0x30) == 0x10);
    send(d, {0xb0, 10, 85});
    CHECK((r.at(0xc0) & 0x30) == 0x20);
    CHECK((r.at(0xc0) & 0x0f) == 0x0c);  // feedconn kept
}

TEST_CASE("opl3 driver: sustain, all notes off, all sound off") {
    const m::OplBank bank = test_bank();
    Recorder r;
    m::Opl3Driver d(bank, r.sink());
    d.reset();
    send(d, {0xb0, 64, 127});
    send(d, {0x90, 60, 100});
    send(d, {0x80, 60, 0});
    CHECK(d.voice(0).on);
    CHECK(d.voice(0).sustained);
    CHECK((r.at(0xb0) & 0x20) == 0x20);
    send(d, {0xb0, 64, 0});
    CHECK(!d.voice(0).on);
    CHECK((r.at(0xb0) & 0x20) == 0);

    // A note-on with velocity 0 is a note off.
    send(d, {0x90, 64, 100});
    const int v = 1;
    CHECK(d.voice(v).on);
    send(d, {0x90, 64, 0});
    CHECK(!d.voice(v).on);

    // CC 123 keys every voice of the channel off, other channels keep theirs.
    for (int k = 0; k < 5; ++k) send(d, {0x90, std::uint8_t(60 + k), 100});
    send(d, {0x91, 40, 100});
    send(d, {0xb0, 123, 0});
    for (int i = 0; i < m::kOplVoices; ++i) {
        if (d.voice(i).channel == 0) {
            CHECK(!d.voice(i).on);
            CHECK((r.at(0xb0 + m::Opl3Driver::channel_reg(i)) & 0x20) == 0);
        }
        if (d.voice(i).channel == 1) CHECK(d.voice(i).on);
    }
    // CC 120 silences at once: TL 63, fastest release, idle.
    send(d, {0xb1, 120, 0});
    for (int i = 0; i < m::kOplVoices; ++i) CHECK(d.voice(i).channel != 1);
}

TEST_CASE("opl3 driver: a note off ends the oldest voice of its key only (M7 s2b fixer)") {
    const m::OplBank bank = test_bank();
    Recorder r;
    m::Opl3Driver d(bank, r.sink());
    d.reset();
    send(d, {0x90, 60, 100});  // voice 0
    send(d, {0x90, 60, 90});   // voice 1, the same key again
    CHECK(d.voice(0).on);
    CHECK(d.voice(1).on);
    send(d, {0x80, 60, 0});
    CHECK(!d.voice(0).on);
    CHECK(d.voice(1).on);
    send(d, {0x80, 60, 0});
    CHECK(!d.voice(1).on);
    // Under the pedal the two note offs mark one voice each.
    send(d, {0xb0, 64, 127});
    send(d, {0x90, 62, 100});
    send(d, {0x90, 62, 100});
    send(d, {0x80, 62, 0});
    int sustained = 0;
    for (int v = 0; v < m::kOplVoices; ++v) sustained += d.voice(v).sustained;
    CHECK(sustained == 1);
    send(d, {0x80, 62, 0});
    sustained = 0;
    for (int v = 0; v < m::kOplVoices; ++v) sustained += d.voice(v).sustained;
    CHECK(sustained == 2);
    send(d, {0xb0, 64, 0});
    for (int v = 0; v < m::kOplVoices; ++v) CHECK(!d.voice(v).on);
}

TEST_CASE("opl3 driver: voice allocation and stealing") {
    const m::OplBank bank = test_bank();
    Recorder r;
    m::Opl3Driver d(bank, r.sink());
    d.reset();
    for (int k = 0; k < 18; ++k) send(d, {0x90, std::uint8_t(40 + k), 100});
    for (int v = 0; v < 18; ++v) {
        CHECK(d.voice(v).on);
        CHECK(d.voice(v).key == 40 + v);
    }
    // The 19th note steals the oldest (key 40 on voice 0).
    send(d, {0x90, 80, 100});
    CHECK(d.voice(0).key == 80);
    CHECK(d.voice(0).on);
    // The steal keyed the old note off before the new key-on.
    const std::uint16_t b0 = 0xb0;
    int off = -1, on = -1;
    for (int i = int(r.writes.size()) - 1; i >= 0; --i) {
        if (r.writes[i].first != b0) continue;
        if (r.writes[i].second & 0x20) {
            if (on < 0) on = i;
        } else if (on >= 0) {
            off = i;
            break;
        }
    }
    CHECK(off >= 0);
    CHECK(off < on);
    // Released voices go before sounding ones, the longest released first.
    send(d, {0x80, 50, 0});  // voice 10
    send(d, {0x80, 45, 0});  // voice 5
    send(d, {0x90, 81, 100});
    CHECK(d.voice(10).key == 81);
    send(d, {0x90, 82, 100});
    CHECK(d.voice(5).key == 82);
    send(d, {0x90, 83, 100});  // none released: the oldest sounding (key 41, voice 1)
    CHECK(d.voice(1).key == 83);
}

TEST_CASE("opl3 driver: channel 10 plays the percussion entry at its key") {
    const m::OplBank bank = test_bank();
    Recorder r;
    m::Opl3Driver d(bank, r.sink());
    d.reset();
    send(d, {0xc9, 5});         // program changes on the drum channel do not pick a melodic voice
    send(d, {0x99, 36, 127});
    const auto& ins = bank.percussion[36];
    CHECK(d.voice(0).channel == 9);
    CHECK(r.at(0x20 + m::Opl3Driver::operator_reg(0, 0)) == ins.op[0].avekf);
    CHECK(r.at(0x20 + m::Opl3Driver::operator_reg(0, 1)) == ins.op[1].avekf);
    CHECK((r.at(0xc0) & 0x0f) == 0x0e);
    const auto p = m::opl_pitch(48);  // the entry's percussion key, not 36
    CHECK(r.at(0xa0) == (p.fnum & 0xff));
    CHECK(r.at(0xb0) == (0x20 | (p.block << 2) | (p.fnum >> 8)));
    send(d, {0x99, 37, 127});  // blank entry: no voice
    CHECK(d.voice(1).channel == -1);
    send(d, {0x89, 36, 0});
    CHECK(!d.voice(0).on);
}

TEST_CASE("opl3 driver: a GM reset SysEx resets the driver") {
    const m::OplBank bank = test_bank();
    Recorder r;
    m::Opl3Driver d(bank, r.sink());
    d.reset();
    send(d, {0x90, 60, 100});
    CHECK(d.voice(0).on);
    send(d, {0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7});
    CHECK(d.voice(0).channel == -1);
}

TEST_CASE("opl3 driver: the Fat Man bank on the Nuked chip sounds and stops") {
    for (std::uint32_t rate : {22050u, 11025u}) {
        CAPTURE(rate);
        Chip c(rate);
        CHECK(peak(c.render(rate / 10)) == 0);
        send(c.drv, {0x90, 60, 110});            // acoustic grand piano, middle C
        send(c.drv, {0x99, 38, 110});            // acoustic snare
        const auto on = c.render(rate / 2);
        CHECK(peak(on) > 500);
        CHECK(peak(on) < 32767);
        send(c.drv, {0xb0, 123, 0});
        send(c.drv, {0xb9, 123, 0});
        c.render(rate * 3);                       // the release
        CHECK(peak(c.render(rate / 10)) <= kChipFloor);
        // CC 120 cuts a sounding note at once.
        send(c.drv, {0xc1, 48});                  // strings: a long release
        send(c.drv, {0x91, 64, 120});
        CHECK(peak(c.render(rate / 4)) > 500);
        send(c.drv, {0xb1, 120, 0});
        c.render(rate / 20);
        CHECK(peak(c.render(rate / 10)) <= kChipFloor);
    }
}

// The gain: each of the four ship-disk files played end to end through the player stays below
// full scale (no sample at the chip's clip). Reports the peak and RMS in dBFS.
TEST_CASE("opl3 driver: the game's music does not clip at the gain") {
    const char* orig = env("IS_ORIGINAL_DIR");
    if (!orig) {
        MESSAGE("skipped: needs IS_ORIGINAL_DIR");
        return;
    }
    const std::uint32_t rate = 22050;
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
        auto s = m::make_opl3();
        REQUIRE(s);
        m::SynthConfig cfg;
        cfg.rate = rate;
        REQUIRE(s->open(cfg));
        int pk = 0;
        long clipped = 0;
        double sum = 0;
        std::size_t frames = 0;
        std::vector<std::int16_t> buf;
        auto advance_to = [&](double ms) {
            const auto target = static_cast<std::size_t>(ms * rate / 1000.0);
            if (target <= frames) return;
            buf.resize((target - frames) * 2);
            s->render(buf.data(), target - frames);
            for (auto x : buf) {
                pk = std::max(pk, std::abs(int(x)));
                if (x == 32767 || x == -32768) ++clipped;
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
        MESSAGE(name, ": ", f.length_ms() / 1000.0, " s, peak ", peak_db, " dBFS, RMS ", rms_db,
                " dBFS, clipped samples ", clipped);
        CHECK(clipped == 0);
        CHECK(pk < 32767);
        CHECK(rms_db > -40.0);
    }
}

#endif  // IS_OPL3
