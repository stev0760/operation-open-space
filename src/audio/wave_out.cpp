// The output (wave_out.h). The waveOut calls are Win32; the dump and the device-less clock
// are portable.
#include "audio/wave_out.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

namespace is::audio {

namespace {
constexpr std::size_t kBlockFrames = 512;  // 23 ms at 22,050 Hz
constexpr int kBlocks = 4;

void put32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
void put16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
}  // namespace

// ---------------------------------------------------------------- WAV writer

bool WavWriter::open(const std::filesystem::path& path, std::uint32_t rate) {
    close();
#ifdef _WIN32
    f_ = _wfopen(path.wstring().c_str(), L"wb");
#else
    f_ = std::fopen(path.string().c_str(), "wb");
#endif
    rate_ = rate;
    frames_ = 0;
    if (f_) header();
    return f_ != nullptr;
}

void WavWriter::header() {
    std::uint8_t h[44] = {};
    const std::uint32_t data = static_cast<std::uint32_t>(frames_ * 4);
    std::memcpy(h, "RIFF", 4);
    put32(h + 4, 36 + data);
    std::memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16);
    put16(h + 20, 1);
    put16(h + 22, 2);
    put32(h + 24, rate_);
    put32(h + 28, rate_ * 4);
    put16(h + 32, 4);
    put16(h + 34, 16);
    std::memcpy(h + 36, "data", 4);
    put32(h + 40, data);
    std::fseek(f_, 0, SEEK_SET);
    std::fwrite(h, 1, sizeof h, f_);
    std::fseek(f_, 0, SEEK_END);
}

void WavWriter::write(const std::int16_t* frames, std::size_t count) {
    if (!f_) return;
    std::vector<std::uint8_t> b(count * 4);
    for (std::size_t i = 0; i < count * 2; ++i)
        put16(b.data() + 2 * i, static_cast<std::uint16_t>(frames[i]));
    std::fwrite(b.data(), 1, b.size(), f_);
    frames_ += count;
    header();
    std::fflush(f_);
}

void WavWriter::close() {
    if (!f_) return;
    header();
    std::fclose(f_);
    f_ = nullptr;
}

// ---------------------------------------------------------------- device

int WaveOut::device_count() {
#ifdef _WIN32
    return static_cast<int>(waveOutGetNumDevs());
#else
    return 0;
#endif
}

void WaveOut::dump_to(const std::filesystem::path& wav) {
    std::lock_guard<std::mutex> lock(dump_mu_);
    dump_.close();
    if (!wav.empty()) dump_.open(wav, rate_);
}

void WaveOut::deliver(const std::int16_t* frames, std::size_t count) {
    std::lock_guard<std::mutex> lock(dump_mu_);
    if (dump_.is_open()) dump_.write(frames, count);
}

bool WaveOut::open() {
    if (thread_.joinable()) return device_;
    device_ = false;
#ifdef _WIN32
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    // 22,050 Hz first, as a modern device takes it; 11,025 Hz when that fails [DESIGN].
    for (std::uint32_t rate : {22050u, 11025u}) {
        WAVEFORMATEX f{};
        f.wFormatTag = WAVE_FORMAT_PCM;
        f.nChannels = 2;
        f.nSamplesPerSec = rate;
        f.wBitsPerSample = 16;
        f.nBlockAlign = 4;
        f.nAvgBytesPerSec = rate * 4;
        HWAVEOUT h = nullptr;
        if (waveOutOpen(&h, WAVE_MAPPER, &f, reinterpret_cast<DWORD_PTR>(event_), 0,
                        CALLBACK_EVENT) == MMSYSERR_NOERROR) {
            handle_ = h;
            rate_ = rate;
            device_ = true;
            break;
        }
    }
#endif
    stop_ = false;
    thread_ = std::thread([this] { run(); });
    return device_;
}

void WaveOut::close() {
    if (thread_.joinable()) {
        stop_ = true;
#ifdef _WIN32
        if (event_) SetEvent(static_cast<HANDLE>(event_));
#endif
        thread_.join();
    }
#ifdef _WIN32
    if (handle_) {
        waveOutReset(static_cast<HWAVEOUT>(handle_));
        waveOutClose(static_cast<HWAVEOUT>(handle_));
        handle_ = nullptr;
    }
    if (event_) {
        CloseHandle(static_cast<HANDLE>(event_));
        event_ = nullptr;
    }
#endif
    device_ = false;
    std::lock_guard<std::mutex> lock(dump_mu_);
    dump_.close();
}

void WaveOut::run() {
#ifdef _WIN32
    if (device_) {
        HWAVEOUT h = static_cast<HWAVEOUT>(handle_);
        std::vector<std::vector<std::int16_t>> data(kBlocks,
                                                    std::vector<std::int16_t>(kBlockFrames * 2));
        std::vector<WAVEHDR> hdr(kBlocks);
        for (int i = 0; i < kBlocks; ++i) {
            hdr[i] = WAVEHDR{};
            hdr[i].lpData = reinterpret_cast<LPSTR>(data[i].data());
            hdr[i].dwBufferLength = static_cast<DWORD>(kBlockFrames * 4);
            waveOutPrepareHeader(h, &hdr[i], sizeof(WAVEHDR));
            hdr[i].dwFlags |= WHDR_DONE;
        }
        while (!stop_) {
            for (int i = 0; i < kBlocks; ++i) {
                if (!(hdr[i].dwFlags & WHDR_DONE)) continue;
                mixer_.render(data[i].data(), kBlockFrames, rate_);
                deliver(data[i].data(), kBlockFrames);
                hdr[i].dwFlags &= ~static_cast<DWORD>(WHDR_DONE);
                waveOutWrite(h, &hdr[i], sizeof(WAVEHDR));
            }
            WaitForSingleObject(static_cast<HANDLE>(event_), 100);
        }
        waveOutReset(h);
        for (int i = 0; i < kBlocks; ++i) waveOutUnprepareHeader(h, &hdr[i], sizeof(WAVEHDR));
        return;
    }
#endif
    // No device: the mixer still advances in real time, for the dump and the completions.
    std::vector<std::int16_t> block(kBlockFrames * 2);
    auto next = std::chrono::steady_clock::now();
    const auto period = std::chrono::microseconds(kBlockFrames * 1000000ull / rate_);
    while (!stop_) {
        mixer_.render(block.data(), kBlockFrames, rate_);
        deliver(block.data(), kBlockFrames);
        next += period;
        std::this_thread::sleep_until(next);
    }
}

}  // namespace is::audio
