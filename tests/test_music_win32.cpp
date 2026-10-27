// The MCI sequencer backend (src/audio/music_mci.cpp) under Win32 (Wine in CTest): the
// mciSendCommand sequence of docs/re/sound.md "MCI sequence" through the test seam, the Midi
// Mapper question and its IDNO, the failure modes, and a real sequencer open of MAIN.MID when
// IS_ORIGINAL_DIR is set and the system has one.
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "audio/music.h"
#include "audio/music_tracks.h"
#include "doctest.h"

namespace m = is::audio::music;

namespace {

struct Call {
    std::uint32_t device, msg, flags, to;
    DWORD_PTR callback;
};

std::vector<Call> g_calls;
std::uint32_t g_port = 0xffffffffu;  // MIDI_MAPPER
std::uint32_t g_fail_msg = 0;        // the command that fails
int g_answer = IDYES;
int g_asked = 0;
std::string g_asked_text;
std::vector<std::uint32_t> g_volumes;

std::uint32_t fake_send(std::uint32_t device, std::uint32_t msg, std::uint32_t flags, void* p) {
    Call c{device, msg, flags, 0, 0};
    switch (msg) {
    case MCI_OPEN: static_cast<MCI_OPEN_PARMSW*>(p)->wDeviceID = 7; break;
    case MCI_STATUS: static_cast<MCI_STATUS_PARMS*>(p)->dwReturn = g_port; break;
    case MCI_SEEK:
        c.to = static_cast<MCI_SEEK_PARMS*>(p)->dwTo;
        c.callback = static_cast<MCI_SEEK_PARMS*>(p)->dwCallback;
        break;
    case MCI_PLAY:
        c.to = static_cast<MCI_PLAY_PARMS*>(p)->dwTo;
        c.callback = static_cast<MCI_PLAY_PARMS*>(p)->dwCallback;
        break;
    case MCI_SET: c.to = static_cast<MCI_SET_PARMS*>(p)->dwTimeFormat; break;
    default: break;
    }
    g_calls.push_back(c);
    return msg == g_fail_msg ? MCIERR_DEVICE_NOT_READY : 0;
}

int fake_ask(void*, const std::string& text) {
    ++g_asked;
    g_asked_text = text;
    return g_answer;
}
bool fake_get(std::uint32_t* v) {
    *v = 0x12341234u;
    return true;
}
void fake_set(std::uint32_t v) { g_volumes.push_back(v); }

const m::MciSeam kSeam{fake_send, fake_ask, fake_get, fake_set};

struct SeamScope {
    SeamScope() {
        g_calls.clear();
        g_volumes.clear();
        g_port = 0xffffffffu;
        g_fail_msg = 0;
        g_answer = IDYES;
        g_asked = 0;
        m::set_mci_seam(&kSeam);
        m::set_mapper_question("Midi Mapper is not available. Continue?");
    }
    ~SeamScope() { m::set_mci_seam(nullptr); }
};

HWND const kWnd = reinterpret_cast<HWND>(0x1234);

m::Player player_on_seam(int sound_system = 3) {
    m::Player p(m::make_mci(kWnd));
    m::Config c;
    c.sound_system = sound_system;
    c.snd_path = "C:\\IS\\";
    p.configure(c);
    return p;
}

}  // namespace

TEST_CASE("music mci: the sequence of a new track") {
    SeamScope s;
    m::Player p = player_on_seam();
    p.request(2, 1);
    // CLOSE to device 0 (nothing open yet), OPEN 0x2200, STATUS port 0x100, SET ms 0x400,
    // SEEK MCI_TO|MCI_WAIT, PLAY MCI_TO|MCI_NOTIFY with no MCI_FROM.
    REQUIRE(g_calls.size() == 6);
    CHECK(g_calls[0].msg == MCI_CLOSE);
    CHECK(g_calls[0].device == 0);
    CHECK(g_calls[0].flags == MCI_WAIT);
    CHECK(g_calls[1].msg == MCI_OPEN);
    CHECK(g_calls[1].flags == 0x2200);
    CHECK(g_calls[2].msg == MCI_STATUS);
    CHECK(g_calls[2].device == 7);
    CHECK(g_calls[2].flags == 0x100);
    CHECK(g_calls[3].msg == MCI_SET);
    CHECK(g_calls[3].flags == 0x400);
    CHECK(g_calls[3].to == MCI_FORMAT_MILLISECONDS);
    CHECK(g_calls[4].msg == MCI_SEEK);
    CHECK(g_calls[4].flags == 0xa);
    CHECK(g_calls[4].to == 170000);
    CHECK(g_calls[4].callback == reinterpret_cast<DWORD_PTR>(kWnd));
    CHECK(g_calls[5].msg == MCI_PLAY);
    CHECK(g_calls[5].flags == 9);
    CHECK(g_calls[5].to == 287000);
    CHECK(g_asked == 0);
    CHECK(g_volumes == std::vector<std::uint32_t>{0xF332F332u});

    // A notify seek, the pause and the close of the next file.
    g_calls.clear();
    p.request(1, 0);
    REQUIRE(g_calls.size() == 1);
    CHECK(g_calls[0].msg == MCI_SEEK);
    CHECK(g_calls[0].flags == 9);
    CHECK(g_calls[0].to == 0);
    p.notify(1);
    p.request(-2, 1);
    REQUIRE(g_calls.size() == 3);
    CHECK(g_calls[2].msg == MCI_PAUSE);
    CHECK(g_calls[2].flags == 0);
    CHECK(g_calls[2].device == 7);
    g_calls.clear();
    p.request(3, 1);
    CHECK(g_calls[0].msg == MCI_CLOSE);
    CHECK(g_calls[0].device == 7);
    // After a close the id is 0xFFFF: a hard pause goes there.
    g_fail_msg = MCI_OPEN;
    p.request(0, 1);  // anthem: close, then a failed open
    g_calls.clear();
    p.request(-1, 1);
    REQUIRE(g_calls.size() == 1);
    CHECK(g_calls[0].msg == MCI_PAUSE);
    CHECK(g_calls[0].device == 0xffff);
    CHECK(p.state() == m::State::kHardPaused);
}

TEST_CASE("music mci: the Midi Mapper question") {
    SeamScope s;
    g_port = 0;  // a port other than the mapper
    m::Player p = player_on_seam();
    g_answer = IDNO;
    p.request(0, 1);
    CHECK(g_asked == 1);
    CHECK(g_asked_text == "Midi Mapper is not available. Continue?");
    // IDNO closes and plays nothing.
    REQUIRE(g_calls.size() == 4);
    CHECK(g_calls[3].msg == MCI_CLOSE);
    CHECK(p.state() == m::State::kClosed);
    // Nothing caches the answer: the box comes back at the next open.
    g_answer = IDYES;
    g_calls.clear();
    p.request(0, 1);
    CHECK(g_asked == 2);
    CHECK(g_calls.back().msg == MCI_PLAY);
    CHECK(p.state() == m::State::kPlaying);
}

TEST_CASE("music mci: STATUS and PLAY errors close, SET errors are ignored") {
    SeamScope s;
    m::Player p = player_on_seam();
    g_fail_msg = MCI_STATUS;
    p.request(6, 1);
    CHECK(g_calls.back().msg == MCI_CLOSE);
    CHECK(p.state() == m::State::kClosed);
    g_fail_msg = MCI_SET;
    g_calls.clear();
    p.request(6, 1);
    CHECK(g_calls.back().msg == MCI_PLAY);
    CHECK(p.state() == m::State::kPlaying);
    g_fail_msg = MCI_PLAY;
    p.request(9, 1);
    CHECK(g_calls.back().msg == MCI_CLOSE);
    CHECK(p.state() == m::State::kClosed);
}

TEST_CASE("music mci: the mapper volume and its restore") {
    SeamScope s;
    {
        auto b = m::make_mci(kWnd);
        b->set_volume(95);
        b->set_volume(m::kSystemVolume);
        b->set_volume(47);
    }
    CHECK(g_volumes ==
          std::vector<std::uint32_t>{0xF332F332u, 0x12341234u, m::volume_dword(47), 0x12341234u});
}

TEST_CASE("music mci: a real sequencer opens MAIN.MID when the system has one") {
    const char* dir = std::getenv("IS_ORIGINAL_DIR");
    if (!dir || !*dir) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    const std::filesystem::path midi = std::filesystem::path(dir) / "MAIN.MID";
    if (!std::filesystem::exists(midi)) {
        MESSAGE("no MAIN.MID: skipped");
        return;
    }
    m::set_mci_seam(nullptr);
    m::set_mapper_question("");
    auto b = m::make_mci(nullptr);
    // Probe first: without a MIDI output, MCI opens no sequencer and the test skips.
    MCI_OPEN_PARMSW o{};
    o.lpstrDeviceType = L"sequencer";
    const std::wstring element = midi.wstring();
    o.lpstrElementName = element.c_str();
    if (mciSendCommandW(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT,
                        reinterpret_cast<DWORD_PTR>(&o)) != 0) {
        MESSAGE("no MCI sequencer: skipped");
        return;
    }
    MCI_STATUS_PARMS st{};
    st.dwItem = MCI_SEQ_STATUS_PORT;
    const bool mapper = mciSendCommandW(o.wDeviceID, MCI_STATUS, MCI_STATUS_ITEM,
                                        reinterpret_cast<DWORD_PTR>(&st)) == 0 &&
                        LOWORD(st.dwReturn) == 0xffff;
    mciSendCommandW(o.wDeviceID, MCI_CLOSE, MCI_WAIT, 0);
    if (!mapper) {
        MESSAGE("the sequencer's port is not the mapper (the question box would show): skipped");
        return;
    }
    CHECK(b->open(midi));
    CHECK(b->seek_ms(4500, true));
    b->close();
}

#endif
