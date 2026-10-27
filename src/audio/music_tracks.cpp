// The music state machine of ISPACE.DLL (docs/re/sound.md "Music"); see music_tracks.h.
#include "audio/music_tracks.h"

#include <algorithm>

namespace is::audio::music {

// Defined here so that every backend, the fake ones of the tests included, links without a
// Win32 file [DESIGN].
Backend::~Backend() = default;

// [BYTES ISPACE.DLL DGROUP:0xaa8 DGROUP:0xac0 DGROUP:0xad8]
const std::array<TrackInfo, kTrackCount> kTracks = {{
    {2, 0, 1810},     // 0 anthem
    {0, 45, 1495},    // 1 main
    {0, 1700, 1170},  // 2 main
    {1, 0, 290},      // 3 race
    {1, 340, 2550},   // 4 race
    {2, 2000, 166},   // 5 anthem
    {3, 190, 2030},   // 6 demon
    {0, 2870, 50},    // 7 main
    {0, 2920, 90},    // 8 main
    {3, 0, 89},       // 9 demon
    {3, 89, 101},     // 10 demon
    {2, 1810, 190},   // 11 anthem
}};

namespace {

// DGROUP:0xaac, the seek start of a negative track (seg9:0x3aa8) [BYTES ISPACE.DLL
// DGROUP:0xaac].
constexpr int kNegativeStart = 1700;

bool valid_track(int t) { return t >= 0 && t < kTrackCount; }

}  // namespace

std::string_view file_stem(int file) {
    switch (file) {  // [BYTES ISPACE.DLL seg9:0x39a8-0x3a03]
    case 0: return "main";
    case 1: return "race";
    case 2: return "anthem";
    case 3: return "demon";
    default: return "";
    }
}

std::filesystem::path midi_path(const std::filesystem::path& snd_path, int file) {
    auto s = snd_path.native();
    using C = std::filesystem::path::value_type;
    if (s.empty() || (s.back() != C('\\') && s.back() != C('/'))) s.push_back(C('\\'));
    const std::string_view stem = file_stem(file);
    for (char c : stem) s.push_back(C(c));
    for (char c : std::string_view(".mid")) s.push_back(C(c));
    return std::filesystem::path(s);
}

unsigned seek_target_ms(int track, int previous) {
    long ms = 100L * (valid_track(track) ? kTracks[track].start : kNegativeStart);
    if (track == 1 && previous == 2)
        ms -= 4500;  // 0x1194 [BYTES ISPACE.DLL seg9:0x3af9]
    else if (track == 2 && previous == 1)
        ms -= 15000;  // 0x3a98 [BYTES ISPACE.DLL seg9:0x3b0f]
    return static_cast<unsigned>(ms);
}

unsigned end_ms(int track) {
    if (!valid_track(track)) return 0;
    return 100u * static_cast<unsigned>(kTracks[track].start + kTracks[track].length);
}

int music_level(int master, int fx_music) {
    if (fx_music >= 50) return master;  // [BYTES ISPACE.DLL seg9:0xacd seg9:0xad4]
    // 16-bit imul keeps the low word, cwd, idiv 100 [BYTES ISPACE.DLL seg9:0xad9-0xae6].
    const auto product = static_cast<std::int16_t>(
        static_cast<std::int16_t>(fx_music * 2) * static_cast<std::int16_t>(master));
    return static_cast<std::int16_t>(product / 100);
}

int music_volume(int level) {
    // (100 * 95 / 100) * Mm / 100 in unsigned 16-bit mul and div, then an unsigned clamp to
    // 100 [BYTES ISPACE.DLL seg9:0x3dbf-0x3ddb].
    const auto mm = static_cast<std::uint16_t>(level);
    const auto v = static_cast<std::uint16_t>(static_cast<std::uint16_t>(95u * mm) / 100u);
    return std::min<int>(v, 100);
}

std::uint32_t volume_dword(int v) {
    const std::uint32_t w = static_cast<std::uint32_t>(v) * 0xffffu / 100u;  // seg3:0x1d6
    return (w << 16) | (w & 0xffffu);
}

Config config_from(int music, int sound_system, int master, int fx_music,
                   const std::filesystem::path& snd_path) {
    Config c;
    c.music = music;
    // SoundSystem -1 resolves to the capability level, 3 in the port, and larger values clamp
    // to it (ADR 0011 decision 5) [DESIGN] [BYTES ISPACE.DLL seg9:0x2538 seg9:0x25b0].
    c.sound_system = sound_system < 0 ? 3 : std::min(sound_system, 3);
    c.level = music_level(master, fx_music);
    c.snd_path = snd_path;
    return c;
}

// ---- Player

// seg9:0x3906 [BYTES ISPACE.DLL seg9:0x3913-0x3a09].
void Player::request(int code, int flag) {
    if (code <= -1 && state_ == State::kHardPaused) return;  // seg9:0x3913
    switch (code) {
    case -5: return;                 // seg9:0x3929
    case -3: play(); return;         // seg9:0x3952
    case -2:                         // seg9:0x3968
        previous_ = -2;
        pause(2);
        return;
    case -1:  // seg9:0x3959
        previous_ = -1;
        pause(4);
        return;
    default: break;
    }
    track_ = code;  // seg9:0x393b
    if (config_.sound_system == 0 || config_.music == 0) return;  // seg9:0x393e seg9:0x3948
    // INSPACE never passes -4, a code below -5 or one above 11; the original would index its
    // tables with it. The port stores it and does nothing more [DESIGN].
    if (!valid_track(code)) return;
    refresh_volume();  // seg9:0x3973
    if (kTracks[code].file == file_ && state_ != State::kClosed) {  // seg9:0x397e seg9:0x3984
        seek(flag);  // seg9:0x3a09
        return;
    }
    close();  // seg9:0x398c
    file_ = kTracks[code].file;
    open_file();  // seg9:0x39dc
}

// seg9:0x2ac4, MM_MCINOTIFY [BYTES ISPACE.DLL seg9:0x2b18-0x2b60].
void Player::notify(unsigned wparam) {
    if (wparam != 1) return;  // MCI_NOTIFY_SUCCESSFUL only
    if (state_ == State::kPlaying) {
        if (track_ == 7 || track_ == 8) track_ = 2;  // seg9:0x2b44
        if (track_ == 5 || track_ == 11) return;     // seg9:0x2b4a seg9:0x2b54
        seek(0);                                     // seg9:0x2b60
    } else if (state_ == State::kSeeked) {
        play();  // seg9:0x2b2e
    }
}

// The music part of command 3 [BYTES ISPACE.DLL seg9:0xafc seg9:0xb1f seg9:0xb2c]. The port
// reads no device volume; when it has set the mapper's volume it hands the mapper its own
// back, as the original writes the saved system value [DESIGN] (ADR 0011 decision 3).
void Player::focus_lost() {
    if (config_.sound_system == 0) return;
    pause(2);
    if (sent_v_ >= 0 && backend_) {
        backend_->set_volume(kSystemVolume);
        sent_ = 0xffffffffu;  // the cache now holds the system value, which the port never reads
    }
}

// The music part of command 4 [BYTES ISPACE.DLL seg9:0xbb0 seg9:0xc01 seg9:0xc25 seg9:0xc36].
void Player::focus_gained() {
    if (config_.sound_system == 0) return;
    if (sent_v_ >= 0 && backend_) {
        backend_->set_volume(sent_v_);
        sent_ = volume_dword(sent_v_);
    }
    if (resume_) play();
}

// seg9:0x38e2, seg9:0x3bf4 and seg9:0x3e46 [BYTES ISPACE.DLL seg9:0x3c01 seg9:0x3e57
// seg9:0x3e67 seg9:0x3e72].
void Player::refresh_volume() {
    if (config_.sound_system <= 1) return;
    const int v = music_volume(config_.level);
    const std::uint32_t d = volume_dword(v);
    if (d == sent_) return;
    sent_ = d;  // updated before the call, whatever its result
    sent_v_ = v;
    if (backend_) backend_->set_volume(v);
}

void Player::restart_after_options(const Config& before, const Config& after) {
    config_ = after;
    if ((before.music == 0 && after.music != 0) ||
        (before.sound_system == 0 && after.sound_system != 0))
        request(track_, 1);  // seg9:0x220a(DGROUP:0xaa4, 1)
}

// seg9:0x3a16: MCI_CLOSE to the device, which then reads 0xFFFF, and state 0.
void Player::close() {
    if (backend_) backend_->close();
    state_ = State::kClosed;
}

// seg9:0x37ea: OPEN, STATUS port and the Midi Mapper question, SET, then a waiting seek. The
// backend closes itself on a STATUS error or IDNO and returns false; the state is already 0
// from the close before the open.
void Player::open_file() {
    if (!backend_) return;
    if (!backend_->open(midi_path(config_.snd_path, file_))) {
        state_ = State::kClosed;
        return;
    }
    seek(1);  // seg9:0x38d6
}

// seg9:0x3a48 [BYTES ISPACE.DLL seg9:0x3a52-0x3a88].
void Player::pause(int code) {
    resume_ = false;
    if (code != 4) {
        if (state_ != State::kPlaying) return;
        if (code == 2) resume_ = true;
    }
    if (backend_) backend_->pause();  // MCI_PAUSE, to device 0 or 0xFFFF when nothing is open
    state_ = static_cast<State>(code);
}

// seg9:0x3a94: the state is set before the seek, whatever its result [BYTES ISPACE.DLL
// seg9:0x3aa1-0x3b4d].
void Player::seek(int flag) {
    state_ = track_ < 0 ? State::kHardPaused : State::kSeeked;
    const unsigned ms = seek_target_ms(track_, previous_);
    if (!backend_) return;
    if (flag != 0) {
        if (backend_->seek_ms(ms, true)) play();
    } else {
        backend_->seek_ms(ms, false);
    }
}

// seg9:0x3b5c [BYTES ISPACE.DLL seg9:0x3b6a-0x3be5].
void Player::play() {
    if (state_ != State::kPaused && state_ != State::kSeeked) return;
    if (config_.music == 0 || config_.sound_system == 0) {
        close();
        return;
    }
    if (!valid_track(track_)) return;  // unreachable from INSPACE's codes [DESIGN]
    const unsigned to = end_ms(track_);
    previous_ = track_;  // seg9:0x3bad, before the play
    if (!backend_ || !backend_->play_to_ms(to)) {
        close();  // seg9:0x3bd1
        return;
    }
    state_ = State::kPlaying;
}

}  // namespace is::audio::music
