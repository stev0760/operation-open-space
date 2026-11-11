// The effects engine of ISPACE.DLL (docs/re/sound.md) as portable state: ordinal 7 commands 0-4
// and 10, the 82 slots, ordinal 9 (playing), ordinal 8 (position and volume), the volume routine
// and INSPACE's wrappers seg10:0x744c, seg10:0x754a and seg10:0x7b48. It drives a Mixer and reads
// waves from a WaveSource; it makes no Win32 call.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "audio/settings.h"
#include "audio/mixer.h"
#include "audio/wave_view.h"
#include "shell/rng.h"

namespace is::audio {

// The flag byte of each slot 0x00-0x51 [BYTES ISPACE.DLL seg9:0x2603 seg9:0x2660].
inline constexpr int kSlots = 82;
std::uint8_t slot_flag(int id);
// The hold added to busy-until for q != 0 [BYTES ISPACE.DLL seg9:0xf93], 0 for other ids.
int hold_ms(int id);
// Slot s is resource #100 <s + 100> of module (flag & 1) [BYTES ISPACE.DLL seg9:0x2771].
inline int slot_resource(int id) { return id + 100; }

// Where the waves come from (wav_source.cpp reads them through the asset store).
struct WaveSource {
    virtual ~WaveSource() = default;
    // Module k (0 SND_FX.DLL, 1 <Language>_FX.DLL) exists (ISPACE.DLL seg9:0xba's OpenFile).
    virtual bool has_module(int k) = 0;
    // Resource #100 <resource> of module k as 8-bit 11,025 Hz mono, or nullptr when absent.
    virtual PcmPtr slot(int k, int resource) = 0;
    // SndPath + name + ".WAV", converted, or nullptr (id -1).
    virtual PcmPtr file(const std::string& name) = 0;
};

struct Point {
    int x = 0, y = 0;
};

// seg8:0x285a(ship, p), its DGROUP:0x9130 = 0 path [BYTES seg8:0x2872-0x29e6], the distance
// seg10:0x7896 measures (cells, 1/8 world unit): both points `sar 3` in 16 bits, each axis's
// difference moved by the cell count (DGROUP:0x5a0 = DGROUP:0x5a2 = 3200, the periods INSPACE
// gives command 0) when its magnitude passes half of it, then one Newton step of the root from
// e = max + min / 2: ((dx² + dy²) / e + e) >> 1, and 1 when e = 0. world::distance_cells is the
// same routine for the world's callers; isaudio cannot link isworld [DESIGN].
int cell_distance(Point ship, Point p, int cells_x = 3200, int cells_y = 3200);

// What INSPACE's wrappers read from the game (DGROUP globals); outside a wave the defaults
// stand for the title screens [OPEN].
struct World {
    int area_w = 640, area_h = 480;  // DGROUP:0x90ea, DGROUP:0x90ec (audio::set_area)
    int seg18_area_w = 640, seg18_area_h = 480;  // DGROUP:0x88c0, DGROUP:0x88c2
    int seg18 = 0;           // DGROUP:0x9130
    int edition = 0;         // DGROUP:0x920c (3 plays nothing)
    int mix_channels = 1;    // DGROUP:0x920a (INSPACE's own MixChannels read, default 1)
    int pool_count = -1;     // DGROUP:0x9324
    int player = 0;          // DGROUP:0x9646
    // Mode 1-3 sources (pool record, table, dword DGROUP:0x5144): the point before seg8:0x29ea;
    // false: no point [OPEN].
    std::function<bool(int mode, int a, int b, Point& p)> source;
    // seg8:0x29ea; identity by default.
    std::function<Point(Point)> to_view;
    // The player's points, Size and the activity rectangles of the running wave; false (or
    // empty) outside a wave (audio::set_wave_view, M4 s8).
    std::function<bool(WaveView&)> view;
};

// INSPACE's music wrapper seg10:0x7480(code, flag) before its ordinal 7 command 11 [BYTES
// seg10:0x748d seg10:0x74aa seg10:0x74bc seg10:0x74c9 seg10:0x74d5 seg10:0x74e9 seg10:0x74f0
// seg10:0x74f9 seg10:0x7504 seg10:0x7512 seg10:0x7524 seg10:0x7538].
struct MusicWrapper {
    int current = 0;          // DGROUP:0x9220 (start value [OPEN])
    int last_control = 0;     // DGROUP:0x9222
    std::int8_t saved[2] = {0, 0};  // DGROUP:0x7e0[DGROUP:0x73e]
    int saved_index = 0;      // DGROUP:0x73e
    std::uint32_t hold = 0;   // DGROUP:0x2384, the adaptive music's hold
    // Leaves in `code` what goes to ISPACE.DLL (a -3 becomes the saved track when it differs
    // from the current one) and stores a track 7 as current 2; false when the call does
    // nothing.
    bool filter(int& code, std::uint32_t now);
};

class Engine {
public:
    // `clock` is GetCurrentTime (the player's booking time) [BYTES ISPACE.DLL seg9:0x13ef].
    Engine(Mixer& mixer, WaveSource& source, std::function<std::uint32_t()> clock);

    // ---- ordinal 7
    // Command 0 [BYTES ISPACE.DLL seg9:0x448]: `wave_devices` is waveOutGetNumDevs(); the
    // port reports capability 3 when a device exists [DESIGN] ADR 0011 decision 2.
    bool command0(const Settings& s, int wave_devices, int period_x = 3200, int period_y = 3200,
                  bool reduced = false);
    void command1();  // seg9:0x6a0
    void command2();  // seg9:0x73a
    void command3();  // seg9:0xaf2, focus lost
    void command4();  // seg9:0xba6, focus gained
    void command10(int saver_volume);  // seg9:0x6e: SaverVolume in place of SoundVolume
    // The INI reader seg9:0x22ca again; `audio_ok` adds Audio OK's module and channel changes
    // (seg9:0x331a steps 2-4).
    void reread(const Settings& s, bool audio_ok = false);

    // ---- ordinal 9 and ordinal 8
    int ordinal9(int id, Point p, int q, int r, int s, int w, int h, std::uint32_t now,
                 const std::string& name = {});
    int ordinal8(int w, int h, int b, int a, Point p);
    // seg9:0x3bf4's percentage for the music (0 when the sound system is at most 1: no call).
    int music_volume_percent() const;
    // seg9:0xa9e.
    void volumes();

    // MM_WOM_DONE for every channel the mixer finished [BYTES ISPACE.DLL seg9:0x2adf].
    void pump();
    void on_done(int channel);
    // seg9:0x1174.
    void stop_all();

    // ---- INSPACE wrappers
    int wrap_744c(int id, std::uint32_t now);                          // seg10:0x744c
    int wrap_754a(int id, int a, int b, int mode, std::uint32_t now);  // seg10:0x754a
    void wrap_7b48(std::uint32_t now);                                 // seg10:0x7b48
    World& world() { return world_; }

    // seg9:0x38e2's refresh of the music volume (ordinal 8 and every track request), the
    // focus pause (seg9:0x3a48(2)) and resume (seg9:0x3b5c): audio.cpp forwards them to the
    // music code [DESIGN].
    std::function<void(int percent)> on_music_volume;
    std::function<void(bool gained)> on_music_focus;

    // ---- state, for the tests and the dialogs
    struct Record {
        int id = 0;
        std::uint32_t start = 0;
        int q = 0;
        int voice = 0;
    };
    int sound_system() const { return sound_system_; }
    int capability() const { return capability_; }
    int channels() const { return channels_; }
    int effects_volume() const { return fx_volume_; }    // DGROUP:0x6a0
    int music_volume() const { return music_volume_; }   // DGROUP:0x2288
    std::uint32_t busy_until() const { return busy_until_; }
    int priority_id() const { return prio_id_; }
    int priority_channel() const { return prio_ch_; }
    int active_count() const { return active_; }
    int held_count() const { return held_; }
    const Record& record(int c) const { return rec_[static_cast<std::size_t>(c)]; }
    bool loaded(int id) const { return slots_[static_cast<std::size_t>(id)] != nullptr; }
    int load_slot(int id, int now);   // seg9:0x25c6
    void purge_slot(int id, bool force);  // seg9:0x28d2
    std::uint32_t pending_deadline() const { return seq_deadline_; }
    int pending_id() const { return seq_follow_; }
    int loudness() const { return loudness_; }  // DGROUP:0x21a6
    // The last seg10:0x754a call, for the log: seg10:0x7896's q (-2 when the call returned
    // before it), and whether ordinal 9 reached a successful play (its return is 1 only for
    // g != 0, so 0 does not mean refused) [DESIGN].
    int last_call_q() const { return call_q_; }
    bool last_played() const { return played_; }
    shell::Rng& rng() { return rng_; }
    const Settings& settings() const { return s_; }

private:
    int admission(int id, int q, int loop);
    int player(int id, PcmPtr file_wave, int q, int loop, int g);
    void read_ini(const Settings& s);
    bool load_module(int k);
    int q_for(int mode, Point p);
    bool source_point(int mode, int a, int b, Point& p, bool& stand_in);
    void view_point(Point& p, bool wrap_seg18_test);

    Mixer& mixer_;
    WaveSource& src_;
    std::function<std::uint32_t()> clock_;
    shell::Rng rng_;  // ISPACE.DLL's own rand, state DGROUP:0xb8c [BYTES ISPACE.DLL seg3:0x668]
    Settings s_{};
    World world_;

    int capability_ = 0;      // DGROUP:0x227e
    int sound_system_ = 0;    // DGROUP:0x2280
    int channels_ = 0;        // DGROUP:0x219e
    bool session_ = false;    // DGROUP:0x21aa != 0
    bool active_session_ = false;  // DGROUP:0x2222
    int refcount_ = 0;        // DGROUP:0x219c
    bool module_[2] = {false, false};  // DGROUP:0x68c
    int master_ = 100;        // DGROUP:0x69e
    int fx_volume_ = 100;     // DGROUP:0x6a0
    int music_volume_ = 100;  // DGROUP:0x2288
    int period_x_ = 3200, period_y_ = 3200;  // DGROUP:0x19c8, DGROUP:0x19ca
    int lst_a_ = 0, lst_b_ = 0, lst_w_ = 0, lst_h_ = 0;  // DGROUP:0x2282, 0x2284, 0x19c4, 0x19c6
    std::uint32_t vol_cache_ = 0xFFFFFFFFu;  // DGROUP:0xcee
    std::uint32_t busy_until_ = 0;  // DGROUP:0xcf2
    std::uint32_t after_1c_ = 0;    // DGROUP:0xcf6
    int last_66_ = 0;  // DGROUP:0xcfc
    int alt_67_ = 0;   // DGROUP:0xcfa
    int prio_id_ = 0, prio_ch_ = kMaxChannels;  // DGROUP:0x21a0, DGROUP:0x21a2
    int active_ = 0, held_ = 0;  // DGROUP:0x21a8, DGROUP:0x21a4
    int loudness_ = 0;           // DGROUP:0x21a6
    std::array<Record, kMaxChannels + 1> rec_{};  // DGROUP:0x1760, record 8 = no priority
    std::array<PcmPtr, kSlots> slots_{};          // DGROUP:0x187c
    std::array<bool, kMaxChannels> file_wave_{};  // DGROUP:0x173c

    // INSPACE side
    int last_id_ = 0, last_a_ = 0, last_b_ = 0, last_mode_ = 0;  // DGROUP:0x921c, 0x928c, 0x928e, 0x9290
    Point last_p_;     // DGROUP:0x3b98
    int last_q_ = 0;   // DGROUP:0x9226
    int call_q_ = -2;      // last_call_q()
    bool played_ = false;  // last_played()
    std::uint32_t seq_deadline_ = 0;  // DGROUP:0x3c88
    int seq_follow_ = 0;              // DGROUP:0x9292
    int seq_first_ = 0, seq_a_ = 0, seq_b_ = 0;  // DGROUP:0x9216, 0x9218, 0x921a
};

}  // namespace is::audio
