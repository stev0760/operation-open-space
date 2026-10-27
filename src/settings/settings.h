// INSPACE.INI for the port (ADR 0004): one file with the original's sections, keys, formats
// and defaults, read and written with the semantics of the Windows profile functions.
// Key meanings, defaults and readers: docs/re/formats.md#ini-files. Portable: no Win32 calls.
#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace is::settings {

// One INI file. Section and key names compare case-insensitively (cp1252 fold, as the profile
// functions do for ASCII). Values are cp1252 bytes (ADR 0003, decision 4). Unknown sections
// and keys survive a save, and so do comments, blank lines and other lines in their places.
// Parsing follows Wine's profile code, the oracle's: a line "[name]" opens a section (the name
// runs to the last ']'), a line "key=value" is an entry with key and value trimmed of white
// space, and any other line (";" comments, lines without '=' or with an empty key) is kept
// as it is but holds no key. Lines before the first section form a section without a header,
// named "". A lookup takes the first match in file order across sections of the same name.
class IniFile {
public:
    // Reads the file when it exists; a missing file is an empty INI with that path.
    explicit IniFile(std::filesystem::path path);

    const std::filesystem::path& path() const { return path_; }
    bool existed() const { return existed_; }  // the file was present when constructed

    // GetPrivateProfileString: the value without quotes, or nullopt when section or key is
    // absent. `nsize` truncates to nsize - 1 bytes as the Win16 call does (0 = no limit).
    std::optional<std::string> get(std::string_view section, std::string_view key,
                                   std::size_t nsize = 0) const;
    std::string get_string(std::string_view section, std::string_view key,
                           std::string_view def, std::size_t nsize = 0) const;
    // GetPrivateProfileInt: decimal, optional sign, parsing stops at the first other
    // character; an absent key or an empty value gives `def`.
    int get_int(std::string_view section, std::string_view key, int def) const;

    // WritePrivateProfileString: creates the section, replaces or appends the key. The value
    // is stored trimmed of surrounding white space, as a later read would see it.
    void set(std::string_view section, std::string_view key, std::string value);
    void set_int(std::string_view section, std::string_view key, int value);  // decimal
    void remove(std::string_view section, std::string_view key);

    // Writes the file (cp1252, CRLF), creating directories as needed. Throws
    // std::runtime_error when the file cannot be written.
    void save() const;

    // Sections and keys in file order, for tests and diagnostics.
    std::vector<std::string> sections() const;
    std::vector<std::pair<std::string, std::string>> entries(std::string_view section) const;

private:
    struct Section {
        std::string name;
        std::vector<std::pair<std::string, std::string>> entries;
        bool header = true;  // false: the lines before the first "[name]"
        // Lines that hold no key, each placed before entries[first] (first == entries.size():
        // after the last entry).
        std::vector<std::pair<std::size_t, std::string>> other;
    };
    Section* find(std::string_view section);
    const Section* find(std::string_view section) const;

    std::filesystem::path path_;
    bool existed_ = false;
    std::vector<Section> sections_;
};

// The section of every game key.
constexpr std::string_view kGameSection = "Inner Space";

// Every [Inner Space] key of formats.md#inspaceini-inner-space with INSPACE.EXE's type, read
// size and default, grouped and ordered as the doc's tables. Where ISPACE.DLL or the Factory
// read a key with another default, the comment names it. String sizes are the nsize INSPACE
// passes (the largest where its readers differ); keys only ISPACE.DLL reads take its size.
struct GameSettings {
    // ---- Paths, identity and game choice
    std::string path;        // S 0x6d; empty triggers regenerate()
    std::string local_path;  // S 0x6d
    std::string ship_path;   // S 0x6d
    std::string snd_path;    // S 0x6d (ISPACE.DLL and the Factory read it)
    std::string race_path;   // S 0x6d
    std::string games;       // S 11
    std::string dead;        // S 11
    std::string language;    // S 5
    std::string the_player;  // S 20 (seg10:0x8c reads 12); ISPACE.DLL: "AutoPilot"
    std::string main_ship = "EAGLE";  // S 20; ISPACE.DLL seg10:0x18e1: ""
    std::string restore;     // S 9 (ISPACE.DLL: 20)
    int play_mode = 0;
    std::string drive = "C";  // S 20 (ISPACE.DLL: 10)
    int size = 1;
    int set_size = 1;  // read only by ISPACE.DLL
    int strat_ask = 0;
    int save_prompt = 1;
    int accept = 0;
    int plus_count = 13;

    // ---- Window state and positions
    int full = 1;
    int on_top = 1;
    int show_left = 0, show_top = 0, show_right = 0, show_bottom = 0;
    int icon_wnd = 1, vscale = 0, sm_wnd = 0, sscale = 0;
    int show_launch = 2;
    int menu = 0;
    int radar_left = 0, radar_top = 0;
    int watch_left = 0, watch_top = 0;
    int watch_right = 280;     // default watch_left + 280 (default_watch_right)
    // Default screen width - 48 (default_icon_left). GameSettings{} assumes 640 pixels;
    // defaults(w) and load(ini, w) take the real width.
    int icon_left = 640 - 48;
    int icon_top = 0, sm_left = 0, sm_top = 0, help_left = 0, help_top = 0;
    int nav_left = 30, nav_top = 30, nav_right = 560, nav_bottom = 350;

    // ---- Settings
    int game_speed = 20000;     // 20000 means "use Calibrate" (timing.md)
    int frame_rate_set = 36;
    int calibrate = 0;          // the Factory: 1000
    int detail = 3;
    int quick = 0;              // ISPACE.DLL: 1
    int enhance = 1;
    int complex = 0;            // ISPACE.DLL: 2
    int braking = 0;
    int mix_channels = 1;       // ISPACE.DLL: 4
    int bad_video = 1;
    int quick_intro = 0;
    int ship_anim = 1;
    int border = 1;
    int bright = 0;
    int ignore_res = 0;
    int aux = 0;
    int rate = 0;
    int joystick = 0;
    int four_btns = 0;          // key "4Btns", read only by ISPACE.DLL
    int jmin_x = 0, jmin_y = 0, jrange_x = 0, jrange_y = 0;  // read only by ISPACE.DLL
    std::array<int, 4> jbtn = {0, 1, 2, 3};                   // keys "JBtn0".."JBtn3", default n

    // ---- Sound (read only by ISPACE.DLL)
    int sound_fx = 1, voice_fx = 1, music = 1;
    int sound_volume = 100, fx_music = 50;
    int stereo_effect = 2, purge_sound = 1, stereo_swap = 0;
    int sound_system = -1;
    int allow_sound = 0, force_vol = 0;
    int saver_volume = 0;

    // ---- Special times of day (hhmm)
    int breakfast = 800, coffee = 1000, lunch = 1200, tea = 1530;

    // ---- Race, duel and demo options
    int racers = 1, race_diff = 0, race_laps = 1, race_opp = 2;
    int duelers = 0, duel_diff = 0, duel_opp = 2;
    int dracers = 0, ddiff = 0, dlaps = 0;
    int demo_type = 519;
    int dscale = 0;
    std::string track;       // S 0x6d (ISPACE.DLL: 20)
    std::string ship1, ship2;  // S 20
    int tree = 0;

    // seg6:0x205: GetSystemMetrics(SM_CXSCREEN) - 48.
    static int default_icon_left(int screen_width) { return screen_width - 48; }
    // seg6:0x1e8: the WatchLeft just read + 280.
    static int default_watch_right(int watch_left) { return watch_left + 280; }

    // Every default, with IconLeft from `screen_width`.
    static GameSettings defaults(int screen_width);

    // Reads every key with its default, as GetPrivateProfileInt and GetPrivateProfileString
    // (nsize as above) give it. WatchRight defaults to the WatchLeft just read + 280.
    void load(const IniFile& ini, int screen_width);
    // Writes every key to [Inner Space] (integers in decimal). The original writes keys at
    // their own moments; the shell picks which moments call this.
    void store(IniFile& ini) const;

    // The key names in the order of formats.md's tables (JBtn0..JBtn3 expanded).
    static std::vector<std::string_view> keys();
};

// The port's own keys (ADR 0006 decision 3): section [Port] of the same file, which the
// original never reads. Scale: 0 (default) picks the window scale factor automatically, 1-4 fix
// it; shell::scale_factor owns that range and counts any other value as 0 [DESIGN]. load()
// keeps the key as written. formats.md documents only the original's keys.
constexpr std::string_view kPortSection = "Port";
// Music and SoundFont choose the music backend and its SoundFont (ADR 0011 decision 3): both
// kept as written; src/audio/music_select.h owns their values, the default rule and the
// SoundFont resolution [DESIGN].
struct PortSettings {
    int scale = 0;
    std::string music;      // [Port] Music: mci | soundfont | opl3; empty: the default
    std::string soundfont;  // [Port] SoundFont: the .sf2 path; empty: the default
    void load(const IniFile& ini);
};

// The port's copy of the registration values (ADR 0008 decision 1): section [Registration],
// keys vidsp, 32BitSP and PDev, which the original keeps in SYSTEM.INI [386Enh] and WIN.INI
// [windows] (formats.md "WIN.INI, SYSTEM.INI and other files"). The port reads them and never
// writes them. `present` is false when none of the three keys is there (a section holding only
// the Order dialog's Serial and SerialCount does not count); the caller then reads the Windows
// files (ADR 0008 decision 2) [DESIGN]. Absent keys read 0, as GetPrivateProfileInt's
// default in seg10:0x885e [BYTES seg10:0x8a26 seg10:0x8a3f seg10:0x8a52].
constexpr std::string_view kRegistrationSection = "Registration";
struct RegistrationSettings {
    bool present = false;
    int vidsp = 0;
    int bit32sp = 0;  // key "32BitSP"
    int pdev = 0;
    void load(const IniFile& ini);
};

// The regeneration of seg6:0x6d8 (formats.md#files-and-sections), with the asset directory
// as the program directory (ADR 0004, decision 4) and `local_dir` as LocalPath (decision 5):
// when [Inner Space] Path is empty, writes Path, LocalPath, ShipPath, SndPath, RacePath, Dead,
// Games, Language, MainShip and ThePlayer and returns true. Otherwise changes nothing.
// Path, ShipPath, SndPath and RacePath derive from `asset_dir`; LocalPath is `local_dir`. The
// directories are written in cp1252 with backslash separators and a trailing backslash, as the
// original writes them; a directory that cp1252 cannot hold throws std::runtime_error (ADR 0003).
bool regenerate(IniFile& ini, const std::filesystem::path& asset_dir,
                const std::filesystem::path& local_dir);

}  // namespace is::settings
