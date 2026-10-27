// The Options/Setup and Audio Options dialogs (audio_options.h, docs/re/sound.md "Audio Options
// and Options"). The rules first (portable), then the Win32 procedures.
#include "dialogs/audio_options.h"

#include <algorithm>
#include <cstdio>

#include "settings/settings.h"
#include "formats/bytes.h"

namespace is::dialogs {

// ---------------------------------------------------------------- portable rules

std::vector<int> sound_system_strings(int capability) {
    std::vector<int> out{2233};  // [BYTES ISPACE.DLL seg9:0x18ea]
    for (int level = 1; level <= 3; ++level)
        if (capability >= level) out.push_back(2233 + level);  // seg9:0x1919 0x194b 0x1981
    return out;
}

std::vector<int> options_disabled(int capability) {
    std::vector<int> out;
    if (capability < 2) out.push_back(opt::kVolumeBar);  // [BYTES ISPACE.DLL seg9:0x19b7]
    if (capability < 1) {                                // [BYTES ISPACE.DLL seg9:0x19d0]
        out.push_back(opt::kSoundSystem);
        out.push_back(opt::kAudio);
    }
    return out;
}

std::vector<int> audio_disabled(int capability) {
    std::vector<int> out;
    if (capability < 3) {
        out.push_back(aud::kSwap);
        out.push_back(aud::kStereoEffect);
    }
    if (capability < 2) out.push_back(aud::kBalanceBar);
    if (capability < 1)
        for (int id : {aud::kMusic, aud::kSoundFx, aud::kVoiceFx, aud::kMixChannels, aud::kKeep})
            out.push_back(id);
    return out;
}

std::vector<std::string> mix_channel_entries(bool wavemix) {
    const int n = wavemix ? 8 : 1;
    std::vector<std::string> out;
    for (int i = 1; i <= n; ++i) out.push_back(std::to_string(i));
    return out;
}

int mix_channels_of_index(int index) { return std::clamp(index + 1, 1, 8); }

int mix_channel_index(int channels, bool wavemix) {
    return std::clamp(channels - 1, 0, wavemix ? 7 : 0);
}

int sound_bar_step(int value, int code, int pos) {
    switch (code) {
    case 0:  // SB_LINEUP
    case 2:  // SB_PAGEUP
        --value;
        break;
    case 1:  // SB_LINEDOWN
    case 3:  // SB_PAGEDOWN
        ++value;
        break;
    case 4:  // SB_THUMBPOSITION
    case 5:  // SB_THUMBTRACK
        value = pos;
        break;
    default: break;
    }
    return std::clamp(value, 0, 100);
}

int speed_position(int r) {
    long p;
    if (r == 1200)
        p = 1000;
    else if (r <= 36)
        p = (static_cast<long>(r) - 4) * 500 / 32;
    else
        p = (static_cast<long>(r) - 36) * 500 / 70 + 500;
    return p < 0 ? 0 : static_cast<int>(p);
}

int speed_rate(int p) {
    // timing.md "Rate settings" [BYTES ISPACE.DLL seg9:0x2242].
    if (p >= 1000) return 1200;
    const int r = p <= 500 ? p * 32 / 500 + 4 : (p - 500) * 70 / 500 + 36;
    return r > 104 ? 1200 : r;
}

int speed_bar_step(int value, int code, int pos) {
    switch (code) {
    case 0:
    case 2: value -= 50; break;
    case 1:
    case 3: value += 50; break;
    case 4:
    case 5: value = pos; break;
    case 6: value = 1000; break;  // SB_TOP
    case 7: value = 0; break;     // SB_BOTTOM
    default: break;
    }
    return std::clamp(value, 0, 1000);
}

std::string speed_label(int position) {
    const int r = speed_rate(position);
    char rate[40];
    if (r == 1200)
        std::snprintf(rate, sizeof rate, "%s", "Full Out");
    else
        std::snprintf(rate, sizeof rate, "%d frames/sec", r);
    return std::string("Play Speed  ") + rate;
}

std::vector<IniWrite> options_ok_writes(const OptionsValues& v) {
    const auto s = [](int n) { return std::to_string(n); };
    const std::string size = s(2 - v.zoom_index);
    return {{"Complex", s(v.complex)},
            {"Braking", s(v.braking)},
            {"Enhance", s(v.enhance)},
            {"SoundVolume", s(v.sound_volume)},
            {"SoundSystem", s(v.sound_system)},
            {"Quick", s(v.quick)},
            {"Detail", s(3 - v.detail_index)},
            {"Size", size},
            {"SetSize", size},
            {"FrameRateSet", s(speed_rate(v.speed))}};
}

std::vector<IniWrite> audio_ok_writes(const AudioValues& v) {
    const auto s = [](int n) { return std::to_string(n); };
    return {{"StereoEffect", s(v.stereo_effect)},
            {"FXMusic", s(v.fx_music)},
            {"Music", s(v.music)},
            {"SoundFX", s(v.sound_fx)},
            {"VoiceFX", s(v.voice_fx)},
            {"MixChannels", s(mix_channels_of_index(v.mix_index))},
            {"PurgeSound", s(1 - v.keep)},
            {"StereoSwap", s(v.stereo_swap)}};
}

bool options_restarts_music(bool ok, int music_at_open, int music_now, int old_system,
                            int new_system) {
    if (music_now != 0 && music_at_open == 0) return true;
    // On Cancel the second test reads a local only OK writes [BYTES ISPACE.DLL seg9:0x1fc1
    // seg9:0x222b]; the port takes it as unchanged [OPEN ISPACE.DLL seg9:0x222b].
    return ok && old_system == 0 && new_system != 0;
}

audio::Settings audio_settings(const settings::IniFile& ini) {
    using settings::kGameSection;
    const auto i = [&](const char* key, int def) { return ini.get_int(kGameSection, key, def); };
    audio::Settings s{};
    s.sound_fx = i("SoundFX", 1);
    s.voice_fx = i("VoiceFX", 1);
    s.music = i("Music", 1);
    s.sound_volume = i("SoundVolume", 100);
    s.fx_music = i("FXMusic", 50);
    s.stereo_effect = i("StereoEffect", 2);
    s.purge_sound = i("PurgeSound", 1);
    s.stereo_swap = i("StereoSwap", 0);
    s.sound_system = i("SoundSystem", -1);
    s.mix_channels = i("MixChannels", 4);
    s.saver_volume = i("SaverVolume", 0);
    s.force_vol = i("ForceVol", 0);
    s.allow_sound = i("AllowSound", 0);
    // SndPath: cp1252 with backslashes; the audio module resolves it (ADR 0003) [DESIGN].
    // Decoded as cp1252, backslashes kept: a narrow std::filesystem::path throws on a non-UTF-8
    // byte under MinGW (M7 s2 fixer).
    s.snd_path = std::filesystem::path(fmt::cp1252_to_utf16(ini.get_string(kGameSection, "SndPath", "", 0x6d)));
    s.language = ini.get_string(kGameSection, "Language", "", 5);
    return s;
}

}  // namespace is::dialogs

// ---------------------------------------------------------------- Win32

#ifdef _WIN32
#include "assets/asset_store.h"
#include "dialogs/dialogs.h"

namespace is::dialogs {

namespace {

audio::Settings g_live{};
bool g_live_loaded = false;
HWND g_options_open = nullptr;  // ISPACE.DLL DGROUP:0x69c

INT_PTR CALLBACK options_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    OptionsDialogBase* d = nullptr;
    if (msg == WM_INITDIALOG) {
        d = reinterpret_cast<OptionsDialogBase*>(l);
        SetWindowLongPtrW(h, DWLP_USER, l);
    } else {
        d = reinterpret_cast<OptionsDialogBase*>(GetWindowLongPtrW(h, DWLP_USER));
    }
    if (!d) return FALSE;
    try {  // no exception may unwind through user32's frames [DESIGN]
        return d->handle(h, msg, w, l);
    } catch (...) {
    }
    return FALSE;
}

std::string class_name(HWND h) {
    wchar_t buf[64] = L"";
    GetClassNameW(h, buf, 64);
    return narrow(buf);
}

// The capability level of the live settings: SoundSystem -1 resolves to the level and the
// value is clamped from above to it in memory (sound.md "Modes and device test" item 5).
int resolved_system(int sound_system, int capability) {
    if (sound_system == -1) sound_system = capability;
    return std::min(sound_system, capability);
}

}  // namespace

audio::Settings& live_settings() { return g_live; }

void load_live_settings(const settings::IniFile& ini) {
    g_live = audio_settings(ini);
    g_live_loaded = true;
}

// ---- the common part of both procedures

OptionsDialogBase::~OptionsDialogBase() {
    if (hwnd_ && IsWindow(hwnd_) && !modal_) {
        SetWindowLongPtrW(hwnd_, DWLP_USER, 0);
        DestroyWindow(hwnd_);
    }
    if (normal_) DeleteObject(normal_);
    if (bold_) DeleteObject(bold_);
}

int OptionsDialogBase::run_modal() {
    if (!env_.store) return -1;
    std::vector<std::uint8_t> t;
    try {
        // DS_NOFAILCREATE: SS_ICON 591 names icon 246, which ENG_LANG.DLL lacks [DESIGN].
        t = env_.store->dialog_template(template_, {}, DS_NOFAILCREATE);
    } catch (const std::exception& x) {
        if (env_.todo) env_.todo(std::string("dialog template: ") + x.what(), "ISPACE.DLL seg9:0x15a8");
        return -1;
    }
    modal_ = true;
    const INT_PTR r = DialogBoxIndirectParamW(env_.instance,
                                              reinterpret_cast<const DLGTEMPLATE*>(t.data()),
                                              env_.owner, options_proc, reinterpret_cast<LPARAM>(this));
    hwnd_ = nullptr;
    return static_cast<int>(r);
}

HWND OptionsDialogBase::create_modeless() {
    if (!env_.store) return nullptr;
    const auto t = env_.store->dialog_template(template_, {}, DS_NOFAILCREATE);
    modal_ = false;
    return CreateDialogIndirectParamW(env_.instance, reinterpret_cast<const DLGTEMPLATE*>(t.data()),
                                      env_.owner, options_proc, reinterpret_cast<LPARAM>(this));
}

INT_PTR OptionsDialogBase::handle(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_INITDIALOG:
        hwnd_ = h;
        // ISPACE.DLL seg9:0x29c0's two fonts, then seg10:0x602a(hDlg, 0) (as dialogs.cpp).
        normal_ = CreateFontW(14, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0x22, L"Arial");
        bold_ = CreateFontW(14, 6, 0, 0, 730, 0, 0, 0, 0, 0, 0, 2, 0x22, L"Arial");
        for (HWND c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
            const auto style = static_cast<std::uint32_t>(GetWindowLongW(c, GWL_STYLE));
            SendMessageW(c, WM_SETFONT,
                         reinterpret_cast<WPARAM>(bold_child(class_name(c), style) ? bold_ : normal_),
                         TRUE);
            ShowWindow(c, SW_SHOW);
        }
        init();
        return FALSE;  // both procedures set the focus themselves or leave it
    case WM_COMMAND:
        command(LOWORD(w));
        return TRUE;
    case WM_HSCROLL:
        // Win32 packs the code in LOWORD(wParam), the position in HIWORD and the bar in lParam
        // (Win16: code in wParam, position in LOWORD(lParam), bar in HIWORD(lParam)).
        hscroll(GetDlgCtrlID(reinterpret_cast<HWND>(l)), LOWORD(w), static_cast<short>(HIWORD(w)));
        return TRUE;
    default: break;
    }
    return FALSE;
}

void OptionsDialogBase::end(int r) {
    result_ = r;
    ended_ = true;
    if (modal_) EndDialog(hwnd_, r);
}

void OptionsDialogBase::write(const IniWrite& w) {
    writes_.push_back(w);
    if (!env_.ini) return;
    env_.ini->set(settings::kGameSection, w.key, w.value);
    if (env_.ini_changed) env_.ini_changed();
}

void OptionsDialogBase::enable(int id, bool on) {
    if (HWND c = GetDlgItem(hwnd_, id)) EnableWindow(c, on ? TRUE : FALSE);
}

void OptionsDialogBase::add(int id, const std::string& s) {
    SendDlgItemMessageW(hwnd_, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(s).c_str()));
}

void OptionsDialogBase::select(int id, int index) {
    SendDlgItemMessageW(hwnd_, id, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

int OptionsDialogBase::selection(int id) const {
    if (!GetDlgItem(hwnd_, id)) return 0;
    return static_cast<int>(SendDlgItemMessageW(hwnd_, id, CB_GETCURSEL, 0, 0));
}

void OptionsDialogBase::set_text(int id, const std::string& s) {
    SetDlgItemTextW(hwnd_, id, wide(s).c_str());
}

// ---- Audio Options #999

void AudioDialog::init() {  // ISPACE.DLL seg9:0x2f30 WM_INITDIALOG
    if (!g_live_loaded && env_.ini) load_live_settings(*env_.ini);
    const audio::Settings& s = g_live;
    // The centre position it computes is never applied [BYTES ISPACE.DLL seg9:0x2f4d].
    for (const std::string& e : mix_channel_entries(env_.wavemix)) add(aud::kMixChannels, e);
    select(aud::kMixChannels, mix_channel_index(s.mix_channels, env_.wavemix));
    for (int id = 2413; id <= 2416; ++id) add(aud::kStereoEffect, lang(id));
    select(aud::kStereoEffect, s.stereo_effect);
    for (int id : audio_disabled(env_.capability)) enable(id, false);
    CheckDlgButton(hwnd_, aud::kSoundFx, s.sound_fx ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd_, aud::kVoiceFx, s.voice_fx ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd_, aud::kMusic, s.music ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd_, aud::kKeep, s.purge_sound == 0 ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd_, aud::kSwap, s.stereo_swap ? BST_CHECKED : BST_UNCHECKED);
    SetScrollRange(GetDlgItem(hwnd_, aud::kBalanceBar), SB_CTL, 0, 100, FALSE);
    SetScrollPos(GetDlgItem(hwnd_, aud::kBalanceBar), SB_CTL, s.fx_music, TRUE);
    fx_music_shown_ = s.fx_music;
}

void AudioDialog::hscroll(int id, int code, int pos) {  // [BYTES ISPACE.DLL seg9:0x31c1-0x322c]
    if (id != aud::kBalanceBar) return;
    g_live.fx_music = sound_bar_step(g_live.fx_music, code, pos);
    if (env_.reread) env_.reread(g_live);  // ISPACE.DLL seg9:0xa9e [BYTES seg9:0x320c]
    if (g_live.fx_music != fx_music_shown_) {
        SetScrollPos(GetDlgItem(hwnd_, aud::kBalanceBar), SB_CTL, g_live.fx_music, TRUE);
        fx_music_shown_ = g_live.fx_music;
    }
}

void AudioDialog::command(int id) {
    switch (id) {
    case aud::kOk: ok(); break;
    case aud::kCancel: end(0); break;  // Cancel only ends the dialog [BYTES ISPACE.DLL seg9:0x332d]
    case aud::kHelp:
        if (env_.help) env_.help();  // [BYTES ISPACE.DLL seg9:0x3262 seg9:0x328e]
        break;
    default: break;
    }
}

void AudioDialog::ok() {  // ISPACE.DLL seg9:0x331a
    AudioValues v;
    v.stereo_effect = selection(aud::kStereoEffect);
    v.fx_music = g_live.fx_music;
    v.music = IsDlgButtonChecked(hwnd_, aud::kMusic) ? 1 : 0;
    v.sound_fx = IsDlgButtonChecked(hwnd_, aud::kSoundFx) ? 1 : 0;
    v.voice_fx = IsDlgButtonChecked(hwnd_, aud::kVoiceFx) ? 1 : 0;
    v.mix_index = selection(aud::kMixChannels);
    v.keep = IsDlgButtonChecked(hwnd_, aud::kKeep) ? 1 : 0;
    v.stereo_swap = IsDlgButtonChecked(hwnd_, aud::kSwap) ? 1 : 0;
    for (const IniWrite& w : audio_ok_writes(v)) write(w);
    g_live.stereo_effect = v.stereo_effect;
    g_live.music = v.music;
    g_live.sound_fx = v.sound_fx;
    g_live.voice_fx = v.voice_fx;
    g_live.purge_sound = 1 - v.keep;
    g_live.stereo_swap = v.stereo_swap;
    // Items 2-4: the module loads and unloads, and on a channel-count change with a non-zero
    // sound system the session reopen with MixChannels re-read and the preloads, belong to the
    // audio module, which applies them from the settings it is handed [BYTES ISPACE.DLL
    // seg9:0x34e4-0x35ae] [DESIGN ADR 0011].
    g_live.mix_channels = mix_channels_of_index(v.mix_index);
    if (env_.reread) env_.reread(g_live);
    end(1);
}

// ---- Options/Setup #100

OptionsDialog::~OptionsDialog() {
    if (g_options_open == hwnd_) g_options_open = nullptr;
}

bool OptionsDialog::open() { return g_options_open != nullptr; }

void OptionsDialog::show_speed(int position) {  // ISPACE.DLL seg9:0x1ea8
    SetScrollPos(GetDlgItem(hwnd_, opt::kSpeedBar), SB_CTL, position, TRUE);
    set_text(opt::kSpeedLabel, speed_label(position));
}

void OptionsDialog::init() {  // ISPACE.DLL seg9:0x15e2-0x1cb3
    g_options_open = hwnd_;  // DGROUP:0x69c [BYTES ISPACE.DLL seg9:0x15e5]
    if (!g_live_loaded && env_.ini) load_live_settings(*env_.ini);
    RECT r;
    GetWindowRect(hwnd_, &r);
    SendDlgItemMessageW(hwnd_, opt::kIcon, STM_SETICON, reinterpret_cast<WPARAM>(env_.game_icon), 0);
    const auto ini = [&](const char* key, int def) {
        return env_.ini ? env_.ini->get_int(settings::kGameSection, key, def) : def;
    };
    music_at_open_ = g_live.music;  // DGROUP:0x2220 [BYTES ISPACE.DLL seg9:0x1683]
    // Play Detail 126: 2221-2224, selected 3 - Detail [BYTES ISPACE.DLL seg9:0x168b-0x16d3].
    // ISPACE.DLL keeps Detail, Quick and Enhance in memory from its INI reader; the port reads
    // the INI, which only this dialog and that reader write [DESIGN].
    for (int i = 0; i < 4; ++i) add(opt::kDetail, lang(2221 + i));
    select(opt::kDetail, 3 - ini("Detail", 3));
    CheckDlgButton(hwnd_, opt::kEnhance, ini("Enhance", 1) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd_, opt::kBraking, ini("Braking", 0) ? BST_CHECKED : BST_UNCHECKED);
    for (int i = 0; i < 3; ++i) add(opt::kQuick, lang(2225 + i));  // [BYTES seg9:0x170f]
    select(opt::kQuick, ini("Quick", 1));
    for (int i = 0; i < 5; ++i) add(opt::kDifficulty, lang(1035 + i));  // [BYTES seg9:0x1773]
    select(opt::kDifficulty, ini("Complex", 2));
    for (int i = 0; i < 3; ++i) add(opt::kZoom, lang(2230 + i));  // [BYTES seg9:0x17c2]
    select(opt::kZoom, 2 - ini("Size", 1));
    // The gate on ISPACE.DLL DGROUP:0x2136 = 1 (controls 0x7f, 0x7d, 0x73, 0x7e, 0x77, 0x72,
    // 0x82, 0x84, 0x89, 0x8a) needs a writer the port does not have [OPEN ISPACE.DLL seg9:0x1824].
    for (int id : sound_system_strings(env_.capability)) add(opt::kSoundSystem, lang(id));
    for (int id : options_disabled(env_.capability)) enable(id, false);
    select(opt::kSoundSystem, resolved_system(g_live.sound_system, env_.capability));
    speed_ = speed_position(ini("FrameRateSet", 36));  // [BYTES ISPACE.DLL seg9:0x1a25-0x1aad]
    speed_shown_ = speed_;
    SetScrollRange(GetDlgItem(hwnd_, opt::kSpeedBar), SB_CTL, 0, 1000, FALSE);
    show_speed(speed_);
    SetScrollRange(GetDlgItem(hwnd_, opt::kVolumeBar), SB_CTL, 0, 100, FALSE);
    SetScrollPos(GetDlgItem(hwnd_, opt::kVolumeBar), SB_CTL, g_live.sound_volume, TRUE);
    volume_shown_ = g_live.sound_volume;
    // MoveWindow to the screen centre [BYTES ISPACE.DLL seg9:0x15ef-0x1632 seg9:0x1b21].
    const Placement p = centred(r.left, r.top, r.right, r.bottom, GetSystemMetrics(SM_CXSCREEN),
                                GetSystemMetrics(SM_CYSCREEN));
    MoveWindow(hwnd_, p.x, p.y, p.w, p.h, TRUE);
    SetFocus(GetDlgItem(hwnd_, opt::kSpeedBar));
    // Labels 144 and 146: the module kind [BYTES ISPACE.DLL seg9:0x1b36-0x1bf1].
    for (const bool voice : {false, true}) {
        const std::string kind = env_.module_kind ? env_.module_kind(voice) : std::string();
        if (!kind.empty()) set_text(voice ? opt::kVoiceKind : opt::kFxKind, kind);
    }
    set_text(opt::kLanguage, lang(2309));  // [BYTES ISPACE.DLL seg9:0x1bf6-0x1c1d]
    // 122: "V0.00.00" with INSPACE.EXE strings 167 and 233 put in [BYTES ISPACE.DLL seg9:0x1f38];
    // 123: INSPACE.EXE 1800; 136: INSPACE.EXE 607 [BYTES ISPACE.DLL seg9:0x1c5c-0x1cae].
    if (env_.exe_string) {
        std::string v = "V0.00.00";
        const std::string a = env_.exe_string(167), b = env_.exe_string(233);
        const auto at = [](const std::string& s, std::size_t i) { return i < s.size() ? s[i] : '\0'; };
        v[1] = at(a, 0), v[3] = at(a, 1), v[4] = at(a, 2), v[6] = at(b, 0), v[7] = at(b, 1);
        set_text(opt::kVersion, v.c_str());  // a NUL ends the text, as lstrcpy's string does
        set_text(opt::kName, env_.exe_string(1800));
        set_text(opt::kRelease, env_.exe_string(607));
    }
}

void OptionsDialog::hscroll(int id, int code, int pos) {
    if (id == opt::kSpeedBar) {  // [BYTES ISPACE.DLL seg9:0x1cce-0x1d42]
        speed_ = speed_bar_step(speed_, code, pos);
        if (speed_ != speed_shown_) {
            show_speed(speed_);
            speed_shown_ = speed_;
        }
        return;
    }
    if (id != opt::kVolumeBar) return;
    if (code == 8 && env_.sound) env_.sound(kVolumeTestSound);  // SB_ENDSCROLL [BYTES seg9:0x1d4a]
    g_live.sound_volume = sound_bar_step(g_live.sound_volume, code, pos);
    if (env_.reread) env_.reread(g_live);  // ISPACE.DLL seg9:0xa9e [BYTES seg9:0x1d9e]
    if (g_live.sound_volume != volume_shown_) {
        SetScrollPos(GetDlgItem(hwnd_, opt::kVolumeBar), SB_CTL, g_live.sound_volume, TRUE);
        volume_shown_ = g_live.sound_volume;
    }
}

void OptionsDialog::command(int id) {  // [BYTES ISPACE.DLL seg9:0x1dce-0x1e81]
    switch (id) {
    case opt::kOk: close(true); break;
    case opt::kCancel: close(false); break;
    case opt::kHelp:
        if (env_.help) env_.help();
        break;
    case opt::kNormalSpeed:
        speed_ = speed_shown_ = 500;
        show_speed(speed_);
        SetFocus(GetDlgItem(hwnd_, opt::kSpeedBar));
        break;
    case opt::kAudio: {  // DialogBox(DIALOG 999, ordinal 38) [BYTES ISPACE.DLL seg9:0x1e14]
        AudioDialog a(env_);
        OptionsEnv& e = env_;
        const HWND owner = e.owner;
        e.owner = hwnd_;
        a.run_modal();
        e.owner = owner;
        break;
    }
    case opt::kDisplay:
        if (env_.todo) env_.todo("Display dialog #029a (ordinal 37)", "ISPACE.DLL seg9:0x2bf8");
        break;
    case opt::kJoystick:
        if (env_.todo) env_.todo("Joystick dialog #03cf (ordinal 42)", "ISPACE.DLL seg9:0x40da");
        break;
    default: break;
    }
}

void OptionsDialog::close(bool ok) {  // ISPACE.DLL seg9:0x1fae
    const int old_system = resolved_system(g_live.sound_system, env_.capability);
    int new_system = old_system;
    if (ok) {
        OptionsValues v;
        v.zoom_index = selection(opt::kZoom);
        v.detail_index = selection(opt::kDetail);
        v.quick = selection(opt::kQuick);
        v.complex = selection(opt::kDifficulty);
        new_system = selection(opt::kSoundSystem);
        v.sound_system = new_system;
        v.enhance = IsDlgButtonChecked(hwnd_, opt::kEnhance) ? 1 : 0;
        v.braking = IsDlgButtonChecked(hwnd_, opt::kBraking) ? 1 : 0;
        v.sound_volume = g_live.sound_volume;
        v.speed = speed_;
        g_live.sound_system = new_system;
        for (const IniWrite& w : options_ok_writes(v)) write(w);
        // A changed sound system re-runs the INI reader [BYTES ISPACE.DLL seg9:0x2207].
        if (new_system != old_system && env_.ini) load_live_settings(*env_.ini);
        if (env_.reread) env_.reread(g_live);
    }
    if (options_restarts_music(ok, music_at_open_, g_live.music, old_system, new_system) &&
        env_.music_restart)
        env_.music_restart();  // seg9:0x3906(DGROUP:0xaa4, 1) [BYTES ISPACE.DLL seg9:0x220a]
    end(ok ? 1 : 0);
}

int options_ordinal10(OptionsEnv& env) {  // ISPACE.DLL seg9:0x1546
    if (OptionsDialog::open()) {
        const std::string text = env.lang ? env.lang(2220) : std::string();
        if (env.message_box)
            env.message_box(nullptr, text, "Inner Space", MB_OK | MB_ICONEXCLAMATION);
        else
            MessageBoxW(nullptr, wide(text).c_str(), L"Inner Space", MB_OK | MB_ICONEXCLAMATION);
        return 0;
    }
    if (env.sound) {  // seg9:0x298c(0x6e), (0x13) [BYTES ISPACE.DLL seg9:0x158f seg9:0x1596]
        env.sound(0x6e);
        env.sound(0x13);
    }
    OptionsDialog d(env);
    const int r = d.run_modal();
    g_options_open = nullptr;  // [BYTES ISPACE.DLL seg9:0x15ba]
    return r;
}

}  // namespace is::dialogs
#endif
