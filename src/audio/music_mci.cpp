// The MCI sequencer backend (ADR 0011 decision 3, `mci`): ISPACE.DLL's MCI sequence command for
// command (docs/re/sound.md "MCI sequence", "Midi Mapper box"). The track logic above it is
// music_tracks.cpp. Every MCI call is logged with its flags and result.
#include "audio/music.h"
#include "audio/music_tracks.h"

#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

#include <cstdio>

namespace is::audio::music {

namespace {

const MciSeam* g_seam = nullptr;
std::string g_question;  // string 2219, cp1252

// The program's log format ("inspace: ", src/app/main.cpp), written here because the audio
// library does not link the program [DESIGN].
void log(const std::string& s) {
    const std::string line = "inspace: music: " + s + "\n";
    std::fputs(line.c_str(), stderr);
    std::fflush(stderr);
    OutputDebugStringA(line.c_str());
}

std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                      nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr,
                        nullptr);
    return s;
}

std::wstring widen(const std::string& cp1252) {
    if (cp1252.empty()) return {};
    const int n = MultiByteToWideChar(1252, 0, cp1252.data(), static_cast<int>(cp1252.size()),
                                      nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(1252, 0, cp1252.data(), static_cast<int>(cp1252.size()), w.data(), n);
    return w;
}

const char* name(UINT msg) {
    switch (msg) {
    case MCI_OPEN: return "OPEN";
    case MCI_CLOSE: return "CLOSE";
    case MCI_PLAY: return "PLAY";
    case MCI_SEEK: return "SEEK";
    case MCI_PAUSE: return "PAUSE";
    case MCI_SET: return "SET";
    case MCI_STATUS: return "STATUS";
    default: return "?";
    }
}

// mciSendCommandW, or the seam, with one log line: command, device, flags, the detail and the
// result with its error text.
DWORD send(UINT device, UINT msg, DWORD flags, void* params, const std::string& detail) {
    DWORD r;
    if (g_seam && g_seam->send)
        r = g_seam->send(device, msg, flags, params);
    else
        r = mciSendCommandW(device, msg, flags, reinterpret_cast<DWORD_PTR>(params));
    char buf[160];
    std::snprintf(buf, sizeof buf, "mci %s (0x%03x) device %u flags 0x%lx", name(msg), msg, device,
                  static_cast<unsigned long>(flags));
    std::string line = buf + (detail.empty() ? std::string() : " " + detail);
    if (r == 0) {
        line += " -> 0";
    } else {
        wchar_t text[128] = L"";
        if (!g_seam) mciGetErrorStringW(r, text, 128);
        std::snprintf(buf, sizeof buf, " -> error %lu ", static_cast<unsigned long>(r));
        line += buf + utf8(text);
    }
    log(line);
    return r;
}

int ask(HWND hwnd, const std::string& text) {
    if (g_seam && g_seam->ask) return g_seam->ask(hwnd, text);
    // MessageBox(notify window, text, "", MB_YESNO) [BYTES ISPACE.DLL seg9:0x3880-0x3890]; the
    // owner is the main window, which stands for the hidden NotifyClass window [DESIGN].
    return MessageBoxW(hwnd, widen(text).c_str(), L"", MB_YESNO);
}

bool get_mapper_volume(DWORD* v) {
    if (g_seam && g_seam->get_volume) {
        std::uint32_t x = 0;
        const bool ok = g_seam->get_volume(&x);
        *v = x;
        return ok;
    }
    return midiOutGetVolume(reinterpret_cast<HMIDIOUT>(static_cast<UINT_PTR>(MIDI_MAPPER)), v) ==
           MMSYSERR_NOERROR;
}

void set_mapper_volume(DWORD v) {
    MMRESULT r = MMSYSERR_NOERROR;
    if (g_seam && g_seam->set_volume)
        g_seam->set_volume(v);
    else
        r = midiOutSetVolume(reinterpret_cast<HMIDIOUT>(static_cast<UINT_PTR>(MIDI_MAPPER)), v);
    char buf[96];
    std::snprintf(buf, sizeof buf, "midiOutSetVolume(MIDI_MAPPER, 0x%08lx) -> %u",
                  static_cast<unsigned long>(v), static_cast<unsigned>(r));
    log(buf);
}

class Mci final : public Backend {
public:
    explicit Mci(HWND hwnd) : hwnd_(hwnd) {}

    // The original never closes the device at exit: command 1 makes no MCI call
    // [BYTES ISPACE.DLL seg9:0x6a0]; the system closes it with the process. The mapper gets
    // its own volume back, as command 1 restores the saved system volumes
    // [BYTES ISPACE.DLL seg9:0x717] (ADR 0011 decision 3) [DESIGN].
    ~Mci() override {
        if (saved_) set_mapper_volume(system_);
    }

    // seg9:0x37ea [BYTES ISPACE.DLL seg9:0x380c-0x38bb].
    bool open(const std::filesystem::path& midi) override {
        const std::wstring element = midi.wstring();
        MCI_OPEN_PARMSW o{};
        o.lpstrDeviceType = L"sequencer";  // ISPACE.DLL DGROUP:0xae4
        o.lpstrElementName = element.c_str();
        if (send(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, &o,
                 "type \"sequencer\" element \"" + utf8(element) + "\"") != 0)
            return false;  // nothing opened; the device id keeps its value
        device_ = o.wDeviceID;  // DGROUP:0x22a6
        MCI_STATUS_PARMS s{};
        s.dwItem = MCI_SEQ_STATUS_PORT;
        if (send(device_, MCI_STATUS, MCI_STATUS_ITEM, &s, "item MCI_SEQ_STATUS_PORT") != 0) {
            close();  // seg9:0x3861
            return false;
        }
        char buf[64];
        std::snprintf(buf, sizeof buf, "port 0x%lx", static_cast<unsigned long>(s.dwReturn));
        log(buf);
        // The low word against -1 (MIDI_MAPPER) [BYTES ISPACE.DLL seg9:0x3866].
        if (LOWORD(s.dwReturn) != 0xffff) {
            const int answer = ask(hwnd_, g_question);
            log("Midi Mapper question answered " + std::string(answer == IDNO ? "No" : "Yes"));
            if (answer == IDNO) {  // seg9:0x3895
                close();
                return false;
            }
        }
        MCI_SET_PARMS t{};
        t.dwTimeFormat = MCI_FORMAT_MILLISECONDS;  // its result is ignored [BYTES seg9:0x38bb]
        send(device_, MCI_SET, MCI_SET_TIME_FORMAT, &t, "time format ms");
        return true;
    }

    // seg9:0x3a94: MCI_TO | MCI_WAIT (0xa) or MCI_TO | MCI_NOTIFY (9), the window as callback
    // [BYTES ISPACE.DLL seg9:0x3adf seg9:0x3b2f seg9:0x3b4d].
    bool seek_ms(unsigned ms, bool wait) override {
        MCI_SEEK_PARMS p{};
        p.dwCallback = reinterpret_cast<DWORD_PTR>(hwnd_);
        p.dwTo = ms;
        return send(device_, MCI_SEEK, MCI_TO | (wait ? MCI_WAIT : MCI_NOTIFY), &p,
                    "to " + std::to_string(ms)) == 0;
    }

    // seg9:0x3b5c: MCI_TO | MCI_NOTIFY, no MCI_FROM [BYTES ISPACE.DLL seg9:0x3bb0-0x3bc1].
    bool play_to_ms(unsigned ms) override {
        to_ = ms;
        MCI_PLAY_PARMS p{};
        p.dwCallback = reinterpret_cast<DWORD_PTR>(hwnd_);
        p.dwTo = ms;
        return send(device_, MCI_PLAY, MCI_TO | MCI_NOTIFY, &p, "to " + std::to_string(ms)) == 0;
    }

    // seg9:0x3a48: flags 0, with the pause parameters that carry the window
    // [BYTES ISPACE.DLL seg9:0x3a71-0x3a80]. Sent to 0 before the first open and to 0xFFFF
    // after a close, as the original does; on Win32 0xFFFF is no device (Win16's
    // MCI_ALL_DEVICE_ID is 0xFFFF, Win32's 0xFFFFFFFF) and the call fails [DESIGN].
    void pause() override {
        MCI_GENERIC_PARMS p{};
        p.dwCallback = reinterpret_cast<DWORD_PTR>(hwnd_);
        send(device_, MCI_PAUSE, 0, &p, "");
    }

    // The original never resumes with MCI_RESUME: -3 and focus play again from the current
    // position (seg9:0x3b5c). The Player does not call this; it plays to the last end [DESIGN].
    void resume() override { play_to_ms(to_); }

    // seg9:0x3a16: MCI_CLOSE with MCI_WAIT and no parameters; the id becomes 0xFFFF
    // [BYTES ISPACE.DLL seg9:0x3a20-0x3a34].
    void close() override {
        send(device_, MCI_CLOSE, MCI_WAIT, nullptr, "");
        device_ = 0xffff;
    }

    // ADR 0011 decision 3: the music volume through the MIDI mapper, where the original's
    // device-1 call fails [DESIGN]. kSystemVolume gives the mapper back the volume it had
    // before the port's first write.
    void set_volume(int percent) override {
        if (percent == kSystemVolume) {
            if (saved_) set_mapper_volume(system_);
            return;
        }
        if (!saved_) {
            DWORD v = 0;
            saved_ = get_mapper_volume(&v);
            system_ = v;
        }
        set_mapper_volume(volume_dword(percent));
    }

private:
    HWND hwnd_;
    UINT device_ = 0;  // DGROUP:0x22a6: 0 at start, 0xFFFF after a close
    unsigned to_ = 0;
    bool saved_ = false;
    DWORD system_ = 0;
};

}  // namespace

void set_mapper_question(std::string cp1252) { g_question = std::move(cp1252); }
void set_mci_seam(const MciSeam* seam) { g_seam = seam; }

std::unique_ptr<Backend> make_mci(void* hwnd_notify) {
    return std::make_unique<Mci>(static_cast<HWND>(hwnd_notify));
}

}  // namespace is::audio::music

#else  // the host: no MCI

namespace is::audio::music {

void set_mapper_question(std::string) {}
void set_mci_seam(const MciSeam*) {}
std::unique_ptr<Backend> make_mci(void*) { return nullptr; }

}  // namespace is::audio::music

#endif
