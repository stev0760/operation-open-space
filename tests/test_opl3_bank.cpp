// The opl3 synth's instrument bank (src/audio/opl3_bank.h, ADR 0011 decision 3): the embedded
// Fat Man 2-operator bank and the WOPL3 reader, on the embedded file and on hand-built images.
#include <cstdint>
#include <vector>

#include "audio/opl3_bank.h"
#include "doctest.h"

namespace m = is::audio::music;

namespace {

// A WOPL3 image: `melodic` and `percussion` banks of 128 records each, version 1..3.
// Record k of bank b gets recognisable bytes so the reader's offsets show.
std::vector<std::uint8_t> wopl_image(std::uint16_t version, std::uint16_t melodic, std::uint16_t percussion) {
    std::vector<std::uint8_t> v = {'W', 'O', 'P', 'L', '3', '-', 'B', 'A', 'N', 'K', 0};
    v.push_back(static_cast<std::uint8_t>(version & 0xFF));  // little-endian version
    v.push_back(static_cast<std::uint8_t>(version >> 8));
    v.push_back(static_cast<std::uint8_t>(melodic >> 8));     // big-endian counts
    v.push_back(static_cast<std::uint8_t>(melodic & 0xFF));
    v.push_back(static_cast<std::uint8_t>(percussion >> 8));
    v.push_back(static_cast<std::uint8_t>(percussion & 0xFF));
    v.push_back(0x03);  // deep tremolo, deep vibrato
    v.push_back(0x04);  // Win9x volume model
    if (version >= 2)
        for (int b = 0; b < melodic + percussion; ++b)
            for (int i = 0; i < 34; ++i) v.push_back(0xEE);  // bank names and LSB/MSB, skipped
    const std::size_t record = version >= 3 ? 66 : 62;
    for (int b = 0; b < melodic + percussion; ++b) {
        const bool drums = b >= melodic;
        for (int k = 0; k < 128; ++k) {
            std::vector<std::uint8_t> r(record, 0);
            r[0] = 'X';
            r[32] = 0xFF;  // note_offset1 = -12 (big-endian)
            r[33] = 0xF4;
            r[34] = 0x00;  // note_offset2, ignored
            r[35] = 0x07;
            r[36] = 0xFE;  // velocity offset -2
            r[37] = 0x05;  // second voice detune, ignored
            r[38] = drums ? static_cast<std::uint8_t>(k + 1) : 0;
            r[39] = k == 5 ? 0x04 : k == 6 ? 0x01 : k == 7 ? 0x02 : 0x00;
            r[40] = 0x0B;  // feedback 5, additive
            r[41] = 0x33;
            for (int op = 0; op < 4; ++op)
                for (int i = 0; i < 5; ++i)
                    r[42 + op * 5 + i] = static_cast<std::uint8_t>((b << 7) | (op << 4) | i);
            if (record == 66) {
                r[62] = 0x12;  // delay on 0x1234
                r[63] = 0x34;
                r[64] = 0x01;  // delay off 0x0190
                r[65] = static_cast<std::uint8_t>(0x90);
            }
            r[42] = static_cast<std::uint8_t>(r[42] ^ k);  // carrier 1's 0x20 byte carries the key
            v.insert(v.end(), r.begin(), r.end());
        }
    }
    return v;
}

}  // namespace

TEST_CASE("opl3 bank: the embedded Fat Man bank parses, 2-operator throughout") {
    const m::OplBank& bank = m::fatman_bank();
    CHECK(&bank == &m::fatman_bank());
    int melodic = 0, drums = 0, four_op = 0;
    for (const auto& i : bank.melodic) {
        melodic += !i.blank;
        four_op += i.four_op;
    }
    for (const auto& i : bank.percussion) {
        drums += !i.blank;
        four_op += i.four_op;
    }
    CHECK(melodic == 128);
    CHECK(drums == 53);
    CHECK(four_op == 0);
    for (int k = 0; k < 128; ++k) CHECK(bank.percussion[k].blank == (k < 35 || k > 87));
}

TEST_CASE("opl3 bank: program 0 is the Fat Man piano, modulator then carrier") {
    const m::OplInstrument& piano = m::fatman_bank().melodic[0];
    CHECK_FALSE(piano.blank);
    CHECK(piano.feedconn == 0x08);  // feedback 4, FM
    CHECK(piano.note_offset == 0);
    // The file stores carrier 1 first (0x01 0x06 0xF2 0xF7 0x00), then modulator 1
    // (0x01 0x8F 0xF2 0xF4 0x00); the struct holds modulator, carrier.
    CHECK(piano.op[1].avekf == 0x01);
    CHECK(piano.op[1].ksl_tl == 0x06);
    CHECK(piano.op[1].ad == 0xF2);
    CHECK(piano.op[1].sr == 0xF7);
    CHECK(piano.op[1].wave == 0x00);
    CHECK(piano.op[0].ksl_tl == 0x8F);
    CHECK(piano.op[0].sr == 0xF4);
    CHECK(piano.ms_sound_kon == 9006);
    CHECK(piano.ms_sound_koff == 400);
}

TEST_CASE("opl3 bank: the drums the game's MIDI files strike") {
    const m::OplBank& bank = m::fatman_bank();
    CHECK(bank.percussion[36].percussion_key == 35);  // bass drum 1
    CHECK(bank.percussion[38].percussion_key == 48);  // acoustic snare
    CHECK_FALSE(bank.percussion[36].blank);
    CHECK_FALSE(bank.percussion[38].blank);
    // Channel 10 (and 16) keys of ANTHEM, DEMON, MAIN and RACE.MID; MAIN.MID also strikes 97 and
    // 98, outside the General MIDI drum map, which the bank leaves blank.
    for (int k : {38, 41, 42, 44, 45, 46, 47, 48, 49, 50, 53, 54, 62, 63, 64, 65, 69, 70, 76, 77, 80,
                  81, 82, 83}) {
        CAPTURE(k);
        CHECK_FALSE(bank.percussion[k].blank);
        CHECK(bank.percussion[k].percussion_key != 0);
    }
    CHECK(bank.percussion[97].blank);
    CHECK(bank.percussion[98].blank);
}

TEST_CASE("opl3 bank: a hand-built WOPL image round-trips, v1, v2 and v3") {
    for (std::uint16_t version : {1, 2, 3}) {
        CAPTURE(version);
        const auto img = wopl_image(version, 2, 1);  // two melodic banks: drums come after both
        m::OplBank bank;
        REQUIRE(m::parse_wopl(img.data(), img.size(), bank));
        const m::OplInstrument& i = bank.melodic[10];
        CHECK_FALSE(i.blank);
        CHECK(i.note_offset == -12);
        CHECK(i.velocity_offset == -2);
        CHECK(i.percussion_key == 0);
        CHECK(i.feedconn == 0x0B);
        CHECK(i.op[1].avekf == (0x00 ^ 10));  // carrier 1, bank 0
        CHECK(i.op[1].wave == 0x04);
        CHECK(i.op[0].avekf == 0x10);         // modulator 1
        CHECK(i.op[0].ksl_tl == 0x11);
        CHECK(i.op[0].ad == 0x12);
        CHECK(i.op[0].sr == 0x13);
        CHECK(i.op[0].wave == 0x14);
        CHECK(i.ms_sound_kon == (version >= 3 ? 0x1234 : 0));
        CHECK(i.ms_sound_koff == (version >= 3 ? 0x0190 : 0));
        CHECK(bank.melodic[5].blank);
        CHECK(bank.melodic[6].four_op);
        CHECK(bank.melodic[7].four_op);  // pseudo 4-op: played as its first voice
        CHECK_FALSE(bank.melodic[8].four_op);
        // Percussion: the third bank in the file (b = 2).
        CHECK(bank.percussion[36].percussion_key == 37);
        CHECK(bank.percussion[36].op[1].avekf == ((2 << 7 & 0xFF) ^ 36));
        CHECK(bank.percussion[36].op[0].avekf == static_cast<std::uint8_t>((2 << 7) | 0x10));
    }
}

TEST_CASE("opl3 bank: bad images fail and leave the output alone") {
    const auto good = wopl_image(3, 1, 1);
    m::OplBank bank;
    bank.melodic[0].note_offset = 99;
    for (std::size_t cut : {std::size_t{0}, std::size_t{10}, std::size_t{18}, std::size_t{19 + 68},
                            good.size() - 1}) {
        CAPTURE(cut);
        CHECK_FALSE(m::parse_wopl(good.data(), cut, bank));
    }
    CHECK_FALSE(m::parse_wopl(nullptr, 100, bank));
    auto magic = good;
    magic[4] = '2';
    CHECK_FALSE(m::parse_wopl(magic.data(), magic.size(), bank));
    auto newer = good;
    newer[11] = 4;
    CHECK_FALSE(m::parse_wopl(newer.data(), newer.size(), bank));
    auto none = wopl_image(3, 0, 1);
    CHECK_FALSE(m::parse_wopl(none.data(), none.size(), bank));
    CHECK(bank.melodic[0].note_offset == 99);
    CHECK(m::parse_wopl(good.data(), good.size(), bank));
    CHECK(bank.melodic[0].note_offset == -12);
}
