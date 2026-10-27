// The output of the mixer (ADR 0011 decision 1): a waveOut device at 22,050 Hz 16-bit stereo
// (11,025 Hz when the device takes it natively), fed from its own thread with
// Mixer::render, and the IS_AUDIO_DUMP WAV writer [DESIGN]. Without a device (or off Windows)
// the thread renders in real time for the dump alone.
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

#include "audio/mixer.h"

namespace is::audio {

// A 16-bit stereo PCM WAV file whose header is rewritten after every block, so a killed
// process leaves a readable file.
class WavWriter {
public:
    bool open(const std::filesystem::path& path, std::uint32_t rate);
    void write(const std::int16_t* frames, std::size_t count);
    void close();
    bool is_open() const { return f_ != nullptr; }
    ~WavWriter() { close(); }

private:
    void header();
    std::FILE* f_ = nullptr;
    std::uint32_t rate_ = 0;
    std::uint64_t frames_ = 0;
};

class WaveOut {
public:
    explicit WaveOut(Mixer& mixer) : mixer_(mixer) {}
    ~WaveOut() { close(); }
    // Opens the device and starts the thread; false when no device opens (the thread still
    // runs, so a dump is written).
    bool open();
    void close();
    bool device_open() const { return device_; }
    // A waveOutPrepareHeader or waveOutWrite failed and the thread went on without the device
    // (M7 s3 fixer).
    bool device_failed() const { return failed_.load(); }
    std::uint32_t rate() const { return rate_; }
    void dump_to(const std::filesystem::path& wav);

    // The device ids are not the port's to change: no waveOutSetVolume [DESIGN] ADR 0011.
    static int device_count();

private:
    void run();
    void deliver(const std::int16_t* frames, std::size_t count);

    Mixer& mixer_;
    std::uint32_t rate_ = 22050;
    bool device_ = false;
    void* handle_ = nullptr;  // HWAVEOUT
    void* event_ = nullptr;   // HANDLE
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> failed_{false};
    std::mutex dump_mu_;
    WavWriter dump_;
};

}  // namespace is::audio
