// The waveOut output (src/audio/wave_out.h) under Windows or Wine: the device opens, a second of
// silence renders without error, and the dump writer records it. Skips without a device.
#ifdef _WIN32
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

#include "audio/mixer.h"
#include "audio/wave_out.h"
#include "doctest.h"

namespace a = is::audio;
namespace fs = std::filesystem;

TEST_CASE("audio win32: waveOut renders a second of silence") {
    a::Mixer mixer;
    mixer.configure(4);
    a::WaveOut out(mixer);
    const fs::path dump = fs::temp_directory_path() / "is_audio_test_dump.wav";
    const bool device = out.open();
    out.dump_to(dump);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    out.close();
    std::error_code ec;
    const auto size = fs::file_size(dump, ec);
    REQUIRE_FALSE(ec);
    // About a second of 16-bit stereo at the device rate, give or take the buffering.
    const double seconds = static_cast<double>(size - 44) / (out.rate() * 4.0);
    CHECK(seconds > 0.5);
    CHECK(seconds < 1.6);
    fs::remove(dump, ec);
    if (a::WaveOut::device_count() == 0) {
        MESSAGE("no waveOut device: rendered for the dump only");
    } else {
        CHECK(device);
    }
}
#endif
