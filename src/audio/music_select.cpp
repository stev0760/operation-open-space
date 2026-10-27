// The music backend choice (music_select.h), ADR 0011 decision 3 [DESIGN].
#include "audio/music_select.h"

#include "audio/music_tracks.h"

#include <cctype>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace is::audio::music {

namespace {

std::string lower_trimmed(std::string_view v) {
    std::size_t a = 0, b = v.size();
    while (a < b && std::isspace(static_cast<unsigned char>(v[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(v[b - 1]))) --b;
    std::string s(v.substr(a, b - a));
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool file_exists(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::is_regular_file(p, ec);
}

}  // namespace

Kind parse_kind(std::string_view value, bool* known) {
    const std::string s = lower_trimmed(value);
    if (known) *known = true;
    if (s.empty() || s == "mci") return Kind::kMci;
    if (s == "soundfont") return Kind::kSoundFont;
    if (s == "opl3") return Kind::kOpl3;
    if (known) *known = false;
    return Kind::kMci;
}

const char* kind_name(Kind k) {
    switch (k) {
    case Kind::kSoundFont: return "soundfont";
    case Kind::kOpl3: return "opl3";
    default: return "mci";
    }
}

bool under_wine() {
#ifdef _WIN32
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
#else
    return false;
#endif
}

std::filesystem::path resolve_soundfont(const std::filesystem::path& key, bool wine,
                                        const std::function<bool(const std::filesystem::path&)>& exists) {
    if (!key.empty()) return key;
    if (!wine) return {};
    const std::filesystem::path def{std::string(kWineSoundFont)};
    const bool there = exists ? exists(def) : file_exists(def);
    return there ? def : std::filesystem::path();
}

std::unique_ptr<Backend> select_backend(const std::string& music_key,
                                        const std::filesystem::path& soundfont_key,
                                        const SelectEnv& env) {
    auto log = [&](const std::string& s) {
        if (env.log) env.log(s);
    };
    bool known = true;
    Kind kind = parse_kind(music_key, &known);
    if (!known) log("music: [Port] Music=" + music_key + " is not mci, soundfont or opl3; using the default");
    const std::filesystem::path sf2 = resolve_soundfont(soundfont_key, env.wine, env.exists);

    // The default rule [DESIGN] (ADR 0011 decision 3 as amended for stage 2b): mci, except under
    // Wine with an empty key, where Wine's MCI opens a sequencer whose port ("Midi Through") has
    // no synth behind it. There the session takes opl3 when it is built in and opens (the FM
    // rendering is what the game was written on), then soundfont when a SoundFont resolves and
    // opens, then mci. An explicit mci is always mci.
    if (kind != Kind::kMci) {
        std::unique_ptr<Backend> b = env.make_synth ? env.make_synth(kind, sf2) : nullptr;
        if (b) {
            log(std::string("music: backend ") + kind_name(kind) +
                (kind == Kind::kSoundFont ? " (" + sf2.string() + ")" : std::string()));
            return b;
        }
        log(std::string("music: backend ") + kind_name(kind) + " unavailable; using the default");
    } else if (known && lower_trimmed(music_key).empty() && env.wine && env.make_synth) {
        if (std::unique_ptr<Backend> b = env.make_synth(Kind::kOpl3, sf2)) {
            log("music: backend opl3, the Wine default");
            return b;
        }
        log("music: opl3 not built in or did not open");
        if (!sf2.empty()) {
            if (std::unique_ptr<Backend> b = env.make_synth(Kind::kSoundFont, sf2)) {
                log("music: backend soundfont (" + sf2.string() + "), the Wine default");
                return b;
            }
            log("music: soundfont " + sf2.string() + " did not open; using mci");
        }
    }
    log("music: backend mci");
    return env.make_mci ? env.make_mci() : nullptr;
}

}  // namespace is::audio::music
