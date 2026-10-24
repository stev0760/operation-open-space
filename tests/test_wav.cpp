// RIFF WAVE parsing: synthetic cases, then golden tests against SND_FX.DLL and ENG_FX.DLL
// and the .wav files `isre res` writes. The golden tests skip when IS_ORIGINAL_DIR or
// IS_ASSETS_DIR is unset or missing.
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "doctest.h"
#include "formats/wav.h"
#include "ne_builder.h"

using namespace is::fmt;
namespace fs = std::filesystem;

namespace {
void p16(std::vector<std::uint8_t>& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void p32(std::vector<std::uint8_t>& b, unsigned v) {
    p16(b, v & 0xFFFF);
    p16(b, v >> 16);
}
void tag(std::vector<std::uint8_t>& b, const char* t) { b.insert(b.end(), t, t + 4); }

// RIFF WAVE: fmt (`fmt_len` 16, PCMWAVEFORMAT, or 14, WAVEFORMAT without the bits word), an
// odd LIST chunk with its pad byte, then a 5-byte data chunk whose pad byte is absent; then
// 7 bytes of resource padding. The six fmt values differ from each other, so a test sees
// two fields swapped: PCM, 2 channels, 22,050 Hz, 88,200 bytes/s, 4-byte blocks, 16 bits.
std::vector<std::uint8_t> wave(unsigned fmt_len = 16) {
    std::vector<std::uint8_t> b;
    tag(b, "RIFF");
    p32(b, 4 + (8 + fmt_len) + 12 + 13);
    tag(b, "WAVE");
    tag(b, "fmt ");
    p32(b, fmt_len);
    p16(b, 1); p16(b, 2); p32(b, 22050); p32(b, 88200); p16(b, 4);
    if (fmt_len >= 16) p16(b, 16);
    tag(b, "LIST");
    p32(b, 3);
    b.push_back('a'); b.push_back('b'); b.push_back('c'); b.push_back(0);
    tag(b, "data");
    p32(b, 5);
    for (int i = 0; i < 5; ++i) b.push_back(static_cast<std::uint8_t>(0x80 + i));
    b.insert(b.end(), 7, 0xEE);
    return b;
}
}  // namespace

TEST_CASE("a RIFF WAVE file: fmt facts, data span and the real file size") {
    auto b = wave();
    Wave w = parse_wav(b.data(), b.size());
    CHECK(w.format_tag == 1);
    CHECK(w.channels == 2);
    CHECK(w.rate == 22050);
    CHECK(w.byte_rate == 88200);
    CHECK(w.block_align == 4);
    CHECK(w.bits == 16);
    CHECK(w.data_offset == 12 + 24 + 12 + 8);
    CHECK(w.data_size == 5);
    CHECK(w.frames() == 1);  // whole blocks only
    CHECK(w.file_size == b.size() - 7);
    CHECK(b[w.data_offset] == 0x80);

    NeModule m(build_ne("ISND_FX", {{kWavResourceType, 100, "", b}}));
    const NeResource* r = m.find(kWavResourceType, 100);
    REQUIRE(r);
    CHECK(m.size(*r) == 80);  // 68 bytes padded to 16
    auto f = wav_file(m, *r);
    CHECK(f.size() == w.file_size);
    CHECK(std::equal(f.begin(), f.end(), b.begin()));
}

TEST_CASE("a 14-byte fmt chunk (WAVEFORMAT) has no bits field") {
    auto b = wave(14);
    Wave w = parse_wav(b.data(), b.size());
    CHECK(w.format_tag == 1);
    CHECK(w.channels == 2);
    CHECK(w.rate == 22050);
    CHECK(w.byte_rate == 88200);
    CHECK(w.block_align == 4);
    CHECK(w.bits == 0);
    CHECK(w.data_offset == 12 + 22 + 12 + 8);
    CHECK(w.data_size == 5);
    CHECK(b[w.data_offset] == 0x80);
    CHECK(w.file_size == b.size() - 7);
}

TEST_CASE("bad RIFF WAVE files are rejected") {
    auto b = wave();
    auto bad = b;
    bad[8] = 'X';  // not WAVE
    CHECK_THROWS_AS(parse_wav(bad.data(), bad.size()), FormatError);
    bad = b;
    bad[4] = 0xFF;  // RIFF size past the data
    bad[5] = 0x7F;
    CHECK_THROWS_AS(parse_wav(bad.data(), bad.size()), FormatError);
    bad = b;
    bad[12 + 24 + 4] = 40;  // LIST chunk past the RIFF size
    CHECK_THROWS_AS(parse_wav(bad.data(), bad.size()), FormatError);
    bad = b;
    std::memcpy(bad.data() + 12 + 24 + 12, "junk", 4);  // no data chunk
    CHECK_THROWS_AS(parse_wav(bad.data(), bad.size()), FormatError);
    bad = b;
    std::memcpy(bad.data() + 12, "junk", 4);  // no fmt chunk
    CHECK_THROWS_AS(parse_wav(bad.data(), bad.size()), FormatError);
    bad.assign(b.begin(), b.begin() + 10);
    CHECK_THROWS_AS(parse_wav(bad.data(), bad.size()), FormatError);
    bad = b;
    bad[16] = 12;  // fmt chunk shorter than WAVEFORMAT
    CHECK_THROWS_WITH_AS(parse_wav(bad.data(), bad.size()), "WAVE fmt chunk too short", FormatError);

    // RIFF size 0, then a data chunk header claiming 0xFFFFFFFF bytes: the RIFF ends before
    // its own "WAVE" tag, and nothing outside it is read
    bad.clear();
    tag(bad, "RIFF");
    p32(bad, 0);
    tag(bad, "WAVE");
    tag(bad, "data");
    p32(bad, 0xFFFFFFFF);
    CHECK_THROWS_WITH_AS(parse_wav(bad.data(), bad.size()), "RIFF size too small", FormatError);
    bad[4] = 3;
    CHECK_THROWS_WITH_AS(parse_wav(bad.data(), bad.size()), "RIFF size too small", FormatError);
    bad[4] = 4;  // just the tag: a valid size, but no chunks
    CHECK_THROWS_WITH_AS(parse_wav(bad.data(), bad.size()), "WAVE lacks a fmt or data chunk", FormatError);
}

// ---- golden

namespace {
fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v || !fs::is_directory(v)) return {};
    return fs::path(v);
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// The distribution mixes cases (eng_fx.dll); look names up ignoring case.
fs::path find_ci(const fs::path& dir, const std::string& name) {
    if (dir.empty() || !fs::is_directory(dir)) return {};
    for (const auto& e : fs::directory_iterator(dir))
        if (upper(e.path().filename().string()) == upper(name)) return e.path();
    return {};
}
}  // namespace

TEST_CASE("SND_FX.DLL and ENG_FX.DLL: every #100 WAV parses and equals isre's .wav") {
    const fs::path dir = env_dir("IS_ORIGINAL_DIR");
    const fs::path assets = env_dir("IS_ASSETS_DIR");
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    if (assets.empty()) MESSAGE("skipped: IS_ASSETS_DIR not available for the .wav comparison");
    // docs/re/resources.md#custom-types: 46 and 32 WAVs, ids 100-178 and 138-180,
    // all PCM 11,025 Hz 8-bit mono
    struct Module {
        const char* file;
        const char* module;
        std::size_t count;
        std::uint16_t lo, hi;
    };
    const Module mods[] = {{"SND_FX.DLL", "ISND_FX", 46, 100, 178}, {"ENG_FX.DLL", "VOICE_FX", 32, 138, 180}};
    for (const Module& mod : mods) {
        INFO(mod.file);
        const fs::path f = find_ci(dir, mod.file);
        REQUIRE(!f.empty());
        NeModule m = NeModule::load(f);
        CHECK(m.module_name() == mod.module);
        CHECK(m.resources().size() == mod.count);  // nothing but the WAVs
        const auto wavs = m.all(kWavResourceType);
        REQUIRE(wavs.size() == mod.count);
        std::uint16_t lo = 0xFFFF, hi = 0;
        for (const NeResource* r : wavs) {
            INFO("#100 " << r->id);
            lo = std::min(lo, r->id);
            hi = std::max(hi, r->id);
            const Wave w = parse_wav(m.data(*r), m.size(*r));
            CHECK(w.format_tag == 1);
            CHECK(w.channels == 1);
            CHECK(w.rate == 11025);
            CHECK(w.bits == 8);
            CHECK(w.block_align == 1);
            CHECK(w.frames() == w.data_size);
            CHECK(w.data_size > 0);
            CHECK(w.file_size <= m.size(*r));
            CHECK(m.size(*r) - w.file_size < 16);  // only the 16-byte alignment padding is cut
            if (!assets.empty())
                CHECK(wav_file(m, *r) == read_file(assets / lower(mod.file) / "100" /
                                                   (std::to_string(r->id) + ".wav")));
        }
        CHECK(lo == mod.lo);
        CHECK(hi == mod.hi);
    }
}

TEST_CASE("ENG_FX.DLL #100 156 holds the samples of THEWORKS ATTACK.WAV") {
    const fs::path dir = env_dir("IS_ORIGINAL_DIR");
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const fs::path base = dir.has_filename() ? dir : dir.parent_path();  // drop a trailing separator
    const fs::path loose = find_ci(base.parent_path() / "theworks", "ATTACK.WAV");
    if (loose.empty()) {
        MESSAGE("THEWORKS ATTACK.WAV not found next to IS_ORIGINAL_DIR; not compared");
        return;
    }
    NeModule m = NeModule::load(find_ci(dir, "ENG_FX.DLL"));
    const NeResource* r = m.find(kWavResourceType, 156);
    REQUIRE(r);
    const Wave w = parse_wav(m.data(*r), m.size(*r));
    const auto file = read_file(loose);
    const Wave lw = parse_wav(file.data(), file.size());
    REQUIRE(w.data_size == lw.data_size);
    CHECK(std::equal(m.data(*r) + w.data_offset, m.data(*r) + w.data_offset + w.data_size,
                     file.begin() + static_cast<std::ptrdiff_t>(lw.data_offset)));
}
