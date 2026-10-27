// The effects engine (engine.h). Every routine cites its original in docs/re/sound.md.
#include "audio/engine.h"

#include <algorithm>
#include <cstdlib>

namespace is::audio {

namespace {

// The flag bytes the loader's dispatch stores [BYTES ISPACE.DLL seg9:0x2603 seg9:0x260e
// seg9:0x2619 seg9:0x2624 seg9:0x262f seg9:0x263a seg9:0x2645 seg9:0x2660].
constexpr std::uint8_t kFlags[kSlots] = {
    0, 2, 2, 0, 2, 6, 0, 2, 0, 6, 6, 6, 6, 6, 6, 6,  // 0x00-0x0f
    6, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // 0x10-0x1f
    2, 2, 2, 0, 0, 0, 1, 0, 4, 2, 3, 2, 3, 3, 3, 3,  // 0x20-0x2f
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // 0x30-0x3f
    3, 2, 6, 7, 3, 0, 2, 3, 3, 3, 6, 7, 7, 7, 6, 7,  // 0x40-0x4f
    7, 7,                                            // 0x50-0x51
};

constexpr int kNoPriority = kMaxChannels;  // channel 8 means none [BYTES ISPACE.DLL seg9:0xd9c]
constexpr int kPreload = -11111;           // 0xd499 [BYTES ISPACE.DLL seg9:0xe63]
constexpr int kCentre = -6000;             // [BYTES ISPACE.DLL seg9:0x7a9]

// seg2:0x850: one Newton step of sqrt(a^2 + b^2) from max + min / 2, 1 when that is 0
// [BYTES ISPACE.DLL seg2:0x850].
unsigned loudness_of(unsigned a, unsigned b) {
    const unsigned hi = std::max(a, b), lo = std::min(a, b);
    unsigned x = hi + lo / 2;
    if (x == 0) x = 1;
    const unsigned n = a * a + b * b;
    return (x + n / x) / 2;
}

// StereoEffect's distance scaling [BYTES ISPACE.DLL seg9:0x823 seg9:0x833 seg9:0x83f].
int scale(int d, int stereo_effect) {
    switch (stereo_effect) {
    case 0: return d / 4;
    case 1: return d / 2;
    case 3: return d + d / 2;
    default: return d;
    }
}

}  // namespace

std::uint8_t slot_flag(int id) {
    return id >= 0 && id < kSlots ? kFlags[id] : 0;
}

// The 36-entry switch on ids 0x26-0x49 [BYTES ISPACE.DLL seg9:0xf88 seg9:0xf93 seg9:0xf98].
int hold_ms(int id) {
    switch (id) {
    case 0x26: return 900;  case 0x2c: return 730;  case 0x2d: return 900;
    case 0x2e: return 1000; case 0x2f: return 900;  case 0x30: return 600;
    case 0x31: return 1300; case 0x32: return 1100; case 0x33: return 1250;
    case 0x34: return 510;  case 0x35: return 300;  case 0x36: return 700;
    case 0x37: return 540;  case 0x38: return 300;  case 0x3d: return 350;
    case 0x3e: return 640;  case 0x3f: return 750;  case 0x40: return 520;
    case 0x47: return 660;  case 0x48: return 960;  case 0x49: return 700;
    default: return 0;
    }
}

Engine::Engine(Mixer& mixer, WaveSource& source, std::function<std::uint32_t()> clock)
    : mixer_(mixer), src_(source), clock_(std::move(clock)) {}

// ---------------------------------------------------------------- settings and session

// The volume routine [BYTES ISPACE.DLL seg9:0xa9e seg9:0xaab seg9:0xab7 seg9:0xaca seg9:0xacd
// seg9:0xad9 seg9:0xae8]: 16-bit imul and idiv.
void Engine::volumes() {
    const int m = master_, f = s_.fx_music;
    fx_volume_ = f <= 50 ? m : static_cast<std::int16_t>(m * 2 * (100 - f)) / 100;
    music_volume_ = f >= 50 ? m : static_cast<std::int16_t>(f * 2 * m) / 100;
}

// The INI reader [BYTES ISPACE.DLL seg9:0x22ca seg9:0x2416 seg9:0x2538 seg9:0x25bc]. The port
// has no PC-speaker driver and reports capability 3 when a device exists [DESIGN] ADR 0011
// decisions 2 and 5. A SoundSystem of -1 resolves in memory; the write-back is the caller's.
void Engine::read_ini(const Settings& s) {
    s_ = s;
    master_ = s.sound_volume;
    volumes();
    int ss = s.sound_system;
    if (ss == -1) ss = capability_;
    if (ss > capability_) ss = capability_;
    sound_system_ = ss;
}

// ISPACE.DLL seg9:0xba: a missing module turns its key off [BYTES ISPACE.DLL seg9:0x1fe
// seg9:0x219]; the port changes the key in memory only [DESIGN].
bool Engine::load_module(int k) {
    module_[k] = src_.has_module(k);
    if (!module_[k]) (k == 0 ? s_.sound_fx : s_.voice_fx) = 0;
    return module_[k];
}

// Ordinal 7 command 0 [BYTES ISPACE.DLL seg9:0x448 seg9:0x47e seg9:0x497 seg9:0x4b0 seg9:0x4c2]
// and the session set-up seg9:0x4e0 [BYTES ISPACE.DLL seg9:0x4ee seg9:0x558 seg9:0x571
// seg9:0x5fd seg9:0x610].
bool Engine::command0(const Settings& s, int wave_devices, int period_x, int period_y,
                      bool reduced) {
    if (period_x != 0) {
        period_x_ = period_x;
        period_y_ = period_y;
    }
    capability_ = wave_devices > 0 ? 3 : 0;
    read_ini(s);
    load_module(0);
    load_module(1);
    if (sound_system_ != 0) {
        if (refcount_ > 0) return true;
        ++refcount_;
        // The port's mixer stands in for WAVEMIX.DLL, so MixChannels always counts [DESIGN]
        // ADR 0011 decision 1; above 8 overruns the records [OPEN], so it is clamped.
        channels_ = reduced ? 1 : std::clamp(s.mix_channels, 1, kMaxChannels);
        mixer_.configure(channels_);
        stop_all();
        session_ = channels_ > 1;
        active_session_ = session_;
    }
    if (reduced) {
        load_slot(0x23, 0);
    } else {
        for (int id = 0; id < kSlots; ++id) load_slot(id, 0);
    }
    return true;
}

// Command 1 [BYTES ISPACE.DLL seg9:0x6a0 seg9:0x6c9 seg9:0x6f0]: the port is the only user
// of its modules, so the purge is forced; it also stops the output [DESIGN].
void Engine::command1() {
    stop_all();
    for (int id = 0; id < kSlots; ++id) purge_slot(id, true);
    module_[0] = module_[1] = false;
    refcount_ = 0;
    session_ = active_session_ = false;
}

// Command 2 [BYTES ISPACE.DLL seg9:0x73a seg9:0x748 seg9:0x756].
void Engine::command2() {
    stop_all();
    for (int id = 0; id < kSlots; ++id) purge_slot(id, false);
}

// Command 3 [BYTES ISPACE.DLL seg9:0xaf2 seg9:0xafc seg9:0xb3d seg9:0xb50]. The saved system
// volumes have nothing to act on in the port [DESIGN] ADR 0011 decision 2.
void Engine::command3() {
    if (sound_system_ == 0) return;
    stop_all();
    if (on_music_focus) on_music_focus(false);
    if (session_) active_session_ = false;
}

// Command 4 [BYTES ISPACE.DLL seg9:0xba6 seg9:0xbb0 seg9:0xc12 seg9:0xc25 seg9:0xc36]. ForceVol
// is read and has no effect [DESIGN] ADR 0011 decision 2.
void Engine::command4() {
    if (sound_system_ == 0) return;
    if (session_) active_session_ = true;
    if (on_music_focus) on_music_focus(true);
}

// Command 10 [BYTES ISPACE.DLL seg9:0x6e seg9:0x73 seg9:0x77].
void Engine::command10(int saver_volume) {
    master_ = saver_volume;
    volumes();
}

// The INI reader again [BYTES ISPACE.DLL seg9:0x2207], and with `audio_ok` Audio OK's module
// and channel steps [BYTES ISPACE.DLL seg9:0x34e4 seg9:0x350a seg9:0x3524 seg9:0x353e
// seg9:0x3547 seg9:0x3582 seg9:0x3599]; the bars and Options OK change no module or session
// (M7 s3 fixer).
void Engine::reread(const Settings& s, bool audio_ok) {
    read_ini(s);
    if (!audio_ok) return;
    bool loaded_any = false;
    for (int k = 0; k < 2; ++k) {
        const int on = k == 0 ? s.sound_fx : s.voice_fx;
        if (!on && module_[k]) {
            // seg9:0x3d8(k) releases the module; its slots go with it [OPEN] what else it frees.
            for (int id = 0; id < kSlots; ++id)
                if ((slot_flag(id) & 1) == k) purge_slot(id, true);
            module_[k] = false;
        } else if (on && !module_[k]) {
            load_module(k);
            loaded_any = true;
        }
    }
    // The new count (combo index + 1) against the session's count DGROUP:0x219e, not the INI's
    // [BYTES ISPACE.DLL seg9:0x3547 seg9:0x354a]: after a start at sound system 0 the count is 0,
    // so an Audio OK with no selection (0) reopens nothing [ORACLE sound-0002] (M7 s3 fixer).
    // seg9:0x4e0(1, 0) rebuilds the session; it sets the reference count to 1 only when it is
    // 0 and never increments it [BYTES ISPACE.DLL seg9:0x592 seg9:0x599 seg9:0x5a2 seg9:0x5ab].
    if (s.mix_channels != channels_ && sound_system_ != 0) {
        if (refcount_ == 0) refcount_ = 1;
        stop_all();
        for (int id = 0; id < kSlots; ++id) purge_slot(id, true);
        load_module(0);
        load_module(1);
        channels_ = std::clamp(s.mix_channels, 1, kMaxChannels);
        mixer_.configure(channels_);
        session_ = channels_ > 1;
        active_session_ = session_;
        loaded_any = true;
    }
    if (loaded_any)
        for (int id = 0; id < kSlots; ++id) load_slot(id, 0);
}

// ---------------------------------------------------------------- slots

// The loader seg9:0x25c6 [BYTES ISPACE.DLL seg9:0x25d4 seg9:0x25f1 seg9:0x2726 seg9:0x2729
// seg9:0x2739 seg9:0x2750 seg9:0x2771 seg9:0x2823 seg9:0x2849].
int Engine::load_slot(int id, int now) {
    if (sound_system_ == 0) return -1;
    if (id < 0 || id >= kSlots) return 0;  // past the table's bound 0x51 [OPEN]
    const std::uint8_t flag = slot_flag(id);
    const int k = flag & 1;
    if (slots_[static_cast<std::size_t>(id)] || !module_[k]) return -3;
    if ((now == 0 && (flag & 2)) || (k && !s_.voice_fx) || (!k && !s_.sound_fx)) return -4;
    // With one channel the resource is kept for sndPlaySound; otherwise WaveMixOpenWave needs
    // an open, active session (DGROUP:0x219e of 0 also takes this branch). So after Options
    // switches the sound system from 0 to non-zero no session exists and every effect stays
    // silent until a restart or an Audio OK with a channel count [ORACLE sound-0002] (M7 s3
    // fixer).
    if (channels_ != 1 && !(session_ && active_session_)) return 0;
    PcmPtr wave = src_.slot(k, slot_resource(id));
    if (!wave) return 0;
    slots_[static_cast<std::size_t>(id)] = std::move(wave);
    return 1;
}

// The purge seg9:0x28d2 [BYTES ISPACE.DLL seg9:0x28dd seg9:0x28e6 seg9:0x28f6 seg9:0x2900].
void Engine::purge_slot(int id, bool force) {
    if (id < 0 || id >= kSlots) return;
    auto& w = slots_[static_cast<std::size_t>(id)];
    if (!w) return;
    const std::uint8_t flag = slot_flag(id);
    if (force || ((flag & 2) && (s_.purge_sound != 0 || (flag & 4)))) w.reset();
}

// ---------------------------------------------------------------- playing

// The stop routine seg9:0x1174 [BYTES ISPACE.DLL seg9:0x1181 seg9:0x11a5 seg9:0x11ab
// seg9:0x11b6 seg9:0x11e8].
void Engine::stop_all() {
    prio_id_ = 0;
    prio_ch_ = kNoPriority;
    mixer_.stop_all();
    active_ = held_ = 0;
    for (int c = 0; c < std::max(channels_, 0) && c < kMaxChannels; ++c) {
        rec_[static_cast<std::size_t>(c)] = Record{};
        file_wave_[static_cast<std::size_t>(c)] = false;
    }
}

// The admission seg9:0x10ae [BYTES ISPACE.DLL seg9:0x10bb seg9:0x10d3 seg9:0x10ec seg9:0x110c
// seg9:0x1114 seg9:0x1124 seg9:0x1131 seg9:0x1138 seg9:0x1145 seg9:0x114e seg9:0x1155
// seg9:0x1163].
int Engine::admission(int id, int q, int loop) {
    const Record& r = rec_[static_cast<std::size_t>(prio_ch_)];
    const int cur = prio_id_;
    if (q == 0 && r.q != 0) return -1;
    if (q != 0 && r.q == 0 && held_ == 0) {
        if (id == 1 && active_ != 0) return -1;
        stop_all();
        return 1;
    }
    if (cur == 1) {
        stop_all();
        return 1;
    }
    if (id == 1) return 0;
    if (slot_flag(id) & 1) return 1;
    if (loop != 0 && channels_ == 1) return 1;
    if (id >= cur) return 1;
    if (slot_flag(cur) & 1) return 0;
    if (id == 0x1a) return cur < 0x20 ? 1 : 0;
    if (id == 0x1c) return 1;
    return 0;
}

// The player seg9:0x11f4 [BYTES ISPACE.DLL seg9:0x1207 seg9:0x1238 seg9:0x124d seg9:0x1282
// seg9:0x1298 seg9:0x12b8 seg9:0x12c9 seg9:0x12ce seg9:0x1304 seg9:0x1383 seg9:0x13be
// seg9:0x13ef seg9:0x1404 seg9:0x140f seg9:0x143d].
int Engine::player(int id, PcmPtr file_wave, int q, int loop, int g) {
    const int n = channels_;
    if (n <= 0) return 0;
    const std::uint32_t now = clock_();
    int c = -1;
    if (id != 0x2b && id != 0x27 && id != 8 && id != 0x14) {
        for (int i = 0; i < n && c < 0; ++i) {
            const Record& r = rec_[static_cast<std::size_t>(i)];
            if (r.id == id && now - r.start < 80) c = i;
        }
    }
    for (int i = 0; i < n && c < 0; ++i)
        if (rec_[static_cast<std::size_t>(i)].id == 0) c = i;
    if (c < 0) {
        std::uint32_t best = now + 500;
        for (int i = 0; i < n; ++i) {
            const Record& r = rec_[static_cast<std::size_t>(i)];
            if (i == prio_ch_ || (r.voice && r.q)) continue;
            if (r.start < best) {
                best = r.start;
                c = i;
            }
        }
    }
    if (c < 0) c = 0;

    const PcmPtr wave = file_wave ? file_wave : slots_[static_cast<std::size_t>(id)];
    bool ok;
    if (n == 1) {
        // sndPlaySound: one voice, SND_LOOP for a loop word, no completion message.
        ok = mixer_.play(0, wave, loop != 0 ? Mixer::kForever : 1, false);
    } else {
        if (!(session_ && active_session_)) return 0;
        // wLoops L: the wave plays L + 1 times [OPEN] [REF Microsoft WaveMix 1.x wavemix.h:
        // 0 plays once, 0xFFFF loops forever].
        ok = mixer_.play(c, wave, loop + 1, true);
    }
    if (!ok) return 0;
    Record& r = rec_[static_cast<std::size_t>(c)];
    if (r.id == 0) ++active_;
    if (q != 0 && r.q == 0) ++held_;
    r = Record{id, now, q, slot_flag(id) & 1};
    file_wave_[static_cast<std::size_t>(c)] = file_wave != nullptr;
    if (g != 0) {
        prio_id_ = id;
        prio_ch_ = c;
    }
    return 1;
}

// MM_WOM_DONE [BYTES ISPACE.DLL seg9:0x2adf seg9:0x2b68 seg9:0x2b8c seg9:0x2b9a seg9:0x2ba8
// seg9:0x2bcc seg9:0x2be6].
void Engine::on_done(int ch) {
    if (ch < 0 || ch >= kMaxChannels) return;
    if (ch == prio_ch_) {
        prio_ch_ = kNoPriority;
        prio_id_ = 0;
    }
    file_wave_[static_cast<std::size_t>(ch)] = false;
    rec_[static_cast<std::size_t>(ch)] = Record{};
    held_ = 0;
    for (int c = 0; c < channels_ && c < kMaxChannels; ++c)
        if (rec_[static_cast<std::size_t>(c)].q != 0) ++held_;
    --active_;
}

void Engine::pump() {
    for (int ch : mixer_.take_completed()) on_done(ch);
}

// Ordinal 9 [BYTES ISPACE.DLL seg9:0xc42 seg9:0xc50 seg9:0xc5c seg9:0xc72 seg9:0xcdb
// seg9:0xe35 seg9:0xe63 seg9:0xe75 seg9:0xe9d seg9:0xea6 seg9:0x101b seg9:0x1037 seg9:0x1099].
int Engine::ordinal9(int id, Point p, int q, int r, int s, int w, int h, std::uint32_t now,
                     const std::string& name) {
    if (sound_system_ == 0) return 0;
    if (id == -1 && s_.voice_fx == 0) return 0;
    pump();

    // The remaps [BYTES ISPACE.DLL seg9:0xcca seg9:0xd0e seg9:0xd5a seg9:0xd7d seg9:0xd8f
    // seg9:0xda4 seg9:0xdb6 seg9:0xdbe seg9:0xdcc seg9:0xdda seg9:0xde7 seg9:0xdf5].
    int slot = id, loop = 0, pick66 = -1;
    const bool file = id == -1;
    switch (id) {
    case -1: slot = 0x11; break;
    case 0x0c: slot = 0x0b; break;
    case 0x26: slot = s_.voice_fx == 0 ? 0x23 : 0x26; break;
    case 0x64: return 0;
    case 0x65: slot = 0x30 + rng_.rand() % 3; break;
    case 0x66:
        pick66 = rng_.rand() % 3;
        if (pick66 == last_66_) pick66 = (pick66 + 1) % 3;
        slot = 0x47 + pick66;
        break;
    case 0x67: slot = alt_67_ == 0 ? 0x49 : 0x2e; break;
    case 0x68: slot = 0x14; loop = 0x14; break;
    case 0x6f: slot = 0x17; loop = 0x14; break;
    case 0x70: slot = 0x4a; loop = 8; break;
    case 0x75: slot = 0x46; loop = 7; break;
    case 0x6e:
        stop_all();
        busy_until_ = 0;
        return 1;
    case 0x69:
        if (channels_ > 1) {
            prio_id_ = 0;
            prio_ch_ = kNoPriority;
        } else {
            stop_all();
        }
        busy_until_ = 0;
        return 1;
    default:
        if (id < 0 || id >= kSlots) return 0;  // no slot [OPEN]
        break;
    }

    if (loop == 0 && (fx_volume_ == 0 || now < busy_until_)) return 0;

    PcmPtr file_wave;
    if (!file) {
        // The loader runs only for an empty slot, its result is ignored, and a slot still
        // empty returns 0 before the admission [BYTES ISPACE.DLL seg9:0xe37 seg9:0xe45
        // seg9:0xe4d seg9:0xe53 seg9:0xe61 seg9:0xe63] (M7 fixer).
        if (!slots_[static_cast<std::size_t>(slot)]) load_slot(slot, 1);
        if (!slots_[static_cast<std::size_t>(slot)] || p.x == kPreload) return 0;
    } else {
        file_wave = src_.file(name);
    }

    const int g = admission(slot, q, loop);
    if (g == -1) return 0;
    if (g != 0) ordinal8(w, h, s, r, p);
    busy_until_ = now + static_cast<std::uint32_t>(q != 0 ? hold_ms(slot) : 0);
    // A custom WAV that cannot be read fails in the player: sndPlaySound with SND_NODEFAULT, or
    // WaveMixOpenWave, opens SndPath + name + .WAV and never plays slot 0x11, which only books
    // it [BYTES ISPACE.DLL seg9:0x1333 seg9:0x1367] (M7 s3 fixer).
    if (file && !file_wave) return 0;
    if (player(slot, file_wave, q, loop, g) == 0) return 0;

    if (id == 0x66) last_66_ = pick66;
    if (id == 0x67) alt_67_ = alt_67_ == 0;
    if (g != 0) ordinal8(w, h, s, r, p);
    if (slot == 0x1c && channels_ < 2) after_1c_ = now + 1200;
    if (q == 0) busy_until_ = now;
    return g != 0 ? 1 : 0;
}

// Ordinal 8 [BYTES ISPACE.DLL seg9:0x76c seg9:0x779 seg9:0x785 seg9:0x7a4 seg9:0x7a9
// seg9:0x7c1 seg9:0x7e6 seg9:0x7f0 seg9:0x86c seg9:0x879 seg9:0x8a2 seg9:0x8c0 seg9:0x8ca
// seg9:0x948 seg9:0x972 seg9:0x98d seg9:0x9a7 seg9:0x9ac seg9:0x9c9 seg9:0x9ec seg9:0xa0e
// seg9:0xa5b seg9:0xa7a seg9:0xa84 seg9:0xb62].
int Engine::ordinal8(int w, int h, int b, int a, Point p) {
    if (sound_system_ <= 1) return 0;
    if (a != 0) {
        lst_a_ = a;
        lst_b_ = b;
        lst_w_ = w;
        lst_h_ = h;
    }
    if (on_music_volume) on_music_volume(music_volume_percent());

    int left, right, v;
    if (p.x == kCentre) {
        left = right = 80;
        v = 100;
    } else {
        auto axis = [&](int pos, int extent, int off, int period) {
            int dd = pos - extent / 2 - off;
            if (std::abs(dd) > period / 2) {
                pos += dd > 0 ? -period : period;
                dd = pos - extent / 2 - off;
            }
            return dd;
        };
        const int dx = axis(p.x, lst_w_, lst_a_, period_x_);
        const int d = scale(std::abs(dx), s_.stereo_effect);
        int A, B;
        if (d < 300) {
            A = d / 60 + 95;
            B = 10 * d / (-35) + 95;
        } else if (d < 600) {
            A = (d - 300) / (-6) + 100;
            B = (d - 300) / (-30) + 10;
        } else if (d < 900) {
            A = 10 * (d - 600) / (-85) + 50;
            B = 0;
        } else {
            A = 15;
            B = 0;
        }
        const int dy = axis(p.y, lst_h_, lst_b_, period_y_);
        const int e = scale(std::abs(dy), s_.stereo_effect);
        if (e < 300) v = e / (-12) + 100;
        else if (e < 600) v = 10 * (e - 300) / (-55) + 75;
        else if (e < 900) v = (e - 600) / (-60) + 20;
        else v = 15;
        if (dx < 0) {
            left = A;
            right = B;
        } else {
            left = B;
            right = A;
        }
    }
    const unsigned rr = static_cast<unsigned>(v * right) / 100;
    const unsigned ll = static_cast<unsigned>(v * left) / 100;
    loudness_ = static_cast<int>(loudness_of(ll, rr));
    if (loudness_ < 65) rec_[static_cast<std::size_t>(prio_ch_)].q = 0;
    auto word = [&](unsigned x) {
        const unsigned y = std::min(x * static_cast<unsigned>(fx_volume_) / 100, 100u);
        return static_cast<std::uint16_t>(y * 65535u / 100u);
    };
    std::uint16_t lw = word(ll), rw = word(rr);
    if (s_.stereo_swap != 0) std::swap(lw, rw);
    const std::uint32_t dword = static_cast<std::uint32_t>(rw) << 16 | lw;
    if (dword != vol_cache_) {
        vol_cache_ = dword;
        mixer_.set_volume(lw, rw);
    }
    return 0;
}

// seg9:0x3bf4 with the -6000 point [BYTES ISPACE.DLL seg9:0x38ef seg9:0x3c01 seg9:0x3c0d
// seg9:0x3dbb seg9:0x3dd7].
int Engine::music_volume_percent() const {
    if (sound_system_ <= 1) return 100;  // no call: the music keeps its level [DESIGN]
    return std::min(100, (100 * 95 / 100) * music_volume_ / 100);
}

// ---------------------------------------------------------------- INSPACE wrappers

bool MusicWrapper::filter(int& code, std::uint32_t now) {
    if ((code == 9 || code == 10) && (current == 9 || current == 10)) return false;
    if (code == 10 && current == 1) code = 9;
    if (code == -2) {
        saved[saved_index & 1] = static_cast<std::int8_t>(current);
        last_control = -2;
    } else {
        last_control = -3;
        if (code == -3 && saved[saved_index & 1] != current) code = saved[saved_index & 1];
    }
    if (code >= 0) {
        current = code == 7 ? 2 : code;
        if (hold < now) hold = now + 9000;
    }
    return true;
}

// seg10:0x744c [BYTES seg10:0x745f seg10:0x7465 seg10:0x7475].
int Engine::wrap_744c(int id, std::uint32_t now) {
    const int r = wrap_754a(0x69, 0, 0, 0, now);
    if (id == 0x69) return r;
    return wrap_754a(id, 0, 0, 0, now);
}

// The point of a source for modes 1-3 (seg10:0x76d9-0x775c) and the zoom wrap (seg10:0x77c1-
// 0x781d) [BYTES seg10:0x776c seg10:0x778b seg10:0x77c1 seg10:0x7801].
bool Engine::point_for(int mode, int a, int b, Point& p, bool wrap_seg18_test) {
    switch (mode) {
    case 0: p = {kCentre, kCentre}; return true;
    case 4: p = {a, b}; return true;
    case 5: p = {kPreload, b}; return true;
    default: break;
    }
    if (mode == 1 && a > world_.pool_count && world_.source) return false;
    if (!world_.source || !world_.source(mode, a, b, p)) {
        // No wave state yet: the sound plays as from the centre [OPEN].
        p = {kCentre, kCentre};
        return true;
    }
    if (world_.to_view) p = world_.to_view(p);
    if (world_.zoom_shift != 1 && (!wrap_seg18_test || world_.seg18 == 0)) {
        p.x = world_.ship.x + ((2 * (p.x - world_.ship.x)) >> world_.zoom_shift);
        p.y = world_.ship.y + ((2 * (p.y - world_.ship.y)) >> world_.zoom_shift);
    }
    return true;
}

// seg10:0x7896 [BYTES seg10:0x78a9 seg10:0x78b5 seg10:0x78bc seg10:0x78e5 seg10:0x78ed
// seg10:0x78f7 seg10:0x78fa seg10:0x7905 seg10:0x792b].
int Engine::q_for(int mode, Point p) {
    // p.x = -6000 here is the port's stand-in point of a source without wave state [OPEN].
    if (mode == 0 || mode == 4 || world_.seg18 != 0 || p.x == kCentre) return 1;
    const int d = world_.distance ? world_.distance(world_.ship, p)
                                  : std::max(std::abs(p.x - world_.ship.x),
                                             std::abs(p.y - world_.ship.y));
    if (d > 800) return -1;
    if (d > 340) return 0;
    return 1;
}

// seg10:0x754a [BYTES seg10:0x7558 seg10:0x7563 seg10:0x756c seg10:0x7574 seg10:0x7582
// seg10:0x7597 seg10:0x765d seg10:0x7677 seg10:0x7795 seg10:0x7820 seg10:0x785e seg10:0x786f].
int Engine::wrap_754a(int id, int a, int b, int mode, std::uint32_t now) {
    if (id == 0x6e) mode = 0;
    if (world_.edition == 3) return 0;
    if (mode != 5 && id >= 0x6a) {
        struct Seq { int id, first, follow, delay; };
        static constexpr Seq kSeq[] = {
            {0x6a, 0x20, 0x21, 1000}, {0x6b, 0x39, 0x3a, 420}, {0x6c, 0x38, 0x39, 385},
            {0x6d, 0x35, 0x39, 385},  {0x71, 0x24, 0x25, 660}, {0x72, 0x1a, 0x1a, 200},
            {0x73, 0x42, 0x43, 700},  {0x74, 0x42, 0x66, 400}, {0x76, 0x23, 0x25, 400},
        };
        bool seq = false;
        for (const Seq& sq : kSeq) {
            if (sq.id != id) continue;
            seq_follow_ = sq.follow;
            seq_deadline_ = now + static_cast<std::uint32_t>(sq.delay);
            seq_first_ = sq.first;
            seq_a_ = a;
            seq_b_ = b;
            id = sq.first;
            seq = true;
            break;
        }
        if (!seq) seq_deadline_ = 0;  // 0x6e, 0x6f, 0x70, 0x75 and ids above 0x76
    }
    Point p;
    if (!point_for(mode, a, b, p, true)) return 0;
    const int q = q_for(mode, p);
    if (q == -1) {
        seq_deadline_ = 0;
        seq_first_ = 0;
        seq_follow_ = 0;
        return 0;
    }
    const bool alt = world_.seg18 != 0;
    const int w = alt ? world_.seg18_area_w : world_.area_w;
    const int h = alt ? world_.seg18_area_h : world_.area_h;
    const int r = ordinal9(id, p, q, 1, 1, w, h, now);
    if (r != 0) {
        last_id_ = id;
        last_a_ = a;
        last_b_ = b;
        last_mode_ = mode;
        last_p_ = p;
        last_q_ = q;
    }
    return r;
}

// seg10:0x7b48 [BYTES seg10:0x7b98 seg10:0x7bb6 seg10:0x7c05 seg10:0x7c1c seg10:0x7c28
// seg10:0x7cbc seg10:0x7cc0 seg10:0x7d30]. The original compares GetCurrentTime with the
// deadline; the port's caller passes its tick as `now` [DESIGN].
void Engine::wrap_7b48(std::uint32_t now) {
    pump();
    if (seq_deadline_ != 0 && now > seq_deadline_) {
        const bool go = world_.mix_channels > 1 || (seq_a_ == 0xe && seq_b_ == world_.player) ||
                        (last_id_ == seq_first_ && last_a_ == seq_a_ && last_b_ == seq_b_) ||
                        seq_follow_ == 0x66 || seq_first_ >= 0x38;
        if (go) {
            const int follow = seq_follow_;
            wrap_754a(0x69, last_a_, last_b_, last_mode_, now);
            wrap_754a(follow, last_a_, last_b_, last_mode_, now);
        }
        seq_deadline_ = 0;
        seq_follow_ = 0;
    }
    if (last_mode_ == 4) return;
    Point p;
    if (last_mode_ == 5) {
        p = last_p_;  // an uninitialised p in the original [OPEN]
    } else if (!point_for(last_mode_, last_a_, last_b_, p, false)) {
        return;
    }
    ordinal8(world_.area_w, world_.area_h, 1, 1, p);
}

}  // namespace is::audio
