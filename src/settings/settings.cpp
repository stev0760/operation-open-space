// INSPACE.INI for the port: the profile semantics, the [Inner Space] keys and the regeneration
// rule of formats.md. Portable: no Win32 calls.
#include "settings/settings.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "formats/bytes.h"

namespace is::settings {

namespace fs = std::filesystem;

namespace {

// White space as Wine's profile code trims it (isspaceW on the ASCII range).
bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && is_space(s.front())) s.remove_prefix(1);
    while (!s.empty() && is_space(s.back())) s.remove_suffix(1);
    return s;
}

bool same_name(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           is::fmt::fold_name(std::string(a)) == is::fmt::fold_name(std::string(b));
}

// UTF-16 to cp1252; nullopt when a code unit has no cp1252 byte.
std::optional<std::string> cp1252_from_utf16(const std::u16string& w) {
    static const std::u16string table = [] {
        std::string all(256, '\0');
        for (int i = 0; i < 256; ++i) all[static_cast<std::size_t>(i)] = static_cast<char>(i);
        return is::fmt::cp1252_to_utf16(all);
    }();
    std::string out;
    out.reserve(w.size());
    for (char16_t c : w) {
        std::size_t i = table.find(c);
        if (i == std::u16string::npos) return std::nullopt;
        out.push_back(static_cast<char>(i));
    }
    return out;
}

// A directory as the original writes it: cp1252, backslashes, ending with a backslash
// (seg6:0x739 cuts the module path after its last '\'). On Windows u16string() is the native
// UTF-16; on the host it decodes the UTF-8 string.
std::string windows_dir(const fs::path& dir) {
    std::u16string w;
    try {
        w = dir.u16string();
    } catch (const std::exception&) {
        throw std::runtime_error("settings: directory is not text: " + is::fmt::path_utf8(dir));
    }
    std::optional<std::string> s = cp1252_from_utf16(w);
    if (!s)
        throw std::runtime_error("settings: cp1252 cannot hold the directory " +
                                 is::fmt::path_utf8(dir));
    for (char& c : *s)
        if (c == '/') c = '\\';
    if (s->empty() || s->back() != '\\') s->push_back('\\');
    return *s;
}

}  // namespace

// ---- IniFile

IniFile::IniFile(fs::path path) : path_(std::move(path)) {
    std::error_code ec;
    if (!fs::is_regular_file(path_, ec)) return;
    std::ifstream in(path_, std::ios::binary);
    if (!in) throw std::runtime_error("settings: cannot read " + is::fmt::path_utf8(path_));
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    existed_ = true;

    bool open = false;  // a section exists; the lines go to sections_.back()
    auto current = [&]() -> Section& {
        if (!open) {
            sections_.push_back(Section{});
            sections_.back().header = false;
            open = true;
        }
        return sections_.back();
    };
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string_view line(text.data() + pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

        const std::string_view body = trim(line);
        if (!body.empty() && body.front() == '[') {
            const std::size_t close = body.rfind(']');
            if (close != std::string_view::npos) {
                sections_.push_back(Section{std::string(body.substr(1, close - 1)), {}, true, {}});
                open = true;
                continue;
            }
        }
        Section& sec = current();
        const std::size_t eq = body.find('=');
        const std::string_view key =
            eq == std::string_view::npos ? std::string_view{} : trim(body.substr(0, eq));
        if (body.empty() || body.front() == ';' || key.empty()) {
            sec.other.emplace_back(sec.entries.size(), std::string(line));
            continue;
        }
        sec.entries.emplace_back(std::string(key), std::string(trim(body.substr(eq + 1))));
    }
}

IniFile::Section* IniFile::find(std::string_view section) {
    for (Section& s : sections_)
        if (same_name(s.name, section)) return &s;
    return nullptr;
}

const IniFile::Section* IniFile::find(std::string_view section) const {
    for (const Section& s : sections_)
        if (same_name(s.name, section)) return &s;
    return nullptr;
}

std::optional<std::string> IniFile::get(std::string_view section, std::string_view key,
                                        std::size_t nsize) const {
    if (key.empty()) return std::nullopt;
    for (const Section& s : sections_) {
        if (!same_name(s.name, section)) continue;
        for (const auto& [k, v] : s.entries) {
            if (!same_name(k, key)) continue;
            std::string value = v;
            // GetPrivateProfileString drops one pair of matching quotes.
            if (value.size() > 1 && (value.front() == '"' || value.front() == '\'') &&
                value.back() == value.front())
                value = value.substr(1, value.size() - 2);
            if (nsize > 0 && value.size() > nsize - 1) value.resize(nsize - 1);
            return value;
        }
    }
    return std::nullopt;
}

std::string IniFile::get_string(std::string_view section, std::string_view key,
                                std::string_view def, std::size_t nsize) const {
    if (std::optional<std::string> v = get(section, key, nsize)) return *v;
    std::string d(def);
    if (nsize > 0 && d.size() > nsize - 1) d.resize(nsize - 1);
    return d;
}

int IniFile::get_int(std::string_view section, std::string_view key, int def) const {
    const std::optional<std::string> v = get(section, key);
    if (!v || v->empty()) return def;
    // RtlUnicodeStringToInteger with base 10, as Wine's GetPrivateProfileInt calls it:
    // leading characters up to ' ' skipped, one sign, digits until any other character.
    std::size_t i = 0;
    while (i < v->size() && static_cast<unsigned char>((*v)[i]) <= ' ') ++i;
    bool neg = false;
    if (i < v->size() && ((*v)[i] == '-' || (*v)[i] == '+')) neg = (*v)[i++] == '-';
    std::uint32_t n = 0;
    for (; i < v->size() && (*v)[i] >= '0' && (*v)[i] <= '9'; ++i)
        n = n * 10u + static_cast<std::uint32_t>((*v)[i] - '0');
    if (neg) n = 0u - n;
    return static_cast<int>(static_cast<std::int32_t>(n));
}

void IniFile::set(std::string_view section, std::string_view key, std::string value) {
    key = trim(key);
    if (key.empty()) return;
    value = std::string(trim(value));
    for (Section& s : sections_) {
        if (!same_name(s.name, section)) continue;
        for (auto& [k, v] : s.entries) {
            if (same_name(k, key)) {
                v = std::move(value);
                return;
            }
        }
    }
    Section* sec = find(section);
    if (!sec) {
        // A new section follows a blank line, as Windows writes it.
        if (!sections_.empty()) {
            Section& last = sections_.back();
            const bool blank_end = !last.other.empty() &&
                                   last.other.back().first == last.entries.size() &&
                                   trim(last.other.back().second).empty();
            if (!blank_end && (!last.entries.empty() || !last.other.empty() || last.header))
                last.other.emplace_back(last.entries.size(), std::string());
        }
        sections_.push_back(Section{std::string(section), {}, true, {}});
        sec = &sections_.back();
    }
    // The new key follows the last entry; blank lines that ended the section stay at its end.
    const std::size_t n = sec->entries.size();
    for (auto it = sec->other.rbegin(); it != sec->other.rend() && it->first == n; ++it) {
        if (!trim(it->second).empty()) break;
        it->first = n + 1;
    }
    sec->entries.emplace_back(std::string(key), std::move(value));
}

void IniFile::set_int(std::string_view section, std::string_view key, int value) {
    set(section, key, std::to_string(value));  // seg6:0xf1a formats in decimal
}

void IniFile::remove(std::string_view section, std::string_view key) {
    for (Section& s : sections_) {
        if (!same_name(s.name, section)) continue;
        for (std::size_t i = 0; i < s.entries.size(); ++i) {
            if (!same_name(s.entries[i].first, key)) continue;
            s.entries.erase(s.entries.begin() + static_cast<std::ptrdiff_t>(i));
            for (auto& o : s.other)
                if (o.first > i) --o.first;
            return;
        }
    }
}

void IniFile::save() const {
    std::string out;
    for (const Section& s : sections_) {
        if (s.header) out += "[" + s.name + "]\r\n";
        std::size_t o = 0;
        for (std::size_t i = 0; i <= s.entries.size(); ++i) {
            for (; o < s.other.size() && s.other[o].first == i; ++o)
                out += s.other[o].second + "\r\n";
            if (i < s.entries.size())
                out += s.entries[i].first + "=" + s.entries[i].second + "\r\n";
        }
    }
    std::error_code ec;
    if (path_.has_parent_path()) fs::create_directories(path_.parent_path(), ec);
    std::ofstream f(path_, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("settings: cannot write " + is::fmt::path_utf8(path_));
    f.write(out.data(), static_cast<std::streamsize>(out.size()));
    f.close();
    if (!f) throw std::runtime_error("settings: cannot write " + is::fmt::path_utf8(path_));
}

std::vector<std::string> IniFile::sections() const {
    std::vector<std::string> names;
    for (const Section& s : sections_)
        if (s.header || !s.entries.empty()) names.push_back(s.name);
    return names;
}

std::vector<std::pair<std::string, std::string>> IniFile::entries(std::string_view section) const {
    std::vector<std::pair<std::string, std::string>> all;
    for (const Section& s : sections_)
        if (same_name(s.name, section)) all.insert(all.end(), s.entries.begin(), s.entries.end());
    return all;
}

// ---- GameSettings

namespace {

struct Field {
    std::string_view key;
    int& (*int_ref)(GameSettings&);          // integer keys
    std::string& (*str_ref)(GameSettings&);  // string keys
    std::size_t nsize;                       // string keys: the read size
};

#define IS_INT(k, m) Field{k, [](GameSettings& s) -> int& { return s.m; }, nullptr, 0}
#define IS_STR(k, m, n) Field{k, nullptr, [](GameSettings& s) -> std::string& { return s.m; }, n}

// The order of formats.md's tables; the test test_settings.cpp holds the two together.
const std::vector<Field>& fields() {
    static const std::vector<Field> f = {
        // Paths, identity and game choice
        IS_STR("Path", path, 0x6d),            // seg6:0x70a
        IS_STR("LocalPath", local_path, 0x6d),  // seg6:0x725
        IS_STR("ShipPath", ship_path, 0x6d),    // seg14:0x3dea
        IS_STR("SndPath", snd_path, 0x6d),      // ISPACE.DLL seg9:0x152
        IS_STR("RacePath", race_path, 0x6d),    // seg6:0x9c8
        IS_STR("Games", games, 11),             // seg6:0x9a0
        IS_STR("Dead", dead, 11),               // seg6:0x978
        IS_STR("Language", language, 5),        // seg2:0x1952
        IS_STR("ThePlayer", the_player, 20),    // seg10:0x8496
        IS_STR("MainShip", main_ship, 20),      // seg14:0x23bf
        IS_STR("Restore", restore, 9),          // seg6:0x8906
        IS_INT("PlayMode", play_mode),
        IS_STR("Drive", drive, 20),  // seg10:0x8525
        IS_INT("Size", size),
        IS_INT("SetSize", set_size),
        IS_INT("StratAsk", strat_ask),
        IS_INT("SavePrompt", save_prompt),
        IS_INT("Accept", accept),
        IS_INT("PlusCount", plus_count),
        // Window state and positions (seg6:0x0 reads, seg6:0x33e writes)
        IS_INT("Full", full),
        IS_INT("OnTop", on_top),
        IS_INT("ShowLeft", show_left),
        IS_INT("ShowTop", show_top),
        IS_INT("ShowRight", show_right),
        IS_INT("ShowBottom", show_bottom),
        IS_INT("IconWnd", icon_wnd),
        IS_INT("VScale", vscale),
        IS_INT("SmWnd", sm_wnd),
        IS_INT("SScale", sscale),
        IS_INT("ShowLaunch", show_launch),
        IS_INT("Menu", menu),
        IS_INT("RadarLeft", radar_left),
        IS_INT("RadarTop", radar_top),
        IS_INT("WatchLeft", watch_left),
        IS_INT("WatchTop", watch_top),
        IS_INT("WatchRight", watch_right),  // seg6:0x1e8, default WatchLeft + 280
        IS_INT("IconLeft", icon_left),      // seg6:0x205, default screen width - 48
        IS_INT("IconTop", icon_top),
        IS_INT("SmLeft", sm_left),
        IS_INT("SmTop", sm_top),
        IS_INT("HelpLeft", help_left),
        IS_INT("HelpTop", help_top),
        IS_INT("NavLeft", nav_left),
        IS_INT("NavTop", nav_top),
        IS_INT("NavRight", nav_right),
        IS_INT("NavBottom", nav_bottom),
        // Settings (seg6:0xb3e)
        IS_INT("GameSpeed", game_speed),
        IS_INT("FrameRateSet", frame_rate_set),
        IS_INT("Calibrate", calibrate),
        IS_INT("Detail", detail),
        IS_INT("Quick", quick),
        IS_INT("Enhance", enhance),
        IS_INT("Complex", complex),
        IS_INT("Braking", braking),
        IS_INT("MixChannels", mix_channels),
        IS_INT("BadVideo", bad_video),
        IS_INT("QuickIntro", quick_intro),
        IS_INT("ShipAnim", ship_anim),
        IS_INT("Border", border),
        IS_INT("Bright", bright),
        IS_INT("IgnoreRes", ignore_res),
        IS_INT("Aux", aux),
        IS_INT("Rate", rate),
        IS_INT("JoyStick", joystick),
        IS_INT("4Btns", four_btns),
        IS_INT("JMinX", jmin_x),
        IS_INT("JMinY", jmin_y),
        IS_INT("JRangeX", jrange_x),
        IS_INT("JRangeY", jrange_y),
        IS_INT("JBtn0", jbtn[0]),
        IS_INT("JBtn1", jbtn[1]),
        IS_INT("JBtn2", jbtn[2]),
        IS_INT("JBtn3", jbtn[3]),
        // Sound
        IS_INT("SoundFX", sound_fx),
        IS_INT("VoiceFX", voice_fx),
        IS_INT("Music", music),
        IS_INT("SoundVolume", sound_volume),
        IS_INT("FXMusic", fx_music),
        IS_INT("StereoEffect", stereo_effect),
        IS_INT("PurgeSound", purge_sound),
        IS_INT("StereoSwap", stereo_swap),
        IS_INT("SoundSystem", sound_system),
        IS_INT("AllowSound", allow_sound),
        IS_INT("ForceVol", force_vol),
        IS_INT("SaverVolume", saver_volume),
        // Special times of day
        IS_INT("Breakfast", breakfast),
        IS_INT("Coffee", coffee),
        IS_INT("Lunch", lunch),
        IS_INT("Tea", tea),
        // Race, duel and demo options
        IS_INT("Racers", racers),
        IS_INT("RaceDiff", race_diff),
        IS_INT("RaceLaps", race_laps),
        IS_INT("RaceOpp", race_opp),
        IS_INT("Duelers", duelers),
        IS_INT("DuelDiff", duel_diff),
        IS_INT("DuelOpp", duel_opp),
        IS_INT("DRacers", dracers),
        IS_INT("DDiff", ddiff),
        IS_INT("DLaps", dlaps),
        IS_INT("DemoType", demo_type),
        IS_INT("DScale", dscale),
        IS_STR("Track", track, 0x6d),  // seg6:0x3c8a
        IS_STR("Ship1", ship1, 20),    // seg14:0x236c
        IS_STR("Ship2", ship2, 20),    // seg14:0x239c
        IS_INT("Tree", tree),
    };
    return f;
}

#undef IS_INT
#undef IS_STR

}  // namespace

GameSettings GameSettings::defaults(int screen_width) {
    GameSettings s;
    s.icon_left = default_icon_left(screen_width);
    return s;
}

void GameSettings::load(const IniFile& ini, int screen_width) {
    GameSettings def = defaults(screen_width);
    for (const Field& f : fields()) {
        if (f.str_ref) {
            f.str_ref(*this) = ini.get_string(kGameSection, f.key, f.str_ref(def), f.nsize);
        } else {
            if (f.key == "WatchRight") def.watch_right = default_watch_right(watch_left);
            f.int_ref(*this) = ini.get_int(kGameSection, f.key, f.int_ref(def));
        }
    }
}

void GameSettings::store(IniFile& ini) const {
    GameSettings& self = const_cast<GameSettings&>(*this);  // the accessors only read here
    for (const Field& f : fields()) {
        if (f.str_ref)
            ini.set(kGameSection, f.key, f.str_ref(self));
        else
            ini.set_int(kGameSection, f.key, f.int_ref(self));
    }
}

std::vector<std::string_view> GameSettings::keys() {
    std::vector<std::string_view> k;
    for (const Field& f : fields()) k.push_back(f.key);
    return k;
}

// ---- Regeneration

bool regenerate(IniFile& ini, const fs::path& asset_dir, const fs::path& local_dir) {
    if (!ini.get_string(kGameSection, "Path", "", 0x6d).empty()) return false;  // seg6:0x7db
    const std::string dir = windows_dir(asset_dir);
    const std::string local = windows_dir(local_dir);
    // seg6:0x814 .. seg6:0x93e, in the original's order.
    ini.set(kGameSection, "Path", dir);
    ini.set(kGameSection, "LocalPath", local);
    ini.set(kGameSection, "ShipPath", dir + "SHIPS\\");
    ini.set(kGameSection, "SndPath", dir + "SOUNDS\\");
    ini.set(kGameSection, "RacePath", dir + "GAMES\\");
    ini.set(kGameSection, "Dead", "RETIRED");
    ini.set(kGameSection, "Games", "GAMES");
    ini.set(kGameSection, "Language", "ENG");
    ini.set(kGameSection, "MainShip", "TRANSFORMER");
    ini.set(kGameSection, "ThePlayer", "your name");
    return true;
}

void RegistrationSettings::load(const IniFile& ini) {
    present = false;
    for (const std::string& name : ini.sections())
        if (same_name(name, kRegistrationSection)) present = true;
    vidsp = ini.get_int(kRegistrationSection, "vidsp", 0);
    bit32sp = ini.get_int(kRegistrationSection, "32BitSP", 0);
    pdev = ini.get_int(kRegistrationSection, "PDev", 0);
}

void PortSettings::load(const IniFile& ini) {
    scale = ini.get_int(kPortSection, "Scale", 0);  // range: shell::scale_factor
}

}  // namespace is::settings
